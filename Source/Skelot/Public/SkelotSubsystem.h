// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "SkelotWorld.h"
#include "Subsystems/WorldSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "SkelotSubsystem.generated.h"


class USkelotWorldSubsystem_Impl;
class UActorComponent;

UCLASS()
class SKELOT_API USkelotWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY()
	ASkelotWorld* PrimaryInstance;

	USkelotWorldSubsystem_Impl* Impl() { return (USkelotWorldSubsystem_Impl*)this; }

	bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;  }

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot", meta=(WorldContext="WorldContextObject", CompactNodeTitle = "Skelot单例", DisplayName = "获取Skelot单例", Keywords="Get Skelot World"))
	static ASkelotWorld* GetSingleton(const UObject* WorldContextObject);

	static ASkelotWorld* Internal_GetSingleton(const UWorld* World, bool bCreateIfNotFound);
	static ASkelotWorld* Internal_GetSingleton(const UObject* WorldContextObject, bool bCreateIfNotFound);

	//helper to get singleton and also check validity of the handle
	static ASkelotWorld* GetSingleton(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);
	static ASkelotWorld* GetSingleton(const UObject* WorldContextObject, FSkelotInstanceHandle Handle0, FSkelotInstanceHandle Handle1);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|实例", meta=(WorldContext="WorldContextObject", DisplayName = "销毁实例"))
	static void Skelot_DestroyInstance(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|实例", meta=(WorldContext="WorldContextObject", DisplayName = "销毁多个实例"))
	static void Skelot_DestroyInstances(const UObject* WorldContextObject, const TArray<FSkelotInstanceHandle>& Handles);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|实例", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm = "Transform", DisplayName = "创建实例"))
	static FSkelotInstanceHandle Skelot_CreateInstance(const UObject* WorldContextObject, const FTransform& Transform, USkelotRenderParams* RenderParams, UObject* UserObject);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|实例", meta=(WorldContext="WorldContextObject", DisplayName = "批量创建实例"))
	static void Skelot_CreateInstances(const UObject* WorldContextObject, const TArray<FTransform>& Transforms, USkelotRenderParams* RenderParams, TArray<FSkelotInstanceHandle>& OutHandles);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|动画", meta=(WorldContext="WorldContextObject", DisplayName = "播放动画"))
	static float Skelot_PlayAnimation(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, const FSkelotAnimPlayParams& Params);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category="Skelot|动画", meta=(WorldContext="WorldContextObject", DisplayName = "获取动画集合"))
	static USkelotAnimCollection* Skelot_GetAnimCollection(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);


	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|渲染", meta=(WorldContext="WorldContextObject", DisplayName = "设置渲染参数(资产)"))
	static void Skelot_SetRenderParam(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, USkelotRenderParams* RenderParams);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|渲染", meta=(WorldContext="WorldContextObject", DisplayName = "设置渲染参数(结构体)"))
	static void Skelot_SetRenderDesc(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, const FSkelotInstanceRenderDesc& RenderParams);

	//////////////////////////////////////////////////////////////////////////
	//either Mesh|OrName|OrIndex must be valid
	UFUNCTION(BlueprintCallable, Category="Skelot|渲染", meta=(WorldContext="WorldContextObject", DisplayName = "附加网格体"))
	static bool Skelot_AttachMesh(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, const USkeletalMesh* Mesh, FName OrName, int32 OrIndex = -1, bool bAttach = true);
	//detach all the meshes from the specified instance
	UFUNCTION(BlueprintCallable, Category="Skelot|渲染", meta=(WorldContext="WorldContextObject", DisplayName = "分离所有网格体"))
	static void Skelot_DetachMeshes(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|渲染", meta=(WorldContext="WorldContextObject", DisplayName = "附加多个网格体"))
	static void Skelot_AttachMeshes(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, const TArray<USkeletalMesh*>& Meshes, bool bAttach = true);

	//////////////////////////////////////////////////////////////////////////
	// Collision Channel API
	UFUNCTION(BlueprintCallable, Category="Skelot|碰撞", meta=(WorldContext="WorldContextObject", DisplayName = "设置实例碰撞通道"))
	static void Skelot_SetInstanceCollisionChannel(const UObject* WorldContextObject, int32 InstanceIndex, ESkelotCollisionChannel Channel);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|碰撞", meta=(WorldContext="WorldContextObject", DisplayName = "获取实例碰撞通道"))
	static ESkelotCollisionChannel Skelot_GetInstanceCollisionChannel(const UObject* WorldContextObject, int32 InstanceIndex);
	UFUNCTION(BlueprintCallable, Category="Skelot|碰撞", meta=(WorldContext="WorldContextObject", DisplayName = "设置实例碰撞通道(句柄)"))
	static void Skelot_SetInstanceCollisionChannelByHandle(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, ESkelotCollisionChannel Channel);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|碰撞", meta=(WorldContext="WorldContextObject", DisplayName = "获取实例碰撞通道(句柄)"))
	static ESkelotCollisionChannel Skelot_GetInstanceCollisionChannelByHandle(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);

	UFUNCTION(BlueprintCallable, Category="Skelot|碰撞", meta=(WorldContext="WorldContextObject", DisplayName = "设置实例碰撞掩码"))
	static void Skelot_SetInstanceCollisionMask(const UObject* WorldContextObject, int32 InstanceIndex, uint8 Mask);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|碰撞", meta=(WorldContext="WorldContextObject", DisplayName = "获取实例碰撞掩码"))
	static uint8 Skelot_GetInstanceCollisionMask(const UObject* WorldContextObject, int32 InstanceIndex);
	UFUNCTION(BlueprintCallable, Category="Skelot|碰撞", meta=(WorldContext="WorldContextObject", DisplayName = "设置实例碰撞掩码(句柄)"))
	static void Skelot_SetInstanceCollisionMaskByHandle(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, uint8 Mask);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|碰撞", meta=(WorldContext="WorldContextObject", DisplayName = "获取实例碰撞掩码(句柄)"))
	static uint8 Skelot_GetInstanceCollisionMaskByHandle(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);


	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", DisplayName = "按组随机附加网格体"))
	static void Skelot_AttachRandomhMeshByGroup(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, FName GroupName);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", DisplayName = "附加所有网格组"))
	static void Skelot_AttachAllMeshGroups(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);


	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|渲染", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm = "Value", DisplayName = "设置自定义数据Float4"))
	static void Skelot_SetCustomDataFloat4(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, int32 Offset, const FVector4f& Value);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|渲染", meta=(WorldContext="WorldContextObject", DisplayName = "获取自定义数据Float"))
	static float Skelot_GetCustomDataFloat(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, int32 FloatIndex = 0);






	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm = "Center", DisplayName = "查询球形重叠位置"))
	static void SkelotQueryLocationOverlappingSphere(const UObject* WorldContextObject, const FVector& Center, float Radius, TArray<FSkelotInstanceHandle>& Instances);

	/**
	 * 查询球形范围内的实例（带碰撞掩码过滤）
	 * @param WorldContextObject 世界上下文对象
	 * @param Center 球心位置（世界坐标）
	 * @param Radius 球体半径（厘米）
	 * @param CollisionMask 碰撞掩码过滤，0xFF表示不过滤
	 * @param Instances 输出实例句柄数组
	 */
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm = "Center", DisplayName = "查询球形重叠位置(带掩码)"))
	static void SkelotQueryLocationOverlappingSphereWithMask(const UObject* WorldContextObject, const FVector& Center, float Radius, uint8 CollisionMask, TArray<FSkelotInstanceHandle>& Instances);

	/**
	 * 设置空间网格单元大小
	 * @param WorldContextObject 世界上下文对象
	 * @param CellSize 单元大小（厘米），建议设为查询半径的1-2倍
	 */
	UFUNCTION(BlueprintCallable, Category="Skelot|空间查询", meta=(WorldContext="WorldContextObject", DisplayName = "设置空间网格单元大小"))
	static void Skelot_SetSpatialGridCellSize(const UObject* WorldContextObject, float CellSize);

	/**
	 * 获取空间网格单元大小
	 * @param WorldContextObject 世界上下文对象
	 * @return 单元大小（厘米）
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|空间查询", meta=(WorldContext="WorldContextObject", DisplayName = "获取空间网格单元大小"))
	static float Skelot_GetSpatialGridCellSize(const UObject* WorldContextObject);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", DisplayName = "移除无效句柄"))
	static void Skelot_RemoveInvalidHandles(const UObject* WorldContextObject, bool bMaintainOrder, TArray<FSkelotInstanceHandle>& Handles);
	//////////////////////////////////////////////////////////////////////////
	//returns all the valid instance handles
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", DisplayName = "获取所有句柄"))
	static void Skelot_GetAllHandles(const UObject* WorldContextObject, UPARAM(ref) TArray<FSkelotInstanceHandle>& Handles);



	//////////////////////////////////////////////////////////////////////////
	//Attach @Child instance to @Parent
	UFUNCTION(BlueprintCallable, Category="Skelot|层级", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm = "ReletiveTransform", DisplayName = "附加子实例"))
	static void SkelotAttachChild(const UObject* WorldContextObject, FSkelotInstanceHandle Parent, FSkelotInstanceHandle Child, FName SocketOrBoneName, const FTransform& ReletiveTransform);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|层级", meta=(WorldContext="WorldContextObject", DisplayName = "从父级分离"))
	static void SkelotDetachFromParent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|实例", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm = "ReletiveTransform", DisplayName = "创建附加实例"))
	static FSkelotInstanceHandle Skelot_CreateInstanceAttached(const UObject* WorldContextObject, USkelotRenderParams* RenderParams, FSkelotInstanceHandle Parent, FName SocketOrBoneName, const FTransform& ReletiveTransform);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|层级", meta=(WorldContext="WorldContextObject", DisplayName = "获取子实例列表"))
	static void SkelotGetChildren(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, TArray<FSkelotInstanceHandle>& Children);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|层级", meta=(WorldContext="WorldContextObject", DisplayName = "获取父实例"))
	static FSkelotInstanceHandle SkelotGetParent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|层级", meta=(WorldContext="WorldContextObject", DisplayName = "是否为子实例"))
	static bool SkelotIsChildOf(const UObject* WorldContextObject, FSkelotInstanceHandle Child, FSkelotInstanceHandle Parent);


	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|变换", meta=(WorldContext="WorldContextObject", DisplayName = "获取变换"))
	static FTransform SkelotGetTransform(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|变换", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="Transform", DisplayName = "设置变换"))
	static void SkelotSetTransform(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, const FTransform& Transform, bool bRelative);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category="Skelot|变换", meta=(WorldContext="WorldContextObject", DisplayName = "获取Socket变换"))
	static FTransform SkelotGetSocketTransform(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, FName SocketOrBoneName, USkeletalMesh* InMesh = nullptr, bool bWorldSpace = true);


	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|数据", meta=(WorldContext="WorldContextObject", ExpandEnumAsExecs = "ExecResult", DeterminesOutputType = "Class", DisplayName = "获取用户对象"))
	static UObject* SkelotGetUserObject(const UObject* WorldContextObject, ESkelotValidity& ExecResult, FSkelotInstanceHandle Handle, TSubclassOf<UObject> Class);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|数据", meta=(WorldContext="WorldContextObject", DisplayName = "设置用户对象"))
	static void SkelotSetUserObject(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, UObject* Object);


	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", DisplayName = "启用动态姿势"))
	static void SkelotEnableDynamicPose(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, bool bEnable);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", DisplayName = "绑定到组件"))
	static void SkelotBindToComponent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, USkeletalMeshComponent* Component, int32 UserData, bool bCopyTransform);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", DisplayName = "从组件解绑"))
	static void SkelotUnbindFromComponent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, bool bKeepDynamicPoseEnabled, USkeletalMeshComponent*& OutComponent, int32& OuUserData);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", DisplayName = "获取绑定的组件"))
	static USkeletalMeshComponent* SkelotGetBoundComponent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);


	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|杂项", meta=(WorldContext="WorldContextObject", DisplayName = "设置生命周期"))
	static void Skelot_SetLifespan(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, float Lifespan);
	//////////////////////////////////////////////////////////////////////////
	//#Note only one timer can be active per every instance.
	UFUNCTION(BlueprintCallable, Category="Skelot|杂项", meta=(WorldContext="WorldContextObject", DisplayName = "设置定时器"))
	static void Skelot_SetTimer(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, float Interval, bool bLoop, bool bGameTime, FSkelotGeneralDynamicDelegate Delegate, FName PayloadTag = NAME_None, UObject* PayloadObject = nullptr);
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|杂项", meta=(WorldContext="WorldContextObject", DisplayName = "清除定时器"))
	static void Skelot_ClearTimer(const UObject* WorldContextObject, FSkelotInstanceHandle Handle);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category="Skelot|杂项", meta=(WorldContext="WorldContextObject", DisplayName = "设置根运动参数"))
	static void SetRootMotionParams(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, bool bExtractRootMotion, bool bApplyRootMotion);

};


UCLASS()
class SKELOT_API USkelotFunctionLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Skelot|工具", meta=(WorldContext="WorldContextObject", DeterminesOutputType = "Class", DisplayName = "生成组件"))
	static UActorComponent* SpawnComponent(const UObject* WorldContextObject, TSubclassOf<UActorComponent> Class, const FTransform& Transform);

	//construct skeletal mesh components from an specified Skelot Instance.
	UFUNCTION(BlueprintCallable, Category="Skelot|杂项", meta=(WorldContext="WorldContextObject", DisplayName = "构建骨骼网格组件"))
	static USkeletalMeshComponent* ConstructSkeletalMeshComponents(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, UObject* Outer, bool bSetCustomPrimitiveDataFloat);
};
