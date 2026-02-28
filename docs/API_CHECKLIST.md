# API 1:1 复刻清单 - 宂版

> 基于参考文档的完整API清单，> 创建日期: 2026-03-01

---

## 📋 完整API清单 (43项)

### 1. 实例管理 (5项)

| # | API名称 | 娡块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 1 | Skelot Create Instance | 实例管理 | ✅ 已有 | ✅ 已有 | Transform, RenderParams, UserObject |
| 2 | Skelot Create Instances | 实例管理 | ❌ 无 | ⬜ 需新增 | **批量创建** |
| 3 | Skelot Destroy Instance | 实例管理 | ✅ 已有 | ✅ 已有 | Handle |
| 4 | Skelot Destroy Instances | 实例管理 | ❌ 无 | ⬜ 需新增 | **批量销毁** |
| 5 | Skelot Set Lifespan | 实例管理 | ✅ 已有 | ✅ 已有 | Handle, Lifespan |

---

### 2. 变换操作 (3项)

| # | API名称 | 模块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 6 | Skelot Get Transform | 变换操作 | ✅ 已有 | ✅ 已有 | Handle |
| 7 | Skelot Set Transform | 变换操作 | ✅ 已有 | ✅ 已有 | Handle, Transform, bRelative |
| 8 | Skelot Get Socket Transform | 变换操作 | ✅ 已有 | ✅ 已有 | Handle, SocketOrBoneName, InMesh, bWorldSpace |

---

### 3. 动画系统 (2项) ⚠️ 参考文档新增

| # | API名称 | 模块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 9 | Skelot Play Animation | 动画系统 | ✅ 已有 | ⬜ 需封装 | 原版: InstancePlayAnimation |
| 10 | Skelot Get Anim Collection | 动画系统 | ✅ 已有 | ⬜ 需封装 | 原版: GetInstanceAnimCollection |

---

### 4. LOD 系统 (3项)

| # | API名称 | 模块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 11 | Skelot Set LOD Update Frequency Enabled | LOD系统 | ❌ 无 | ⬜ 需新增 | **全局开关** |
| 12 | Skelot Is LOD Update Frequency Enabled | LOD系统 | ❌ 无 | ⬜ 需新增 | |
| 13 | Skelot Set LOD Distances | LOD系统 | ❌ 无 | ⬜ 需新增 | MediumDist, FarDist |

---

### 5. 碰撞通道系统 (6项)

| # | API名称 | 模块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 14 | Skelot Set Instance Collision Mask | 碰撞通道 | ❌ 无 | ⬜ 需新增 | InstanceIndex, Mask |
| 15 | Skelot Set Instance Collision Mask By Handle | 碰撞通道 | ❌ 无 | ⬜ 需新增 | |
| 16 | Skelot Get Instance Collision Mask | 碰撞通道 | ❌ 无 | ⬜ 需新增 | InstanceIndex |
| 17 | Skelot Set Instance Collision Channel | 碰撞通道 | ❌ 无 | ⬜ 需新增 | InstanceIndex, Channel |
| 18 | Skelot Set Instance Collision Channel By Handle | 碰撞通道 | ❌ 无 | ⬜ 需新增 | |
| 19 | Skelot Get Instance Collision Channel | 碰撞通道 | ❌ 无 | ⬜ 需新增 | InstanceIndex |

---

### 6. 移动系统 (6项) ⚠️ 参考文档新增
| # | API名称 | 模块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 20 | Skelot Set Instance Velocity | 移动系统 | ❌ 无 | ⬜ 需新增 | **新功能** |
| 21 | Skelot Set Instance Velocity By Index | 移动系统 | ❌ 无 | ⬜ 需新增 | **新功能** |
| 22 | Skelot Get Instance Velocity | 移动系统 | ❌ 无 | ⬜ 需新增 | **新功能** |
| 23 | Skelot Get Instance Velocity By Index | 移动系统 | ❌ 无 | ⬜ 需新增 | **新功能** |
| 24 | Skelot Set Instance Velocities | 移动系统 | ❌ 无 | ⬜ 需新增 | **批量** |
| 25 | Skelot Set Instance Velocities By Index | 移动系统 | ❌ 无 | ⬜ 需新增 | **批量** |

