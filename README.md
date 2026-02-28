# Skelot

**Skelot** 是一款专为 Unreal Engine 5.6 设计的高性能实例化骨骼网格渲染插件。通过 GPU 驱动的动画系统，实现大规模骨骼网格角色的高效渲染。

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.6-purple)](https://www.unrealengine.com/)
[![Platform](https://img.shields.io/badge/Platform-Win64%20%7C%20Linux%20%7C%20Mac%20%7C%20Android%20%7C%20iOS-blue)]()

---

## ✨ 核心特性

- 🚀 **GPU 驱动动画** - 动画数据完全在 GPU 上处理，CPU 开销极低
- 🎯 **实例化渲染** - 单次 Draw Call 渲染大量相同骨骼的角色
- 🎬 **平滑过渡** - 自动生成动画过渡，支持多种混合模式
- 🔗 **层级附加** - 支持实例间的父子附加和 Socket 挂载
- 🎭 **多网格支持** - 单个实例可附加多个骨骼网格（装备系统）
- 🎨 **自定义数据** - 支持向材质传递每实例自定义数据
- 💀 **动态姿势** - 支持与布娃娃系统或复杂 AnimBP 集成
- 📊 **曲线缓存** - 动画曲线数据可在材质中采样

---

## 📋 环境要求

| 依赖 | 版本 |
|------|------|
| Unreal Engine | 5.6 |
| Visual Studio | 2022 |
| Windows SDK | 10/11 |

**VS 工作负载**:
- 使用 C++ 的游戏开发
- MSVC v143 生成工具
- .NET 6.0/7.0/8.0 SDK

---

## 🚀 快速开始

### 编译插件

**方式一：使用脚本（推荐）**

双击运行 `开始编译.bat` 或在 PowerShell 中执行：

```powershell
pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1
```

编译完成后，插件输出到 `../CompiledPlugin/` 目录。

**方式二：手动编译**

```batch
<Path_to_UE>\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin ^
    -Plugin="Skelot.uplugin" ^
    -Package="<OutputPath>" ^
    -TargetPlatforms=Win64 ^
    -VS2022
```

### 安装到项目

1. 将编译后的 `CompiledPlugin/Skelot` 文件夹复制到你的项目 `Plugins/` 目录
2. 重启 Unreal Editor
3. 在插件管理器中启用 **Skelot**

---

## 📖 使用指南

### 1. 创建动画集合

动画集合（`USkelotAnimCollection`）是 Skelot 的核心资产，存储烘焙后的动画数据。

1. 右键 → Miscellaneous → Skelot 动画集合
2. 设置骨骼（Skeleton）
3. 添加动画序列（Sequences）
4. 添加网格（Meshes）
5. 点击"重新构建"

### 2. 创建渲染参数

渲染参数（`USkelotRenderParams`）定义实例的渲染配置。

1. 右键 → Blueprints → Data Asset → Skelot 渲染参数
2. 选择动画集合
3. 添加网格和材质覆盖
4. 配置渲染设置（绘制距离、阴影等）

### 3. 生成实例

**Blueprint:**

```
Get Skelot Singleton → Create Instance
```

**C++:**

```cpp
#include "SkelotWorld.h"

void SpawnSkelotInstance(UWorld* World)
{
    ASkelotWorld* SkelotWorld = ASkelotWorld::Get(World);

    USkelotRenderParams* Params = LoadObject<USkelotRenderParams>(...);

    FTransform SpawnTransform(FVector(0, 0, 0));
    FSkelotInstanceHandle Handle = SkelotWorld->CreateInstance(SpawnTransform, Params);
}
```

### 4. 播放动画

**Blueprint:**

```
Get Skelot Singleton → Play Animation
```

**C++:**

```cpp
FSkelotAnimPlayParams AnimParams;
AnimParams.Animation = MyAnimSequence;
AnimParams.bLoop = true;
AnimParams.TransitionDuration = 0.2f;

SkelotWorld->InstancePlayAnimation(Handle.InstanceIndex, AnimParams);
```

---

## 🏗️ 架构概览

```
┌─────────────────────────────────────────────────┐
│                  ASkelotWorld                    │
│              (单例 Actor, 管理所有实例)           │
├─────────────────────────────────────────────────┤
│  FSkelotInstancesSOA  │  FSkelotInstanceRenderDesc │
│     (实例数据)        │      (渲染描述)            │
├─────────────────────────────────────────────────┤
│           USkelotAnimCollection                  │
│           (动画数据资产, GPU Buffer)              │
├─────────────────────────────────────────────────┤
│        USkelotClusterComponent                   │
│           (渲染组件, SceneProxy)                  │
└─────────────────────────────────────────────────┘
```

---

## 📁 项目结构

```
Skelot/
├── Source/
│   ├── Skelot/                 # 运行时模块
│   │   ├── Public/
│   │   │   ├── Skelot.h        # 模块入口
│   │   │   ├── SkelotWorld.h   # 核心单例
│   │   │   ├── SkelotAnimCollection.h
│   │   │   ├── SkelotInstanceComponent.h
│   │   │   └── ...
│   │   └── Private/
│   │       ├── SkelotRenderResources.h  # GPU 资源
│   │       └── ...
│   └── SkelotEd/               # 编辑器模块
│       └── ...
├── Shaders/
│   └── Private/
│       ├── SkelotVertexFactory.ush    # 顶点工厂 Shader
│       ├── SkelotCPID.ush
│       └── SkelotCurve.ush
├── Content/                    # 蓝图工具函数
├── Resources/                  # 插件图标
├── Config/
│   └── BaseSkelot.ini         # 默认配置
├── Skelot.uplugin
├── CLAUDE.md                   # Claude Code 开发指南
└── TECHNICAL_REFERENCE.md      # 技术参考文档
```

---

## 🎮 性能对比

| 场景 | 传统 USkeletalMeshComponent | Skelot |
|------|---------------------------|--------|
| 1000 角色 | ~1000 Draw Calls | ~1-10 Draw Calls |
| CPU 动画更新 | 每角色每帧 | 仅 GPU |
| 内存占用 | 每角色独立 | 共享动画数据 |

---

## ⚙️ 配置选项

编辑 `Config/DefaultSkelot.ini`:

```ini
[/Script/Skelot.SkelotDeveloperSettings]
; 每帧最大过渡生成数
MaxTransitionGenerationPerFrame=10

; 集群生命周期（帧）
ClusterLifeTime=600

; 每实例最大子网格数
MaxSubmeshPerInstance=15

; 集群模式 (None, Tiled)
ClusterMode=None
```

---

## 🔧 调试

```bash
# 绘制实例边界
Skelot.DrawInstanceBounds 1

# 显示集群信息
Skelot.DebugClusters 1
```

---

## 📚 文档

- [技术参考文档](TECHNICAL_REFERENCE.md) - 详细的架构说明和 API 参考
- [CLAUDE.md](CLAUDE.md) - Claude Code 开发指南

---

## 🤝 支持

- **Discord**: https://discord.gg/uVVyWbprnP
- **Marketplace**: [Skelot on UE Marketplace](https://www.unrealengine.com/marketplace/product/skelot)

---

## 📜 许可证

Copyright 2024 Lazy Marmot Games. All Rights Reserved.

---

*Made with ❤️ for Unreal Engine developers*
