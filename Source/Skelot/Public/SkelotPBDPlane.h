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

	/** RVO邻居半径（厘米）- 约为碰撞半径的4-6倍 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "100", ClampMax = "800"))
	float NeighborRadius = 300.0f;

	/** RVO时间窗（秒）- 越大越提前避障，但可能过度绕行 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float TimeHorizon = 1.0f;

	/** RVO最大速度（厘米/秒）- 0表示使用当前速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "0", ClampMax = "1000"))
	float MaxSpeed = 0.0f;

	/** RVO最小速度（厘米/秒）- 防止完全停止 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "0", ClampMax = "200"))
	float MinSpeed = 50.0f;

	/** 到达半径（厘米）- 接近目标时减速范围 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "50", ClampMax = "500"))
	float ArrivalRadius = 200.0f;

	/** 到达密度阈值 - 防止目标点过度拥挤 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "1", ClampMax = "20"))
	int32 ArrivalDensityThreshold = 6;

	/** RVO最大邻居数 - 平衡精度和性能 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "4", ClampMax = "32"))
	int32 MaxNeighbors = 16;

	/** RVO分帧步长 - 2可降低50%计算量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO", meta = (ClampMin = "1", ClampMax = "4"))
	int32 FrameStride = 1;

	/** 默认构造 */
	FSkelotRVOConfig() = default;

	/** 获取推荐的默认配置 */
	static FSkelotRVOConfig GetRecommendedConfig()
	{
		FSkelotRVOConfig Config;
		Config.bEnableRVO = true;
		Config.NeighborRadius = 300.0f;
		Config.TimeHorizon = 1.0f;
		Config.MaxSpeed = 0.0f;
		Config.MinSpeed = 50.0f;
		Config.ArrivalRadius = 200.0f;
		Config.ArrivalDensityThreshold = 6;
		Config.MaxNeighbors = 16;
		Config.FrameStride = 1;
		return Config;
	}
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

	/** 密度阈值 - 邻居数超过此值触发自适应 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "1", ClampMax = "20"))
	int32 DensityThreshold = 8;

	/** 高密度松弛系数 - 高密度时的松弛系数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float HighDensityRelaxation = 0.3f;

	/** 启用速度平滑 - 平滑速度变化防止抖动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动")
	bool bEnableVelocitySmoothing = true;

	/** 速度平滑系数 - 越大越平滑，但响应变慢 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float VelocitySmoothFactor = 0.15f;

	/** 启用抖动检测 - 自动检测并抑制抖动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动")
	bool bEnableJitterDetection = true;

	/** 抖动检测阈值 - 方向变化超过此值视为抖动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JitterThreshold = 0.7f;

	/** 抖动抑制强度 - 检测到抖动时的抑制力度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "抗抖动", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JitterSuppression = 0.5f;

	/** 默认构造 */
	FSkelotAntiJitterConfig() = default;

	/** 获取推荐的默认配置 */
	static FSkelotAntiJitterConfig GetRecommendedConfig()
	{
		FSkelotAntiJitterConfig Config;
		Config.bEnableDensityAdaptation = true;
		Config.DensityThreshold = 8;
		Config.HighDensityRelaxation = 0.3f;
		Config.bEnableVelocitySmoothing = true;
		Config.VelocitySmoothFactor = 0.15f;
		Config.bEnableJitterDetection = true;
		Config.JitterThreshold = 0.7f;
		Config.JitterSuppression = 0.5f;
		return Config;
	}
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
