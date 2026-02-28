# Skelot 扩展实现难点分析

> 基于 SkelotRbn 参考文档和现有 Skelot 代码的深度分析
> 分析日期: 2026-02-28

---

## 📊 难度评估总览

| 功能模块 | 技术难度 | 工作量 | 风险等级 | 优先级 |
|----------|----------|--------|----------|--------|
| 数据结构扩展 | ⭐⭐ | 中 | 低 | P0 |
| 碰撞通道系统 | ⭐⭐ | 低 | 低 | P0 |
| 空间检测系统 | ⭐⭐⭐ | 中 | 中 | P0 |
| PBD 碰撞系统 | ⭐⭐⭐⭐ | 高 | 高 | P0 |
| ORCA/RVO 避障 | ⭐⭐⭐⭐⭐ | 高 | 高 | P1 |
| LOD 更新频率 | ⭐⭐ | 低 | 低 | P1 |
| 几何工具库 | ⭐⭐ | 中 | 低 | P1 |
| 编辑器扩展 | ⭐⭐ | 低 | 低 | P2 |

---

## 🔴 高难度挑战

### 1. PBD 碰撞系统 - 难度 ⭐⭐⭐⭐

#### 核心挑战

**1.1 大规模邻居查询效率**

```
问题: 4-5万实例需要在每帧检测碰撞
复杂度: O(n²) = 50000² = 25亿次比较 → 不可接受

解决方案: 空间哈希网格
- 预期复杂度: O(n)
- 但需要精心设计哈希函数和网格大小
```

**1.2 约束求解的稳定性**

```cpp
// PBD 核心公式
Δxᵢ = -[wᵢ / (Σⱼ wⱼ + α)] · ∇xᵢ C(x)

// 难点:
// 1. 松弛系数选择 - 太大会震荡，太小响应慢
// 2. 迭代次数 - 太少不稳定，太多性能差
// 3. 穿透恢复 - 高密度时容易穿透后弹不出
```

**1.3 多线程竞争问题**

```cpp
// 现有代码模式 (SkelotWorld.cpp)
// 使用 FMemStack 和 TStackIncrementalArray 处理临时数据

// PBD 的挑战:
// - 多个线程可能同时修改同一实例的位置
// - 需要原子操作或分批处理
// - 空间网格更新需要同步
```

**1.4 与现有动画系统集成**

```
问题链:
1. 动画更新 → 位置变化
2. PBD 碰撞 → 位置再次变化
3. 渲染数据提交 → 需要最终位置

时序挑战:
- 动画在 UpdateAnimations() 中更新
- PBD 需要在动画之后、渲染之前执行
- 现有 Tick 流程需要修改
```

#### 现有代码参考

```cpp
// SkelotWorld.cpp 已有的集群更新模式
void UpdateClusters()
{
    // 每32帧才做一次完整更新
    if (++ClusterUpdateCounter == 32)
    {
        ClusterUpdateCounter = 0;
        // 完整更新...
    }
}

// 可以借鉴: PBD 也采用类似的分帧更新策略
```

#### 解决方案建议

```
Phase 1: 单线程实现
- 先确保算法正确性
- 使用简单的空间网格

Phase 2: 多线程优化
- 使用 ParallelFor 处理网格单元
- 每个网格单元独立处理，避免竞争

Phase 3: GPU 加速 (可选)
- Compute Shader 实现
- 参考 GPU Gems 的 PBD 实现
```

---

### 2. ORCA/RVO 避障 - 难度 ⭐⭐⭐⭐⭐

#### 核心挑战

**2.1 算法复杂度**

```
RVO 计算流程:
1. 获取邻居 (O(n) with spatial grid)
2. 为每个邻居计算速度障碍锥
3. 构建线性约束平面
4. 求解线性规划找最优速度

难点: 步骤2-4对每个实例都需要执行
- 假设平均邻居数 k=20
- 50000 实例 × 20 邻居 = 100万次 VO 计算
- 每次涉及三角函数和矩阵运算
```

**2.2 线性规划求解**

```cpp
// ORCA 核心是求解半平面交集
// 需要实现或集成:
// - 2D 半平面交集算法
// - 或线性规划求解器 (如 simplex)

// 挑战:
// 1. UE 没有内置 LP 求解器
// 2. 需要自己实现轻量级求解
// 3. 数值稳定性问题
```

**2.3 抖动问题**

