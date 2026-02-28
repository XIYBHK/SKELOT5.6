# 参考文档对比分析报告

> 对比 SkelotRbn 参考文档与现有任务列表/技术预演文档
> 分析日期: 2026-03-01

---

## 📊 覆盖度总览

| 模块 | 覆盖状态 | 完整度 |
|------|----------|--------|
| 实例管理 | ✅ 完整覆盖 | 100% |
| 变换操作 | ✅ 完整覆盖 | 100% |
| 动画系统 | ❌ **完全遗漏** | 0% |
| LOD 系统 | ✅ 完整覆盖 | 100% |
| 碰撞通道 | ✅ 完整覆盖 | 100% |
| 移动系统 | ✅ 完整覆盖 | 100% |
| 空间检测 | ✅ 完整覆盖 | 100% |
| 层级关系 | ❌ **遗漏** | 10% |
| 几何工具 | ⚠️ 部分覆盖 | 70% |
| PBD/RVO 配置 | ✅ 完整覆盖 | 100% |
| 编辑器扩展 | ✅ 覆盖 | 80% |

**总体覆盖率: ~75%**

---

## 🔴 严重遗漏 (需立即补充)

### 1. 动画系统 API

**参考文档定义:**

```
#### Skelot Play Animation
播放动画。

参数:
- Handle: 实例句柄
- Params: 动画播放参数
  - Animation: 动画序列
  - bLoop: 是否循环
  - PlayRate: 播放速率 (1.0为正常速度)
  - StartPosition: 起始位置 (秒)
  - BlendInTime: 混合时间 (秒)

返回值:
- 动画长度 (秒)，失败返回-1

#### Skelot Get Anim Collection
获取实例关联的动画集资产。

参数:
- Handle: 实例句柄

返回值:
- 动画集资产
```

**现状:** 任务列表完全遗漏动画系统扩展

**影响:** 原版 Skelot 有 `InstancePlayAnimation`，但缺少:
- 结构化的播放参数 `FSkelotAnimPlayParamsEx`
- 混合时间 (BlendInTime) 支持
- 蓝图友好的封装 API

**需添加任务:**

| 任务 | 优先级 | 说明 |
|------|--------|------|
| 定义 FSkelotAnimPlayParamsEx 结构体 | P0 | 包含 Animation, bLoop, PlayRate, StartPosition, BlendInTime |
| 扩展 InstancePlayAnimation | P0 | 支持新的参数结构体 |
| 实现 BlendInTime 混合 | P1 | 平滑过渡到新动画 |
| Skelot_PlayAnimation 蓝图API | P0 | USkelotWorldSubsystem 封装 |
| Skelot_GetAnimCollection 蓝图API | P1 | 获取实例的动画集 |

---

### 2. 层级关系 API

**参考文档定义:**

```
#### Skelot Attach Child
将实例附加到另一个实例，形成父子关系。

参数:
- Parent: 父实例句柄
- Child: 子实例句柄
- SocketOrBoneName: 附加到的骨骼/插槽名
- Transform: 相对变换
- bKeepWorldTransform: true表示保持世界位置

#### Skelot Detach From Parent
将实例从父级分离。

#### Skelot Get Children
获取所有子实例。

返回值: Children - 子实例句柄数组

#### Skelot Get Parent
获取父实例。

返回值: 父实例句柄，无父级返回无效句柄
```

**现状:**
- 原版 Skelot 有 `InstanceAttachChild` 和 `DetachInstanceFromParent`
- 但任务列表没有明确列出层级关系 API 的封装
- 缺少 `GetChildren` 和 `GetParent` 的蓝图 API

**需添加任务:**

| 任务 | 优先级 | 说明 |
|------|--------|------|
| Skelot_AttachChild 蓝图API | P0 | 封装 InstanceAttachChild |
| Skelot_DetachFromParent 蓝图API | P0 | 封装 DetachInstanceFromParent |
| Skelot_GetChildren 蓝图API | P1 | 新增：获取子实例数组 |
| Skelot_GetParent 蓝图API | P1 | 新增：获取父实例 |
| bKeepWorldTransform 参数 | P1 | 附加时保持世界位置选项 |

---

## 🟡 部分遗漏 (需补充)

### 3. 几何工具 - GetPointsByMesh

**参考文档定义:**

