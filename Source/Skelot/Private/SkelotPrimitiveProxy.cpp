#include "SkelotPrimitiveProxy.h"
#include "BatchGenerator.h"


void Skelot_PreRenderFrame(class FRDGBuilder&)
{
}

void Skelot_PostRenderFrame(class FRDGBuilder&)
{
}


void FSkelotCluserISDB::Init(FSkelotClusterProxy* Proxy)
{
	this->Flags.bHasPerInstanceDynamicData = true;
	this->Flags.bHasPerInstanceLocalBounds = Proxy->bWithPerInstanceLocalBound;
	//this->Flags.bHasPerInstanceRandom = true;
	//this->Flags.bHasPerInstanceCustomData = true;
	//this->Flags.bHasPerInstancePayloadExtension = true;
	this->Flags.bHasPerInstanceCustomData = true;
}

FSkelotClusterProxy::FSkelotClusterProxy(const USKelotClusterComponent* Component, FName ResourceName) : FPrimitiveSceneProxy(Component, ResourceName)
, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
, AnimCollection(Component->GetRenderDesc().AnimCollection)
, bWithPerInstanceLocalBound(Component->GetRenderDesc().bPerInstanceLocalBounds)
, bRenderDescNegativeDeterminant(Component->GetRenderDesc().bIsNegativeDeterminant)
, NumCustomDataFloat(Component->GetRenderDesc().NumCustomDataFloat)
, Materials(Component->GetMaterials())
{
	const FSkelotInstanceRenderDesc& RnDesc = Component->GetRenderDesc();
	const bool bUseGPUScene = UseGPUScene(GetScene().GetShaderPlatform(), GetScene().GetFeatureLevel());
	const bool bMobilePath = (GetScene().GetFeatureLevel() == ERHIFeatureLevel::ES3_1);

	this->bLegacyRender = SKELOT_WITH_GPUSCENE == 0 || !bUseGPUScene || bMobilePath;
	
	this->bCanSkipRedundantTransformUpdates = false;
	this->bAlwaysHasVelocity = true;
	this->bHasDeformableMesh = true;
	this->bGoodCandidateForCachedShadowmap = false;
	this->bHasWorldPositionOffsetVelocity = true; //velocity wont work without it :thinking:
	this->bVFRequiresPrimitiveUniformBuffer = bLegacyRender;
	this->bDoesMeshBatchesUseSceneInstanceCount = false; // !bLegacyRender;
	this->bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = false;
	this->bSupportsGPUScene = !bLegacyRender;
	this->bSupportsParallelGDME = !bLegacyRender;
	this->bSupportsParallelGDRTI = !bLegacyRender;

	if (!this->bLegacyRender)
	{
		this->SetupInstanceSceneDataBuffers(&InstanceBuffer);
		//const int32 NumCustomDataFloats = Align(Component->GetRenderDesc().NumCustomDataFloat + 2, 4);
		InstanceBuffer.Init(this);
	}

	for (const FSkelotMeshRenderDesc& MeshDesc : RnDesc.Meshes)
	{
		FProxyMeshData& ProxyMD = Meshes.AddDefaulted_GetRef();
		ProxyMD.SKMesh = MeshDesc.Mesh;
		ProxyMD.LODScreenSizeScale = MeshDesc.LODScreenSizeScale;
		ProxyMD.MeshDefIdx = AnimCollection->FindMeshDef(MeshDesc.Mesh);
		ProxyMD.SKRenderData = MeshDesc.Mesh->GetResourceForRendering();
		ProxyMD.MinLODIndex = 0;
		if (ProxyMD.MeshDefIdx != -1)
		{
			ProxyMD.MinLODIndex = FMath::Max(AnimCollection->Meshes[ProxyMD.MeshDefIdx].BaseLOD, (uint8)MeshDesc.Mesh->GetMinLodIdx());
		}
	}

	ApproximateLocalBounds = Component->CalcApproximateLocalBound();
	float LODScale = RnDesc.LODScale.GetValue();
	GPULODRadius = ApproximateLocalBounds.GetExtent().Size() * LODScale; //Component->GetAverageSmallestBoundRadius();

	DistanceScale = LODScale;
	//copy LOD distances
	for (int i = 0; i < SKELOT_MAX_LOD - 1; i++)
		this->LODDistances[i] = RnDesc.LODDistances[i];

	for (int i = 0; i < Materials.Num(); i++)
	{
		UMaterialInterface*& Material = Materials[i];
		
		if (Material)
		{
			if (!Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh) || !Material->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes))
			{
				UE_LOG(LogSkelot, Error, TEXT("Material %s with missing usage flag was applied to %s"), *Material->GetName(), *Component->GetName());
				//Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			FMaterialRenderProxy* RP = Material->GetRenderProxy();
			check(RP);
		}
		else
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}
}

