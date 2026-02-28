// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotAnimCollection.h"
#include "Animation/AnimSequence.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "SkelotPrivate.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Misc/MemStack.h"
#include "EngineUtils.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "Engine/AssetManager.h"
#include "ShaderCore.h"
#include "Animation/AnimationPoseData.h"
#include "Async/ParallelFor.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IContentBrowserSingleton.h"
#include "DerivedDataCacheInterface.h"
#endif


#include "Chaos/Sphere.h"
#include "Chaos/Box.h"
#include "Chaos/Capsule.h"
#include "Styling/AppStyle.h"

#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "BoneContainer.h"
#include "Animation/AnimCurveTypes.h"
#include "BonePose.h"
#include "Interfaces/ITargetPlatform.h"


#include "SkelotObjectVersion.h"
#include "Misc/Fnv.h"
#include "Math/UnitConversion.h"
#include "SkelotPrivateUtils.h"
#include "SkelotSettings.h"

#include "../Private/SkelotRenderResources.h"
#include "Animation/AttributesRuntime.h"
#include "SkelotAnimNotify.h"
#include "Engine/Engine.h"
#include "Animation/AnimComposite.h"
#include "AssetRegistry/AssetData.h"
#include "SkelotClusterComponent.h"
#include "RenderGraphBuilder.h"
#include "AnimationUtils.h"
#include "UObject/LinkerLoad.h"
#include "Rendering/RenderCommandPipes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkelotAnimCollection)

typedef TArray<FTransform, TFixedAllocator<256>> FixedTransformArray;
typedef TArray<FTransform, FAnimStackAllocator> TransformArrayAnimStack;

bool GGenerateSingleThread = true; //!!there are some problem when it related to CurveCaching 
int32 GSkelot_NumTransitionGeneratedThisFrame = 0;


class UAnimSequence_Public : public UAnimSequence
{
public:
	
};

namespace Utils
{
	//extract component space reference pose and its inverse from bone container
	void ExtractRefPose(const FBoneContainer& BoneContainer, TArray<FTransform>& OutPoseComponentSpace, TArray<FMatrix44f>& OutInverse)
	{
		OutPoseComponentSpace.SetNumUninitialized(BoneContainer.GetNumBones());
		OutInverse.SetNumUninitialized(BoneContainer.GetNumBones());

		OutPoseComponentSpace[0] = BoneContainer.GetRefPoseTransform(FCompactPoseBoneIndex(0));
		OutInverse[0] = ((FTransform3f)OutPoseComponentSpace[0]).Inverse().ToMatrixWithScale();

		for (FCompactPoseBoneIndex i(1); i < BoneContainer.GetNumBones(); ++i)
		{
			const FCompactPoseBoneIndex parentIdx = BoneContainer.GetParentBoneIndex(i);
			check(parentIdx < i);
			OutPoseComponentSpace[i.GetInt()] = BoneContainer.GetRefPoseTransform(i) * OutPoseComponentSpace[parentIdx.GetInt()];
			OutInverse[i.GetInt()] = ((FTransform3f)OutPoseComponentSpace[i.GetInt()]).Inverse().ToMatrixWithScale();
		}
	}

	//
	TArray<FTransform> ConvertMeshPoseToSkeleton(USkeletalMesh* Mesh)
	{
		const FReferenceSkeleton& SkelRefPose = Mesh->GetSkeleton()->GetReferenceSkeleton();
		const FReferenceSkeleton& MeshRefPose = Mesh->GetRefSkeleton();

		TArray<FTransform> Pose = SkelRefPose.GetRefBonePose();

		for (int MeshBoneIndex = 0; MeshBoneIndex < MeshRefPose.GetNum(); MeshBoneIndex++)
		{
			int SkelBoneIndex = Mesh->GetSkeleton()->GetSkeletonBoneIndexFromMeshBoneIndex(Mesh, MeshBoneIndex);
			if (SkelBoneIndex != -1)
			{
				FTransform MeshT = MeshRefPose.GetRefBonePose()[MeshBoneIndex];
				Pose[SkelBoneIndex] = MeshT;
			}
		}

		return Pose;
	}

	void LocalPoseToComponent(const FCompactPose& Pose, FTransform* OutComponentSpace)
	{
		OutComponentSpace[0] = Pose[FCompactPoseBoneIndex(0)];

		for (FCompactPoseBoneIndex CompactIdx(1); CompactIdx < Pose.GetNumBones(); ++CompactIdx)
		{
			const FCompactPoseBoneIndex ParentIdx = Pose.GetParentBoneIndex(CompactIdx);
			OutComponentSpace[CompactIdx.GetInt()] = Pose[CompactIdx] * OutComponentSpace[ParentIdx.GetInt()];
		}
	}
	void LocalPoseToComponent(const FCompactPose& Pose, const FTransform* InLocalSpace, FTransform* OutComponentSpace)
	{
		OutComponentSpace[0] = InLocalSpace[0];

		for (FCompactPoseBoneIndex CompactIdx(1); CompactIdx < Pose.GetNumBones(); ++CompactIdx)
		{
			const FCompactPoseBoneIndex ParentIdx = Pose.GetParentBoneIndex(CompactIdx);
			OutComponentSpace[CompactIdx.GetInt()] = InLocalSpace[CompactIdx.GetInt()] * OutComponentSpace[ParentIdx.GetInt()];
		}
	}

	FString CheckMeshIsQualified(const USkeletalMesh* SKMesh, int InBaseLOD)
	{
		const FSkeletalMeshRenderData* RenderResource = SKMesh->GetResourceForRendering();
		check(RenderResource);

		for (int32 LODIndex = InBaseLOD; LODIndex < RenderResource->LODRenderData.Num(); ++LODIndex)
		{
			const FSkeletalMeshLODRenderData& Data = RenderResource->LODRenderData[LODIndex];
			if (Data.GetSkinWeightVertexBuffer()->GetBoneInfluenceType() != DefaultBoneInfluence || Data.GetVertexBufferMaxBoneInfluences() > FSkelotMeshDataEx::MAX_INFLUENCE)
			{
				return FString::Printf(TEXT("Can't build SkeletalMesh %s at LOD %d. MaxBoneInfluence supported is <= %d."), *SKMesh->GetName(), LODIndex, FSkelotMeshDataEx::MAX_INFLUENCE);
			}
		}

		return FString();
	}

	void HelperBlendPose(const TransformArrayAnimStack& A, const TransformArrayAnimStack& B, float Alpha, TransformArrayAnimStack& Out)
	{
		check(Alpha >= 0 && Alpha <= 1);
		for (int i = 0; i < A.Num(); i++)
		{
			Out[i].Blend(A[i], B[i], Alpha);
		}
	}


#if WITH_EDITOR
	//
	FBox HelperCalcSkeletalMeshBoundCPUSkin(USkeletalMesh* SKMesh, const TArrayView<FTransform>& BoneTransforms, bool bDrawDebug, int LODIndex)
	{
		TArray<FMatrix, TFixedAllocator<1024>> BoneMatrices;
		for (FTransform T : BoneTransforms)
			BoneMatrices.Add(T.ToMatrixWithScale());

		const FSkeletalMeshLODRenderData& LODData = SKMesh->GetResourceForRendering()->LODRenderData[LODIndex];

		const int NumVertex = LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
		const FSkinWeightVertexBuffer* SkinWeightVB = LODData.GetSkinWeightVertexBuffer();
		const int MaxBoneInf = SkinWeightVB->GetMaxBoneInfluences();
		//TArray<FSkinWeightInfo> SkinWeights;
		//SkinWeightVB->GetSkinWeights(SkinWeights);

		FBox Bound(ForceInit);

		for (int VertexIndex = 0; VertexIndex < NumVertex; VertexIndex++)
		{
			const FVector VertexPos = FVector(LODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex));
			int SectionIndex, SectionVertexIndex;
			LODData.GetSectionFromVertexIndex(VertexIndex, SectionIndex, SectionVertexIndex);
			const FSkelMeshRenderSection& SectionInfo = LODData.RenderSections[SectionIndex];

			FMatrix BlendMatrix;
			FMemory::Memzero(BlendMatrix);

			for (int InfluenceIndex = 0; InfluenceIndex < MaxBoneInf; InfluenceIndex++)
			{
				const uint32 BondeIndex = SkinWeightVB->GetBoneIndex(VertexIndex, InfluenceIndex); //SkinWeights[VertexIndex].InfluenceBones[InfluenceIndex];
				const uint16 Weight = SkinWeightVB->GetBoneWeight(VertexIndex, InfluenceIndex);	// SkinWeights[VertexIndex].InfluenceWeights[InfluenceIndex];
				if (Weight == 0)
					continue;

				check(BondeIndex <= 255 && Weight <= 0xFFFF);

				const float WeighScaler = Weight / (65535.0f);
				check(WeighScaler >= 0 && WeighScaler <= 1.0f);
				FBoneIndexType RealBoneIndex = SectionInfo.BoneMap[BondeIndex];
				BlendMatrix += BoneMatrices[RealBoneIndex] * WeighScaler;
			}

			FVector SkinnedVertex = BlendMatrix.TransformPosition(VertexPos);
			Bound += SkinnedVertex;
			//if(bDrawDebug)
			//	DrawDebugPoint(GWorld, SkinnedVertex, 4, FColor::MakeRandomColor(), false, 20);
		}

		return Bound;
	}
#endif


	void NotifyError(const USkelotAnimCollection* AC, const FString& InString)
	{
#if WITH_EDITOR
		if (GEditor)
		{
			FString ErrString = FString::Printf(TEXT("%s Build Failed: %s"), *AC->GetName(), *InString);
			
			GEditor->GetTimerManager()->SetTimerForNextTick([ErrString](){
				FNotificationInfo Info(FText::FromString(ErrString));
				Info.Image = FAppStyle::GetBrush(TEXT("Icons.Error"));
				Info.FadeInDuration = 0.1f;
				Info.FadeOutDuration = 0.5f;
				Info.ExpireDuration = 7;
				Info.bUseThrobber = false;
				Info.bUseSuccessFailIcons = true;
				Info.bUseLargeFont = true;
				Info.bFireAndForget = false;
				Info.bAllowThrottleWhenFrameRateIsLow = false;
				TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
				if (NotificationItem)
				{
					NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
					NotificationItem->ExpireAndFadeout();
					//GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
				}
			});

			UE_LOG(LogSkelot, Error, TEXT("%s"), *ErrString);
		}
#endif
	}

	FLinearColor ExtractCurve(const USkelotAnimCollection* AC, const FBlendedCurve& InCurve)
	{
		FLinearColor Curve4Float = FLinearColor::Transparent;
		for (int i = 0; i < AC->CurvesToCache.Num(); i++)
			Curve4Float.Component(i) = InCurve.Get(AC->CurvesToCache[i]);
		
		return Curve4Float;
	}

};







TArray<FName> USkelotAnimCollection::GetValidBones() const
{
	TArray<FName> BoneNames;
	if (Skeleton)
	{
		for (const FMeshBoneInfo& BoneInfo : Skeleton->GetReferenceSkeleton().GetRefBoneInfo())
			BoneNames.Add(BoneInfo.Name);
	}
	return BoneNames;
}

TArray<FName> USkelotAnimCollection::GetCurveNames() const
{
	TArray<FName> Names;
	Names.Add(NAME_None);
	Skeleton->ForEachCurveMetaData([&](FName Name, FCurveMetaData MD) {
		Names.Add(Name);
	});
	return Names;
}

