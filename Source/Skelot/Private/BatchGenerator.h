#pragma once

#include "SkelotPrimitiveProxy.h"
#include "Engine/Engine.h"

//generator used for legacy mesh batch
struct FSkelotBatchGenerator
{
	const FSkelotClusterProxy* Proxy;
	bool bShaddowCollector = false;
	bool bWireframe = false;
	const FConvexVolume* EditedViewFrustum = nullptr;
	FVector3f ResolvedViewLocation = FVector3f::ZeroVector;
	uint32* VisibleInstances = nullptr;	//indices of visible instances
	uint32* Distances = nullptr; //distance of instances (float is treated as uint32 for faster comparison), accessed by vis index, not instance index
	uint32 TotalInstances = 0;
	uint32 NumVisibleInstance = 0;
	const FSceneViewFamily* ViewFamily = nullptr;
	const FSceneView* View = nullptr;
	FMeshElementCollector* Collector = nullptr;
	int ViewIndex = 0;
	uint32 LODDrawDistances[SKELOT_MAX_LOD] = {};	//sorted from low to high. value is binary form of float. in other words we use: reinterpret_cast<uint32>(floatA) < reinterpret_cast<uint32>(floatB)
	uint32 MinDrawDist = 0;
	uint32 MaxDrawDist = ~0u;
	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;

	int32 SubMeshMatOffset = 0;
	FConvexVolume StackConvex;
	FMemStackBase MemStack;

	struct FMeshLODRange
	{
		FIntRange LODRanges[SKELOT_MAX_LOD];
		FSkelotVertexFactoryBufferRef LODBuffers[SKELOT_MAX_LOD];
		uint32 LODInstanceCount[SKELOT_MAX_LOD];
		uint32 InstanceOffsets[SKELOT_MAX_LOD];

		FMeshLODRange()
		{
			FMemory::Memzero(LODRanges);
		}
	};

	struct FMeshLODRangePack : FOneFrameResource
	{
		TArray<FMeshLODRange> MeshLODRanges; //accessed by sub mesh index
	};

	FMeshLODRangePack* MeshLODPack = nullptr;


	static const uint32 DISTANCING_NUM_FLOAT_PER_REG = 4;


