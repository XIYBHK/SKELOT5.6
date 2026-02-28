# Skelot 扩展技术预演文档

> 基于2024-2026年最新研究和最佳实践
> 创建日期: 2026-03-01

---

## 📚 调研资源汇总

### 核心参考资源

| 资源 | 类型 | 链接 |
|------|------|------|
| RVO2 官方库 | C++ 源码 | [GitHub - RVO2](https://github.com/snape/RVO2) |
| RecastNavigation | 导航+人群 | [GitCode - recastnavigation](https://gitcode.com/gh_mirrors/re/recastnavigation) |
| ORCA 算法论文 | 学术 | [ORCA: Optimal Reciprocal Collision Avoidance](https://gamma.cs.unc.edu/ORCA/) |
| PBD 论文 | 学术 | [Position Based Dynamics (Müller et al.)](https://matthias-research.github.io/pages/publications/posBasedDyn.pdf) |
| CrowdSplat | 2025研究 | Gaussian Splatting for Crowd Rendering |

---

## 1️⃣ PBD 碰撞系统 - 技术预演

### 算法核心原理

```cpp
// PBD 约束求解核心公式
// Δxᵢ = -[wᵢ / (Σⱼ wⱼ + α)] · ∇xᵢ C(x)

// 其中:
// wᵢ = 1/mᵢ (逆质量，通常=1)
// α = compliance (柔度参数，0=完全刚性)
// ∇xᵢ C(x) = 约束梯度
```

### 最佳实践配置参数

根据研究和参考文档，推荐以下参数：

```cpp
struct FPBDConfig
{
    // 核心参数
    float CollisionRadius = 50.0f;         // 碰撞半径 (cm)
    int32 IterationCount = 3;              // 迭代次数 (2-4)
    float RelaxationFactor = 0.3f;         // 松弛系数 (0.3-0.6)
    int32 MaxNeighbors = 64;               // 最大邻居数

    // 高级参数
    int32 PostObstacleIterations = 1;      // 障碍物后额外迭代
    float GridCellSizeMultiplier = 1.0f;   // 网格尺寸倍率

    // 速度投影 (防止穿透抖动)
    bool bEnableVelocityProjection = true;
    float VelocityProjectionStrength = 0.8f;  // (0.8-1.0)
    float VelocityRecoveryRate = 2.0f;        // 速度恢复速率

    // 推力限制
    float MaxPushForceCoefficient = 1.5f;    // 深度重叠时推力上限
};
```

### 关键实现代码模板

```cpp
// PBD 碰撞约束
void SolveCollisionConstraint(
    TArray<FSkelotInstancesSOA::FSlotData>& Slots,
    TArray<FVector3f>& Locations,
    const TArray<int32>& Neighbors,
    const FPBDConfig& Config)
{
    for (int32 Iter = 0; Iter < Config.IterationCount; Iter++)
    {
        for (int32 Idx : Neighbors)
        {
            if (Slots[Idx].bDestroyed) continue;

            FVector3f PosA = Locations[Idx];

            for (int32 OtherIdx : GetNeighborsInCell(Idx))
            {
                if (OtherIdx <= Idx) continue; // 避免重复处理
                if (Slots[OtherIdx].bDestroyed) continue;

                FVector3f PosB = Locations[OtherIdx];
                FVector3f Delta = PosB - PosA;
                float Dist = Delta.Size();
                float MinDist = Config.CollisionRadius * 2.0f;

                if (Dist < MinDist && Dist > KINDA_SMALL_NUMBER)
                {
                    // 计算穿透深度
                    float Penetration = MinDist - Dist;
                    FVector3f Direction = Delta / Dist;

                    // 应用位置校正 (各分担一半)
                    float Correction = Penetration * Config.RelaxationFactor * 0.5f;

                    Locations[Idx] -= Direction * Correction;
                    Locations[OtherIdx] += Direction * Correction;
                }
            }
        }
    }
}
```

### 性能优化策略

| 策略 | 效果 | 实现复杂度 |
|------|------|------------|
| **空间网格分区** | O(n²)→O(n) | 中 |
| **分帧更新** | 50%计算量降低 | 低 |
| **并行处理** | ~20x 加速 (GPU) | 高 |
| **LOD 距离分级** | 远距离跳过更新 | 低 |

### GPU 加速方案 (可选)

```hlsl
// Compute Shader 示例
[numthreads(64,1,1)]
void PBDSolveMain(uint3 DTid : SV_DispatchThreadID)
{
    uint InstanceIndex = DTid.x;
    if (!IsInstanceValid(InstanceIndex)) return;

    float3 MyPos = Locations[InstanceIndex];

    // 获取邻居
    uint NeighborCount;
    uint Neighbors[MAX_NEIGHBORS];
    GetNeighbors(InstanceIndex, NeighborCount, Neighbors);

    // 碰撞响应
    float3 Correction = float3(0,0,0);
    for (uint i = 0; i < NeighborCount; i++)
    {
        uint OtherIdx = Neighbors[i];
        float3 OtherPos = Locations[OtherIdx];

        float3 Delta = MyPos - OtherPos;
        float Dist = length(Delta);

        if (Dist < CollisionRadius * 2.0 && Dist > 0.001)
        {
            float Penetration = CollisionRadius * 2.0 - Dist;
            Correction += normalize(Delta) * Penetration * RelaxationFactor;
        }
    }

    // 原子操作写入结果
    InterlockedAdd(Locations[InstanceIndex].x, Correction.x);
    InterlockedAdd(Locations[InstanceIndex].y, Correction.y);
    InterlockedAdd(Locations[InstanceIndex].z, Correction.z);
}
```

**参考**: [Houdini Vellum Solver](https://www.sidefx.com/docs/houdini/nodes/sop/vellumsolver.html) - GPU 加速 PBD

---

## 2️⃣ ORCA/RVO 避障系统 - 技术预演

### RVO2 库集成方案

**推荐方案**: 直接集成官方 RVO2 库

```cpp
// RVO2 核心接口
#include "RVO.h"

class FSkelotRVOManager
{
public:
    RVO::RVOSimulator* Simulator;

    void Initialize(int32 MaxAgents, float TimeStep)
    {
        Simulator = new RVO::RVOSimulator();
        Simulator->setTimeStep(TimeStep);

        // 配置默认参数
        Simulator->setAgentDefaults(
            15.0f,   // neighborDist
            10,      // maxNeighbors
            5.0f,    // timeHorizon
            5.0f,    // timeHorizonObst
            50.0f,   // radius
            200.0f   // maxSpeed
        );
    }

    int32 AddAgent(const FVector& Position, float Radius, float MaxSpeed)
    {
        RVO::Vector2 Pos2D(Position.X, Position.Y);
        int32 AgentId = Simulator->addAgent(Pos2D);

        Simulator->setAgentRadius(AgentId, Radius);
        Simulator->setAgentMaxSpeed(AgentId, MaxSpeed);

        return AgentId;
    }

    void SetAgentGoal(int32 AgentId, const FVector& Goal)
    {
        RVO::Vector2 Goal2D(Goal.X, Goal.Y);
        Simulator->setAgentPrefVelocity(AgentId, Goal2D - Simulator->getAgentPosition(AgentId));
    }

    void Step()
    {
        Simulator->doStep();
    }

    FVector GetAgentVelocity(int32 AgentId)
    {
        RVO::Vector2 Vel = Simulator->getAgentVelocity(AgentId);
        return FVector(Vel.x(), Vel.y(), 0.0f);
    }
};
```

### ORCA 线性规划核心算法

```cpp
// ORCA 半平面交集求解 (来自 RVO2 源码)
bool linearProgram1(
    const TArray<FPlane2D>& Planes,     // 约束平面
    int32 PlaneNo,                       // 起始平面
    float Radius,                        // 最大速度
    const FVector2D& PreferredVelocity,  // 期望速度
    FVector2D& OutResult)                // 输出最优速度
{
    const float DotProduct = FVector2D::DotProduct(Planes[PlaneNo].Point, PreferredVelocity);
    const float Discriminant = DotProduct * DotProduct + Radius * Radius
                               - FVector2D::DistSquared(Planes[PlaneNo].Point, FVector2D::ZeroVector);

    if (Discriminant < 0.0f)
    {
        // 无解
        return false;
    }

    const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
    float tLeft = -DotProduct - SqrtDiscriminant;
    float tRight = -DotProduct + SqrtDiscriminant;

    // 检查与其他平面的交集
    for (int32 i = 0; i < PlaneNo; i++)
    {
        const float DotProduct2 = FVector2D::DotProduct(Planes[PlaneNo].Direction, Planes[i].Point);
        const float DotProduct3 = FVector2D::DotProduct(Planes[PlaneNo].Direction, Planes[i].Direction);
        const float Numerator = DotProduct2 - DotProduct3 * tLeft;
        const float Denominator = DotProduct3 * (tRight - tLeft);

        if (Denominator >= 0.0f)
        {
            if (Numerator <= 0.0f)
            {
                // 无解
                return false;
            }
            // 否则，继续
        }
        else
        {
            const float t = Numerator / Denominator;
            if (t > tLeft)
            {
                tLeft = t;
                if (tLeft > tRight)
                {
                    return false;
                }
            }
        }
    }

    OutResult = Planes[PlaneNo].Point + Planes[PlaneNo].Direction * tLeft;
    return true;
}
```

**参考**: [ORCA 避障源码笔记](https://www.cnblogs.com/kashin/p/18585330)

### RVO 参数配置最佳实践

```cpp
struct FRVOConfig
{
    // 基础参数
    float NeighborRadius = 300.0f;         // 邻居检测半径 (碰撞半径的4-6倍)
    float TimeHorizon = 1.0f;              // 时间窗 (0.5-1.0s)
    int32 MaxNeighbors = 16;               // 最大邻居数 (12-20)
    float MaxSpeed = 200.0f;               // 最大速度
    float MinSpeed = 50.0f;                // 最小速度 (防止完全停止)

    // 到达参数
    float ArrivalRadius = 200.0f;          // 到达半径 (开始减速)
    int32 ArrivalDensityThreshold = 6;     // 到达密度阈值 (防止拥挤)

    // 性能参数
    int32 FrameStride = 1;                 // 分帧步长 (2可降50%计算)

    // 抗抖动参数
    bool bEnableDensityAdaptation = true;  // 密度自适应
    int32 DensityThreshold = 8;            // 触发自适应的邻居数
    float HighDensityRelaxation = 0.3f;    // 高密度松弛系数

    bool bEnableVelocitySmoothing = true;  // 速度平滑
    float VelocitySmoothFactor = 0.15f;    // 平滑系数 (0.15-0.25)

    bool bEnableJitterDetection = true;    // 抖动检测
    float JitterThreshold = 0.7f;          // 方向变化阈值
    float JitterSuppression = 0.5f;        // 抑制强度
};
```

### 抗抖动算法实现

```cpp
// 速度平滑 (Spring-Damper 模型)
FVector SmoothVelocity(
    const FVector& CurrentVelocity,
    const FVector& TargetVelocity,
    float SmoothTime,
    float DeltaTime,
    FVector& VelocityRef)  // 内部状态
{
    // 基于 Unity SmoothDamp 算法
    float Omega = 2.0f / SmoothTime;
    float X = Omega * DeltaTime;
    float Exp = 1.0f / (1.0f + X + 0.48f * X * X + 0.235f * X * X * X);

    FVector Change = CurrentVelocity - TargetVelocity;
    FVector Temp = (VelocityRef + Omega * Change) * DeltaTime;
    VelocityRef = (VelocityRef - Omega * Temp) * Exp;

    return TargetVelocity + (CurrentVelocity - TargetVelocity + Temp) * Exp;
}

// 抖动检测
bool DetectJitter(
    const FVector& CurrentDirection,
    const FVector& PreviousDirection,
    float Threshold)
{
    float DotProduct = FVector::DotProduct(CurrentDirection, PreviousDirection);
    return DotProduct < Threshold;  // 方向变化过大 = 抖动
}

// 密度自适应
float GetAdaptiveRelaxation(
    int32 NeighborCount,
    const FRVOConfig& Config)
{
    if (NeighborCount > Config.DensityThreshold)
    {
        float ExcessRatio = (float)(NeighborCount - Config.DensityThreshold) / Config.DensityThreshold;
        return Config.HighDensityRelaxation / (1.0f + ExcessRatio);
    }
    return 1.0f;
}
```

**参考**: [Unity SmoothDamp](https://docs.unity3d.com/cn/2021.2/ScriptReference/Vector3.SmoothDamp.html)

---

## 3️⃣ 空间检测系统 - 技术预演

### 空间哈希网格实现

```cpp
// 空间网格核心结构
struct FSpatialGrid
{
    // 配置
    float CellSize = 200.0f;           // 单元大小 (建议: 查询半径的1-2倍)
    float InvCellSize;                  // 1/CellSize

    // 网格存储
    TMap<int64, TArray<int32>> Cells;   // HashKey -> 实例索引列表

    // 坐标转 HashKey
    int64 CoordToKey(int32 X, int32 Y) const
    {
        // 使用 Morton 编码优化空间局部性 (可选)
        // return MortonEncode(X, Y);
        return (int64)X << 32 | (uint32)Y;
    }

    FIntPoint LocationToCoord(const FVector& Loc) const
    {
        return FIntPoint(
            FMath::FloorToInt(Loc.X * InvCellSize),
            FMath::FloorToInt(Loc.Y * InvCellSize)
        );
    }

    // 更新实例位置
    void UpdateInstance(int32 InstanceIndex, const FVector& OldLoc, const FVector& NewLoc)
    {
        FIntPoint OldCoord = LocationToCoord(OldLoc);
        FIntPoint NewCoord = LocationToCoord(NewLoc);

        if (OldCoord == NewCoord) return;

        int64 OldKey = CoordToKey(OldCoord.X, OldCoord.Y);
        int64 NewKey = CoordToKey(NewCoord.X, NewCoord.Y);

        // 从旧单元移除
        if (TArray<int32>* OldCell = Cells.Find(OldKey))
        {
            OldCell->RemoveSwap(InstanceIndex);
            if (OldCell->Num() == 0)
            {
                Cells.Remove(OldKey);
            }
        }

        // 添加到新单元
        Cells.FindOrAdd(NewKey).Add(InstanceIndex);
    }

    // 球形范围查询
    void QuerySphere(
        const FVector& Center,
        float Radius,
        TArray<int32>& OutInstances,
        uint8 CollisionMask = 0xFF) const
    {
        FIntPoint MinCoord = LocationToCoord(Center - FVector(Radius));
        FIntPoint MaxCoord = LocationToCoord(Center + FVector(Radius));
        float RadiusSq = Radius * Radius;

        // 遍历可能相交的单元
        for (int32 Y = MinCoord.Y; Y <= MaxCoord.Y; Y++)
        {
            for (int32 X = MinCoord.X; X <= MaxCoord.X; X++)
            {
                int64 Key = CoordToKey(X, Y);
                if (const TArray<int32>* Cell = Cells.Find(Key))
                {
                    for (int32 Idx : *Cell)
                    {
                        // TODO: 检查碰撞掩码
                        // TODO: 检查距离
                        OutInstances.Add(Idx);
                    }
                }
            }
        }
    }

    // 获取邻居 (用于碰撞检测)
    void GetNeighbors(
        int32 InstanceIndex,
        const FVector& Location,
        float Radius,
        TArray<int32>& OutNeighbors) const
    {
        FIntPoint CenterCoord = LocationToCoord(Location);

        // 只检查 3x3 邻域
        for (int32 DY = -1; DY <= 1; DY++)
        {
            for (int32 DX = -1; DX <= 1; DX++)
            {
                int64 Key = CoordToKey(CenterCoord.X + DX, CenterCoord.Y + DY);
                if (const TArray<int32>* Cell = Cells.Find(Key))
                {
                    for (int32 Idx : *Cell)
                    {
                        if (Idx != InstanceIndex)
                        {
                            OutNeighbors.Add(Idx);
                        }
                    }
                }
            }
        }
    }
};
```

### 网格大小动态调整

```cpp
// 基于场景密度动态调整网格大小
float CalculateOptimalCellSize(
    const TArray<FVector>& Locations,
    float QueryRadius)
{
    // 统计平均密度
    float TotalDensity = 0.0f;
    int32 SampleCount = 0;

    for (int32 i = 0; i < FMath::Min(1000, Locations.Num()); i++)
    {
        int32 NearbyCount = 0;
        for (int32 j = 0; j < Locations.Num(); j++)
        {
            if (FVector::DistSquared(Locations[i], Locations[j]) < QueryRadius * QueryRadius * 4.0f)
            {
                NearbyCount++;
            }
        }
        TotalDensity += NearbyCount;
        SampleCount++;
    }

    float AvgDensity = TotalDensity / SampleCount;

    // 高密度场景用较小网格
    if (AvgDensity > 20) return QueryRadius * 0.8f;
    if (AvgDensity > 10) return QueryRadius * 1.0f;
    return QueryRadius * 1.5f;
}
```

### 多线程安全方案

```cpp
// 双缓冲空间网格
struct FThreadSafeSpatialGrid
{
    FSpatialGrid Grids[2];
    std::atomic<int32> ReadIndex{0};

    FSpatialGrid& GetReadGrid() { return Grids[ReadIndex.load()]; }
    FSpatialGrid& GetWriteGrid() { return Grids[1 - ReadIndex.load()]; }

    void Swap()
    {
        ReadIndex.store(1 - ReadIndex.load());
        // 清空新的写缓冲
        GetWriteGrid().Cells.Empty();
    }
};

// 使用方式
// 游戏线程: 写入 WriteGrid
// 查询线程: 读取 ReadGrid
// 每帧末尾调用 Swap()
```

**参考**: [Spatial Hashing Implementation](https://m.php.cn/faq/1927461.html)

---

## 4️⃣ DetourCrowd 集成参考

### DetourCrowd 核心类

```cpp
// 来自 RecastNavigation 的 DetourCrowd 模块

// dtCrowd - 人群管理器
class dtCrowd
{
    int init(const int maxAgents, const float maxAgentRadius, dtNavMesh* nav);
    int addAgent(const float* pos, const dtCrowdAgentParams* params);
    void removeAgent(const int idx);

    void update(const float dt, dtCrowdAgentDebugInfo* debug);

    // 设置目标点
    int requestMoveTarget(const int idx, dtPolyRef ref, const float* pos);
};

// dtCrowdAgentParams - 智能体参数
struct dtCrowdAgentParams
{
    float radius;           // 半径
    float height;           // 高度
    float maxAcceleration;  // 最大加速度
    float maxSpeed;         // 最大速度

    float collisionQueryRange;    // 碰撞查询范围
    float pathOptimizationRange;  // 路径优化范围

    float separationWeight;  // 分离力权重
    float crowdingWeight;    // 拥挤权重
    float obstacleAvoidanceWeight;  // 障碍避让权重

    unsigned char obstacleAvoidanceType;  // 避障类型
    unsigned char queryFilterType;        // 查询过滤器类型
};
```

**参考**: [RecastNavigation GitCode](https://gitcode.com/gh_mirrors/re/recastnavigation)

---

## 5️⃣ 性能基准参考

### 大规模人群模拟性能 (2024-2025 研究)

| 规模 | 技术 | 硬件 | 帧率 |
|------|------|------|------|
| 10,000 智能体 | GPU 社会力模型 | RTX 3080 | 60+ FPS |
| 50,000 智能体 | Curl-Noise | 主流 GPU | 50+ FPS |
| 100,000 智能体 | 混合路径规划 | RTX 4090 | 30+ FPS |

### 优化效果对比

| 优化技术 | 加速比 |
|----------|--------|
| CUDA 并行计算 | ~20x vs CPU |
| Curl-Noise 方法 | 50x+ vs 传统方法 |
| GPU 动画混合 | 40x 增加智能体数 |
| 分帧计算 (步长=2) | 50% 计算量降低 |

**参考**: [CrowdSplat: Gaussian Splatting for Crowd Rendering](https://www.researchgate.net/publication/)

---

## 6️⃣ 实施建议

### 推荐技术栈

```
┌─────────────────────────────────────────────────────────────┐
│                      Skelot 扩展架构                         │
├─────────────────────────────────────────────────────────────┤
│  空间网格层                                                  │
│  ├── FSpatialGrid (自定义实现)                              │
│  └── 动态网格大小调整                                        │
├─────────────────────────────────────────────────────────────┤
│  PBD 碰撞层                                                  │
│  ├── 单线程实现 (Phase 1)                                   │
│  ├── 多线程优化 (Phase 2)                                   │
│  └── GPU 加速 (可选 Phase 3)                                │
├─────────────────────────────────────────────────────────────┤
│  RVO 避障层                                                  │
│  ├── 集成 RVO2 库 (推荐)                                    │
│  ├── 2D 实现 (XY 平面)                                      │
│  └── 抗抖动系统                                             │
├─────────────────────────────────────────────────────────────┤
│  可选: DetourCrowd 集成                                      │
│  └── 用于导航网格支持                                        │
└─────────────────────────────────────────────────────────────┘
```

### 分阶段验证目标

| 阶段 | 目标 | 验证标准 |
|------|------|----------|
| Phase 1 | 1,000 实例 PBD | 60 FPS, 无穿透 |
| Phase 2 | 10,000 实例 PBD | 60 FPS, 稳定 |
| Phase 3 | 10,000 实例 PBD+RVO | 60 FPS, 无抖动 |
| Phase 4 | 50,000 实例 | 30+ FPS, 可用 |

---

## 📚 完整参考链接

### 算法与论文
- [RVO2 Official Library](https://gamma.cs.unc.edu/RVO2/)
- [ORCA Algorithm Paper](https://gamma.cs.unc.edu/ORCA/)
- [Position Based Dynamics Paper](https://matthias-research.github.io/pages/publications/posBasedDyn.pdf)
- [Half-plane Intersection Algorithm](https://wk.baidu.com/view/ae085f0e7cd184254b35351a)

### 源码参考
- [RVO2 GitHub](https://github.com/snape/RVO2)
- [RecastNavigation GitCode](https://gitcode.com/gh_mirrors/re/recastnavigation)
- [ORCA 源码分析](https://blog.csdn.net/Mhypnos/article/details/136101372)
- [RVO 源码笔记](https://www.cnblogs.com/kashin/p/18585330)

### 技术博客
- [Unity SmoothDamp](https://docs.unity3d.com/cn/2021.2/ScriptReference/Vector3.SmoothDamp.html)
- [Spatial Hashing C++](https://m.php.cn/faq/1927461.html)
- [Quake III Arena Optimization](https://m.blog.csdn.net/gitblog_00944/article/details/154328788)
- [PBD 最佳实践](https://blog.csdn.net/gitblog_00393/article/details/151500546)

### 工具与框架
- [Houdini Vellum Solver](https://www.sidefx.com/docs/houdini/nodes/sop/vellumsolver.html)
- [Unity Obi Softbody](https://obi.virtualmethodstudio.com/)
- [DetourCrowd Documentation](https://gitcode.com/gh_mirrors/re/recastnavigation)

---

*文档版本: 1.0 | 最后更新: 2026-03-01*