bool USkelotAnimCollection::Sequences_ShouldBeExcluded(const FAssetData& AssetData) const
{
	const UClass* AssetClass = AssetData.GetClass(EResolveClass::No);
	bool bValidClass = (AssetClass == UAnimComposite::StaticClass() || AssetClass == UAnimSequence::StaticClass());
	return !bValidClass || !IsAssetDataSkeletonCompatible(AssetData) || FindSequenceDefByPath(AssetData.GetSoftObjectPath()) != -1;
}

bool USkelotAnimCollection::Meshes_ShouldBeExclude(const FAssetData& AssetData) const
{
	return !IsAssetDataSkeletonCompatible(AssetData) || FindMeshDefByPath(AssetData.GetSoftObjectPath()) != -1;
}

int32 USkelotAnimCollection::FindSkeletonBoneIndex(FName BoneName) const
{
	if (BoneName != NAME_None && Skeleton)
	{
		return Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
	}
	return -1;
}

FName USkelotAnimCollection::GetBoneName(int32 RefSkeletonBoneIndex) const
{
	if(Skeleton)
	{
		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		if (Ref.IsValidIndex(RefSkeletonBoneIndex))
			return Ref.GetBoneName(RefSkeletonBoneIndex);
	}

	return FName();
}

USkelotAnimCollection::USkelotAnimCollection()
{
}

USkelotAnimCollection::~USkelotAnimCollection()
{

}

bool USkelotAnimCollection::UseHighPrecisionFormat() const
{
	if (bHighPrecision && GPixelFormats[PF_A32B32G32R32F].Supported)
		return true;

	if (!bHighPrecision && GPixelFormats[PF_FloatRGBA].Supported)
		return false;

	if (GPixelFormats[PF_A32B32G32R32F].Supported)
		return true;

	if (GPixelFormats[PF_FloatRGBA].Supported)
		return false;

	UE_LOG(LogSkelot, Fatal, TEXT("Pixel Format Not Supported"));
	return true;
}

const FTransform3f& USkelotAnimCollection::GetBoneTransform(uint32 SkeletonBoneIndex, uint32 FrameIndex)
{
	if (IsBoneTransformCached(SkeletonBoneIndex))
	{
		if (IsAnimationFrameIndex(FrameIndex))
			return GetBoneTransformFast(SkeletonBoneIndex, FrameIndex);

		ConditionalFlushDeferredTransitions(FrameIndex);
		return GetBoneTransformFast(SkeletonBoneIndex, FrameIndex);
	}
	return FTransform3f::Identity;
}

void USkelotAnimCollection::PostInitProperties()
{
	Super::PostInitProperties();

	if(!IsTemplate())
	{
#if WITH_EDITOR
		FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &USkelotAnimCollection::ObjectModifiedEvent);
#endif
		
		FWorldDelegates::OnWorldPreSendAllEndOfFrameUpdates.AddUObject(this, &USkelotAnimCollection::OnPreSendAllEndOfFrameUpdates);
		FCoreDelegates::OnBeginFrame.AddUObject(this, &USkelotAnimCollection::OnBeginFrame);
		FCoreDelegates::OnEndFrame.AddUObject(this, &USkelotAnimCollection::OnEndFrame);
		FCoreDelegates::OnBeginFrameRT.AddUObject(this, &USkelotAnimCollection::OnBeginFrameRT);
		FCoreDelegates::OnEndFrameRT.AddUObject(this, &USkelotAnimCollection::OnEndFrameRT);

	}
}

void USkelotAnimCollection::PostLoad()
{
	Super::PostLoad();

	auto LPreLoad = [](UObject* Obj) {

		if (Obj)
		{
			if (FLinkerLoad* Linker = Obj->GetLinker())
			{
				Linker->Preload(Obj);
			}

			Obj->ConditionalPostLoad();
		}
	};

	LPreLoad(Skeleton);

	
	for(FSkelotMeshDef& MeshDef : Meshes)
	{
		LPreLoad(MeshDef.Mesh);
	}

	for (FSkelotSequenceDef& SeqDef : Sequences)
	{
		if(UAnimSequence* Seq = Cast<UAnimSequence>(SeqDef.Sequence))
			FAnimationUtils::EnsureAnimSequenceLoaded(*Seq);
	}
	

	if(!IsTemplate() && !UE_SERVER)
	{
		FString ErrString;
		if (!CheckCanBuild(ErrString))
		{
#if WITH_EDITOR
			if(!ErrString.IsEmpty())
				Utils::NotifyError(this, ErrString);
#endif
		}
		else
		{
			if (!BuildData())
			{
				DestroyBuildData();
			}
		}

	}
}

#if WITH_EDITOR
void USkelotAnimCollection::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PRPName = PropertyChangedEvent.Property->GetFName();
	
	CurvesToCache.SetNum(FMath::Min(4, CurvesToCache.Num()));

	bNeedRebuild = true;

	LastEditId = FGuid::NewGuid();
}

void USkelotAnimCollection::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	FName PRPName = PropertyThatWillChange->GetFName();

	if(PRPName == GET_MEMBER_NAME_CHECKED(USkelotAnimCollection, Meshes) || PropertyThatWillChange == nullptr)
	{
		EnqueueReleaseResources();
		FlushRenderingCommands();
		DestroyBuildData();
	}
}


bool USkelotAnimCollection::IsObjectRelatedToThis(const UObject* Other) const
{
	if(Skeleton == Other)
		return true;

	for(const FSkelotMeshDef& MD : Meshes)
		if(MD.Mesh == Other)
			return true;

	for(const FSkelotSequenceDef& SD : Sequences)
		if(SD.Sequence == Other)
			return true;

	return false;
}

void USkelotAnimCollection::ObjectModifiedEvent(UObject* Object)
{
	if(!bNeedRebuild && IsObjectRelatedToThis(Object))
	{
		bNeedRebuild = true;
	}
}

#endif



void USkelotAnimCollection::BeginDestroy()
{
	Super::BeginDestroy();

	if(!IsTemplate())
	{
#if WITH_EDITOR
		FCoreUObjectDelegates::OnObjectModified.RemoveAll(this);
#endif

		if(FApp::CanEverRender())
		{
			EnqueueReleaseResources();
			ReleaseResourcesFence.BeginFence();
		}
	}

	FWorldDelegates::OnWorldPreSendAllEndOfFrameUpdates.RemoveAll(this);
	FCoreDelegates::OnBeginFrame.RemoveAll(this);
	FCoreDelegates::OnEndFrame.RemoveAll(this);
	FCoreDelegates::OnBeginFrameRT.RemoveAll(this);
	FCoreDelegates::OnEndFrameRT.RemoveAll(this);
}

bool USkelotAnimCollection::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && (IsTemplate() || ReleaseResourcesFence.IsFenceComplete());
}

void USkelotAnimCollection::FinishDestroy()
{
	Super::FinishDestroy();

	DestroyBuildData();

}

void USkelotAnimCollection::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);
	//#TODO 
	auto MatrixSize = UseHighPrecisionFormat() ? sizeof(float[3][4]) : sizeof(uint16[3][4]);
	CumulativeResourceSize.AddDedicatedVideoMemoryBytes(TotalFrameCount * RenderBoneCount * MatrixSize);
}


void USkelotAnimCollection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	Ar.UsingCustomVersion(FSkelotObjectVersion::GUID);
	

	if(!IsTemplate() && (Ar.IsCooking() || FPlatformProperties::RequiresCookedData()))
	{
		FStripDataFlags StripFlags(Ar);
		const bool bIsServerOnly = Ar.IsSaving() ? Ar.CookingTarget()->IsServerOnly() : FPlatformProperties::IsServerOnly();
		
		for (FSkelotMeshDef& MeshDef : Meshes)
		{
			if(!bIsServerOnly)  //dedicated server doesn't need skin weight
			{
				MeshDef.SerializeCooked(Ar);
			}
		}
	}
}

#if WITH_EDITOR
void USkelotAnimCollection::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);

	//if (!IsTemplate())
	//{
	//	if(Skeleton && Meshes.Num())
	//	{
	//		InitSkeletonData();
	//		BuildMeshData();
	//	}
	//}
}

void USkelotAnimCollection::ClearAllCachedCookedPlatformData()
{
	Super::ClearAllCachedCookedPlatformData();

	//DestroyBuildData();
}

#endif


#if WITH_EDITOR

void USkelotAnimCollection::Rebuild()
{
	//FGlobalComponentReregisterContext ReregisterContext;

	//lets unregister skelot components
	TArray<USKelotClusterComponent*> Comps;
	for (USKelotClusterComponent* Component : TObjectRange<USKelotClusterComponent>())
	{
		if (Component->IsRegistered())
		{
			Component->UnregisterComponent();
			Comps.Add(Component);
		}
	}

	FlushRenderingCommands();
	//build data now
	InternalBuildAll();
	//register skelot components again
	for (USKelotClusterComponent* Comp : Comps)
	{
		Comp->ReregisterComponent();
	}

	FlushRenderingCommands();
}



void USkelotAnimCollection::AddAllMeshes()
{
	IAssetRegistry* AR = IAssetRegistry::Get();
	if(AR)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
		Filter.TagsAndValues.Add(FName(TEXT("Skeleton")), GetSkeletonTagValue());

		TArray<FAssetData> AssetData;
		AR->GetAssets(Filter, AssetData);

		for(const FAssetData& AD : AssetData)
		{
			AddMeshUnique(AD);
		}
	}
}

void USkelotAnimCollection::AddAllAnimations()
{
	IAssetRegistry* AR = IAssetRegistry::Get();
	if (AR)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
		Filter.TagsAndValues.Add(FName(TEXT("Skeleton")), GetSkeletonTagValue());

		TArray<FAssetData> AssetData;
		AR->GetAssets(Filter, AssetData);

		for (const FAssetData& AD : AssetData)
		{
			AddAnimationUnique(AD);
		}
	}
}

void USkelotAnimCollection::AddSelectedAssets()
{
	TArray<FAssetData> SelectedAssets;
	IContentBrowserSingleton::Get().GetSelectedAssets(SelectedAssets);

	for (const FAssetData& AD : SelectedAssets)
	{
		if(!IsAssetDataSkeletonCompatible(AD))
			continue;

		if(AD.IsInstanceOf<USkeletalMesh>())
		{
			AddMeshUnique(AD);
		}
		else if (AD.IsInstanceOf<UAnimSequence>())
		{
			AddAnimationUnique(AD);
		}
		else if (AD.IsInstanceOf<UAnimComposite>())
		{
			UAnimComposite* Composite = Cast<UAnimComposite>(AD.GetAsset());
			if(Composite)
			{
				for (const FAnimSegment& AnimSeg : Composite->AnimationTrack.AnimSegments)
				{
					if (AnimSeg.GetAnimReference())
					{
						int32 SeqDefIdx = FindSequenceDef(AnimSeg.GetAnimReference());
						if (SeqDefIdx == -1)
						{
							Sequences.AddDefaulted_GetRef().Sequence = AnimSeg.GetAnimReference();
						}
					}
				}
			}

		}
	}
}

void USkelotAnimCollection::AddAllCurves()
{
	this->CurvesToCache.Empty();
	for (const FSkelotSequenceDef& Seg : Sequences)
	{
		if (Seg.Sequence)
		{
			for (const FFloatCurve& Curve : Seg.Sequence->GetCurveData().FloatCurves)
			{
				this->CurvesToCache.AddUnique(Curve.GetName());
			}
		}
	}
}

