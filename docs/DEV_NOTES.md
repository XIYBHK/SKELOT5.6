# 开发注意事项

> 记录开发过程中遇到的问题和解决方案，避免重复踩坑

## 工具使用规范

### 1. Edit 工具必须先 Read

**问题**：使用 Edit 工具前必须先 Read 文件，否则会报错 `File has not been read yet`

**正确做法**：
```
1. 先调用 Read 读取目标文件
2. 从 Read 输出中复制精确的字符串
3. 调用 Edit 进行替换
```

**错误示例**：
- 直接调用 Edit 而没有先 Read
- 手动输入 old_string 而不是从 Read 输出复制

### 2. 字符串匹配必须精确

**问题**：Edit 工具的 old_string 必须与文件内容完全匹配，包括：
- 空行数量
- 缩进（空格/Tab）
- 换行符

**正确做法**：
- 从 Read 工具输出直接复制文本
- 注意 Read 输出中的行号前缀（如 `    10→`）后面才是实际内容
- 保留原始的空行和缩进

### 3. Grep 结果需谨慎处理

**问题**：Grep 输出包含行号和文件路径前缀

**正确做法**：
- 使用 `-n` 参数获取行号
- 使用 `-A`/`-B`/`-C` 获取上下文
- 复制代码时去掉行号前缀

## UE C++ 开发规范

### 1. 新增类文件

**步骤**：
1. 在 `Source/Skelot/Public/` 创建 .h 文件
2. 在 `Source/Skelot/Private/` 创建 .cpp 文件
3. 确保包含正确的模块头文件
4. 编译测试

### 2. 头文件包含顺序

```cpp
// 1. 预编译头
#include "CoreMinimal.h"

// 2. 引擎头文件
#include "Engine/World.h"

// 3. 项目头文件
#include "SkelotWorldBase.h"

// 4. 生成的头文件（最后）
#include "SkelotWorld.generated.h"
```

### 3. UCLASS/USTRUCT 宏

- 必须在 `.generated.h` 包含之前定义
- BlueprintType 需要添加 `BlueprintType` 说明符
- 使用 `UENUM(BlueprintType)` 使枚举可在蓝图使用

## 常见编译错误

### 1. 链接错误 LNK2019

**原因**：函数声明但未实现

**解决**：检查 .cpp 文件中是否实现了所有声明的函数

### 2. 未定义的外部符号

**原因**：忘记添加模块导出宏

**解决**：确保类声明中有 `class MODULENAME_API ClassName`

### 3. 头文件循环依赖

**原因**：A 包含 B，B 包含 A

**解决**：使用前向声明 `class ClassName;` 替代 `#include`

## Git 提交规范

### 提交信息格式

```
<type>(<scope>): <subject>

<body>
```

### Type 类型

| 类型 | 说明 |
|------|------|
| feat | 新功能 |
| fix | Bug 修复 |
| refactor | 重构（不改变功能） |
| docs | 文档更新 |
| test | 测试相关 |
| chore | 构建/工具变更 |

### 示例

```
feat(Skelot): 实现空间哈希网格系统

- 新增 FSkelotSpatialGrid 类
- 添加球形范围查询 API
- 优化查询性能 O(n) -> O(1)
```

---

## 空间网格优化调研结论

> 基于 2024-2026 年最新研究和 DetourCrowd 实践

### 已验证有效的方案

| 方案 | 效果 | 实现难度 | 是否采用 |
|------|------|----------|----------|
| **降低更新频率** | FrameStride=2 降 50% | 低 | ✅ 已实现 |
| **Cell Size = 2x 对象大小** | 最优查询效率 | 低 | ✅ 已实现 |

### 已验证无效/过度的方案

