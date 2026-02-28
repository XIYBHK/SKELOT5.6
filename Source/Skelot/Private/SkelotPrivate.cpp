// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotPrivate.h"

#include "HAL/IConsoleManager.h"
#include "SkelotSettings.h"



DEFINE_STAT(STAT_SKELOT_ViewNumVisible);
DEFINE_STAT(STAT_SKELOT_ShadowNumVisible);

DEFINE_STAT(STAT_SKELOT_NumTransitionPoseGenerated);
DEFINE_STAT(STAT_SKELOT_TotalInstances);
DEFINE_STAT(STAT_SKELOT_TotalClusters);
DEFINE_STAT(STAT_SKELOT_TotalRenderDescs);
DEFINE_STAT(STAT_SKELOT_NumCompDirtied);

DEFINE_STAT(STAT_SKELOT_AvgInsCountPerCluster);


float GSkelot_LocalBoundUpdateInterval = 1 / 25.0f;
FAutoConsoleVariableRef CVar_LocalBoundUpdateInterval(TEXT("Skelot.LocalBoundUpdateInterval"), GSkelot_LocalBoundUpdateInterval, TEXT(""), ECVF_Default);

bool GSkelot_CallAnimNotifies = true;


FAutoConsoleVariableRef CVar_CallAnimNotifies(TEXT("Skelot.CallAnimNotifies"), GSkelot_CallAnimNotifies, TEXT(""), ECVF_Default);

float GSkelot_DistanceScale = 1;
FAutoConsoleVariableRef CVar_DistanceScale(TEXT("skelot.DistanceScale"), GSkelot_DistanceScale, TEXT("scale used for distance based LOD. higher value results in higher LOD."), ECVF_Default);

bool GSkelot_DisableFrustumCull = false;
FAutoConsoleVariableRef CVar_DisableFrustumCull(TEXT("skelot.DisableFrustumCull"), GSkelot_DisableFrustumCull, TEXT(""), ECVF_Default);

bool GSkelot_DisableGridCull = false;
FAutoConsoleVariableRef CVar_DisableGriding(TEXT("skelot.DisableGridCull"), GSkelot_DisableGridCull, TEXT(""), ECVF_Default);

int32 GSkelot_NumInstancePerGridCell = 256;
FAutoConsoleVariableRef CVar_NumInstancePerGridCell(TEXT("skelot.NumInstancePerGridCell"), GSkelot_NumInstancePerGridCell, TEXT(""), ECVF_Default);

bool GSkelot_DisableSectionsUnification = false;
FAutoConsoleVariableRef CVar_DisableSectionsUnification(TEXT("skelot.DisableSectionsUnification"), GSkelot_DisableSectionsUnification, TEXT(""), ECVF_Default);

bool GSkelot_FlatInstaneRun = false;
FAutoConsoleVariableRef CVar_FlatInstaneRun(TEXT("skelot.FlatInstaneRun"), GSkelot_FlatInstaneRun, TEXT(""), ECVF_Default);

bool GSkelot_UseStaticDrawList = false;
FAutoConsoleVariableRef CVar_UseStaticDrawList(TEXT("skelot.UseStaticDrawList"), GSkelot_UseStaticDrawList, TEXT(""), ECVF_Default);


float GSkelot_ClusterCellSize = 40000;
FAutoConsoleVariableRef CVar_ClusterCellSize(TEXT("skelot.ClusterCellSize"), GSkelot_ClusterCellSize, TEXT(""), ECVF_Default);


bool GSkelot_DisableTransitionGeneration = false;
FAutoConsoleVariableRef CV_DisableTransitionGeneration(TEXT("skelot.DisableTransitionGeneration"), GSkelot_DisableTransitionGeneration, TEXT("true if no more transition should be generated. only those in cache are used."), ECVF_Default);


bool GSkelot_DisableTransition = false;
FAutoConsoleVariableRef CV_DisableTransition(TEXT("skelot.DisableTransition"), GSkelot_DisableTransition, TEXT("if true no animation will be played with transition."), ECVF_Default);

bool GSkelot_ForcePerInstanceLocalBounds = false;
FAutoConsoleVariableRef CV_ForcePerInstanceLocalBounds(TEXT("skelot.ForcePerInstanceLocalBounds"), GSkelot_ForcePerInstanceLocalBounds, TEXT(""), ECVF_Default);

bool GSkelot_ForceDefaultMaterial = false;
FAutoConsoleVariableRef CV_ForceDefaultMaterial(TEXT("skelot.ForceDefaultMaterial"), GSkelot_ForceDefaultMaterial, TEXT(""), ECVF_Default);


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

bool GSkelot_DebugTransitions = false;
FAutoConsoleVariableRef CV_DebugTransitions(TEXT("skelot.DebugTransitions"), GSkelot_DebugTransitions, TEXT(""), ECVF_Default);

bool GSkelot_DebugAnimations = false;
FAutoConsoleVariableRef CV_DebugAnimations(TEXT("skelot.DebugAnimations"), GSkelot_DebugAnimations, TEXT(""), ECVF_Default);

bool GSkelot_DrawInstanceBounds = false;
FAutoConsoleVariableRef CVar_DrawInstanceBounds(TEXT("skelot.DrawInstanceBounds"), GSkelot_DrawInstanceBounds, TEXT(""), ECVF_Default);

int32 GSkelot_MaxTrianglePerInstance = -1;
FAutoConsoleVariableRef CVar_MaxTrianglePerInstance(TEXT("skelot.MaxTrianglePerInstance"), GSkelot_MaxTrianglePerInstance, TEXT("limits the per instance triangle counts, used for debug/profile purposes. <= 0 to disable"), ECVF_Default);

int32 GSkelot_FroceMaxBoneInfluence = -1;
FAutoConsoleVariableRef CVar_FroceMaxBoneInfluence(TEXT("skelot.FroceMaxBoneInfluence"), GSkelot_FroceMaxBoneInfluence, TEXT("limits the MaxBoneInfluence for all instances, -1 to disable"), ECVF_Default);

bool GSkelot_DrawCells = false;
FAutoConsoleVariableRef CVar_DrawCells(TEXT("skelot.DrawCells"), GSkelot_DrawCells, TEXT(""), ECVF_Default);

bool GSkelot_DebugClusters = false;
FAutoConsoleVariableRef CVar_DebugClusters(TEXT("skelot.DebugClusters"), GSkelot_DebugClusters, TEXT(""), ECVF_Default);

int32 GSkelot_ForcedAnimFrameIndex = -1;
FAutoConsoleVariableRef CVar_ForcedAnimFrameIndex(TEXT("skelot.ForcedAnimFrameIndex"), GSkelot_ForcedAnimFrameIndex, TEXT("force all instances to use the specified animation frame index. -1 to disable"), ECVF_Default);

#endif