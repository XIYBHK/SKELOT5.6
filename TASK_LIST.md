# Skelot 二次开发任务列表

> 基于 SkelotRbn 参考文档的功能实现计划
> 创建日期: 2026-02-28
> 状态说明: ⬜ 待开始 | 🔄 进行中 | ✅ 已完成 | ⏸️ 暂停

---

## 📋 项目概览

将原版 Skelot 插件扩展为高性能群集渲染插件，集成碰撞、空间查询、寻路避障功能。

### 核心目标
- [ ] PBD 碰撞系统
- [ ] ORCA/RVO 避障系统
- [ ] 空间检测系统
- [ ] 几何工具库
- [ ] 批量操作 API
- [ ] LOD 更新频率优化
- [ ] 碰撞通道系统
- [ ] 快速资产创建

---

## 🎯 Phase 1: 基础架构扩展

### 1.1 数据结构扩展
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 扩展 FSkelotInstancesSOA 添加速度数据 | P0 | ⬜ | 添加 `TArray<FVector3f> Velocities` |
| 扩展 FSkelotInstancesSOA 添加碰撞数据 | P0 | ⬜ | 添加碰撞通道(1字节)和碰撞掩码(1字节) |
| 添加批量创建 API | P0 | ⬜ | `CreateInstances(TArray<FTransform>, RenderParams)` |
| 添加批量销毁 API | P0 | ⬜ | `DestroyInstances(TArray<FSkelotInstanceHandle>)` |

### 1.2 移动系统基础
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| SetInstanceVelocity API | P0 | ⬜ | 设置单个实例速度 |
| GetInstanceVelocity API | P0 | ⬜ | 获取单个实例速度 |
| SetInstanceVelocities 批量API | P1 | ⬜ | 批量设置速度，性能优化 |
| 速度数据持久化 | P2 | ⬜ | SaveGame 支持 |

---

## 🎯 Phase 1.5: 动画系统扩展 (⚠️ 参考文档遗漏补充)

### 1.5.1 动画播放参数
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 定义 FSkelotAnimPlayParamsEx | P0 | ⬜ | Animation, bLoop, PlayRate, StartPosition, BlendInTime |
| 扩展 InstancePlayAnimation | P0 | ⬜ | 支持新的参数结构体 |
| 实现 BlendInTime 混合 | P1 | ⬜ | 动画过渡混合时间支持 |

### 1.5.2 动画蓝图 API
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| Skelot_PlayAnimation | P0 | ⬜ | 蓝图封装，返回动画长度 |
| Skelot_GetAnimCollection | P1 | ⬜ | 获取实例关联的动画集 |

---

## 🎯 Phase 2: 碰撞通道系统

### 2.1 碰撞通道数据结构
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 定义碰撞通道枚举 (8通道) | P0 | ⬜ | Channel0-Channel7, 值 0x01-0x80 |
| 扩展 SlotData 添加碰撞数据 | P0 | ⬜ | 添加 CollisionChannel(3bit) + CollisionMask(8bit) |
| 实现碰撞判定逻辑 | P0 | ⬜ | `(MaskA & (1<<ChannelB)) && (MaskB & (1<<ChannelA))` |

### 2.2 碰撞通道 API
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| SetInstanceCollisionChannel | P0 | ⬜ | 设置实例所属通道 |
| GetInstanceCollisionChannel | P0 | ⬜ | 获取实例所属通道 |
| SetInstanceCollisionMask | P0 | ⬜ | 设置碰撞掩码 |
| GetInstanceCollisionMask | P0 | ⬜ | 获取碰撞掩码 |
| Handle 版本 API | P1 | ⬜ | 所有通道 API 的 Handle 版本 |

---

## 🎯 Phase 2.5: 层级关系 API (⚠️ 参考文档遗漏补充)

### 2.5.1 层级操作 API
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| Skelot_AttachChild | P0 | ⬜ | 蓝图封装，支持 bKeepWorldTransform |
| Skelot_DetachFromParent | P0 | ⬜ | 蓝图封装 |
| Skelot_GetChildren | P1 | ⬜ | 新增：获取子实例数组 |
| Skelot_GetParent | P1 | ⬜ | 新增：获取父实例句柄 |

---

## 🎯 Phase 3: 空间检测系统

### 3.1 空间网格实现
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 设计空间哈希网格结构 | P0 | ⬜ | 参考: Spatial Hashing, O(n) 复杂度 |
| 实现网格单元大小配置 | P0 | ⬜ | SetSpatialGridCellSize/GetSpatialGridCellSize |
| 实现实例到网格的映射 | P0 | ⬜ | 每帧更新实例所在网格单元 |
| 实现邻居查询 | P0 | ⬜ | 获取当前单元+相邻单元的实例 |

