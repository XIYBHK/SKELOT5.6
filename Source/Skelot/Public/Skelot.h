// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"
#include "Logging/StructuredLog.h"



DECLARE_LOG_CATEGORY_EXTERN(LogSkelot, Log, All);

class FSkelotModule : public IModuleInterface
{
public:

	FDelegateHandle PreSendAllEndOfFrameUpdatesHandle;
	FDelegateHandle OnWorldTickStartHandle;
	FDelegateHandle OnWorldTickEndHandle;
	FDelegateHandle OnWorldPreActorTickHandle;
	FDelegateHandle OnWorldPostActorTickHandle;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static void OnBeginFrame();
	static void OnEndFrame();
};

static constexpr int SKELOT_MAX_LOD = 8;
static constexpr int SKELOT_MAX_BONE_INFLUENCE = 8;

#define SKELOT_UE_VERSION 5.5	//the version of engine this plugin is for

