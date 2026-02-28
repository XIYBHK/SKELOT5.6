// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotUtils.h"

#include "DrawDebugHelpers.h"

void FSkelotUtils::DrawDebugCircle2D(const UWorld* InWorld, FVector Center, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines /*= false*/, float LifeTime /*= -1.f*/, uint8 DepthPriority /*= 0*/, float Thickness /*= 0.f*/)
{
	::DrawDebugCircle(InWorld, Center, Radius, Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness, FVector::XAxisVector, FVector::YAxisVector, false);
}