void USkelotAnimCollection::RemoveInvalidAssets()
{
	Sequences.RemoveAll([&](const FSkelotSequenceDef& SD) {
		if (!IsValid(SD.Sequence))
			return true;

		FAssetData AD = IAssetRegistry::Get()->GetAssetByObjectPath(SD.Sequence->GetPathName());
		return !IsAssetDataSkeletonCompatible(AD);
	});

	Meshes.RemoveAll([&](const FSkelotMeshDef& MD) {
		if (!IsValid(MD.Mesh))
			return true;

		FAssetData AD = IAssetRegistry::Get()->GetAssetByObjectPath(MD.Mesh->GetPathName());
		return !IsAssetDataSkeletonCompatible(AD);
	});
}

void USkelotAnimCollection::AddMeshUnique(const FAssetData& InAssetData)
{
	if (this->FindMeshDefByPath(InAssetData.GetSoftObjectPath()) == -1)
	{
		this->Meshes.AddDefaulted();
		this->Meshes.Last().Mesh = Cast<USkeletalMesh>(InAssetData.GetAsset());
	}
}

void USkelotAnimCollection::AddAnimationUnique(const FAssetData& InAssetData)
{
	if (this->FindSequenceDefByPath(InAssetData.GetSoftObjectPath()) == -1)
	{
		this->Sequences.AddDefaulted();
		this->Sequences.Last().Sequence = Cast<UAnimSequence>(InAssetData.GetAsset());
	}
}





void USkelotAnimCollection::TryBuildAll()
{
 	//if we have not build any data then build it now
 	if (bNeedRebuild)
 	{
		InternalBuildAll();
 	}
}


void USkelotAnimCollection::InternalBuildAll()
{
	if (FApp::CanEverRender() && this->bIsBuilt)
	{
		EnqueueReleaseResources();
		FlushRenderingCommands();
	}

	DestroyBuildData();

	FString ErrString;
	if (!CheckCanBuild(ErrString))
	{
		if (!ErrString.IsEmpty())
			Utils::NotifyError(this, ErrString);

		return;
	}

	if (!BuildData())
	{
		DestroyBuildData();
	}

	FlushRenderingCommands();
}



#endif

FString USkelotAnimCollection::GetSkeletonTagValue() const
{
	return Skeleton ? FString::Printf(TEXT("%s'%s'"), *Skeleton->GetClass()->GetPathName(), *Skeleton->GetPathName()) : FString();
}

bool USkelotAnimCollection::IsAssetDataSkeletonCompatible(const FAssetData& AssetData) const
{
	static FName Name_Skeleton(TEXT("Skeleton"));
	return Skeleton && AssetData.TagsAndValues.ContainsKeyValue(Name_Skeleton, GetSkeletonTagValue());
}

void USkelotAnimCollection::DestroyBuildData()
{
	check(IsInGameThread());

	AnimationBuffer = nullptr;
	CurveBuffer = nullptr;

	for(FSkelotSequenceDef& SeqDef : Sequences)
	{
		SeqDef.AnimationFrameIndex = SeqDef.AnimationFrameCount = 0;
		SeqDef.Notifies.Empty();
	}
	for (FSkelotMeshDef& MeshDef : Meshes)
	{
		MeshDef.MeshData = nullptr;
		MeshDef.OwningBounds.Empty();
		MeshDef.MaxBBox = FBoxMinMaxFloat(ForceInit);
		MeshDef.SmallestBoundRadius = FLT_MAX;
		MeshDef.CompactPhysicsAsset = FSkelotCompactPhysicsAsset();
	}

	CachedTransforms.Empty();
	SkeletonBoneIndexTo_CachedTransforms.Empty();
	BonesToCache_Indices.Empty();

	

	RefPoseComponentSpace.Empty();
	RefPoseInverse.Empty();

	RenderRequiredBones.Empty();

	AnimationBoneContainer = FBoneContainer();

	TotalFrameCount = FrameCountSequences = RenderBoneCount = AnimationBoneCount = TotalAnimationBufferSize = TotalMeshBonesBufferSize = 0;
	NumTransitionFrameAllocated = 0;

	MeshesBBox = FBoxCenterExtentFloat(ForceInit);

	
	Transitions.Empty();
	TransitionsHashTable.Free();
	TransitionPoseAllocator.Empty();
	
	ZeroRCTransitions.Empty();
	NegativeRCTransitions.Empty();
	DeferredTransitions.Reset();
	DeferredTransitions_FrameCount = 0;

	DynamicPoseAllocator.Empty();
	DynamicPoseFlipFlags.Empty();

	CurrentUpload = FPoseUploadData{};
	ScatterBuffer = FScatterUploadBuffer{};

	bNeedRebuild = true;
	bIsBuilt = false;
	
}



FArchive& operator<<(FArchive& Ar, FSkelotCompactPhysicsAsset& PA)
{
	PA.Capsules.BulkSerialize(Ar);
	PA.Spheres.BulkSerialize(Ar);
	PA.Boxes.BulkSerialize(Ar);
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FSkelotCompactPhysicsAsset::FShapeSphere& Shape)
{
	Ar << Shape.Center << Shape.Radius << Shape.BoneIndex;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FSkelotCompactPhysicsAsset::FShapeBox& Shape)
{
	Ar << Shape.Rotation << Shape.Center << Shape.BoxMin << Shape.BoxMax << Shape.BoneIndex << Shape.bHasTransform;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FSkelotCompactPhysicsAsset::FShapeCapsule& Shape)
{
	Ar << Shape.A << Shape.B << Shape.BoneIndex << Shape.Radius;
	return Ar;
}


int USkelotAnimCollection::FindSequenceDef(const UAnimSequenceBase* Animation) const
{
	return Sequences.IndexOfByPredicate([=](const FSkelotSequenceDef& Item){ return Item.Sequence == Animation; });
}

int USkelotAnimCollection::FindSequenceDefByPath(const FSoftObjectPath& AnimationPath) const
{
	return Sequences.IndexOfByPredicate([=](const FSkelotSequenceDef& Item) { return Item.Sequence && FSoftObjectPath(Item.Sequence) == AnimationPath; });
}

int USkelotAnimCollection::FindMeshDef(const USkeletalMesh* Mesh) const
{
	return Meshes.IndexOfByPredicate([=](const FSkelotMeshDef& Item){ return Item.Mesh == Mesh; });
}

int USkelotAnimCollection::FindMeshDefByPath(const FSoftObjectPath& MeshPath) const
{
	return Meshes.IndexOfByPredicate([=](const FSkelotMeshDef& Item) { return Item.Mesh && FSoftObjectPath(Item.Mesh) == MeshPath; });
}

FSkelotMeshDataExPtr USkelotAnimCollection::FindMeshData(const USkeletalMesh* InMesh) const
{
	for (const FSkelotMeshDef& MeshDef : Meshes)
		if(MeshDef.Mesh == InMesh && MeshDef.MeshData)
			return MeshDef.MeshData;

	return nullptr;
}

UAnimSequenceBase* USkelotAnimCollection::GetRandomAnimSequence() const
{
	if (Sequences.Num())
		return Sequences[FMath::RandHelper(Sequences.Num())].Sequence;
	return nullptr;
}

UAnimSequenceBase* USkelotAnimCollection::FindAnimByName(const FString& ContainingName)
{
	for (FSkelotSequenceDef& SequenceIter : Sequences)
	{
		if (SequenceIter.Sequence && SequenceIter.Sequence->GetName().Contains(ContainingName))
			return SequenceIter.Sequence;
	}

	return nullptr;
}

USkeletalMesh* USkelotAnimCollection::FindMeshByName(const FString& ContainingName)
{
	for (FSkelotMeshDef& MeshIter : Meshes)
	{
		if (MeshIter.Mesh && MeshIter.Mesh->GetName().Contains(ContainingName))
			return MeshIter.Mesh;
	}
	return nullptr;
}

USkeletalMesh* USkelotAnimCollection::GetRandomMesh() const
{
	if(Meshes.Num())
		return Meshes[FMath::RandHelper(Meshes.Num())].Mesh;
	return nullptr;
}

USkeletalMesh* USkelotAnimCollection::GetRandomMeshFromStream(const FRandomStream& RandomSteam) const
{
	if (Meshes.Num())
		return Meshes[RandomSteam.RandHelper(Meshes.Num())].Mesh;
	return nullptr;
}

void USkelotAnimCollection::InitMeshDataResources(FRHICommandListBase& RHICmdList)
{
	//check(IsInRenderingThread());
	for (const FSkelotMeshDef& MeshDef : Meshes)
	{
		if (MeshDef.MeshData)
		{
			check(MeshDef.Mesh && MeshDef.Mesh->GetResourceForRendering());
			
			MeshDef.Mesh->GetResourceForRendering();

			MeshDef.MeshData->InitMeshData(MeshDef.Mesh->GetResourceForRendering(), MeshDef.BaseLOD);
			MeshDef.MeshData->InitResources(RHICmdList, this);
		}
	}
}

void USkelotAnimCollection::ReleaseMeshDataResources()
{
	check(IsInRenderingThread());
	for (FSkelotMeshDef& MeshDef : Meshes)
	{
		if (MeshDef.MeshData)
		{
			MeshDef.MeshData->ReleaseResources();
			MeshDef.MeshData = nullptr;
		}
	}
}

void USkelotAnimCollection::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
#if WITH_EDITOR
	DestroyBuildData();
#endif
}