---

### 7. 空间检测 (3项) ⚠️ 参考文档新增
| # | API名称 | 模块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 26 | Skelot Query Location Overlapping Sphere | 空间检测 | ❌ 无 | ⬜ 需新增 | Center, Radius, Instances(out), CollisionMask |
| 27 | Skelot Set Spatial Grid Cell Size | 空间检测 | ❌ 无 | ⬜ 需新增 | CellSize |
| 28 | Skelot Get Spatial Grid Cell Size | 空间检测 | ❌ 无 | ⬜ 需新增 | |

---

### 8. 层级关系 (4项)

| # | API名称 | 模块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 29 | Skelot Attach Child | 层级关系 | ✅ 已有 | ✅ 已有 | 原版: InstanceAttachChild |
| 30 | Skelot Detach From Parent | 层级关系 | ✅ 已有 | ✅ 已有 | 原版: DetachInstanceFromParent |
| 31 | Skelot Get Children | 层级关系 | ⚠️ 部分 | ⬜ 需封装 | 原版: GetInstanceChildren (模板) |
| 32 | Skelot Get Parent | 层级关系 | ⚠️ 部分 | ⬜ 需封装 | 原版: GetInstanceParentIndex |

---

### 9. 几何工具 (8项)

| # | API名称 | 模块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 33 | Get Bezier Point | 几何工具 | ❌ 无 | ⬜ 需新增 | Points[], Progress |
| 34 | Get Points By Shape | 几何工具 | ❌ 无 | ⬜ 需新增 | Shape, Distance, bSurfaceOnly, Noise |
| 35 | Get Points By Round | 几何工具 | ❌ 无 | ⬜ 需新增 | Origin, Radius, Distance, Noise |
| 36 | Get Points By Grid | 几何工具 | ❌ 无 | ⬜ 需新增 | Origin, DistanceX/Y/Z, CountX/Y/Z |
| 37 | Get Points By Mesh | 几何工具 | ❌ 无 | ⬜ 需新增 | Mesh, Transform, Distance, Noise, LODIndex |
| 38 | Get Points By Mesh Voxel | 几何工具 | ❌ 无 | ⬜ 需新增 | Mesh, Transform, VoxelSize, bSolid, Noise, LODIndex |
| 39 | Get Points By Spline | 几何工具 | ❌ 无 | ⬜ 需新增 | Spline, CountX
 CountY
 Width, Noise |
| 40 | Get Pixels By Texture | 几何工具 | ❌ 无 | ⬜ 需新增 | Texture, SampleStep |

---

### 10. 配置Actor (3项)

| # | API名称 | 模块 | 原版状态 | 任务状态 | 备注 |
|---|---------|------|----------|----------|------|
| 41 | Skelot PBD Plane | 配置Actor | ❌ 无 | ⬜ 需新增 | 场景配置 |
| 42 | Skelot Sphere Obstacle | 配置Actor | ❌ 无 | ⬜ 需新增 | 球形障碍物 |
| 43 | Skelot Box Obstacle | 配置Actor | ❌ 无 | ⬜ 需新增 | 盒形障碍物 |

---

## 📊 统计汇总

| 分类 | 原版已有 | 需新增 | 需封装 | 总计 |
|------|----------|----------|----------|------|
| 宂例管理 | 5 | 2 | 0 | 0 | 7 |
| 变换操作 | 3 | 3 | 0 | 0 | 6 |
| 动画系统 | 2 | 2 | 0 | 0 | 4 |
| LOD系统 | 0 | 3 | 0 | 0 | 3 |
| 碰撞通道 | 0 | 6 | 0 | 0 | 6 |
| 移动系统 | 0 | 6 | 0 | 0 | 6 |
| 空间检测 | 0 | 3 | 0 | 0 | 3 |
| 层级关系 | 4 | 2 | 2 | 0 | 6 |
| 几何工具 | 0 | 8 | 0 | 0 | 8 |
| 配置Actor | 0 | 3 | 0 | 0 | 3 |
| **总计** | **20** | **28** | **10** | **10** |

