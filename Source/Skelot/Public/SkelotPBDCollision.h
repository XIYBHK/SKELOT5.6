// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SkelotWorldBase.h"

#include "SkelotPBDCollision.generated.h"

/**
 * PBD 碰撞系统配置参数
 *
 * 基于参考文档和预研文档的最佳实践配置
 * 用于控制实例间的碰撞避让行为
 */
USTRUCT(BlueprintType)
struct FSkelotPBDConfig
{
	GENERATED_BODY()

	/** 是否启用PBD碰撞 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD")
	bool bEnablePBD = true;

	/** PBD碰撞半径（厘米）- 根据角色体型调整，过小会重叠 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD", meta = (ClampMin = "10", ClampMax = "200"))
	float CollisionRadius = 60.0f;

	/** PBD迭代次数 - 平衡性能和稳定性，2-4都可以 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD", meta = (ClampMin = "1", ClampMax = "8"))
	int32 IterationCount = 3;

	/** 障碍物后额外迭代次数 - 有障碍物时增加稳定性 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD", meta = (ClampMin = "0", ClampMax = "4"))
	int32 PostObstacleIterations = 1;

	/** PBD松弛系数 - 越低越稳定但响应慢，推荐0.3 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float RelaxationFactor = 0.3f;

	/** PBD网格尺寸倍率 - 保持默认1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float GridCellSizeMultiplier = 1.0f;

	/** PBD最大邻居数 - 密集场景可增加到128 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD", meta = (ClampMin = "8", ClampMax = "128"))
	int32 MaxNeighbors = 64;

	/** PBD更新频率 - 每N帧更新一次，1表示每帧更新 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD", meta = (ClampMin = "1", ClampMax = "4"))
	int32 UpdateFrequency = 1;

	/** 启用速度投影 - 防止穿透后抖动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD|速度投影")
	bool bEnableVelocityProjection = true;

	/** 速度投影强度 - 越高越稳定 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD|速度投影", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityProjectionStrength = 0.8f;

	/** 速度恢复速率 - 碰撞后恢复原速度的速率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD|速度投影", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float VelocityRecoveryRate = 2.0f;

	/** 最大推力系数 - 深度重叠时的推力上限 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float MaxPushForceCoefficient = 1.5f;

	/** 默认构造 */
	FSkelotPBDConfig() = default;

	/** 获取推荐的默认配置 */
	static FSkelotPBDConfig GetRecommendedConfig()
	{
		FSkelotPBDConfig Config;
		Config.bEnablePBD = true;
		Config.CollisionRadius = 60.0f;
		Config.IterationCount = 3;
		Config.PostObstacleIterations = 1;
		Config.RelaxationFactor = 0.3f;
		Config.GridCellSizeMultiplier = 1.0f;
		Config.MaxNeighbors = 64;
		Config.UpdateFrequency = 1;
		Config.bEnableVelocityProjection = true;
		Config.VelocityProjectionStrength = 0.8f;
		Config.VelocityRecoveryRate = 2.0f;
		Config.MaxPushForceCoefficient = 1.5f;
		return Config;
	}
};

/**
 * PBD碰撞系统
 *
 * 基于位置的动态碰撞系统，用于实例间的碰撞避让
 * 核心算法：PBD (Position Based Dynamics)
 *
 * 工作流程：
 * 1. 使用空间网格快速获取潜在碰撞邻居
 * 2. 对每对邻居计算穿透深度
 * 3. 应用位置约束进行分离
 * 4. 多次迭代提高稳定性
 * 5. 速度投影防止穿透抖动
 */
class FSkelotPBDCollisionSystem
{
public:
	FSkelotPBDCollisionSystem();

	/** 设置PBD配置 */
	void SetConfig(const FSkelotPBDConfig& InConfig) { Config = InConfig; }

	/** 获取PBD配置 */
	const FSkelotPBDConfig& GetConfig() const { return Config; }

	/**
	 * 执行PBD碰撞求解
	 *
	 * @param SOA 实例数据数组
	 * @param NumInstances 实例总数
	 * @param SpatialGrid 空间网格（用于邻居查询）
	 * @param DeltaTime 帧时间
	 */
	void SolveCollisions(FSkelotInstancesSOA& SOA, int32 NumInstances,
						 const class FSkelotSpatialGrid& SpatialGrid, float DeltaTime);

	/**
	 * 执行单次碰撞迭代
	 * @param SOA 实例数据数组
	 * @param NumInstances 实例总数
	 * @param SpatialGrid 空间网格
	 */
	void SolveIteration(FSkelotInstancesSOA& SOA, int32 NumInstances,
						const FSkelotSpatialGrid& SpatialGrid);

	/** 获取统计信息：处理的碰撞对数量 */
	int32 GetProcessedCollisionPairs() const { return ProcessedCollisionPairs; }

	/** 获取统计信息：总位置校正量 */
	float GetTotalCorrection() const { return TotalCorrection; }

	/** 重置统计信息 */
	void ResetStats();

private:
	/** PBD配置 */
	FSkelotPBDConfig Config;

	/** 临时数组：邻居索引 */
	TArray<int32> NeighborIndices;

	/** 临时数组：位置校正量 */
	TArray<FVector3f> PositionCorrections;

	/** 统计：处理的碰撞对数量 */
	int32 ProcessedCollisionPairs;

	/** 统计：总位置校正量 */
	float TotalCorrection;

	/**
	 * 检查两个实例是否应该碰撞
	 * 基于碰撞通道和掩码判断
	 */
	bool ShouldCollide(const FSkelotInstancesSOA& SOA, int32 IndexA, int32 IndexB) const;

	/**
	 * 求解单对碰撞约束
	 * @param SOA 实例数据数组
	 * @param IndexA 实例A索引
	 * @param IndexB 实例B索引
	 * @param OutCorrectionA 输出：A的位置校正量
	 * @param OutCorrectionB 输出：B的位置校正量
	 * @return 是否发生碰撞
	 */
	bool SolveCollisionPair(const FSkelotInstancesSOA& SOA, int32 IndexA, int32 IndexB,
							FVector3f& OutCorrectionA, FVector3f& OutCorrectionB);

	/**
	 * 应用速度投影
	 * 防止穿透后的速度抖动
	 */
	void ApplyVelocityProjection(FSkelotInstancesSOA& SOA, int32 Index,
								 const FVector3f& Correction, float DeltaTime);
};
