# 修复计划：RVO 到达行为 + 障碍物脏标记

> 创建日期: 2026-03-21
> 优先级: 中
> 状态: 已完成

---

## 问题总览

代码审查发现 2 处"声明了参数但没有接入逻辑"的遗漏：

| # | 问题 | 严重程度 | 影响 |
|---|------|----------|------|
| 1 | `ArrivalRadius` + `ArrivalDensityThreshold` 未接入 RVO 逻辑 | **中** | 用户调参无效果，实例全速冲向目标后靠PBD硬弹开 |
| 2 | `bObstaclesDirty` 只写不读，障碍物数据每帧无条件重建 | **低** | 功能正确但浪费了预留的优化机制 |

---

## 修复 1：RVO 到达行为（ArrivalRadius + ArrivalDensityThreshold）

### 1.1 问题描述

`FSkelotRVOConfig` 中定义了两个参数：
- `ArrivalRadius = 200.0f` — 接近目标时的减速范围（厘米）
- `ArrivalDensityThreshold = 6` — 目标点附近实例数超过此值时停止趋近

这两个参数在蓝图编辑器中可见可调，但 **整个 `SkelotRVOSystem.cpp` 中没有任何引用**，调参完全无效果。

### 1.2 当前速度流程

```
用户代码 SetInstanceVelocity() / AdvanceInstancesByVelocity()
    │
    ↓ 写入 SOA.Velocities[idx]
    │
Tick() 阶段
    ├─ RVO: ComputeAvoidance()
    │   └─ ComputeAgentAvoidance()
    │       └─ PreferredVelocity = 直接从 SOA.Velocities 提取（无到达减速）
    │       └─ LinearProgram2() 求解 → 修正速度写回 SOA.Velocities
    │
    ├─ PBD: SolvePBDCollisions()
    │   └─ 位置碰撞修正
    │
PostActorTick() 阶段
    └─ ConsumePendingVelocityAdvances()
        └─ SOA.Locations += AppliedVelocity * DeltaTime  ← 位置积分
```

**关键点**：当前系统**没有"目标点"概念**。速度是用户直接设置的方向向量，RVO 只负责在保持方向的同时避开邻居。

### 1.3 设计思路

到达行为应该在 RVO 求解 **之前** 缩放 `PreferredVelocity`，让 ORCA 线性规划以降低后的速度为目标求解。

**方案**：在 `ComputeAgentAvoidance` 中，利用当前速度大小和邻居密度来判断"到达状态"并缩放速度。

由于系统没有显式目标点，到达行为的触发条件改为：**当实例速度很低（接近停止）且周围邻居密集时，进一步降低 PreferredVelocity**。这与参考文档中"防止目标点过度拥挤"的意图一致——用户在蓝图中给实例一个朝向目标的速度，当接近目标时会自然减速（用户降低速度），此时到达逻辑进一步平滑减速过程。

**替代方案（更完整）**：如果希望完全自动化到达行为，可以让 `AdvanceInstancesByVelocity` 接受一个可选的目标位置，存储到 SOA 中，然后在 RVO 中根据到目标距离缩放。但这需要扩展 SOA 数据结构，改动较大。

**推荐方案**：基于当前速度大小 + 邻居密度的轻量方案，不需要新增 SOA 字段。

### 1.4 修改清单

#### 文件 1: `Source/Skelot/Private/SkelotRVOSystem.cpp`

**修改函数**: `ComputeAgentAvoidance`（第 172-266 行）

**修改位置**: 在 `PreferredVelocity` 计算之后、`LinearProgram2` 调用之前（约第 194-250 行之间）

**当前代码（第 192-200 行）**:
```cpp
FVector2f PreferredVelocity(MyVel.X, MyVel.Y);

float MaxSpeed = Config.MaxSpeed > 0 ? Config.MaxSpeed : CurrentSpeed;
MaxSpeed = FMath::Max(MaxSpeed, Config.MinSpeed);

LocalNeighborIndices.Reset();
SpatialGrid.QuerySphere(FVector(MyPos3D), Config.NeighborRadius, LocalNeighborIndices, 0xFF, &SOA);

int32 NumNeighbors = FMath::Min(LocalNeighborIndices.Num(), Config.MaxNeighbors);
```

**需要在邻居查询之后、ORCA 平面构建之前插入到达行为逻辑**:

```cpp
// ---- 到达行为：基于速度大小和邻居密度缩放 PreferredVelocity ----
if (Config.ArrivalRadius > 0.0f)
{
    // 速度比例：当前速度 / 最大速度，反映用户是否在减速
    const float SpeedRatio = CurrentSpeed / FMath::Max(MaxSpeed, RVO_EPSILON);

    // 当速度较低时（用户在减速趋近目标），应用到达减速
    if (SpeedRatio < 1.0f)
    {
        float ArrivalFactor = 1.0f;

        // 密度减速：邻居数超过阈值时进一步降速
        if (Config.ArrivalDensityThreshold > 0 && NumNeighbors > Config.ArrivalDensityThreshold)
        {
            const float ExcessRatio = static_cast<float>(NumNeighbors - Config.ArrivalDensityThreshold)
                                    / static_cast<float>(Config.ArrivalDensityThreshold);
            ArrivalFactor = 1.0f / (1.0f + ExcessRatio);  // 密度越高，速度越低
        }

        // 速度平滑减速：低速时进一步衰减
        ArrivalFactor *= FMath::Clamp(SpeedRatio, 0.0f, 1.0f);

        PreferredVelocity *= ArrivalFactor;
        MaxSpeed *= ArrivalFactor;
        MaxSpeed = FMath::Max(MaxSpeed, Config.MinSpeed);  // 保底最小速度
    }
}
```

