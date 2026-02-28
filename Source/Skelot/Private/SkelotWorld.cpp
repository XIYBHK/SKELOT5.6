// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotWorld.h"
#include "SkelotClusterComponent.h"
#include "SkelotSubsystem.h"
#include "SkelotPrivate.h"
#include "SkelotAnimCollection.h"
#include "SkelotPrivateUtils.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Misc/MemStack.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Chaos/Sphere.h"
#include "Chaos/Box.h"
#include "Chaos/Capsule.h"
#include "Engine/SkinnedAssetCommon.h"
#include "ContentStreaming.h"
#include "SkelotObjectVersion.h"
#include "UObject/UObjectIterator.h"
#include "UnrealEngine.h"
#include "SceneInterface.h"
#include "Engine/World.h"
#include "RHICommandList.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "SkelotAnimNotify.h"
#include "SkelotSettings.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Skelot.h"
#include "GameFramework/PlayerController.h"
#include "InstanceDataSceneProxy.h"
#include "SkelotRenderResources.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"

#if WITH_EDITOR
#include "IContentBrowserSingleton.h"
#include "DerivedDataCacheInterface.h"
#endif

#define private public 
#include "ScenePrivate.h"
#include "KismetTraceUtils.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimSequenceBase.h"

#include "SkelotPrimitiveProxy.h"

class FSkelotClusterProxy;

double GSkelot_InvClusterCellSize = 1;




inline FName GetDbgFName(const UObject* Ptr) { return Ptr ? Ptr->GetFName() : FName(TEXT("Null")); }

//debug console vars
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
bool GSkelot_DebugDisableDynData = false;
FAutoConsoleVariableRef CV_DebugDisableDynData(TEXT("skelot.DebugDisableDynData"), GSkelot_DebugDisableDynData, TEXT(""), ECVF_Default);

bool GSkelot_DrawPhyAsset = false;
FAutoConsoleVariableRef CV_DrawPhyAsset(TEXT("skelot.DrawPhyAssets"), GSkelot_DrawPhyAsset, TEXT(""), ECVF_Default);

#else
constexpr bool GSkelot_DebugDisableDynData = false;
constexpr bool GSkelot_DrawPhyAsset = true;


#endif







class ASkelotWorld_Impl : public ASkelotWorld
{
public:
	/*
	*/
	template<typename ArrayT> static void GetNullTerminatedMeshDefs(const FSkelotInstanceRenderDesc& Desc, ArrayT& MeshDefs)
	{
		for (const FSkelotMeshRenderDesc& MeshDesc : Desc.Meshes)
		{
			int32 MeshDefIndex = Desc.AnimCollection->FindMeshDef(MeshDesc.Mesh);
			if (MeshDefIndex != -1)
				MeshDefs.Add(&Desc.AnimCollection->Meshes[MeshDefIndex]);
		}
		MeshDefs.Add(nullptr);
	}
	
