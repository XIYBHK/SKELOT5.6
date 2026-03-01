// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SkelotWorldBase.h"
#include "SkelotPBDPlane.h"

/**
 * ORCA 半平面结构
 * 用于线性规划求解
 */
struct FORCAPlane
{
	/** 平面上的点（2D） */
	FVector2f Point;

	/** 平面方向（2D） */
	FVector2f Direction;

	FORCAPlane() : Point(FVector2f::ZeroVector), Direction(FVector2f::ZeroVector) {}
	FORCAPlane(const FVector2f& InPoint, const FVector2f& InDirection)
		: Point(InPoint), Direction(InDirection) {}
};

/**
 * RVO 代理数据
 * 存储单个实例的 RVO 计算数据
 */
struct FRVOAgentData
{
	/** 平滑后的速度 */
	FVector3f SmoothedVelocity = FVector3f::ZeroVector;

	/** 上一帧的方向（用于抖动检测） */
	FVector3f PreviousDirection = FVector3f::ZeroVector;

	/** 速度平滑内部状态 */
	FVector3f VelocitySmoothRef = FVector3f::ZeroVector;

	/** 抖动计数器 */
	int32 JitterCounter = 0;

	/** 当前邻居数量（用于密度自适应） */
	int32 CurrentNeighborCount = 0;
};

/**
 * RVO/ORCA 避障系统
 *
 * 基于 Optimal Reciprocal Collision Avoidance (ORCA) 算法
 * 用于实例间的智能避障，实现流畅的群体移动
 *
 * 核心算法：
 * 1. 速度障碍 (Velocity Obstacle) 计算
 * 2. 互惠速度障碍 (RVO) - 共享避障责任
 * 3. ORCA 线性规划 - 求解最优速度
 *
 * 参考：
 * - ORCA: Optimal Reciprocal Collision Avoidance (van den Berg et al.)
 * - RVO2 Library (https://github.com/snape/RVO2)
 */
class FSkelotRVOSystem
{
public:
	FSkelotRVOSystem();

	/** 设置 RVO 配置 */
	void SetConfig(const FSkelotRVOConfig& InConfig);

	/** 获取 RVO 配置 */
	const FSkelotRVOConfig& GetConfig() const;

	/** 设置抗抖动配置 */
	void SetAntiJitterConfig(const FSkelotAntiJitterConfig& InConfig);

	/** 获取抗抖动配置 */
	const FSkelotAntiJitterConfig& GetAntiJitterConfig() const;

	/**
	 * 执行 RVO 避障计算
	 *
	 * @param SOA 实例数据数组
	 * @param NumInstances 实例总数
	 * @param SpatialGrid 空间网格（用于邻居查询）
	 * @param DeltaTime 帧时间
	 * @param CollisionRadius 碰撞半径（用于计算）
	 */
	void ComputeAvoidance(FSkelotInstancesSOA& SOA, int32 NumInstances,
						  const class FSkelotSpatialGrid& SpatialGrid,
						  float DeltaTime,
						  float CollisionRadius);

	/**
	 * 获取实例的 RVO 代理数据
	 * @param InstanceIndex 实例索引
	 * @return 代理数据指针，不存在则返回 nullptr
	 */
	FRVOAgentData* GetAgentData(int32 InstanceIndex);
	const FRVOAgentData* GetAgentData(int32 InstanceIndex) const;

	/** 获取统计信息：处理的代理数量 */
	int32 GetProcessedAgents() const { return ProcessedAgents; }

	/** 获取统计信息：速度调整总次数 */
	int32 GetTotalVelocityAdjustments() const { return TotalVelocityAdjustments; }

	/** 重置统计信息 */
	void ResetStats();

	/** 重置所有代理数据 */
	void ResetAgentData();

private:
	/** RVO 配置 */
	FSkelotRVOConfig Config;

	/** 抗抖动配置 */
	FSkelotAntiJitterConfig AntiJitterConfig;

	/** 代理数据映射（InstanceIndex -> AgentData） */
	TMap<int32, FRVOAgentData> AgentDataMap;

	/** 临时数组：邻居索引 */
	TArray<int32> NeighborIndices;

	/** 临时数组：ORCA 平面 */
	TArray<FORCAPlane> ORCAPlanes;

	/** 统计：处理的代理数量 */
	int32 ProcessedAgents;

	/** 统计：速度调整总次数 */
	int32 TotalVelocityAdjustments;

	/** 当前碰撞半径（从外部传入） */
	float CurrentCollisionRadius;

	/** 当前帧计数器（用于分帧更新） */
	int32 FrameCounter;

