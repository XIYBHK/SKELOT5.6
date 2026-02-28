# Skelot 技术参考文档

> 本文档面向进行二次开发的开发者，提供详细的架构说明和 API 参考。

## 目录

1. [架构概览](#架构概览)
2. [核心模块](#核心模块)
3. [数据结构详解](#数据结构详解)
4. [渲染管线](#渲染管线)
5. [动画系统](#动画系统)
6. [API 参考](#api-参考)
7. [Shader 系统](#shader-系统)
8. [扩展指南](#扩展指南)

---

## 架构概览

### 系统架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                         Game Thread                             │
│  ┌─────────────┐    ┌──────────────────┐    ┌───────────────┐  │
│  │ ASkelotWorld│◄───│USkelotWorldSubsystem│◄──│ Blueprint/C++ │  │
│  │  (Singleton)│    └──────────────────┘    └───────────────┘  │
│  └──────┬──────┘                                                │
│         │ manages                                               │
│         ▼                                                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              FSkelotInstancesSOA (实例数据)              │   │
│  │  ┌─────────┬─────────┬─────────┬─────────┬─────────┐   │   │
│  │  │Locations│Rotations│ AnimData│ClusterDt│UserData │   │   │
│  │  └─────────┴─────────┴─────────┴─────────┴─────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
│         │                                                       │
│         │ references                                            │
│         ▼                                                       │
│  ┌──────────────────────┐    ┌─────────────────────────────┐   │
│  │FSkelotInstanceRenderDesc│◄──│   USkelotAnimCollection     │   │
│  │   (渲染描述)          │    │   (动画数据资产)            │   │
│  └──────────────────────┘    └─────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Render Thread
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                        Render Thread                            │
│  ┌────────────────────┐    ┌───────────────────────────────┐   │
│  │USkelotClusterComponent│──►│    FSkelotProxy (SceneProxy)  │   │
│  └────────────────────┘    └───────────────────────────────┘   │
│                                       │                         │
│                                       ▼                         │
│  ┌────────────────────────────────────────────────────────────┐│
│  │                    GPU Resources                           ││
│  │  ┌───────────────────┐  ┌────────────────────────────┐   ││
│  │  │FSkelotAnimationBuffer│ │  FSkelotVertexFactory     │   ││
│  │  │  (Bone Transforms) │  │  (TSkelotVertexFactory<N>)│   ││
│  │  └───────────────────┘  └────────────────────────────┘   ││
│  └────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

---

## 核心模块

### 1. Skelot 模块 (Runtime)

**加载阶段**: `PostConfigInit`

**依赖模块**:
- Core, RenderCore, Engine, Renderer, RHI
- Niagara, NiagaraAnimNotifies
- Chaos, PhysicsCore, Landscape

**预处理器定义**:
```cpp
SKELOT_WITH_EXTRA_BONE=1        // 支持 8 骨骼影响（默认 4）
SKELOT_WITH_MANUAL_VERTEX_FETCH=1  // 手动顶点获取
SKELOT_WITH_GPUSCENE=1          // GPU Scene 支持
```

### 2. SkelotEd 模块 (Editor)

**加载阶段**: `PostEngineInit`

**依赖模块**:
- Skelot (核心模块)
- UnrealEd, Blutility, AssetRegistry, AssetTools
- PropertyEditor, InputCore

---

## 数据结构详解

### FSkelotInstanceHandle

实例句柄，用于安全地引用 Skelot 实例。

```cpp
USTRUCT(BlueprintType)
struct FSkelotInstanceHandle
{
    int32 InstanceIndex;  // 实例索引
    uint32 Version;       // 版本号（用于验证有效性）

    bool IsValid() const { return Version != 0; }
};
```

**设计说明**:
- 使用版本号机制防止引用已销毁的实例
- 当实例被销毁时，Slot 的 Version 会递增，使旧 Handle 失效

### FSkelotInstancesSOA

实例数据的 Structure of Arrays 布局，优化缓存命中率。

```cpp
struct FSkelotInstancesSOA
{
    // 槽位数据（状态标志）
    TArray<FSlotData> Slots;

    // 变换数据（世界空间）
    TArray<FVector3d> Locations;      // 当前帧位置
    TArray<FQuat4f>   Rotations;      // 当前帧旋转
    TArray<FVector3f> Scales;         // 当前帧缩放

    // 上一帧变换（用于运动模糊）
    TArray<FVector3d> PrevLocations;
    TArray<FQuat4f>   PrevRotations;
    TArray<FVector3f> PrevScales;

    // 动画数据
    TArray<int32>     CurAnimFrames;  // 当前动画帧索引
    TArray<int32>     PreAnimFrames;  // 上一帧动画帧索引
    TArray<FAnimData> AnimDatas;      // 动画状态

    // 渲染聚类数据
    TArray<FClusterData> ClusterData;

    // 子网格索引
    TArray<uint8> SubmeshIndices;

    // 自定义数据
    TArray<float> PerInstanceCustomData;
    int32 MaxNumCustomDataFloat;

    // 用户数据
    TArray<FUserData> UserData;
    TArray<TObjectPtr<UObject>> UserObjects;
};
```

### FSlotData 状态标志

```cpp
struct FSlotData
{
    uint32 Version;              // 版本号
    uint32 bDestroyed : 1;       // 是否已销毁
    uint32 bAnimationLooped : 1; // 动画是否循环
    uint32 bAnimationPaused : 1; // 动画是否暂停
    uint32 bNoSequence : 1;      // 无动画序列
    uint32 bDynamicPose : 1;     // 动态姿势（用于布娃娃）
    uint32 bExtractRootMotion : 1;
    uint32 bApplyRootMotion : 1;
    uint32 bCreatedThisFrame : 1;
    uint8 UserFlags;             // 用户自定义标志
};
```

### FSkelotInstanceRenderDesc

定义实例的渲染参数。

```cpp
USTRUCT(BlueprintType)
struct FSkelotInstanceRenderDesc
{
    USkelotAnimCollection* AnimCollection;  // 动画集合
    TArray<FSkelotMeshRenderDesc> Meshes;   // 网格列表

    // 渲染设置
    int32 NumCustomDataFloat;       // 自定义数据数量
    float MaxDrawDistance;          // 最大绘制距离
    float BoundsScale;              // 边界缩放
    bool bCastDynamicShadow;        // 投射动态阴影
    bool bRenderCustomDepth;        // 渲染自定义深度
    // ... 更多渲染标志
};
```

---

## 渲染管线

### Vertex Factory 层次结构

```
FVertexFactory
    └── FSkelotVertexFactory
            ├── TSkelotVertexFactory<4>  // 4 骨骼影响
            └── TSkelotVertexFactory<8>  // 8 骨骼影响
```

### GPU 数据流

```
┌─────────────────────────────────────────────────────────────┐
│                    Animation Buffer                          │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ Frame 0: [Bone0][Bone1]...[BoneN]                       ││
│  │ Frame 1: [Bone0][Bone1]...[BoneN]                       ││
│  │ ...                                                     ││
│  │ Frame N: [Bone0][Bone1]...[BoneN]                       ││
│  │ [Transition Poses...]                                   ││
│  │ [Dynamic Poses...]                                      ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│                  Vertex Shader                               │
│  1. 读取 Instance 的 FrameIndex                              │
│  2. GetBoneMatrix(FrameIndex, BoneIndex)                    │
│  3. 混合骨骼矩阵：BlendMatrix = Σ(Weight * BoneMatrix)       │
│  4. 变换顶点：Position = mul(BlendMatrix, LocalPosition)     │
│  5. 应用实例变换：WorldPos = mul(LocalToWorld, Position)     │
└─────────────────────────────────────────────────────────────┘
```

### 集群（Cluster）渲染

```
FSkelotCluster
├── Component: USkelotClusterComponent  // 渲染组件
├── Instances: TArray<int32>            // 实例索引列表
├── InstanceRunRanges                   // 按子网格分组的实例范围
└── SortedInstances                     // 按重要性排序的实例
```

**渲染流程**:
1. `ASkelotWorld` 根据实例的 RenderDesc 分组
2. 每组创建 `FSkelotCluster`
3. `USkelotClusterComponent` 创建 `FSkelotProxy`
4. Proxy 收集实例数据，提交到 GPU

---

## 动画系统

### USkelotAnimCollection

动画数据资产，存储烘焙后的动画帧。

```cpp
UCLASS(BlueprintType)
class USkelotAnimCollection : public UDataAsset
{
    USkeleton* Skeleton;                    // 目标骨骼
    TArray<FSkelotSequenceDef> Sequences;   // 动画序列
    TArray<FSkelotMeshDef> Meshes;          // 支持的网格

    // 缓存设置
    TSet<FName> BonesToCache;               // 需要缓存变换的骨骼
    TArray<FName> CurvesToCache;            // 需要缓存的曲线

    // 运行时数据
    TUniquePtr<FSkelotAnimationBuffer> AnimationBuffer;  // GPU 动画缓冲
    TUniquePtr<FSkelotCurveBuffer> CurveBuffer;          // GPU 曲线缓冲

    // 过渡姿势池
    int32 MaxTransitionPose;                // 最大过渡姿势数
    TSparseArray<FTransition> Transitions;  // 过渡缓存
};
```

### 帧索引分配

```
┌────────────────────────────────────────────────────────────┐
│                    Frame Index Range                       │
├────────────────────────────────────────────────────────────┤
│ 0 ~ FrameCountSequences-1        │ 序列帧（预烘焙）        │
├──────────────────────────────────┼─────────────────────────┤
│ FrameCountSequences ~            │ 过渡帧（运行时生成）    │
│ FrameCountSequences+MaxTransitionPose-1                    │
├──────────────────────────────────┼─────────────────────────┤
│ FrameCountSequences+             │ 动态姿势（布娃娃等）    │
│ MaxTransitionPose ~ TotalFrameCount-1                      │
└──────────────────────────────────┴─────────────────────────┘
```

### 动画播放参数

```cpp
USTRUCT(BlueprintType)
struct FSkelotAnimPlayParams
{
    UAnimSequenceBase* Animation;      // 动画资产
    float StartAt;                     // 起始时间
    float PlayScale;                   // 播放速率
    float TransitionDuration;          // 过渡时长
    EAlphaBlendOption BlendOption;     // 混合选项
    bool bIgnoreTransitionGeneration;  // 忽略过渡生成
    bool bLoop;                        // 循环
    bool bUnique;                      // 唯一（不重复播放相同动画）
};
```

---

## API 参考

### ASkelotWorld - 核心单例

#### 获取单例

```cpp
// C++
ASkelotWorld* World = ASkelotWorld::Get(WorldContext);
ASkelotWorld* World = USkelotWorldSubsystem::GetSingleton(WorldContext);

// Blueprint
Get Skelot Singleton节点
```

#### 实例管理

```cpp
// 创建实例
FSkelotInstanceHandle CreateInstance(const FTransform& Transform, const FSkelotInstanceRenderDesc& Desc);
FSkelotInstanceHandle CreateInstance(const FTransform& Transform, USkelotRenderParams* RenderParams);

// 销毁实例
void DestroyInstance(int32 InstanceIndex);
void DestroyInstance(FSkelotInstanceHandle H);

// 验证句柄
bool IsHandleValid(FSkelotInstanceHandle H) const;
bool IsInstanceAlive(int32 InstanceIndex) const;
```

#### 变换操作

```cpp
// 获取
FVector GetInstanceLocation(int32 InstanceIndex) const;
FQuat4f GetInstanceRotation(int32 InstanceIndex) const;
FTransform GetInstanceTransform(int32 InstanceIndex) const;

// 设置
void SetInstanceLocation(int32 InstanceIndex, const FVector& L);
void SetInstanceRotation(int32 InstanceIndex, const FQuat4f& Q);
void SetInstanceTransform(int32 InstanceIndex, const FTransform& T);
```

#### 动画控制

```cpp
// 播放动画
float InstancePlayAnimation(int32 InstanceIndex, const FSkelotAnimPlayParams& Params);

// 状态查询
UAnimSequenceBase* GetPlayingAnimSequence(int32 InstanceIndex) const;
bool IsPlayingAnimation(int32 InstanceIndex, const UAnimSequenceBase* Animation) const;
float GetInstancePlayTime(int32 InstanceIndex) const;
float GetInstancePlayTimeRemaining(int32 InstanceIndex) const;

// 控制
void SetAnimationLooped(int32 InstanceIndex, bool bLoop);
void SetAnimationPaused(int32 InstanceIndex, bool bPause);
void ResetAnimationState(int32 InstanceIndex);
```

#### 子网格管理

```cpp
// 附加/分离网格
bool InstanceAttachMesh_ByName(int32 InstanceIndex, FName Name, bool bAttach);
bool InstanceAttachMesh_ByIndex(int32 InstanceIndex, int32 Index, bool bAttach);
bool InstanceAttachMesh_ByAsset(int32 InstanceIndex, const USkeletalMesh* Mesh, bool bAttach);
void InstanceDetachMeshes(int32 InstanceIndex);

// 工具函数
void AttachRandomhMeshByGroup(int32 InstanceIndex, FName GroupName);
void AttachAllMeshGroups(int32 InstanceIndex);
```

#### 层级附加

```cpp
// 附加子实例
bool InstanceAttachChild(FSkelotInstanceHandle Parent, FSkelotInstanceHandle Child,
                         FName SocketOrBoneName, const FTransform& RelativeTransform);

// 分离
void DetachInstanceFromParent(FSkelotInstanceHandle H);

// 查询
int32 GetInstanceParentIndex(int32 InstanceIndex) const;
void GetInstanceChildren(int32 InstanceIndex, TArray<FSkelotInstanceHandle>& OutChildren) const;
bool IsInstanceChildOf(int32 InstanceIndex, int32 ParentIndex) const;
```

#### 自定义数据

```cpp
// 设置自定义数据（传递给材质）
void SetInstanceCustomDataFloat(int32 InstanceIndex, float Float, int32 Offset);
float* GetInstanceCustomDataFloats(int32 InstanceIndex);

// 用户对象（GC 管理）
TObjectPtr<UObject> GetInstanceUserObject(int32 InstanceIndex);
void SetInstanceUserObject(int32 InstanceIndex, TObjectPtr<UObject> Object);

// 用户数据（原始指针）
template<typename T> T& GetInstanceUserDataAs(int32 InstanceIndex);
```

#### 动态姿势

```cpp
// 启用动态姿势（用于布娃娃）
bool EnableInstanceDynamicPose(int32 InstanceIndex, bool bEnable);

// 绑定到 SkeletalMeshComponent
bool TieDynamicPoseToComponent(int32 InstanceIndex, USkeletalMeshComponent* SrcComponent,
                                int32 UserData, bool bCopyTransform);
USkeletalMeshComponent* UntieDynamicPoseFromComponent(int32 InstanceIndex, int32* OutUserData);
```

### USkelotWorldSubsystem - 蓝图接口

所有函数都有 `WorldContext` 参数，可在蓝图中直接调用。

```cpp
// 实例
FSkelotInstanceHandle Skelot_CreateInstance(...);
void Skelot_DestroyInstance(...);

// 动画
float Skelot_PlayAnimation(...);
USkelotAnimCollection* Skelot_GetAnimCollection(...);

// 变换
FTransform SkelotGetTransform(...);
void SkelotSetTransform(...);

// 网格
bool Skelot_AttachMesh(...);
void Skelot_DetachMeshes(...);

// 层级
void SkelotAttachChild(...);
void SkelotDetachFromParent(...);
```

---

## Shader 系统

### Uniform Buffer

#### FSkelotAnimCollectionUniformParams

```hlsl
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSkelotAnimCollectionUniformParams)
    SHADER_PARAMETER(uint32, BoneCount)
    SHADER_PARAMETER(uint32, CurveCount)
    SHADER_PARAMETER(uint32, FrameCountSequences)
    SHADER_PARAMETER(uint32, MaxTransitionPose)
    SHADER_PARAMETER_SRV(Buffer<float4>, AnimationBuffer)
    SHADER_PARAMETER_SRV(Buffer<float>, CurveBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
```

#### FSkelotMeshVertexFetchParams

```hlsl
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSkelotMeshVertexFetchParams)
    SHADER_PARAMETER(uint32, ColorIndexMask)
    SHADER_PARAMETER(uint32, NumTexCoords)
    SHADER_PARAMETER(uint32, NumBoneInfluence)
    SHADER_PARAMETER_SRV(Buffer<float2>, VF_TexCoordBuffer)
    SHADER_PARAMETER_SRV(Buffer<float>, VF_PositionBuffer)
    SHADER_PARAMETER_SRV(Buffer<float4>, VF_PackedTangentsBuffer)
    SHADER_PARAMETER_SRV(Buffer<float4>, VF_ColorComponentsBuffer)
    SHADER_PARAMETER_SRV(Buffer<float4>, VF_BoneWeights)
    SHADER_PARAMETER_SRV(Buffer<uint4>, VF_BoneIndices)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
```

### 关键 Shader 函数

#### GetBoneMatrix

```hlsl
// 从动画缓冲获取骨骼矩阵
float3x4 GetBoneMatrix(uint AnimationFrameIndex, uint BoneIndex)
{
    uint TransformIndex = AnimationFrameIndex * SkelotAC.BoneCount + BoneIndex;
    float4 A = SkelotAC.AnimationBuffer[TransformIndex * 3 + 0];
    float4 B = SkelotAC.AnimationBuffer[TransformIndex * 3 + 1];
    float4 C = SkelotAC.AnimationBuffer[TransformIndex * 3 + 2];
    return float3x4(A, B, C);
}
```

#### CalcBoneMatrix

```hlsl
// 计算顶点的混合骨骼矩阵
float3x4 CalcBoneMatrix(FVertexFactoryInput Input, uint AnimationFrameIndex)
{
    float3x4 BoneMatrix = Input.BlendWeights.x * GetBoneMatrix(AnimationFrameIndex, Input.BlendIndices.x);
    BoneMatrix += Input.BlendWeights.y * GetBoneMatrix(AnimationFrameIndex, Input.BlendIndices.y);
    BoneMatrix += Input.BlendWeights.z * GetBoneMatrix(AnimationFrameIndex, Input.BlendIndices.z);
    BoneMatrix += Input.BlendWeights.w * GetBoneMatrix(AnimationFrameIndex, Input.BlendIndices.w);
    // ... 额外骨骼影响
    return BoneMatrix;
}
```

---

## 扩展指南

### 自定义 ASkelotWorld 子类

1. 创建继承自 `ASkelotWorld` 的类
2. 在项目设置中指定类路径：

```cpp
// DefaultSkelot.ini
[/Script/Skelot.SkelotDeveloperSettings]
SkelotWorldClass=/Script/YourModule.YourSkelotWorld
```

### 自定义动画通知

1. 实现 `ISkelotNotifyInterface`
2. 在 `USkelotAnimCollection::ForbiddenNotifies` 中排除不需要的引擎通知

### 添加新的渲染特性

1. 修改 `FSkelotInstanceRenderDesc` 添加新属性
2. 更新 `GetPackedFlags()` 和 `ComputeHash()`
3. 在 Shader 中通过 `CustomPrimitiveData` 或实例数据传递

### 性能优化建议

1. **批量操作**: 使用 `ForEachValidInstance` 批量处理实例
2. **LOD 管理**: 设置合理的 `MaxDrawDistance` 和 `LODDistances`
3. **过渡生成**: 对远处实例设置 `bIgnoreTransitionGeneration = true`
4. **集群模式**: 使用 `Tiled` 模式优化大规模实例渲染

---

## 配置文件

### DefaultSkelot.ini

```ini
[/Script/Skelot.SkelotDeveloperSettings]
MaxTransitionGenerationPerFrame=10
ClusterLifeTime=600
RenderDescLifetime=60
MaxSubmeshPerInstance=15
ClusterMode=None
SkelotWorldClass=
```

---

## 调试命令

```bash
# 绘制实例边界
Skelot.DrawInstanceBounds 1

# 显示调试信息
Skelot.DebugShowInfo 1
```

---

*文档版本: 1.0 | 适用于 Skelot 3.0 (UE 5.6)*