	//
	void DebugDrawClusterBound(FIntPoint Coord, FColor Color)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		DrawDebugBox(GetWorld(), GetTileCoordCenter(Coord), FVector(GSkelot_ClusterCellSize * 0.5 - 50) * FVector(1, 1, 0.05f), Color, false, 0.5f, 0, 40);
#endif
	}
	//
	void DebugDrawInstanceBound(int32 InstanceIndex, float BoundExtentOffset, FColor const& Color, bool bPersistentLines, float LifeTime = -1, uint8 DepthPriority = 0, float Thickness = 0)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		{
			FRenderBounds WorldBound = CalcInstanceBoundWS(InstanceIndex);
			DrawDebugBox(GetWorld(), FVector(WorldBound.GetCenter()), FVector(WorldBound.GetExtent()) + BoundExtentOffset, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		}
#endif
	}
	/*
	*/
	FSetElementId FindOrAddDesc(const FSkelotInstanceRenderDesc& InDesc)
	{
		auto Hash = GetTypeHash(InDesc);
		FSetElementId Id = RenderDescs.FindIdByHash(Hash, InDesc);
		if (!Id.IsValidId())
		{
			//#TODO FSkelotInstanceRenderDescFinal(InDesc) failed to compile with GCC-Android 
			FSkelotInstanceRenderDescFinal StackDesc;
			static_cast<FSkelotInstanceRenderDesc&>(StackDesc) = InDesc;
			Id = RenderDescs.AddByHash(Hash, StackDesc, nullptr);
			FSkelotInstanceRenderDescFinal& NewDesc = RenderDescs.Get(Id);
			NewDesc.CacheMeshDefIndices();


			check(NewDesc.NumCustomDataFloat < 64);
			ResizeCustomDataArray(NewDesc.NumCustomDataFloat);
		}

		return Id;
	}

	//
	void ResizeCustomDataArray(int32 NewNumCustomDataFloat)
	{
		if (NewNumCustomDataFloat > SOA.MaxNumCustomDataFloat)
		{
			int32 OldNumCustomDataFloat = SOA.MaxNumCustomDataFloat;
			SOA.MaxNumCustomDataFloat = NewNumCustomDataFloat;
			ResizeScatteredArray(SOA.PerInstanceCustomData, SOA.Slots.Num(), OldNumCustomDataFloat, NewNumCustomDataFloat);
		}
	}

	template<typename T> void ResizeScatteredArray(TArray<T>& Array, int32 Num, int32 OldNumDataPerInstance, int32 NewNumDataPerInstance)
	{
		check(OldNumDataPerInstance < NewNumDataPerInstance);
		auto OldData = MoveTemp(Array);
		Array.SetNumZeroed(Num * NewNumDataPerInstance);

		for (int32 Index = 0; Index < Num; Index++)
		{
			for (int32 DataIndex = 0; DataIndex < OldNumDataPerInstance; DataIndex++)
			{
				Array[Index * NewNumDataPerInstance + DataIndex] = MoveTemp(OldData[Index * OldNumDataPerInstance + DataIndex]);
			}
		}
	}
	//
	void IncreaseSOAs()
	{
		const uint32 NewSize = FMath::Max(256, SOA.Slots.Num() << 1);
		const uint32 GrowSize = NewSize - SOA.Slots.Num();
		InstanceIndexMask = NewSize - 1;

		FRandomStream Rnd(666);
		int32 BaseIdx = SOA.Slots.AddDefaulted(GrowSize);
		for (uint32 i = 0; i < GrowSize; i++)
		{
			uint32 Version;
			do
			{
				Version = Rnd.GetUnsignedInt();
			} while (Version == 0);

			SOA.Slots[BaseIdx + i].bDestroyed = true;
			SOA.Slots[BaseIdx + i].Version = Version;
		}

		SOA.Locations.AddZeroed(GrowSize);
		SOA.Rotations.AddZeroed(GrowSize);
		SOA.Scales.AddZeroed(GrowSize);

		SOA.PrevLocations.AddZeroed(GrowSize);
		SOA.PrevRotations.AddZeroed(GrowSize);
		SOA.PrevScales.AddZeroed(GrowSize);

		//velocity data for PBD collision and RVO avoidance
		SOA.Velocities.AddZeroed(GrowSize);

		//collision channel data (1 byte each): channel (0-7) and mask (bit flags)
		SOA.CollisionChannels.AddZeroed(GrowSize);
		SOA.CollisionMasks.AddZeroed(GrowSize);

		SOA.ClusterData.AddDefaulted(GrowSize);
		SOA.SubmeshIndices.AddZeroed(GrowSize * (MaxSubmeshPerInstance + 1));
		SOA.UserData.AddZeroed(GrowSize);
		SOA.AnimDatas.AddDefaulted(GrowSize);
		SOA.CurAnimFrames.AddZeroed(GrowSize);
		SOA.PreAnimFrames.AddZeroed(GrowSize);

		if (SOA.MaxNumCustomDataFloat > 0)
			SOA.PerInstanceCustomData.AddZeroed(GrowSize * SOA.MaxNumCustomDataFloat);

		SOA.MiscData.AddDefaulted(GrowSize);

		if(SOA.UserObjects.Num() != 0)
			SOA.UserObjects.AddZeroed(GrowSize);

		SOA.RootMotions.AddZeroed(GrowSize);
	}
	//
	void ReattachToDesc(int32 InstanceIdx, const FSkelotInstanceRenderDesc& NewDescOnStack)
	{
		ReattachToDesc(InstanceIdx, FindOrAddDesc(NewDescOnStack));
	}
	//
	void ReattachToDesc(int32 InstanceIdx, FSetElementId NewDescId)
	{
		FSkelotInstancesSOA::FClusterData& CD = SOA.ClusterData[InstanceIdx];
		const FSetElementId CurDescId = FSetElementId::FromInteger(CD.DescIdx);
		if (CurDescId != NewDescId)
		{
			//lets remove invalid sub mesh indices ----------
			{
				uint8* SMIIter = GetInstanceSubmeshIndices(InstanceIdx);
				uint8* SMIIterWrite = SMIIter;

				const FSkelotInstanceRenderDescFinal& NewRD = RenderDescs.Get(NewDescId);
				for (; *SMIIter != 0xFF; SMIIter++)
				{
					if (NewRD.Meshes.IsValidIndex(*SMIIter))
					{
						*SMIIterWrite = *SMIIter;
						SMIIterWrite++;
					}
				}
				
				*SMIIterWrite = 0xff;
			}

			//if attached to any cluster remove it right now and let end of frame assign cluster again
			if (CD.ClusterIdx != -1)
			{
				FSkelotCluster& CurClusterRef = this->RenderDescs.Get(CurDescId).Clusters.Get(FSetElementId::FromInteger(CD.ClusterIdx));
				RemoveInstanceFromCluster(CurClusterRef, CD.RenderIdx);
				CD.DescIdx = NewDescId.AsInteger();
				CD.ClusterIdx = -1;
				CD.RenderIdx = -1;
				check(!InstancesNeedCluster.Contains(InstanceIdx));
				InstancesNeedCluster.Add(InstanceIdx);
			}
			else
			{
				check(InstancesNeedCluster.Contains(InstanceIdx));
				CD.DescIdx = NewDescId.AsInteger();
			}

			
		}
	}
	//
	void UpdateAnimations(float DeltaSeconds)
	{
		SKELOT_SCOPE_CYCLE_COUNTER(AnimationsTick);

		DeltaSeconds *= AnimationPlayRate;

		if (DeltaSeconds <= 0)
			return;

		for (int32 InstanceIndex = 0; InstanceIndex < HandleAllocator.GetMaxSize(); InstanceIndex++)
		{
			UpdateAnimation(InstanceIndex, DeltaSeconds);
		}

	}

	void UpdateAnimation(int32 InstanceIndex, float Delta)
	{
		FSkelotInstancesSOA::FSlotData& Slot = SOA.Slots[InstanceIndex];
		FSkelotInstancesSOA::FAnimData& AnimData = SOA.AnimDatas[InstanceIndex];
		
		if (Slot.bDestroyed | Slot.bAnimationPaused | Slot.bNoSequence | Slot.bCreatedThisFrame | Slot.bDynamicPose)
			return;

		if (GSkelot_DebugAnimations)
		{
			FString Str = FString::Printf(TEXT("Time:%f, FrameIndex:%d"), AnimData.AnimationTime, SOA.CurAnimFrames[InstanceIndex]);
			DrawDebugString(GetWorld(), GetInstanceLocation(InstanceIndex), Str, nullptr, FColor::Green, 0, false, 2);
		}

		const FSkelotInstanceRenderDescFinal& RenderDesc = GetInstanceDesc(InstanceIndex);
		if (!RenderDesc.AnimCollection)
			return;

		USkelotAnimCollection* AnimCollection = RenderDesc.AnimCollection;

		float OldTime = AnimData.AnimationTime;
		float NewDelta = AnimData.AnimationPlayRate * Delta;
		float AssetLength = AnimData.CurrentAsset->GetPlayLength();
		float AnimPos;
		ETypeAdvanceAnim result = SkelotAnimAdvanceTime(Slot.bAnimationLooped, NewDelta, AnimData.AnimationTime, AssetLength);
		
		check(AnimData.IsSequenceValid() && AnimData.CurrentAsset);

		if (Slot.bExtractRootMotion) //maybe AnimData.CurrentAsset->HasRootMotion() ?
		{
			FAnimExtractContext Context(static_cast<double>(OldTime), true, FDeltaTimeRecord(NewDelta), Slot.bAnimationLooped);
			FTransform3f RMT = (FTransform3f)AnimData.CurrentAsset->ExtractRootMotion(Context);
			SOA.RootMotions[InstanceIndex] = RMT * SOA.RootMotions[InstanceIndex];
		}

		if (AnimData.CurrentAsset->GetClass() == UAnimComposite::StaticClass())
		{
			const UAnimComposite* CompositeAsset = static_cast<UAnimComposite*>(AnimData.CurrentAsset);
			//AssetLength = CompositeAsset->AnimationTrack.GetLength();
			const FAnimSegment* OldSegment = CompositeAsset->AnimationTrack.GetSegmentAtTime(OldTime);
			const FAnimSegment* NewSegment = CompositeAsset->AnimationTrack.GetSegmentAtTime(AnimData.AnimationTime);
			check(OldSegment && NewSegment);
			if (OldSegment != NewSegment)
			{
				uint16 NewSeqDefIndex = static_cast<uint16>(AnimCollection->FindSequenceDef(NewSegment->GetAnimReference()));
				check(NewSeqDefIndex != AnimData.AnimationCurrentSequence && NewSeqDefIndex != 0xFFff);
				AnimData.AnimationCurrentSequence = NewSeqDefIndex;

				//if segment changed then transition is not valid anymore
				if (AnimData.IsTransitionValid())
				{
					AnimCollection->DecTransitionRef(AnimData.AnimationTransitionIndex);
				}
			}

			AnimPos = NewSegment->ConvertTrackPosToAnimPos(AnimData.AnimationTime);

		}
		else
		{
			const UAnimSequence* SequenceAsset = reinterpret_cast<UAnimSequence*>(AnimData.CurrentAsset);
			AnimPos = AnimData.AnimationTime;
		}

		const FSkelotSequenceDef& ActiveSequenceStruct = AnimCollection->Sequences[AnimData.AnimationCurrentSequence];

		int32 LocalFrameIndex = FMath::TruncToInt32(AnimPos * ActiveSequenceStruct.SampleFrequency);
		check(LocalFrameIndex < ActiveSequenceStruct.AnimationFrameCount);
		int32 GlobalFrameIndex = ActiveSequenceStruct.AnimationFrameIndex + LocalFrameIndex;
		check(GlobalFrameIndex < AnimCollection->FrameCountSequences);

		if(ActiveSequenceStruct.Sequence->IsNotifyAvailable())
		{
			AnimationNotifyContext.ActiveNotifies.Reset();
			ActiveSequenceStruct.Sequence->GetAnimNotifies(OldTime, NewDelta, AnimationNotifyContext);
			for (const FAnimNotifyEventReference& Ev : AnimationNotifyContext.ActiveNotifies)
			{
				const FAnimNotifyEvent* NotifyEv = Ev.GetNotify(); 
				check(NotifyEv);
				const bool bHasChance = FMath::FRandRange(0.f, 1.f) < NotifyEv->NotifyTriggerChance;
				if(bHasChance)
				{
					if (NotifyEv->Notify)
					{
						if(bEnableAnimNotifyObjects)
						{
							if (ISkelotNotifyInterface* SkelotNotify = Cast<ISkelotNotifyInterface>(NotifyEv->Notify))
							{
								AnimationNotifyObjectEvents.Add(FSkelotAnimNotifyObjectEvent{ this->IndexToHandle(InstanceIndex), ActiveSequenceStruct.Sequence, SkelotNotify });
							}
						}
					}
					else
					{
						AnimationNotifyEvents.Add(FSkelotAnimNotifyEvent{ this->IndexToHandle(InstanceIndex), ActiveSequenceStruct.Sequence,  NotifyEv->NotifyName });
					}
				}
			}
		}
		


		if (AnimData.IsTransitionValid())
		{
			const USkelotAnimCollection::FTransition& Transition = AnimCollection->Transitions[AnimData.AnimationTransitionIndex];
			check((Transition.ToFI + Transition.FrameCount) <= ActiveSequenceStruct.AnimationFrameCount);
			if ((LocalFrameIndex >= (Transition.ToFI + Transition.FrameCount)) || result != ETAA_Default) //transition is over ?
			{
				AnimCollection->DecTransitionRef(AnimData.AnimationTransitionIndex);
			}
			else
			{
				int32 TransitionLFI = LocalFrameIndex - Transition.ToFI;
				check(TransitionLFI < Transition.FrameCount);
				SetAnimFrame(InstanceIndex, Transition.FrameIndex + TransitionLFI);
				return;
			}
		}

		SetAnimFrame(InstanceIndex, GlobalFrameIndex);



		if (result == ETAA_Finished)
		{
			check(LocalFrameIndex == ActiveSequenceStruct.AnimationFrameCount - 1);
			Slot.bNoSequence = true;
			AnimationFinishEvents.Add(FSkelotAnimFinishEvent{ FSkelotInstanceHandle { InstanceIndex, Slot.Version }, InstanceIndex, AnimData.CurrentAsset });
			AnimData.CurrentAsset = nullptr;

		}

		
	}
	void SetAnimFrame(int32 InstanceIndex, int32 FrameIndex)
	{	
		SOA.CurAnimFrames[InstanceIndex] = FrameIndex;
		if(SOA.Slots[InstanceIndex].bCreatedThisFrame)
			SOA.PreAnimFrames[InstanceIndex] = FrameIndex;
	}


	/*
	*/
	void CalculateBounds(float Delta)
	{
		SKELOT_SCOPE_CYCLE_COUNTER(CalculateBounds);

		for (int32 InstanceIndex = 0; InstanceIndex < HandleAllocator.GetMaxSize(); InstanceIndex++)
		{
			FSkelotInstancesSOA::FSlotData& Slot = SOA.Slots[InstanceIndex];
			FSkelotInstancesSOA::FAnimData& AnimData = SOA.AnimDatas[InstanceIndex];
		}
	}
	/*
	*/
	void UpdateDeterminant(float DeltaSeconds)
	{
		SKELOT_SCOPE_CYCLE_COUNTER(UpdateDeterminant);

		FMemMark Marker = FMemStack::Get();

		struct FPack
		{
			FSetElementId DescId;
			TStackIncrementalArray<int32, 64> InstanceIndices;
		};

		TArray<FPack, TInlineAllocator<32>> Packs;

		//for each render desc --------------------------------------------
		for (auto DescIter = RenderDescs.CreateIterator(); DescIter; ++DescIter)
		{
			if (!DescIter->bMayHaveNegativeDeterminant)
				continue;

			FPack* Changelist = nullptr;

			//for each cluster --------------------------------------------
			for (FSkelotCluster& ClusterIter : DescIter->Clusters)
			{
				//for each instance in the cluster ----------------------------
				for (int32 InstanceIndex : ClusterIter.Instances)
				{
					const FVector3f& Scale = SOA.Scales[InstanceIndex];
					const bool bInstanceDeterminantNegative = (Scale.X * Scale.Y * Scale.Z) < 0; //check if determinant is negative, see FTransform::GetDeterminant
					if (DescIter->bIsNegativeDeterminant != bInstanceDeterminantNegative) //determinant sign changed ?
					{
						if (!Changelist)
						{
							Changelist = &Packs.AddDefaulted_GetRef();
							Changelist->DescId = DescIter.GetId();
						}

						Changelist->InstanceIndices.Add(InstanceIndex);
					}
				}
			}
		}

		for (FPack& Pack : Packs)
		{
			const FSkelotInstanceRenderDescFinal& Desc = this->RenderDescs.Get(Pack.DescId);
			FSkelotInstanceRenderDesc DescFlipped = Desc;
			DescFlipped.bIsNegativeDeterminant = !Desc.bIsNegativeDeterminant;
			FSetElementId NewDescId = this->FindOrAddDesc(DescFlipped);

			Pack.InstanceIndices.ForEach([&](int32 InstanceIndex) {
				this->ReattachToDesc(InstanceIndex, NewDescId);
			});
		}
	}
	/*
	*/
	USKelotClusterComponent* CreateClusterComp(FSkelotInstanceRenderDescFinal& Desc, FSetElementId DescId) const
	{
		USKelotClusterComponent* Component = NewObject<USKelotClusterComponent>((UObject*)this, NAME_None, RF_NoFlags);  /*Desc.ComponentTemplate*/
		Component->DescId = DescId;
		//Ref.Component->SetWorldLocation(GetTileCoordCenter(InCoord));
		//fill properties from the descriptor now
		Component->MinDrawDistance = Desc.MinDrawDistance.GetValue();
		Component->SetCullDistance(Desc.MaxDrawDistance.GetValue());
		Component->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Always;
		Component->bRenderInMainPass = Desc.bRenderInMainPass;
		Component->bRenderInDepthPass = Desc.bRenderInDepthPass;
		Component->bReceivesDecals = Desc.bReceivesDecals;
		Component->bUseAsOccluder = Desc.bUseAsOccluder;
		Component->CastShadow = Desc.bCastDynamicShadow;
		Component->bCastDynamicShadow = Desc.bCastDynamicShadow;
		Component->bRenderCustomDepth = Desc.bRenderCustomDepth;
		Component->CustomDepthStencilWriteMask = Desc.CustomDepthStencilWriteMask;
		Component->CustomDepthStencilValue = Desc.CustomDepthStencilValue;
		Component->LightingChannels = Desc.LightingChannels;
		Component->BoundsScale = Desc.BoundsScale;

		Component->AddToRoot();	//cleanup manually, not exposed to GC

		return Component;
	}
	/*
	*/
	FSetElementId FindOrCreateCluster(FSkelotInstanceRenderDescFinal& Desc, FSetElementId DescId, FIntPoint InCoord)
	{
		FSetElementId Idx = Desc.Clusters.FindIdByHash(GetTypeHash(InCoord), InCoord);
		if (!Idx.IsValidId())
		{
			Idx = Desc.Clusters.AddByHash(GetTypeHash(InCoord), FSkelotCluster{ .Coord = InCoord }, nullptr);

			FSkelotCluster& Ref = Desc.Clusters.Get(Idx);
			Ref.Coord = InCoord;
			Ref.Component = CreateClusterComp(Desc, DescId);
			Ref.Component->ClusterId = Idx;

			if (GSkelot_DebugClusters)
				DebugDrawClusterBound(InCoord, FColor::Purple);
		}
		return Idx;
	}
	/*
	*/
	void BindInstanceToCluster(int32 InstanceIndex, const FIntPoint& TileCoord)
	{
		FSkelotInstancesSOA::FSlotData& Slot = SOA.Slots[InstanceIndex];
		FSkelotInstancesSOA::FClusterData& CD = SOA.ClusterData[InstanceIndex];
		check(!Slot.bDestroyed && CD.ClusterIdx == -1 && CD.RenderIdx == -1 && CD.DescIdx != -1);

		FSetElementId DescId = FSetElementId::FromInteger(CD.DescIdx);
		FSkelotInstanceRenderDescFinal& Desc = this->RenderDescs.Get(DescId);

		FSetElementId ClusterIdx = FindOrCreateCluster(Desc, DescId, TileCoord);
		FSkelotCluster& ClusterRef = Desc.Clusters.Get(ClusterIdx);

		CD.ClusterIdx = ClusterIdx.AsInteger();
		
		//add instance to cluster
		checkSlow(!ClusterRef.Instances.Contains(InstanceIndex));
		CD.RenderIdx = ClusterRef.Instances.Add(InstanceIndex);
		ClusterRef.bAnyAddRemove = true;

		if (!ClusterRef.Component->IsRegistered())
		{
			ClusterRef.Component->RegisterComponent();
			ClusterRef.Component->MarkRenderDynamicDataDirty();
		}
	}
	/*
	*/
	void RemoveInstanceFromCluster(FSkelotCluster& ClusterRef, int32 RenderIndex)
	{
		check(RenderIndex != -1);
		ClusterRef.bAnyAddRemove = true;
		int32 LastInstanceIndex = ClusterRef.Instances.Last();	//instance index of the last element in the cluster
		this->SOA.ClusterData[LastInstanceIndex].RenderIdx = RenderIndex;
		ClusterRef.Instances[RenderIndex] = LastInstanceIndex;
		ClusterRef.Instances.Pop(EAllowShrinking::No);
	}
	/*
	*/
	void UpdateClusters(FSkelotInstanceRenderDescFinal& Desc, FSetElementId DescId)
	{
		struct FPointAndIndex { FIntPoint Coord; int32 InstanceIndex; };

		TStackIncrementalArrayWithMark<FPointAndIndex, 128> Changelist;

		//for each cluster in render desc ----------------------------------
		for (auto ClusterIter = Desc.Clusters.CreateIterator(); ClusterIter; ++ClusterIter)
		{
			FSkelotCluster& CurCluster = *ClusterIter;
			//for each instance in the cluster --------------------------------------------
			for (int32 RenderIdx = 0; RenderIdx < CurCluster.Instances.Num();)
			{
				int32 InstanceIndex = CurCluster.Instances[RenderIdx];

				FSkelotInstancesSOA::FClusterData& CD = SOA.ClusterData[InstanceIndex];
				check(CD.DescIdx == DescId.AsInteger());
				check(CD.ClusterIdx != -1 && CD.RenderIdx != -1 && CD.RenderIdx == RenderIdx);
				FIntPoint NewTileCoord = LocationToTileCoord(SOA.Locations[InstanceIndex]);

				if (NewTileCoord != CurCluster.Coord) //tile coordinate has changed ?
				{
					RemoveInstanceFromCluster(CurCluster, RenderIdx);
					CD.ClusterIdx = -1;
					CD.RenderIdx = -1;
					Changelist.Add(FPointAndIndex{ NewTileCoord, InstanceIndex });
				}
				else
				{
					RenderIdx++;
				}
			}
		}

		Changelist.ForEach([&](FPointAndIndex Data) {

			BindInstanceToCluster(Data.InstanceIndex, Data.Coord);
			if (GSkelot_DebugClusters)
				DebugDrawInstanceBound(Data.InstanceIndex, FMath::FRandRange(-10.0f, 10.0f), FColor::Magenta, false, 0.3f);
		});
	}


	/*
	*/
	void UpdateClusters()
	{
		if (ClusterMode == ESkelotClusterMode::None)
		{
			// loop over those instances that need to be assigned to a cluster ------------------------------------------------------
			for (int32 InstanceIndex : InstancesNeedCluster)
			{
				BindInstanceToCluster(InstanceIndex, FIntPoint(-1, -1));
			}
			InstancesNeedCluster.Reset();
		}
		else
		{
			// loop over those instances that need to be assigned to a cluster ------------------------------------------------------
			for (int32 InstanceIndex : InstancesNeedCluster)
			{
				FIntPoint TileCoord = LocationToTileCoord(this->SOA.Locations[InstanceIndex]);
				BindInstanceToCluster(InstanceIndex, TileCoord);
			}
			InstancesNeedCluster.Reset();


			if (++ClusterUpdateCounter == 32)
			{
				ClusterUpdateCounter = 0;

				for (auto Iter = this->RenderDescs.CreateIterator(); Iter; ++Iter)
				{
					UpdateClusters(*Iter, Iter.GetId());
				}
			}
		}
	}
	/*
	*/
	void UpdateFlush(float DeltaSeconds)
	{
		SKELOT_SCOPE_CYCLE_COUNTER(UpdateFlush);

		const USkelotDeveloperSettings* SkelotSettings = GetDefault<USkelotDeveloperSettings>();

#if 0
       //rect that define tiles who are close to main view
		FIntRect HighQualityTiles = FIntRect(FIntPoint(INT32_MIN), FIntPoint(INT32_MAX));

		if (APlayerController* PC = this->GetWorld()->GetFirstPlayerController())
		{
			FVector ViewLoc = FVector::ZeroVector;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			const FIntPoint ViewTileLoc = LocationToTileCoord(ViewLoc);

			HighQualityTiles = FIntRect(LocationToTileCoord(ViewLoc - SkelotSettings->HighQualityRange) - 1, LocationToTileCoord(ViewLoc + SkelotSettings->HighQualityRange) + 1);
			if (GSkelot_DebugClusters)
				DrawDebugSphere(GetWorld(), ViewLoc, SkelotSettings->HighQualityRange, 128, FColor::Green);
		}
#endif // 
		
		
		int32 AvrageInstanceCount = 0;
		int32 NumValidComp = 0;
		int32 TotalCluster = 0;

		
		//for each render desc --------------------------------------------
		for (auto DescIter = this->RenderDescs.CreateIterator(); DescIter; ++DescIter)
		{
			FSkelotInstanceRenderDescFinal& DescRef = *DescIter;
			ClusterSetT& ClusterSet = DescRef.Clusters;
			TotalCluster += ClusterSet.Num();

			//for each cluster --------------------------------------------
			for (auto ClusterIter = ClusterSet.CreateIterator(); ClusterIter; ++ClusterIter)
			{
				FSkelotCluster& ClusterRef = *ClusterIter;
				if (!GSkelot_DebugDisableDynData)
				{
					ClusterRef.Component->MarkRenderDynamicDataDirty();
				}

				if (ClusterRef.Instances.Num() != 0)
				{
					AvrageInstanceCount += ClusterRef.Instances.Num();
					NumValidComp++;
				}
				else //cluster has no instance ?
				{
					if (ClusterRef.KillCounter++ > SkelotSettings->ClusterLifeTime) //free the cluster element and its component if its old enough 
					{
						if (GSkelot_DebugClusters)
							Impl()->DebugDrawClusterBound(ClusterRef.Coord, FColor::Red);

						ClusterRef.KillCounter = 0;
						ClusterRef.Component->RemoveFromRoot();
						ClusterRef.Component->DestroyComponent();
						ClusterRef.Component = nullptr;
						ClusterIter.RemoveCurrent();

						if (ClusterSet.Num() == 0)
						{
							DescRef.KillCounter = 0;
						}
					}
				}
			}

			//descriptors with no cluster must be remove from the TSet to let GC free the referenced assets
			if (ClusterSet.Num() == 0)
			{
				if (DescRef.KillCounter++ > SkelotSettings->RenderDescLifetime)
				{
					DescRef.KillCounter = 0;
					DescIter.RemoveCurrent();
				}
			}
		}

		if (NumValidComp != 0)
			AvrageInstanceCount /= NumValidComp;

		SET_DWORD_STAT(STAT_SKELOT_AvgInsCountPerCluster, AvrageInstanceCount);
		SET_DWORD_STAT(STAT_SKELOT_TotalInstances, HandleAllocator.GetSparselyAllocatedSize());
		SET_DWORD_STAT(STAT_SKELOT_TotalRenderDescs, this->RenderDescs.Num());
		SET_DWORD_STAT(STAT_SKELOT_TotalClusters, TotalCluster);
	}
	//
	static FIntPoint LocationToTileCoord(const FVector L)
	{
		const double InvTileSize = GSkelot_InvClusterCellSize;
		return FIntPoint(FMath::FloorToInt32(static_cast<float>(L.X * InvTileSize)), FMath::FloorToInt32(static_cast<float>(L.Y * InvTileSize)));
	}
	//
	static FVector GetTileCoordCenter(const FIntPoint Coord)
	{
		const double TileSize = GSkelot_ClusterCellSize;
		return FVector((Coord.X + 0.5) * TileSize, (Coord.Y + 0.5) * TileSize, 0);
	}
	//
	void LowLevelDestroyInstance(int32 InstanceIndex)
	{
		FSkelotInstancesSOA::FSlotData& Slot = SOA.Slots[InstanceIndex];

		ResetAnimationState(InstanceIndex);

		//remove attachment data if any
		int32& AttachmentIdx = SOA.MiscData[InstanceIndex].AttachmentIndex;
		if (AttachmentIdx != -1)
		{
			AttachParentArray.RemoveAt(AttachmentIdx);
			AttachmentIdx = -1;
		}
		

		HandleAllocator.Free(InstanceIndex);
		Slot.IncVersion();
		Slot.bDestroyed = true;
		

		DestructItem(&SOA.AnimDatas[InstanceIndex]);

		if(SOA.UserObjects.Num() != 0)
			SOA.UserObjects[InstanceIndex] = nullptr;

		FSkelotInstancesSOA::FClusterData& CD = SOA.ClusterData[InstanceIndex];
		check(CD.DescIdx != -1);

		if (CD.ClusterIdx == -1) //happens if we add and remove at same frame ! rare !
		{
			check(CD.RenderIdx == -1);
			InstancesNeedCluster.RemoveAtSwap(InstancesNeedCluster.IndexOfByKey(InstanceIndex), EAllowShrinking::No);
		}
		else
		{
			FSkelotCluster& ClusterRef = RenderDescs.Get(FSetElementId::FromInteger(CD.DescIdx)).Clusters.Get(FSetElementId::FromInteger(CD.ClusterIdx));
			RemoveInstanceFromCluster(ClusterRef, CD.RenderIdx);
			CD.ClusterIdx = -1;
			CD.RenderIdx = -1;
		}

		CD.DescIdx = -1;
	}
	