```
#### Get Points By Mesh
在静态网格表面生成随机点。

参数:
- Mesh: 静态网格资产
- Transform: 网格变换
- Distance: 目标点间距
- Noise: 随机偏移量
- LODIndex: LOD级别 (0为最高)

返回值: 点坐标数组
```

**现状:** 任务列表有 `GetPointsByMesh - 表面` 但缺少详细参数说明

**需补充:**
- LODIndex 参数支持
- 基于三角形面积加权的随机采样算法

---

### 4. 空间检测 - 掩码过滤

**参考文档定义:**

```
#### Skelot Query Location Overlapping Sphere
参数:
- Center: 球心位置
- Radius: 球体半径
- Instances: 输出，查询到的句柄
- CollisionMask: 掩码过滤，默认全部通道
```

**现状:** 任务列表有此功能，但未明确说明 `CollisionMask` 过滤参数

**需补充:**
- 查询 API 支持按碰撞掩码过滤
- 只返回与掩码匹配的实例

---

## 🟢 已覆盖但需细化

### 5. 批量操作 API

**参考文档定义了更多批量版本:**

| API | 任务列表状态 |
|-----|-------------|
| SetInstanceVelocities (Handle版) | ✅ 有 |
| SetInstanceVelocities ByIndex | ✅ 有 |
| SetInstanceVelocities ByIndex | ⚠️ 需明确区分 |

### 6. 蓝图 API 命名规范

参考文档使用以下命名规范:
- `Skelot_CreateInstance` (下划线分隔)
- `Skelot Set Instance Velocity` (空格分隔 - 蓝图搜索友好)

**建议:** 统一使用 `Skelot_` 前缀 + 空格分隔，便于蓝图搜索

---

## 📋 遗漏任务清单

### Phase 1.5: 动画系统扩展 (新增)

| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| 定义 FSkelotAnimPlayParamsEx | P0 | ⬜ | 扩展动画播放参数 |
| 实现 BlendInTime 混合 | P1 | ⬜ | 动画过渡混合 |
| Skelot_PlayAnimation API | P0 | ⬜ | 蓝图封装 |
| Skelot_GetAnimCollection API | P1 | ⬜ | 蓝图封装 |

### Phase 2.5: 层级关系 API (新增)

| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| Skelot_AttachChild | P0 | ⬜ | 蓝图封装 + bKeepWorldTransform |
| Skelot_DetachFromParent | P0 | ⬜ | 蓝图封装 |
| Skelot_GetChildren | P1 | ⬜ | 新增功能 |
| Skelot_GetParent | P1 | ⬜ | 新增功能 |

### Phase 7: 几何工具补充

| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| GetPointsByMesh 完善 | P1 | ⬜ | 添加 LODIndex 参数 |
| 三角形面积加权采样 | P2 | ⬜ | 表面均匀分布 |

### Phase 3: 空间检测补充

| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| QuerySphere 掩码过滤 | P0 | ⬜ | CollisionMask 参数 |
| QueryBox 掩码过滤 | P1 | ⬜ | CollisionMask 参数 |

---

## 🔧 技术预演文档补充需求

### 需要添加的技术内容

| 内容 | 状态 | 说明 |
|------|------|------|
| 动画混合算法 | ❌ 缺失 | BlendInTime 实现方案 |
| 层级变换传播 | ❌ 缺失 | 父子关系变换更新 |
| Mesh 表面采样 | ⚠️ 简略 | 需详细算法说明 |
| 掩码过滤优化 | ❌ 缺失 | 空间查询中的掩码处理 |

---

## 📈 更新后任务统计

| 类别 | 原任务数 | 新增任务 | 总计 |
|------|----------|----------|------|
| 动画系统 | 0 | 4 | 4 |
| 层级关系 | 0 | 4 | 4 |
| 几何工具 | 12 | 2 | 14 |
| 空间检测 | 11 | 2 | 13 |
| **总计** | **95** | **12** | **107** |

---

## ✅ 行动项

1. **立即更新 TASK_LIST.md** - 添加遗漏的任务
2. **更新 TECHNICAL_RESEARCH.md** - 补充动画混合和层级变换算法
3. **检查原版 Skelot API** - 确认哪些功能已存在，只需封装

---

*分析完成 - 发现 12 项遗漏任务*
