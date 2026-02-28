#include "SkelotClusterComponent.h"
#include "SkelotWorld.h"

#include "SkelotPrimitiveProxy.h"
#include "Containers/StaticBitArray.h"


USKelotClusterComponent::USKelotClusterComponent()
{
	bComputeFastLocalBounds = false;
	this->bCastStaticShadow = false;
	this->bCanEverAffectNavigation = false;
	//this->bForceMipStreaming = true;
}



FSkelotInstanceRenderDesc& USKelotClusterComponent::GetRenderDesc() const
{
	ASkelotWorld* SKWorld = static_cast<ASkelotWorld*>(GetOwner());
	return SKWorld->RenderDescs.Get(DescId);
}

FSkelotCluster& USKelotClusterComponent::GetClusterStruct() const
{
	ASkelotWorld* SKWorld = static_cast<ASkelotWorld*>(GetOwner());
	return SKWorld->RenderDescs.Get(DescId).Clusters.Get(ClusterId);
}

ASkelotWorld* USKelotClusterComponent::GetSkelotWorld() const
{
	return GetOwner<ASkelotWorld>();
}

FBoxSphereBounds USKelotClusterComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return ComputeBoundWS();
}

FBoxSphereBounds USKelotClusterComponent::ComputeBoundWS() const
{
	FSkelotCluster& ClusterRef = GetClusterStruct();
	if (ClusterRef.Instances.Num() == 0)
		return FBoxSphereBounds(ForceInit);

	FBox LocalBound = CalcApproximateLocalBound();
	LocalBound.IsValid = true;

	FBox Bound(ForceInit);

	const ASkelotWorld* SKW = GetSkelotWorld();
	for (int32 InstanceIdx : ClusterRef.Instances)
	{
		Bound += LocalBound.TransformBy(SKW->GetInstanceTransform(InstanceIdx));
	}

	FBoxSphereBounds BS(Bound);
	BS.SphereRadius *= this->BoundsScale;
	BS.BoxExtent *= this->BoundsScale;

	return BS;
}

FBox USKelotClusterComponent::CalcApproximateLocalBound() const
{
	FBox LocalBound(ForceInit);

	USkelotAnimCollection* AC = GetRenderDesc().AnimCollection;
	if (AC)
	{
		for (const FSkelotMeshRenderDesc& MeshDesc : GetRenderDesc().Meshes)
		{
			if (MeshDesc.Mesh)
			{
				int32 MeshDefIdx = AC->FindMeshDef(MeshDesc.Mesh);
				if (MeshDefIdx != -1)
				{
					LocalBound += FBox(AC->Meshes[MeshDefIdx].MaxBBox.ToBox());
				}
			}
		}

		if (LocalBound.IsValid)
			return LocalBound;

		return FBox(AC->MeshesBBox.GetFBox());
	}

	return LocalBound;
}


float USKelotClusterComponent::GetAverageSmallestBoundRadius() const
{
	int32 Counter = 0;
	float Sums = 0;
	USkelotAnimCollection* AC = GetRenderDesc().AnimCollection;
	for (const FSkelotMeshRenderDesc& MeshDesc : GetRenderDesc().Meshes)
	{
		if (MeshDesc.Mesh)
		{
			int32 MeshDefIdx = AC->FindMeshDef(MeshDesc.Mesh);
			if (MeshDefIdx != -1)
			{
				Counter++;
				Sums += AC->Meshes[MeshDefIdx].SmallestBoundRadius;
			}
		}
	}

	if (Counter != 0)
		return Sums / Counter;

	return 0;
}



void USKelotClusterComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);

	//UE_LOGFMT(LogSkelot, VeryVerbose, "CreateRenderState_Concurrent At {1}", GFrameNumber);
	//SendRenderDynamicData_Concurrent();
}

void USKelotClusterComponent::PostLoad()
{
	Super::PostLoad();
}

void USKelotClusterComponent::BeginDestroy()
{
	Super::BeginDestroy();
}

void USKelotClusterComponent::BeginPlay()
{
	Super::BeginPlay();
}


void USKelotClusterComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
}

