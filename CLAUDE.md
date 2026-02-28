# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Context

本项目是对 **Skelot** 插件的二次开发，目标是 1:1 复刻参考插件 (SkelotRbn) 的功能。参考文档位于 `docs/参考插件_说明文档.md`。

核心扩展功能：
- **PBD 碰撞系统** - 基于位置的动态碰撞
- **ORCA/RVO 避障** - 人群避让算法
- **空间检测系统** - 空间哈希网格查询
- **碰撞通道系统** - 8通道掩码过滤
- **几何工具库** - 点位生成工具

详细任务列表见 `docs/TASK_LIST.md`，API 清单见 `docs/API_CHECKLIST.md`。

## Workflow

每个任务完成后必须遵循以下流程：

1. **编译测试** - 运行编译脚本确保无编译错误
   ```powershell
   pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1
   ```

2. **提交 Git** - 使用规范的提交信息
   ```bash
   git add -A
   git commit -m "feat(module): 简短描述"
   git push
   ```

提交规范：
- `feat`: 新功能
- `fix`: 修复 bug
- `refactor`: 重构
- `docs`: 文档更新
- `test`: 测试相关

## Project Overview

Skelot is an Unreal Engine 5.6 plugin for **Instanced Skeletal Mesh Rendering**. It enables rendering large numbers of skeletal meshes with GPU-driven animation, significantly improving performance compared to traditional USkeletalMeshComponent.

## Build Commands

### Compile Plugin
```powershell
# Run the Chinese-localized build script
pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1

# Or use the batch wrapper
./开始编译.bat
```

The script uses UE's RunUAT to build the plugin:
- Output: `../CompiledPlugin/`
- Platform: Win64
- Requires: UE 5.6, Visual Studio 2022 with C++ game development workload

### Manual Build (Command Line)
```batch
<Path_to_UE>\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin="<PluginPath>/Skelot.uplugin" -Package="<OutputPath>" -TargetPlatforms=Win64 -VS2022
```

## Architecture

### Module Structure
- **Skelot** (Runtime): Core rendering logic, loaded at `PostConfigInit`
- **SkelotEd** (Editor): Editor-only functionality, loaded at `PostEngineInit`

### Core Classes

| Class | Purpose |
|-------|---------|
| `ASkelotWorld` | Singleton actor managing all Skelot instances. Use `ASkelotWorld::Get()` to access. |
| `USkelotAnimCollection` | Data asset storing baked animation data in GPU buffers. One per skeleton. |
| `USkelotInstanceComponent` | Helper component syncing transform to Skelot instance. |
| `USkelotClusterComponent` | Renders batches of skeletal meshes via custom scene proxy. |

### Instance Management Flow
1. Get `ASkelotWorld` singleton via `ASkelotWorld::Get(Context)`
2. Create instance: `CreateInstance(Transform, RenderDesc)` returns `FSkelotInstanceHandle`
3. Modify instance via index (faster) or handle (safer): `SetInstanceTransform(InstanceIndex, Transform)`
4. Destroy instance: `DestroyInstance(Handle)`

### Rendering Pipeline
- **Vertex Factory**: `TSkelotVertexFactory<MBI>` where MBI = max bone influences (4 or 8)
- **Animation Buffer**: `FSkelotAnimationBuffer` stores bone transforms in VRAM (FMatrix3x4 or FMatrix3x4Half)
- **Curve Buffer**: `FSkelotCurveBuffer` stores animation curve values
- Supports GPU Scene and Manual Vertex Fetch modes

### Key Data Structures
- `FSkelotInstancesSOA`: Structure-of-arrays for instance data (transforms, animation state, custom data)
- `FSkelotInstanceRenderDesc`: Describes meshes, materials, and animation collection for instances
- `FSkelotSequenceDef`: Animation sequence definition with sample frequency

## Preprocessor Definitions
```cpp
SKELOT_WITH_EXTRA_BONE=1      // Support >4 bone influences
SKELOT_WITH_MANUAL_VERTEX_FETCH=1
SKELOT_WITH_GPUSCENE=1
```

## Platform Support
- Win64, Linux, Mac (Runtime + Editor)
- Android, iOS (Runtime only)

## Dependencies
- Niagara (required plugin)
- Renderer module (private include paths used)

## 开发注意事项

详细开发注意事项见 `docs/DEV_NOTES.md`，关键规则如下：

### 工具使用

1. **Edit 工具必须先 Read** - 编辑任何文件前必须先使用 Read 工具读取，否则会报错
2. **字符串精确匹配** - Edit 的 old_string 必须与文件内容完全一致（包括空行、缩进），建议从 Read 输出直接复制
3. **Grep 结果处理** - Grep 输出包含行号前缀，复制代码时需去除

### 代码规范

1. **新增文件位置**：
   - 头文件：`Source/Skelot/Public/` 或 `Source/SkelotEd/Public/`
   - 源文件：`Source/Skelot/Private/` 或 `Source/SkelotEd/Private/`

2. **头文件包含顺序**：预编译头 → 引擎头文件 → 项目头文件 → `.generated.h`

3. **避免循环依赖**：使用前向声明 `class ClassName;` 替代 `#include`