**注意事项**:
- `ArrivalRadius` 在此方案中作为"启用开关"（> 0 时启用），未来如果引入目标点可以用作距离阈值
- 密度缩放公式与已有的 `GetDensityAdaptationFactor` 保持一致风格
- 必须保证 `MaxSpeed >= Config.MinSpeed`，防止完全停止

#### 不需要修改的文件

- `SkelotPBDPlane.h` — 参数定义已正确，无需改动
- `SkelotRVOSystem.h` — 函数签名不变
- `SkelotWorld.cpp` — 调用链不变

### 1.5 测试要点

1. **基础功能**：设置一群实例朝同一目标点移动，观察到达时是否平滑减速而非硬弹开
2. **密度阈值**：调整 `ArrivalDensityThreshold`，验证密集区域实例速度是否降低
3. **参数无效化**：设置 `ArrivalRadius = 0`，验证行为与修改前完全一致（不引入回归）
4. **与抗抖动交互**：验证到达减速 + 抗抖动系统不会互相冲突导致实例卡死

---

## 修复 2：障碍物脏标记（bObstaclesDirty）

### 2.1 问题描述

`bObstaclesDirty` 在以下时机被设为 `true`：
- `RegisterObstacle()` — 新障碍物注册时
- `UnregisterObstacle()` — 障碍物注销时
- `MarkObstaclesDirty()` — 编辑器中障碍物属性变更或移动时

但 **SkelotWorld 的 Tick/SolvePBDCollisions 中从不读取这个标记**，每帧都无条件重新遍历 `RegisteredObstacles` 构建 `FObstacleCollisionData` 临时数组。

### 2.2 当前障碍物碰撞流程

```
SkelotWorld::Tick()
    └─ SolvePBDCollisions()
        └─ if (RegisteredObstacles.Num() > 0)
            └─ PBDCollisionSystem.SolveObstacleCollisions(SOA, N, RegisteredObstacles, DeltaTime)
                │
                ├─ 步骤1: 每帧遍历 RegisteredObstacles → 构建 TArray<FObstacleCollisionData>
                │   （读取每个障碍物的 Transform, Rotation, Location, Radius/Extent...）
                │
                └─ 步骤2: ParallelFor 所有实例 vs 所有障碍物做碰撞检测
```

**FObstacleCollisionData 结构**（`SkelotPBDCollision.cpp` 匿名命名空间，第 11-21 行）:
```cpp
struct FObstacleCollisionData
{
    ESkelotObstacleType Type = ESkelotObstacleType::Sphere;
    uint8 CollisionMask = 0xFF;
    float RadiusOffset = 0.0f;
    float SphereRadius = 0.0f;
    FVector BoxExtent = FVector::ZeroVector;
    FTransform Transform = FTransform::Identity;
    FQuat Rotation = FQuat::Identity;
    FVector Location = FVector::ZeroVector;
};
```

### 2.3 设计思路

将 `FObstacleCollisionData` 数组从函数局部变量提升为 `FSkelotPBDCollisionSystem` 的成员缓存。只在 `bObstaclesDirty == true` 时重建缓存，否则复用上一帧的数据。

**注意**：障碍物在运行时可能移动（比如移动平台）。当前实现每帧读取最新 Transform 是正确的。缓存优化只适用于**静态障碍物**场景。如果需要支持运动障碍物，需要区分静态/动态障碍物，或者在障碍物 Tick 中调用 `MarkObstaclesDirty()`。

**推荐方案**：
- 在 `SkelotWorld::SolvePBDCollisions` 中检查 `bObstaclesDirty`
- dirty 时重建缓存并清除标记
- 非 dirty 时复用缓存
- 同时在 `ASkelotObstacle::Tick` 中对移动的障碍物自动标记 dirty（如果障碍物位置变化）

### 2.4 修改清单

#### 文件 1: `Source/Skelot/Public/SkelotPBDCollision.h`

**新增**：缓存成员和重建方法

```cpp
// 在 FSkelotPBDCollisionSystem 的 private 区域新增：

/** 障碍物碰撞数据缓存 */
TArray<FObstacleCollisionData> CachedObstacleData;

/** 重建障碍物碰撞数据缓存 */
void RebuildObstacleDataCache(const TArray<TObjectPtr<ASkelotObstacle>>& Obstacles);
```