---

## 🔴 需新增的API (28项)

### 优先级 P0 (核心)

| # | API | 模块 | 实现难度 |
|---|-----|------|------|
| 1 | Skelot Create Instances | 实例管理 | 低 |
| 2 | Skelot Destroy Instances | 实例管理 | 低 |
| 3 | Skelot Play Animation | 动画系统 | 低 (封装) |
| 4 | Skelot Get Anim Collection | 动画系统 | 低 (封装) |
| 5 | Skelot Set Instance Velocity | 移动系统 | 中 |
| 6 | Skelot Set Instance Velocity By Index | 移动系统 | 中 |
| 7 | Skelot Get Instance Velocity | 移动系统 | 中 |
| 8 | Skelot Get Instance Velocity By Index | 移动系统 | 中 |
| 9 | Skelot Set Instance Collision Mask | 碰撞通道 | 低 |
| 10 | Skelot Get Instance Collision Mask | 碰撞通道 | 低 |
| 11 | Skelot Set Instance Collision Channel | 碰撞通道 | 低 |
| 12 | Skelot Get Instance Collision Channel | 碰撞通道 | 低 |
| 13 | Skelot Query Location Overlapping Sphere | 空间检测 | 中 |
| 14 | Skelot Set Spatial Grid Cell Size | 空间检测 | 低 |
| 15 | Skelot Get Spatial Grid Cell Size | 空间检测 | 低 |

### 优先级 P1 (扩展)

| # | API | 模块 | 实现难度 |
|---|-----|------|------|
| 16 | Skelot Set Instance Collision Mask By Handle | 碰撞通道 | 低 |
| 17 | Skelot Set Instance Collision Channel By Handle | 碰撞通道 | 低 |
| 18 | Skelot Set Instance Velocities | 移动系统 | 中 |
| 19 | Skelot Set Instance Velocities By Index | 移动系统 | 中 |
| 20 | Skelot Get Children | 层级关系 | 低 (封装) |
| 21 | Skelot Get Parent | 层级关系 | 低 (封装) |
| 22 | Skelot Set LOD Update Frequency Enabled | LOD系统 | 低 |
| 23 | Skelot Is LOD Update Frequency Enabled | LOD系统 | 低 |
| 24 | Skelot Set LOD Distances | LOD系统 | 低 |
| 25 | Skelot PBD Plane | 配置Actor | 中 |

### 优先级 P2 (工具)

| # | API | 模块 | 实现难度 |
|---|-----|------|------|
| 26 | Skelot Sphere Obstacle | 配置Actor | 低 |
| 27 | Skelot Box Obstacle | 配置Actor | 低 |
| 28 | Get Bezier Point | 几何工具 | 低 |
| 29 | Get Points By Shape | 几何工具 | 中 |
| 30 | Get Points By Round | 几何工具 | 低 |
| 31 | Get Points By Grid | 几何工具 | 低 |
| 32 | Get Points By Mesh | 几何工具 | 中 |
| 33 | Get Points By Mesh Voxel | 几何工具 | 中 |
| 34 | Get Points By Spline | 几何工具 | 中 |
| 35 | Get Pixels By Texture | 几何工具 | 低 |

---

## 📋 任务列表更新建议

根据对比分析，需要更新 `TASK_LIST.md`：

1. **Phase 1.5 动画系统** - 补充蓝图封装任务
2. **Phase 2.5 层级关系** - 补充蓝图封装任务
3. **Phase 3 空间检测** - 已覆盖， 但需要细化实现
4. **Phase 4 PBD 系统** - 补充 `Skelot PBD Plane` Actor
5. **Phase 7 几何工具** - 补充所有几何工具的详细实现
6. **Phase 8 编辑器扩展** - 补充快速资产创建的完整流程

---

## 🔧 声明

- **原版已有** 的API 可大部分已工作已经完成， 只需要封装为蓝图API
- **需新增的API** 需要完整实现
- **需封装的API** 需要创建蓝图友好的接口
- **参考文档与参考文档的参数细节需补充到任务中