#pragma region hierarchy and attachments

	//
	void UpdateHierarchyTransforms(float DeltaSeconds)
	{
		SKELOT_SCOPE_CYCLE_COUNTER(UpdateHierarchyTransforms);

		//socket transform of those who are playing transition must be ready by now
		for (auto DescIter = RenderDescs.CreateIterator(); DescIter; ++DescIter)
		{
			if (DescIter->AnimCollection)
			{
				DescIter->AnimCollection->FlushDeferredTransitions();
			}
		}

		for (FSkelotAttachParentData& AtchData : AttachParentArray)
		{
			if (AtchData.Parent == -1) //no parent means its root
			{
				UpdateChildTransforms(AtchData);
			}
		}
	}

#pragma endregion




	static void GetSocketMinimalInfo(const FSkelotInstanceRenderDesc& Desc, FName InSocketOrBoneName, USkeletalMesh* InMesh, int32 OutBoneIndex, FTransform& OutSocketTransform)
	{
		OutBoneIndex = -1;
		OutSocketTransform = FTransform::Identity;
		check(Desc.AnimCollection);
		const USkeletalMeshSocket* Socket = FindSkeletalSocket(Desc, InSocketOrBoneName, InMesh);
		if (Socket)
		{
			OutBoneIndex = Desc.AnimCollection->FindSkeletonBoneIndex(Socket->BoneName);
			if (OutBoneIndex != -1)
				OutSocketTransform = Socket->GetSocketLocalTransform();
		}
		else
		{
			OutBoneIndex = Desc.AnimCollection->FindSkeletonBoneIndex(InSocketOrBoneName);
		}
	}



	void ReleaseDynamicPose(int32 InstanceIndex)
	{
		check(SOA.Slots[InstanceIndex].bDynamicPose);

		SOA.Slots[InstanceIndex].bDynamicPose = false;
		SOA.Slots[InstanceIndex].bNoSequence = true;
		
		const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);

		int32& FrameIndex = SOA.CurAnimFrames[InstanceIndex];
		check(Desc.AnimCollection->IsDynamicPoseFrameIndex(FrameIndex));
		int32 DynamicPoseIndex = Desc.AnimCollection->FrameIndexToDynamicPoseIndex(FrameIndex);

		DynamicPosTiedMap.Remove(InstanceIndex);

		Desc.AnimCollection->FlipDynamicPoseSign(DynamicPoseIndex);
		Desc.AnimCollection->FreeDynamicPose(DynamicPoseIndex);
		FrameIndex = 0;
	}
	void FillDynamicPoseFromComponents()
	{

		SKELOT_SCOPE_CYCLE_COUNTER(FillDynamicPoseFromComponents);

		
		for (TPair<int32, FSkelotFrag_DynPoseTie>& PairData : DynamicPosTiedMap)
		{
			const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(PairData.Key);
			PairData.Value.TmpUploadIndex = Desc.AnimCollection->ReserveUploadData(1);
			check(PairData.Value.TmpUploadIndex != -1);
			int32& FrameIndex = SOA.CurAnimFrames[PairData.Key];
			int32 DynamicPoseIndex = Desc.AnimCollection->FrameIndexToDynamicPoseIndex(FrameIndex);
			FrameIndex = Desc.AnimCollection->FlipDynamicPoseSign(DynamicPoseIndex);
		}
		
		ParallelFor(DynamicPosTiedMap.GetMaxIndex(), [this](int32 ElementIndex) {
			
			if (!DynamicPosTiedMap.IsValidId(FSetElementId::FromInteger(ElementIndex)))
				return;

			const TPair<int32, FSkelotFrag_DynPoseTie>& PD = this->DynamicPosTiedMap.Get(FSetElementId::FromInteger(ElementIndex));
			check(::IsValid(PD.Value.SKMeshComponent));
			Impl()->FillDynamicPoseFromComponent_Concurrent(PD);

		});
	}
	void FillDynamicPoseFromComponent_Concurrent(const TPair<int32, FSkelotFrag_DynPoseTie>& Data)
	{
		if(Data.Value.bCopyTransform)
			SetInstanceTransform(Data.Key, Data.Value.SKMeshComponent->GetComponentTransform());

		const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(Data.Key);
		
		TArray<FTransform, TInlineAllocator<255>> FullPose;
		FullPose.Init(FTransform::Identity, Desc.AnimCollection->AnimationBoneCount);

		const TArray<FTransform>& Transforms = Data.Value.SKMeshComponent->GetComponentSpaceTransforms();
		USkeleton* Skeleton = Desc.AnimCollection->Skeleton;
		const FSkeletonToMeshLinkup& LinkupCache = Skeleton->FindOrAddMeshLinkupData(Data.Value.SKMeshComponent->GetSkeletalMeshAsset());
		for (FBoneIndexType BoneIndex : Data.Value.SKMeshComponent->RequiredBones)
		{
			int32 SKBoneIndex = LinkupCache.MeshToSkeletonTable[BoneIndex];
			FullPose[SKBoneIndex] = Transforms[BoneIndex];
		}

		Desc.AnimCollection->CurrentUpload.ScatterData[Data.Value.TmpUploadIndex] = SOA.CurAnimFrames[Data.Key];

		FMatrix3x4* RenderMatrices = &Desc.AnimCollection->CurrentUpload.PoseData[Data.Value.TmpUploadIndex * Desc.AnimCollection->RenderBoneCount];
		Desc.AnimCollection->CalcRenderMatrices(FullPose, RenderMatrices);
	}

	void ConsumeRootMotions()
	{
		for (int32 InstanceIndex = 0; InstanceIndex < HandleAllocator.GetMaxSize(); InstanceIndex++)
		{
			if (SOA.Slots[InstanceIndex].bDestroyed)
				continue;

			if (SOA.Slots[InstanceIndex].bApplyRootMotion)
			{
				SetInstanceTransform(InstanceIndex, FTransform(SOA.RootMotions[InstanceIndex]) * GetInstanceTransform(InstanceIndex));
				SOA.RootMotions[InstanceIndex] = FTransform3f::Identity;
			}
		}
	}
};





















TArray<int32> ASkelotWorld::GetAllValidInstances() const
{
	TArray<int32> Indices;
	Indices.SetNumUninitialized(GetNumValidInstance());
	int32* DataPtr = Indices.GetData();
	for (int32 i = 0; i < GetNumInstance(); i++)
	{
		if (IsInstanceAlive(i))
		{
			*DataPtr = i;
			DataPtr++;
		}
	}
	return Indices;
}

void ASkelotWorld::Tick(float DeltaSeconds)
{
	SKELOT_SCOPE_CYCLE_COUNTER(ASkelotWorld_Tick);

	Super::Tick(DeltaSeconds);

	// 重建空间网格（用于高效的空间查询）
	RebuildSpatialGrid();

	// 执行PBD碰撞求解（用于实例间碰撞避让）
	SolvePBDCollisions(DeltaSeconds);

#if UE_ENABLE_DEBUG_DRAWING	//draw phys bounds of instances
	if(GSkelot_DrawPhyAsset)
	{
		for (int32 InstanceIndex = 0, NumDraw = 0; InstanceIndex < this->GetNumInstance() && NumDraw < 128; InstanceIndex++)
		{
			if (!IsInstanceAlive(InstanceIndex))
				continue;

			NumDraw++;
			this->DebugDrawCompactPhysicsAsset(InstanceIndex, FColor::MakeRandomSeededColor(InstanceIndex ^ SOA.Slots[InstanceIndex].Version));
		}
	}
#endif


}

void ASkelotWorld::BeginDestroy()
{
	Super::BeginDestroy();
}

void ASkelotWorld::BeginPlay()
{
	Super::BeginPlay();
}

void ASkelotWorld::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);


	for (int32 InstanceIndex = 0; InstanceIndex < GetNumInstance(); InstanceIndex++)
	{
		if(IsInstanceAlive(InstanceIndex))
			DestroyInstance(IndexToHandle(InstanceIndex));
	}

	//destroy components
	for (auto DescIter = this->RenderDescs.CreateIterator(); DescIter; ++DescIter)
	{
		for (auto ClusterIter = DescIter->Clusters.CreateIterator(); ClusterIter; ++ClusterIter)
		{
			if (ClusterIter->Component)
			{
				ClusterIter->Component->RemoveFromRoot();
				ClusterIter->Component->DestroyComponent();
				ClusterIter->Component = nullptr;
			}
		}

		DescIter->Clusters.Empty();
	}

	this->RenderDescs.Empty();
	this->InstancesNeedCluster.Empty();
}

void ASkelotWorld::Destroyed()
{
	Super::Destroyed();

}


void ASkelotWorld::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	ASkelotWorld* This = static_cast<ASkelotWorld*>(InThis);
}

FSkelotInstanceHandle ASkelotWorld::CreateInstance(const FTransform& Transform, const FSkelotInstanceRenderDesc& Desc)
{
	FSetElementId DescId = Impl()->FindOrAddDesc(Desc);
	return CreateInstance(Transform, DescId);
}

FSkelotInstanceHandle ASkelotWorld::CreateInstance(const FTransform& Transform, USkelotRenderParams* RenderParams)
{
	if(!RenderParams)
		return FSkelotInstanceHandle();

	return CreateInstance(Transform, RenderParams->Data);
}

FSkelotInstanceHandle ASkelotWorld::CreateInstance(const FTransform& Transform, FSetElementId DescId)
{
	int32 InstanceIdx = HandleAllocator.Allocate();
	if (InstanceIdx >= SOA.Slots.Num())
	{
		Impl()->IncreaseSOAs();
	}

	uint32 OldVersion = SOA.Slots[InstanceIdx].Version;
	//reset to default but keep version
	SOA.Slots[InstanceIdx] = FSkelotInstancesSOA::FSlotData();
	SOA.Slots[InstanceIdx].Version = OldVersion;

	SOA.ClusterData[InstanceIdx].ClusterIdx = -1;
	SOA.ClusterData[InstanceIdx].DescIdx = DescId.AsInteger();
	SOA.ClusterData[InstanceIdx].RenderIdx = -1;

	//lets attach default submeshes 
	uint8* SMIIter = GetInstanceSubmeshIndices(InstanceIdx);
	const uint8* SMIEnd = SMIIter + this->MaxSubmeshPerInstance;
	const FSkelotInstanceRenderDescFinal& CurDesc = RenderDescs.Get(DescId);
	checkf(CurDesc.Meshes.Num() < 255, TEXT("at most 254 sub mesh are supported."));
	for (int32 MeshIdx = 0; MeshIdx < CurDesc.Meshes.Num(); MeshIdx++)
	{
		if (CurDesc.Meshes[MeshIdx].bAttachByDefault)
		{
			*SMIIter = (uint8)MeshIdx;
			SMIIter++;
			if (SMIIter == SMIEnd)
				break;
		}
	}
	*SMIIter = 0xFF;

	SetInstanceTransform(InstanceIdx, Transform);
	SOA.PrevLocations[InstanceIdx] = SOA.Locations[InstanceIdx];
	SOA.PrevRotations[InstanceIdx] = SOA.Rotations[InstanceIdx];
	SOA.PrevScales[InstanceIdx]	   = SOA.Scales[InstanceIdx];

	//initialize velocity to zero
	SOA.Velocities[InstanceIdx] = FVector3f::ZeroVector;

	//initialize collision channel and mask to defaults
	//default: Channel0 (value 0), mask 0xFF (collide with all channels)
	SOA.CollisionChannels[InstanceIdx] = SkelotCollision::DefaultCollisionChannel;
	SOA.CollisionMasks[InstanceIdx] = SkelotCollision::DefaultCollisionMask;

	SOA.CurAnimFrames[InstanceIdx] = 0;
	SOA.PreAnimFrames[InstanceIdx] = 0;
	new (&SOA.AnimDatas[InstanceIdx])  FSkelotInstancesSOA::FAnimData();
	new (&SOA.MiscData[InstanceIdx]) FSkelotInstancesSOA::FMiscData();

	SOA.UserData[InstanceIdx].Pointer = nullptr;

	//fill custom data with zero
	float* CustomData = Impl()->GetInstanceCustomDataFloats(InstanceIdx);
	for (int32 i = 0; i < SOA.MaxNumCustomDataFloat; i++)
		CustomData[i] = 0;

	SOA.RootMotions[InstanceIdx] = FTransform3f::Identity;

	InstancesNeedCluster.Add(InstanceIdx);
	return FSkelotInstanceHandle{ InstanceIdx, SOA.Slots[InstanceIdx].Version };
}

