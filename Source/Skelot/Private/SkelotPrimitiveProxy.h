#pragma once


#include "PrimitiveSceneProxy.h"


#include "KismetTraceUtils.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimSequenceBase.h"

#include "SkelotWorld.h"
#include "SkelotClusterComponent.h"

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
#include "Engine/SkinnedAssetCommon.h"
#include "ContentStreaming.h"
#include "SkelotObjectVersion.h"
#include "UObject/UObjectIterator.h"
#include "SceneInterface.h"
#include "Engine/World.h"
#include "RHICommandList.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "SkelotSettings.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Skelot.h"
#include "InstanceDataSceneProxy.h"
#include "SkelotRenderResources.h"
#include "Materials/MaterialRenderProxy.h"

#ifndef private 
#define private public 
#endif
#include "ScenePrivate.h"

#undef  private


class FSkelotClusterProxy;



struct FSkelotCompDynData
{
	FSkelotClusterProxy* ComponentProxy = nullptr;

	FBoxSphereBounds WorldBounds;
	FBoxSphereBounds LocalBounds;
	FVector Center;
	FVector PrevCenter;

	TArray<FRenderTransform> Transforms;
	TArray<FRenderTransform> PrevTransforms;
	TArray<float> CustomData;
	TArray<FRenderBounds> PerInstanceLocalBounds;

	TArray<TArray<FInstanceRunData>> InstanceRuns;	//instance run ranges for sub meshes


	FMatrix GetLocalToWorld(bool bNegDet) const { return FScaleRotationTranslationMatrix(FVector(bNegDet ? -1 : 1, 1, 1), FRotator::ZeroRotator, Center); }
	FMatrix GetPrevLocalToWorld(bool bNegDet) const { return FScaleRotationTranslationMatrix(FVector(bNegDet ? -1 : 1, 1, 1), FRotator::ZeroRotator, PrevCenter); }
};



struct FSkelotCluserISDB : public FInstanceSceneDataBuffers
{
	void Init(FSkelotClusterProxy* Proxy);

};


//////////////////////////////////////////////////////////////////////////
class FSkelotClusterProxy : public FPrimitiveSceneProxy
{
public:
	typedef FPrimitiveSceneProxy Super;

	struct FProxyMeshData
	{
		USkeletalMesh* SKMesh;
		int32 MeshDefIdx;
		const FSkeletalMeshRenderData* SKRenderData;
		uint8 MinLODIndex;
		float LODScreenSizeScale;
	};

	FMaterialRelevance MaterialRelevance;
	USkelotAnimCollection* AnimCollection;
	TArray<FProxyMeshData, TInlineAllocator<6>> Meshes;
	ASkelotWorld* OwnerActor;
	FSkelotCluserISDB InstanceBuffer;
	bool bWithPerInstanceLocalBound = false;
	bool bRenderDescNegativeDeterminant = false;
	bool bLegacyRender = false;
	
	int32 NumCustomDataFloat = 0;
	TArray<UMaterialInterface*> Materials;
	FRenderBounds ApproximateLocalBounds;
	float GPULODRadius;
	//last dynamic data
	TUniquePtr<FSkelotCompDynData> DynData;
	
	//used for legacy renders
	float LODDistances[SKELOT_MAX_LOD - 1];
	float DistanceScale;
	bool bNeedCustomDataForShadowPass = false;	//
	uint8 ShadowLODBias;
	uint8 StartShadowLODBias;

	FSkelotClusterProxy(const USKelotClusterComponent* Component, FName ResourceName);
	~FSkelotClusterProxy()
	{

	}


	bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest && !MaterialRelevance.bPostMotionBlurTranslucency && !ShouldRenderCustomDepth();
	}
	void ApplyWorldOffset(FRHICommandListBase& RHICmdList, FVector InOffset) override {}

	FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	SIZE_T GetTypeHash() const override;
	void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override
	{

	}

	void DestroyRenderThreadResources() override
	{

	}
	uint32 GetMemoryFootprint(void) const override
	{
		return (sizeof(*this) + GetAllocatedSize());
	}
	uint32 GetAllocatedSize(void) const
	{
		return 0;
	}
	void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	//////////////////////////////////////////////////////////////////////////
	void RenderInstanceBound(uint32 Index, FPrimitiveDrawInterface* PDI, const FSceneView* View) const
	{
		FRenderBounds BoundLS = this->InstanceBuffer.GetInstanceLocalBounds(Index);
		FMatrix T = this->InstanceBuffer.GetInstanceToWorld(Index);
		FRenderBounds BoundsWS = BoundLS.TransformBy(T);
		DrawWireBox(PDI, BoundsWS.ToBox(), FColor::Purple, SDPG_World);

		//draw axis  ?
		if (FVector::DistSquared(View->ViewLocation, T.GetOrigin()) < FMath::Square(5000))
		{
			FVector AxisLoc = T.GetOrigin();
			FVector X, Y, Z;
			T.GetScaledAxes(X, Y, Z);
			const float Scale = 50;
			PDI->DrawLine(AxisLoc, AxisLoc + X * Scale, FColor::Red, SDPG_World, 1);
			PDI->DrawLine(AxisLoc, AxisLoc + Y * Scale, FColor::Green, SDPG_World, 1);
			PDI->DrawLine(AxisLoc, AxisLoc + Z * Scale, FColor::Blue, SDPG_World, 1);
		}
	}
#endif

	void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	//////////////////////////////////////////////////////////////////////////
	void FillMeshBatch(FMeshBatch& Mesh, int32 LODIndex, int32 SectionIndex, const FSkeletalMeshLODRenderData& SKLODData, const FSkelotMeshDef& MeshDefStruct) const;
	//////////////////////////////////////////////////////////////////////////
	static int32 RedirectMaterialIndex(const FSkeletalMeshLODInfo& SKLODInfo, int32 SectionIndex, int32 MaterialIndex)
	{
		//remap to the correct material index
		if (SKLODInfo.LODMaterialMap.IsValidIndex(SectionIndex) && SKLODInfo.LODMaterialMap[SectionIndex] != INDEX_NONE)
		{
			return SKLODInfo.LODMaterialMap[SectionIndex];
		}
		return MaterialIndex;
	}
	//////////////////////////////////////////////////////////////////////////
	bool GetInstanceDrawDistanceMinMax(FVector2f& OutDistanceMinMax) const override
	{
		OutDistanceMinMax = FVector2f(MinDrawDistance, MaxDrawDistance);
		return true;
	}
	float GetLodScreenSizeScale() const override { return 1; }
	float GetGpuLodInstanceRadius() const override { return GPULODRadius; }

	FInstanceDataUpdateTaskInfo* GetInstanceDataUpdateTaskInfo() const override
	{
		return nullptr;
		//return const_cast<FInstanceDataUpdateTaskInfo*>(&InstanceDataSceneProxy.InstanceDataUpdateTaskInfo);
	}
	bool AllowInstanceCullingOcclusionQueries() const
	{
		return false;
	}
	void SetInstanceCullDistance_RenderThread(float StartCullDistance, float EndCullDistance) override
	{

	}

	void UpdateInstanceBuffer(FSkelotCompDynData* CompDynData);


	
};