//generate skeleton related data 
void USkelotAnimCollection::InitSkeletonData()
{
	{
		this->TransitionsHashTable.Clear(FMath::RoundUpToPowerOfTwo(this->Sequences.Num() * 3));
		if(this->MaxTransitionPose > 0)
		{
			//this->TransitionPoseAllocator.Init(this->MaxTransitionPose);
		}

		if(this->MaxDynamicPose > 0)
		{
			//this->DynamicPoseAllocator.Init(this->MaxDynamicPose);
			this->DynamicPoseFlipFlags.Init(false, this->MaxDynamicPose);
		}
	}

	//cache animation notifies
	for (FSkelotSequenceDef& SD : Sequences)
	{
		SD.Notifies.Empty();
		if (SD.Sequence)
		{
			for (const FAnimNotifyEvent& Notify : SD.Sequence->Notifies)
				ConvertNotification(Notify, SD);
		}
	}

	this->CachedTransforms.Empty();
	this->SkeletonBoneIndexTo_CachedTransforms.Empty();
	this->BonesToCache_Indices.Empty();

	this->RenderRequiredBones.Empty();
	this->RefPoseComponentSpace.Empty();
	this->RefPoseInverse.Empty();

	if (Skeleton)
	{
		//try add bones that are used by physics assets
		if (bCachePhysicsAssetBones)
		{
			for (const FSkelotMeshDef& MeshDef : Meshes)
			{
				if (MeshDef.Mesh && MeshDef.Mesh->GetPhysicsAsset())
				{
					const UPhysicsAsset* PhysAsset = MeshDef.Mesh->GetPhysicsAsset();
					for (const USkeletalBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
					{
						this->BonesToCache.Add(BodySetup->BoneName);
					}
				}
			}
		}
		//find bone indices by name
		this->BonesToCache_Indices.Reserve(this->BonesToCache.Num());
		for (FName BoneName : BonesToCache)
		{
			int SkelBoneIndex = this->Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
			if (SkelBoneIndex != INDEX_NONE)
			{
				this->BonesToCache_Indices.AddUnique((FBoneIndexType)SkelBoneIndex);
			}
			else
			{
				UE_LOG(LogSkelot, Warning, TEXT("Bone [%s] listed in USkelotAnimCollection.BonesToCache not exist in reference skeleton. AnimCollection:%s"), *BoneName.ToString(), *GetFullName());
			}
		}

		InitBoneContainer();


		//init compact physics assets
		for (FSkelotMeshDef& MeshDef : Meshes)
		{
			const UPhysicsAsset* PXAsset = MeshDef.Mesh ? MeshDef.Mesh->GetPhysicsAsset() : nullptr;
			MeshDef.CompactPhysicsAsset = FSkelotCompactPhysicsAsset();
			if (PXAsset)
			{
				MeshDef.CompactPhysicsAsset.Init(this->Skeleton, PXAsset);
			}
		}
	}

	

}

void USkelotAnimCollection::CachePose(int PoseIndex, const TArrayView<FTransform>& PoseComponentSpace)
{
	CachePoseBounds(PoseIndex, PoseComponentSpace);
	CachePoseBones(PoseIndex, PoseComponentSpace);
}

void USkelotAnimCollection::CachePoseBounds(int PoseIndex, const TArrayView<FTransform>& PoseComponentSpace)
{
	check(PoseIndex < this->FrameCountSequences);
	//generate and cache bounds
	for (FSkelotMeshDef& MeshDef : this->Meshes)
	{
		if (MeshDef.Mesh && MeshDef.OwningBoundMeshIndex == -1)	//check if has mesh and needs bound
		{
			FBox3f Box;
			if (MeshDef.Mesh->GetPhysicsAsset())
			{
				Box = (FBox3f)CalcPhysicsAssetBound(MeshDef.Mesh->GetPhysicsAsset(), PoseComponentSpace, false);
			}
			else
			{
				Box = (FBox3f)MeshDef.Mesh->GetBounds().GetBox();
			}

			check(Box.IsValid);

			FBoxCenterExtentFloat BoundCE(Box.ExpandBy(MeshDef.BoundExtent));
			float ExtentSize = BoundCE.Extent.Size();
			MeshDef.MaxBBox.Add(BoundCE);
			if(ExtentSize > 0)
				MeshDef.SmallestBoundRadius = FMath::Min(MeshDef.SmallestBoundRadius, ExtentSize);

			if (MeshDef.OwningBounds.Num())
				MeshDef.OwningBounds[PoseIndex] = BoundCE;

			//MeshDef.MaxCoveringRadius = FMath::Max(MeshDef.MaxCoveringRadius, GetBoxCoveringRadius(MeshBound));
		}
	}
}

void USkelotAnimCollection::CachePoseBones(int PoseIndex, const TArrayView<FTransform>& PoseComponentSpace)
{
	for (FBoneIndexType SkelBoneIndex : this->BonesToCache_Indices)
	{
		FTransform3f& BoneT = this->GetBoneTransformFast(SkelBoneIndex, PoseIndex);
		FCompactPoseBoneIndex CompactBoneIdx = this->AnimationBoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(FSkeletonPoseBoneIndex(SkelBoneIndex));
		BoneT = (FTransform3f)PoseComponentSpace[CompactBoneIdx.GetInt()];
	}
}

bool USkelotAnimCollection::CheckCanBuild(FString& OutErrorString) const
{
	if (!Skeleton || Meshes.Num() == 0)
		return false;

	bool bAnyMesh = false;

	//check meshes are valid
	for (const FSkelotMeshDef& MeshDef : this->Meshes)
	{
		if (IsValid(MeshDef.Mesh))
		{
			if (MeshDef.Mesh->GetSkeleton() != Skeleton)
			{
				OutErrorString = FString::Printf(TEXT("Skeleton Mismatch. AnimSet:%s SkeletalMesh:%s"), *GetName(), *MeshDef.Mesh->GetName());
				return false;
			}
		
			OutErrorString = Utils::CheckMeshIsQualified(MeshDef.Mesh, MeshDef.BaseLOD);
			if (!OutErrorString.IsEmpty())
				return false;

			bAnyMesh = true;
		}
	}

	int FrameCounter = 1;
	
	for (int SequenceIndex = 0; SequenceIndex < this->Sequences.Num(); SequenceIndex++)
	{
		const FSkelotSequenceDef& SequenceStruct = this->Sequences[SequenceIndex];
		if (!SequenceStruct.Sequence)
			continue;

		const int MaxFrame = SequenceStruct.CalcFrameCount();
		FrameCounter += MaxFrame;
	}

	return bAnyMesh;
}

bool USkelotAnimCollection::BuildData()
{
	const double BuildStartTime = FPlatformTime::Seconds();

	bool bDone = BuildAnimationData();
	if(!bDone)
		return false;

	double ExecutionTime = FPlatformTime::Seconds() - BuildStartTime;
	UE_LOG(LogSkelot, Log, TEXT("%s animations build finished in %f seconds"), *GetName(), ExecutionTime);


	BuildMeshData();

	bNeedRebuild = false;
	bIsBuilt = true;

	if (FApp::CanEverRender())
	{
		ENQUEUE_RENDER_COMMAND(InitAnimationBuffer)(&UE::RenderCommandPipe::SkeletalMesh, [this](FRHICommandList& RHICmdList) {
			
			this->AnimationBuffer->InitResource(RHICmdList);
			if(this->CurveBuffer)
				this->CurveBuffer->InitResource(RHICmdList);

			

			//init aim collection uniform buffer
			{
				FSkelotAnimCollectionUniformParams Params;
				Params.BoneCount = this->RenderBoneCount;
				Params.FrameCountSequences = this->FrameCountSequences;
				Params.MaxTransitionPose = this->MaxTransitionPose;
				Params.AnimationBuffer = this->AnimationBuffer->ShaderResourceViewRHI;
				Params.CurveCount = this->CurveBuffer ? this->CurveBuffer->NumCurve : 0;
				Params.CurveBuffer = this->CurveBuffer ? this->CurveBuffer->ShaderResourceViewRHI : GNullVertexBuffer.VertexBufferSRV;
				this->AnimCollectionUB = FSkelotAnimCollectionUniformParamsRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);
			}

			this->InitMeshDataResources(RHICmdList);
		});

#if WITH_EDITOR
		if (GEditor)
		{
			//#Note: FSlateNotificationManager::Get().AddNotification must not be called in PostLoad it flushes current tasks. so we ended up in messed up game play events !!!
			GEditor->GetTimerManager()->SetTimerForNextTick([this]() {
				FNotificationInfo Info = FNotificationInfo(FText::FromString(FString::Printf(TEXT("%s: Build Done!"), *this->GetName())));
				Info.Image = FAppStyle::GetBrush(TEXT("LevelEditor.RecompileGameCode"));
				Info.FadeInDuration = 0.1f;
				Info.FadeOutDuration = 0.5f;
				Info.ExpireDuration = 1.5f;
				Info.bUseThrobber = false;
				Info.bUseSuccessFailIcons = true;
				Info.bUseLargeFont = true;
				Info.bFireAndForget = false;
				Info.bAllowThrottleWhenFrameRateIsLow = false;
				TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
				if (NotificationItem)
				{
					NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
					NotificationItem->ExpireAndFadeout();
					//GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
				}	
			});
		}
#endif
	}

	return true;
}

bool USkelotAnimCollection::BuildAnimationData()
{
	if(!Skeleton || Meshes.Num() == 0)
		return false;

	this->InitSkeletonData();
	
	const FReferenceSkeleton& ReferenceSkelton = this->Skeleton->GetReferenceSkeleton();
	
	int FrameCounter = 1;
	//for each sequence 
	for (int SequenceIndex = 0; SequenceIndex < this->Sequences.Num(); SequenceIndex++)
	{
		FSkelotSequenceDef& SequenceStruct = this->Sequences[SequenceIndex];
		if (!SequenceStruct.Sequence)
			continue;

		UAnimSequence* AsAnimSeq = Cast<UAnimSequence>(SequenceStruct.Sequence);
		if (AsAnimSeq)
		{
#if WITH_EDITOR
			AsAnimSeq->WaitOnExistingCompression(true);
#endif
		}

		const int MaxFrame = SequenceStruct.CalcFrameCount();
		SequenceStruct.AnimationFrameCount = MaxFrame;
		SequenceStruct.AnimationFrameIndex = FrameCounter;

		FrameCounter += MaxFrame;
	}

	this->FrameCountSequences = FrameCounter;
	this->TotalFrameCount = this->FrameCountSequences + this->MaxTransitionPose + (this->MaxDynamicPose * 2);
	this->RenderBoneCount = this->RenderRequiredBones.Num(); //ReferenceSkelton.GetNum();
	this->TotalAnimationBufferSize = this->RenderBoneCount * this->TotalFrameCount * this->GetRenderMatrixSize();
	//LexToString(FUnitConversion::QuantizeUnitsToBestFit((this->RenderBoneCount * this->PoseCount * this->GetRenderMatrixSize()), EUnit::Bytes));

	this->AnimationBuffer = MakeUnique<FSkelotAnimationBuffer>();
	this->AnimationBuffer->InitBuffer(this->RenderBoneCount * this->TotalFrameCount, this->UseHighPrecisionFormat(), true);

	this->CurveBuffer = nullptr;
	if (HasAnyCurve())
	{	
		this->CurveBuffer = MakeUnique<FSkelotCurveBuffer>();
		this->CurveBuffer->InitBuffer(this->TotalFrameCount,  Align(this->CurvesToCache.Num(), 2));
	}

	//reserve bounds
	{
		for (FSkelotMeshDef& MeshDef : this->Meshes)
		{
			MeshDef.MaxBBox = FBoxMinMaxFloat(ForceInit);
			MeshDef.SmallestBoundRadius = FLT_MAX;
			MeshDef.OwningBounds.Empty();

			if (!this->bDontGenerateBounds && MeshDef.Mesh && !this->Meshes.IsValidIndex(MeshDef.OwningBoundMeshIndex))
			{
				MeshDef.OwningBounds.SetNumUninitialized(this->FrameCountSequences);
			}
		}
	}

	//reserve cached transforms
	{
		this->CachedTransforms.Init(FTransform3f::Identity, this->TotalFrameCount * this->BonesToCache_Indices.Num());
		this->SkeletonBoneIndexTo_CachedTransforms.SetNumZeroed(ReferenceSkelton.GetNum());
		for (int i = 0; i < this->BonesToCache_Indices.Num(); i++)
		{
			FBoneIndexType BoneIndex = this->BonesToCache_Indices[i];
			SkeletonBoneIndexTo_CachedTransforms[BoneIndex] = &this->CachedTransforms[i];
		}
	}
	
	


	//0 index is identity data (default pose)
	CachePose(0, this->RefPoseComponentSpace);

	//build animation sequences
	ParallelFor(Sequences.Num(), [this](int SI) {
		this->BuildSequence(SI);

	}, GGenerateSingleThread ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);


	{
		this->MeshesBBox = FBoxCenterExtentFloat(ForceInit);
		FBoxMinMaxFloat MaxPossibleBound(ForceInit);

		for (FSkelotMeshDef& MeshDef : this->Meshes)
		{
			if(MeshDef.Mesh)
			{
				MeshDef.BoundsView = MeshDef.OwningBounds;
				if (this->Meshes.IsValidIndex(MeshDef.OwningBoundMeshIndex)) //get from other MeshDef if its not independent
				{
					MeshDef.BoundsView = this->Meshes[MeshDef.OwningBoundMeshIndex].OwningBounds;
					MeshDef.MaxBBox = this->Meshes[MeshDef.OwningBoundMeshIndex].MaxBBox;
					MeshDef.SmallestBoundRadius = this->Meshes[MeshDef.OwningBoundMeshIndex].SmallestBoundRadius;
				}

				MaxPossibleBound.Add(MeshDef.MaxBBox);
			}
		}
		if (!MaxPossibleBound.IsForceInitValue())
			MaxPossibleBound.ToCenterExtentBox(this->MeshesBBox);
	}


	return true;
}