void ASkelotWorld::CreateInstances(const TArray<FTransform>& Transforms, const FSkelotInstanceRenderDesc& Desc, TArray<FSkelotInstanceHandle>& OutHandles)
{
	FSetElementId DescId = Impl()->FindOrAddDesc(Desc);
	OutHandles.Reset();
	OutHandles.Reserve(Transforms.Num());
	for (const FTransform& Transform : Transforms)
	{
		OutHandles.Add(CreateInstance(Transform, DescId));
	}
}

void ASkelotWorld::CreateInstances(const TArray<FTransform>& Transforms, USkelotRenderParams* RenderParams, TArray<FSkelotInstanceHandle>& OutHandles)
{
	OutHandles.Reset();
	if (!RenderParams)
		return;

	CreateInstances(Transforms, RenderParams->Data, OutHandles);
}

void ASkelotWorld::DestroyInstance(FSkelotInstanceHandle H)
{
	if (!IsHandleValid(H))
		return;

	DestroyInstance(H.InstanceIndex);
}

void ASkelotWorld::DestroyInstance(int32 InstanceIndex)
{
	if (FSkelotAttachParentData* AttachFrag = GetInstanceAttachParentData(InstanceIndex)) //any attachment data ?
	{
		DetachInstanceFromParent(InstanceIndex);
		//no one is the parent now, we can release the hierarchy

		TArray<int32, TInlineAllocator<10>> InstancesToBeRemoved;
		InstancesToBeRemoved.Add(InstanceIndex);

		GetInstanceChildren<false>(InstanceIndex, InstancesToBeRemoved);

		for (int32 Idx : InstancesToBeRemoved)
		{
			Impl()->LowLevelDestroyInstance(Idx);
		}
	}
	else
	{
		Impl()->LowLevelDestroyInstance(InstanceIndex);
	}
}

//////////////////////////////////////////////////////////////////////////
// Velocity API

void ASkelotWorld::SetInstanceVelocity(FSkelotInstanceHandle H, const FVector3f& V)
{
	if (IsHandleValid(H))
	{
		SOA.Velocities[H.InstanceIndex] = V;
	}
}

void ASkelotWorld::SetInstanceVelocities(const TArray<int32>& InstanceIndices, const TArray<FVector3f>& Velocities)
{
	check(InstanceIndices.Num() == Velocities.Num());
	for (int32 i = 0; i < InstanceIndices.Num(); i++)
	{
		SOA.Velocities[InstanceIndices[i]] = Velocities[i];
	}
}

void ASkelotWorld::SetInstanceVelocities(const TArray<FSkelotInstanceHandle>& Handles, const TArray<FVector3f>& Velocities)
{
	check(Handles.Num() == Velocities.Num());
	for (int32 i = 0; i < Handles.Num(); i++)
	{
		if (IsHandleValid(Handles[i]))
		{
			SOA.Velocities[Handles[i].InstanceIndex] = Velocities[i];
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Collision Channel API

uint8 ASkelotWorld::GetInstanceCollisionChannel(FSkelotInstanceHandle H) const
{
	return IsHandleValid(H) ? SOA.CollisionChannels[H.InstanceIndex] : 0;
}

void ASkelotWorld::SetInstanceCollisionChannel(FSkelotInstanceHandle H, uint8 Channel)
{
	if (IsHandleValid(H))
	{
		SOA.CollisionChannels[H.InstanceIndex] = Channel;
	}
}

uint8 ASkelotWorld::GetInstanceCollisionMask(FSkelotInstanceHandle H) const
{
	return IsHandleValid(H) ? SOA.CollisionMasks[H.InstanceIndex] : 0;
}

void ASkelotWorld::SetInstanceCollisionMask(FSkelotInstanceHandle H, uint8 Mask)
{
	if (IsHandleValid(H))
	{
		SOA.CollisionMasks[H.InstanceIndex] = Mask;
	}
}

#if 0
void ASkelotWorld::SetInstanceMaterial(int32 InstanceIndex, const USkeletalMesh* Mesh, int32 MaterialIndex, FName MaterialSlot, const UMaterialInterface* Material)
{
	check(IsInstanceAlive(InstanceIndex));

	const FSkelotInstanceRenderDescFinal& CurDesc = Impl()->GetInstanceDesc(InstanceIndex);
	FSkelotInstanceRenderDesc TempDesc = CurDesc;

	//use first SKeletalMesh if nothing is explicitly defined
	if(Mesh == nullptr && CurDesc.Meshes.Num() == 1)
		Mesh = CurDesc.Meshes[0].Mesh;

	if (!Mesh)
	{
		//UE_LOGFMT(LogSkelot, Error, "Can't set material to {1}. Invalid SkeletalMesh!", GetDbgFName(CurDesc.Component));
		return;
	}

	int32 CurMeshIdx = CurDesc.IndexOfMesh(const_cast<USkeletalMesh*>(Mesh));


	if (CurMeshIdx == -1) //mesh not attached ?
	{
		//UE_LOGFMT(LogSkelot, Error, "Can't set material {0} to {1}. SkeletalMesh Not Attached !", GetDbgFName(CurDesc.Component), GetDbgFName(CurDesc.AnimCollection));
		return;
	}

	if (!MaterialSlot.IsNone()) //should we use SlotName ?
	{
		MaterialIndex = Mesh->GetMaterials().IndexOfByPredicate([&](const FSkeletalMaterial& MatInfo) { return MatInfo.MaterialSlotName == MaterialSlot; });
		if (MaterialIndex == -1)
		{
			//UE_LOGFMT(LogSkelot, Error, "Can't set material {0} to {1}. MaterialSlot {2} is not invalid !", GetDbgFName(Material), GetDbgFName(CurDesc.Component), MaterialSlot);
			return;
		}
	}
	
	if (!Mesh->GetMaterials().IsValidIndex(MaterialIndex)) //invalid material index ?
	{
		//UE_LOGFMT(LogSkelot, Error, "Can't set material {0} to {1}. MaterialIndex {2} is not valid !", GetDbgFName(Material), GetDbgFName(CurDesc.Component), MaterialIndex);
		return;
	}

	if(CurDesc.Meshes[CurMeshIdx].GetMaterial(MaterialIndex) == Material) //same material ?
		return;


	
	TempDesc.Meshes[CurMeshIdx].OverrideMaterials.SetNumZeroed(Mesh->GetMaterials().Num());
	TempDesc.Meshes[CurMeshIdx].OverrideMaterials[MaterialIndex] = const_cast<UMaterialInterface*>(Material);
	
	Impl()->ReattachToDesc(InstanceIndex, TempDesc);
}

void ASkelotWorld::InstanceAttachMesh(int32 InstanceIndex, const USkeletalMesh* Mesh, bool bAttach)
{
	InstanceAttachMeshes(InstanceIndex, MakeArrayView((USkeletalMesh**)&Mesh, 1), bAttach);
}

void ASkelotWorld::InstanceAttachMeshes(int32 InstanceIndex, TArrayView<USkeletalMesh*> Meshes, bool bAttach)
{
	if (Meshes.Num() == 0)
		return;

	const FSkelotInstanceRenderDescFinal& CurDesc = Impl()->GetInstanceDesc(InstanceIndex);
	FSkelotInstanceRenderDesc TempDesc = CurDesc;	//#Note TempDesc is FSkelotInstanceRenderDesc because FSkelotInstanceRenderDescFinal contains runtime data that must not be duplicated !

	for(USkeletalMesh* Mesh : Meshes)
	{
		if(!Mesh)
			continue;

		int32 CurMeshIdx = CurDesc.IndexOfMesh(Mesh);

		if (bAttach)
		{
			if (CurMeshIdx != -1)	//already attached ?
				continue;

			if (TempDesc.AnimCollection)
			{
				int32 MeshDefIndex = TempDesc.AnimCollection->FindMeshDef(Mesh);
				if (MeshDefIndex == -1)
				{
					//UE_LOGFMT(LogSkelot, Error, "Can't attach SkeletalMesh {0} to {1}. AnimCollection Mismatch or SkeletalMesh not registered!", GetDbgFName(Mesh),  GetDbgFName(TempDesc.Component));
					continue;
				}
			}

			FSkelotMeshRenderDesc& MeshDesc = TempDesc.Meshes.AddDefaulted_GetRef();
			MeshDesc.Mesh = const_cast<USkeletalMesh*>(Mesh);

		}
		else
		{
			if (CurMeshIdx == -1) //already detached ?
				continue;

			TempDesc.Meshes.RemoveAt(CurMeshIdx);
		}
	}

	Impl()->ReattachToDesc(InstanceIndex, TempDesc);
}
#endif

void ASkelotWorld::SetInstanceRenderParams(int32 InstanceIndex, const FSkelotInstanceRenderDesc& Desc)
{
	Impl()->ReattachToDesc(InstanceIndex, Desc);
}

void ASkelotWorld::SetInstanceRenderParams(int32 InstanceIndex, USkelotRenderParams* RenderParams)
{
	Impl()->ReattachToDesc(InstanceIndex, RenderParams->Data);
}

void ASkelotWorld::SetInstanceRenderParams(int32 InstanceIndex, FSetElementId DescId)
{
	Impl()->ReattachToDesc(InstanceIndex, DescId);
}

bool ASkelotWorld::InstanceAttachMesh_ByName(int32 InstanceIndex, FName Name, bool bAttach)
{
	if (SOA.ClusterData[InstanceIndex].DescIdx != -1 && !Name.IsNone())
	{
		int32 SubMeshIdx = GetInstanceDesc(InstanceIndex).IndexOfMesh(Name);
		if (!SubMeshIdx)
		{
			UE_LOGFMT(LogSkelot, Warning, "submesh name {0} not valid!", Name);
			return false;
		}

		return InstanceAttachMesh_ByIndex_Unsafe(InstanceIndex, SubMeshIdx, bAttach);
	}

	return false;
}

bool ASkelotWorld::InstanceAttachMesh_ByIndex_Unsafe(int32 InstanceIndex, uint8 SubMeshIdx, bool bAttach)
{
	checkf(SubMeshIdx != 0xFF, TEXT("254 submeshes are supported at most."));

	const FSkelotInstancesSOA::FClusterData& CD = SOA.ClusterData[InstanceIndex];
	if (CD.ClusterIdx != -1)
	{
		FSkelotCluster& SK = this->RenderDescs.Get(CD.GetDescId()).Clusters.Get(CD.GetClusterId());
		SK.bAnyAddRemove = true;
	}

	if (bAttach)
	{
		return SkelotTerminatedArrayAddUnique(GetInstanceSubmeshIndices(InstanceIndex), MaxSubmeshPerInstance, SubMeshIdx);
	}
	else
	{
		return SkelotTerminatedArrayRemoveShift(GetInstanceSubmeshIndices(InstanceIndex), MaxSubmeshPerInstance, SubMeshIdx);
	}
}

bool ASkelotWorld::InstanceAttachMesh_ByIndex(int32 InstanceIndex, int32 SubMeshIdx, bool bAttach)
{
	if (SOA.ClusterData[InstanceIndex].DescIdx != -1)
	{
		const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);
		if (Desc.Meshes.IsValidIndex(SubMeshIdx))
			return InstanceAttachMesh_ByIndex_Unsafe(InstanceIndex, (uint8)SubMeshIdx, bAttach);
	}
	return false;
}

bool ASkelotWorld::InstanceAttachMesh_ByAsset(int32 InstanceIndex, const USkeletalMesh* Mesh, bool bAttach)
{
	if (SOA.ClusterData[InstanceIndex].DescIdx != -1 && Mesh)
	{
		int32 SubMeshIdx = GetInstanceDesc(InstanceIndex).IndexOfMesh(Mesh);
		if (SubMeshIdx == -1)
		{
			UE_LOGFMT(LogSkelot, Warning, "skeletal mesh {0} not registered as submesh. check FSkelotInstanceRenderDesc.Meshes.", Mesh->GetFName());
			return false;
		}

		return InstanceAttachMesh_ByIndex_Unsafe(InstanceIndex, (uint8)SubMeshIdx, bAttach);
	}

	return false;
}

void ASkelotWorld::InstanceDetachMeshes(int32 InstanceIndex)
{
	const FSkelotInstancesSOA::FClusterData& CD = SOA.ClusterData[InstanceIndex];
	if (CD.ClusterIdx != -1)
	{
		FSkelotCluster& SK = this->RenderDescs.Get(CD.GetDescId()).Clusters.Get(CD.GetClusterId());
		SK.bAnyAddRemove = true;
	}

	uint8* SMI = GetInstanceSubmeshIndices(InstanceIndex);
	for (uint32 i = 0; i < MaxSubmeshPerInstance; i++)
		SMI[i] = 0xFF;
}

void ASkelotWorld::AttachRandomhMeshByGroup(int32 InstanceIndex, FName GroupName)
{
	if (SOA.ClusterData[InstanceIndex].DescIdx != -1)
	{
		const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);
		TArray<int32, TInlineAllocator<64>> Submeshes;
		for (int32 SubMeshIdx = 0; SubMeshIdx < Desc.Meshes.Num(); SubMeshIdx++)
		{
			if (Desc.Meshes[SubMeshIdx].GroupName == GroupName && !IsMeshAttached(InstanceIndex, SubMeshIdx))
				Submeshes.Add(SubMeshIdx);
		}

		if (Submeshes.Num())
		{
			InstanceAttachMesh_ByIndex_Unsafe(InstanceIndex, Submeshes[FMath::RandHelper(Submeshes.Num())], true);
		}
	}
}

void ASkelotWorld::AttachAllMeshGroups(int32 InstanceIndex)
{
	if (SOA.ClusterData[InstanceIndex].DescIdx != -1)
	{
		const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);
		
		TArray<FName, TInlineAllocator<64>> Groups;
		for (int32 SubMeshIdx = 0; SubMeshIdx < Desc.Meshes.Num(); SubMeshIdx++)
			Groups.AddUnique(Desc.Meshes[SubMeshIdx].GroupName);

		for(FName GroupName : Groups)
		{
			if(!GroupName.IsNone())
				AttachRandomhMeshByGroup(InstanceIndex, GroupName);
		}
	}
}

void ASkelotWorld::DetachMeshesByGroup(int32 InstanceIndex, FName GroupName)
{
	if (SOA.ClusterData[InstanceIndex].DescIdx != -1)
	{
		const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);
		TArray<uint8, TInlineAllocator<64>> ToBeRemoved;
		for (const uint8* SMIIter = GetInstanceSubmeshIndices(InstanceIndex); *SMIIter != 0xFF; SMIIter++)
		{
			if (Desc.Meshes.IsValidIndex(*SMIIter) && Desc.Meshes[*SMIIter].GroupName == GroupName)
				ToBeRemoved.Add(*SMIIter);
		}

		for (uint8 SubMeshIdx : ToBeRemoved)
			InstanceAttachMesh_ByIndex_Unsafe(InstanceIndex, SubMeshIdx, false);
	}
}

