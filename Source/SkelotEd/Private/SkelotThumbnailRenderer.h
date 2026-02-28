// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailHelpers.h"
#include "UObject/ObjectMacros.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "ThumbnailRendering/SkeletonThumbnailRenderer.h"
#include "SkelotThumbnailRenderer.generated.h"


UCLASS()
class USkelotThumbnailRenderer : public USkeletonThumbnailRenderer
{
	GENERATED_BODY()

	void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas, bool bAdditionalViewFamily) override;

	bool CanVisualizeAsset(UObject* Object) override;

	EThumbnailRenderFrequency GetThumbnailRenderFrequency(UObject* Object) const override;
};

