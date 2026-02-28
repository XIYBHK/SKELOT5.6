// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimNotifyQueue.h"
#include "Components/MeshComponent.h"
#include "Skelot.h"
#include "SkelotBase.h"
#include "Chaos/Real.h"
#include "SpanAllocator.h"
#include "AlphaBlend.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/PerPlatformProperties.h"

#include "SkelotWorldBase.generated.h"

class USkelotAnimCollection;
class USkeletalMeshSocket;
class UAnimNotify_Skelot;
class ISkelotNotifyInterface;
class UAnimSequenceBase;
class USkelotRenderParams;
class USKelotClusterComponent;
class USkeletalMesh;
class UMaterialInterface;
class ASkelotWorld;
class USkeletalMeshComponent;

using SkelotInstanceIndex = int32;

// Skelot有效性枚举
UENUM(BlueprintType)
enum class ESkelotValidity : uint8
{
	Valid		UMETA(DisplayName="有效"),
	NotValid	UMETA(DisplayName="无效"),
};

// Skelot碰撞通道枚举 (8通道系统)
// 用于控制实例间的碰撞检测
UENUM(BlueprintType, meta = (DisplayName = "Skelot碰撞通道"))
enum class ESkelotCollisionChannel : uint8
{
	Channel0 = 0	UMETA(DisplayName = "通道0 (值=0x01)"),
	Channel1 = 1	UMETA(DisplayName = "通道1 (值=0x02)"),
	Channel2 = 2	UMETA(DisplayName = "通道2 (值=0x04)"),
	Channel3 = 3	UMETA(DisplayName = "通道3 (值=0x08)"),
	Channel4 = 4	UMETA(DisplayName = "通道4 (值=0x10)"),
	Channel5 = 5	UMETA(DisplayName = "通道5 (值=0x20)"),
	Channel6 = 6	UMETA(DisplayName = "通道6 (值=0x40)"),
	Channel7 = 7	UMETA(DisplayName = "通道7 (值=0x80)"),
};

// 碰撞通道工具函数
namespace SkelotCollision
{
	// 将通道枚举转换为位掩码值
	constexpr uint8 ChannelToMask(ESkelotCollisionChannel Channel)
	{
		return 1 << static_cast<uint8>(Channel);
	}

	// 检查两个实例是否应该碰撞
	// 碰撞条件: (MaskA & (1<<ChannelB)) && (MaskB & (1<<ChannelA))
	inline bool ShouldCollide(uint8 ChannelA, uint8 MaskA, uint8 ChannelB, uint8 MaskB)
	{
		return (MaskA & (1 << ChannelB)) && (MaskB & (1 << ChannelA));
	}

	// 默认碰撞掩码 (与所有通道碰撞)
	constexpr uint8 DefaultCollisionMask = 0xFF;

	// 默认碰撞通道
	constexpr uint8 DefaultCollisionChannel = 0; // Channel0
}


// Skelot实例句柄
USTRUCT(BlueprintType)
struct FSkelotInstanceHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skelot", meta = (DisplayName = "实例索引"))
	int32 InstanceIndex = 0;
	UPROPERTY()
	uint32 Version = 0;

	bool IsValid() const { return Version != 0; }

	bool operator == (const FSkelotInstanceHandle Other) const { return InstanceIndex == Other.InstanceIndex && Version == Other.Version; }

	FString ToDebugString() const;

	friend inline uint32 GetTypeHash(FSkelotInstanceHandle H) { return H.InstanceIndex / 8; }
};


template<> struct TIsPODType<FSkelotInstanceHandle> { enum { Value = true }; };
template<> struct TStructOpsTypeTraits<FSkelotInstanceHandle> : public TStructOpsTypeTraitsBase2<FSkelotInstanceHandle>
{
	enum
	{
		WithZeroConstructor = true,
		WithIdenticalViaEquality = true,
	};
};


// Skelot动画完成事件
USTRUCT(BlueprintType)
struct FSkelotAnimFinishEvent
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skelot", meta = (DisplayName = "实例句柄"))
	FSkelotInstanceHandle Handle;
	UPROPERTY(BlueprintReadOnly, Category = "Skelot", meta = (DisplayName = "实例索引"))
	int32 InstanceIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Skelot", meta = (DisplayName = "动画序列"))
	UAnimSequenceBase* AnimSequence = nullptr;
};