FPrimitiveViewRelevance FSkelotClusterProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.SkeletalMeshes != 0;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = this->bLegacyRender || !UE_BUILD_SHIPPING || !UE_BUILD_TEST || HasViewDependentDPG();
	Result.bStaticRelevance = !this->bLegacyRender;

	Result.bRenderInDepthPass = ShouldRenderInDepthPass();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque == 1 && Result.bRenderInMainPass == 1;
	return Result;
}

SIZE_T FSkelotClusterProxy::GetTypeHash() const
{
	static SIZE_T UniquePointer;
	return reinterpret_cast<SIZE_T>(&UniquePointer);
}

void FSkelotClusterProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SKELOT_SCOPE_CYCLE_COUNTER(GetDynamicMeshElements);
	
	if (!DynData)
		return;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());

			if (bLegacyRender)
			{
				bool bIgnoreView = View->GetDynamicMeshElementsShadowCullFrustum() == nullptr && (View->bIsInstancedStereoEnabled && View->StereoPass == EStereoscopicPass::eSSP_SECONDARY);
				if (bIgnoreView)
					continue;

				FSkelotBatchGenerator generator;
				generator.Proxy = this;
				generator.bShaddowCollector = View->GetDynamicMeshElementsShadowCullFrustum() != nullptr;
				generator.View = View;
				generator.Collector = &Collector;
				generator.ViewFamily = &ViewFamily;
				generator.ViewIndex = ViewIndex;
				generator.TotalInstances = DynData->Transforms.Num();

				generator.Start();
			}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (GSkelot_DrawInstanceBounds)
			{
				const int32 InstanceCount = FMath::Min(100000, this->InstanceBuffer.GetNumInstances());
				for (int32 RenderIndex = 0; RenderIndex < InstanceCount; RenderIndex++)
				{
					RenderInstanceBound(RenderIndex, Collector.GetDebugPDI(ViewIndex), View);
				}
			}
#endif
		}
	}
}

void FSkelotClusterProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	check(IsInParallelRenderingThread());
	check(!HasViewDependentDPG());
	

	if (bLegacyRender)
		return;
	
	int32 SubMeshMatOffset = 0;
	
	if (!DynData)
		return;

	//if (DynData && DynData->Transforms.Num() != 0)
	//	InstanceRunTest[1] = DynData->Transforms.Num() - 1;


	//for each sub mesh --------------------------------------------------------------
	for (int32 SubMeshIdx = 0; SubMeshIdx < this->Meshes.Num(); SubMeshIdx++)
	{
		const FProxyMeshData& ProxyMD = this->Meshes[SubMeshIdx];
		if (ProxyMD.MeshDefIdx == -1)
			continue;

		const FSkelotMeshDef& MeshDefStruct = this->AnimCollection->Meshes[ProxyMD.MeshDefIdx];
		if (!MeshDefStruct.Mesh || !MeshDefStruct.MeshData)
			continue;



		const FSkeletalMeshRenderData* SKMRenderData = MeshDefStruct.Mesh->GetResourceForRendering();

		const int32 CurSubMeshMatOffset = SubMeshMatOffset;
		SubMeshMatOffset += MeshDefStruct.Mesh->GetNumMaterials();

		if (DynData->InstanceRuns[SubMeshIdx].Num() == 0)
			continue;

		//for each LOD --------------------------------------------------------------
		for (int32 LODIndex = (int32)ProxyMD.MinLODIndex; LODIndex < SKMRenderData->LODRenderData.Num(); LODIndex++)
		{
			const FSkeletalMeshLODRenderData& SKLODData = SKMRenderData->LODRenderData[LODIndex];
			const FSkeletalMeshLODInfo* SKLODInfo = MeshDefStruct.Mesh->GetLODInfo(LODIndex);
			const float ScreenSize = SKLODInfo->ScreenSize.GetValue();

			//#TODO generate unified depth,shadow mesh if possible

			
			//for each section --------------------------------------------------------------
			for (int32 SectionIndex = 0; SectionIndex < SKLODData.RenderSections.Num(); SectionIndex++)
			{
				const FSkelMeshRenderSection& Section = SKLODData.RenderSections[SectionIndex];

				FMeshBatch Mesh;
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				FillMeshBatch(Mesh, LODIndex, SectionIndex, SKLODData, MeshDefStruct);

				int32 RedirectedMatIndex = RedirectMaterialIndex(*SKLODInfo, SectionIndex, Section.MaterialIndex);

				Mesh.MaterialRenderProxy = Materials[CurSubMeshMatOffset + RedirectedMatIndex]->GetRenderProxy();
				Mesh.DepthPriorityGroup = GetStaticDepthPriorityGroup();

				//init screen sizes
				BatchElement.MaxScreenSize = ScreenSize * ProxyMD.LODScreenSizeScale;
				BatchElement.MinScreenSize = 0;
				const FSkeletalMeshLODInfo* NextSKLODInfo = MeshDefStruct.Mesh->GetLODInfo(LODIndex + 1);
				if (NextSKLODInfo)
					BatchElement.MinScreenSize = NextSKLODInfo->ScreenSize.GetValue() * ProxyMD.LODScreenSizeScale;

				BatchElement.bIsInstanceRuns = true;
				BatchElement.NumInstances = DynData->InstanceRuns[SubMeshIdx].Num();
				BatchElement.InstanceRuns = reinterpret_cast<uint32*>(DynData->InstanceRuns[SubMeshIdx].GetData());
				
				PDI->DrawMesh(Mesh, ScreenSize);


			}
		}


	}
}

void FSkelotClusterProxy::FillMeshBatch(FMeshBatch& Mesh, int32 LODIndex, int32 SectionIndex, const FSkeletalMeshLODRenderData& SKLODData, const FSkelotMeshDef& MeshDefStruct) const
{
	const FSkelMeshRenderSection& Section = SKLODData.RenderSections[SectionIndex];

	FMeshBatchElement& BatchElement = Mesh.Elements[0];

	Mesh.SegmentIndex = SectionIndex;
	Mesh.MeshIdInPrimitive = SectionIndex;
	Mesh.LODIndex = LODIndex;
#if STATICMESH_ENABLE_DEBUG_RENDERING
	Mesh.VisualizeLODIndex = LODIndex;
	BatchElement.VisualizeElementIndex = SectionIndex;
#endif

	Mesh.ReverseCulling = bRenderDescNegativeDeterminant; //this->IsLocalToWorldDeterminantNegative();
	Mesh.bUseAsOccluder = this->CanBeOccluded();
	Mesh.bUseForDepthPass = this->ShouldRenderInDepthPass();
	Mesh.bUseAsOccluder = this->ShouldUseAsOccluder();
	Mesh.CastShadow = bCastDynamicShadow && Section.bCastShadow;

	const int MBI = FMath::Min(Skelot::OverrideMaxBoneInfluence(Section.MaxBoneInfluences), SKELOT_WITH_EXTRA_BONE ? 8 : 4);
	Mesh.VertexFactory = MeshDefStruct.MeshData->LODs[LODIndex - MeshDefStruct.BaseLOD].GetVertexFactory(MBI);

	BatchElement.IndexBuffer = SKLODData.MultiSizeIndexContainer.GetIndexBuffer();
	BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
	BatchElement.FirstIndex = Section.BaseIndex;
	BatchElement.NumPrimitives = Skelot::OverrideNumPrimitive(Section.NumTriangles);
	BatchElement.MinVertexIndex = Section.BaseVertexIndex;
	BatchElement.MaxVertexIndex = SKLODData.GetNumVertices() - 1;

	check(Section.MaxBoneInfluences <= SKELOT_MAX_BONE_INFLUENCE);

	FRHIUniformBuffer* AnimUB = this->AnimCollection->AnimCollectionUB.GetReference();
	BatchElement.UserData = nullptr;
	BatchElement.VertexFactoryUserData = nullptr;

	BatchElement.InstancedLODIndex = LODIndex;
	BatchElement.bForceInstanceCulling = true;
	BatchElement.bFetchInstanceCountFromScene = true;
	BatchElement.bPreserveInstanceOrder = false;
	//BatchElement.NumInstances = 1;
}

