# API 1:1 复刻清单

> 创建日期: 2026-03-01  
> 修订日期: 2026-03-04

---

## 统计

- 总计: 43 项
- 已完成: 43 项
- 待完成: 0 项

---

## 1. 实例管理 (5/5)

- [x] Skelot Create Instance
- [x] Skelot Create Instances
- [x] Skelot Destroy Instance
- [x] Skelot Destroy Instances
- [x] Skelot Set Lifespan

## 2. 变换操作 (3/3)

- [x] Skelot Get Transform
- [x] Skelot Set Transform
- [x] Skelot Get Socket Transform

## 3. 动画系统 (2/2)

- [x] Skelot Play Animation
- [x] Skelot Get Anim Collection

## 4. LOD 系统 (3/3)

- [x] Skelot Set LOD Update Frequency Enabled
- [x] Skelot Is LOD Update Frequency Enabled
- [x] Skelot Set LOD Distances

## 5. 碰撞通道系统 (6/6)

- [x] Skelot Set Instance Collision Mask
- [x] Skelot Set Instance Collision Mask By Handle
- [x] Skelot Get Instance Collision Mask
- [x] Skelot Set Instance Collision Channel
- [x] Skelot Set Instance Collision Channel By Handle
- [x] Skelot Get Instance Collision Channel

## 6. 移动系统 (6/6)

- [x] Skelot Set Instance Velocity
- [x] Skelot Set Instance Velocity By Index
- [x] Skelot Get Instance Velocity
- [x] Skelot Get Instance Velocity By Index
- [x] Skelot Set Instance Velocities
- [x] Skelot Set Instance Velocities By Index

## 7. 空间检测 (4/4)

- [x] Skelot Query Location Overlapping Sphere
- [x] Skelot Query Location Overlapping Box
- [x] Skelot Set Spatial Grid Cell Size
- [x] Skelot Get Spatial Grid Cell Size

## 8. 层级关系 (4/4)

- [x] Skelot Attach Child
- [x] Skelot Detach From Parent
- [x] Skelot Get Children
- [x] Skelot Get Parent

## 9. 几何工具 (8/8)

- [x] Get Bezier Point
- [x] Get Points By Shape
- [x] Get Points By Round
- [x] Get Points By Grid
- [x] Get Points By Mesh
- [x] Get Points By Mesh Voxel
- [x] Get Points By Spline
- [x] Get Pixels By Texture

## 10. 配置 Actor (3/3)

- [x] Skelot PBD Plane
- [x] Skelot Sphere Obstacle
- [x] Skelot Box Obstacle

---

## 备注

- 参数与字段命名以源码为准（如 `FSkelotAnimPlayParams`, `PlayScale`, `StartAt`, `TransitionDuration`）。
- 蓝图函数签名以 `USkelotWorldSubsystem` 为准。
