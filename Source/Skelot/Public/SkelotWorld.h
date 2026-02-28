// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once


#include "SkelotWorldBase.h"



#include "SkelotWorld.generated.h"

enum class ESkelotClusterMode : uint8;

/*
Skelot Singleton Actor, spawned automatically if not already in the world. (you may need to edit default properties ).

#Note that most functions take FSkelotInstanceHandle or int32 InstanceIndex. those taking InstanceIndex are faster but its caller's responsibly to send valid index.
you can check for validity of handles as follow:
if(SkelotWorld->IsHandleValid(Handle)
	SkelotWorld->FunctionXXX(Handle.InstanceIndex, ...);



*/
UCLASS(meta=(DisplayName="Skelot世界"))
class SKELOT_API ASkelotWorld : public AActor
{
	GENERATED_BODY()
public:
	

	FSpanAllocator HandleAllocator;
	uint32 InstanceIndexMask;
	uint32 MaxSubmeshPerInstance;
	ESkelotClusterMode ClusterMode;
	FVector ViewCenterForClusters;

	UPROPERTY(Transient)
	FSkelotInstancesSOA SOA;
	//current render descriptors
	UPROPERTY(Transient)
	TSet<FSkelotInstanceRenderDescFinal> RenderDescs;
	//
	TSparseArray<FSkelotAttachParentData> AttachParentArray;
	//
	UPROPERTY(Transient)
	TMap<int32, FSkelotFrag_DynPoseTie> DynamicPosTiedMap;
	//index of instances whose ClusterIndex is -1
	TArray<int32> InstancesNeedCluster;
	//
	uint32 ClusterUpdateCounter;
	
	//
	FAnimNotifyContext AnimationNotifyContext;
	TArray<FSkelotAnimFinishEvent> AnimationFinishEvents;
	TArray<FSkelotAnimNotifyEvent> AnimationNotifyEvents;
	TArray<FSkelotAnimNotifyObjectEvent> AnimationNotifyObjectEvents;
	
