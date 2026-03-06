# ORCA 求解器重写记录

> 基于 RVO2 库 (github.com/snape/RVO2) 的 Agent.cc 重写 Skelot ORCA 避障核心算法
> 日期: 2026-03-06

---

## 背景

原始 ORCA 实现存在 5 个核心数学错误，导致避障行为不正确：

1. ORCA 半平面构建缺少 `TimeHorizon` 缩放和 `u` 修正向量
2. 线性规划约束检查使用 `DotProduct` 而非 `det`（2D 行列式）
3. LP1 交集公式完全错误（分子依赖 tLeft，使用 DotProduct 而非 det）
4. LP1 始终取 tLeft，缺少 optVelocity 投影选择最优点
5. LP3 逐个投影不保证全局可行性

## 参照标准

RVO2 库 Apache 2.0 许可，核心算法文件：
- `Agent.cc` — `computeNewVelocity()`, `linearProgram1()`, `linearProgram2()`, `linearProgram3()`
- `Vector2.h` — `det()` (2D 行列式), `absSq()`, `normalize()`
- `Line.h` — 有向线段结构 (point + direction)

## 决策：为什么不直接集成 RVO2 库

| 方案 | 评估 |
|------|------|
| 集成 RVO2 库源码 | KdTree 与 SpatialGrid 重复；RVO2 单线程，我们已有 ParallelFor；需要 SOA ↔ Agent 数据转换 |
| 使用 UE AvoidanceManager | 设计目标是 UMovementComponent，不支持批量实例化渲染 |
| **参照 RVO2 修正数学** | 保留现有并行架构和 SpatialGrid，零依赖，只修正算法 |

## 数学基础

### Det2D — 2D 行列式（叉积）

```cpp
// 对应 RVO2 的 det(a, b)
static FORCEINLINE float Det2D(const FVector2f& A, const FVector2f& B)
{
    return A.X * B.Y - A.Y * B.X;
}
```

几何含义：
- `Det2D(A, B) > 0`：B 在 A 的**左侧**
- `Det2D(A, B) < 0`：B 在 A 的**右侧**
- `Det2D(A, B) = 0`：A 和 B **共线**

用于 ORCA 的核心判定：
- 半平面约束违反检查：`Det2D(direction, point - result) > 0` 表示 result 在约束允许区域外
- LP1 两条约束线的交点计算

### RVO2 → UE 类型映射

| RVO2 | UE (Skelot) | 说明 |
|------|-------------|------|
| `det(a, b)` | `Det2D(A, B)` | 2D 行列式 |
| `a * b` (operator) | `FVector2f::DotProduct(A, B)` | 点积 |
| `absSq(a)` | `A.SquaredLength()` | 长度平方 |
| `abs(a)` | `A.Size()` | 长度 |
| `normalize(a)` | `A.GetSafeNormal()` | 归一化 |
| `RVO::Line` | `FORCAPlane` | 有向线段 (Point + Direction) |

---

## 修改详情

### 1. ComputeORCAPlaneInternal

**文件**: `SkelotRVOSystem.cpp` 第 274-343 行

**对照**: RVO2 `Agent::computeNewVelocity()` agent ORCA lines 构建部分

**核心逻辑**（参照 RVO2）：

```
invTimeHorizon = 1.0 / TimeHorizon

// w: 从 cutoff center 到相对速度的向量
w = relativeVelocity - invTimeHorizon * relativePosition

if (未碰撞 distSq > combinedRadiusSq):
    dotProduct1 = dot(w, relativePosition)

    if (dotProduct1 < 0 且 dotProduct1^2 > combinedRadiusSq * wLengthSq):
        // 投影到 cutoff circle
        unitW = normalize(w)
        direction = (unitW.y, -unitW.x)
        u = (combinedRadius * invTimeHorizon - wLength) * unitW
    else:
        // 投影到 leg
        leg = sqrt(distSq - combinedRadiusSq)
        if (det(relPos, w) > 0):
            // Left leg 方向
            direction = (relPos.x*leg - relPos.y*R, relPos.x*R + relPos.y*leg) / distSq
        else:
            // Right leg 方向（取反）
            direction = -(relPos.x*leg + relPos.y*R, -relPos.x*R + relPos.y*leg) / distSq
        u = dot(relVel, direction) * direction - relVel

else (已碰撞):
    invTimeStep = 1.0 / DeltaTime
    w = relativeVelocity - invTimeStep * relativePosition
    unitW = normalize(w)
    direction = (unitW.y, -unitW.x)
    u = (combinedRadius * invTimeStep - wLength) * unitW

// 关键：Point = 自身速度 + 责任比例 * u 修正
point = myVelocity + responsibilityFactor * u
```

**与旧代码的关键区别**：

| 项目 | 旧代码 | 新代码 |
|------|--------|--------|
| TimeHorizon | 未使用 | `invTimeHorizon = 1/TimeHorizon` 缩放 relPos |
| w 向量 | 不存在 | `w = relVel - invTimeHorizon * relPos` |
| u 修正 | 不存在 | 从 VO 边界到当前相对速度的最短修正向量 |
| Point | `RelVel2D * 0.5f` | `myVelocity + 0.5f * u` |
| Leg 方向 | 简化的切线近似 | 精确的 `sqrt(distSq - R^2)` 公式 |
| 碰撞处理 | 简单分离 | `invTimeStep` 缩放 + cutoff circle 投影 |

### 2. 统一 RVO/VO/HRVO

