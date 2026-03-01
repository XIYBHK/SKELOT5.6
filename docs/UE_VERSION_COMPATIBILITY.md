# UE 版本 API 兼容性问题

> 记录不同 UE 版本之间的 API 变更和兼容性解决方案

## UE 5.6 API 变更

### 1. FToolMenuSection::AddMenuEntry

**变更版本**：UE 5.6

**影响模块**：SkelotEd

**问题描述**：
旧代码使用 `FUIAction` 作为参数，编译通过但链接失败：

```
error LNK2019: 无法解析的外部符号 "FToolMenuSection::AddMenuEntry(...FUIAction...)"
```

**旧 API（UE 5.5 及更早）**：
```cpp
FToolMenuEntry& AddMenuEntry(
    FName InName,
    const FText& InLabel,
    const FText& InToolTip,
    const FSlateIcon& InIcon,
    const FUIAction& InAction  // 旧版本使用 FUIAction
);
```

**新 API（UE 5.6）**：
```cpp
FToolMenuEntry& AddMenuEntry(
    FName InName,
    const TAttribute<FText>& InLabel,      // TAttribute 包装
    const TAttribute<FText>& InToolTip,   // TAttribute 包装
    const TAttribute<FSlateIcon>& InIcon, // TAttribute 包装
    const FToolUIActionChoice& InAction, // 新版本使用 FToolUIActionChoice
    EUserInterfaceActionType UserInterfaceActionType = EUserInterfaceActionType::Button,
    FName InTutorialHighlightName = NAME_None
);
```

**迁移方案**：
```cpp
// === 旧代码 ===
Section.AddMenuEntry(
    "Skelot_CreateAnimCollection",
    LOCTEXT("Label", "Label"),
    LOCTEXT("ToolTip", "ToolTip"),
    FSlateIcon(),
    FUIAction(FExecuteAction::CreateSP(this, &MyClass::ExecuteCommand))
);

// === 新代码 ===
FToolMenuEntry& Entry = Section.AddMenuEntry(
    "Skelot_CreateAnimCollection",
    LOCTEXT("Label", "Label"),
    LOCTEXT("ToolTip", "ToolTip"),
    FSlateIcon(),
    FToolUIActionChoice(FExecuteAction::CreateSP(this, &MyClass::ExecuteCommand))
);
```

**需要添加的模块依赖**：
```cs
// SkelotEd.Build.cs
PublicDependencyModuleNames.AddRange(
    new string[]
    {
        // ... 其他模块 ...
        "ToolMenus"  // 必须添加
    }
);
```

---

### 2. TObjectIterator 在不同构建配置中的可用性

**影响模块**：Skelot (Runtime)

**问题描述**：
`TObjectIterator` 在非 Editor 构建中不可用，导致编译错误：

```
error C7568: 假定的函数模板"TObjectIterator"调用缺少参数列表
```

**原因**：
- `TObjectIterator` 是编辑器专用功能
- 在 Shipping 构建中，`TObjectIterator` 宏未定义

**解决方案**：
```cpp
static void PrintStats()
{
#if WITH_EDITOR
    // Editor 构建中可用
    for (TObjectIterator<ASkelotWorld> It; It; ++It)
    {
        ASkelotWorld* World = *It;
        if (World && World->GetWorld() && World->GetWorld()->IsGameWorld())
        {
            // 处理逻辑
        }
    }
#else
    // 非 Editor 构建中提供替代方案
    UE_LOG(LogTemp, Warning, TEXT("Stats command only available in Editor builds"));
#endif
}
```

**最佳实践**：
- 运行时代码避免使用 `TObjectIterator`
- 使用 `ASkelotWorld::Get()` 的重载版本获取实例
- Editor-only 功能用 `#if WITH_EDITOR` 包裹

---

### 3. ASkelotWorld::Get 重载歧义

**影响模块**：Skelot (Runtime)

**问题描述**：
调用 `ASkelotWorld::Get(nullptr, false)` 时编译器报歧义错误

**原因**：
`ASkelotWorld::Get` 有两个重载版本：
```cpp
static ASkelotWorld* Get(const UObject* Context, bool bCreateIfNotFound = true);
static ASkelotWorld* Get(const UWorld* World, bool bCreateIfNotFound = true);
```

`nullptr` 可以同时匹配 `const UObject*` 和 `const UWorld*`

**解决方案**：
```cpp
// === 方案1：显式类型转换 ===
ASkelotWorld* World = ASkelotWorld::Get((const UObject*)nullptr, false);

// === 方案2：使用有效指针 ===
UWorld* GameWorld = GWorld->GetWorld();
ASkelotWorld* World = ASkelotWorld::Get(GameWorld, false);

// === 方案3：使用 TObjectIterator（仅 Editor） ===
#if WITH_EDITOR
for (TObjectIterator<ASkelotWorld> It; It; ++It)
{
    ASkelotWorld* World = *It;
    // ...
}
#endif
```

---

## 控制台命令最佳实践

### 自动注册 vs 手动注册

| 方式 | 优点 | 缺点 |
|------|------|------|
| `FAutoConsoleCommand` | 自动注册/注销，代码简洁 | 无法动态控制 |
| `IConsoleManager::RegisterConsoleCommand` | 可以动态注册/注销 | 需要手动管理生命周期 |
| `FAutoConsoleVariableRef` | 自动同步 CVar 和变量 | 仅适用于简单类型 |

### 推荐模式

```cpp
// 1. 声明全局变量（用于跨函数共享状态）
static int32 GSkelot_DebugMode = 0;
static float GSkelot_DebugDrawDistance = 5000.0f;

// 2. 自动注册控制台变量
static FAutoConsoleVariableRef CVarDebugMode(
    TEXT("Skelot.DebugMode"),
    GSkelot_DebugMode,
    TEXT("Debug draw mode bitmask (0=bounds, 2=grid, 4=collision, 8=velocity)")
);

static FAutoConsoleVariableRef CVarDebugDrawDistance(
    TEXT("Skelot.DebugDrawDistance"),
    GSkelot_DebugDrawDistance,
    TEXT("Maximum distance for debug drawing")
);

// 3. 自动注册控制台命令
static FAutoConsoleCommand CmdDrawAllBounds(
    TEXT("Skelot.DrawAllBounds"),
    TEXT("Toggle drawing of all instance bounding boxes"),
    FConsoleCommandDelegate::CreateStatic(&ToggleDrawAllBounds)
);

// 4. 命令实现函数
static void ToggleDrawAllBounds()
{
    FSkelotDebugTools& Tools = FSkelotDebugTools::Get();
    if (Tools.IsDebugDrawModeEnabled(ESkelotDebugDrawMode::InstanceBounds))
    {
        Tools.RemoveDebugDrawMode(ESkelotDebugDrawMode::InstanceBounds);
        GSkelot_DebugMode &= ~static_cast<int32>(ESkelotDebugDrawMode::InstanceBounds);
        UE_LOG(LogTemp, Log, TEXT("Skelot: DrawAllBounds disabled"));
    }
    else
    {
        Tools.AddDebugDrawMode(ESkelotDebugDrawMode::InstanceBounds);
        GSkelot_DebugMode |= static_cast<int32>(ESkelotDebugDrawMode::InstanceBounds);
        UE_LOG(LogTemp, Log, TEXT("Skelot: DrawAllBounds enabled"));
    }
}
```

---

*最后更新: 2026-03-02*
