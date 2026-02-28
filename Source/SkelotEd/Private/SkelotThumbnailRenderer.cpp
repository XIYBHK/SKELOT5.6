#include "SkelotThumbnailRenderer.h"
#include "SkelotAnimCollection.h"
#include "AssetThumbnail.h"
#include "Slate/SlateTextures.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "CanvasTypes.h"

void USkelotThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	USkelotAnimCollection* AnimCollection = Cast<USkelotAnimCollection>(Object);
	if (!AnimCollection || !AnimCollection->Skeleton)
		return;

	TSharedRef<FSkeletalMeshThumbnailScene> ThumbnailScene = ThumbnailSceneCache.EnsureThumbnailScene(Object);

	//FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	//const TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(USkeleton::StaticClass()).Pin();
	//ThumbnailScene->SetDrawDebugSkeleton(true, AssetTypeActions->GetTypeColor());

	constexpr bool bFindIfNotSet = true;
	if (USkeletalMesh* SkeletalMesh = AnimCollection->Skeleton->GetPreviewMesh(bFindIfNotSet))
	{
		ThumbnailScene->SetSkeletalMesh(SkeletalMesh);
	}

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
		.SetTime(UThumbnailRenderer::GetTime())
		.SetAdditionalViewFamily(bAdditionalViewFamily));

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));
	ThumbnailScene->SetSkeletalMesh(nullptr);

#if 0 //!!!reading from cached didn't work SkeletonTT was null all the time
		TSharedRef<FAssetThumbnail> SkeletonTN = MakeShared<FAssetThumbnail>(AnimCollection->Skeleton, Width, Height, UThumbnailManager::Get().GetSharedThumbnailPool());
		SkeletonTN->RefreshThumbnail();
		ISlateViewport* IVP = &SkeletonTN.Get();
		FSlateTexture2DRHIRef* SkeletonTT = static_cast<FSlateTexture2DRHIRef*>(IVP->GetViewportRenderTargetTexture());
		if (SkeletonTT && SkeletonTT->IsValid())
		{
			FTexture Tx;
			Tx.TextureRHI = SkeletonTT->GetRHIRef();
			Canvas->DrawTile(X, Y, Width, Height, 0, 0, 1, 1, FLinearColor::White, &Tx, SE_BLEND_AlphaBlend);
		}
#endif

}

bool USkelotThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	USkelotAnimCollection* AnimCollection = Cast<USkelotAnimCollection>(Object);
	if (AnimCollection && AnimCollection->Skeleton)
		return Super::CanVisualizeAsset(AnimCollection->Skeleton);

	return false;
}

EThumbnailRenderFrequency USkelotThumbnailRenderer::GetThumbnailRenderFrequency(UObject* Object) const
{
	USkelotAnimCollection* AnimCollection = Cast<USkelotAnimCollection>(Object);
	return Super::GetThumbnailRenderFrequency(AnimCollection ? AnimCollection->Skeleton : nullptr);
}