void ASkelotWorld::GetInstanceAttachedMeshes(int32 InstanceIndex, TArray<USkeletalMesh*>& OutMeshes)
{
	if (SOA.ClusterData[InstanceIndex].DescIdx != -1)
	{
		const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);
		for (const uint8* SMIIter = GetInstanceSubmeshIndices(InstanceIndex); *SMIIter != 0xFF; SMIIter++)
		{
			OutMeshes.Add(Desc.Meshes[*SMIIter].Mesh);
		}
	}
}

bool ASkelotWorld::IsMeshAttached(int32 InstanceIndex, uint8 SubMeshIndex) const
{
	check(SubMeshIndex != 0xFF);
	for (const uint8* Iter = GetInstanceSubmeshIndices(InstanceIndex); *Iter != 0xFF; Iter++)
		if (*Iter == SubMeshIndex)
			return true;
	
	return false;
}

bool ASkelotWorld::IsMeshAttached(int32 InstanceIndex, FName SubMeshName) const
{
	if (SOA.ClusterData[InstanceIndex].DescIdx != -1)
	{
		const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);
		for (const uint8* Iter = GetInstanceSubmeshIndices(InstanceIndex); *Iter != 0xFF; Iter++)
			if (Desc.Meshes.IsValidIndex(*Iter) && Desc.Meshes[*Iter].Name == SubMeshName)
				return true;
	}

	return false;
}

float ASkelotWorld::InstancePlayAnimation(int32 InstanceIndex, const FSkelotAnimPlayParams& Params)
{
	if (!IsValid(Params.Animation))
		return -1;

	if (this->SOA.ClusterData[InstanceIndex].DescIdx == -1) //no valid render params ?
		return -1;
		 
	const FSkelotInstanceRenderDesc& Desc = Impl()->GetInstanceDesc(InstanceIndex);
	USkelotAnimCollection* AnimCollection = Desc.AnimCollection;

	if (!AnimCollection)
	{
		UE_LOGFMT(LogSkelot, Error, "Can't Play Animation. Invalid AnimCollection.");
		return -1;
	}

	if (!AnimCollection->bIsBuilt)
		return -1;

	FSkelotInstancesSOA::FSlotData& Slot = SOA.Slots[InstanceIndex];
	FSkelotInstancesSOA::FAnimData& AnimData = SOA.AnimDatas[InstanceIndex];
	
	if((Slot.bDestroyed | Slot.bAnimationPaused | Slot.bDynamicPose))
		return -1;

	
	int32 TargetSeqDefIndex = -1; //index for AnimCollection->Sequences[]
	

	const float AssetLength = Params.Animation->GetPlayLength();	//length of the Sequence or Composite (sum of its segments)
	check(AssetLength > 0);
	float PlayTrackPos = Params.StartAt;
	float PlayAnimPos;

	if (Params.bUnique && AnimData.CurrentAsset == Params.Animation) //same animation is already being played ?
		return AssetLength;

	SkelotAnimAdvanceTime(Params.bLoop, float(0), PlayTrackPos, AssetLength);	//clamp or wrap time


	if(Params.Animation->GetClass() == UAnimComposite::StaticClass()) //is it UAnimComposite ?
	{
		UAnimComposite* CompositeAnimation = static_cast<UAnimComposite*>(Params.Animation);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		//make sure all segments of the track are registered in the AnimCollection
		for (const FAnimSegment& Track : CompositeAnimation->AnimationTrack.AnimSegments)
		{
			int32 TrackAnimSeqIndex = AnimCollection->FindSequenceDef(Track.GetAnimReference());
			if (TrackAnimSeqIndex == -1)
			{
				UE_LOGFMT(LogSkelot, Error, "Can't Play {0}. AnimSequence {0} of AnimComposite {1} is not registered in {2} ", GetDbgFName(Track.GetAnimReference()), GetDbgFName(Params.Animation), GetDbgFName(AnimCollection));
				return -1;
			}
		}
#endif
		//find what segment need to be played
		const FAnimSegment* AnimSegment = CompositeAnimation->AnimationTrack.GetSegmentAtTime(PlayTrackPos);
		PlayAnimPos = AnimSegment->ConvertTrackPosToAnimPos(PlayTrackPos);
		TargetSeqDefIndex = AnimCollection->FindSequenceDef(AnimSegment->GetAnimReference());
		check(TargetSeqDefIndex != -1);
	}
	else if (Params.Animation->GetClass() == UAnimSequence::StaticClass()) // is it UAnimSequence ?
	{
		PlayAnimPos = PlayTrackPos;
		TargetSeqDefIndex = AnimCollection->FindSequenceDef(Params.Animation);
		if (TargetSeqDefIndex == -1)
		{
			UE_LOGFMT(LogSkelot, Error, "Can't Play {0}. AnimSequence not registered in {1}", GetDbgFName(Params.Animation), GetDbgFName(AnimCollection));
			return -1;
		}
	}
	else
	{
		UE_LOGFMT(LogSkelot, Error, "Can't Play {0}. Only UAnimSequence and UAnimComposite is supported !", GetDbgFName(Params.Animation));
		return -1;
	}

	const FSkelotSequenceDef& TargetSeq = AnimCollection->Sequences[TargetSeqDefIndex];

	const bool bIsLooped = Slot.bAnimationLooped;
	Slot.bNoSequence = false;
	Slot.bAnimationLooped = Params.bLoop;
	

	AnimData.AnimationPlayRate = Params.PlayScale;
	AnimData.AnimationTime = PlayTrackPos;
	AnimData.CurrentAsset = Params.Animation;

	const int32 TargetLocalFrameIndex = static_cast<int32>(PlayAnimPos * TargetSeq.SampleFrequency);
	const int32 TargetGlobalFrameIndex = TargetSeq.AnimationFrameIndex + TargetLocalFrameIndex;
	check(TargetLocalFrameIndex < TargetSeq.AnimationFrameCount);
	check(TargetGlobalFrameIndex < AnimCollection->FrameCountSequences);
	const bool bComputeShaderSupported = this->GetWorld()->GetFeatureLevel() >= ERHIFeatureLevel::SM5;

	const bool bIsPlayingTransition = AnimData.IsTransitionValid();
	const bool bCanTransition = Params.TransitionDuration > 0 && bComputeShaderSupported && AnimData.IsSequenceValid() && !bIsPlayingTransition && !GSkelot_DisableTransition;

	if (bCanTransition)	//blend should happen if a sequence is already being played without blend
	{
		//#TODO looped transition is not supported yet. 
		int32 TransitionFrameCount = static_cast<int32>(TargetSeq.SampleFrequency * Params.TransitionDuration) /*+ 1*/;
		if (TransitionFrameCount >= 3)
		{
			int32 Remain = TargetSeq.AnimationFrameCount - TargetLocalFrameIndex;
			TransitionFrameCount = FMath::Min(TransitionFrameCount, Remain);

			if (TransitionFrameCount >= 3)
			{
				const FSkelotSequenceDef& CurrentSeqStruct = AnimCollection->Sequences[AnimData.AnimationCurrentSequence];

				check(SOA.CurAnimFrames[InstanceIndex] < AnimCollection->FrameCountSequences);
				int32 CurLocalFrameIndex = SOA.CurAnimFrames[InstanceIndex] - CurrentSeqStruct.AnimationFrameIndex;
				check(CurLocalFrameIndex < CurrentSeqStruct.AnimationFrameCount);

				USkelotAnimCollection::FTransitionKey TransitionKey;
				TransitionKey.FromSI = static_cast<uint16>(AnimData.AnimationCurrentSequence);
				TransitionKey.ToSI = static_cast<uint16>(TargetSeqDefIndex);
				TransitionKey.FromFI = CurLocalFrameIndex;
				TransitionKey.ToFI = TargetLocalFrameIndex;
				TransitionKey.FrameCount = static_cast<uint16>(TransitionFrameCount);
				TransitionKey.BlendOption = Params.BlendOption;
				TransitionKey.bFromLoops = bIsLooped;
				TransitionKey.bToLoops = Params.bLoop;

				auto [TransitionIndex, Result] = AnimCollection->FindOrCreateTransition(TransitionKey, Params.bIgnoreTransitionGeneration);
				if (TransitionIndex != -1)
				{
					if (GSkelot_DebugTransitions)
						Impl()->DebugDrawInstanceBound(InstanceIndex, 0, Result == USkelotAnimCollection::ETR_Success_Found ? FColor::Green : FColor::Yellow, false, 0.3f);

					const USkelotAnimCollection::FTransition& Transition = AnimCollection->Transitions[TransitionIndex];

					AnimData.AnimationCurrentSequence = static_cast<uint16>(TargetSeqDefIndex);
					AnimData.AnimationTransitionIndex = static_cast<uint16>(TransitionIndex);
					Impl()->SetAnimFrame(InstanceIndex, Transition.FrameIndex);
					return AssetLength;
				}
				else
				{

				}

			}
			else
			{
				UE_LOGFMT(LogSkelot, Warning, "Can't Transition From {0} To {1}. wrapping is not supported.", AnimData.AnimationCurrentSequence, TargetSeqDefIndex);
			}
		}
		else
		{
			UE_LOGFMT(LogSkelot, Warning, "Can't Transition From {0} To {1}. duration too low.", AnimData.AnimationCurrentSequence, TargetSeqDefIndex);
		}


	}

	//NormalPlay:
	if (bIsPlayingTransition)
	{
		AnimCollection->DecTransitionRef(AnimData.AnimationTransitionIndex);
	}

	if (GSkelot_DebugTransitions && Params.TransitionDuration > 0) //show that instance failed to play transition
		Impl()->DebugDrawInstanceBound(InstanceIndex, -2, FColor::Red, false, 0.3f);

	AnimData.AnimationTransitionIndex = static_cast<uint16>(-1);
	AnimData.AnimationCurrentSequence = static_cast<uint16>(TargetSeqDefIndex);
	Impl()->SetAnimFrame(InstanceIndex, TargetGlobalFrameIndex);


	return AssetLength;
}

void ASkelotWorld::ResetAnimationState(int32 InstanceIndex)
{
	this->EnableInstanceDynamicPose(InstanceIndex, false);

	FSkelotInstancesSOA::FAnimData& AnimData = SOA.AnimDatas[InstanceIndex];
	const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);
	if (::IsValid(Desc.AnimCollection))
	{
		//release transition if any
		if (AnimData.IsTransitionValid())
			Desc.AnimCollection->DecTransitionRef(AnimData.AnimationTransitionIndex);
	}
	AnimData = FSkelotInstancesSOA::FAnimData();
	SOA.CurAnimFrames[InstanceIndex] = 0;
}

float ASkelotWorld::GetInstancePlayLength(int32 InstanceIndex) const
{
	if (UAnimSequenceBase* AS = SOA.AnimDatas[InstanceIndex].CurrentAsset)
		return AS->GetPlayLength();

	return 0;
}

float ASkelotWorld::GetInstancePlayTimeRemaining(int32 InstanceIndex) const
{
	if (UAnimSequenceBase* AS = SOA.AnimDatas[InstanceIndex].CurrentAsset)
	{
		float Remain = AS->GetPlayLength() - SOA.AnimDatas[InstanceIndex].AnimationTime;
		check(Remain >= 0 && Remain <= AS->GetPlayLength());
		return Remain;
	}
	return 0;
}

float ASkelotWorld::GetInstancePlayTimeFraction(int32 InstanceIndex) const
{
	if (UAnimSequenceBase* AS = SOA.AnimDatas[InstanceIndex].CurrentAsset)
	{
		float Fraction = SOA.AnimDatas[InstanceIndex].AnimationTime / AS->GetPlayLength();
		return FMath::Clamp(Fraction, 0.0f, 1.0f);
	}
	return 0;
}

USkelotAnimCollection* ASkelotWorld::GetInstanceAnimCollection(FSkelotInstanceHandle H) const
{
	if (IsHandleValid(H))
	{
 		return Impl()->GetInstanceDesc(H.InstanceIndex).AnimCollection;
	}

	return nullptr;
}

USkelotAnimCollection* ASkelotWorld::GetInstanceAnimCollection(int32 InstanceIndex) const
{
	return Impl()->GetInstanceDesc(InstanceIndex).AnimCollection;
}

FSkelotAttachParentData& ASkelotWorld::GetOrCreateInstanceAttachParentData(int32 InstanceIndex)
{
	int32& AttchIdx = SOA.MiscData[InstanceIndex].AttachmentIndex;
	if (AttchIdx == -1)
	{
		AttchIdx = AttachParentArray.Add(FSkelotAttachParentData{});
		AttachParentArray[AttchIdx].InstanceIndex = InstanceIndex;
	}
	return AttachParentArray[AttchIdx];
}

FSkelotAttachParentData* ASkelotWorld::GetInstanceAttachParentData(int32 InstanceIndex) const
{
	check(InstanceIndex != -1);
	int32 AttchIdx = SOA.MiscData[InstanceIndex].AttachmentIndex;
	return AttchIdx != -1 ? const_cast<FSkelotAttachParentData*>(&AttachParentArray[AttchIdx]) : nullptr;
}

bool ASkelotWorld::InstanceAttachChild(FSkelotInstanceHandle Parent, FSkelotInstanceHandle Child, FName SocketOrBoneName, const FTransform& ReletiveTransform)
{
	if (IsHandleValid(Parent) && IsHandleValid(Child))
	{
		FSkelotAttachParentData& AttachFrag = this->Internal_InstanceAttachChild(Parent.InstanceIndex, Child.InstanceIndex);
		AttachFrag.RelativeTransform = (FTransform3f)ReletiveTransform;
		AttachFrag.SocketPtr = nullptr;
		AttachFrag.SocketBoneIndex = -1;

		if(!SocketOrBoneName.IsNone())
		{
			const FSkelotInstanceRenderDescFinal& ParentDesc = GetInstanceDesc(Parent.InstanceIndex);
			if (ParentDesc.AnimCollection)
			{
				AttachFrag.SocketPtr = ASkelotWorld_Impl::FindSkeletalSocket(ParentDesc, SocketOrBoneName, nullptr);
				AttachFrag.SocketBoneIndex = ParentDesc.AnimCollection->FindSkeletonBoneIndex(AttachFrag.SocketPtr ? AttachFrag.SocketPtr->BoneName : SocketOrBoneName);
				if (!ParentDesc.AnimCollection->IsBoneTransformCached(AttachFrag.SocketBoneIndex))
				{
					AttachFrag.SocketPtr = nullptr;
					AttachFrag.SocketBoneIndex = -1;
					UE_LOGFMT(LogSkelot, Warning, "SocketOrBoneName {0} is invalid or not cached. check AnimCollection {1}, ", SocketOrBoneName, GetDbgFName(ParentDesc.AnimCollection));
				}
			}
		}

		UpdateChildTransforms(Parent.InstanceIndex);
		return true;
	}

	return false;
}

FSkelotAttachParentData& ASkelotWorld::Internal_InstanceAttachChild(int32 ParentIdx, int32 ChildIdx)
{
	DetachInstanceFromParent(ChildIdx);

	//create attachment data, array may realloc so we take pointer afterward
	GetOrCreateInstanceAttachParentData(ParentIdx);
	GetOrCreateInstanceAttachParentData(ChildIdx);
	FSkelotAttachParentData* ParentFrag = &GetOrCreateInstanceAttachParentData(ParentIdx);
	FSkelotAttachParentData* ChildFrag = &GetOrCreateInstanceAttachParentData(ChildIdx);

	//new child becomes the first child of its parent
	ChildFrag->Parent = ParentIdx;
	ChildFrag->Down = ParentFrag->FirstChild;
	ParentFrag->FirstChild = ChildIdx;

	return *ChildFrag;
}