	void Start()
	{
		ResolvedViewLocation = (FVector3f)View->CullingOrigin;	//OverrideLODViewOrigin ?

		EditedViewFrustum = &View->GetCullingFrustum();
		if (bShaddowCollector)	//is it collecting for shadow ?
		{
			this->EditedViewFrustum = View->GetDynamicMeshElementsShadowCullFrustum();
			const bool bShadowNeedTranslation = !View->GetPreShadowTranslation().IsZero();
			if (bShadowNeedTranslation)
			{
				auto NewPlanes = View->GetDynamicMeshElementsShadowCullFrustum()->Planes;
				//apply offset to the planes
				for (FPlane& P : NewPlanes)
					P.W = (P.GetOrigin() - View->GetPreShadowTranslation()) | P.GetNormal();

				//regenerate Permuted planes
				this->StackConvex = FConvexVolume(NewPlanes);
				this->EditedViewFrustum = &this->StackConvex;
			}

		}

		Cull();

		if (this->NumVisibleInstance == 0)
			return;

		CalcLODS();
		AllocateBuffers();
		GenerateBatches();
	}
	//////////////////////////////////////////////////////////////////////////
	void GenerateBatches()
	{
		for (int32 MeshIdx = 0; MeshIdx < this->Proxy->Meshes.Num(); MeshIdx++)
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			this->bWireframe = AllowDebugViewmodes() && this->ViewFamily->EngineShowFlags.Wireframe;
			if (this->bWireframe)
			{
				FLinearColor MaterialColor = FLinearColor::MakeRandomSeededColor(MeshIdx);
				this->WireframeMaterialInstance = new FColoredMaterialRenderProxy(GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr, MaterialColor);
				this->Collector->RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
			}
#endif
			GenerateLODBatch(MeshIdx);
		}
	};



	//////////////////////////////////////////////////////////////////////////

	void GenerateLODBatch(uint32 MeshIdx)
	{
		const FSkelotClusterProxy::FProxyMeshData& ProxyMD = this->Proxy->Meshes[MeshIdx];
		const FSkelotMeshDef& MeshDefRef = this->Proxy->AnimCollection->Meshes[ProxyMD.MeshDefIdx];

		const int32 CurSubMeshMatOffset = SubMeshMatOffset;
		SubMeshMatOffset += MeshDefRef.Mesh->GetNumMaterials();

		for (int32 LODIndex = 0; LODIndex < SKELOT_MAX_LOD; LODIndex++)
		{
			const FMeshLODRange& MeshLODRange = this->MeshLODPack->MeshLODRanges[MeshIdx];

			if (MeshLODRange.LODInstanceCount[LODIndex] == 0) //nothing to draw ?
				continue;

			const FSkeletalMeshLODRenderData& SkelLODData = ProxyMD.SKRenderData->LODRenderData[LODIndex];
			const FSkelotMeshDataEx::FLODData& SkelotLODData = MeshDefRef.MeshData->LODs[LODIndex];

			for (int32 SectionIndex = 0; SectionIndex < SkelLODData.RenderSections.Num(); SectionIndex++) //for each section ---------
			{
				const FSkelMeshRenderSection& SectionInfo = SkelLODData.RenderSections[SectionIndex];
				const FSkeletalMeshLODInfo* SKMLODInfo = ProxyMD.SKMesh->GetLODInfo(LODIndex);
				int32 SolvedMaterialSection = this->Proxy->RedirectMaterialIndex(*SKMLODInfo, SectionIndex, SectionInfo.MaterialIndex);

				FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Proxy->Materials[CurSubMeshMatOffset + SolvedMaterialSection]->GetRenderProxy();

				if (GSkelot_ForceDefaultMaterial)
					MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();

				if (!MaterialProxy)
					continue;

				FSkelotVertexFactory* VF = SkelotLODData.GetVertexFactory(SectionInfo.MaxBoneInfluences);
				if (!VF)
					continue;


				FMeshBatch& Mesh = this->Collector->AllocateMesh();
				Mesh.VertexFactory = VF;
				Mesh.ReverseCulling = this->Proxy->bRenderDescNegativeDeterminant;
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = this->Proxy->GetDepthPriorityGroup(View);
				Mesh.bCanApplyViewModeOverrides = false;
				Mesh.bSelectable = false;
				Mesh.bUseForMaterial = true;
				Mesh.bUseSelectionOutline = false;
				Mesh.LODIndex = static_cast<int8>(LODIndex);
				Mesh.SegmentIndex = static_cast<uint8>(SectionIndex);
				Mesh.bWireframe = bWireframe;
				Mesh.MaterialRenderProxy = MaterialProxy;
				Mesh.bUseForDepthPass = Proxy->ShouldRenderInDepthPass();
				Mesh.bUseAsOccluder = Proxy->ShouldUseAsOccluder();
				Mesh.CastShadow = SectionInfo.bCastShadow;

				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.UserData = MeshLODRange.LODBuffers[LODIndex].GetReference();
				BatchElement.PrimitiveIdMode = PrimID_ForceZero;
				BatchElement.IndexBuffer = SkelLODData.MultiSizeIndexContainer.GetIndexBuffer();
				//BatchElement.PrimitiveUniformBufferResource = &PrimitiveUniformBuffer->UniformBuffer; //&GIdentityPrimitiveUniformBuffer; 
				BatchElement.PrimitiveUniformBuffer = this->Proxy->GetUniformBuffer();
				check(BatchElement.PrimitiveUniformBuffer);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				Mesh.VisualizeLODIndex = static_cast<int8>(LODIndex);
				BatchElement.VisualizeElementIndex = static_cast<int32>(SectionIndex);
#endif

				BatchElement.FirstIndex = SectionInfo.BaseIndex;
				BatchElement.NumPrimitives = SectionInfo.NumTriangles;
				BatchElement.MinVertexIndex = SectionInfo.BaseVertexIndex;
				BatchElement.MaxVertexIndex = SkelLODData.GetNumVertices() - 1; //SectionInfo.GetVertexBufferIndex() + SectionInfo.GetNumVertices() - 1; //

				BatchElement.NumInstances = MeshLODRange.LODInstanceCount[LODIndex];

				Collector->AddMesh(ViewIndex, Mesh);
			}
		}
	}


	//////////////////////////////////////////////////////////////////////////
	void CollectDistances()
	{
		SKELOT_SCOPE_CYCLE_COUNTER(CollectDistances);

		auto CamPos = VectorLoad(&ResolvedViewLocation.X);
		auto CamX = VectorReplicate(CamPos, 0);
		auto CamY = VectorReplicate(CamPos, 1);
		auto CamZ = VectorReplicate(CamPos, 2);

		const uint32 AlignedNumVis = Align(this->NumVisibleInstance, DISTANCING_NUM_FLOAT_PER_REG);
		const FRenderTransform* Transforms = this->Proxy->DynData->Transforms.GetData();
		check(IsAligned(sizeof(FRenderTransform), 16) && IsAligned((SIZE_T)Transforms, SIZE_T(16)));

		//collect distances of visible indices
		for (uint32 VisIndex = 0; VisIndex < AlignedNumVis; VisIndex += DISTANCING_NUM_FLOAT_PER_REG)
		{
			auto CentersA = VectorLoad(&Transforms[this->VisibleInstances[VisIndex + 0]].Origin.X);
			auto CentersB = VectorLoad(&Transforms[this->VisibleInstances[VisIndex + 1]].Origin.X);
			auto CentersC = VectorLoad(&Transforms[this->VisibleInstances[VisIndex + 2]].Origin.X);
			auto CentersD = VectorLoad(&Transforms[this->VisibleInstances[VisIndex + 3]].Origin.X);

			SkelotVectorTranspose4x4(CentersA, CentersB, CentersC, CentersD);

			auto XD = VectorSubtract(CamX, CentersA);
			auto YD = VectorSubtract(CamY, CentersB);
			auto ZD = VectorSubtract(CamZ, CentersC);

			auto LenSQ = VectorMultiplyAdd(XD, XD, VectorMultiplyAdd(YD, YD, VectorMultiply(ZD, ZD)));

			VectorStoreAligned(LenSQ, reinterpret_cast<float*>(this->Distances + VisIndex));
		}
	}
	//////////////////////////////////////////////////////////////////////////
	void Cull()
	{
		SKELOT_SCOPE_CYCLE_COUNTER(Cull);

		const FSkelotCompDynData* DynData = this->Proxy->DynData.Get();
		//allocate instance indices
		this->VisibleInstances = (uint32*)MemStack.Alloc(sizeof(uint32) * Align(this->TotalInstances, DISTANCING_NUM_FLOAT_PER_REG), alignof(uint32));
		uint32* VisibleInstancesIter = this->VisibleInstances;

		//per instance frustum cull 
		for (uint32 InstanceIndex = 0; InstanceIndex < this->TotalInstances; InstanceIndex++)
		{
			const FVector3f InsOrig = DynData->Transforms[InstanceIndex].Origin;
			if (!EditedViewFrustum->IntersectSphere(FVector(InsOrig), this->Proxy->GPULODRadius))	//frustum cull
				continue;

			*VisibleInstancesIter++ = InstanceIndex;
		}

		this->NumVisibleInstance = VisibleInstancesIter - this->VisibleInstances;

		if (this->NumVisibleInstance == 0)
			return;

		if (bShaddowCollector)
		{
			INC_DWORD_STAT_BY(STAT_SKELOT_ShadowNumVisible, this->NumVisibleInstance);
		}
		else
		{
			INC_DWORD_STAT_BY(STAT_SKELOT_ViewNumVisible, this->NumVisibleInstance);
		}

		{
			//fix the remaining data because we go SIMD only
			uint32 AVI = Align(this->NumVisibleInstance, DISTANCING_NUM_FLOAT_PER_REG);
			for (uint32 i = this->NumVisibleInstance; i < AVI; i++)
				this->VisibleInstances[i] = this->VisibleInstances[0];

			this->Distances = (uint32*)MemStack.Alloc(sizeof(uint32) * AVI, 16);

			CollectDistances();
		}

		//#TODO maybe sort with Algo::Sort when NumVisibleInstance is small
		//sort the instances from front to rear
		if (NumVisibleInstance > 1)
		{
			SKELOT_SCOPE_CYCLE_COUNTER(SortInstances);

			{
				uint32* Distances_Sorted = (uint32*)MemStack.Alloc(sizeof(uint32) * NumVisibleInstance, 16);
				uint32* VisibleInstances_Sorted = (uint32*)MemStack.Alloc(sizeof(uint32) * NumVisibleInstance, 16);

				SkelotRadixSort32(VisibleInstances_Sorted, this->VisibleInstances, Distances_Sorted, this->Distances, static_cast<uint32>(NumVisibleInstance));

				this->VisibleInstances = VisibleInstances_Sorted;
				this->Distances = Distances_Sorted;
			}
		}
#if 0
		if (MaxDrawDist != ~0u)
		{
			uint32* DistIter = this->Distances + this->NumVisibleInstance;
			do
			{
				DistIter--;
				if (*DistIter <= this->MaxDrawDist)
				{
					DistIter++;
					break;
				}
			} while (DistIter != this->Distances);


			this->NumVisibleInstance = DistIter - this->Distances;

			if (this->NumVisibleInstance == 0)
				return;
		}

		if (MinDrawDist > 0)
		{
			uint32 i = 0;
			for (; i < this->NumVisibleInstance; i++)
			{
				if (this->Distances[i] >= this->MinDrawDist)
					break;
			}
			this->VisibleInstances += i;
			this->Distances += i;
			this->NumVisibleInstance -= i;

			if (this->NumVisibleInstance == 0)
				return;
		}
#endif
	}
	//////////////////////////////////////////////////////////////////////////
	void CalcLODS()
	{
		MeshLODPack = &this->Collector->AllocateOneFrameResource<FMeshLODRangePack>();
		MeshLODPack->MeshLODRanges.SetNum(this->Proxy->Meshes.Num());


		static auto CVForceLOD = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ForceLOD"));
		int32 ForceLODIdx = CVForceLOD ? CVForceLOD->GetValueOnRenderThread() : -1;
		static auto CVForceLODShadow = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ForceLODShadow"));
		int32 ForceLODShadowIdx = CVForceLODShadow ? CVForceLODShadow->GetValueOnRenderThread() : -1;
		if (bShaddowCollector && ForceLODShadowIdx >= 0)
			ForceLODIdx = ForceLODShadowIdx;

		if (ForceLODIdx >= 0) //any force LOD ?
		{
			for (int32 MeshIdx = 0; MeshIdx < Proxy->Meshes.Num(); MeshIdx++)
			{
				const FSkelotClusterProxy::FProxyMeshData& ProxyMD = this->Proxy->Meshes[MeshIdx];
				const FSkelotMeshDef& MeshDefRef = this->Proxy->AnimCollection->Meshes[ProxyMD.MeshDefIdx];
				int32 MeshLODIdx = FMath::Clamp(ForceLODIdx, (int32)FMath::Max(ProxyMD.SKRenderData->CurrentFirstLODIdx, ProxyMD.MinLODIndex), ProxyMD.SKRenderData->LODRenderData.Num() - 1);

				MeshLODPack->MeshLODRanges[MeshIdx].LODRanges[MeshLODIdx].Count = NumVisibleInstance;
			}
			return;	//forcefully did set LOD
		}

		const float DistanceFactor = (1.0f / (GSkelot_DistanceScale * Proxy->DistanceScale));

		//init instance data
		{
			for (int LODIndex = 0; LODIndex < SKELOT_MAX_LOD - 1; LODIndex++)
			{
				float LODDrawDist = FMath::Square(Proxy->LODDistances[LODIndex] * DistanceFactor);
				LODDrawDistances[LODIndex] = *reinterpret_cast<uint32*>(&LODDrawDist);
			};
		}

		LODDrawDistances[SKELOT_MAX_LOD - 1] = ~0u;	//MAX_UINT32 so it never pases last LOD

		FIntRange LODRanges[SKELOT_MAX_LOD] = {};	//accessed by LOD Index, value is VisIndex range

		uint32 CurLODIndex = 0;
		uint32 LastIndex = 0;
		//find out LODRanges, Distances are already sorted !
		for (uint32 VisIndex = 0; VisIndex < this->NumVisibleInstance; VisIndex++)
		{
			const uint32 InstanceIndex = this->VisibleInstances[VisIndex];
			const uint32 DistanceSQ = this->Distances[VisIndex];

			while (DistanceSQ > LODDrawDistances[CurLODIndex])
			{
				LODRanges[CurLODIndex].Count = VisIndex - LastIndex;
				LastIndex = VisIndex;
				CurLODIndex++;
				LODRanges[CurLODIndex].Start = VisIndex;
			}
		}
		LODRanges[CurLODIndex].Count = NumVisibleInstance - LastIndex;

		//found the LODIndex of Instances but meshes may have different fist, last LODIndex, so we need to adjust them
		for (int32 MeshIdx = 0; MeshIdx < Proxy->Meshes.Num(); MeshIdx++)
		{
			const FSkelotClusterProxy::FProxyMeshData& ProxyMD = this->Proxy->Meshes[MeshIdx];
			const FSkelotMeshDef& MeshDefRef = this->Proxy->AnimCollection->Meshes[ProxyMD.MeshDefIdx];
			const int32 FirstLODIdx = FMath::Max(ProxyMD.SKRenderData->CurrentFirstLODIdx, ProxyMD.MinLODIndex);
			const int32 LastLODIdx = ProxyMD.SKRenderData->LODRenderData.Num() - 1;

			FMemory::Memcpy(MeshLODPack->MeshLODRanges[MeshIdx].LODRanges, LODRanges, sizeof(LODRanges));
			SkelotMergeRanges(MeshLODPack->MeshLODRanges[MeshIdx].LODRanges, SKELOT_MAX_LOD, FirstLODIdx, LastLODIdx);
		}

	}


	//////////////////////////////////////////////////////////////////////////
	//allocate buffers and fill them
	void AllocateBuffers()
	{
		FGlobalDynamicReadBuffer::FAllocation FrameIndicesAlc = this->Collector->GetDynamicReadBuffer().AllocateUInt32(NumVisibleInstance * 2);
		FGlobalDynamicReadBuffer::FAllocation DataIndicesAlc = this->Collector->GetDynamicReadBuffer().AllocateUInt32(NumVisibleInstance * Proxy->Meshes.Num());
		FGlobalDynamicReadBuffer::FAllocation TransformsAlc = this->Collector->GetDynamicReadBuffer().AllocateFloat(NumVisibleInstance * 2 * 12);

		FGlobalDynamicReadBuffer::FAllocation CustomDataAlc;

		uint32 NumCDF = Proxy->NumCustomDataFloat;
		if (bShaddowCollector && !Proxy->bNeedCustomDataForShadowPass)
			NumCDF = 0;

		if (NumCDF != 0)
		{
			CustomDataAlc = this->Collector->GetDynamicReadBuffer().AllocateFloat(NumVisibleInstance * NumCDF);
		}
		//inputs
		const FSkelotCompDynData* DynData = this->Proxy->DynData.Get();
		const uint32* CustomDataAsUINT32 = (uint32*)DynData->CustomData.GetData();
		const int32 RealNCDF = DynData->CustomData.Num() / DynData->Transforms.Num();
		const float* SrcCDFloats = DynData->CustomData.GetData() + 2;

		//mapped buffers
		FRenderTransform* DstTransforms = (FRenderTransform*)TransformsAlc.Buffer;
		uint32* DstFrames = (uint32*)FrameIndicesAlc.Buffer;
		float* DstCustomDataFloats = (float*)CustomDataAlc.Buffer;
		uint32* DstDataIndices = (uint32*)DataIndicesAlc.Buffer;
		

		//fill cur prev transforms and frame indices
		for (uint32 VisIdx = 0; VisIdx < this->NumVisibleInstance; VisIdx++)
		{
			uint32 InstanceIdx = this->VisibleInstances[VisIdx];

			//DstTransforms[VisIdx * 2 + 0] = DynData->Transforms[InstanceIdx];
			//DstTransforms[VisIdx * 2 + 1] = DynData->PrevTransforms[InstanceIdx];
			//or VVV
			DynData->Transforms[InstanceIdx].To3x4MatrixTranspose(reinterpret_cast<float*>(&DstTransforms[VisIdx * 2 + 0]));
			DynData->PrevTransforms[InstanceIdx].To3x4MatrixTranspose(reinterpret_cast<float*>(&DstTransforms[VisIdx * 2 + 1]));

			DstFrames[VisIdx * 2 + 0] = CustomDataAsUINT32[InstanceIdx * RealNCDF + 0];	//cur frame anim index
			DstFrames[VisIdx * 2 + 1] = CustomDataAsUINT32[InstanceIdx * RealNCDF + 1];	//prev frame anim index

			//copy per instance custom data float if any
			for (uint32 FloatIdx = 0; FloatIdx < NumCDF; FloatIdx++)
			{
				DstCustomDataFloats[VisIdx * NumCDF + FloatIdx] = SrcCDFloats[InstanceIdx * RealNCDF + FloatIdx];
			}
		}

		//fill elements indices ---------------------
		{
			//lets fill InstancesSubMeshMask from InstanceRun of Dynamic Data (we don't have access to per instance sub mesh index)
			TBitArray<> InstancesSubMeshMask[0xFF];
			for (int32 MeshIdx = 0; MeshIdx < Proxy->DynData->InstanceRuns.Num(); MeshIdx++)
			{
				InstancesSubMeshMask[MeshIdx].Init(false, Proxy->DynData->Transforms.Num());
				for (const FInstanceRunData IRD : Proxy->DynData->InstanceRuns[MeshIdx])
				{
					for (int32 InstanceIndex = IRD.StartOffset; InstanceIndex <= IRD.EndOffset; InstanceIndex++)
					{
						InstancesSubMeshMask[MeshIdx][InstanceIndex] = true;
					}
				}
			}

			FSkelotVertexFactoryParameters UniformParams;
			UniformParams.NumCustomDataFloats = 0;
			//UniformParams.DeterminantSign = this->Proxy->bRenderDescNegativeDeterminant;
			UniformParams.Instance_CustomData = GNullVertexBuffer.VertexBufferSRV;//#TODO proper SRV ?

			if (NumCDF != 0) //do we have any per instance custom data 
			{
				UniformParams.Instance_CustomData = CustomDataAlc.SRV;
				UniformParams.NumCustomDataFloats = NumCDF;
			}

			UniformParams.Instance_Transforms = TransformsAlc.SRV;
			UniformParams.Instance_AnimationFrameIndices = FrameIndicesAlc.SRV;
			UniformParams.Instance_DataIndex = DataIndicesAlc.SRV;

			uint32 DataIndex = 0;
			//generate instance run from culled instances
			{
				for (int32 MeshIdx = 0; MeshIdx < Proxy->Meshes.Num(); MeshIdx++) //for each sub mesh ----------------
				{
					for (int32 LODIndex = 0; LODIndex < SKELOT_MAX_LOD; LODIndex++) //for each LOD ---------------
					{
						const FIntRange LODRange = MeshLODPack->MeshLODRanges[MeshIdx].LODRanges[LODIndex];

						uint32 InstanceStartOffset = DataIndex;

						for (int32 VisIndex = LODRange.Start; VisIndex < LODRange.Start + LODRange.Count; VisIndex++)
						{
							const int32 InstanceIdx = this->VisibleInstances[VisIndex];
							if (InstancesSubMeshMask[MeshIdx][InstanceIdx])
							{
								DstDataIndices[DataIndex] = VisIndex;
								DataIndex++;
							}
						}

						const int32 Count = DataIndex - InstanceStartOffset;
						MeshLODPack->MeshLODRanges[MeshIdx].LODInstanceCount[LODIndex] = Count;

						if (Count != 0)
						{
							FSkelotVertexFactoryParameters LODUniformParams = UniformParams;
							LODUniformParams.InstanceOffset = InstanceStartOffset;

							MeshLODPack->MeshLODRanges[MeshIdx].InstanceOffsets[LODIndex] = InstanceStartOffset;
							MeshLODPack->MeshLODRanges[MeshIdx].LODBuffers[LODIndex] = FSkelotVertexFactoryBufferRef::CreateUniformBufferImmediate(LODUniformParams, UniformBuffer_SingleFrame);
						}

					}
				}
			}
		}

	}

};