### 3.2 空间查询 API
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| QueryLocationOverlappingSphere | P0 | ⬜ | 球形范围查询，支持掩码过滤 |
| QueryLocationOverlappingBox | P1 | ⬜ | 盒形范围查询 |
| QuerySphere 掩码过滤 | P0 | ⬜ | CollisionMask 参数过滤结果 |
| QueryBox 掩码过滤 | P1 | ⬜ | CollisionMask 参数过滤结果 |
| 空间网格调试可视化 | P2 | ⬜ | DrawDebug 网格边界 |

### 3.3 技术调研
| 任务 | 状态 | 说明 |
|------|------|------|
| ✅ 空间哈希算法 | 已完成 | 参考 [Spatial Hashing](https://m.php.cn/faq/1927461.html) |
| ⬜ Morton 编码优化 | 待调研 | Z-order curve 提升空间局部性 |
| ⬜ GPU 空间查询 | 待调研 | Compute Shader 实现 |

---

## 🎯 Phase 4: PBD 碰撞系统

### 4.1 PBD 核心算法
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 实现 PBD 碰撞检测 | P0 | ⬜ | 基于空间网格的邻居检测 |
| 实现位置约束求解 | P0 | ⬜ | 约束函数 C(x), 位置校正 Δx |
| 实现迭代求解器 | P0 | ⬜ | 多次迭代提高稳定性 |
| 实现松弛系数 | P0 | ⬜ | 控制约束响应强度 |

### 4.2 PBD 配置参数
| 任务 | 优先级 | 状态 | 默认值 |
|------|--------|------|--------|
| PBD 碰撞半径 | P0 | ⬜ | 50-80 |
| PBD 迭代次数 | P0 | ⬜ | 3 |
| 松弛系数 | P0 | ⬜ | 0.3-0.6 |
| 最大邻居数 | P1 | ⬜ | 64 |
| 速度投影 | P1 | ⬜ | 启用 |

### 4.3 PBD Actor 组件
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| SkelotPBDPlane Actor | P0 | ⬜ | 场景配置 Actor |
| SkelotSphereObstacle | P1 | ⬜ | 球形障碍物 |
| SkelotBoxObstacle | P1 | ⬜ | 盒形障碍物 |
| 障碍物与实例碰撞 | P1 | ⬜ | 实例避开障碍物 |

### 4.4 技术调研
| 任务 | 状态 | 说明 |
|------|------|------|
| ✅ PBD 算法原理 | 已完成 | 参考 [Position Based Dynamics](https://blog.csdn.net/gitblog_00393/article/details/151500546) |
| ⬜ GPU PBD 实现 | 待调研 | Compute Shader 加速 |
| ⬜ 密度自适应 | 待调研 | 高密度时自动降响应 |

---

## 🎯 Phase 5: ORCA/RVO 避障系统

### 5.1 RVO 核心算法
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 实现速度障碍(VO)计算 | P0 | ⬜ | 锥形速度区域 |
| 实现互惠速度障碍(RVO) | P0 | ⬜ | 共享避障责任 |
| 实现 ORCA 线性约束 | P0 | ⬜ | 简化最优速度选择 |
| 实现 HRVO 混合模式 | P2 | ⬜ | 结合 RVO 和 VO 优势 |

### 5.2 RVO 配置参数
| 任务 | 优先级 | 状态 | 默认值 |
|------|--------|------|--------|
| 邻居半径 | P0 | ⬜ | 200-400 |
| 时间窗 | P0 | ⬜ | 0.5-1.0s |
| 最大速度 | P1 | ⬜ | 0=使用当前速度 |
| 最小速度 | P1 | ⬜ | 50-100 |
| 到达半径 | P1 | ⬜ | 150-300 |
| 最大邻居数 | P1 | ⬜ | 12-20 |
| 分帧步长 | P2 | ⬜ | 1-2 |

### 5.3 抗抖动系统
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 密度自适应 | P1 | ⬜ | 高密度时降低响应 |
| 速度平滑 | P1 | ⬜ | 平滑速度变化 |
| 抖动检测 | P2 | ⬜ | 检测方向变化 |
| 抖动抑制 | P2 | ⬜ | 自动抑制抖动 |

### 5.4 技术调研
| 任务 | 状态 | 说明 |
|------|------|------|
| ✅ RVO/ORCA 算法 | 已完成 | 参考 [RVO算法详解](https://blog.csdn.net/weixin_42216813/article/details/147072134) |
| ⬜ RVO2 库集成 | 待调研 | C#/C++ 开源实现 |
| ⬜ Flow Field 结合 | 待调研 | 与流场寻路结合 |

---

## 🎯 Phase 6: LOD 更新频率系统

### 6.1 距离分级更新
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 实现距离计算 | P0 | ⬜ | 实例到相机距离 |
| 实现更新频率分级 | P0 | ⬜ | 近/中/远三级 |
| 实现分帧更新 | P0 | ⬜ | 远距离实例分帧更新 |

### 6.2 LOD 配置 API
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| SetLODUpdateFrequencyEnabled | P0 | ⬜ | 启用/禁用 LOD 更新 |
| IsLODUpdateFrequencyEnabled | P0 | ⬜ | 查询启用状态 |
| SetLODDistances | P0 | ⬜ | 设置距离阈值 |

---

## 🎯 Phase 7: 几何工具库

### 7.1 基础几何生成
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| GetBezierPoint | P1 | ⬜ | 贝塞尔曲线点，支持任意阶控制点 |
| GetPointsByRound | P0 | ⬜ | 圆形区域点阵，支持噪声随机偏移 |
| GetPointsByGrid | P0 | ⬜ | 网格点阵 (1D/2D/3D)，完整参数支持 |

### 7.2 Shape 组件生成
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| GetPointsByShape - 表面 | P1 | ⬜ | 球/盒/胶囊表面点 |
| GetPointsByShape - 填充 | P1 | ⬜ | 球/盒/胶囊内部点 |

### 7.3 Mesh 相关生成
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| GetPointsByMesh - 表面 | P2 | ⬜ | 静态网格表面随机点，支持 LODIndex |
| 三角形面积加权采样 | P2 | ⬜ | 表面均匀分布算法 |
| GetPointsByMeshVoxel - 外壳 | P2 | ⬜ | 体素化外壳 |
| GetPointsByMeshVoxel - 实心 | P2 | ⬜ | 体素化实心 |

### 7.4 Spline 相关生成
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| GetPointsBySpline | P2 | ⬜ | 沿样条曲线生成点带 |

### 7.5 纹理采样
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| GetPixelsByTexture | P2 | ⬜ | 从纹理获取像素数据 |

---

## 🎯 Phase 8: 编辑器扩展

### 8.1 快速资产创建
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 右键菜单扩展 | P0 | ⬜ | 内容浏览器右键菜单 |
| 自动创建 AnimCollection | P0 | ⬜ | 从 SkeletalMesh 创建 |
| 自动创建 RenderParams | P0 | ⬜ | 从 SkeletalMesh 创建 |

### 8.2 调试工具
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 控制台命令 | P1 | ⬜ | Skelot.DrawInstanceBounds 等 |
| 调试绘制 | P1 | ⬜ | 碰撞半径、网格、速度向量 |
| 统计信息 | P2 | ⬜ | 实例数、更新时间等 |

---

## 🎯 Phase 9: 示例和文档

### 9.1 示例关卡
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 基础实例创建示例 | P1 | ⬜ | 创建/销毁/动画 |
| 碰撞避让示例 | P1 | ⬜ | PBD + RVO 演示 |
| 几何工具示例 | P2 | ⬜ | 各种点位生成 |
| 性能测试关卡 | P2 | ⬜ | 4-5w 实例压力测试 |

### 9.2 文档更新
| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| API 文档更新 | P1 | ⬜ | 新增 API 说明 |
| 配置参数文档 | P1 | ⬜ | PBD/RVO 参数说明 |
| 最佳实践指南 | P2 | ⬜ | 性能优化建议 |

---

## 📊 进度统计

| 阶段 | 总任务 | 已完成 | 进度 |
|------|--------|--------|------|
| Phase 1: 基础架构 | 8 | 0 | 0% |
| Phase 1.5: 动画系统 | 5 | 0 | 0% |
| Phase 2: 碰撞通道 | 10 | 0 | 0% |
| Phase 2.5: 层级关系 | 4 | 0 | 0% |
| Phase 3: 空间检测 | 13 | 1 | 8% |
| Phase 4: PBD 系统 | 16 | 1 | 6% |
| Phase 5: RVO 系统 | 18 | 1 | 6% |
| Phase 6: LOD 系统 | 6 | 0 | 0% |
| Phase 7: 几何工具 | 14 | 0 | 0% |
| Phase 8: 编辑器 | 7 | 0 | 0% |
| Phase 9: 示例文档 | 7 | 0 | 0% |
| **总计** | **108** | **3** | **3%** |

---

## 📚 技术参考

### PBD (Position Based Dynamics)
- [Genesis PBD求解器](https://blog.csdn.net/gitblog_00393/article/details/151500546)
- [Position Based Dynamics GitHub](https://github.com/topics/position-based-dynamics)

### RVO/ORCA
- [RVO算法详解](https://blog.csdn.net/weixin_42216813/article/details/147072134)
- [RVO2 C# 实现](https://blog.csdn.net/weixin_50702814/article/details/147606017)
- [Unity RVO 实践](https://m.blog.csdn.net/oWanMeiShiKong/article/details/147013848)

### Spatial Hashing
- [空间哈希实现](https://m.php.cn/faq/1927461.html)
- [Quake III 优化](https://m.blog.csdn.net/gitblog_00944/article/details/154328788)

---

## 🔄 更新日志

| 日期 | 更新内容 |
|------|----------|
| 2026-03-01 | 对比参考文档，补充遗漏任务：动画系统、层级关系、空间检测掩码过滤、几何工具细化 |
| 2026-02-28 | 创建任务列表，完成技术调研 |

---

*最后更新: 2026-03-01*