int32 USKelotClusterComponent::GetNumMaterials() const
{
	int32 Counter = 0;
	for (const FSkelotMeshRenderDesc& MeshDesc : GetRenderDesc().Meshes)
	{
		if (MeshDesc.Mesh)
		{
			Counter += MeshDesc.Mesh->GetMaterials().Num();
		}
	}
	return Counter;
}

UMaterialInterface* USKelotClusterComponent::GetMaterial(int32 MaterialIndex) const
{
	for (const FSkelotMeshRenderDesc& MeshDesc : GetRenderDesc().Meshes)
	{
		if (MeshDesc.Mesh)
		{
			const TArray<FSkeletalMaterial>& Mats = MeshDesc.Mesh->GetMaterials();
			if (MaterialIndex >= Mats.Num())
			{
				MaterialIndex -= Mats.Num();
			}
			else
			{
				return MeshDesc.GetMaterial(MaterialIndex);
			}
		}
	}

	return nullptr;
}

int32 USKelotClusterComponent::GetMaterialIndex(FName MaterialSlotName) const
{
	int32 Counter = 0;
	for (const FSkelotMeshRenderDesc& MeshDesc : GetRenderDesc().Meshes)
	{
		if (MeshDesc.Mesh)
		{
			const TArray<FSkeletalMaterial>& SkeletalMeshMaterials = MeshDesc.Mesh->GetMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMeshMaterials.Num(); ++MaterialIndex)
			{
				const FSkeletalMaterial& SkeletalMaterial = SkeletalMeshMaterials[MaterialIndex];
				if (SkeletalMaterial.MaterialSlotName == MaterialSlotName)
					return Counter + MaterialIndex;
			}
			Counter += SkeletalMeshMaterials.Num();
		}
	}
	return INDEX_NONE;
}

TArray<FName> USKelotClusterComponent::GetMaterialSlotNames() const
{
	TArray<FName> MaterialNames;
	for (const FSkelotMeshRenderDesc& MeshDesc : GetRenderDesc().Meshes)
	{
		if (MeshDesc.Mesh)
		{
			const TArray<FSkeletalMaterial>& SkeletalMeshMaterials = MeshDesc.Mesh->GetMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMeshMaterials.Num(); ++MaterialIndex)
			{
				const FSkeletalMaterial& SkeletalMaterial = SkeletalMeshMaterials[MaterialIndex];
				MaterialNames.Add(SkeletalMaterial.MaterialSlotName);
			}
		}
	}

	return MaterialNames;
}

bool USKelotClusterComponent::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	return GetMaterialIndex(MaterialSlotName) >= 0;
}



bool USKelotClusterComponent::ShouldCreateRenderState() const
{
	if (!Super::ShouldCreateRenderState())
		return false;

	if (!DescId.IsValidId())
		return false;

	if (!GetRenderDesc().AnimCollection /*|| !GetRenderDesc().AnimCollection->bIsBuilt*/)
		return false;

	return true;
}

FPrimitiveSceneProxy* USKelotClusterComponent::CreateSceneProxy()
{
	if (ShouldCreateRenderState())
		return new FSkelotClusterProxy(this, GetFName());

	return nullptr;
}