void ASkelotWorld::DetachInstanceFromParent(FSkelotInstanceHandle H)
{
	if(IsHandleValid(H))
	{
		this->DetachInstanceFromParent(H.InstanceIndex);
	}
}

void ASkelotWorld::DetachInstanceFromParent(int32 InstanceIdx)
{
	FSkelotAttachParentData* AttachFrag = GetInstanceAttachParentData(InstanceIdx);
	if (!AttachFrag)
		return;

	if (AttachFrag->Parent != -1) //has any parent ?
	{
		FSkelotAttachParentData* ParentFrag = GetInstanceAttachParentData(AttachFrag->Parent);
		check(ParentFrag->FirstChild != -1);

		if (ParentFrag->FirstChild == InstanceIdx) //its first child of parent ?
		{
			ParentFrag->FirstChild = AttachFrag->Down;
		}
		else
		{
			FSkelotAttachParentData* Iter = GetInstanceAttachParentData(ParentFrag->FirstChild);
			while (true)
			{
				check(Iter->Down != -1 && Iter->Parent == AttachFrag->Parent);

				if (Iter->Down == InstanceIdx)
				{
					Iter->Down = AttachFrag->Down;
					break;
				}

				Iter = GetInstanceAttachParentData(Iter->Down);
			}
		}

		AttachFrag->Parent = -1;
		AttachFrag->Down = -1;
	}
}

void ASkelotWorld::SetInstanceRelativeTransform(int32 InstanceIndex, const FTransform3f& T)
{
	if(FSkelotAttachParentData* Frag = GetInstanceAttachParentData(InstanceIndex))
	{
		Frag->RelativeTransform = T;
	}
}

int32 ASkelotWorld::GetInstanceParentIndex(int32 InstanceIndex) const
{
	const FSkelotAttachParentData* Frag = GetInstanceAttachParentData(InstanceIndex);
	return Frag ? Frag->Parent : -1;
}

bool ASkelotWorld::IsInstanceChildOf(int32 InstanceIndex, int32 TargetParentIndex) const
{
	int32 ParentIdx = GetInstanceParentIndex(InstanceIndex);
	while (ParentIdx != -1)
	{
		if(ParentIdx == TargetParentIndex)
			return true;

		ParentIdx = GetInstanceParentIndex(ParentIdx);
	}

	return false;

}


void ASkelotWorld::ForEachChild(FSkelotAttachParentData& Root, ChildVisitFunc Func)
{
	int32 ChildIdx = Root.FirstChild;
	while (ChildIdx != -1)
	{
		FSkelotAttachParentData* ChildFrag = GetInstanceAttachParentData(ChildIdx);
		checkf(ChildFrag, TEXT("when parent is pointing to the child. child MUST have FSkelotAttachParentData."));
		Func(Root, *ChildFrag);
		ForEachChild(*ChildFrag, Func);
		ChildIdx = ChildFrag->Down;
	}
}

void ASkelotWorld::ForEachChild(int32 InstanceIndex, ChildVisitFunc Func)
{
	if(FSkelotAttachParentData* Frag = GetInstanceAttachParentData(InstanceIndex))
		ForEachChild(*Frag, Func);
}

void ASkelotWorld::ForEachOffspring(int32 InstanceIndex, ChildVisitFunc Func)
{
	if (FSkelotAttachParentData* Frag = GetInstanceAttachParentData(InstanceIndex))
	{
		int32 ChildIdx = Frag->FirstChild;
		while (ChildIdx != -1)
		{
			FSkelotAttachParentData* ChildFrag = GetInstanceAttachParentData(ChildIdx);
			Func(*Frag, *ChildFrag);
			ChildIdx = ChildFrag->Down;
		}
	}
}

void ASkelotWorld::UpdateChildTransforms(int32 InstanceIndex)
{
	if(FSkelotAttachParentData* Frag = GetInstanceAttachParentData(InstanceIndex))
		UpdateChildTransforms(*Frag);
}

void ASkelotWorld::UpdateChildTransforms(FSkelotAttachParentData& RootFrag)
{
	ForEachChild(RootFrag, [&](FSkelotAttachParentData& ParentFrag, FSkelotAttachParentData& ChildFrag) {

		//mul order is == RelT * SocketT * BoneT * ParentInstanceT
		FTransform3f ChildT = ChildFrag.RelativeTransform;
		if (ChildFrag.SocketPtr)
			ChildT *= (FTransform3f)ChildFrag.SocketPtr->GetSocketLocalTransform();

		ChildT *= GetInstanceBoneTransformCS(ParentFrag.InstanceIndex, ChildFrag.SocketBoneIndex);

		SetInstanceTransform(ChildFrag.InstanceIndex, FTransform(ChildT) * GetInstanceTransform(ParentFrag.InstanceIndex));
	});
}
FTransform3f ASkelotWorld::GetInstanceBoneTransformCS(int32 InstanceIndex, int32 BoneIndex) const
{
	const USkelotAnimCollection* AnimCollection = GetInstanceDesc(InstanceIndex).AnimCollection;
	if (AnimCollection && AnimCollection->IsBoneTransformCached(BoneIndex))
	{
		return AnimCollection->GetBoneTransformFast(BoneIndex, SOA.CurAnimFrames[InstanceIndex]);
	}

	return FTransform3f::Identity;
}

FTransform ASkelotWorld::GetInstanceBoneTransformWS(int32 InstanceIndex, int32 SkeletonBoneIndex) const
{
	FTransform BoneT = (FTransform)GetInstanceBoneTransformCS(InstanceIndex, SkeletonBoneIndex);
	return BoneT * GetInstanceTransform(InstanceIndex);
}

FTransform ASkelotWorld::GetInstanceSocketTransform(int32 InstanceIndex, FName SocketOrBoneName, USkeletalMesh* InMesh /*= nullptr*/, bool bWorldSpace /*= true*/) const
{
	FTransform SocketT = FTransform::Identity;

	if (!SocketOrBoneName.IsNone())
	{
		const FSkelotInstanceRenderDescFinal& Desc = GetInstanceDesc(InstanceIndex);
		if (Desc.AnimCollection)
		{
			const USkeletalMeshSocket* SocketPtr = ASkelotWorld_Impl::FindSkeletalSocket(Desc, SocketOrBoneName, InMesh);
			int32 BoneIndex = Desc.AnimCollection->FindSkeletonBoneIndex(SocketPtr ? SocketPtr->BoneName : SocketOrBoneName);
			if (BoneIndex == -1)
			{
				UE_LOGFMT(LogSkelot, Warning, "SocketOrBoneName {0} is invalid or not cached. check AnimCollection {1}, ", SocketOrBoneName, GetDbgFName(Desc.AnimCollection));
			}
			else
			{
				SocketT = SocketPtr->GetSocketLocalTransform() * ((FTransform)GetInstanceBoneTransformCS(InstanceIndex, BoneIndex));
			}
		}
	}

	return bWorldSpace ? SocketT * GetInstanceTransform(InstanceIndex) : SocketT;
}



USkeletalMeshSocket* ASkelotWorld::FindSkeletalSocket(const FSkelotInstanceRenderDesc& Desc, FName SocketName, const USkeletalMesh* InMesh /*= nullptr*/)
{
	if (InMesh) //#TODO check if skeletons are identical ?
		return InMesh->FindSocket(SocketName);

	if (Desc.Meshes.Num() == 0 && Desc.Meshes[0].Mesh)
	{
		return Desc.Meshes[0].Mesh->FindSocket(SocketName);
	}

	if (Desc.AnimCollection && Desc.AnimCollection->Skeleton)
		return Desc.AnimCollection->Skeleton->FindSocket(SocketName);

	return nullptr;
}

void ASkelotWorld::SetInstanceCustomDataFloat(int32 InstanceIndex, const float* Floats, int32 Offset, int32 Count)
{
	float* InstanceFloats = GetInstanceCustomDataFloats(InstanceIndex);
	check(Offset >= 0);
	for (int32 Idx = 0; Idx < Count; Idx++)
	{
		const int32 FloatIdx = Offset + Idx;
		if (FloatIdx < SOA.MaxNumCustomDataFloat)
			InstanceFloats[FloatIdx] = Floats[Idx];
	}
}

void ASkelotWorld::DestroyChildren(int32 InstanceIndex)
{
	TArray<FSkelotInstanceHandle, TInlineAllocator<16>> Childs;
	GetInstanceChildren<true>(InstanceIndex, Childs);
	for (FSkelotInstanceHandle H : Childs)
		DestroyInstance(H);
}

void ASkelotWorld::Internal_CallOnAnimationFinished()
{
	if (AnimationFinishEvents.Num())
	{
		OnAnimationFinished(AnimationFinishEvents);
		OnAnimationFinishedDelegate.Broadcast(this, AnimationFinishEvents);
		AnimationFinishEvents.Reset();
	}
}

void ASkelotWorld::Internal_CallOnAnimationNotify()
{
	//name only notifications
	if (AnimationNotifyEvents.Num())
	{
		OnAnimationNotify(AnimationNotifyEvents);
		OnAnimationNotifyDelegate.Broadcast(this, AnimationNotifyEvents);
		AnimationNotifyEvents.Reset();
	}

	for (const FSkelotAnimNotifyObjectEvent& Ev : AnimationNotifyObjectEvents)
	{
		Ev.SkelotNotify->OnNotify(this, Ev.Handle, Ev.AnimSequence);
	}

	AnimationNotifyObjectEvents.Reset();
	
}

void ASkelotWorld::QueryLocationOverlappingSphere(const FVector& Center, float Radius, TArray<FSkelotInstanceHandle>& Instances)
{
	// 使用空间网格优化查询
	if (bEnableSpatialGrid && SpatialGrid.GetNumCells() > 0)
	{
		TArray<int32> Indices;
		SpatialGrid.QuerySphere(Center, Radius, Indices, 0xFF, &SOA);
		Instances.Reserve(Instances.Num() + Indices.Num());
		for (int32 Idx : Indices)
		{
			Instances.Add(IndexToHandle(Idx));
		}
	}
	else
	{
		// 回退到简单遍历
		for (int32 InstanceIndex = 0; InstanceIndex < GetNumInstance(); InstanceIndex++)
		{
			if (IsInstanceAlive(InstanceIndex))
			{
				auto DistSQ = (SOA.Locations[InstanceIndex] - Center).SizeSquared();
				if (DistSQ < (Radius * Radius))
				{
					Instances.Add(this->IndexToHandle(InstanceIndex));
				}
			}
		}
	}
}

void ASkelotWorld::QueryLocationOverlappingSphereWithMask(const FVector& Center, float Radius, TArray<FSkelotInstanceHandle>& OutInstances, uint8 CollisionMask)
{
	// 使用空间网格优化查询
	if (bEnableSpatialGrid && SpatialGrid.GetNumCells() > 0)
	{
		TArray<int32> Indices;
		SpatialGrid.QuerySphere(Center, Radius, Indices, CollisionMask, &SOA);
		OutInstances.Reserve(OutInstances.Num() + Indices.Num());
		for (int32 Idx : Indices)
		{
			OutInstances.Add(IndexToHandle(Idx));
		}
	}
	else
	{
		// 回退到简单遍历
		for (int32 InstanceIndex = 0; InstanceIndex < GetNumInstance(); InstanceIndex++)
		{
			if (IsInstanceAlive(InstanceIndex))
			{
				// 掩码过滤
				if (CollisionMask != 0xFF)
				{
					uint8 InstanceMask = SOA.CollisionMasks[InstanceIndex];
					if ((InstanceMask & CollisionMask) == 0)
					{
						continue;
					}
				}

				auto DistSQ = (SOA.Locations[InstanceIndex] - Center).SizeSquared();
				if (DistSQ < (Radius * Radius))
				{
					OutInstances.Add(this->IndexToHandle(InstanceIndex));
				}
			}
		}
	}
}

void ASkelotWorld::SetSpatialGridCellSize(float CellSize)
{
	SpatialGrid.SetCellSize(CellSize);
}

float ASkelotWorld::GetSpatialGridCellSize() const
{
	return SpatialGrid.GetCellSize();
}

void ASkelotWorld::RebuildSpatialGrid()
{
	if (bEnableSpatialGrid)
	{
		// 使用分帧更新（来自预研文档的 FrameStride 方案）
		SpatialGrid.RebuildIncremental(SOA, GetNumInstance());
	}
}

//////////////////////////////////////////////////////////////////////////
// PBD Collision API Implementation

void ASkelotWorld::SetPBDConfig(const FSkelotPBDConfig& InConfig)
{
	PBDConfig = InConfig;
	PBDCollisionSystem.SetConfig(PBDConfig);

	// 根据PBD碰撞半径调整空间网格大小
	if (bEnableSpatialGrid && PBDConfig.bEnablePBD)
	{
		float OptimalCellSize = PBDConfig.CollisionRadius * 2.0f * PBDConfig.GridCellSizeMultiplier;
		SpatialGrid.SetCellSize(OptimalCellSize);
	}
}

void ASkelotWorld::SetPBDEnabled(bool bEnable)
{
	PBDConfig.bEnablePBD = bEnable;
	PBDCollisionSystem.SetConfig(PBDConfig);
}

void ASkelotWorld::SetPBDCollisionRadius(float Radius)
{
	PBDConfig.CollisionRadius = Radius;
	PBDCollisionSystem.SetConfig(PBDConfig);

	// 更新空间网格大小
	if (bEnableSpatialGrid && PBDConfig.bEnablePBD)
	{
		float OptimalCellSize = PBDConfig.CollisionRadius * 2.0f * PBDConfig.GridCellSizeMultiplier;
		SpatialGrid.SetCellSize(OptimalCellSize);
	}
}

void ASkelotWorld::SolvePBDCollisions(float DeltaTime)
{
	// 检查是否启用PBD碰撞
	if (!PBDConfig.bEnablePBD)
	{
		return;
	}

	// 检查更新频率
	PBDUpdateFrameCounter++;
	if (PBDUpdateFrameCounter < PBDConfig.UpdateFrequency)
	{
		return;
	}
	PBDUpdateFrameCounter = 0;

	// 执行PBD碰撞求解
	PBDCollisionSystem.SolveCollisions(SOA, GetNumInstance(), SpatialGrid, DeltaTime);
}

void ASkelotWorld::RemoveInvalidHandles(bool bMaintainOrder, TArray<FSkelotInstanceHandle>& InOutHandles)
{
	if (bMaintainOrder)
	{
		InOutHandles.RemoveAll([&](FSkelotInstanceHandle H) { return !this->IsHandleValid(H); });
	}
	else
	{
		InOutHandles.RemoveAllSwap([&](FSkelotInstanceHandle H) { return !this->IsHandleValid(H); });
	}
}




