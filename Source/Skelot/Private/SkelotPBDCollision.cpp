// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotPBDCollision.h"
#include "SkelotSpatialGrid.h"

FSkelotPBDCollisionSystem::FSkelotPBDCollisionSystem()
	: Config(FSkelotPBDConfig::GetRecommendedConfig())
	, ProcessedCollisionPairs(0)
	, TotalCorrection(0.0f)
{
	NeighborIndices.Reserve(128);
	PositionCorrections.Reserve(128);
}

void FSkelotPBDCollisionSystem::ResetStats()
{
	ProcessedCollisionPairs = 0;
	TotalCorrection = 0.0f;
}

bool FSkelotPBDCollisionSystem::ShouldCollide(const FSkelotInstancesSOA& SOA, int32 IndexA, int32 IndexB) const
{
	// 检查两个实例是否都存活
	if (SOA.Slots[IndexA].bDestroyed || SOA.Slots[IndexB].bDestroyed)
	{
		return false;
	}

	// 获取碰撞通道和掩码
	uint8 ChannelA = SOA.CollisionChannels[IndexA];
	uint8 MaskA = SOA.CollisionMasks[IndexA];
	uint8 ChannelB = SOA.CollisionChannels[IndexB];
	uint8 MaskB = SOA.CollisionMasks[IndexB];

	// 碰撞判定：(MaskA & (1 << ChannelB)) && (MaskB & (1 << ChannelA))
	bool bCanACollideWithB = (MaskA & (1 << ChannelB)) != 0;
	bool bCanBCollideWithA = (MaskB & (1 << ChannelA)) != 0;

	return bCanACollideWithB && bCanBCollideWithA;
}

bool FSkelotPBDCollisionSystem::SolveCollisionPair(const FSkelotInstancesSOA& SOA, int32 IndexA, int32 IndexB,
												   FVector3f& OutCorrectionA, FVector3f& OutCorrectionB)
{
	OutCorrectionA = FVector3f::ZeroVector;
	OutCorrectionB = FVector3f::ZeroVector;

	// 获取位置 (FVector3d -> FVector3f 转换)
	const FVector3f PosA(SOA.Locations[IndexA]);
	const FVector3f PosB(SOA.Locations[IndexB]);

	// 计算距离向量
	FVector3f Delta = PosB - PosA;
	float DistSq = Delta.SquaredLength();

	// 计算最小距离（两个碰撞半径之和）
	float MinDist = Config.CollisionRadius * 2.0f;
	float MinDistSq = MinDist * MinDist;

	// 如果距离大于最小距离，不碰撞
	if (DistSq >= MinDistSq)
	{
		return false;
	}

	// 避免距离为零的情况
	float Dist = FMath::Sqrt(DistSq);
	if (Dist < KINDA_SMALL_NUMBER)
	{
		// 距离为零，使用随机方向分离
		Delta = FVector3f(1.0f, 0.0f, 0.0f);
		Dist = KINDA_SMALL_NUMBER;
	}

	// 计算穿透深度
	float Penetration = MinDist - Dist;

	// 计算方向（从A指向B）
	FVector3f Direction = Delta / Dist;

	// 计算位置校正量（各分担一半）
	float Correction = Penetration * Config.RelaxationFactor * 0.5f;

	// 限制最大推力
	float MaxCorrection = Penetration * Config.MaxPushForceCoefficient * 0.5f;
	Correction = FMath::Min(Correction, MaxCorrection);

	OutCorrectionA = -Direction * Correction;
	OutCorrectionB = Direction * Correction;

	return true;
}

