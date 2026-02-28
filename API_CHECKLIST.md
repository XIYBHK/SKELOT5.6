# API 1:1 复刻清单

> 基于参考文档的完整API清单，逐项核对
> 创建日期: 2026-03-01

---

## 📋 完整API清单 (43项)

### 1. 实例管理 (5项)

| # | API名称 | 参数 | 返回值 | 任务状态 | 备注 |
|---|---------|------|--------|----------|------|
| 1 | Skelot Create Instance | Transform, RenderParams, UserObject | Handle | ✅已有 | 原版有 |
| 2 | Skelot Create Instances | Transforms[], RenderParams | Handle[] | ⬜需新增 | **批量创建** |
| 3 | Skelot Destroy Instance | Handle | void | ✅已有 | 原版有 |
| 4 | Skelot Destroy Instances | Handles[] | void | ⬜需新增 | **批量销毁** |
| 5 | Skelot Set Lifespan | Handle, Lifespan | void | ✅已有 | 原版有 |

---

### 2. 变换操作 (3项)

| # | API名称 | 参数 | 返回值 | 任务状态 | 备注 |
|---|---------|------|--------|----------|------|
| 6 | Skelot Get Transform | Handle | FTransform | ✅已有 | 原版有 |
| 7 | Skelot Set Transform | Handle, Transform, bRelative | void | ✅已有 | 原版有 |
| 8 | Skelot Get Socket Transform | Handle, SocketOrBoneName, InMesh, bWorldSpace | FTransform | ✅已有 | 原版有 |

---

### 3. 动画系统 (2项) ⚠️ 遗漏

| # | API名称 | 参数 | 返回值 | 任务状态 | 备注 |
|---|---------|------|--------|----------|------|
| 9 | Skelot Play Animation | Handle, Params(Animation,bLoop,PlayRate,StartPosition,BlendInTime) | float(时长) | ⬜需新增 | **蓝图封装** |
| 10 | Skelot Get Anim Collection | Handle | AnimCollection | ⬜需新增 | **蓝图封装** |

**Params 结构体详情:**
```cpp
struct FSkelotAnimPlayParams
{
    UAnimSequenceBase* Animation;
    bool bLoop = true;
    float PlayRate = 1.0f;
    float StartPosition = 0.0f;
    float BlendInTime = 0.2f;
};
```

---

### 4. LOD 系统 (3项)

| # | API名称 | 参数 | 返回值 | 任务状态 | 备注 |
|---|---------|------|--------|----------|------|
| 11 | Skelot Set LOD Update Frequency Enabled | bEnable | void | ⬜需新增 | **新功能** |
| 12 | Skelot Is LOD Update Frequency Enabled | - | bool | ⬜需新增 | **新功能** |
| 13 | Skelot Set LOD Distances | MediumDist, FarDist | void | ⬜需新增 | **新功能** |

---

### 5. 碰撞通道系统 (6项)

| # | API名称 | 参数 | 返回值 | 任务状态 | 备注 |
|---|---------|------|--------|----------|------|
| 14 | Skelot Set Instance Collision Mask | InstanceIndex, Mask | void | ⬜需新增 | **新功能** |
| 15 | Skelot Set Instance Collision Mask By Handle | Handle, Mask | void | ⬜需新增 | **新功能** |
| 16 | Skelot Get Instance Collision Mask | InstanceIndex | uint8 | ⬜需新增 | **新功能** |
| 17 | Skelot Set Instance Collision Channel | InstanceIndex, Channel | void | ⬜需新增 | **新功能** |
| 18 | Skelot Set Instance Collision Channel By Handle | Handle, Channel | void | ⬜需新增 | **新功能** |
| 19 | Skelot Get Instance Collision Channel | InstanceIndex | uint8 | ⬜需新增 | **新功能** |

**碰撞判定逻辑:**
```cpp
// 碰撞条件: (MaskA & (1 << ChannelB)) && (MaskB & (1 << ChannelA))
bool ShouldCollide(uint8 MaskA, uint8 ChannelA, uint8 MaskB, uint8 ChannelB)
{
    return (MaskA & (1 << ChannelB)) && (MaskB & (1 << ChannelA));
}
```