```
问题表现:
- 实例在两个方向间反复切换
- 特别是在高密度或窄通道场景

原因:
1. 离散时间步长导致速度突变
2. 多个实例同时避让造成振荡
3. RVO 局部最优不等于全局最优

参考文档提到的解决方案:
- 速度平滑 (Velocity Smoothing)
- 密度自适应 (Density Adaptation)
- 抖动检测 (Jitter Detection)
```

**2.4 3D vs 2D**

```
问题: RVO 论文主要针对 2D 人群模拟

扩展到 3D 的挑战:
1. 速度障碍从锥形变为更复杂的形状
2. 半平面交集变成半空间交集
3. 计算量显著增加

建议: 先实现 2D 版本 (XY 平面)
- 大多数群集场景在地面移动
- 3D 扩展可作为后续优化
```

#### 解决方案建议

```
1. 集成 RVO2 库
   - https://gamma.cs.unc.edu/RVO2/
   - 成熟的开源实现
   - 需要处理 UE 类型转换

2. 分帧计算
   - 将实例分成多批
   - 每帧只计算一部分
   - 参考文档: RVO分帧步长=2 可降低50%计算量

3. LOD 策略
   - 远处实例不做 RVO，只用简单碰撞
   - 近处实例才做完整避障计算
```

---

### 3. 空间检测系统 - 难度 ⭐⭐⭐

#### 核心挑战

**3.1 网格大小选择**

```cpp
// 网格太小: 太多空单元，内存浪费
// 网格太大: 退化为全局搜索

// 参考文档建议: 设为查询半径的1-2倍
// 但不同场景查询半径不同...

// 动态调整方案:
// 1. 统计平均查询半径
// 2. 每N帧重新计算网格大小
// 3. 重建网格 (有性能开销)
```

**3.2 实例快速移动**

```
问题: 实例每帧可能移动很远

时序:
Frame N:   实例在 Cell(10, 10)
Frame N+1: 实例在 Cell(15, 12)

挑战:
1. 需要每帧更新网格映射
2. 更新过程不能太慢
3. 查询时数据必须一致
```

**3.3 并发安全**

```cpp
// 场景: 游戏线程更新位置，同时有查询请求

// 方案 A: 读写锁
// - 简单但可能阻塞

// 方案 B: 双缓冲
// - 两个网格交替使用
// - 一帧更新一个，查询另一个
// - 内存翻倍但无锁
```

#### 现有代码可复用

```cpp
// SkelotWorld.cpp 已有坐标转换
FIntPoint LocationToTileCoord(const FVector& Loc)
{
    // 使用 GSkelot_InvClusterCellSize
    return FIntPoint(
        FMath::FloorToInt(Loc.X * GSkelot_InvClusterCellSize),
        FMath::FloorToInt(Loc.Y * GSkelot_InvClusterCellSize)
    );
}

// 可以扩展为空间网格使用
```

---

## 🟡 中等难度挑战

### 4. 数据结构扩展 - 难度 ⭐⭐

#### 挑战

**4.1 内存布局**

```cpp
// FSkelotInstancesSOA 使用 SOA (Structure of Arrays) 布局
// 优点: 缓存友好，SIMD 友好

// 扩展时需要:
// 1. 在 IncreaseSOAs() 中添加新数组
// 2. 保持与现有代码兼容
// 3. 考虑序列化 (SaveGame)
```

**4.2 SlotData 位域限制**

```cpp
// FSlotData 使用位域节省空间
struct FSlotData
{
    uint32 Version;
    uint32 bDestroyed : 1;
    uint32 bAnimationLooped : 1;
    // ... 已用 8 位

    uint8 UserFlags;  // 用户可用 8 位

    // 添加碰撞通道需要:
    // - CollisionChannel: 3 位 (0-7)
    // - 可能需要重新设计位域布局
};
```

**4.3 碰撞掩码存储**

```cpp
// 8 位掩码 = 1 字节
// 方案 A: 添加到 SlotData (紧凑但破坏现有布局)
// 方案 B: 添加新数组 SOA.CollisionMasks (简单但额外内存)

// 建议: 方案 B，每实例 1 字节开销可接受
```

---

### 5. LOD 更新频率 - 难度 ⭐⭐

#### 挑战

**5.1 距离计算**

```cpp
// 每帧计算所有实例到相机的距离
// 50000 实例 × 距离计算 = 可观的开销

// 优化方案:
// 1. 使用距离平方避免 sqrt
// 2. 分帧计算 (每帧只算一部分)
// 3. 使用相机移动阈值触发更新
```