void USkelotAnimCollection::BuildSequence(int SequenceIndex)
{
	FMemMark MemMarker(FMemStack::Get());	//animation structures use FMemMemStack so we need marker

	FSkelotSequenceDef& SequenceStruct = this->Sequences[SequenceIndex];
	UAnimSequenceBase* AnimSeqBase = SequenceStruct.Sequence;
	if (!AnimSeqBase)
		return;


	UAnimSequence_Public* AnimSeq = static_cast<UAnimSequence_Public*>(AnimSeqBase);
	check(WITH_EDITOR || AnimSeq->IsCompressedDataValid());

	

	TransformArrayAnimStack PoseComponentSpace;
	PoseComponentSpace.SetNumUninitialized(this->AnimationBoneContainer.GetCompactPoseNumBones());

	FMatrix3x4* RenderMatricesFloat = this->UseHighPrecisionFormat() ? this->AnimationBuffer->GetDataPointerHP() : nullptr;
	FMatrix3x4Half* RenderMatricesHalf = !this->UseHighPrecisionFormat() ? this->AnimationBuffer->GetDataPointerLP() : nullptr;
	FFloat16* CurvesBufferValue = this->CurveBuffer ? this->CurveBuffer->Values.GetData() : nullptr;

	FCompactPose CompactPose;
	CompactPose.SetBoneContainer(&this->AnimationBoneContainer);
	FBlendedCurve InCurve;
	InCurve.InitFrom(AnimationBoneContainer);
	UE::Anim::FStackAttributeContainer InAttributes;
	FAnimationPoseData poseData(CompactPose, InCurve, InAttributes);

	const double FrameTime = 1.0f / SequenceStruct.SampleFrequency;

	//for each frame
	for (int SeqFrameIndex = 0; SeqFrameIndex < SequenceStruct.AnimationFrameCount; SeqFrameIndex++)
	{
		const double SampleTime = SeqFrameIndex * FrameTime;
		const double PrevSampleTime = FMath::Max(0, SeqFrameIndex - 1) * FrameTime;
		const int AnimBufferFrameIndex = SequenceStruct.AnimationFrameIndex + SeqFrameIndex;
		


		CompactPose.ResetToRefPose();
		AnimSeqBase->GetAnimationPose(poseData, FAnimExtractContext(SampleTime, this->bExtractRootMotion));
		
		Utils::LocalPoseToComponent(CompactPose, PoseComponentSpace.GetData());
		CachePose(AnimBufferFrameIndex, PoseComponentSpace);

		bool bIdentityMatrix = false;
		if(this->UseHighPrecisionFormat())
		{
			bIdentityMatrix = CalcRenderMatrices(PoseComponentSpace, RenderMatricesFloat + (AnimBufferFrameIndex * this->RenderBoneCount));
		}
		else
		{
			bIdentityMatrix = CalcRenderMatrices(PoseComponentSpace, RenderMatricesHalf + (AnimBufferFrameIndex * this->RenderBoneCount));
		}

		if (CurvesBufferValue)
		{
			//check(this->CurveBuffer->NumCurve == this->CurvesToCache.Num());
			for (int i = 0; i < this->CurvesToCache.Num(); i++)
				CurvesBufferValue[AnimBufferFrameIndex * this->CurveBuffer->NumCurve + i] = InCurve.Get(this->CurvesToCache[i]);
		}


	}
}

void USkelotAnimCollection::BuildMeshData()
{
	double StartTime = FPlatformTime::Seconds();

	//#note mesh data are serialized in cooked builds, but editor generates and cache them using DDC
	
	bool bIsRuneimtAC = !this->IsAsset();

	//build mesh data
	ParallelFor(Meshes.Num(), [&](int MeshDefIdx) {
			
		FSkelotMeshDef& MeshDef = this->Meshes[MeshDefIdx];
		if (MeshDef.Mesh)
		{
			

			if (bIsRuneimtAC)
			{
				check(!MeshDef.MeshData);
				MeshDef.MeshData = MakeShared<FSkelotMeshDataEx>();
				MeshDef.MeshData->InitFromMesh(MeshDef.BaseLOD, MeshDef.Mesh, this);
			}
			else
			{
#if WITH_EDITOR
				check(!MeshDef.MeshData);
				MeshDef.MeshData = MakeShared<FSkelotMeshDataEx>();

				FString MeshDataDDCKey = this->GetDDCKeyForMeshBoneIndex(MeshDef);
				TArray<uint8> DDCData;
				if (GetDerivedDataCacheRef().GetSynchronous(*MeshDataDDCKey, DDCData, this->GetPathName()))
				{
					FMemoryReader DDCReader(DDCData);
					MeshDef.MeshData->Serialize(DDCReader);
				}
				else
				{
					MeshDef.MeshData->InitFromMesh(MeshDef.BaseLOD, MeshDef.Mesh, this);
					FMemoryWriter DDCWriter(DDCData);
					MeshDef.MeshData->Serialize(DDCWriter);
					GetDerivedDataCacheRef().Put(*MeshDataDDCKey, DDCData, this->GetPathName());
				}
#endif
			}
		}

	}, GGenerateSingleThread ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

	double ExecutionTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogSkelot, Log, TEXT("%s mesh builds finished in %f seconds"), *GetName(), ExecutionTime);
	
	this->TotalMeshBonesBufferSize = 0;
	for (FSkelotMeshDef& MeshDef : this->Meshes)
		if (MeshDef.Mesh && MeshDef.MeshData)
			this->TotalMeshBonesBufferSize += MeshDef.MeshData->GetTotalBufferSize();

}

void USkelotAnimCollection::BuildData_FromAsyncDone()
{
	if (!BuildData())
	{
		DestroyBuildData();
	}
}

void USkelotAnimCollection::InitBoneContainer()
{
	const FReferenceSkeleton& SkelRefPose = Skeleton->GetReferenceSkeleton();
	TArray<bool> RequiredBones; //index is Skeleton bone index
	RequiredBones.Init(false, SkelRefPose.GetNum());

	for (const FSkelotMeshDef& MeshDef : this->Meshes)
	{
		if (!MeshDef.Mesh)
			continue;

		const FReferenceSkeleton& MeshRefPose = MeshDef.Mesh->GetRefSkeleton();
		const FSkeletalMeshRenderData* RenderData = MeshDef.Mesh->GetResourceForRendering();
		check(RenderData && RenderData->LODRenderData.Num());
		//bones that are not skinned will be excluded
		for (int LODIndex = MeshDef.BaseLOD; LODIndex < RenderData->LODRenderData.Num(); LODIndex++)
		{
			const FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];
			for (FBoneIndexType MeshBoneIndex : LODRenderData.ActiveBoneIndices)
			{
				int SkelBoneIndex = this->Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex(MeshDef.Mesh, MeshBoneIndex);
				check(SkelBoneIndex != -1);
				RequiredBones[SkelBoneIndex] = true;
			}
		}
	}

	this->RenderRequiredBones.Reserve(RequiredBones.Num());
	this->SkeletonBoneToRenderBone.Init(-1, RequiredBones.Num());

	for (int SkelBoneIndex = 0; SkelBoneIndex < RequiredBones.Num(); SkelBoneIndex++)
	{
		if (RequiredBones[SkelBoneIndex])
		{
			this->SkeletonBoneToRenderBone[SkelBoneIndex] = RenderRequiredBones.Num();
			RenderRequiredBones.Add(SkelBoneIndex);
		}
	}

	//we need all bones so our compact pose bone index is same as skeleton bone index
	TArray<FBoneIndexType> AnimationRequiredBones;
	AnimationRequiredBones.SetNumUninitialized(SkelRefPose.GetNum());
	for (int SkelBoneIndex = 0; SkelBoneIndex < SkelRefPose.GetNum(); SkelBoneIndex++)
		AnimationRequiredBones[SkelBoneIndex] = SkelBoneIndex;

	this->AnimationBoneCount = AnimationRequiredBones.Num();
	this->RenderBoneCount = this->RenderRequiredBones.Num();


	UE::Anim::FCurveFilterSettings CurFillter(UE::Anim::ECurveFilterMode::DisallowFiltered);
	this->AnimationBoneContainer.InitializeTo(MoveTemp(AnimationRequiredBones), CurFillter, *this->Skeleton);
	this->AnimationBoneContainer.SetDisableRetargeting(this->bDisableRetargeting);
	this->AnimationBoneContainer.SetUseRAWData(WITH_EDITOR);	//#TODO forcing to raw data because some animations are not fully loaded at PostLoad
	if (this->RefPoseOverrideMesh && this->RefPoseOverrideMesh->GetSkeleton() == this->Skeleton)
	{
		InitRefPoseOverride();
	}
	else
	{
		Utils::ExtractRefPose(this->AnimationBoneContainer, this->RefPoseComponentSpace, this->RefPoseInverse);
	}
}



void USkelotAnimCollection::InitRefPoseOverride()
{
	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	TArray<FTransform> NewRefPose = Utils::ConvertMeshPoseToSkeleton(this->RefPoseOverrideMesh);
	check(NewRefPose.Num() == RefSkel.GetNum());

	TSharedPtr<FSkelMeshRefPoseOverride> RefPoseOverride = MakeShared<FSkelMeshRefPoseOverride>();
	RefPoseOverride->RefBonePoses = NewRefPose;
	this->AnimationBoneContainer.SetRefPoseOverride(RefPoseOverride);

	Utils::ExtractRefPose(this->AnimationBoneContainer, this->RefPoseComponentSpace, this->RefPoseInverse);
	RefPoseOverride->RefBasesInvMatrix = RefPoseInverse;
}