// Skelot动画通知事件
USTRUCT(BlueprintType)
struct FSkelotAnimNotifyEvent
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skelot", meta = (DisplayName = "实例句柄"))
	FSkelotInstanceHandle Handle;
	UPROPERTY(BlueprintReadOnly, Category = "Skelot", meta = (DisplayName = "动画序列"))
	UAnimSequenceBase* AnimSequence = nullptr;
	UPROPERTY(BlueprintReadOnly, Category = "Skelot", meta = (DisplayName = "通知名称"))
	FName NotifyName;
};

struct FSkelotAnimNotifyObjectEvent
{
	FSkelotInstanceHandle Handle;
	UAnimSequenceBase* AnimSequence = nullptr;
	ISkelotNotifyInterface* SkelotNotify = nullptr;
};

// Skelot动画播放参数
USTRUCT(BlueprintType)
struct SKELOT_API FSkelotAnimPlayParams
{
	GENERATED_USTRUCT_BODY()

	//animation to be played, only UAnimSequence and UAnimComposite are supported
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot", meta = (DisplayName = "动画"))
	UAnimSequenceBase* Animation = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot", meta = (DisplayName = "起始时间"))
	float StartAt = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot", meta = (DisplayName = "播放速率"))
	float PlayScale = 1;
	//if <= 0 then dont transition
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot", meta = (DisplayName = "过渡时长"))
	float TransitionDuration = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot", meta = (DisplayName = "混合选项"))
	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;
	//true if should only look for transition but not generate one, typically instances far from view don't need to generate transition
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot", meta = (DisplayName = "忽略过渡生成"))
	bool bIgnoreTransitionGeneration = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot", meta = (DisplayName = "循环"))
	bool bLoop = true;
	//if true then wont replay the same animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot", meta = (DisplayName = "唯一"))
	bool bUnique = false;
};



DECLARE_DYNAMIC_DELEGATE_ThreeParams(FSkelotGeneralDynamicDelegate, FSkelotInstanceHandle, Handle, FName, PayloadTag, UObject*, PayloadObject);
DECLARE_DELEGATE_ThreeParams(FSkelotGeneralDelegate, FSkelotInstanceHandle, FName, UObject*);

UCLASS(Abstract)
class USkelotBaseComponent : public UMeshComponent
{
	GENERATED_BODY()

};






//defines a dynamic pose enabled instance that takes bones transform from a USkeletalMeshComponent, typically used for physics simulation.
USTRUCT()
struct FSkelotFrag_DynPoseTie 
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> SKMeshComponent = nullptr;

	int32 UserData = -1;
	int32 TmpUploadIndex = -1;
	bool bCopyTransform = false;	//should SKMeshComponent's transform be copied to the Skelot instance every frame ?
};



/*
instance data as struct of arrays
*/
USTRUCT()
struct SKELOT_API FSkelotInstancesSOA
{
	GENERATED_BODY()

	struct FSlotData
	{
		uint32 Version = 1; //version zero should never exist, our handles are zero initialized
		
		uint32 bDestroyed : 1 = false; //true if instance is invalid

		//animation flags
		uint32 bAnimationLooped : 1 = false;
		uint32 bAnimationPaused : 1 = false;
		uint32 bNoSequence : 1 = true;
		uint32 bDynamicPose : 1 = false;
		//WIP, extract root motion from the current animation and store it in SOA.RootMotions 
		uint32 bExtractRootMotion : 1 = false;
		//WIP, consume and apply root motion 
		uint32 bApplyRootMotion : 1 = false;

		uint32 bCreatedThisFrame : 1 = false;

		uint8 UserFlags = 0;

		void IncVersion()
		{
			Version++;
			if(Version == 0)
				Version = 1;
		}
	};

	struct FAnimFrame
	{
		int32 Cur = 0;
		int32 Pre = 0;
	};

	struct FAnimData
	{
		//index for AnimCollection->Sequences[]. current playing sequence index
		uint16 AnimationCurrentSequence = 0xFFff;
		//index for AnimCollection->Transitions[] if any
		uint16 AnimationTransitionIndex = 0xFFff;
		//time since start of play
		float AnimationTime = 0;
		//#Note negative not supported
		float AnimationPlayRate = 1;
		//either UAnimSequence* or UAnimComposite*
		UAnimSequenceBase* CurrentAsset = nullptr; 


		bool IsSequenceValid() const { return AnimationCurrentSequence != 0xFFff; }
		bool IsTransitionValid() const { return AnimationTransitionIndex != 0xFFff; }
	};