---

### 6. 移动系统 (6项)

| # | API名称 | 参数 | 返回值 | 任务状态 | 备注 |
|---|---------|------|--------|----------|------|
| 20 | Skelot Set Instance Velocity | Handle, Velocity | void | ⬜需新增 | **新功能** |
| 21 | Skelot Set Instance Velocity By Index | InstanceIndex, Velocity | void | ⬜需新增 | **新功能** |
| 22 | Skelot Get Instance Velocity | Handle | FVector | ⬜需新增 | **新功能** |
| 23 | Skelot Get Instance Velocity By Index | InstanceIndex | FVector | ⬜需新增 | **新功能** |
| 24 | Skelot Set Instance Velocities | Handles[], Velocities[] | void | ⬜需新增 | **批量** |
| 25 | Skelot Set Instance Velocities By Index | Indices[], Velocities[] | void | ⬜需新增 | **批量** |

---

### 7. 空间检测 (3项)

| # | API名称 | 参数 | 返回值 | 任务状态 | 备注 |
|---|---------|------|--------|----------|------|
| 26 | Skelot Query Location Overlapping Sphere | Center, Radius, Instances(out), CollisionMask | void | ⬜需新增 | **新功能** |
| 27 | Skelot Set Spatial Grid Cell Size | CellSize | void | ⬜需新增 | **新功能** |
| 28 | Skelot Get Spatial Grid Cell Size | - | float | ⬜需新增 | **新功能** |

---

### 8. 层级关系 (4项) ⚠️ 部分遗漏

| # | API名称 | 参数 | 返回值 | 任务状态 | 备注 |
|---|---------|------|--------|----------|------|
| 29 | Skelot Attach Child | Parent, Child, SocketOrBoneName, Transform, bKeepWorldTransform | void | ✅已有 | 原版有 |
| 30 | Skelot Detach From Parent | Handle | void | ✅已有 | 原版有 |
| 31 | Skelot Get Children | Handle | Children[] | ⬜需新增 | **新功能** |
| 32 | Skelot Get Parent | Handle | Handle | ⬜需新增 | **新功能** |

---

### 9. 几何工具 (8项)

| # | API名称 | 参数 | 返回值 | 任务状态 | 备注 |
|---|---------|------|--------|----------|------|
| 33 | Get Bezier Point | Points[], Progress | FVector | ⬜需新增 | **新功能** |
| 34 | Get Points By Shape | Shape, Distance, bSurfaceOnly, Noise | FVector[] | ⬜需新增 | **新功能** |
| 35 | Get Points By Round | Origin, Radius, Distance, Noise | FVector[] | ⬜需新增 | **新功能** |
| 36 | Get Points By Grid | Origin, DistanceX, CountX, CountY, DistanceY, CountZ, DistanceZ | FVector[] | ⬜需新增 | **新功能** |
| 37 | Get Points By Mesh | Mesh, Transform, Distance, Noise, LODIndex | FVector[] | ⬜需新增 | **新功能** |
| 38 | Get Points By Mesh Voxel | Mesh, Transform, VoxelSize, bSolid, Noise, LODIndex | FVector[] | ⬜需新增 | **新功能** |
| 39 | Get Points By Spline | Spline, CountX, CountY, Width, Noise | FVector[] | ⬜需新增 | **新功能** |
| 40 | Get Pixels By Texture | Texture, SampleStep | PixelData[] | ⬜需新增 | **新功能** |

---

### 10. 配置 Actor (3项)

| # | Actor名称 | 功能 | 任务状态 | 备注 |
|---|-----------|------|----------|------|
| 41 | Skelot PBD Plane | 场景PBD/RVO配置 | ⬜需新增 | **新功能** |
| 42 | Skelot Sphere Obstacle | 球形障碍物 | ⬜需新增 | **新功能** |
| 43 | Skelot Box Obstacle | 盒形障碍物 | ⬜需新增 | **新功能** |