**5.2 与现有系统协调**

```cpp
// 现有动画更新在 UpdateAnimations()
// 需要添加条件跳过远距离实例

void UpdateAnimation(int32 InstanceIndex, float Delta)
{
    FSkelotInstancesSOA::FSlotData& Slot = SOA.Slots[InstanceIndex];

    // 添加 LOD 检查
    if (ShouldSkipUpdateDueToLOD(InstanceIndex))
        return;

    // ... 现有逻辑
}
```

---

### 6. 几何工具库 - 难度 ⭐⭐

#### 挑点

**6.1 Mesh 表面采样**

```cpp
// GetPointsByMesh 需要:
// 1. 访问 StaticMesh 的三角形数据
// 2. 基于面积加权随机采样
// 3. 可能需要 BVH 加速

// UE 提供的工具:
// - FStaticMeshRenderData
// - FPositionVertexBuffer
// 但需要自己实现采样算法
```

**6.2 体素化**

```cpp
// GetPointsByMeshVoxel 需要:
// 1. 三角形与体素相交测试
// 2. 实心填充算法 (洪水填充等)
// 3. 性能优化 (LOD 简化)
```

---

## 🟢 低难度挑战

### 7. 碰撞通道系统 - 难度 ⭐⭐

```cpp
// 实现相对简单
// 主要是数据存储和位运算

uint8 CollisionChannel;  // 0-7
uint8 CollisionMask;     // 0x00-0xFF

bool ShouldCollide(A, B)
{
    return (A.Mask & (1 << B.Channel)) &&
           (B.Mask & (1 << A.Channel));
}
```

### 8. 编辑器扩展 - 难度 ⭐⭐

```cpp
// UE 提供完善的扩展 API
// 主要是菜单注册和资产工厂

// 右键菜单扩展
FContentBrowserMenuExtender
// 资产工厂
UAssetFactory
```

---

## 📈 风险矩阵

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| PBD 数值不稳定 | 高 | 高 | 增量开发，先小规模验证 |
| RVO 抖动严重 | 中 | 高 | 参考文档参数，分阶段调试 |
| 性能不达标 | 中 | 高 | 分帧计算，GPU 加速 |
| 多线程竞争 | 中 | 中 | 使用原子操作或双缓冲 |
| 与现有代码冲突 | 低 | 中 | 保持现有接口不变，只扩展 |

---

## 🛠️ 建议开发策略

### 渐进式开发

```
Week 1-2: 基础设施
├── 数据结构扩展 (Velocities, CollisionData)
├── 碰撞通道 API
└── 空间网格基础

Week 3-4: PBD 系统
├── 单线程 PBD 实现
├── 基础参数配置
└── 调试可视化

Week 5-6: PBD 优化
├── 多线程优化
├── 障碍物支持
└── 性能测试

Week 7-8: RVO 系统
├── 2D RVO 实现
├── 与 PBD 集成
└── 抗抖动调优

Week 9-10: 完善与测试
├── LOD 更新频率
├── 几何工具
├── 示例关卡
└── 文档完善
```

### 关键里程碑

1. **M1**: 1000 实例 PBD 稳定运行
2. **M2**: 10000 实例 60FPS
3. **M3**: 50000 实例 PBD+RVO 可用
4. **M4**: 完整功能，文档齐全

---

## 📚 参考资源

### 算法论文
- [Position Based Dynamics (Müller et al., 2006)](https://matthias-research.github.io/pages/publications/posBasedDyn.pdf)
- [RVO: Reciprocal Velocity Obstacles (van den Berg et al., 2008)](https://gamma.cs.unc.edu/RVO/)
- [ORCA: Optimal Reciprocal Collision Avoidance (van den Berg et al., 2011)](https://gamma.cs.unc.edu/ORCA/)

### 开源实现
- [RVO2 Library](https://gamma.cs.unc.edu/RVO2/) - C++ 实现
- [Unity RVO](https://arongranberg.com/astar/docs/rvo/) - Unity 插件

### UE 文档
- [FFeedbackContextAnsi](https://docs.unrealengine.com/5.6/en-US/)
- [ParallelFor](https://docs.unrealengine.com/5.6/en-US/programming/development/multithreading/)

---

*分析完成 - 建议从 Phase 1-2 开始，逐步验证后再进入高难度模块*