**注意**：`FObstacleCollisionData` 当前定义在 `SkelotPBDCollision.cpp` 的匿名命名空间中，需要移到头文件或 forward declare。最简方案是把结构体定义移到 `SkelotPBDCollision.h` 中（private 前向声明不适用于结构体成员）。

#### 文件 2: `Source/Skelot/Private/SkelotPBDCollision.cpp`

**修改 1**：将 `FObstacleCollisionData` 从匿名命名空间移到 `SkelotPBDCollision.h`

**修改 2**：新增 `RebuildObstacleDataCache` 方法，提取 `SolveObstacleCollisions` 中第 337-365 行的数据收集逻辑

**修改 3**：修改 `SolveObstacleCollisions`，接受 `const TArray<FObstacleCollisionData>&` 而非 `const TArray<TObjectPtr<ASkelotObstacle>>&`，直接使用缓存数据

**当前 SolveObstacleCollisions 数据收集段（第 337-365 行）**:
```cpp
TArray<FObstacleCollisionData> ObstacleDataArray;
ObstacleDataArray.Reserve(Obstacles.Num());
for (const TObjectPtr<ASkelotObstacle>& ObstaclePtr : Obstacles)
{
    const ASkelotObstacle* Obstacle = ObstaclePtr.Get();
    if (!Obstacle || !Obstacle->bEnabled)
    {
        continue;
    }

    FObstacleCollisionData ObstacleData;
    // ... 收集数据 ...
    ObstacleDataArray.Add(ObstacleData);
}
```

**改为**：
```cpp
void FSkelotPBDCollisionSystem::RebuildObstacleDataCache(const TArray<TObjectPtr<ASkelotObstacle>>& Obstacles)
{
    CachedObstacleData.Reset();
    CachedObstacleData.Reserve(Obstacles.Num());
    for (const TObjectPtr<ASkelotObstacle>& ObstaclePtr : Obstacles)
    {
        // ... 与当前逻辑相同 ...
    }
}
```

#### 文件 3: `Source/Skelot/Private/SkelotWorld.cpp`

**修改函数**: `SolvePBDCollisions`（第 2547-2578 行）

**当前代码**:
```cpp
if (RegisteredObstacles.Num() > 0)
{
    PBDCollisionSystem.SolveObstacleCollisions(SOA, GetNumInstance(), RegisteredObstacles, DeltaTime);
    // 额外迭代...
}
```

**改为**:
```cpp
if (RegisteredObstacles.Num() > 0)
{
    // 仅在障碍物数据变化时重建缓存
    if (bObstaclesDirty)
    {
        PBDCollisionSystem.RebuildObstacleDataCache(RegisteredObstacles);
        bObstaclesDirty = false;
    }

    PBDCollisionSystem.SolveObstacleCollisions(SOA, GetNumInstance(), DeltaTime);
    // 额外迭代...
}
```

#### 文件 4: `Source/Skelot/Private/SkelotObstacle.cpp`

**新增**：运行时障碍物移动时标记 dirty

**当前 ASkelotObstacle 只在编辑器中通过 `PostEditMove` 标记 dirty**。运行时移动的障碍物（如移动平台）不会触发 dirty。

**方案 A（简单）**：在 `ASkelotObstacle` 中添加 Tick，检测位置是否变化：

```cpp
void ASkelotObstacle::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 检测运行时位置/旋转变化
    FTransform CurrentTransform = GetActorTransform();
    if (!CurrentTransform.Equals(CachedTransform, KINDA_SMALL_NUMBER))
    {
        CachedTransform = CurrentTransform;
        if (ASkelotWorld* SkelotWorld = ASkelotWorld::Get(this, false))
        {
            SkelotWorld->MarkObstaclesDirty();
        }
    }
}
```

需要在 `ASkelotObstacle` 头文件中添加 `FTransform CachedTransform` 成员。

**方案 B（零开销）**：不处理运动障碍物，文档中说明 `bObstaclesDirty` 缓存仅适用于静态障碍物。运动障碍物应每帧标记 dirty 或禁用缓存。

**推荐**：方案 A，开销极低（每个障碍物一次 Transform 比较），且对用户透明。

### 2.5 测试要点

1. **静态障碍物**：放置不动的障碍物，验证碰撞行为与修改前一致
2. **动态障碍物**：运行时移动障碍物，验证实例能正确响应新位置
3. **注册/注销**：运行时添加/移除障碍物，验证缓存正确更新
4. **编辑器**：编辑器中移动障碍物属性/位置，验证 dirty 标记生效
5. **性能**：对比修改前后，50+ 静态障碍物场景的帧时间差异

---

## 实施顺序

1. **先修复 #1（RVO 到达行为）** — 影响用户可见行为，优先级更高
2. **再修复 #2（障碍物脏标记）** — 优化性质，功能无影响

## 编译与提交

每个修复完成后：
1. 运行 `pwsh -ExecutionPolicy Bypass -File QuickCompile.ps1` 确认编译通过
2. 记录到 `docs/DEV_NOTES.md`
3. 更新 `docs/TASK_LIST.md`
4. 分别提交（两个独立 commit）

---

*文档创建: 2026-03-21*
