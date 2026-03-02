# Skelot API 参考文档

> 版本: v0.9
> 更新日期: 2026-03-02

本文档描述 Skelot 插件的所有蓝图 API。在蓝图中搜索 `Skelot` 或直接使用下面的函数名即可找到。

---

## 目录

1. [实例管理](#1-实例管理)
2. [变换操作](#2-变换操作)
3. [动画系统](#3-动画系统)
4. [LOD 系统](#4-lod-系统)
5. [碰撞通道系统](#5-碰撞通道系统)
6. [移动系统](#6-移动系统)
7. [空间检测](#7-空间检测)
8. [层级关系](#8-层级关系)
9. [几何工具](#9-几何工具)
10. [调试工具](#10-调试工具)

---

## 1. 实例管理

### Skelot Create Instance

创建一个 Skelot 实例。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Transform | FTransform | 实例的世界变换（位置、旋转、缩放） |
| RenderParams | USkelotInstanceRenderDesc* | 渲染参数资产，定义外观和动画集 |
| UserObject | UObject* | 可选的用户自定义对象 |

**返回值**
| 类型 | 说明 |
|------|------|
| FSkelotInstanceHandle | 实例句柄，用于后续操作 |

---

### Skelot Create Instances

批量创建多个实例，比循环调用更高效。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Transforms | TArray\<FTransform\> | 变换数组 |
| RenderParams | USkelotInstanceRenderDesc* | 所有实例共用的渲染参数 |

**返回值**
| 类型 | 说明 |
|------|------|
| TArray\<FSkelotInstanceHandle\> | 句柄数组，顺序与输入对应 |

---

### Skelot Destroy Instance

销毁一个 Skelot 实例。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 要销毁的实例句柄 |

---

### Skelot Destroy Instances

批量销毁多个实例。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handles | TArray\<FSkelotInstanceHandle\> | 要销毁的句柄数组 |

---

### Skelot Set Lifespan

设置实例的生命周期，到期后自动销毁。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |
| Lifespan | float | 生命周期（秒），0表示取消自动销毁 |

---

## 2. 变换操作

### Skelot Get Transform

获取实例的世界变换。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |

**返回值**
| 类型 | 说明 |
|------|------|
| FTransform | 世界变换（位置、旋转、缩放） |

---

### Skelot Set Transform

设置实例的变换。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |
| Transform | FTransform | 新的变换 |
| bRelative | bool | false=世界变换，true=相对父级变换 |

---

### Skelot Get Socket Transform

获取骨骼或插槽的变换，用于在骨骼位置生成特效。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |
| SocketOrBoneName | FName | 骨骼或插槽名称 |
| InMesh | int32 | 可选，指定网格索引 |
| bWorldSpace | bool | true=世界空间，false=组件空间 |

**返回值**
| 类型 | 说明 |
|------|------|
| FTransform | 骨骼/插槽变换 |

---

## 3. 动画系统

### Skelot Play Animation

播放动画。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |
| Params | FSkelotAnimPlayParamsEx | 动画播放参数 |

**FSkelotAnimPlayParamsEx 结构**
| 字段 | 类型 | 说明 |
|------|------|------|
| Animation | UAnimSequence* | 动画序列 |
| bLoop | bool | 是否循环 |
| PlayScale | float | 播放速率（1.0为正常速度） |
| StartPosition | float | 起始位置（秒） |
| BlendInTime | float | 混合时间（秒） |

**返回值**
| 类型 | 说明 |
|------|------|
| float | 动画长度（秒），失败返回-1 |

---

### Skelot Get Anim Collection

获取实例关联的动画集资产。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |

**返回值**
| 类型 | 说明 |
|------|------|
| USkelotAnimCollection* | 动画集资产 |

---

## 4. LOD 系统

### Skelot Set LOD Update Frequency Enabled

启用或禁用基于距离的更新频率优化。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| bEnable | bool | true=启用，false=禁用（所有实例每帧更新） |

---

### Skelot Is LOD Update Frequency Enabled

检查LOD更新频率是否已启用。

**返回值**
| 类型 | 说明 |
|------|------|
| bool | true=已启用，false=已禁用 |

---

### Skelot Set LOD Distances

设置LOD距离阈值。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| MediumDist | float | 中距离阈值（厘米），超过后每2帧更新 |
| FarDist | float | 远距离阈值（厘米），超过后每4帧更新 |

---

## 5. 碰撞通道系统

Skelot 使用8个碰撞通道（Channel0-Channel7）控制实例间碰撞。

**核心概念**
- **碰撞通道**: 实例所属通道，即"我是谁"
- **碰撞掩码**: 与哪些通道碰撞，即"我和谁碰撞"
- **默认值**: 新实例属于Channel0，掩码0xFF（与所有通道碰撞）
- **碰撞判定**: `(MaskA & ChannelB) != 0 且 (MaskB & ChannelA) != 0`

### 碰撞通道枚举

| 通道 | 值 | 说明 |
|------|----|----|
| Channel0 | 0x01 | 默认通道 |
| Channel1 | 0x02 | 可配置通道 |
| Channel2 | 0x04 | 可配置通道 |
| Channel3 | 0x08 | 可配置通道 |
| Channel4 | 0x10 | 可配置通道 |
| Channel5 | 0x20 | 可配置通道 |
| Channel6 | 0x40 | 可配置通道 |
| Channel7 | 0x80 | 可配置通道 |

---

### Skelot Set Instance Collision Mask

设置碰撞掩码，控制与哪些通道碰撞。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| InstanceIndex | int32 | 实例索引 |
| Mask | uint8 | 碰撞掩码位标志 |

**掩码值参考**
| 值 | 说明 |
|----|----|
| 0x01 | 只与Channel0碰撞 |
| 0x02 | 只与Channel1碰撞 |
| 0xFF | 与所有通道碰撞（默认） |
| 0x00 | 不与任何通道碰撞 |

---

### Skelot Set Instance Collision Mask By Handle

通过句柄设置碰撞掩码。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |
| Mask | uint8 | 碰撞掩码 |

---

### Skelot Get Instance Collision Mask

获取当前碰撞掩码。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| InstanceIndex | int32 | 实例索引 |

**返回值**
| 类型 | 说明 |
|------|------|
| uint8 | 当前碰撞掩码 |

---

### Skelot Set Instance Collision Channel

设置实例所属的碰撞通道。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| InstanceIndex | int32 | 实例索引 |
| Channel | uint8 | 目标通道（使用枚举值 0x01-0x80） |

---

### Skelot Set Instance Collision Channel By Handle

通过句柄设置所属通道。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |
| Channel | uint8 | 目标通道 |

---

### Skelot Get Instance Collision Channel

获取当前所属通道。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| InstanceIndex | int32 | 实例索引 |

**返回值**
| 类型 | 说明 |
|------|------|
| uint8 | 当前所属通道 |

---

## 6. 移动系统

### Skelot Set Instance Velocity

设置速度向量，用于PBD/ORCA避障。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |
| Velocity | FVector | 速度向量（厘米/秒） |

---

### Skelot Set Instance Velocity By Index

通过索引设置速度，比Handle版本更快。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| InstanceIndex | int32 | 实例索引 |
| Velocity | FVector | 速度向量 |

---

### Skelot Get Instance Velocity

获取当前速度向量。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |

**返回值**
| 类型 | 说明 |
|------|------|
| FVector | 当前速度向量 |

---

### Skelot Get Instance Velocity By Index

通过索引获取速度。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| InstanceIndex | int32 | 实例索引 |

**返回值**
| 类型 | 说明 |
|------|------|
| FVector | 当前速度向量 |

---

### Skelot Set Instance Velocities

批量设置速度，比循环调用更高效。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handles | TArray\<FSkelotInstanceHandle\> | 句柄数组 |
| Velocities | TArray\<FVector\> | 速度数组，需一一对应 |

---

### Skelot Set Instance Velocities By Index

通过索引批量设置速度。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| InstanceIndices | TArray\<int32\> | 索引数组 |
| Velocities | TArray\<FVector\> | 速度数组 |

---

## 7. 空间检测

### Skelot Query Location Overlapping Sphere

查询球形范围内的实例，用于范围攻击、AOE技能。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Center | FVector | 球心位置（世界坐标） |
| Radius | float | 球体半径（厘米） |
| Instances | TArray\<FSkelotInstanceHandle\>& | 输出，查询到的句柄 |
| CollisionMask | uint8 | 掩码过滤，默认0xFF（全部通道） |

---

### Skelot Query Location Overlapping Box

查询盒形范围内的实例。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| BoxCenter | FVector | 盒子中心（世界坐标） |
| BoxExtent | FVector | 盒子半尺寸 |
| Instances | TArray\<FSkelotInstanceHandle\>& | 输出，查询到的句柄 |
| CollisionMask | uint8 | 掩码过滤，默认0xFF |

---

### Skelot Set Spatial Grid Cell Size

设置空间网格单元大小，影响查询性能。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| CellSize | float | 单元大小（厘米） |

**建议**: 设为查询半径的1-2倍

---

### Skelot Get Spatial Grid Cell Size

获取当前单元大小。

**返回值**
| 类型 | 说明 |
|------|------|
| float | 当前单元大小 |

---

## 8. 层级关系

### Skelot Attach Child

将实例附加到另一个实例，形成父子关系。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Parent | FSkelotInstanceHandle | 父实例句柄 |
| Child | FSkelotInstanceHandle | 子实例句柄 |
| SocketOrBoneName | FName | 附加到的骨骼/插槽名 |
| Transform | FTransform | 相对变换 |
| bKeepWorldTransform | bool | true=保持世界位置 |

---

### Skelot Detach From Parent

将实例从父级分离。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 要分离的句柄 |

---

### Skelot Get Children

获取所有子实例。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 父实例句柄 |

**返回值**
| 类型 | 说明 |
|------|------|
| TArray\<FSkelotInstanceHandle\> | 子实例句柄数组 |

---

### Skelot Get Parent

获取父实例。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Handle | FSkelotInstanceHandle | 实例句柄 |

**返回值**
| 类型 | 说明 |
|------|------|
| FSkelotInstanceHandle | 父实例句柄，无父级返回无效句柄 |

---

## 9. 几何工具

用于生成点阵，批量生成实例位置。

### Get Bezier Point

计算贝塞尔曲线上的点。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Points | TArray\<FVector\> | 控制点数组 |
| Progress | float | 曲线进度（0.0-1.0） |

**返回值**
| 类型 | 说明 |
|------|------|
| FVector | 曲线上的点坐标 |

---

### Get Points By Shape

在形状组件内生成均匀分布的点。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Shape | UShapeComponent* | 形状组件（球/盒/胶囊） |
| Distance | float | 点间距 |
| bSurfaceOnly | bool | true=仅表面，false=填充体积 |
| Noise | float | 随机偏移量，0=规则排列 |

**返回值**
| 类型 | 说明 |
|------|------|
| TArray\<FVector\> | 点坐标数组 |

---

### Get Points By Round

在XY平面圆形区域内生成点。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Origin | FVector | 圆心位置 |
| Radius | float | 圆半径 |
| Distance | float | 点间距 |
| Noise | float | 随机偏移量 |

**返回值**
| 类型 | 说明 |
|------|------|
| TArray\<FVector\> | 点坐标数组 |

---

### Get Points By Grid

按网格模式生成点（1D/2D/3D）。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Origin | FVector | 网格起点 |
| DistanceX | float | X方向间距 |
| CountX | int32 | X方向数量 |
| DistanceY | float | Y方向间距（1D时为0） |
| CountY | int32 | Y方向数量（1D时为1） |
| DistanceZ | float | Z方向间距（2D时为0） |
| CountZ | int32 | Z方向数量（2D时为1） |

**返回值**
| 类型 | 说明 |
|------|------|
| TArray\<FVector\> | 点坐标数组 |

---

### Get Points By Mesh

在静态网格表面生成随机点。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Mesh | UStaticMesh* | 静态网格资产 |
| Transform | FTransform | 网格变换 |
| Distance | float | 目标点间距 |
| Noise | float | 随机偏移量 |
| LODIndex | int32 | LOD级别（0为最高） |

**返回值**
| 类型 | 说明 |
|------|------|
| TArray\<FVector\> | 点坐标数组 |

**注意**: 仅编辑器可用

---

### Get Points By Mesh Voxel

在静态网格上生成体素化点。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Mesh | UStaticMesh* | 静态网格资产 |
| Transform | FTransform | 网格变换 |
| VoxelSize | float | 体素大小 |
| bSolid | bool | true=实心，false=外壳 |
| Noise | float | 随机偏移量 |
| LODIndex | int32 | LOD级别 |

**返回值**
| 类型 | 说明 |
|------|------|
| TArray\<FVector\> | 点坐标数组 |

**注意**: 仅编辑器可用

---

### Get Points By Spline

沿样条曲线生成点。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Spline | USplineComponent* | 样条组件 |
| CountX | int32 | 沿样条方向点数 |
| CountY | int32 | 垂直方向点数（形成带状） |
| Width | float | 带状宽度 |
| Noise | float | 随机偏移量 |

**返回值**
| 类型 | 说明 |
|------|------|
| TArray\<FVector\> | 点坐标数组 |

---

### Get Pixels By Texture

从纹理获取像素数据。

**参数**
| 参数 | 类型 | 说明 |
|------|------|------|
| Texture | UTexture2D* | 纹理资产 |
| SampleStep | int32 | 采样步长，1=全部，3=每3个取1个 |

**返回值**
| 类型 | 说明 |
|------|------|
| TArray\<FSkelotPixelData\> | 像素数据（颜色和UV） |

---

## 10. 调试工具

通过控制台命令使用调试工具。

| 命令 | 说明 |
|------|------|
| `Skelot.DrawAllBounds` | 绘制所有实例包围盒 |
| `Skelot.DrawSpatialGrid` | 绘制空间网格 |
| `Skelot.DrawCollisionRadius` | 绘制PBD碰撞半径 |
| `Skelot.DrawVelocities` | 绘制速度向量 |
| `Skelot.DrawNeighborLinks` | 绘制邻居连接线 |
| `Skelot.Stats` | 打印统计信息到日志 |
| `Skelot.DebugMode [0-255]` | 设置调试模式位掩码 |
| `Skelot.DebugDrawDistance [距离]` | 设置调试绘制距离 |

---

*最后更新: 2026-03-02*