void FSkelotClusterProxy::UpdateInstanceBuffer(FSkelotCompDynData* CompDynData)
{
	check(IsInRenderingThread());
	SKELOT_SCOPE_CYCLE_COUNTER(UpdateInstanceBuffer);

	this->DynData = TUniquePtr<FSkelotCompDynData>(CompDynData);

	if (this->bLegacyRender)
	{

	}
	else
	{
		//update GPU scene instance buffer ---

		FInstanceSceneDataBuffers::FAccessTag AT(666);
		FInstanceSceneDataBuffers::FWriteView RW = this->InstanceBuffer.BeginWriteAccess(AT);

		InstanceBuffer.SetPrimitiveLocalToWorld(CompDynData->GetLocalToWorld(this->bRenderDescNegativeDeterminant), AT);
		const int32 Num = CompDynData->Transforms.Num();

		if (Num != 0)
		{
			RW.NumCustomDataFloats = (CompDynData->CustomData.Num() / CompDynData->Transforms.Num());
			check(RW.NumCustomDataFloats % 4 == 0);
		}

		if (this->bWithPerInstanceLocalBound)
		{
			check(CompDynData->PerInstanceLocalBounds.Num() == CompDynData->Transforms.Num());
			RW.InstanceLocalBounds = MoveTemp(CompDynData->PerInstanceLocalBounds);
		}
		else
		{
			RW.InstanceLocalBounds.Reset(1);
			RW.InstanceLocalBounds.Add(CompDynData->LocalBounds);
		}

		RW.InstanceToPrimitiveRelative = MoveTemp(CompDynData->Transforms);
		RW.PrevInstanceToPrimitiveRelative = MoveTemp(CompDynData->PrevTransforms);
		RW.InstanceCustomData = MoveTemp(CompDynData->CustomData);


		this->InstanceBuffer.EndWriteAccess(AT);


	}

	FScene* ScenePtr = this->GetScene().GetRenderScene();
	FPrimitiveSceneInfo* PSI = this->GetPrimitiveSceneInfo();
	ScenePtr->PrimitiveUpdates.Enqueue(PSI, FUpdateTransformCommand{ .WorldBounds = CompDynData->WorldBounds, .LocalBounds = CompDynData->LocalBounds, .LocalToWorld = CompDynData->GetLocalToWorld(bRenderDescNegativeDeterminant), .AttachmentRootPosition = FVector::Zero() });

	ScenePtr->PrimitiveUpdates.Enqueue(PSI, FUpdateOverridePreviousTransformData(CompDynData->GetPrevLocalToWorld(bRenderDescNegativeDeterminant)));
	ScenePtr->PrimitiveUpdates.Enqueue(PSI, FUpdateInstanceCommand{ .PrimitiveSceneProxy = this, .WorldBounds = CompDynData->WorldBounds, .LocalBounds = CompDynData->LocalBounds });
}