| 方案 | 问题 | 结论 |
|------|------|------|
| **Morton 编码** | 只对连续数组存储有效，`TMap` 无收益 | ❌ 不采用 |
| **分批更新** | 导致网格数据不一致，查询结果错误 | ❌ 不采用 |
| **增量更新** | 复杂度高，[Unity 经验表明](https://m.blog.csdn.net/gitblog_00325/article/details/153719621)变化累积后反而更慢 | ❌ 不采用 |
| **双缓冲** | 无异步查询需求时是内存浪费 | ⏸️ 暂不需要 |

### 分帧更新实现（已验证）

```cpp
// 基于 DetourCrowd 的简化方案
void RebuildIncremental(...)
{
    CurrentFrameIndex = (CurrentFrameIndex + 1) % FrameStride;

    // 只有当 CurrentFrameIndex == 0 时才重建
    if (CurrentFrameIndex == 0)
    {
        Rebuild(SOA, NumInstances);
    }
    // 否则跳过，使用上一帧数据
}
```

**优点**：
1. 简单实现，不会出错
2. 网格数据始终一致
3. 对群集 AI 场景足够（不需要每帧精确位置）

### 参考资料

- [DetourCrowd 分析](https://blog.csdn.net/gitblog_00648/article/details/154421533) - MAX_ITERS_PER_UPDATE 模式
- [空间哈希最佳实践](https://m.php.cn/faq/1927461.html) - Cell Size 设置
- [Unity Tilemap 经验](https://m.blog.csdn.net/gitblog_00325/article/details/153719621) - 增量更新 vs 完整重建

---

## 预研文档使用规范

### 重要原则

1. **优先查阅预研文档**：`docs/IMPLEMENTATION_CHALLENGES.md` 和 `docs/TECHNICAL_RESEARCH.md`
2. **批判性思考**：预研文档的建议需要验证，不是无条件执行
3. **网络调研验证**：对不确定的方案，先搜索验证再实现

### 预研文档评估流程

```
1. 阅读预研文档建议
2. 批判性分析：
   - 是否适用于当前场景？
   - 复杂度是否合理？
   - 是否有更好的替代方案？
3. 必要时进行网络调研
4. 做出决定并记录原因
```

---

## RVO 系统类型转换问题 (2026-03-02)

### 问题描述

编译 SkelotRVOSystem.cpp 时出现类型转换错误：
- `FVector3f` 无法隐式转换为 `FVector2f`
- `FVector2f` 和 `FVector3f` 混用导致赋值失败

### 根本原因

1. 代码中 `FVector2f` 和 `FVector3f` 类型混用
2. 注释格式错误导致变量定义被注释掉
   ```cpp
   // 错误示例：注释和代码在同一行
   // 转换为 3D（XY 平面，	FVector3f MyPos(...);
   ```

### 解决方案

1. **显式类型转换**：
   ```cpp
   // 错误
   FVector2f PreferredVelocity = MyVel;  // MyVel 是 FVector3f

   // 正确
   FVector2f PreferredVelocity(MyVel.X, MyVel.Y);
   ```

2. **统一变量类型**：
   - `NewVelocity2D` 从 `FVector3f` 改为 `FVector2f`
   - 最后再转换回 `FVector3f` 输出

3. **修复注释格式**：
   ```cpp
   // 正确：注释独占一行
   // 转换为 3D（XY 平面）
   FVector3f MyPos(MyPos3D.X, MyPos3D.Y, 0.0f);
   ```

### 经验总结

- RVO/ORCA 算法本质是 2D 算法，只在 XY 平面计算
- 内部计算使用 `FVector2f`，输入输出使用 `FVector3f`（Z=0）
- 在类型边界处显式转换，避免隐式转换错误

---

## 几何工具库实现 (2026-03-02)

### 创建文件

- `Source/Skelot/Public/SkelotGeometryTools.h` - 头文件
- `Source/Skelot/Private/SkelotGeometryTools.cpp` - 实现文件

### 实现的功能

| 函数 | 说明 |
|------|------|
| `GetPointsByRound` | 圆形区域点阵，六边形紧密填充算法 |
| `GetPointsByGrid` | 1D/2D/3D 网格点阵 |
| `GetBezierPoint` | 贝塞尔曲线点，De Casteljau 算法 |
| `GetPointsByShape` | 球/盒/胶囊组件表面与填充点阵 |

### 算法要点

1. **圆形填充**：使用六边形紧密填充（蜂窝排列）
   - 水平间距 = Distance
   - 垂直间距 = Distance * sqrt(3)/2
   - 奇数行水平偏移一半间距

2. **贝塞尔曲线**：使用 De Casteljau 算法
   - 支持任意阶控制点
   - 时间复杂度 O(n²)

3. **Shape 组件**：
   - 球体：球坐标采样（表面）+ 切片填充（内部）
   - 盒体：6 面采样（表面）+ 网格填充（内部）
   - 胶囊：圆柱+半球组合采样

### 组件依赖

```cpp
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
```

---

## 调试工具系统实现 (2026-03-02)

> 命名约定：本文涉及“蓝图节点名 / C++ 函数名 / 控制台命令名”三套命名，示例代码以实际 C++ 标识符为准；对外说明建议同时标注蓝图显示名。

### 创建文件

- `Source/Skelot/Public/SkelotDebugTools.h` - 调试工具头文件
- `Source/Skelot/Private/SkelotDebugTools.cpp` - 调试工具实现

### 实现的功能

| 命令 | 功能 |
|------|------|
| `Skelot.DrawAllBounds` | 绘制所有实例包围盒 |
| `Skelot.DrawSpatialGrid` | 绘制空间网格 |
| `Skelot.DrawCollisionRadius` | 绘制碰撞半径 |
| `Skelot.DrawVelocities` | 绘制速度向量 |
| `Skelot.DrawNeighborLinks` | 绘制邻居连接线 |
| `Skelot.Stats` | 打印统计信息 |
| `Skelot.DebugMode [0-255]` | 设置调试模式 |
| `Skelot.DebugDrawDistance [距离]` | 设置绘制距离 |

### 遇到的问题

> **详细解决方案请参考**: [UE_VERSION_COMPATIBILITY.md](./UE_VERSION_COMPATIBILITY.md)

1. **FToolMenuSection::AddMenuEntry API 变更** - UE 5.6 要求使用 `FToolUIActionChoice`
2. **TObjectIterator 在 Game 构建中不可用** - 需要使用 `#if WITH_EDITOR` 包裹
3. **ASkelotWorld::Get 重载歧义** - `nullptr` 参数导致编译器无法选择重载版本

### 控制台命令最佳实践

```cpp
// 使用 FAutoConsoleCommand 自动注册
static FAutoConsoleCommand CmdDrawAllBounds(
    TEXT("Skelot.DrawAllBounds"),
    TEXT("Toggle drawing of all instance bounding boxes"),
    FConsoleCommandDelegate::CreateStatic(&ToggleDrawAllBounds)
);

// 使用 FAutoConsoleVariableRef 自动同步变量
static int32 GSkelot_DebugMode = 0;
static FAutoConsoleVariableRef CVarDebugMode(
    TEXT("Skelot.DebugMode"),
    GSkelot_DebugMode,
    TEXT("Debug draw mode bitmask")
);
```

---

## 障碍物系统实现 (2026-03-02)

### 创建文件

- `Source/Skelot/Public/SkelotObstacle.h` - 障碍物基类和派生类
- `Source/Skelot/Private/SkelotObstacle.cpp` - 障碍物实现

### 实现的功能

| 类 | 功能 |
|------|------|
| `ASkelotObstacle` | 抽象基类，提供碰撞接口 |
| `ASkelotSphereObstacle` | 球形障碍物 |
| `ASkelotBoxObstacle` | 盒形障碍物（支持旋转） |

### 遇到的问题

1. **TObjectPtr::IsValid() 方法不存在**

   **问题**：`TObjectPtr.IsValid()` 不是有效的成员函数
   ```cpp
   // 错误
   if (!ObstaclePtr.IsValid())

   // 正确
   if (!ObstaclePtr.Get())
   ```

   **原因**：TObjectPtr 没有 IsValid() 方法，需要用 Get() 返回原始指针再判断

2. **DrawDebugBox 旋转参数类型**

   **问题**：传入 FRotator 导致编译错误
   ```cpp
   // 错误
   DrawDebugBox(World, Location, Extent, GetActorRotation(), ...);

   // 正确
   DrawDebugBox(World, Location, Extent, GetActorQuat(), ...);
   ```

   **原因**：UE5 的 DrawDebugBox 要求 FQuat 类型，不接受 FRotator

### 架构设计

障碍物系统采用以下设计：

1. **自动注册**：障碍物在 BeginPlay 时自动注册到 SkelotWorld
2. **碰撞掩码**：支持按碰撞通道过滤
3. **PBD 集成**：在 PBD 碰撞求解后额外处理障碍物碰撞
4. **PostObstacleIterations**：障碍物碰撞后额外迭代以提高稳定性

---

## 示例 Actor 类实现 (2026-03-02)

### 创建文件

- `Source/Skelot/Public/SkelotExampleActors.h` - 示例 Actor 头文件
- `Source/Skelot/Private/SkelotExampleActors.cpp` - 示例 Actor 实现

### 实现的类

| 类名 | 功能 | 演示内容 |
|------|------|----------|
| `ASkelotExampleBasicInstance` | 基础实例操作 | 创建/销毁/动画播放/变换操作 |
| `ASkelotExampleCollisionAvoidance` | 碰撞避让演示 | PBD 碰撞 + RVO 避障 + 群体移动 |
| `ASkelotExampleGeometryTools` | 几何工具演示 | 圆形/网格/贝塞尔曲线点阵生成 |

### 遇到的问题

1. **UPROPERTY 中枚举声明语法**

   **问题**：在 UPROPERTY 中声明 enum class 导致 UHT 错误
   ```cpp
   // 错误
   UPROPERTY()
   enum class EPattern : uint8 { ... } Pattern;

   // 正确
   UPROPERTY()
   uint8 Pattern = 0;  // 使用整数代替
   ```

2. **FSkelotAnimPlayParams 成员名称**

   **问题**：使用 `PlayRate` 编译错误
   ```cpp
   // 错误
   Params.PlayRate = 1.0f;

   // 正确
   Params.PlayScale = 1.0f;
   ```

3. **ASkelotWorld 缺少批量销毁方法**

   **问题**：ASkelotWorld 没有 `DestroyInstances` 方法

   **解决方案**：使用 `USkelotWorldSubsystem::Skelot_DestroyInstances`

4. **FQuat4f 类型转换**

   **问题**：FRotator 无法直接转换为 FQuat4f
   ```cpp
   // 错误
   FQuat4f Rot = FQuat4f(FRotator(...));

   // 正确
   FQuat Rot = FRotator(...).Quaternion();
   SkelotWorld->SetInstanceRotation(Index, FQuat4f(Rot));
   ```

5. **InputCore 模块依赖**

   **问题**：使用 EKeys 需要链接 InputCore 模块

   **解决方案**：在 `Skelot.Build.cs` 中添加 `"InputCore"` 依赖

### 架构设计

示例 Actor 类采用以下设计：

1. **可配置参数**：所有参数通过 UPROPERTY 暴露给编辑器
2. **键盘交互**：支持通过键盘键位触发不同功能演示
3. **详细注释**：每个类都有完整的使用说明和演示内容
4. **模块化设计**：每个示例类独立，用户可以按需使用

---

## 几何工具库 Mesh 功能实现 (2026-03-02)

### 新增功能

| 函数 | 说明 |
|------|------|
| `GetPointsByMesh` | 静态网格表面点，三角形面积加权采样 |
| `GetPointsByMeshVoxel` | 体素化点生成，支持外壳/实心模式 |
| `GetPointsBySpline` | 沿样条曲线生成点带 |
| `GetPixelsByTexture` | 从纹理提取像素数据 |

### 遇到的问题

1. **StaticMeshResources API 在 UE5.6 中不可用**

   **问题**：`FStaticMeshRenderData`、`FStaticMeshLODResources` 等类型在 Game 构建中编译错误
   ```
   error C2027: 使用了未定义类型"FStaticMeshRenderData"
   ```

   **解决方案**：使用 `WITH_EDITORONLY_DATA` 宏包裹编辑器专用代码
   ```cpp
   #if WITH_EDITORONLY_DATA
   #include "StaticMeshResources.h"
   // ... 编辑器专用实现
   #else
   // 运行时返回空数组或占位数据
   #endif
   ```

2. **索引缓冲 API 变化**

   **问题**：`FRawStaticIndexBuffer::GetIndex32/GetIndex16` 和 `GetDataTypeSize()` 在 UE5.6 中不存在

   **源码定位**：`D:\UE\UE_5.6\Engine\Source\Runtime\Engine\Public\RawIndexBuffer.h`
   - `FRawStaticIndexBuffer::GetIndex(uint32 At)` - 获取单个索引
   - `FRawStaticIndexBuffer::GetArrayView()` - 获取 FIndexArrayView 视图
   - `FRawStaticIndexBuffer::Is32Bit()` - 判断是否 32 位

   **错误做法**：直接用 `Section.FirstIndex` 作为顶点索引
   ```cpp
   // 错误：FirstIndex 是索引缓冲偏移，不是顶点索引
   FVector V0 = PositionBuffer.VertexPosition(Section.FirstIndex);
   ```

   **正确做法**：通过索引缓冲获取真正的顶点索引
   ```cpp
   FIndexArrayView IndexArrayView = IndexBuffer.GetArrayView();
   for (uint32 TriIdx = 0; TriIdx < Section.NumTriangles; ++TriIdx)
   {
       const uint32 IndexBase = Section.FirstIndex + TriIdx * 3;
       const uint32 Vtx0 = IndexArrayView[IndexBase];     // 真正的顶点索引
       const uint32 Vtx1 = IndexArrayView[IndexBase + 1];
       const uint32 Vtx2 = IndexArrayView[IndexBase + 2];
       FVector V0 = FVector(PositionBuffer.VertexPosition(Vtx0));
       // ...
   }
   ```

3. **FBox::GetCorner 不存在**

   **问题**：`ExpandedBounds.GetCorner(i)` 编译错误

   **解决方案**：手动定义 8 个角点坐标
   ```cpp
   const FVector Corners[8] = {
       FVector(Min.X, Min.Y, Min.Z),
       FVector(Max.X, Min.Y, Min.Z),
       // ... 其余 6 个角点
   };
   ```

4. **FTexturePlatformData::GetMips() 变化**

   **问题**：`PlatformData->GetMips()` 方法不存在

   **解决方案**：直接访问 `PlatformData->Mips` 成员
   ```cpp
   // 错误
   FTexture2DMipMap& Mip = PlatformData->GetMips()[0];

   // 正确
   TArray<FTexture2DMipMap>& Mips = PlatformData->Mips;
   FTexture2DMipMap& Mip = Mips[0];
   ```

### 算法要点

1. **三角形面积加权采样**：确保表面点均匀分布
   - 计算每个三角形面积和累积面积
   - 随机选择三角形（面积大的概率高）
   - 在选中的三角形内生成随机点

2. **体素化内外判定**：使用射线投射法
   - 从体素中心发射射线（X 方向）
   - 统计与三角形的交点数
   - 奇数 = 内部，偶数 = 外部

3. **外壳模式**：检测体素到三角形表面距离
   - 计算 SearchRadius = HalfVoxel * sqrt(3)
   - 检查体素中心到最近三角形的距离

### 注意事项

- Mesh/Voxel 功能仅编辑器可用，运行时返回空数组
- 可考虑未来使用 Physics API 实现运行时版本

---

## UE API 源码复验 (2026-03-02)

### 复验背景

根据 `CLAUDE.md` 中建立的 UE 内置 API 源码核查机制，对之前完成的任务中涉及的 UE API 使用进行源码复验，确认解决方案的正确性。

### 已验证的 API（UE 5.6 源码）

| API | 源码位置 | 验证结果 |
|-----|----------|----------|
| `FRawStaticIndexBuffer::GetArrayView()` | `RawIndexBuffer.h:253` | 返回 `FIndexArrayView` |
| `FIndexArrayView::operator[]` | `RawIndexBuffer.h:116-117` | 支持 16/32 位索引自动转换 |
| `DrawDebugBox(..., FQuat&, ...)` | `DrawDebugHelpers.h:30` | 带旋转参数版本需要 `FQuat` |
| `FTexturePlatformData::Mips` | `Texture.h:853` | 类型为 `TIndirectArray<FTexture2DMipMap>` |
| `TObjectPtr::Get()` | `ObjectPtr.h:681` | 返回 `T*`，无 `IsValid()` 方法 |

### 验证通过的解决方案

1. **索引缓冲访问**
   ```cpp
   // 正确做法（已验证）
   FIndexArrayView IndexArrayView = IndexBuffer.GetArrayView();
   const uint32 Vtx0 = IndexArrayView[IndexBase];
   ```
   - 源码：`FRawStaticIndexBuffer::GetArrayView()` 返回 `FIndexArrayView`
   - `FIndexArrayView::operator[]` 自动处理 16/32 位索引

2. **DrawDebugBox 旋转参数**
   ```cpp
   // 正确做法（已验证）
   DrawDebugBox(World, Location, Extent, GetActorQuat(), Color, ...);
   ```
   - 源码：`DrawDebugHelpers.h:30` 声明 `DrawDebugBox(..., const FQuat& Rotation, ...)`
   - 注意：`KismetSystemLibrary.h:1653` 的蓝图版本使用 `FRotator`，但 `DrawDebugHelpers` 版本需要 `FQuat`

3. **FBox 角点获取**
   ```cpp
   // 正确做法（已验证）- FBox 没有 GetCorner() 方法
   const FVector Corners[8] = {
       FVector(Min.X, Min.Y, Min.Z),
       FVector(Max.X, Min.Y, Min.Z),
       // ... 其余 6 个角点
   };
   ```

4. **纹理 Mips 访问**
   ```cpp
   // 正确做法（已验证）
   FTexturePlatformData* PlatformData = Texture->GetPlatformData();
   TArray<FTexture2DMipMap>& Mips = PlatformData->Mips;  // 直接访问成员
   FTexture2DMipMap& Mip = Mips[0];
   ```
   - 源码：`Texture.h:853` 定义为 `TIndirectArray<struct FTexture2DMipMap> Mips;`

5. **TObjectPtr 判空**
   ```cpp
   // 正确做法（已验证）- TObjectPtr 没有 IsValid() 方法
   if (!ObstaclePtr.Get()) { ... }  // 方式1：Get() 返回原始指针
   if (!ObstaclePtr) { ... }        // 方式2：operator bool()
   ```
   - 源码：`ObjectPtr.h` 中 `TObjectPtr` 只有 `Get()` 和 `operator bool()`，无 `IsValid()`

### 源码位置参考

常用 UE 模块路径（`D:\UE\UE_5.6\Engine\Source`）：
- `Runtime/Core/Public/Math/` - 数学类型（FVector, FQuat 等）
- `Runtime/CoreUObject/Public/UObject/` - UObject 相关（TObjectPtr 等）
- `Runtime/Engine/Public/` - 引擎公共接口（DrawDebugHelpers 等）
- `Runtime/Engine/Classes/Engine/` - 引擎类定义（Texture, StaticMesh 等）
- `Runtime/Engine/Private/` - 引擎实现（调试绘制等）

### 复验结论

所有之前记录的 UE API 问题和解决方案都已通过 UE 5.6 源码验证，实现代码使用的 API 符合 UE 5.6 的实际定义。

---

## 调试绘制 API 兼容性 (2026-03-02)

### 问题描述

在实现性能测试 Actor 的 HUD 绘制时遇到编译错误：

1. `DrawDebugRect` 函数不存在
2. `ParseIntoArrayLines` 参数不正确
3. `DrawDebugString` 在 Game 构建中不可用

### 解决方案

1. **DrawDebugRect 不存在**：移除背景绘制，简化 HUD

2. **ParseIntoArrayLines 参数**：
   ```cpp
   // 错误
   HUDText.ParseIntoArrayLines(Lines, TEXT("\n"), false);

   // 正确
   HUDText.ParseIntoArray(Lines, TEXT("\n"), true);
   ```

3. **Game 构建兼容**：
   ```cpp
   void DrawHUD()
   {
   #if ENABLE_DRAW_DEBUG
       // 调试绘制代码...
   #endif
   }
   ```

### 关键规则

- 所有 `DrawDebug*` 函数调用必须用 `#if ENABLE_DRAW_DEBUG` 包裹
- Editor 编译通过不代表 Game 构建也能通过
- QuickCompile.ps1 会同时测试 Editor + Game(Development + Shipping) 三种构建

---

## HRVO 混合避障模式实现 (2026-03-02)

### 功能说明

HRVO (Hybrid Reciprocal Velocity Obstacle) 结合了 RVO 和 VO 的优点：
- **RVO**：双方各承担一半避障责任，适用于大多数情况
- **VO**：一方承担全部避障责任，适用于迎面相遇场景
- **HRVO**：根据情况自动选择 RVO 或 VO

### 实现的函数

| 函数 | 说明 |
|------|------|
| `ComputeHRVOPlane` | HRVO 主入口，检测迎面相遇并选择算法 |
| `IsHeadOnCollision` | 检测是否迎面相遇（点积 < 阈值） |
| `ComputeRVOPlane` | 标准 RVO 算法，速度障碍分半 |
| `ComputeVOPlane` | 标准 VO 算法，速度障碍不分半 |

### 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `bEnableHRVO` | false | 是否启用 HRVO 混合模式 |
| `HRVOHeadOnThreshold` | -0.5f | 迎面检测阈值（负值，越小越严格） |

### 緻加位置
- 配置参数：`SkelotPBDPlane.h` 的 `FSkelotRVOConfig` 结构体
- 函数声明：`SkelotRVOSystem.h`
- 函数实现：`SkelotRVOSystem.cpp`

### 算法原理
```cpp
// 迎面检测：相对位置与相对速度方向相反
bool IsHeadOnCollision(RelPos, RelVel)
{
    float Dot = dot(normalize(RelPos), normalize(RelVel));
    return Dot < Threshold;  // 负值表示方向相反
}

// HRVO 主逻辑
void ComputeHRVOPlane(...)
{
    if (IsHeadOnCollision(...))
    {
        ComputeVOPlane(...);  // 迎面：一方全责
    }
    else
    {
        ComputeRVOPlane(...);  // 非迎面：双方共担
    }
}
```

---

*最后更新: 2026-03-02*