	union FUserData
	{
		void* Pointer;
		SIZE_T Index;
	};

	struct FClusterData
	{
		int32 DescIdx = -1;	//index for ASkelotWorld.RenderDescs 
		int32 ClusterIdx = -1; //index for FSkelotInstanceRenderDesc.Clusters 
		int32 RenderIdx = -1; //index for FSkelotCluster.Instances 
		
		inline FSetElementId GetDescId() const { return FSetElementId::FromInteger(DescIdx); }
		inline FSetElementId GetClusterId() const { return FSetElementId::FromInteger(ClusterIdx); }
	};

	struct FMiscData
	{
		//index for AttachParentArray if there is any relation, -1 otherwise
		int32 AttachmentIndex = -1;
	};

	//most of the following array are accessed by instance index
	TArray<FSlotData>		Slots;
	//world space transform of instances, #Note switched to SOA with less data size, FTransform caused too much cache miss
	TArray<FVector3d>		Locations;
	TArray<FQuat4f>			Rotations;
	TArray<FVector3f>		Scales;

	//previous frame transform of instances
	TArray<FVector3d>		PrevLocations;
	TArray<FQuat4f>			PrevRotations;
	TArray<FVector3f>		PrevScales;

	//velocity of instances, used for PBD collision and RVO avoidance
	TArray<FVector3f>		Velocities;

	//collision channel of instances (0-7, maps to ESkelotCollisionChannel)
	TArray<uint8>			CollisionChannels;
	//collision mask of instances (bit flags for which channels to collide with)
	TArray<uint8>			CollisionMasks;

	//current animation frame index, (frame index could be in sequence range, transition, or dynamic pose)
	TArray<int32>			CurAnimFrames;
	//previous frame animation frame index
	TArray<int32>			PreAnimFrames;
	//
	TArray<FAnimData>		AnimDatas;
	//
	TArray<FClusterData>	ClusterData;
	//
	TArray<uint8>			SubmeshIndices;
	//
	TArray<FUserData>		UserData;
	//holds per instance custom data float, see GetInstanceCustomDataFloats()
	TArray<float>			PerInstanceCustomData;
	//
	int32 MaxNumCustomDataFloat = 0;
	
	
	//just like UserData but used by blueprint, array might be empty, resized on demand,see SetInstanceUserObject
	UPROPERTY()
	TArray<TObjectPtr<UObject>> UserObjects;

	TArray<FMiscData>			MiscData;
	



	TArray<FTransform3f> RootMotions;
};


USTRUCT()
struct SKELOT_API FSkelotSubMeshData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;

	UMaterialInterface* GetMaterial(int32 Index) const;
};

// Skelot网格渲染描述
USTRUCT(BlueprintType)
struct SKELOT_API FSkelotMeshRenderDesc
{
	GENERATED_BODY()

	//optional, but must be unique, used by InstanceAttachMesh_ByName()
	UPROPERTY(EditAnywhere, Category="渲染", meta = (DisplayName = "名称"))
	FName Name;
	//optional, group this mesh belongs to, used by AttachRandomhMeshByGroup()
	UPROPERTY(EditAnywhere, Category="渲染", meta = (DisplayName = "组名称"))
	FName GroupName;
	UPROPERTY(EditAnywhere, Category="渲染", meta = (GetAssetFilter = MeshesShouldBeExcluded, DisplayName = "网格"))
	TObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "覆盖材质"))
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "LOD屏幕尺寸缩放"))
	float LODScreenSizeScale = 1;
	UPROPERTY(EditAnywhere, Category="渲染", meta = (DisplayName = "默认附加"))
	bool bAttachByDefault = false;

	UMaterialInterface* GetMaterial(int32 Index) const;

	bool operator ==(const FSkelotMeshRenderDesc& Other) const;

	inline friend uint32 GetTypeHash(const FSkelotMeshRenderDesc& Self) 
	{
		return GetTypeHash(Self.Mesh) ^ GetTypeHash(Self.OverrideMaterials);
	}

};

struct FInstanceRunData
{
	int32 StartOffset, EndOffset;

	int32 Num() const { return EndOffset - StartOffset + 1; }
};

struct FInstanceIndexAndSortKey
{
	uint32 Index;
	uint32 Value;
};

