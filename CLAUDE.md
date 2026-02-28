# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