void ASkelotWorld::GetAllHandles(TArray<FSkelotInstanceHandle>& OutHandles)
{
	int32 BaseIdx = OutHandles.AddUninitialized(GetNumValidInstance());
	for (int32 InstanceIndex = 0; InstanceIndex < GetNumInstance(); InstanceIndex++)
		if (IsInstanceAlive(InstanceIndex))
			OutHandles[BaseIdx++] = IndexToHandle(InstanceIndex);
}


TObjectPtr<UObject> ASkelotWorld::GetInstanceUserObject(int32 InstanceIndex)
{
	return SOA.UserObjects.Num() != 0 ? SOA.UserObjects[InstanceIndex] : nullptr;
}

void ASkelotWorld::SetInstanceUserObject(int32 InstanceIndex, TObjectPtr<UObject> InObject)
{
	if (SOA.UserObjects.Num() == 0)
	{
		SOA.UserObjects.SetNumZeroed(SOA.Slots.Num());
	}
	SOA.UserObjects[InstanceIndex] = InObject;
}


bool ASkelotWorld::EnableInstanceDynamicPose(int32 InstanceIndex, bool bEnable)
{
	const bool bIsDynamic = SOA.Slots[InstanceIndex].bDynamicPose;
	if (bIsDynamic == bEnable)
		return true;

	const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);
	if (!::IsValid(Desc.AnimCollection))
		return false;

	FSkelotInstancesSOA::FAnimData& AnimData = SOA.AnimDatas[InstanceIndex];
	if (bEnable)
	{
		int32 DynamicPoseIndex = Desc.AnimCollection->AllocDynamicPose();
		if (DynamicPoseIndex == -1)
		{
			UE_LOGFMT(LogSkelot, Warning, "EnableInstanceDynamicPose Failed. Buffer Full! InstanceIndex:{1}", InstanceIndex);
			return false;
		}

		//release transition if any
		if (AnimData.IsTransitionValid())
			Desc.AnimCollection->DecTransitionRef(AnimData.AnimationTransitionIndex);

		SOA.Slots[InstanceIndex].bDynamicPose = true;

		SOA.CurAnimFrames[InstanceIndex] = Desc.AnimCollection->DynamicPoseIndexToFrameIndex(DynamicPoseIndex);
		AnimData = FSkelotInstancesSOA::FAnimData();

		UE_LOGFMT(LogSkelot, VeryVerbose, "DynamicPose Enabled. DynamicPoseIndex:{1}", DynamicPoseIndex);
	}
	else
	{
		Impl()->ReleaseDynamicPose(InstanceIndex);
	}

	return true;
}

bool ASkelotWorld::TieDynamicPoseToComponent(int32 InstanceIndex, USkeletalMeshComponent* SrcComponent, int32 UserData, bool bCopyTransform)
{
	if(SOA.Slots[InstanceIndex].bDynamicPose && ::IsValid(SrcComponent))
	{
		FSkelotFrag_DynPoseTie& Frag = DynamicPosTiedMap.FindOrAdd(InstanceIndex, FSkelotFrag_DynPoseTie());
		Frag.SKMeshComponent = SrcComponent;
		Frag.UserData = UserData;
		Frag.bCopyTransform = bCopyTransform;
		return true;
	}
	return false;
}

USkeletalMeshComponent* ASkelotWorld::UntieDynamicPoseFromComponent(int32 InstanceIndex, int32* OutUserData)
{
	FSkelotFrag_DynPoseTie RemovedData;
	if (DynamicPosTiedMap.RemoveAndCopyValue(InstanceIndex, RemovedData))
	{
		if (OutUserData)
			*OutUserData = RemovedData.UserData;

		return RemovedData.SKMeshComponent;
	}
	return nullptr;
}

FSkelotFrag_DynPoseTie* ASkelotWorld::GetDynamicPoseTieData(int32 InstanceIndex)
{
	return DynamicPosTiedMap.Find(InstanceIndex);
}




void ASkelotWorld::LineTraceInstance(int32 InstanceIndex, const FLineTraceParams& Params, FLineTraceResult& OutResult) const
{
	OutResult.BoneIndex = -1;	//bone index of closest hit

	const FSkelotInstanceRenderDesc& RenderDesc = GetInstanceDesc(InstanceIndex);
	USkelotAnimCollection* AnimCollection = RenderDesc.AnimCollection;
	if (!AnimCollection)
		return;

	const FTransform InstanceTransform = GetInstanceTransform(InstanceIndex);
	check(InstanceTransform.IsValid());
	Chaos::FVec3 Dir = Params.End - Params.Start;
	const auto Len = Dir.Size();
	checkf(Len > UE_KINDA_SMALL_NUMBER, TEXT("ray length should be greater that zero"));
	Dir /= Len;

	
	auto LRayCastCompactAssets = [&](const Chaos::FVec3& S, const Chaos::FVec3& D, const Chaos::FReal L, const Chaos::FReal T, Chaos::FReal& TOI, Chaos::FVec3& POI, Chaos::FVec3& NOI) -> bool {

		//check for Max Bounding Box vs Ray first 
		if (Params.bCheckBoundingBoxFirst)
		{
			const FBox LocalBound = FBox(AnimCollection->MeshesBBox.GetFBox());
			Chaos::FReal unusedTime;
			Chaos::FVec3 unusedPos, unusedNormal;
			int unusedFaceIndex;
			if (!Chaos::FAABB3(LocalBound.Min, LocalBound.Max).Raycast(S, D, L, T, unusedTime, unusedPos, unusedNormal, unusedFaceIndex))
			{
				return false;
			}
		}

		FSkelotCompactPhysicsAsset::FRaycastParams RP;
		RP.AnimCollection = AnimCollection;
		RP.bReturnFirstHit = Params.bReturnFirstHit;
		RP.BonesToExclude = Params.BonesToExclude;
		RP.FrameIndex = SOA.CurAnimFrames[InstanceIndex];
		RP.StartPoint = S;
		RP.Direction = D;
		RP.Length = L;
		RP.Thickness = T;

		AnimCollection->ConditionalFlushDeferredTransitions(RP.FrameIndex);	//transitions must be ready by now

		for (int32 MeshDefIndex : RenderDesc.CachedMeshDefIndices)
		{
			const FSkelotMeshDef& MeshDef = AnimCollection->Meshes[MeshDefIndex];
			MeshDef.CompactPhysicsAsset.Raycast(RP);
			if (RP.OutBoneIndex != -1) //hit anything ?
			{
				if (OutResult.BoneIndex == -1 || RP.OutTime < TOI)
				{
					OutResult.BoneIndex = RP.OutBoneIndex;

					TOI = RP.OutTime;
					POI = RP.OutPosition;
					NOI = RP.OutNormal;

					if (Params.bReturnFirstHit)
						return true;
				}
			}
		}

		return OutResult.BoneIndex != -1;
	};

	static_assert(sizeof(Chaos::FVec3) <= sizeof(FVector));

	SkelotTransformedRaycast(InstanceTransform, Params.Start, Dir, Len, Params.Thickness, OutResult.Distance, reinterpret_cast<Chaos::FVec3&>(OutResult.Position), reinterpret_cast<Chaos::FVec3&>(OutResult.Normal), LRayCastCompactAssets);
}

FSkelotInstanceHandle ASkelotWorld::LineTraceInstances(TConstArrayView<int32> Instances, const FLineTraceParams& InParams, FLineTraceResult& OutResult, float DebugDrawTime) const
{
	OutResult.BoneIndex = -1;

	FLineTraceResult CurHit;
	int32 HitInstanceIndex = -1;

	for (int32 InstanceIdx : Instances)
	{
		LineTraceInstance(InstanceIdx, InParams, CurHit);

		if (CurHit.BoneIndex != -1)
		{
			if (HitInstanceIndex == -1 || CurHit.Distance < OutResult.Distance)
			{
				HitInstanceIndex = InstanceIdx;
				OutResult = CurHit;
			}
		}
	}

#if ENABLE_DRAW_DEBUG 
	if (DebugDrawTime >= 0)
	{
		if (HitInstanceIndex == -1)
		{

			DrawDebugSweptSphere(GetWorld(), InParams.Start, InParams.End, InParams.Thickness, FColor::Red, false, DebugDrawTime, 0);
		}
		else
		{
			// Red up to the blocking hit, green thereafter
			::DrawDebugSweptSphere(GetWorld(), InParams.Start, OutResult.Position, InParams.Thickness, FColor::Red, false, DebugDrawTime);
			::DrawDebugSweptSphere(GetWorld(), OutResult.Position, InParams.End, InParams.Thickness, FColor::Green, false, DebugDrawTime);

			::DrawDebugPoint(GetWorld(), OutResult.Position, 8, FColor::Red, false, DebugDrawTime);//hit point
			::DrawDebugDirectionalArrow(GetWorld(), OutResult.Position, OutResult.Position + OutResult.Normal * 32, 1, FColor::Yellow, false, DebugDrawTime); //hit normal

			//FTransform BoneT = GetInstanceBoneTransformWS(HitInstanceIndex, OutResult.BoneIndex);
			//#TODO
			//FString BoneStr = OutBoneName.ToString();
			//::DrawDebugString(Singleton->GetWorld(), BoneT.GetLocation(), BoneStr, nullptr, FColor::Yellow, DebugDrawTime);
			//FBoxCenterExtentFloat BoundWS = Singleton->CalculateInstanceBound(HitInstanceIndex);
			//DrawDebugBox(Comp->GetWorld(), FVector(BoundWS.Center), FVector(BoundWS.Extent), FColor::Green, false, DebugDrawTime);
		}
	}
#endif

	if (HitInstanceIndex != -1)
		return IndexToHandle(HitInstanceIndex);

	return FSkelotInstanceHandle();
}

void ASkelotWorld::DebugDrawCompactPhysicsAsset(int32 InstanceIndex, FColor Color)
{
#if UE_ENABLE_DEBUG_DRAWING
	const FSkelotInstanceRenderDesc& RenderDesc = GetInstanceDesc(InstanceIndex);
	USkelotAnimCollection* AnimCollection = RenderDesc.AnimCollection;
	if (!AnimCollection)
		return;

	const FTransform InstanceTransform = GetInstanceTransform(InstanceIndex);
	check(InstanceTransform.IsValid());

	AnimCollection->ConditionalFlushDeferredTransitions(SOA.CurAnimFrames[InstanceIndex]);	//transitions must be ready by now

	for (int32 MeshDefIndex : RenderDesc.CachedMeshDefIndices)
	{
		const FSkelotMeshDef& MeshDef = AnimCollection->Meshes[MeshDefIndex];

		for (auto ShapeData : MeshDef.CompactPhysicsAsset.Spheres) //draw spheres
		{
			FTransform BT = (FTransform)this->GetInstanceBoneTransformWS(InstanceIndex, ShapeData.BoneIndex);
			DrawDebugSphere(GetWorld(), BT.TransformPosition(ShapeData.Center), BT.GetScale3D().GetAbsMax() * ShapeData.Radius, 64, Color);
		}
		for (auto ShapeData : MeshDef.CompactPhysicsAsset.Capsules) //draw capsules
		{
			FTransform BT = (FTransform)this->GetInstanceBoneTransformWS(InstanceIndex, ShapeData.BoneIndex);
			FVector P0 = BT.TransformPosition(ShapeData.A);
			FVector P1 = BT.TransformPosition(ShapeData.B);
			FVector C = FMath::Lerp(P0, P1, 0.5);
			float R = BT.GetScale3D().GetAbsMax() * ShapeData.Radius;
			auto HH = FVector::Distance(P0, P1) * 0.5 + R;
			FQuat Rot = FRotationMatrix::MakeFromZ((P1 - P0).GetSafeNormal()).ToQuat();
			DrawDebugCapsule(GetWorld(), C, HH, R, Rot, Color);
		}
		for (auto ShapeData : MeshDef.CompactPhysicsAsset.Boxes) //draw boxes
		{
			FTransform BoxT(ShapeData.Rotation, ShapeData.Center);
			FTransform BT = (FTransform)this->GetInstanceBoneTransformWS(InstanceIndex, ShapeData.BoneIndex);
			FTransform FinalT = BoxT * BT;
			FVector E = FBox(ShapeData.BoxMin, ShapeData.BoxMax).GetExtent() * FinalT.GetScale3D();
			DrawDebugBox(GetWorld(), FinalT.GetLocation(), E, FinalT.GetRotation(), Color);
		}
	}
#endif
}