struct FSkelotCluster
{
	//
	FIntPoint Coord;
	//is rooted, so wont GCed
	USKelotClusterComponent* Component = nullptr;
	//indices of Skelot instances 
	TArray<int32> Instances;
	//instances sorted by importance of their sub mesh. instances run are generated from this
	TArray<FInstanceIndexAndSortKey> SortedInstances;
	//accessed by [sub mesh index][range index]
	TArray<TArray<FInstanceRunData>> InstanceRunRanges;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TArray<uint32> InstanceRun_AvgInstanceCount;
#endif
	//
	
	//
	uint32 KillCounter = 0;
	//true if any instances was add/removed to this cluster this frame
	uint32 bAnyAddRemove : 1 = true;
	//true if this cluster is way far from main camera 
	//uint32 bLowQuality : 1 = false;
	//
	//uint8 NumIgnoredTick = 0;
	

	//we compare cluster by their Coordinate, used by TSet
	inline bool operator == (const FSkelotCluster& Other) const { return this->Coord == Other.Coord; }
	inline bool operator == (const FIntPoint& OtherTileCoord) const { return this->Coord == OtherTileCoord; }

	friend uint32 GetTypeHash(const FSkelotCluster& C) { return GetTypeHash(C.Coord); }
};


typedef TSet<FSkelotCluster> ClusterSetT;
typedef TArray<FSkelotCluster> ClusterArrayT;

// Skelot实例渲染描述，包含定义Skelot实例如何渲染的属性
USTRUCT(BlueprintType)
struct SKELOT_API FSkelotInstanceRenderDesc
{
	GENERATED_BODY()


	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "动画集合"))
	TObjectPtr<USkelotAnimCollection> AnimCollection;
	//array of all the sub meshes, duplicate mesh is allowed. use ASkelotWorld::InstanceAttachSubmesh_*** to attach/detach meshes to instances
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "网格列表"))
	TArray<FSkelotMeshRenderDesc> Meshes;

	//true if the instance may have any negative scale, doesn't effect key
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "可能有负行列式"))
	uint8 bMayHaveNegativeDeterminant : 1;

	//try to pack your data into 2 floats. custom data are send as float4 and two are already taken by animation frames.
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "自定义数据浮点数数量"))
	int32 NumCustomDataFloat;
	//see also GSkelot_ForcePerInstanceLocalBounds
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "每实例本地边界"))
	uint8 bPerInstanceLocalBounds : 1;
	//
	uint8 bIsNegativeDeterminant : 1;
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "最小绘制距离"))
	FPerPlatformFloat MinDrawDistance;
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "最大绘制距离"))
	FPerPlatformFloat MaxDrawDistance;
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "LOD缩放"))
	FPerPlatformFloat LODScale;
	/** If true, will be rendered in the main pass (z prepass, basepass, transparency) */
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "在主通道渲染"))
	uint8 bRenderInMainPass:1;
	/** If true, will be rendered in the depth pass even if it's not rendered in the main pass */
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (EditCondition = "!bRenderInMainPass", DisplayName = "在深度通道渲染"))
	uint8 bRenderInDepthPass:1;
	UPROPERTY(EditAnywhere, Category="渲染", meta = (DisplayName = "接收贴花"))
	uint8 bReceivesDecals:1;
	UPROPERTY(EditAnywhere, Category="渲染", meta = (DisplayName = "用作遮挡体"))
	uint8 bUseAsOccluder:1;
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "投射动态阴影"))
	uint8 bCastDynamicShadow:1;
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "渲染自定义深度"))
	uint8 bRenderCustomDepth:1;
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (editcondition = "bRenderCustomDepth", DisplayName = "自定义深度模板写入遮罩"))
	ERendererStencilMask CustomDepthStencilWriteMask;
	UPROPERTY(EditAnywhere, Category="渲染",  meta=(UIMin = "0", UIMax = "255", editcondition = "bRenderCustomDepth", DisplayName = "自定义深度模板值"))
	int32 CustomDepthStencilValue;
	UPROPERTY(EditAnywhere, Category= "渲染", meta = (DisplayName = "光照通道"))
	FLightingChannels LightingChannels;
	UPROPERTY(EditAnywhere, Category="渲染", meta=(UIMin = "1", UIMax = "10.0", DisplayName = "边界缩放"))
	float BoundsScale;
	UPROPERTY(EditAnywhere, Category= "渲染", meta = (DisplayName = "随机种子"))
	uint32 Seed;
	//distance based LOD. used for legacy render when GPUScene not available on platform. must be sorted from low to high.
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (DisplayName = "LOD距离"))
	float LODDistances[SKELOT_MAX_LOD - 1];

	//
	mutable TArray<int32> CachedMeshDefIndices;

	FSkelotInstanceRenderDesc();

	void CacheMeshDefIndices();

	int32 IndexOfMesh(const USkeletalMesh* InMesh) const { return Meshes.IndexOfByPredicate([&](const FSkelotMeshRenderDesc& Elem) { return Elem.Mesh == InMesh; }); };
	int32 IndexOfMesh(FName Name) const { return Meshes.IndexOfByPredicate([&](const FSkelotMeshRenderDesc& Elem) { return Elem.Name == Name; }); };

	void AddMesh(USkeletalMesh* InMesh);
	void RemoveMesh(USkeletalMesh* InMesh);


	uint32 GetPackedFlags() const;

	friend uint32 GetTypeHash(const FSkelotInstanceRenderDesc& In) { return In.ComputeHash(); }

	uint32 ComputeHash() const;

	bool Equal(const FSkelotInstanceRenderDesc& Other) const;

	void AddAllMeshes();
};

