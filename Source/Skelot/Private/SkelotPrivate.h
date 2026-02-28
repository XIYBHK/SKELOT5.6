// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "Skelot.h"

DECLARE_STATS_GROUP(TEXT("Skelot"), STATGROUP_SKELOT, STATCAT_Advanced);



DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("ViewNumVisibleInstance"), STAT_SKELOT_ViewNumVisible, STATGROUP_SKELOT, SKELOT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("ShadowNumVisibleInstance"), STAT_SKELOT_ShadowNumVisible, STATGROUP_SKELOT, SKELOT_API);


DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("NumTransitionPoseGenerated"), STAT_SKELOT_NumTransitionPoseGenerated, STATGROUP_SKELOT, SKELOT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("TotalInstances"), STAT_SKELOT_TotalInstances, STATGROUP_SKELOT, SKELOT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("TotalClusters"), STAT_SKELOT_TotalClusters, STATGROUP_SKELOT, SKELOT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("TotalRenderDescs"), STAT_SKELOT_TotalRenderDescs, STATGROUP_SKELOT, SKELOT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("NumCompDirtied"), STAT_SKELOT_NumCompDirtied, STATGROUP_SKELOT, SKELOT_API);



DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("AvgInstanceCountPerClusterComp"), STAT_SKELOT_AvgInsCountPerCluster, STATGROUP_SKELOT, SKELOT_API);


#define SKELOT_SCOPE_CYCLE_COUNTER(CounterName) DECLARE_SCOPE_CYCLE_COUNTER(TEXT(#CounterName), STAT_SKELOT_##CounterName, STATGROUP_SKELOT)


#ifndef SKELOT_WITH_EXTRA_BONE
#define SKELOT_WITH_EXTRA_BONE 1
#endif


extern float	GSkelot_LocalBoundUpdateInterval;
extern bool		GSkelot_CallAnimNotifies;
extern bool		GSkelot_DisableTransitionGeneration;
extern bool		GSkelot_DisableTransition;
extern float	GSkelot_LocalBoundUpdateInterval;
extern bool		GSkelot_CallAnimNotifies;


extern float	GSkelot_DistanceScale;
extern bool		GSkelot_DisableFrustumCull;
extern bool		GSkelot_DisableGridCull;
extern int32	GSkelot_NumInstancePerGridCell;
extern bool		GSkelot_DisableSectionsUnification;
extern bool		GSkelot_FlatInstaneRun;
extern bool		GSkelot_UseStaticDrawList;


extern float	GSkelot_ClusterCellSize;
extern bool		GSkelot_ForcePerInstanceLocalBounds;
extern bool		GSkelot_ForceDefaultMaterial;


//CVars available in debug but excluded in shipping
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

extern bool		GSkelot_DebugAnimations;
extern bool		GSkelot_DebugTransitions;
extern bool		GSkelot_DrawInstanceBounds;
extern int32	GSkelot_MaxTrianglePerInstance;
extern int32	GSkelot_FroceMaxBoneInfluence;
extern bool		GSkelot_DrawCells;
extern bool		GSkelot_DebugClusters;
extern int32	GSkelot_ForcedAnimFrameIndex;

#else

constexpr bool	GSkelot_DebugAnimations = false;
constexpr bool	GSkelot_DebugTransitions = false;
constexpr bool	GSkelot_DrawInstanceBounds = false;
constexpr int32	GSkelot_MaxTrianglePerInstance = -1;
constexpr int32	GSkelot_FroceMaxBoneInfluence = -1;
constexpr bool	GSkelot_DrawCells = false;
constexpr bool	GSkelot_DebugClusters = false;
constexpr int32	GSkelot_ForcedAnimFrameIndex = -1;

#endif



namespace Skelot
{
	static int32 OverrideNumPrimitive(int32 Value)
	{
		if (!UE_BUILD_SHIPPING)
			return GSkelot_MaxTrianglePerInstance > 0 ? FMath::Min(GSkelot_MaxTrianglePerInstance, Value) : Value;

		return Value;
	}
	static int32 OverrideMaxBoneInfluence(int32 Value)
	{
		if (!UE_BUILD_SHIPPING)
			return GSkelot_FroceMaxBoneInfluence > 0 ? FMath::Min(8, GSkelot_FroceMaxBoneInfluence) : Value;

		return Value;
	}
	static int32 OverrideAnimFrameIndex(uint32 Value)
	{
		if (!UE_BUILD_SHIPPING && GSkelot_ForcedAnimFrameIndex >= 0)
			return GSkelot_ForcedAnimFrameIndex;

		return Value;
	};
};