FBox USkelotAnimCollection::CalcPhysicsAssetBound(const UPhysicsAsset* PhysAsset, const TArrayView<FTransform>& PoseComponentSpace, bool bConsiderAllBodiesForBounds)
{
	//see UPhysicsAsset::CalcAABB for reference

	FBox AssetBound{ ForceInit };

	auto LCalcBodyBound = [&](const UBodySetup* BodySetup) {
		uint32 SkelBoneIndex = this->Skeleton->GetReferenceSkeleton().FindBoneIndex(BodySetup->BoneName);
		if (SkelBoneIndex == INDEX_NONE)
			return;

		FCompactPoseBoneIndex CompactBoneIndex = this->AnimationBoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(FSkeletonPoseBoneIndex(SkelBoneIndex));
		FBox BodyBound = BodySetup->AggGeom.CalcAABB(PoseComponentSpace[CompactBoneIndex.GetInt()]);	//Min Max might be reversed if transform has negative scale
		BodyBound = FBox(BodyBound.Min.ComponentMin(BodyBound.Max), BodyBound.Min.ComponentMax(BodyBound.Max));
		AssetBound += BodyBound;
		};

	if (bConsiderAllBodiesForBounds)
	{
		for (UBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
		{
			if (BodySetup)
				LCalcBodyBound(BodySetup);
		}
	}
	else
	{
		for (int BodyIndex : PhysAsset->BoundsBodies)
		{
			const UBodySetup* BodySetup = PhysAsset->SkeletalBodySetups[BodyIndex];
			if (BodySetup && BodySetup->bConsiderForBounds)
				LCalcBodyBound(BodySetup);
		}
	}

	check(AssetBound.IsValid && !AssetBound.Min.ContainsNaN() && !AssetBound.Max.ContainsNaN());
	return AssetBound;
}

//void USkelotAnimCollection::ExtractCompactPose(const FCompactPose& Pose, FTransform* OutComponentSpace, FMatrix44f* OutShaderSpace)
//{
// 	OutComponentSpace[0] = Pose[FCompactPoseBoneIndex(0)];
// 
// 	for (FCompactPoseBoneIndex CompactIdx(1); CompactIdx < Pose.GetNumBones(); ++CompactIdx)
// 	{
// 		const FCompactPoseBoneIndex ParentIdx = Pose.GetParentBoneIndex(CompactIdx);
// 		OutComponentSpace[CompactIdx.GetInt()] = Pose[CompactIdx] * OutComponentSpace[ParentIdx.GetInt()];
// 	}
// 
// 	for (int i = 0; i < RenderRequiredBones.Num(); i++)
// 	{
// 		FBoneIndexType CompactBoneIdx = RenderRequiredBones[i];
// 		FMatrix44f BoneMatrix = ((FTransform3f)OutComponentSpace[CompactBoneIdx]).ToMatrixWithScale();
// 		OutShaderSpace[i] = this->RefPoseInverse[CompactBoneIdx] * BoneMatrix;
// 	}
//}


bool USkelotAnimCollection::CalcRenderMatrices(const TArrayView<FTransform> PoseComponentSpace, FMatrix3x4* OutMatrices) const
{
	bool bAllIdentity = true;

	for (int i = 0; i < RenderRequiredBones.Num(); i++)
	{
		FBoneIndexType CompactBoneIdx = RenderRequiredBones[i];
		FMatrix44f BoneMatrix = static_cast<FTransform3f>(PoseComponentSpace[CompactBoneIdx]).ToMatrixWithScale();
		FMatrix44f ShaderMatrix = this->RefPoseInverse[CompactBoneIdx] * BoneMatrix;
		SkelotSetMatrix3x4Transpose(OutMatrices[i], ShaderMatrix);

		bool bIdentityMatrix = ShaderMatrix.Equals(FMatrix44f::Identity, 0.01f);
		bAllIdentity &= bIdentityMatrix;
	}

	return bAllIdentity;
}

bool USkelotAnimCollection::CalcRenderMatrices(const TArrayView<FTransform> PoseComponentSpace, FMatrix3x4Half* OutMatrices) const
{
	FMatrix3x4 TmpMatrices[2048];
	bool bAllIdentity = CalcRenderMatrices(PoseComponentSpace, TmpMatrices);

	for (int i = 0; i < RenderRequiredBones.Num(); i++)
		OutMatrices[i] = TmpMatrices[i];

	return bAllIdentity;
}

// 
// void USkelotAnimCollection::CalcRenderMatrices(const TArrayView<FTransform> PoseComponentSpace, FMatrix44f* OutMatrices) const
// {
// 	for (int i = 0; i < RenderRequiredBones.Num(); i++)
// 	{
// 		FBoneIndexType CompactBoneIdx = RenderRequiredBones[i];
// 		FMatrix44f BoneMatrix = ((FTransform3f)PoseComponentSpace[CompactBoneIdx]).ToMatrixWithScale();
// 		OutMatrices[i] = (this->RefPoseInverse[CompactBoneIdx] * BoneMatrix).GetTransposed();
// 	}
// }

uint32 USkelotAnimCollection::GetRenderMatrixSize() const
{
	return UseHighPrecisionFormat() ? sizeof(FMatrix3x4) : sizeof(FMatrix3x4Half);
}

void USkelotAnimCollection::EnqueueReleaseResources()
{
	ENQUEUE_RENDER_COMMAND(ReleaseResoruces)([this](FRHICommandListImmediate& RHICmdList) {
		if(this->AnimationBuffer)
			this->AnimationBuffer->ReleaseResource();
		
		if(this->CurveBuffer)
			this->CurveBuffer->ReleaseResource();

		this->AnimCollectionUB.SafeRelease();

		this->ReleaseMeshDataResources();
		this->ScatterBuffer.Release();
	});
}

UAnimSequenceBase* USkelotAnimCollection::GetRandomAnimSequenceFromStream(const FRandomStream& RandomSteam) const
{
	if (Sequences.Num())
		return Sequences[RandomSteam.RandHelper(Sequences.Num())].Sequence;
	return nullptr;
}

#if WITH_EDITOR
FString USkelotAnimCollection::GetDDCKeyForMeshBoneIndex(const FSkelotMeshDef& MeshDef) const
{
	uint32 BonesHash = FFnv::MemFnv32(this->RenderRequiredBones.GetData(), this->RenderRequiredBones.Num() * this->RenderRequiredBones.GetTypeSize());
	TStringBuilder<800> SBuilder;
	SBuilder.Appendf(TEXT("_MeshBoneIndices_%d_%d_%s"), BonesHash, MeshDef.BaseLOD, *MeshDef.Mesh->GetDerivedDataKey());

	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("SKELOT"), TEXT("6"), SBuilder.GetData());
}
#endif

int USkelotAnimCollection::CalcFrameIndex(const FSkelotSequenceDef& SequenceStruct, float SequencePlayTime) const
{
	int LocalFrameIndex = static_cast<int>(SequencePlayTime * SequenceStruct.SampleFrequency);
	check(LocalFrameIndex < SequenceStruct.AnimationFrameCount);
	int SequenceBufferFrameIndex = SequenceStruct.AnimationFrameIndex + LocalFrameIndex;
	check(SequenceBufferFrameIndex < this->FrameCountSequences);
	return static_cast<int>(SequenceBufferFrameIndex);
}




int USkelotAnimCollection::ReserveUploadData(int FrameCount)
{
	int ScatterIdx = this->CurrentUpload.ScatterData.AddUninitialized(FrameCount);
	this->CurrentUpload.PoseData.AddUninitialized(FrameCount * this->RenderBoneCount);
	if(this->CurveBuffer)
		this->CurrentUpload.CurvesValue.AddUninitialized(FrameCount * this->CurveBuffer->NumCurve);

	return ScatterIdx;
}

void USkelotAnimCollection::UploadDataSetNumUninitialized(int N)
{
	CurrentUpload.ScatterData.SetNumUninitialized(N, EAllowShrinking::No);
	CurrentUpload.PoseData.SetNumUninitialized(N * this->RenderBoneCount, EAllowShrinking::No);
	if(this->CurveBuffer)
		this->CurrentUpload.CurvesValue.SetNumUninitialized(N * this->CurveBuffer->NumCurve);
}

void USkelotAnimCollection::RemoveUnusedTransition(SkelotTransitionIndex UnusedTI)
{
	FTransition& T = this->Transitions[UnusedTI];
	check(T.IsUnused());
	check(!T.IsDeferred());

	this->TransitionsHashTable.Remove(T.GetKeyHash(), UnusedTI);
	this->TransitionPoseAllocator.Free(T.BlockOffset, T.FrameCount);
	this->Transitions.RemoveAt(UnusedTI);
}

void USkelotAnimCollection::RemoveAllUnusedTransitions()
{
	for (SkelotTransitionIndex UnusedTI : NegativeRCTransitions)
	{
		RemoveUnusedTransition(UnusedTI);
	}
	
	NegativeRCTransitions.Reset();
}



int32 USkelotAnimCollection::AllocTransitionPose(int32 FrameCount)
{
	int32 BlockOffset = this->TransitionPoseAllocator.Allocate(FrameCount);
	//exceeding GetMaxSize() means our buffer is full
	if (this->TransitionPoseAllocator.GetMaxSize() > this->MaxTransitionPose)
	{
		this->TransitionPoseAllocator.Free(BlockOffset, FrameCount);
		return -1;
	}
	return BlockOffset;
}

TPair<int, USkelotAnimCollection::ETransitionResult> USkelotAnimCollection::FindOrCreateTransition(const FTransitionKey& Key, bool bIgonreTransitionGeneration)
{
	check(IsInGameThread());

	SKELOT_SCOPE_CYCLE_COUNTER(USkelotAnimCollection_FindOrCreateTransition);

	const uint32 KeyHash = Key.GetKeyHash();
	
	for (uint32 TransitionIndex = this->TransitionsHashTable.First(KeyHash); this->TransitionsHashTable.IsValid(TransitionIndex); TransitionIndex = this->TransitionsHashTable.Next(TransitionIndex))
	{
		FTransition& Trn = this->Transitions[TransitionIndex];
		if (Trn.KeysEqual(Key))
		{
			IncTransitionRef(TransitionIndex);
			return { TransitionIndex, ETR_Success_Found };
		}
	}
	


	//developer settings with console variable was not loading properly so we use CDO instead of CVar
	if (bIgonreTransitionGeneration || GSkelot_NumTransitionGeneratedThisFrame >= GetDefault<USkelotDeveloperSettings>()->MaxTransitionGenerationPerFrame || GSkelot_DisableTransitionGeneration)
		return { -1, ETR_Failed_RateLimitReached };

	if (Transitions.Num() >= 0xFFff) //transition index is uint16
		return { -1, ETR_Failed_BufferFull };

	int32 BlockOffset = AllocTransitionPose(Key.FrameCount);
	if (BlockOffset == -1) //if pool is full free unused transitions 
	{
		while (NegativeRCTransitions.Num())
		{
			//try remove several elements at once
			int N = FMath::Min(8, NegativeRCTransitions.Num());
			while (N > 0)
			{
				SkelotTransitionIndex UnusedTransitionIndex = NegativeRCTransitions.Pop();
				RemoveUnusedTransition(UnusedTransitionIndex);
				N--;
			};

			BlockOffset = AllocTransitionPose(Key.FrameCount);
			if (BlockOffset != -1)
				break;
		};
		
		if (BlockOffset == -1)
		{
			return { -1, ETR_Failed_BufferFull };
		}
	}
	
	NumTransitionFrameAllocated = this->TransitionPoseAllocator.GetSparselyAllocatedSize();
	GSkelot_NumTransitionGeneratedThisFrame++;

	uint32 NewTransitionIndex = Transitions.Add(FTransition{});
	FTransition& NewTransition = Transitions[NewTransitionIndex];
	static_cast<FTransitionKey&>(NewTransition) = Key;
	NewTransition.BlockOffset = BlockOffset;
	NewTransition.FrameIndex = this->FrameCountSequences + BlockOffset;

	//push it for concurrent end of frame generation
	//#Note CachedTransforms of the transitions contain invalid value
	NewTransition.DeferredIndex = this->DeferredTransitions.Add(NewTransitionIndex);
	this->DeferredTransitions_FrameCount += Key.FrameCount;

	this->TransitionsHashTable.Add(KeyHash, NewTransitionIndex);

	return { NewTransitionIndex, ETR_Success_NewlyCreated };
}

void USkelotAnimCollection::IncTransitionRef(SkelotTransitionIndex TransitionIndex)
{
	check(IsInGameThread());
	FTransition& T = this->Transitions[TransitionIndex];

	if (T.RefCount > 0)
	{
		T.RefCount++;
	}
	else
	{
		check(T.RefCount == 0 || T.RefCount == -1);

		TArray<SkelotTransitionIndex>& TargetArray = T.RefCount == 0 ? ZeroRCTransitions : NegativeRCTransitions;
		TargetArray[T.StateIndex] = TargetArray.Last();
		this->Transitions[TargetArray.Last()].StateIndex = T.StateIndex;
		TargetArray.Pop();

		T.StateIndex = ~SkelotTransitionIndex(0);
		T.RefCount = 1;
	}
}

