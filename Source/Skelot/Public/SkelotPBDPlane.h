// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkelotPBDCollision.h"

#include "SkelotPBDPlane.generated.h"

/**
 * RVO/ORCA 避障系统配置参数
 *
 * 基于参考文档和预研文档的最佳实践配置
 * 用于控制实例间的智能避障行为
 */
USTRUCT(BlueprintType)
struct FSkelotRVOConfig
{
	GENERATED_BODY()

	/** 是否启用RVO/ORCA避障 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO")
	bool bEnableRVO = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "100", ClampMax = "800", UIMin = "100", UIMax = "800", ForceUnits = "cm", EditCondition = "bEnableRVO"))
	float NeighborRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "0.1", ClampMax = "2.0", UIMin = "0.1", UIMax = "2.0", ForceUnits = "s", EditCondition = "bEnableRVO"))
	float TimeHorizon = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "0", ClampMax = "1000", UIMin = "0", UIMax = "1000", ForceUnits = "cm/s", EditCondition = "bEnableRVO"))
	float MaxSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "0", ClampMax = "200", UIMin = "0", UIMax = "200", ForceUnits = "cm/s", EditCondition = "bEnableRVO"))
	float MinSpeed = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "50", ClampMax = "500", UIMin = "50", UIMax = "500", ForceUnits = "cm", EditCondition = "bEnableRVO"))
	float ArrivalRadius = 200.0f;

	/** 到达密度阈值 - 防止目标点过度拥挤 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ArrivalDensityThreshold = 6;

	/** RVO最大邻居数 - 平衡精度和性能 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "4", ClampMax = "32"))
	int32 MaxNeighbors = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", AdvancedDisplay, meta = (ClampMin = "1", ClampMax = "4", UIMin = "1", UIMax = "4", EditCondition = "bEnableRVO"))
	int32 FrameStride = 1;

	/** 是否启用HRVO混合模式 - 结合RVO和VO的优点，解决迎面相遇时的振荡问题 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO")
	bool bEnableHRVO = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", AdvancedDisplay, meta = (ClampMin = "-1.0", ClampMax = "0.0", UIMin = "-1.0", UIMax = "0.0", EditCondition = "bEnableHRVO"))
	float HRVOHeadOnThreshold = -0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", AdvancedDisplay, meta = (ClampMin = "0", ClampMax = "1000", UIMin = "0", UIMax = "1000", ForceUnits = "cm", EditCondition = "bEnableRVO"))
	float HeightDifferenceThreshold = 200.0f;

	/** 默认构造 */
	FSkelotRVOConfig() = default;

	static FSkelotRVOConfig GetRecommendedConfig() { return FSkelotRVOConfig{}; }
};

/**
 * 抗抖动系统配置参数
 *
 * 用于防止高密度场景下的抖动和不稳定行为
 */
USTRUCT(BlueprintType)
struct FSkelotAntiJitterConfig
{
	GENERATED_BODY()

	/** 启用密度自适应 - 高密度时自动降低响应 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动")
	bool bEnableDensityAdaptation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "1", ClampMax = "20", UIMin = "1", UIMax = "20", EditCondition = "bEnableDensityAdaptation"))
	int32 DensityThreshold = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0", EditCondition = "bEnableDensityAdaptation"))
	float HighDensityRelaxation = 0.3f;

	/** 启用速度平滑 - 平滑速度变化防止抖动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动")
	bool bEnableVelocitySmoothing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "0.05", ClampMax = "0.5", UIMin = "0.05", UIMax = "0.5", EditCondition = "bEnableVelocitySmoothing"))
	float VelocitySmoothFactor = 0.15f;

	/** 启用抖动检测 - 自动检测并抑制抖动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动")
	bool bEnableJitterDetection = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnableJitterDetection"))
	float JitterThreshold = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnableJitterDetection"))
	float JitterSuppression = 0.5f;

	/** 默认构造 */
	FSkelotAntiJitterConfig() = default;

	static FSkelotAntiJitterConfig GetRecommendedConfig() { return FSkelotAntiJitterConfig{}; }
};

/**
 * LOD 更新频率系统配置参数
 *
 * 基于实例到相机的距离，动态调整更新频率
 * 远距离实例使用更低的更新频率，提升性能
 */
USTRUCT(BlueprintType)
struct FSkelotLODConfig
{
	GENERATED_BODY()

	/** 是否启用 LOD 更新频率优化 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
	bool bEnableLODUpdateFrequency = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "100", ClampMax = "10000", UIMin = "100", UIMax = "10000", ForceUnits = "cm", EditCondition = "bEnableLODUpdateFrequency"))
	float MediumDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "100", ClampMax = "20000", UIMin = "100", UIMax = "20000", ForceUnits = "cm", EditCondition = "bEnableLODUpdateFrequency"))
	float FarDistance = 5000.0f;

	/** 默认构造 */
	FSkelotLODConfig() = default;

	static FSkelotLODConfig GetRecommendedConfig() { return FSkelotLODConfig{}; }
};

/**
 * Skelot PBD Plane Actor
 *
 * 场景配置 Actor，用于配置 PBD 碰撞和 RVO 避障参数。
 * 放置到场景中后，参数会自动应用到 SkelotWorld。
 *
 * 使用方法：
 * 1. 将此 Actor 放置到场景中
 * 2. 在 Details 面板中配置 PBD 和 RVO 参数
 * 3. 参数会在运行时自动应用到 ASkelotWorld
 *
 * 推荐配置（高性能 + 不抖动 + 不重叠）：
 * - PBD碰撞半径: 50-80
 * - PBD迭代次数: 3
 * - PBD松弛系数: 0.3
 * - RVO邻居半径: 200-400 (碰撞半径的4-6倍)
 * - RVO时间窗: 0.5-1.0s
 */
UCLASS(meta = (DisplayName = "Skelot PBD Plane"))
class SKELOT_API ASkelotPBDPlane : public AActor
{
	GENERATED_BODY()

public:
	ASkelotPBDPlane();

	/** PBD碰撞配置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD碰撞", meta = (DisplayName = "PBD配置"))
	FSkelotPBDConfig PBDConfig;

	/** RVO/ORCA避障配置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO避障", meta = (DisplayName = "RVO配置"))
	FSkelotRVOConfig RVOConfig;

	/** 抗抖动配置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (DisplayName = "抗抖动配置"))
	FSkelotAntiJitterConfig AntiJitterConfig;

	/** LOD更新频率配置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD更新", meta = (DisplayName = "LOD配置"))
	FSkelotLODConfig LODConfig;

	/** 是否在运行时自动应用配置到 SkelotWorld */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "自动应用配置"))
	bool bAutoApplyConfig = true;

	/** 是否显示调试可视化 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试", meta = (DisplayName = "显示调试可视化"))
	bool bShowDebugVisualization = false;

	/**
	 * 手动应用当前配置到 SkelotWorld
	 * 可在蓝图中调用以在运行时更新配置
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot", meta = (DisplayName = "应用配置到SkelotWorld"))
	void ApplyConfigToWorld();

	/**
	 * 加载推荐的默认配置
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot", meta = (DisplayName = "加载推荐配置"))
	void LoadRecommendedConfig();

	/**
	 * 获取关联的 SkelotWorld
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot", meta = (DisplayName = "获取SkelotWorld"))
	class ASkelotWorld* GetSkelotWorld() const;

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	virtual void Tick(float DeltaTime) override;
};