void ASkelotWorld::DebugDrawInstanceBound(int32 InstanceIndex, float BoundExtentOffset, FColor const& Color, bool bPersistentLines /*= false*/, float LifeTime /*= -1.f*/, uint8 DepthPriority /*= 0*/, float Thickness /*= 0.f*/)
{
#if UE_ENABLE_DEBUG_DRAWING
	if (GetWorld())
	{
		FRenderBounds B = this->CalcInstanceBoundWS(InstanceIndex);
		DrawDebugBox(GetWorld(), FVector(B.GetCenter()), FVector(B.GetExtent()) + BoundExtentOffset, FQuat::Identity, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
	}
#endif
}

void ASkelotWorld::SetInstancelifespan(FSkelotInstanceHandle H, float Lifespan)
{
	if (Lifespan < 0)
		LifeSpanMap.Remove(H);
	else
		LifeSpanMap.FindOrAdd(H, GetWorld()->GetTimeSeconds() + Lifespan);
}

void ASkelotWorld::ClearInstancelifespan(FSkelotInstanceHandle H)
{
	LifeSpanMap.Remove(H);
}

void ASkelotWorld::TickLifeSpans()
{
	for (auto Iter = LifeSpanMap.CreateIterator(); Iter; ++Iter)
	{
		if (!IsHandleValid(Iter->Key))
		{
			Iter.RemoveCurrent();
		}
		else if (GetWorld()->GetTimeSeconds() >= Iter->Value)
		{
			DestroyInstance(Iter->Key);
			Iter.RemoveCurrent();
		}
	}
}

FSkelotInstanceTimerData* ASkelotWorld::Internal_SetInstanceTimer(FSkelotInstanceHandle H, float Interval, bool bLoop, bool bGameTime)
{
	if (Interval > 0)
	{
		FSkelotInstanceTimerData& TimerData = TimerMap.FindOrAdd(H);
		TimerData.bLoop = bLoop;
		TimerData.Interval = Interval;
		TimerData.TimeIndex = bGameTime ? 1 : 0;
		TimerData.FireTime = (bGameTime ? GetWorld()->GetTimeSeconds() : GetWorld()->GetRealTimeSeconds()) + Interval;
		return &TimerData;
	}
	else
	{
		TimerMap.Remove(H);
		return nullptr;
	}
}

void ASkelotWorld::ClearInstanceTimer(FSkelotInstanceHandle H)
{
	TimerMap.Remove(H);
}

void ASkelotWorld::SetInstanceTimer(FSkelotInstanceHandle H, float Interval, bool bLoop, bool bGameTime, FSkelotGeneralDelegate Delegate, FName PayloadTag /*= FName()*/, UObject* PayloadObject /*= nullptr*/)
{
	FSkelotInstanceTimerData* TD = Internal_SetInstanceTimer(H, Interval, bLoop, bGameTime);
	if (TD)
	{
		TD->Data.Delegate.Set<FSkelotGeneralDelegate>(Delegate);
		TD->Data.PayloadTag = PayloadTag;
		TD->Data.PayloadObject = PayloadObject;
	}
}

void ASkelotWorld::TickTimers()
{
	TArray<TPair<FSkelotInstanceHandle, FSkelotInstanceGeneralDelegate>, TInlineAllocator<64>> Calls;

	const double CurTimes[] = { GetWorld()->GetRealTimeSeconds(), GetWorld()->GetTimeSeconds() };

	for (auto Iter = TimerMap.CreateIterator(); Iter; ++Iter)
	{
		auto CurTime = CurTimes[Iter->Value.TimeIndex];

		if (!IsHandleValid(Iter->Key))
		{
			Iter.RemoveCurrent();
		}
		else if (CurTime >= Iter->Value.FireTime)
		{
			Calls.Add(MakeTuple(Iter->Key, MoveTemp(Iter->Value.Data)));

			if (!Iter->Value.bLoop)
			{
				Iter.RemoveCurrent();
			}
			else
			{
				//set next invoke time
				auto Exceed = CurTime - Iter->Value.FireTime;
				Iter->Value.FireTime = CurTime + Iter->Value.Interval - Exceed;
			}

		}
	}

	for (auto Elem : Calls)
	{
		if (FSkelotGeneralDynamicDelegate* DynDlg = Elem.Value.Delegate.TryGet<FSkelotGeneralDynamicDelegate>()) //is it dynamic delegate ?
		{
			DynDlg->ExecuteIfBound(Elem.Key, Elem.Value.PayloadTag, Elem.Value.PayloadObject);
		}
		else //then normal delegate
		{
			Elem.Value.Delegate.Get<FSkelotGeneralDelegate>().ExecuteIfBound(Elem.Key, Elem.Value.PayloadTag, Elem.Value.PayloadObject);
		}
	}
}


FRenderBounds ASkelotWorld::CalcLocalBound(int32 InstanceIndex) const
{
	const FSkelotInstanceRenderDesc& Desc = GetInstanceDesc(InstanceIndex);
	FBoxMinMaxFloat InstanceLocalBound(ForceInit);

	for (int32 MeshDefIndex : Desc.CachedMeshDefIndices)
	{
		const FSkelotMeshDef& MeshDef = Desc.AnimCollection->Meshes[MeshDefIndex];
		if (SOA.CurAnimFrames[InstanceIndex] < MeshDef.BoundsView.Num())
		{
			InstanceLocalBound.Add(MeshDef.BoundsView[SOA.CurAnimFrames[InstanceIndex]]);
		}
		else
		{
			InstanceLocalBound.Add(MeshDef.MaxBBox);
		}
		//#TODO shouldn't dynamic pose enabled get bound from somewhere ?
	}

	check(!InstanceLocalBound.IsForceInitValue());
	return InstanceLocalBound.ToRenderBounds();
}

FRenderBounds ASkelotWorld::CalcInstanceBoundWS(int32 InstanceIndex)
{
	FRenderBounds LocalBound = this->CalcLocalBound(InstanceIndex);
	return LocalBound.TransformBy(GetInstanceTransform(InstanceIndex).ToMatrixWithScale());
}

ASkelotWorld* ASkelotWorld::Get(const UObject* Context, bool bCreateIfNotFound)
{
	return USkelotWorldSubsystem::Internal_GetSingleton(Context, bCreateIfNotFound);
}

ASkelotWorld* ASkelotWorld::Get(const UWorld* World, bool bCreateIfNotFound /*= true*/)
{
	return USkelotWorldSubsystem::Internal_GetSingleton(World, bCreateIfNotFound);
}

ASkelotWorld::ASkelotWorld(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HandleAllocator(true),
	bEnableSpatialGrid(true)
{
	PrimaryActorTick.bStartWithTickEnabled = PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

#if WITH_EDITORONLY_DATA
	this->bListedInSceneOutliner = false;
#endif

	AnimationPlayRate = 1;

	if (!IsTemplate())
	{
		const USkelotDeveloperSettings* Settings = GetDefault<USkelotDeveloperSettings>();
		MaxSubmeshPerInstance = Settings->MaxSubmeshPerInstance;
		ClusterMode = Settings->ClusterMode;

		Impl()->IncreaseSOAs();
	}
}


ASkelotWorld::~ASkelotWorld()
{

}

void Skelot_OnWorldPreSendAllEndOfFrameUpdates(UWorld* InWorld)
{
	USkelotWorldSubsystem* SubSys = InWorld->GetSubsystem<USkelotWorldSubsystem>();
	if (SubSys && SubSys->PrimaryInstance)
	{
		SubSys->PrimaryInstance->PreSendEndOfFrame();
	}
}

void Skelot_OnWorldTickStart(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	
	USkelotWorldSubsystem* SubSys = InWorld->GetSubsystem<USkelotWorldSubsystem>();
	if (SubSys && SubSys->PrimaryInstance)
	{
		SubSys->PrimaryInstance->OnWorldTickStart(TickType, DeltaSeconds);
	}
}

void Skelot_OnWorldPreActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	USkelotWorldSubsystem* SubSys = InWorld->GetSubsystem<USkelotWorldSubsystem>();
	if (SubSys && SubSys->PrimaryInstance)
	{
		SubSys->PrimaryInstance->OnWorldPreActorTick(TickType, DeltaSeconds);

	}
}

void Skelot_OnWorldPostActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	USkelotWorldSubsystem* SubSys = InWorld->GetSubsystem<USkelotWorldSubsystem>();
	if (SubSys && SubSys->PrimaryInstance)
	{
		SubSys->PrimaryInstance->OnWorldPostActorTick(TickType, DeltaSeconds);
	}
}


void Skelot_OnWorldTickEnd(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	USkelotWorldSubsystem* SubSys = InWorld->GetSubsystem<USkelotWorldSubsystem>();
	if (SubSys && SubSys->PrimaryInstance)
	{
		SubSys->PrimaryInstance->OnWorldTickEnd(TickType, DeltaSeconds);
	}
}

void ASkelotWorld::PreSendEndOfFrame()
{

	
}

void ASkelotWorld::OnWorldTickStart(ELevelTick TickType, float DeltaSeconds)
{

}

void ASkelotWorld::OnWorldTickEnd(ELevelTick TickType, float DeltaSeconds)
{

}

void ASkelotWorld::OnWorldPreActorTick(ELevelTick TickType, float DeltaSeconds)
{
	SKELOT_SCOPE_CYCLE_COUNTER(OnWorldPreActorTick);

	Internal_CallOnAnimationFinished();
	

	if (GetNumInstance() == 0)
		return;

	for (int32 InstanceIndex = 0; InstanceIndex < GetNumInstance(); InstanceIndex++)
		SOA.Slots[InstanceIndex].bCreatedThisFrame = false;

	FMemory::Memcpy(SOA.PrevLocations.GetData(), SOA.Locations.GetData(), SOA.Locations.GetTypeSize() * GetNumInstance());
	FMemory::Memcpy(SOA.PrevRotations.GetData(), SOA.Rotations.GetData(), SOA.Rotations.GetTypeSize() * GetNumInstance());
	FMemory::Memcpy(SOA.PrevScales.GetData()   , SOA.Scales.GetData()   , SOA.Scales.GetTypeSize()    * GetNumInstance());

	FMemory::Memcpy(SOA.PreAnimFrames.GetData(), SOA.CurAnimFrames.GetData(), SOA.CurAnimFrames.GetTypeSize() * GetNumInstance());

	Impl()->UpdateAnimations(DeltaSeconds);
	Impl()->TickTimers();
	Impl()->ConsumeRootMotions();


	OnWorldPreActorTick_End.ExecuteIfBound(this, TickType, DeltaSeconds);
	
	Internal_CallOnAnimationNotify();
}

void ASkelotWorld::OnWorldPostActorTick(ELevelTick TickType, float DeltaSeconds)
{
	SKELOT_SCOPE_CYCLE_COUNTER(OnWorldPostActorTick);

	GSkelot_InvClusterCellSize = GSkelot_ClusterCellSize > 0 ? (1.0f / GSkelot_ClusterCellSize) : 0;

	TickLifeSpans();

	Impl()->UpdateHierarchyTransforms(DeltaSeconds);
	Impl()->CalculateBounds(DeltaSeconds);
	Impl()->UpdateDeterminant(DeltaSeconds);
	Impl()->UpdateClusters();
	Impl()->UpdateFlush(DeltaSeconds);
	Impl()->FillDynamicPoseFromComponents();
}

UMaterialInterface* FSkelotSubMeshData::GetMaterial(int32 Index) const
{
	if (OverrideMaterials.IsValidIndex(Index) && OverrideMaterials[Index])
		return OverrideMaterials[Index];

	const TArray<FSkeletalMaterial>& Mats = Mesh->GetMaterials();
	return Mats.IsValidIndex(Index) ? Mats[Index].MaterialInterface : nullptr;
}

UMaterialInterface* FSkelotMeshRenderDesc::GetMaterial(int32 Index) const
{
	if (OverrideMaterials.IsValidIndex(Index) && OverrideMaterials[Index])
		return OverrideMaterials[Index];

	const TArray<FSkeletalMaterial>& Mats = Mesh->GetMaterials();
	return Mats.IsValidIndex(Index) ? Mats[Index].MaterialInterface : nullptr;
}


uint32 FSkelotInstanceRenderDesc::ComputeHash() const
{
	uint32 H =
	GetTypeHash(AnimCollection) ^
	GetTypeHash(NumCustomDataFloat) ^
// 	GetTypeHash(MinDrawDistance) ^
// 	GetTypeHash(MaxDrawDistance) ^
// 	GetTypeHash(LODScale) ^
// 	GetTypeHash(CustomDepthStencilValue) ^
// 	GetTypeHash(BoundsScale) ^
// 	GetTypeHash(CustomDepthStencilWriteMask) ^
	GetTypeHash(GetPackedFlags()) ^
	Seed;

	return H;
}

bool FSkelotInstanceRenderDesc::Equal(const FSkelotInstanceRenderDesc& Other) const
{
	const bool bDataSame = AnimCollection == Other.AnimCollection &&
		NumCustomDataFloat == Other.NumCustomDataFloat &&
		MinDrawDistance.GetValue() == Other.MinDrawDistance.GetValue() &&
		MaxDrawDistance.GetValue() == Other.MaxDrawDistance.GetValue() &&
		LODScale.GetValue() == Other.LODScale.GetValue() &&
		CustomDepthStencilWriteMask == Other.CustomDepthStencilWriteMask &&
		CustomDepthStencilValue == Other.CustomDepthStencilValue &&
		BoundsScale == Other.BoundsScale &&
		Seed == Other.Seed &&
		GetPackedFlags() == Other.GetPackedFlags();

	bool bLODEqual = FMemory::Memcmp(LODDistances, Other.LODDistances, sizeof(LODDistances)) == 0;

	return (bDataSame && bLODEqual) && Meshes == Other.Meshes;
}

void FSkelotInstanceRenderDesc::AddAllMeshes()
{
	if (AnimCollection)
	{
		for (const FSkelotMeshDef& MD : AnimCollection->Meshes)
		{
			AddMesh(MD.Mesh);
		}
	}
}


FSkelotInstanceRenderDesc::FSkelotInstanceRenderDesc()
{
	FMemory::Memzero(*this);

	LODScale = 1;
	bCastDynamicShadow = true;
	bRenderInMainPass = true;
	bRenderInDepthPass = true;
	bReceivesDecals = true;
	bUseAsOccluder = true;
	BoundsScale = 1;
	LightingChannels = FLightingChannels();
	Seed = 0;

	LODDistances[0] = 1000;
	LODDistances[1] = 3000;
	LODDistances[2] = 8000;
	LODDistances[3] = 14000;
	LODDistances[4] = 20000;
	LODDistances[5] = 40000;
	LODDistances[6] = 80000;
}

void FSkelotInstanceRenderDesc::CacheMeshDefIndices()
{
	CachedMeshDefIndices.Reset();
	CachedMeshDefIndices.Init(-1, Meshes.Num());

	if(AnimCollection)
	{
		for (int32 SubMeshIdx = 0; SubMeshIdx < Meshes.Num(); SubMeshIdx++)
		{
			CachedMeshDefIndices[SubMeshIdx] = AnimCollection->FindMeshDef(Meshes[SubMeshIdx].Mesh);
		}
	}
}

void FSkelotInstanceRenderDesc::AddMesh(USkeletalMesh* InMesh)
{
	if(InMesh)
	{
		int32 CurIdx = IndexOfMesh(InMesh);
		if (CurIdx == -1)
		{
			Meshes.AddDefaulted_GetRef().Mesh = InMesh;
			CachedMeshDefIndices.Reset();
		}
		
	}
}

void FSkelotInstanceRenderDesc::RemoveMesh(USkeletalMesh* InMesh)
{
	int32 CurIdx = IndexOfMesh(InMesh);
	if (CurIdx != -1)
	{
		Meshes.RemoveAt(CurIdx);
		CachedMeshDefIndices.Reset();
	}
}



#if WITH_EDITOR
void USkelotRenderParams::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif




bool FSkelotMeshRenderDesc::operator==(const FSkelotMeshRenderDesc& Other) const
{
	return Mesh == Other.Mesh && OverrideMaterials == Other.OverrideMaterials && LODScreenSizeScale == Other.LODScreenSizeScale;
}



uint32 FSkelotInstanceRenderDesc::GetPackedFlags() const
{
	uint32 Flags = 0;
	Flags |= bPerInstanceLocalBounds		? (1 <<  0) : 0;
	Flags |= bIsNegativeDeterminant			? (1 <<  1) : 0;
	Flags |= bRenderInMainPass				? (1 <<  2) : 0;
	Flags |= bRenderInDepthPass				? (1 <<  3) : 0;
	Flags |= bReceivesDecals				? (1 <<  4) : 0;
	Flags |= bUseAsOccluder					? (1 <<  5) : 0;
	Flags |= bCastDynamicShadow				? (1 <<  6) : 0;
	Flags |= bRenderCustomDepth				? (1 <<  7) : 0;
	Flags |= LightingChannels.bChannel0		? (1 <<  8) : 0;
	Flags |= LightingChannels.bChannel1		? (1 <<  9) : 0;
	Flags |= LightingChannels.bChannel2		? (1 << 10) : 0;
	return Flags;
}


bool USkelotRenderParams::MeshesShouldBeExcluded(const FAssetData& AssetData) const
{
	return Data.AnimCollection && !Data.AnimCollection->IsAssetDataSkeletonCompatible(AssetData);
}


void USkelotRenderParams::AddSelectedMeshes()
{
#if WITH_EDITOR
	TArray<FAssetData> SelectedAssets;
	IContentBrowserSingleton::Get().GetSelectedAssets(SelectedAssets);

	for (const FAssetData& AD : SelectedAssets)
	{
		if (MeshesShouldBeExcluded(AD))
			continue;

		if (AD.IsInstanceOf<USkeletalMesh>())
		{
			USkeletalMesh* SKMesh = Cast<USkeletalMesh>(AD.GetAsset());
			if(SKMesh && Data.IndexOfMesh(SKMesh) == -1)
			{
				Data.Meshes.AddDefaulted_GetRef().Mesh = SKMesh;
			}
		}
	}
#endif
}



#undef private