void FSkelotPBDCollisionSystem::ApplyVelocityProjection(FSkelotInstancesSOA& SOA, int32 Index,
														const FVector3f& Correction, float DeltaTime)
{
	if (!Config.bEnableVelocityProjection || DeltaTime < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 获取当前速度
	FVector3f& Velocity = SOA.Velocities[Index];

	// 计算速度投影：如果速度方向与校正方向相反，需要调整速度
	float CorrectionMagSq = Correction.SquaredLength();
	if (CorrectionMagSq < KINDA_SMALL_NUMBER * KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector3f CorrectionDir = Correction / FMath::Sqrt(CorrectionMagSq);
	float VelocityDotCorrection = FVector3f::DotProduct(Velocity, CorrectionDir);

	// 如果速度方向与校正方向相反（即速度指向另一个实例），需要投影
	if (VelocityDotCorrection < 0.0f)
	{
		// 移除速度在碰撞方向上的分量
		FVector3f ProjectedVelocity = Velocity - CorrectionDir * VelocityDotCorrection * Config.VelocityProjectionStrength;
		Velocity = ProjectedVelocity;
	}
}

void FSkelotPBDCollisionSystem::SolveCollisions(FSkelotInstancesSOA& SOA, int32 NumInstances,
												const FSkelotSpatialGrid& SpatialGrid, float DeltaTime)
{
	if (!Config.bEnablePBD || NumInstances == 0)
	{
		return;
	}

	// 重置统计
	ResetStats();

	// 预分配位置校正数组
	if (PositionCorrections.Num() < NumInstances)
	{
		PositionCorrections.SetNumZeroed(NumInstances);
	}
	else
	{
		// 清零已有的校正量
		for (int32 i = 0; i < NumInstances; i++)
		{
			PositionCorrections[i] = FVector3f::ZeroVector;
		}
	}

	// 执行多次迭代
	int32 TotalIterations = Config.IterationCount + Config.PostObstacleIterations;
	for (int32 Iter = 0; Iter < TotalIterations; Iter++)
	{
		SolveIteration(SOA, NumInstances, SpatialGrid);
	}
}

void FSkelotPBDCollisionSystem::SolveIteration(FSkelotInstancesSOA& SOA, int32 NumInstances,
											   const FSkelotSpatialGrid& SpatialGrid)
{
	// 遍历所有有效实例
	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
	{
		// 跳过已销毁的实例
		if (SOA.Slots[InstanceIndex].bDestroyed)
		{
			continue;
		}

		// 获取当前位置
		const FVector3d& MyPos = SOA.Locations[InstanceIndex];

		// 使用空间网格查询邻居
		NeighborIndices.Reset();
		SpatialGrid.QuerySphere(FVector(MyPos), Config.CollisionRadius * 2.0f, NeighborIndices);

		// 限制邻居数量
		int32 NumNeighbors = FMath::Min(NeighborIndices.Num(), Config.MaxNeighbors);

		// 处理每个邻居
		for (int32 i = 0; i < NumNeighbors; i++)
		{
			int32 NeighborIndex = NeighborIndices[i];

			// 跳过自己
			if (NeighborIndex == InstanceIndex)
			{
				continue;
			}

			// 跳过已销毁的实例
			if (SOA.Slots[NeighborIndex].bDestroyed)
			{
				continue;
			}

			// 检查碰撞过滤
			if (!ShouldCollide(SOA, InstanceIndex, NeighborIndex))
			{
				continue;
			}

			// 避免重复处理（只处理索引小的）
			if (NeighborIndex < InstanceIndex)
			{
				continue;
			}

			// 求解碰撞对
			FVector3f CorrectionA, CorrectionB;
			if (SolveCollisionPair(SOA, InstanceIndex, NeighborIndex, CorrectionA, CorrectionB))
			{
				// 累加位置校正量
				PositionCorrections[InstanceIndex] += CorrectionA;
				PositionCorrections[NeighborIndex] += CorrectionB;

				// 更新统计
				ProcessedCollisionPairs++;
				TotalCorrection += (CorrectionA.Length() + CorrectionB.Length());
			}
		}
	}

	// 应用位置校正
	for (int32 i = 0; i < NumInstances; i++)
	{
		if (SOA.Slots[i].bDestroyed)
		{
			continue;
		}

		const FVector3f& Correction = PositionCorrections[i];
		if (Correction.SquaredLength() > KINDA_SMALL_NUMBER * KINDA_SMALL_NUMBER)
		{
			// 将 FVector3f 校正量添加到 FVector3d 位置
			SOA.Locations[i] += FVector3d(Correction);

			// TODO: 如果需要，可以在这里调用速度投影
			// ApplyVelocityProjection(SOA, i, Correction, DeltaTime);
		}
	}
}
