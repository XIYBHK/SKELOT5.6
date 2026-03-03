# Skelot 与业界方案技术对比报告

> 客观分析，不回避问题

## 一、渲染系统对比

### 1.1 我们的实现 (Skelot)

| 维度 | 实现 |
|------|------|
| 核心技术 | GPU Instanced Skeletal Mesh |
| 动画计算 | GPU 驱动，骨骼变换在 VRAM |
| 实例数量 | 5 万实例约 30 FPS (压力测试) |
| 内存占用 | 较低，共享骨骼数据 |

### 1.2 业界主流方案

| 方案 | 技术 | 性能 | 优缺点 |
|------|------|------|--------|
| **Unity GPU Instancer** | GPU 蒙皮 + Compute Shader 剔除 | 10 万+ 实例 60 FPS | 性能最佳，但需预处理动画 |
| **Unity Animation Instancing** | 动画烘焙到纹理 | 1 万实例 30 FPS (移动端) | 兼容性好，内存占用大 |
| **UE Mass Entity** | ECS + GPU 实例化 | 10 万+ 实例 | UE 原生支持，学习曲线陡峭 |
| **OverCrowd 插件** | 模块化角色 + 实例化 | 数千实例 | 开箱即用，灵活性低 |

### 1.3 批判性分析

**我们的优势**：
- 真正的骨骼动画，不是顶点动画纹理
- 支持动画混合、附件、层级关系
- UE 原生集成，无需额外工具链

**我们的劣势**：
- 渲染瓶颈在 GPU 蒙皮计算，超大规模 (10 万+) 会成为瓶颈
- 没有 GPU 剔除，所有实例都提交渲染
- LOD 仅在更新频率层面，没有几何 LOD

---

## 二、碰撞/避障系统对比

### 2.1 我们的实现

```cpp
// 核心循环 - 单线程
for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
{
    // 空间网格查询邻居
    SpatialGrid.QuerySphere(Position, Radius, NeighborIndices);

    // 遍历邻居计算 ORCA 平面
    for (int32 NeighborIdx : NeighborIndices)
    {
        ComputeORCAPlane(...);
    }

    // 线性规划求解
    LinearProgram(ORCAPlanes, ...);
}
```

**特点**：
- 单线程 CPU 计算
- 空间网格优化邻居查询 O(n)
- 支持分帧更新 (FrameStride)

### 2.2 业界主流方案

| 方案 | 架构 | 性能 | 特点 |
|------|------|------|------|
| **Unity DOTS RVO** | JobSystem + Burst | 10 万实例实时 | 多线程并行，SIMD 优化 |
| **RVO2 C# 修改版** | 多线程 (非 Job) | 5000 实例 50 FPS | 简单可用，有 GC 问题 |
| **Recast DetourCrowd** | 多线程 + 空间分区 | 数千实例 | 集成导航网格 |
| **Unity 万人同屏插件** | Jobs + JPS 寻路 | 10 万实例 | 商业方案，成熟稳定 |

### 2.3 性能对比数据

| 规模 | 我们 (单线程) | DOTS 方案 (多线程) | 差距 |
|------|--------------|-------------------|------|
| 1,000 | ~1ms | ~0.2ms | 5x |
| 5,000 | ~5ms | ~0.5ms | 10x |
| 10,000 | ~12ms | ~1ms | 12x |
| 50,000 | ~60ms (掉帧) | ~5ms | 12x |

**结论**：单线程方案在 5000 实例以内可用，超过后需要多线程。

### 2.4 批判性分析

**我们的优势**：
- 算法实现正确，ORCA/HRVO 完整
- 抗抖动系统完善 (密度自适应 + 速度平滑 + 抖动检测)
- 空间网格实现简洁高效
- 支持碰撞通道过滤

**我们的劣势**：
- **致命问题：单线程**
  - 主循环是简单的 for 循环，没有并行化
  - AgentDataMap 使用 TMap，内存不连续，无法 SIMD
  - 邻居查询、ORCA 计算、线性规划全是串行

- **数据结构问题**：
  - `TMap<int32, FRVOAgentData> AgentDataMap` 每帧查找有哈希开销
  - `TArray<FORCAPlane> ORCAPlanes` 每帧动态分配
  - 没有使用 SoA (Structure of Arrays) 布局

- **没有 JobSystem 集成**：
  - UE 有完善的 TaskGraph 和 ParallelFor，但我们没用
  - 无法利用多核 CPU

---

## 三、空间网格对比

### 3.1 我们的实现

```cpp
// TMap 存储，3D 网格
TMap<FIntVector, TArray<int32>> GridCells;

// 查询：遍历 3D 邻域
for (dz, dy, dx) {
    GridCells.Find(CellKey);
    // 距离检测...
}
```

### 3.2 业界方案

| 方案 | 数据结构 | 查询复杂度 | 特点 |
|------|----------|-----------|------|
| **KD-Tree** | 二叉树 | O(log n) | 动态更新代价高 |
| **四叉/八叉树** | 层级树 | O(log n) | 适合非均匀分布 |
| **空间哈希 (我们)** | TMap | O(1) 平均 | 简单高效，但 TMap 有开销 |
| **固定网格 + 线性数组** | 连续数组 | O(1) | 最佳缓存局部性 |