**旧架构**: 3 个独立函数 (`ComputeRVOPlane`, `ComputeVOPlane`, `ComputeHRVOPlane`)，各含完整 VO 构建逻辑（大量重复代码）

**新架构**: 1 个核心函数 + 责任系数参数

```cpp
// 路由层
void ComputeORCAPlane(...) {
    float ResponsibilityFactor = 0.5f;  // 默认 RVO（互惠）
    if (Config.bEnableHRVO && IsHeadOnCollision(...)) {
        ResponsibilityFactor = 1.0f;    // HRVO 迎面时切换为 VO
    }
    ComputeORCAPlaneInternal(..., ResponsibilityFactor, ...);
}

// 核心算法
void ComputeORCAPlaneInternal(..., float ResponsibilityFactor, ...) {
    // ... 计算 u 修正向量 ...
    OutPlane.Point = MyVel2D + ResponsibilityFactor * u;
    // RVO: 0.5*u (双方各半)  |  VO: 1.0*u (一方全责)
}
```

### 3. LinearProgram1（1D 约束求解）

**文件**: `SkelotRVOSystem.cpp` 第 363-428 行

**对照**: RVO2 `linearProgram1()`

**修正项**：

| 项目 | 旧代码 | 新代码 (RVO2) |
|------|--------|---------------|
| 交集分母 | `DotProduct(lineNo.dir, i.dir)` | `Det2D(lineNo.dir, i.dir)` |
| 交集分子 | `DotProduct(lineNo.dir, i.point) - DotProduct(...) * tLeft` | `Det2D(i.dir, lineNo.point - i.point)` |
| 平行线 | 无处理 | `abs(denom) <= eps` → `numerator < 0` 则无解 |
| 约束更新 | 只更新 tLeft | `denom >= 0` → 更新 tRight; `denom < 0` → 更新 tLeft |
| 最优点 | `point + dir * tLeft` | `t = dot(dir, optVel - point); clamp(t, tLeft, tRight)` |

### 4. LinearProgram2（2D 增量约束求解）

**文件**: `SkelotRVOSystem.cpp` 第 431-460 行

**对照**: RVO2 `linearProgram2()`

**修正项**：

| 项目 | 旧代码 | 新代码 (RVO2) |
|------|--------|---------------|
| 约束违反检查 | `DotProduct(dir, point) - DotProduct(dir, result) < 0` | `Det2D(dir, point - result) > 0` |
| 返回值 | `bool` | `int32`（失败行号，成功返回 Num()） |
| 初始 result | `PreferredVelocity` | 根据 `bDirectionOpt` 和速度圆正确初始化 |
| LP1 失败处理 | 直接进入 LP3 | 恢复 TempResult 后返回失败行号 |

### 5. LinearProgram3（回退求解）

**文件**: `SkelotRVOSystem.cpp` 第 462-504 行

**对照**: RVO2 `linearProgram3()`

**旧实现**（错误）：
```cpp
// 逐个投影 — 后续投影可能违反之前满足的约束
for (i = 0; i < PlaneNo; i++) {
    if (violated) InOutResult += Direction * correction;
}
clamp to speed circle;
```

**新实现**（参照 RVO2）：
```cpp
for (i = BeginLine; i < Planes.Num(); i++) {
    if (Det2D(dir, point - result) > distance) {
        // 构建投影约束集：前 i 条约束线的两两交集
        ProjLines = [];
        for (j = 0; j < i; j++) {
            det_ij = Det2D(lines[i].dir, lines[j].dir);
            if (parallel && same direction) continue;
            if (parallel && opposite) line.point = midpoint;
            else line.point = intersection;
            line.direction = normalize(lines[j].dir - lines[i].dir);
            ProjLines.add(line);
        }
        // 用投影约束集调用 LP2 求解
        LP2(ProjLines, radius, perpendicular_direction, true, result);
    }
}
```

关键区别：不是简单逐个投影，而是对每个违反的约束构建一组新的投影约束线，然后用 LP2 在这组约束下求解，确保全局可行性。

---

## 架构保留（未修改）

以下部分在重写中保持不变：

- **ParallelFor 并行架构**: `ComputeAvoidance` 主循环中的 InputVelocities/OutputVelocities 快照模式
- **SpatialGrid 邻居查询**: 未引入 RVO2 的 KdTree，继续使用空间哈希网格
- **AntiJitter 系统**: 密度自适应、速度平滑、抖动检测抑制
- **碰撞掩码过滤**: `ShouldAvoid` 双向通道检查
- **FSkelotRVOConfig**: `TimeHorizon` 参数已存在（默认 1.0s），重写后生效
- **FRVOAgentData**: 代理数据结构不变

---

## 代码量变化

```
SkelotRVOSystem.cpp: 237 行新增, 467 行删除 (净减 230 行)
SkelotRVOSystem.h:   删除 ComputeRVOPlane/ComputeVOPlane/ComputeHRVOPlane/BuildORCAPlanes/旧 LP2
```

合并三个独立 VO 构建函数为一个带参数的内部函数，消除了大量重复代码。

---

## 许可声明

算法参照 RVO2 Library (Apache License 2.0)，未直接复制源码。
实现为 Skelot 架构量身定制（SOA 数据布局、ParallelFor 并行、SpatialGrid 邻居查询）。

参考文献：
- Jur van den Berg, Stephen J. Guy, Jamie Snape, Ming C. Lin, Dinesh Manocha.
  "Reciprocal n-Body Collision Avoidance." Robotics Research, 2011.
- https://gamma.cs.unc.edu/RVO2/
- https://github.com/snape/RVO2