//////////////////////////////////////////////////////////////////////////
// Skelot渲染参数数据资产，包含FSkelotInstanceRenderDesc，主要用于蓝图
UCLASS(Blueprintable, BlueprintType, meta=(DisplayName="Skelot渲染参数"))
class SKELOT_API USkelotRenderParams : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "渲染", meta = (ShowOnlyInnerProperties, DisplayName = "渲染数据"))
	FSkelotInstanceRenderDesc Data;


	UFUNCTION()
	bool MeshesShouldBeExcluded(const FAssetData& AssetData) const;

	UFUNCTION(CallInEditor, Category="渲染", meta=(DisplayName="添加选中网格"))
	void AddSelectedMeshes();

	UFUNCTION(CallInEditor, Category="渲染", meta=(DisplayName="添加所有网格"))
	void AddAllMeshes()
	{
		Data.AddAllMeshes();
	}

#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};


//final struct, contains runtime data too
USTRUCT()
struct SKELOT_API FSkelotInstanceRenderDescFinal : public FSkelotInstanceRenderDesc
{
	GENERATED_BODY()

	TSet<FSkelotCluster> Clusters;
	//
	uint32 KillCounter = 0;
	
	FSkelotInstanceRenderDescFinal(){}
	FSkelotInstanceRenderDescFinal(const FSkelotInstanceRenderDesc& Copy) 
	{
		*static_cast<FSkelotInstanceRenderDesc*>(this) = Copy;
	}
	bool operator == (const FSkelotInstanceRenderDescFinal& Other) const { return Equal(Other); }
	bool operator == (const FSkelotInstanceRenderDesc& Other) const { return Equal(Other);  }
};



class ASkelotWorld_Impl;


//keeps relation data of a Skelot instance, used for attachment hierarchy. see InstanceAttachChild() DetachInstanceFromParent()
struct FSkelotAttachParentData
{
	FTransform3f RelativeTransform = FTransform3f::Identity; //most of the time not used
	SkelotInstanceIndex InstanceIndex = -1;
	SkelotInstanceIndex Parent = -1;	//instance index of the parent 
	SkelotInstanceIndex FirstChild = -1;  //-1 means it has no children
	SkelotInstanceIndex Down = -1;	//instance index of the next sibling
	int32 SocketBoneIndex = -1;	//resolved from socket name for fast access
	USkeletalMeshSocket* SocketPtr = nullptr; //not worried of GC since skeletal meshes are referenced already
};

struct FSkelotLifeSpan
{
	double DeathTime;
};



//keeps a delegate and its values
USTRUCT()
struct FSkelotInstanceGeneralDelegate
{
	GENERATED_BODY()

	using TimerDelegateT = TVariant<FSkelotGeneralDynamicDelegate, FSkelotGeneralDelegate>;
	//
	TimerDelegateT Delegate;
	//
	UPROPERTY()
	TObjectPtr<UObject> PayloadObject;
	//
	FName PayloadTag;
};

USTRUCT()
struct FSkelotInstanceTimerData 
{
	GENERATED_BODY()


	UPROPERTY()
	FSkelotInstanceGeneralDelegate Data;

	double FireTime;
	float Interval;
	bool bLoop;
	uint8 TimeIndex; //0 == GetRealTimeSeconds(), 1  == GetTimeSeconds()
};