---

## 📊 统计汇总

| 分类 | 总数 | 原版已有 | 需新增 | 完成度 |
|------|------|----------|--------|--------|
| 实例管理 | 5 | 3 | 2 | 60% |
| 变换操作 | 3 | 3 | 0 | 100% |
| 动画系统 | 2 | 0 | 2 | 0% |
| LOD 系统 | 3 | 0 | 3 | 0% |
| 碰撞通道 | 6 | 0 | 6 | 0% |
| 移动系统 | 6 | 0 | 6 | 0% |
| 空间检测 | 3 | 0 | 3 | 0% |
| 层级关系 | 4 | 2 | 2 | 50% |
| 几何工具 | 8 | 0 | 8 | 0% |
| 配置Actor | 3 | 0 | 3 | 0% |
| **总计** | **43** | **8** | **35** | **19%** |

---

## 📋 需新增的35项API清单

### 优先级 P0 (核心功能)

| # | API | 模块 |
|---|-----|------|
| 1 | Skelot Create Instances | 实例管理 |
| 2 | Skelot Destroy Instances | 实例管理 |
| 3 | Skelot Play Animation | 动画系统 |
| 4 | Skelot Get Anim Collection | 动画系统 |
| 5 | Skelot Set Instance Velocity | 移动系统 |
| 6 | Skelot Set Instance Velocity By Index | 移动系统 |
| 7 | Skelot Get Instance Velocity | 移动系统 |
| 8 | Skelot Get Instance Velocity By Index | 移动系统 |
| 9 | Skelot Set Instance Collision Mask | 碰撞通道 |
| 10 | Skelot Get Instance Collision Mask | 碰撞通道 |
| 11 | Skelot Set Instance Collision Channel | 碰撞通道 |
| 12 | Skelot Get Instance Collision Channel | 碰撞通道 |
| 13 | Skelot Query Location Overlapping Sphere | 空间检测 |
| 14 | Skelot Set Spatial Grid Cell Size | 空间检测 |
| 15 | Skelot Get Spatial Grid Cell Size | 空间检测 |

### 优先级 P1 (扩展功能)

| # | API | 模块 |
|---|-----|------|
| 16 | Skelot Set Instance Collision Mask By Handle | 碰撞通道 |
| 17 | Skelot Set Instance Collision Channel By Handle | 碰撞通道 |
| 18 | Skelot Set Instance Velocities | 移动系统 |
| 19 | Skelot Set Instance Velocities By Index | 移动系统 |
| 20 | Skelot Get Children | 层级关系 |
| 21 | Skelot Get Parent | 层级关系 |
| 22 | Skelot Set LOD Update Frequency Enabled | LOD系统 |
| 23 | Skelot Is LOD Update Frequency Enabled | LOD系统 |
| 24 | Skelot Set LOD Distances | LOD系统 |
| 25 | Skelot PBD Plane Actor | 配置Actor |

### 优先级 P2 (工具功能)

| # | API | 模块 |
|---|-----|------|
| 26 | Skelot Sphere Obstacle Actor | 配置Actor |
| 27 | Skelot Box Obstacle Actor | 配置Actor |
| 28 | Get Bezier Point | 几何工具 |
| 29 | Get Points By Shape | 几何工具 |
| 30 | Get Points By Round | 几何工具 |
| 31 | Get Points By Grid | 几何工具 |
| 32 | Get Points By Mesh | 几何工具 |
| 33 | Get Points By Mesh Voxel | 几何工具 |
| 34 | Get Points By Spline | 几何工具 |
| 35 | Get Pixels By Texture | 几何工具 |

---

## ✅ 行动项

1. **更新 TASK_LIST.md** - 按35项API更新任务
2. **检查原版 API** - 确认哪些已有但未在任务中列出
3. **创建 API 实现顺序** - 按 P0 → P1 → P2 顺序

---

*API清单版本: 1.0 | 43项API | 35项需新增*
