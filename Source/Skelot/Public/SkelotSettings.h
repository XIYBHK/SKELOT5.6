// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "CoreMinimal.h"

#include "SkelotSettings.generated.h"

class ASkelotWorld;

// Skelot集群模式
UENUM(BlueprintType)
enum class ESkelotClusterMode : uint8
{
	//don't use any clustering
	None	UMETA(DisplayName="无"),
	//
	Tiled	UMETA(DisplayName="平铺"),
};

UCLASS(Config = Skelot, defaultconfig, meta = (DisplayName = "Skelot"))
class SKELOT_API USkelotDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()
public:

	UPROPERTY(Config, EditAnywhere, Category = "设置", meta = (DisplayName = "每帧最大过渡生成数"))
	int32 MaxTransitionGenerationPerFrame;
	UPROPERTY(Config, EditAnywhere, Category = "设置", meta = (DisplayName = "集群生命周期"))
	uint32 ClusterLifeTime = 600;
	UPROPERTY(Config, EditAnywhere, Category = "设置", meta = (DisplayName = "渲染描述生命周期"))
	uint32 RenderDescLifetime = 60;
	UPROPERTY(Config, EditAnywhere, Category = "设置", meta = (MetaClass = "SkelotWorld", DisplayName = "Skelot世界类"))
	FSoftClassPath SkelotWorldClass;
	UPROPERTY(Config, EditAnywhere, Category = "设置", meta = (DisplayName = "每实例最大子网格数"))
	uint32 MaxSubmeshPerInstance = 15;
	UPROPERTY(config, EditAnywhere, Category = "设置", meta = (DisplayName = "集群模式"))
	ESkelotClusterMode ClusterMode;

	USkelotDeveloperSettings(const FObjectInitializer& Initializer);
};