void USkelotAnimCollection::DecTransitionRef(SkelotTransitionIndex& TransitionIndex)
{
	check(IsInGameThread());
	FTransition& T = this->Transitions[TransitionIndex];
	check(T.RefCount > 0)
	T.RefCount--;
	if(T.RefCount == 0)
	{
		T.StateIndex = this->ZeroRCTransitions.Add(TransitionIndex);
	}
	TransitionIndex = ~SkelotTransitionIndex(0);
}

void USkelotAnimCollection::ReleasePendingTransitions()
{
	int BaseIndex = NegativeRCTransitions.AddUninitialized(ZeroRCTransitions.Num());
	for (int i = 0; i < ZeroRCTransitions.Num(); i++)
	{
		SkelotTransitionIndex TI = this->ZeroRCTransitions[i];
		FTransition& T = Transitions[TI];
		T.RefCount = -1;
		T.StateIndex = static_cast<SkelotTransitionIndex>(BaseIndex + i);
		NegativeRCTransitions[T.StateIndex] = TI;
	}

	this->ZeroRCTransitions.Reset();

}

void USkelotAnimCollection::GenerateTransition_Concurrent(uint32 TransitionIndex, uint32 ScatterIdx)
{
	const FTransition& Trs = this->Transitions[TransitionIndex];
	const FSkelotSequenceDef& SequenceStructFrom = this->Sequences[Trs.FromSI];
	const FSkelotSequenceDef& SequenceStructTo = this->Sequences[Trs.ToSI];

	const int TransitionFrameCount = Trs.FrameCount;
	const double TransitionFrameTime = 1.0f / SequenceStructTo.SampleFrequency;
	const double FrameTime = 1.0f / SequenceStructFrom.SampleFrequency;

	INC_DWORD_STAT_BY(STAT_SKELOT_NumTransitionPoseGenerated, TransitionFrameCount);

	for (int i = 0; i < TransitionFrameCount; i++)
		this->CurrentUpload.ScatterData[ScatterIdx + i] = static_cast<uint32>(Trs.FrameIndex + i);

	FMatrix3x4* UploadMatrices = &this->CurrentUpload.PoseData[ScatterIdx * this->RenderBoneCount];
	FFloat16* UploadCurves = this->CurveBuffer ? &this->CurrentUpload.CurvesValue[ScatterIdx * this->CurveBuffer->NumCurve] : nullptr;

	FMemMark MemMarker(FMemStack::Get());

	FCompactPose CompactPoseFrom, CompactPoseTo;
	CompactPoseFrom.SetBoneContainer(&this->AnimationBoneContainer);
	CompactPoseTo.SetBoneContainer(&this->AnimationBoneContainer);
	
	FBlendedCurve CurveFrom, CurveTo;
	CurveFrom.InitFrom(AnimationBoneContainer);
	CurveTo.InitFrom(AnimationBoneContainer);
	UE::Anim::FStackAttributeContainer InAttributes;
	FAnimationPoseData PoseDataFrom(CompactPoseFrom, CurveFrom, InAttributes);
	FAnimationPoseData PoseDataTo(CompactPoseTo, CurveTo, InAttributes);

	{
		const double SampleStartTimeA = Trs.FromFI * FrameTime;
		const double SampleStartTimeB = Trs.ToFI * TransitionFrameTime;

		TransformArrayAnimStack PoseComponentSpace;
		PoseComponentSpace.SetNumUninitialized(this->AnimationBoneContainer.GetCompactPoseNumBones());

		for (int TransitionFrameIndex = 0; TransitionFrameIndex < TransitionFrameCount; TransitionFrameIndex++)
		{
			const double LocalTime = TransitionFrameIndex * TransitionFrameTime;
			const double SampleTimeA = SampleStartTimeA + LocalTime;
			const double SampleTimeB = SampleStartTimeB + LocalTime;
			
			const float TransitionAlpha = (TransitionFrameIndex + 1) / static_cast<float>(TransitionFrameCount + 1);
			check(TransitionAlpha != 0 && TransitionAlpha != 1); //not having any blend is waste
			const float FinalAlpha = FAlphaBlend::AlphaToBlendOption(TransitionAlpha, Trs.BlendOption);
			const int TransitionPoseIndex = Trs.FrameIndex + TransitionFrameIndex;

			SequenceStructFrom.Sequence->GetAnimationPose(PoseDataFrom, FAnimExtractContext(SampleTimeA, this->bExtractRootMotion,{}, Trs.bFromLoops));
			SequenceStructTo.Sequence->GetAnimationPose(PoseDataTo, FAnimExtractContext(SampleTimeB, this->bExtractRootMotion, {}, Trs.bToLoops));

			for (int TransformIndex = 0; TransformIndex < CompactPoseFrom.GetNumBones(); TransformIndex++)
			{
				CompactPoseFrom.GetMutableBones()[TransformIndex].BlendWith(CompactPoseTo.GetMutableBones()[TransformIndex], FinalAlpha);
			}

			Utils::LocalPoseToComponent(CompactPoseFrom, PoseComponentSpace.GetData());
			CachePoseBones(TransitionPoseIndex, PoseComponentSpace);
			CalcRenderMatrices(PoseComponentSpace, UploadMatrices + (TransitionFrameIndex * this->RenderBoneCount));
			
			if(UploadCurves)
			{
				CurveFrom.LerpToValid(CurveTo, FinalAlpha);
				for (int i = 0; i < this->CurvesToCache.Num(); i++)
					UploadCurves[TransitionFrameIndex * this->CurveBuffer->NumCurve + i] = CurveFrom.Get(this->CurvesToCache[i]);
			}
		}
	}
}

void USkelotAnimCollection::FlushDeferredTransitions()
{
	check(IsInGameThread());
	SKELOT_SCOPE_CYCLE_COUNTER(USkelotAnimCollection_FlushDeferredTransitions);

	if (this->DeferredTransitions.Num())
	{
		FMemMark MemMarker(FMemStack::Get());
		int* ScatterIndices = New<int>(FMemStack::Get(), this->DeferredTransitions.Num());

		int ScatterIdx = ReserveUploadData(this->DeferredTransitions_FrameCount);
		for (int i = 0; i < this->DeferredTransitions.Num(); i++)
		{
			FTransition& T = this->Transitions[DeferredTransitions[i]];
			check(T.IsDeferred());
			T.DeferredIndex = -1;
			ScatterIndices[i] = ScatterIdx;
			ScatterIdx += T.FrameCount;
		}

		ParallelFor(DeferredTransitions.Num(), [this, ScatterIndices](int Index) {
			this->GenerateTransition_Concurrent(this->DeferredTransitions[Index], ScatterIndices[Index]);
		});

		this->DeferredTransitions.Reset();
		this->DeferredTransitions_FrameCount = 0;
	}
	
}

void USkelotAnimCollection::ApplyScatterBufferRT(FRHICommandList& RHICmdList, const FPoseUploadData& UploadData)
{
	check(IsInRenderingThread());

	if (UploadData.ScatterData.Num())
	{
		SKELOT_SCOPE_CYCLE_COUNTER(USkelotAnimCollection_ApplyScatterBufferRT);
		//#TODO async or RDG scatter ?
		
		//upload poses
		{
			const uint32 MatrixSize = sizeof(FMatrix3x4);
			const uint32 PoseSizeBytes = this->RenderBoneCount * MatrixSize;
			this->ScatterBuffer.Init(UploadData.ScatterData, PoseSizeBytes, true, TEXT("AnimCollectionScatter"));
			FMemory::Memcpy(this->ScatterBuffer.UploadData, UploadData.PoseData.GetData(), UploadData.PoseData.Num() * MatrixSize);

			FRWBuffer buffData;
			buffData.Buffer = this->AnimationBuffer->Buffer;
			buffData.SRV = this->AnimationBuffer->ShaderResourceViewRHI;
			buffData.UAV = this->AnimationBuffer->UAV;
			this->ScatterBuffer.ResourceUploadTo(RHICmdList, buffData, true);
		}


		//upload curves
		if(this->CurveBuffer)
		{
			const uint32 ValuePackSize = sizeof(FFloat16) * this->CurveBuffer->NumCurve;
			check(IsAligned(ValuePackSize, 4));
			this->ScatterBuffer.Init(UploadData.ScatterData, ValuePackSize, false, TEXT("AnimCollectionScatter"));
			FMemory::Memcpy(this->ScatterBuffer.UploadData, UploadData.CurvesValue.GetData(), UploadData.CurvesValue.Num() * UploadData.CurvesValue.GetTypeSize());

			FRWByteAddressBuffer buffData;
			buffData.Buffer = this->CurveBuffer->Buffer;
			buffData.SRV = this->CurveBuffer->ShaderResourceViewRHI;
			buffData.UAV = this->CurveBuffer->UAV;

			this->ScatterBuffer.ResourceUploadTo(RHICmdList, buffData, true);
		}
	}
}


FMatrix3x4* USkelotAnimCollection::RequestPoseUpload(int PoseIndex)
{
	int ScatterIdx = ReserveUploadData(1);
	this->CurrentUpload.ScatterData[ScatterIdx] = PoseIndex;
	return &this->CurrentUpload.PoseData[ScatterIdx * RenderBoneCount];
}

void USkelotAnimCollection::RequestPoseUpload(int PoseIndex, const TArrayView<FMatrix3x4> PoseTransforms)
{
	check(PoseTransforms.Num() == RenderBoneCount);
	FMatrix3x4* Dst = RequestPoseUpload(PoseIndex);
	FMemory::Memcpy(Dst, PoseTransforms.GetData(), sizeof(FMatrix3x4) * RenderBoneCount);
}
 


int32 USkelotAnimCollection::AllocDynamicPose()
{
	int32 Offset = DynamicPoseAllocator.Allocate();
	if (this->DynamicPoseAllocator.GetMaxSize() > this->MaxDynamicPose)
	{
		this->DynamicPoseAllocator.Free(Offset);
		return -1;
	}
	return Offset;
}

FSkelotMeshDef::FSkelotMeshDef()
{
	Mesh = nullptr;
	BaseLOD = 0;
	BoundExtent = FVector3f::ZeroVector;
	MaxBBox = FBoxMinMaxFloat(ForceInit);
	//MaxCoveringRadius = 0;
	OwningBoundMeshIndex = -1;

}

const FSkeletalMeshLODRenderData& FSkelotMeshDef::GetBaseLODRenderData() const
{
	const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
	check(RenderData && RenderData->LODRenderData.Num());
	return RenderData->LODRenderData[FMath::Min(this->BaseLOD, static_cast<uint8>(RenderData->LODRenderData.Num() - 1))];
}

void FSkelotMeshDef::SerializeCooked(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		bool bHasMeshData = MeshData.IsValid() && MeshData->LODs.Num();
		Ar << bHasMeshData;
		if (bHasMeshData)
		{
			MeshData->Serialize(Ar);
		}
	}
	else
	{
		bool bHasMeshData = false;
		Ar << bHasMeshData;
		if(bHasMeshData)
		{
			MeshData = MakeShared<FSkelotMeshDataEx>();
			MeshData->Serialize(Ar);
		}
		else
		{
			UE_LOGFMT(LogSkelot, Warning, "MeshDataEx no available.");
		}
	}
}