void USKelotClusterComponent::SendRenderDynamicData_Concurrent()
{
	SKELOT_SCOPE_CYCLE_COUNTER(SendRenderDynamicData_Concurrent);

	Super::SendRenderDynamicData_Concurrent();
	
	//SendRenderTransform_Concurrent();

	FSkelotClusterProxy* Proxy = static_cast<FSkelotClusterProxy*>(this->SceneProxy);
	if (!Proxy && !DescId.IsValidId())
		return;

	const USkelotAnimCollection* AnimCollection = GetRenderDesc().AnimCollection;
	if (!AnimCollection)
		return;

	ASkelotWorld* SKWorld = this->GetSkelotWorld();

	FSkelotCompDynData* DynData = new FSkelotCompDynData();
	FSkelotCluster& ClusterRef = GetClusterStruct();

	this->UpdateBounds();

	DynData->Center = this->GetComponentLocation();
	DynData->PrevCenter = DynData->Center;

	DynData->LocalBounds = this->CalcApproximateLocalBound();
	DynData->WorldBounds = this->Bounds;
	DynData->ComponentProxy = Proxy;

	DynData->Transforms.SetNumUninitialized(ClusterRef.Instances.Num());
	DynData->PrevTransforms.SetNumUninitialized(ClusterRef.Instances.Num());

	const int32 DescNumFloatPerInstance = GetRenderDesc().NumCustomDataFloat;
	const int32 PayloadNumFloat4PerInstance = FMath::DivideAndRoundUp(DescNumFloatPerInstance + 2, 4);
	const int32 PayloadNumFloatPerInstance = PayloadNumFloat4PerInstance * 4;
	DynData->CustomData.SetNumZeroed(ClusterRef.Instances.Num() * PayloadNumFloatPerInstance);

	const bool bWithPerInstanceLocalBounds = (GetRenderDesc().bPerInstanceLocalBounds || GSkelot_ForcePerInstanceLocalBounds) && !Proxy->bLegacyRender;

	if (bWithPerInstanceLocalBounds)
	{
		DynData->PerInstanceLocalBounds.SetNumUninitialized(ClusterRef.Instances.Num());
	}

	//make instance runs again if needed ------------------------------------------------------------------
	if (ClusterRef.bAnyAddRemove)
	{
		const uint32 NumSubMesh = GetRenderDesc().Meshes.Num();

		//count how many times a sub mesh is being used  ---------------
		uint32 SubMeshCounts[64] = { };
		for (int32 InstanceIdx : ClusterRef.Instances)
		{
			for (uint8* SMIIter = SKWorld->GetInstanceSubmeshIndices(InstanceIdx); *SMIIter != 0xFF; SMIIter++)
			{
				SubMeshCounts[*SMIIter]++;
			}
		}

		for (uint32& Count : SubMeshCounts)
			Count *= 1;

		ClusterRef.SortedInstances.SetNumUninitialized(ClusterRef.Instances.Num());

		TArray<FInstanceIndexAndSortKey> TmpInstances;
		TmpInstances.SetNumUninitialized(ClusterRef.Instances.Num());

		//find out how important are instances in terms of batching 
		for (int32 DrawIdx = 0; DrawIdx < ClusterRef.Instances.Num(); DrawIdx++)
		{
			const int32 InstanceIdx = ClusterRef.Instances[DrawIdx];
			TmpInstances[DrawIdx].Index = InstanceIdx;
			TmpInstances[DrawIdx].Value = 0;

			for (uint8* SMIIter = SKWorld->GetInstanceSubmeshIndices(InstanceIdx); *SMIIter != 0xFF; SMIIter++)
			{
				TmpInstances[DrawIdx].Value += SubMeshCounts[*SMIIter];
			}
		}

		RadixSort32(ClusterRef.SortedInstances.GetData(), TmpInstances.GetData(), ClusterRef.Instances.Num(), [](FInstanceIndexAndSortKey In) { return In.Value; });

		ClusterRef.InstanceRunRanges.Reset();
		ClusterRef.InstanceRunRanges.SetNum(NumSubMesh);

		struct FRangeData
		{
			int32 StartIdx = -1;
		};
		FRangeData RangesData[0xFF];

		for (int32 ItemIdx = 0; ItemIdx < ClusterRef.SortedInstances.Num(); ItemIdx++)
		{
			bool HasSMI[0xff] = {};

			for (uint8* SMIIter = SKWorld->GetInstanceSubmeshIndices(ClusterRef.SortedInstances[ItemIdx].Index); *SMIIter != 0xFF; SMIIter++)
			{
				const uint8 SMI = *SMIIter;
				HasSMI[SMI] = true;
				if (RangesData[SMI].StartIdx == -1)
				{
					RangesData[SMI].StartIdx = ItemIdx; // Start a new range
				}
			}

			for (uint32 SMI = 0; SMI < NumSubMesh; SMI++)
			{
				if (!HasSMI[SMI])
				{
					if (RangesData[SMI].StartIdx != -1)
					{
						// End the current range
						ClusterRef.InstanceRunRanges[SMI].Add(FInstanceRunData{ RangesData[SMI].StartIdx, ItemIdx - 1 });
						RangesData[SMI].StartIdx = -1;
					}
				}
			}
		}

		for (uint32 SMI = 0; SMI < NumSubMesh; SMI++)
		{
			// Handle case where a range extends to the end of the array
			if (RangesData[SMI].StartIdx != -1)
			{
				ClusterRef.InstanceRunRanges[SMI].Add(FInstanceRunData{ RangesData[SMI].StartIdx, ClusterRef.SortedInstances.Num() - 1 });
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)	//lets find out average instance count of sub meshes
		ClusterRef.InstanceRun_AvgInstanceCount.Reset();
		ClusterRef.InstanceRun_AvgInstanceCount.SetNumZeroed(NumSubMesh);

		for (uint32 SubMeshIdx = 0; SubMeshIdx < NumSubMesh; SubMeshIdx++)
		{
			if (ClusterRef.InstanceRunRanges[SubMeshIdx].Num() != 0)
			{
				for (const FInstanceRunData& Range : ClusterRef.InstanceRunRanges[SubMeshIdx])
					ClusterRef.InstanceRun_AvgInstanceCount[SubMeshIdx] += Range.Num();

				ClusterRef.InstanceRun_AvgInstanceCount[SubMeshIdx] /= ClusterRef.InstanceRunRanges[SubMeshIdx].Num();
			}
		}
#endif

	}

	check(ClusterRef.Instances.Num() == ClusterRef.SortedInstances.Num());
	//copy the instance run ranges
	DynData->InstanceRuns = ClusterRef.InstanceRunRanges;
	

	//fill instances, always happens -------------------------------------------------
	for (int32 RenderIndex = 0; RenderIndex < ClusterRef.SortedInstances.Num(); RenderIndex++)
	{
		const int32 InstanceIndex = ClusterRef.SortedInstances[RenderIndex].Index;

		//set animation frame indices to float[0] and float[1]
		FSkelotInstancesSOA::FAnimFrame* PayloadFrame = (FSkelotInstancesSOA::FAnimFrame*)&DynData->CustomData[RenderIndex * PayloadNumFloatPerInstance];
		PayloadFrame->Cur = SKWorld->SOA.CurAnimFrames[InstanceIndex];
		PayloadFrame->Pre = SKWorld->SOA.PreAnimFrames[InstanceIndex];

		if (GSkelot_ForcedAnimFrameIndex != -1)
			*PayloadFrame = FSkelotInstancesSOA::FAnimFrame{ GSkelot_ForcedAnimFrameIndex, GSkelot_ForcedAnimFrameIndex };

		//copy user floats
		for (int32 i = 0; i < DescNumFloatPerInstance; i++)
		{
			DynData->CustomData[RenderIndex * PayloadNumFloatPerInstance + i + 2] = SKWorld->SOA.PerInstanceCustomData[InstanceIndex * SKWorld->SOA.MaxNumCustomDataFloat + i];
		}

		FTransform InsT = SKWorld->GetInstanceTransform(InstanceIndex);
		//InsT.SetLocation(InsT.GetLocation() - DynData->Center);
		DynData->Transforms[RenderIndex] = FRenderTransform(InsT.ToMatrixWithScale());

		if (!SKWorld->SOA.Slots[InstanceIndex].bCreatedThisFrame)
		{
			FTransform PrevInsT = SKWorld->GetInstancePrevTransform(InstanceIndex);
			//PrevInsT.SetLocation(PrevInsT.GetLocation() - DynData->PrevCenter);
			DynData->PrevTransforms[RenderIndex] = FRenderTransform(PrevInsT.ToMatrixWithScale());
		}
		else
		{
			DynData->PrevTransforms[RenderIndex] = DynData->Transforms[RenderIndex];
		}

		if (bWithPerInstanceLocalBounds)
		{
			DynData->PerInstanceLocalBounds[RenderIndex] = SKWorld->CalcLocalBound(InstanceIndex);
		}
	}
	
		


	ClusterRef.bAnyAddRemove = false;

	ENQUEUE_RENDER_COMMAND(SendInstances)([DynData](FRHICommandListBase&) mutable {
		DynData->ComponentProxy->UpdateInstanceBuffer(DynData);
	});
}

void USKelotClusterComponent::SendRenderTransform_Concurrent()
{
	Super::SendRenderTransform_Concurrent();


}