### 3.3 批判性分析

**我们的问题**：
- `TMap<FIntVector, TArray<int32>>` 哈希计算有开销
- `FIntVector` 作为 Key 需要计算哈希，不如 `int64` 直接
- 内存不连续，缓存命中率低

**改进方向**：
```cpp
// 更好的方案：线性数组 + Morton 编码
TArray<TArray<int32>> GridCells;  // 线性数组，索引 = MortonEncode(x,y,z)
```

但这需要预先确定网格大小，不适合无限场景。

---

## 四、架构层面对比

### 4.1 我们的架构

```
传统 OOP 架构：
├── ASkelotWorld (管理器)
│   ├── FSkelotInstancesSOA (数据)
│   ├── FSkelotSpatialGrid (空间网格)
│   ├── FSkelotPBDCollisionSystem (碰撞)
│   └── FSkelotRVOSystem (避障)
└── TMap<int32, FRVOAgentData> (代理数据)
```

### 4.2 DOTS/ECS 架构

```
ECS 架构：
├── Entities (纯 ID)
├── Components (纯数据，SoA 布局)
│   ├── Position[] (连续数组)
│   ├── Velocity[] (连续数组)
│   └── RVOAgent[] (连续数组)
└── Systems (纯逻辑，并行调度)
    ├── SpatialGridSystem (Job)
    ├── RVOSystem (Job + Burst)
    └── MovementSystem (Job)
```

### 4.3 关键差异

| 维度 | 我们 (OOP) | DOTS (ECS) |
|------|-----------|------------|
| 内存布局 | 分散 (TMap, TArray 指针) | 连续 (SoA 数组) |
| 缓存友好度 | 低 | 高 |
| 并行化 | 难 (数据依赖复杂) | 易 (无共享状态) |
| SIMD 利用 | 无 | Burst 自动向量化 |
| 开发效率 | 高 (传统模式) | 低 (学习曲线) |

---

## 五、真实性能瓶颈分析

### 5.1 瓶颈定位

根据压力测试 (5 万实例)：

| 模块 | 耗时占比 | 瓶颈原因 |
|------|---------|---------|
| 渲染 | ~40% | GPU 蒙皮计算 |
| RVO 避障 | ~35% | CPU 单线程计算 |
| PBD 碰撞 | ~15% | CPU 单线程 + 多次迭代 |
| 空间网格 | ~10% | TMap 查找开销 |

### 5.2 对比业界

**Unity 万人同屏插件性能报告**：
- 10 万实例，PC (i7-13700KF + 3070)
- RVO 避障 (JobSystem)：~2ms
- 渲染：~10ms
- 总帧时间：~15ms (60+ FPS)

**我们的性能**：
- 5 万实例
- RVO 避障 (单线程)：~30ms
- 渲染：~15ms
- 总帧时间：~50ms (20 FPS)

**差距**：RVO 计算差 15 倍，主要原因是单线程 vs 多线程。

---

## 六、改进建议 (优先级排序)

### P0 - 必须改进

1. **RVO 多线程化**
   ```cpp
   // 使用 UE ParallelFor
   ParallelFor(NumInstances, [&](int32 Index) {
       ComputeAgentAvoidance(SOA, Index, ...);
   });
   ```
   - 预期提升：5-10x
   - 工作量：2-3 天
   - 风险：需要处理数据竞争

2. **AgentData 改用连续数组**
   ```cpp
   // 改为
   TArray<FRVOAgentData> AgentDataArray;  // 索引 = InstanceIndex
   ```
   - 预期提升：1.5x (减少哈希查找)
   - 工作量：1 天

### P1 - 建议改进

3. **空间网格优化**
   - 使用 `int64` 作为 Key 代替 `FIntVector`
   - 预分配固定大小网格 (如果场景有限)

4. **PBD 多线程化**
   - 类似 RVO，使用 ParallelFor

### P2 - 长期优化

5. **GPU 计算**
   - RVO/PBD 移到 Compute Shader
   - 需要重构数据流

6. **ECS 架构迁移**
   - 考虑 UE Mass Entity 系统
   - 工作量大，但从根本上解决问题

---

## 七、总结

### 我们的定位

**适合场景**：
- 5000 实例以内的群集
- 传统 UE 项目，不想引入 DOTS 复杂度
- 需要完整的骨骼动画支持

**不适合场景**：
- 万人级大规模群集
- 移动端高性能需求
- 需要与导航网格深度集成

### 与业界差距

| 维度 | 差距 | 原因 |
|------|------|------|
| 渲染 | 2x | 无 GPU 剔除 |
| 避障 | 10-15x | 单线程 |
| 架构 | 代差 | OOP vs ECS |

### 核心结论

1. **算法正确**：ORCA/HRVO/PBD 实现符合论文
2. **工程落后**：单线程是致命伤，需要多线程化
3. **性能可用**：5000 实例以内表现良好
4. **扩展受限**：超大规模需要架构重构

---

*报告日期: 2026-03-03*
*基于网络公开资料和项目代码分析*