FSkelotSequenceDef::FSkelotSequenceDef()
{
	Sequence = nullptr;
	SampleFrequency = 60;
	AnimationFrameIndex = 0;
	AnimationFrameCount = 0;
}

int FSkelotSequenceDef::CalcFrameIndex(float time) const
{
	check(time >= 0 && time <= GetSequenceLength());
	int seqIdx = (int)(time * SampleFrequency);
	check(seqIdx < AnimationFrameCount);
	return AnimationFrameIndex + seqIdx;
}

float FSkelotSequenceDef::GetSequenceLength() const
{
	return Sequence->GetPlayLength();
}

int FSkelotSequenceDef::CalcFrameCount() const
{
	const int MaxFrame = static_cast<int>(SampleFrequency * Sequence->GetPlayLength()) + 1;
	return MaxFrame;
}

void FSkelotCompactPhysicsAsset::Init(const USkeleton* Skeleton, const UPhysicsAsset* PhysAsset)
{
	for (const USkeletalBodySetup* Body : PhysAsset->SkeletalBodySetups)
	{
		int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(Body->BoneName);
		if(BoneIndex != -1)
		{
			for (const FKSphereElem& Shape : Body->AggGeom.SphereElems)
			{
				Spheres.Emplace(FShapeSphere{ Shape.Center, Shape.Radius, BoneIndex });
			}
			for (const FKSphylElem& Shape : Body->AggGeom.SphylElems)
			{
				const FVector Axis = Shape.Rotation.RotateVector(FVector(0, 0, Shape.Length * 0.5f));
				Capsules.Emplace(FShapeCapsule{ Shape.Center - Axis, Shape.Center + Axis , Shape.Radius, BoneIndex });
			}
			for (const FKBoxElem& Shape : Body->AggGeom.BoxElems)
			{
				const FVector HalfExtent = FVector(Shape.X, Shape.Y, Shape.Z) * 0.5f;
				const FQuat BoxQuat = Shape.Rotation.Quaternion();

				if (BoxQuat.IsIdentity()) //see ChaosInterface.CreateGeometry  AABB can handle translations internally but if we have a rotation we need to wrap it in a transform
				{
					Boxes.Emplace(FShapeBox{ FQuat::Identity, FVector::ZeroVector, Shape.Center - HalfExtent, Shape.Center + HalfExtent, BoneIndex, false });
				}
				else
				{
					Boxes.Emplace(FShapeBox{ BoxQuat, Shape.Center, -HalfExtent, HalfExtent, BoneIndex, true });
				}
			}
		}
	}

}


int FSkelotCompactPhysicsAsset::Overlap(const USkelotAnimCollection* AnimCollection, int FrameIndex, const FVector& Point, Chaos::FReal Thickness) const
{
	const auto ThicknessSQ = Thickness * Thickness;

	auto InvertByBone = [&](int BoneIndex) {
		check(AnimCollection->IsBoneTransformCached(BoneIndex));
		FTransform BoneT = (FTransform)AnimCollection->GetBoneTransformFast(BoneIndex, FrameIndex);
		return BoneT.InverseTransformPosition(Point);
	};

	for (const FShapeCapsule& Capsule : Capsules)
	{
		FVector P = InvertByBone(Capsule.BoneIndex);
		if (FMath::PointDistToSegmentSquared(P, Capsule.A, Capsule.B) <= ThicknessSQ)
			return Capsule.BoneIndex;
	}

	for (const FShapeSphere& Sphere : Spheres)
	{
		FVector P = InvertByBone(Sphere.BoneIndex);
		if (FVector::DistSquared(Sphere.Center, P) <= FMath::Square(Thickness + Sphere.Radius))
			return Sphere.BoneIndex;
	}

	for (const FShapeBox& Box : Boxes)
	{
		FVector P = InvertByBone(Box.BoneIndex);
		if (Box.bHasTransform)
		{
			P = FTransform(Box.Rotation, Box.Center).InverseTransformPositionNoScale(P);
		}

		if (FMath::SphereAABBIntersection(P, ThicknessSQ, FBox(Box.BoxMin, Box.BoxMax)))
			return Box.BoneIndex;
	}

	return -1;
}


void FSkelotCompactPhysicsAsset::Raycast(FRaycastParams& Params) const
{
	Params.OutBoneIndex = -1;

	//takes the bone's transform and performs raycast over the shape
	auto LRayCast = [&Params](int32 BoneIndex, auto&& RayCastFunc) {

		if (Params.BonesToExclude.Contains(BoneIndex))
			return false;

		if (!Params.AnimCollection->IsBoneTransformCached(BoneIndex))
		{
			FName BoneName = Params.AnimCollection->GetBoneName(BoneIndex);
			UE_LOGFMT(LogSkelot, Error, "Bone {0} transform not cached, check USkelotAnimCollection.bCachePhysicsAssetBones. AnimCollection:{1}", BoneName, Params.AnimCollection->GetFName());
			return false;
		}

		FTransform BoneT = (FTransform)Params.AnimCollection->GetBoneTransformFast(BoneIndex, Params.FrameIndex);
		check(BoneT.IsValid());

		Chaos::FReal TOI;
		Chaos::FVec3 POI, NOI;

		const bool bHit = SkelotTransformedRaycast(BoneT, Params.StartPoint, Params.Direction, Params.Length, Params.Thickness, TOI, POI, NOI, MoveTemp(RayCastFunc));
		if (bHit)
		{
			if (Params.OutBoneIndex == -1 || TOI < Params.OutTime)
			{
				Params.OutBoneIndex = BoneIndex;
				Params.OutTime = TOI;
				Params.OutPosition = POI;
				Params.OutNormal = NOI;
			}
		}

		return bHit;
		};

	for (const FShapeCapsule& Capsule : Capsules) //for each capsule shape
	{
		bool bHit = LRayCast(Capsule.BoneIndex, [&Capsule](const Chaos::FVec3& S, const Chaos::FVec3& D, const Chaos::FReal L, const Chaos::FReal T, Chaos::FReal& TOI, Chaos::FVec3& POI, Chaos::FVec3& NOI) {
			int32 Unused;
			return Chaos::FCapsule(Capsule.A, Capsule.B, Capsule.Radius).Raycast(S, D, L, T, TOI, POI, NOI, Unused);
			});

		if (Params.bReturnFirstHit && bHit)
			return;
	}

	for (const FShapeSphere& Sphere : Spheres) //for each sphere shape
	{
		bool bHit = LRayCast(Sphere.BoneIndex, [&Sphere](const Chaos::FVec3& S, const Chaos::FVec3& D, const Chaos::FReal L, const Chaos::FReal T, Chaos::FReal& TOI, Chaos::FVec3& POI, Chaos::FVec3& NOI) {
			int32 Unused;
			return Chaos::FImplicitSphere3(Sphere.Center, Sphere.Radius).Raycast(S, D, L, T, TOI, POI, NOI, Unused);
			});

		if (Params.bReturnFirstHit && bHit)
			return;
	}

	for (const FShapeBox& Box : Boxes) //for each box shape
	{
		bool bHit = LRayCast(Box.BoneIndex, [&Box](const Chaos::FVec3& S, const Chaos::FVec3& D, const Chaos::FReal L, const Chaos::FReal T, Chaos::FReal& TOI, Chaos::FVec3& POI, Chaos::FVec3& NOI) {
			int32 Unused;
			if (Box.bHasTransform)
			{
				Chaos::FVec3 LS, LD;
				FTransform BT(Box.Rotation, Box.Center);
				LS = BT.InverseTransformPositionNoScale(S);
				LD = BT.InverseTransformVectorNoScale(D);
				if (Chaos::FAABB3(Box.BoxMin, Box.BoxMax).Raycast(LS, LD, L, T, TOI, POI, NOI, Unused))
				{
					POI = BT.TransformPositionNoScale(POI);
					NOI = BT.TransformVectorNoScale(NOI);
					return true;
				}
				return false;
			}
			else
			{
				return (Chaos::FAABB3(Box.BoxMin, Box.BoxMax).Raycast(S, D, L, T, TOI, POI, NOI, Unused));
			}
			});

		if (Params.bReturnFirstHit && bHit)
			return;
	}
}


void USkelotAnimCollection::OnPreSendAllEndOfFrameUpdates(UWorld* World)
{
	FlushDeferredTransitions();

	if (this->CurrentUpload.ScatterData.Num())
	{
		ENQUEUE_RENDER_COMMAND(ScatterUpdate)([this, UploadData = MoveTemp(CurrentUpload)](FRHICommandListImmediate& RHICmdList) {
			
			this->ApplyScatterBufferRT(RHICmdList, UploadData);
		});
	}
}
void USkelotAnimCollection::OnEndFrame()
{
}

void USkelotAnimCollection::OnBeginFrameRT()
{

}

void USkelotAnimCollection::OnEndFrameRT()
{

}

void USkelotAnimCollection::OnBeginFrame()
{
	ReleasePendingTransitions();

}

void USkelotAnimCollection::ConvertNotification(const FAnimNotifyEvent& Notify /*From*/, FSkelotSequenceDef& SequenceDef /*To*/)
{
	if (Notify.Notify && ForbiddenNotifies.Contains(Notify.Notify->GetClass()))
		return;
	
	if (ISkelotNotifyInterface* AsSkelotInterface = Cast<ISkelotNotifyInterface>(Notify.Notify))
	{
		SequenceDef.Notifies.Emplace(FSkelotAnimNotifyDesc{ Notify.GetTriggerTime(), Notify.NotifyName, Notify.Notify, AsSkelotInterface });
		return;
	}

	//convert UAnimNotify_SkelotPlaySound to UAnimNotify_SkelotPlaySound
	if (UAnimNotify_PlaySound* AsPlaySound = Cast<UAnimNotify_PlaySound>(Notify.Notify))
	{
		UAnimNotify_SkelotPlaySound* SkelotNotify = NewObject<UAnimNotify_SkelotPlaySound>();
		UEngine::CopyPropertiesForUnrelatedObjects(AsPlaySound, SkelotNotify);
		SequenceDef.Notifies.Emplace(FSkelotAnimNotifyDesc{ Notify.GetTriggerTime(), Notify.NotifyName, SkelotNotify, Cast<ISkelotNotifyInterface>(SkelotNotify) });
		return;
	}

	//convert UAnimNotify_PlayNiagaraEffect to UAnimNotify_SkelotPlayNiagaraEffect
	if (UAnimNotify_PlayNiagaraEffect* AsPlayNiagara = Cast<UAnimNotify_PlayNiagaraEffect>(Notify.Notify)) 
	{
		UAnimNotify_SkelotPlayNiagaraEffect* SkelotNotify = NewObject<UAnimNotify_SkelotPlayNiagaraEffect>();
		UEngine::CopyPropertiesForUnrelatedObjects(AsPlayNiagara, SkelotNotify);
		SequenceDef.Notifies.Emplace(FSkelotAnimNotifyDesc{ Notify.GetTriggerTime(), Notify.NotifyName, SkelotNotify, Cast<ISkelotNotifyInterface>(SkelotNotify) });
		return;
	}

	if (Notify.NotifyStateClass == nullptr && Notify.Notify == nullptr) //name only notification ?
	{
		SequenceDef.Notifies.Emplace(FSkelotAnimNotifyDesc{ Notify.GetTriggerTime(), Notify.NotifyName, nullptr, nullptr });
		return;
	}

}