	/**
	 * 计算单个实例的避障速度
	 *
	 * @param SOA 实例数据数组
	 * @param InstanceIndex 当前实例索引
	 * @param SpatialGrid 空间网格
	 * @param DeltaTime 帧时间
	 * @param OutNewVelocity 输出新速度
	 * @return 是否进行了速度调整
	 */
	bool ComputeAgentAvoidance(const FSkelotInstancesSOA& SOA, int32 InstanceIndex,
							   const FSkelotSpatialGrid& SpatialGrid,
							   float DeltaTime,
							   FVector3f& OutNewVelocity);

	/**
	 * 构建 ORCA 平面
	 *
	 * @param SOA 实例数据数组
	 * @param MyIndex 当前实例索引
	 * @param MyPosition 当前实例位置
	 * @param MyVelocity 当前实例速度
	 * @param NeighborIndices 邻居索引数组
	 * @param OutPlanes 输出 ORCA 平面数组
	 */
	void BuildORCAPlanes(const FSkelotInstancesSOA& SOA, int32 MyIndex,
						 const FVector3f& MyPosition, const FVector3f& MyVelocity,
						 const TArray<int32>& NeighborIndices,
						 TArray<FORCAPlane>& OutPlanes);

	/**
	 * 计算两个实例之间的 ORCA 平面
	 *
	 * @param PosA 实例A位置
	 * @param VelA 实例A速度
	 * @param PosB 实例B位置
	 * @param VelB 实例B速度
	 * @param CombinedRadius 组合半径
	 * @param OutPlane 输出 ORCA 平面
	 */
	void ComputeORCAPlane(const FVector3f& PosA, const FVector3f& VelA,
						  const FVector3f& PosB, const FVector3f& VelB,
						  float CombinedRadius,
						  FORCAPlane& OutPlane);

	/**
	 * 线性规划求解 - 找到满足所有约束的最优速度
	 *
	 * @param Planes ORCA 平面数组
	 * @param PreferredVelocity 期望速度
	 * @param MaxSpeed 最大速度
	 * @param OutResult 输出最优速度
	 * @return 是否找到有效解
	 */
	bool LinearProgram(const TArray<FORCAPlane>& Planes,
					   const FVector2f& PreferredVelocity,
					   float MaxSpeed,
					   FVector2f& OutResult);

	/**
	 * 线性规划 - 单个约束求解
	 */
	bool LinearProgram1(const TArray<FORCAPlane>& Planes, int32 PlaneNo,
						float Radius, const FVector2f& PreferredVelocity,
						FVector2f& OutResult);

	/**
	 * 线性规划 - 二维约束求解
	 */
	bool LinearProgram2(const TArray<FORCAPlane>& Planes, int32 PlaneNo,
						float Radius, const FVector2f& PreferredVelocity,
						FVector2f& OutResult);

	/**
	 * 线性规划 - 三维投影求解（当二维无解时）
	 */
	void LinearProgram3(const TArray<FORCAPlane>& Planes, int32 PlaneNo,
						float Radius, FVector2f& InOutResult);

	/**
	 * 应用抗抖动处理
	 *
	 * @param InstanceIndex 实例索引
	 * @param CurrentVelocity 当前速度
	 * @param InOutNewVelocity 输入/输出新速度
	 * @param DeltaTime 帧时间
	 * @param NeighborCount 邻居数量
	 */
	void ApplyAntiJitter(int32 InstanceIndex, const FVector3f& CurrentVelocity,
						 FVector3f& InOutNewVelocity, float DeltaTime, int32 NeighborCount);

	/**
	 * 应用速度平滑
	 *
	 * @param InstanceIndex 实例索引
	 * @param CurrentVelocity 当前速度
	 * @param TargetVelocity 目标速度
	 * @param DeltaTime 帧时间
	 * @return 平滑后的速度
	 */
	FVector3f ApplyVelocitySmoothing(int32 InstanceIndex, const FVector3f& CurrentVelocity,
									 const FVector3f& TargetVelocity, float DeltaTime);

	/**
	 * 检测抖动
	 *
	 * @param InstanceIndex 实例索引
	 * @param NewDirection 新方向
	 * @return 是否检测到抖动
	 */
	bool DetectJitter(int32 InstanceIndex, const FVector3f& NewDirection);

	/**
	 * 获取密度自适应系数
	 *
	 * @param NeighborCount 邻居数量
	 * @return 自适应系数 (0-1)
	 */
	float GetDensityAdaptationFactor(int32 NeighborCount) const;

	/**
	 * 检查两个实例是否应该进行避障
	 */
	bool ShouldAvoid(const FSkelotInstancesSOA& SOA, int32 IndexA, int32 IndexB) const;

	/**
	 * 获取或创建代理数据
	 */
	FRVOAgentData& GetOrCreateAgentData(int32 InstanceIndex);
};