	//if true UAnimNotify* will be process, only skelot notifies are supported. see ISkelotNotifyInterface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skelot|动画", meta=(DisplayName="启用动画通知对象"))
	bool bEnableAnimNotifyObjects;
	//play rate for all animations
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skelot|动画", meta=(DisplayName="动画播放速率"))
	float AnimationPlayRate;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAnimFinished, ASkelotWorld*, Context, const TArray<FSkelotAnimFinishEvent>&, Events);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAnimNotify, ASkelotWorld*, Context, const TArray<FSkelotAnimNotifyEvent>&, Events);

	//is called when animation is finished (only non-looped animations)
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "动画播放完成时"), Category = "Skelot|动画")
	FOnAnimFinished OnAnimationFinishedDelegate;
	//is called for name only notifications (name notifications are enabled by default)
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "动画通知时"), Category = "Skelot|动画")
	FOnAnimNotify OnAnimationNotifyDelegate;
	//
	UPROPERTY(Transient)
	TMap<FSkelotInstanceHandle, double> LifeSpanMap;
	//
	UPROPERTY(Transient)
	TMap<FSkelotInstanceHandle, FSkelotInstanceTimerData> TimerMap;
	
	DECLARE_DELEGATE_ThreeParams(FOnWorldActorTick, ASkelotWorld*, ELevelTick, float);
	//
	FOnWorldActorTick OnWorldPreActorTick_End;

	//ctor dtor
	ASkelotWorld(const FObjectInitializer& ObjectInitializer);
	~ASkelotWorld();


	static ASkelotWorld* Get(const UObject* Context, bool bCreateIfNotFound = true);
	static ASkelotWorld* Get(const UWorld* World, bool bCreateIfNotFound = true);

	//cast to private implementation
	ASkelotWorld_Impl* Impl() { return (ASkelotWorld_Impl*)this; }
	const ASkelotWorld_Impl* Impl() const { return (const ASkelotWorld_Impl*)this; }

	//return true if handle is valid
	bool IsHandleValid(FSkelotInstanceHandle H) const
	{
		const FSkelotInstancesSOA::FSlotData& Slot = SOA.Slots[H.InstanceIndex & InstanceIndexMask];
		return H.Version == Slot.Version;
	}
	//checks the validity bit only
	bool IsInstanceAlive(int32 InstanceIndex) const { return !SOA.Slots[InstanceIndex].bDestroyed; }
	//returns number of instances (including destroyed ones)
	int32 GetNumInstance() const { return HandleAllocator.GetMaxSize(); }
	//returns number of valid instances
	int32 GetNumValidInstance() const { return HandleAllocator.GetSparselyAllocatedSize(); }
	//returns indices of all valid instances
	TArray<int32> GetAllValidInstances() const;
	
	//
	template<typename TLambda> void ForEachValidInstance(TLambda Proc)
	{
		for(int32 InstanceIndex = 0; InstanceIndex < GetNumInstance(); InstanceIndex++)
			if(IsInstanceAlive(InstanceIndex))
				Proc(InstanceIndex);
	}

	//////////////////////////////////////////////////////////////////////////
	//create an Skelot Instance, fast enough so don't think of pooling them
	FSkelotInstanceHandle CreateInstance(const FTransform& Transform, const FSkelotInstanceRenderDesc& Desc);
	FSkelotInstanceHandle CreateInstance(const FTransform& Transform, USkelotRenderParams* RenderParams);
	FSkelotInstanceHandle CreateInstance(const FTransform& Transform, FSetElementId DescId);

	//////////////////////////////////////////////////////////////////////////
	//destroy an instance
	void DestroyInstance(int32 InstanceIndex);
	void DestroyInstance(FSkelotInstanceHandle H);

	//////////////////////////////////////////////////////////////////////////
	//changes the render params of an instance, better to use this instead of individual functions like SetInstanceMaterial, InstanceAttachMeshes, ...
	void SetInstanceRenderParams(int32 InstanceIndex, const FSkelotInstanceRenderDesc& Desc);
	void SetInstanceRenderParams(int32 InstanceIndex, USkelotRenderParams* RenderParams);
	void SetInstanceRenderParams(int32 InstanceIndex, FSetElementId DescId);
	

	//returns pointer to the 0xFF terminated mesh indices array. value is index for FSkelotInstanceRenderDesc.Meshes[]
	uint8* GetInstanceSubmeshIndices(int32 InstanceIndex)				{ return SOA.SubmeshIndices.GetData() + (InstanceIndex * (MaxSubmeshPerInstance + 1)); }
	const uint8* GetInstanceSubmeshIndices(int32 InstanceIndex) const	{ return SOA.SubmeshIndices.GetData() + (InstanceIndex * (MaxSubmeshPerInstance + 1)); }

	//functions to attach/detach meshes 
	bool InstanceAttachMesh_ByName(int32 InstanceIndex, FName Name /*Name of FSkelotInstanceRenderDesc.Meshes[].Name */, bool bAttach);
	bool InstanceAttachMesh_ByIndex_Unsafe(int32 InstanceIndex, uint8 Index /*Index for FSkelotInstanceRenderDesc.Meshes[] */, bool bAttach);
	bool InstanceAttachMesh_ByIndex(int32 InstanceIndex, int32 Index /*Index for FSkelotInstanceRenderDesc.Meshes[] */, bool bAttach);
	bool InstanceAttachMesh_ByAsset(int32 InstanceIndex, const USkeletalMesh* Mesh, bool bAttach);
	//detach all the submeshes 
	void InstanceDetachMeshes(int32 InstanceIndex);

	//utility function to attach one unique skeletal mesh from the specified group
	void AttachRandomhMeshByGroup(int32 InstanceIndex, FName GroupName);
	//utility function to attach one unique skeletal mesh from every group
	void AttachAllMeshGroups(int32 InstanceIndex);
	//remove all the meshes of the specified group
	void DetachMeshesByGroup(int32 InstanceIndex, FName GroupName);
	//
	void GetInstanceAttachedMeshes(int32 InstanceIndex, TArray<USkeletalMesh*>& OutMeshes);
	//
	bool IsMeshAttached(int32 InstanceIndex, uint8 SubMeshIndex) const;
	//
	bool IsMeshAttached(int32 InstanceIndex, FName SubMeshName) const;



	//////////////////////////////////////////////////////////////////////////
	//play animation on the specified instance
	float InstancePlayAnimation(int32 InstanceIndex, const FSkelotAnimPlayParams& Params);
	//reset to reference pose (releases transition/dynamic pose if any)
	void ResetAnimationState(int32 InstanceIndex);


	//
	UAnimSequenceBase* GetPlayingAnimSequence(int32 InstanceIndex) const { return SOA.AnimDatas[InstanceIndex].CurrentAsset; }
	//returns true if the specified animation is being played by the the instance
	bool IsPlayingAnimation(int32 InstanceIndex, const UAnimSequenceBase* Animation) const { return SOA.AnimDatas[InstanceIndex].CurrentAsset == Animation; }
	//returns true if the specified instance is playing any animation
	bool IsPlayingAnyAnimation(int32 InstanceIndex) const { return SOA.AnimDatas[InstanceIndex].CurrentAsset != nullptr; }
	
	//returns the length of currently playing AnimSequence
	float GetInstancePlayLength(int32 InstanceIndex) const;
	//
	float GetInstancePlayTime(int32 InstanceIndex) const { return SOA.AnimDatas[InstanceIndex].AnimationTime; }
	//
	float GetInstancePlayTimeRemaining(int32 InstanceIndex) const;
	//
	float GetInstancePlayTimeFraction(int32 InstanceIndex) const;
	//
	float GetInstancePlayTimeRemainingFraction(int32 InstanceIndex) const { return 1.0f - GetInstancePlayTimeFraction(InstanceIndex); }
	//
	void SetAnimationLooped(int32 InstanceIndex, bool bLoop) { SOA.Slots[InstanceIndex].bAnimationLooped = bLoop; }
	bool IsAnimationLooped(int32 InstanceIndex) const { return SOA.Slots[InstanceIndex].bAnimationLooped; }
	//
	void SetAnimationPaused(int32 InstanceIndex, bool bPause) { SOA.Slots[InstanceIndex].bAnimationPaused = bPause; }

	//
	USkelotAnimCollection* GetInstanceAnimCollection(int32 InstanceIndex) const;
	USkelotAnimCollection* GetInstanceAnimCollection(FSkelotInstanceHandle H) const;

	//returns world space location of the instance
	inline FVector GetInstanceLocation(int32 InstanceIndex) const				{ return SOA.Locations[InstanceIndex]; }
	//returns world space rotation of the instance
	inline FQuat4f GetInstanceRotation(int32 InstanceIndex) const				{ return SOA.Rotations[InstanceIndex]; }
	//returns world space transform of the instance
	inline FTransform GetInstanceTransform(int32 InstanceIndex) const			{ return FTransform((FQuat4d)SOA.Rotations[InstanceIndex], SOA.Locations[InstanceIndex], (FVector3d)SOA.Scales[InstanceIndex]); }
	//
	inline FTransform GetInstancePrevTransform(int32 InstanceIndex) const		{ return FTransform((FQuat4d)SOA.PrevRotations[InstanceIndex], SOA.PrevLocations[InstanceIndex], (FVector3d)SOA.PrevScales[InstanceIndex]); }
	//
	inline void SetInstanceTransform(int32 InstanceIndex, const FTransform& T)	
	{
		SOA.Locations[InstanceIndex] = T.GetLocation(); 
		SOA.Rotations[InstanceIndex] = (FQuat4f)T.GetRotation();
		SOA.Scales[InstanceIndex]	 = (FVector3f)T.GetScale3D();
	}
	//
	inline void SetInstanceLocation(int32 InstanceIndex, const FVector& L)
	{
		SOA.Locations[InstanceIndex] = L;
	}
	//
	inline void SetInstanceRotation(int32 InstanceIndex, const FQuat4f& Q)
	{
		SOA.Rotations[InstanceIndex] = Q;
	}
	//
	inline void SetInstanceLocationAndRotation(int32 InstanceIndex, const FVector& L, const FQuat4f& Q)
	{ 
		SOA.Locations[InstanceIndex] = L;
		SOA.Rotations[InstanceIndex] = Q;
	}
	
	//internal
	FSkelotAttachParentData& GetOrCreateInstanceAttachParentData(int32 InstanceIndex);
	FSkelotAttachParentData* GetInstanceAttachParentData(int32 InstanceIndex) const;

	//attach @Child to @parent 
	bool InstanceAttachChild(FSkelotInstanceHandle Parent, FSkelotInstanceHandle Child, FName SocketOrBoneName, const FTransform& ReletiveTransform);
	//detach the instance from its parent if any
	void DetachInstanceFromParent(FSkelotInstanceHandle H);
	//
	void DetachInstanceFromParent(int32 InstanceIdx);

	FSkelotAttachParentData& Internal_InstanceAttachChild(int32 ParentIdx, int32 ChildIdx);
	//
	void SetInstanceRelativeTransform(int32 InstanceIndex, const FTransform3f& RelativeT);
	//returns the index of parent instance if any 
	int32 GetInstanceParentIndex(int32 InstanceIndex) const;
	//walks up the tree and see if three is such a parent
	bool IsInstanceChildOf(int32 InstanceIndex, int32 ParentIndex) const;
	//Walks up the attachment chain and returns the instance index at the top. returns -1 if no parent.
	int32 GetInstanceRootIndex(int32 InstanceIndex) const
	{
		int32 Top = GetInstanceParentIndex(InstanceIndex);
		for (; Top != -1 && GetInstanceParentIndex(Top) != -1; Top = GetInstanceParentIndex(Top));
		return Top;
	}


	using ChildVisitFunc = TFunctionRef<void(FSkelotAttachParentData& ParentFrag, FSkelotAttachParentData& ChildFrag)>;

	//visits all descendants
	void ForEachChild(int32 InstanceIndex, ChildVisitFunc Func);
	//visits all descendants
	void ForEachChild(FSkelotAttachParentData& Root, ChildVisitFunc Func);
	//
	template<bool bHandle /*handle or index ?*/, typename T> void GetInstanceChildren(int32 InstanceIndex, T& OutArray) const
	{
		const_cast<ASkelotWorld*>(this)->ForEachChild(InstanceIndex, [&](FSkelotAttachParentData& ParentFrag, FSkelotAttachParentData& ChildFrag) {
			if constexpr (bHandle)
				OutArray.Add(IndexToHandle(ChildFrag.InstanceIndex));
			else
				OutArray.Add(ChildFrag.InstanceIndex);
		});
	}

	//visits only direct offspring, not grand children 
	void ForEachOffspring(int32 InstanceIndex, ChildVisitFunc Func);
	
	//update the transform of attached children, if any
	void UpdateChildTransforms(int32 InstanceIndex);
	//internal
	void UpdateChildTransforms(FSkelotAttachParentData& RootFrag);

	//bad naming, use GetInstanceRenderDesc instead
	inline const FSkelotInstanceRenderDescFinal& GetInstanceDesc(int32 InstanceIdx) const		{ return this->RenderDescs.Get(FSetElementId::FromInteger(this->SOA.ClusterData[InstanceIdx].DescIdx)); }
	inline const FSkelotInstanceRenderDescFinal& GetInstanceRenderDesc(int32 InstanceIdx) const { return GetInstanceDesc(InstanceIdx); }

	//
	inline const FSkelotCluster& GetInstanceCluster(int32 InstanceIdx) const 
	{
		const FSkelotInstancesSOA::FClusterData& CD = SOA.ClusterData[InstanceIdx];
		return this->RenderDescs.Get(CD.GetDescId()).Clusters.Get(CD.GetClusterId());
	}


	//returns the cached bone transform (component space)
	FTransform3f GetInstanceBoneTransformCS(int32 InstanceIndex, int32 SkeletonBoneIndex) const;
	//returns the cached bone transform in world space
	FTransform GetInstanceBoneTransformWS(int32 InstanceIndex, int32 SkeletonBoneIndex) const;
	//return the transform of a socket/bone on success, identity otherwise.
	//@SocketName Bone/Socket Name on Skeleton Or SkeletalMesh
	//@InMesh	  SkeletalMesh to get the socket data from. set null to take from default mesh or skeleton.
	FTransform GetInstanceSocketTransform(int32 InstanceIndex, FName SocketOrBoneName, USkeletalMesh* InMesh = nullptr, bool bWorldSpace = true) const;

	static USkeletalMeshSocket* FindSkeletalSocket(const FSkelotInstanceRenderDesc& Desc, FName SocketName, const USkeletalMesh* InMesh = nullptr);
	
	//set custom data float for the specified instance, see FSkelotInstanceRenderDesc.NumCustomDataFloat 
	void SetInstanceCustomDataFloat(int32 InstanceIndex, float Float, int32 Offset) { SetInstanceCustomDataFloat(InstanceIndex, &Float, Offset, 1); }
	//
	void SetInstanceCustomDataFloat(int32 InstanceIndex, const float* Floats, int32 Offset, int32 Count);
	//
	float* GetInstanceCustomDataFloats(int32 InstanceIndex)
	{
		//intentionally not [] operator because might be: PerInstanceCustomData.Num() == 0 && MaxNumCustomDataFloat == 0
		return this->SOA.PerInstanceCustomData.GetData() + (InstanceIndex * SOA.MaxNumCustomDataFloat);
	}

	//destroy all the attached Skelot Instances
	void DestroyChildren(int32 InstanceIndex);

	//
	template<typename T> T& GetInstanceUserDataAs(int32 InstanceIndex) 
	{
		static_assert(sizeof(T) <= sizeof(FSkelotInstancesSOA::FUserData));
		return *reinterpret_cast<T*>(&SOA.UserData[InstanceIndex]); 
	}
	//
	template<typename T> const T& GetInstanceUserDataAs(int32 InstanceIndex) const 
	{
		static_assert(sizeof(T) <= sizeof(FSkelotInstancesSOA::FUserData));
		return *reinterpret_cast<T*>(&SOA.UserData[InstanceIndex]); 
	}


	//
	uint32 GetInstanceVersion(int32 InstanceIndex) const { return SOA.Slots[InstanceIndex].Version; }
	//converts instance index to handle
	FSkelotInstanceHandle IndexToHandle(int32 InstanceIndex) const { return FSkelotInstanceHandle { InstanceIndex, SOA.Slots[InstanceIndex].Version }; }
	//
	FSkelotInstanceHandle IndexToHandle_Safe(int32 InstanceIndex) const 
	{
		if(SOA.Slots.IsValidIndex(InstanceIndex) && !SOA.Slots[InstanceIndex].bDestroyed)
			return FSkelotInstanceHandle{ InstanceIndex, SOA.Slots[InstanceIndex].Version }; 

		return FSkelotInstanceHandle();
	}
	//
	int32 HandleToIndex(FSkelotInstanceHandle H) const { return H.InstanceIndex; }


	void Internal_CallOnAnimationFinished();
	void Internal_CallOnAnimationNotify();

	//
	virtual void OnAnimationFinished(const TArray<FSkelotAnimFinishEvent>& Events) {}
	virtual void OnAnimationNotify(const TArray<FSkelotAnimNotifyEvent>& Events) {}

	
	void QueryLocationOverlappingSphere(const FVector& Center, float Radius, TArray<FSkelotInstanceHandle>& OutInstances);
	void RemoveInvalidHandles(bool bMaintainOrder, TArray<FSkelotInstanceHandle>& InOutHandles);
	//function to get handles of all the valid instances
	void GetAllHandles(TArray<FSkelotInstanceHandle>& OutHandles);


	//similar to void* UserData , but this one is UObject* and get garbage collected
	TObjectPtr<UObject> GetInstanceUserObject(int32 InstanceIndex);
	void SetInstanceUserObject(int32 InstanceIndex, TObjectPtr<UObject> InObject);



	//will fail if dynamic pose buffer is full. see USkelotAnimCollection.MaxDynamicPose 
	bool EnableInstanceDynamicPose(int32 InstanceIndex, bool bEnable);
	//
	bool IsInstanceDynamicPose(int32 InstanceIndex) const { return SOA.Slots[InstanceIndex].bDynamicPose; }
	//make an Skelot instance to take its bones from the specified USkeletalMeshComponent. usually used for ragdolls or advanced animations that need AnimBP
	//Note: dynamic pose must be enabled by EnableInstanceDynamicPose first .
	bool TieDynamicPoseToComponent(int32 InstanceIndex /* dynamic-pose-enabled instance*/, USkeletalMeshComponent* SrcComponent /* component to take bones from*/, int32 UserData, bool bCopyTransform);
	//
	USkeletalMeshComponent* UntieDynamicPoseFromComponent(int32 InstanceIndex, int32* OutUserData = nullptr);
	//
	FSkelotFrag_DynPoseTie* GetDynamicPoseTieData(int32 InstanceIndex);
	//
	//FMatrix3x4* InstanceRequestDynamicPoseUpload(int32 InstanceIndex);
	



	struct FLineTraceParams
	{
		FVector Start;	//start of the ray in world space
		FVector End; //end of the ray in world space
		float Thickness = 0; //0 is line trace, > 0 sphere trace
		TConstArrayView<int32> BonesToExclude;	//reference skeleton bones indices to be ignored, if any
		bool bCheckBoundingBoxFirst = true;
		bool bReturnFirstHit = false; //whether to just return first hit or the closest hit
	};

	struct FLineTraceResult
	{
		double Distance;	//the distance from the Start to the hit location
		FVector Position;	//position of impact
		FVector Normal;	//normal at impact point
		int32 BoneIndex = -1;	//reference skeleton bone that was hit, -1 if not hit
	};
	/*
	line traces the specified instance using cached bone transforms and FSkelotCompactPhysicsAsset of the meshes. (check USkelotAnimCollection.bCachePhysicsAssetBones)
	in order to have a real world line trace:
		perform a UWorld->LineTrace*** to get the obstacles/static-objects
		perform line trace over the Skelot instances that are likely to be hit . (Skelot does not have any spatial-partition/broad-phase utilize your movement system + some heuristic value like max bounding box)
		then check which one is the closest hit. (FHitResult.Distance or FLineTraceParams.OutTime)
	*/
	void LineTraceInstance(int32 InstanceIndex, const FLineTraceParams& InParams, FLineTraceResult& OutResult) const;
	
	//returns the handle of hit instance. 
	FSkelotInstanceHandle LineTraceInstances(TConstArrayView<int32> Instances, const FLineTraceParams& InParams, FLineTraceResult& OutResult, float DebugDrawTime = -1) const;

	//draw the compact phys asset of the specified instance 
	void DebugDrawCompactPhysicsAsset(int32 InstanceIndex, FColor Color);

	//debug draw AABB of the specified instance
	void DebugDrawInstanceBound(int32 InstanceIndex, float BoundExtentOffset, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f);

	//helper functions to set/clear lifespan for an Skelot instance, similar to AActor.SetLifeSpan
	void SetInstancelifespan(FSkelotInstanceHandle H, float Lifespan);
	void ClearInstancelifespan(FSkelotInstanceHandle H);
	//internal
	void TickLifeSpans();


	//simple per instance timer, payloads are passed to the delegate
	//@bGameTime	whether to use game time or real time
	void SetInstanceTimer(FSkelotInstanceHandle H, float Interval, bool bLoop, bool bGameTime, FSkelotGeneralDelegate Delegate, FName PayloadTag = FName(), UObject* PayloadObject = nullptr);
	void ClearInstanceTimer(FSkelotInstanceHandle H);
	FSkelotInstanceTimerData* Internal_SetInstanceTimer(FSkelotInstanceHandle H, float Interval, bool bLoop, bool bGameTime);
	void TickTimers();
	
	//returns local AABB of an instance
	FRenderBounds CalcLocalBound(int32 InstanceIndex) const;
	//returns world space AABB of an instance
	FRenderBounds CalcInstanceBoundWS(int32 InstanceIndex);

	//
	FTransform3f ConsumeRootMotion(int32 InstanceIdx)
	{
		FTransform3f R = SOA.RootMotions[InstanceIdx];
		SOA.RootMotions[InstanceIdx] = FTransform3f::Identity;
		return R;
	}

	void Tick(float DeltaSeconds) override;
	void BeginDestroy() override;
	void BeginPlay() override;
	void EndPlay(EEndPlayReason::Type Reason) override;
	void Destroyed() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	void PreSendEndOfFrame();
	void OnWorldTickStart(ELevelTick TickType, float DeltaSeconds);
	void OnWorldTickEnd(ELevelTick TickType, float DeltaSeconds);
	void OnWorldPreActorTick(ELevelTick TickType, float DeltaSeconds);
	void OnWorldPostActorTick(ELevelTick TickType, float DeltaSeconds);



};




