// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotPBDCollision.h"
#include "SkelotSpatialGrid.h"
#include "SkelotObstacle.h"
#include "SkelotWorld.h"
#include "Async/ParallelFor.h"

namespace
{
	bool ComputeObstacleCollisionResponse(const FObstacleCollisionData& ObstacleData, const FVector& InstanceLocation,
		float InstanceRadius, FVector& OutPushDirection, float& OutPushMagnitude)
	{
		OutPushDirection = FVector::ZeroVector;
		OutPushMagnitude = 0.0f;

		if (ObstacleData.Type == ESkelotObstacleType::Sphere)
		{
			const float EffectiveRadius = ObstacleData.SphereRadius + ObstacleData.RadiusOffset;
			const float MinDist = EffectiveRadius + InstanceRadius;

			const FVector Delta = InstanceLocation - ObstacleData.Location;
			const float DistSq = Delta.SquaredLength();
			if (DistSq >= MinDist * MinDist)
			{
				return false;
			}

			const float Dist = FMath::Sqrt(DistSq);
			if (Dist < KINDA_SMALL_NUMBER)
			{
				OutPushDirection = FVector(1.0f, 0.0f, 0.0f);
				OutPushMagnitude = MinDist;
				return true;
			}

			OutPushDirection = Delta / Dist;
			OutPushMagnitude = MinDist - Dist;
			return true;
		}

		const FVector LocalPoint = ObstacleData.Transform.InverseTransformPosition(InstanceLocation);
		const FVector EffectiveExtent = ObstacleData.BoxExtent + FVector(ObstacleData.RadiusOffset);
		const FVector ExpandedExtent = EffectiveExtent + FVector(InstanceRadius);

		if (FMath::Abs(LocalPoint.X) >= ExpandedExtent.X ||
			FMath::Abs(LocalPoint.Y) >= ExpandedExtent.Y ||
			FMath::Abs(LocalPoint.Z) >= ExpandedExtent.Z)
		{
			return false;
		}

		const float DX = ExpandedExtent.X - FMath::Abs(LocalPoint.X);
		const float DY = ExpandedExtent.Y - FMath::Abs(LocalPoint.Y);
		const float DZ = ExpandedExtent.Z - FMath::Abs(LocalPoint.Z);

		FVector LocalPushDir = FVector::ZeroVector;
		float Penetration = 0.0f;

		if (DX <= DY && DX <= DZ)
		{
			Penetration = DX;
			LocalPushDir = FVector(LocalPoint.X >= 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
		}
		else if (DY <= DX && DY <= DZ)
		{
			Penetration = DY;
			LocalPushDir = FVector(0.0f, LocalPoint.Y >= 0 ? 1.0f : -1.0f, 0.0f);
		}
		else
		{
			Penetration = DZ;
			LocalPushDir = FVector(0.0f, 0.0f, LocalPoint.Z >= 0 ? 1.0f : -1.0f);
		}

		OutPushDirection = ObstacleData.Rotation.RotateVector(LocalPushDir);
		OutPushMagnitude = Penetration;
		return true;
	}
}

FSkelotPBDCollisionSystem::FSkelotPBDCollisionSystem()
	: Config(FSkelotPBDConfig::GetRecommendedConfig())
	, ProcessedCollisionPairs(0)
	, TotalCorrection(0.0f)
{
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
		// 距离为零，使用固定方向分离；Dist 必须与 Delta 长度匹配，否则 Direction = Delta/Dist 不是单位向量
		Delta = FVector3f(1.0f, 0.0f, 0.0f);
		Dist = 1.0f;
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
		const float ProjectionAlpha = FMath::Clamp(
			Config.VelocityProjectionStrength * (1.0f - FMath::Exp(-Config.VelocityRecoveryRate * DeltaTime)),
			0.0f,
			1.0f);

		// 按帧时间平滑移除速度在碰撞方向上的分量，避免重新穿透
		FVector3f ProjectedVelocity = Velocity - CorrectionDir * VelocityDotCorrection * ProjectionAlpha;
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

	// 预分配并清零位置校正数组（SetNumZeroed 会自动扩展或缩减到目标大小）
	PositionCorrections.SetNumUninitialized(NumInstances);
	FMemory::Memzero(PositionCorrections.GetData(), NumInstances * sizeof(FVector3f));

	// 这里只处理实例间碰撞；障碍物后额外迭代由调用方在需要时单独执行
	for (int32 Iter = 0; Iter < Config.IterationCount; Iter++)
	{
		SolveIteration(SOA, NumInstances, SpatialGrid, DeltaTime);
	}
}

void FSkelotPBDCollisionSystem::SolveIteration(FSkelotInstancesSOA& SOA, int32 NumInstances,
											   const FSkelotSpatialGrid& SpatialGrid, float DeltaTime)
{
	TArray<int32> PerInstancePairCounts;
	TArray<float> PerInstanceCorrectionSums;
	PerInstancePairCounts.SetNumZeroed(NumInstances);
	PerInstanceCorrectionSums.SetNumZeroed(NumInstances);

	ParallelFor(NumInstances, [&](int32 InstanceIndex)
	{
		// 跳过已销毁的实例
		if (SOA.Slots[InstanceIndex].bDestroyed)
		{
			return;
		}

		// 获取当前位置
		const FVector3d& MyPos = SOA.Locations[InstanceIndex];
		FVector3f AccumulatedCorrection = FVector3f::ZeroVector;
		int32 LocalPairCount = 0;
		float LocalCorrectionSum = 0.0f;

		// 使用空间网格查询邻居（线程局部缓存，预分配减少堆扩容）
		TArray<int32> LocalNeighborIndices;
		LocalNeighborIndices.Reserve(Config.MaxNeighbors);
		SpatialGrid.QuerySphere(FVector(MyPos), Config.CollisionRadius * 2.0f, LocalNeighborIndices, 0xFF, &SOA);

		// 限制邻居数量
		int32 NumNeighbors = FMath::Min(LocalNeighborIndices.Num(), Config.MaxNeighbors);

		// 处理每个邻居，仅累计“自身”校正，避免跨线程写冲突
		for (int32 i = 0; i < NumNeighbors; i++)
		{
			int32 NeighborIndex = LocalNeighborIndices[i];

			if (NeighborIndex == InstanceIndex || SOA.Slots[NeighborIndex].bDestroyed)
			{
				continue;
			}

			if (!ShouldCollide(SOA, InstanceIndex, NeighborIndex))
			{
				continue;
			}

			FVector3f CorrectionSelf, CorrectionNeighbor;
			if (SolveCollisionPair(SOA, InstanceIndex, NeighborIndex, CorrectionSelf, CorrectionNeighbor))
			{
				AccumulatedCorrection += CorrectionSelf;
				LocalPairCount++;
				LocalCorrectionSum += CorrectionSelf.Length();
			}
		}

		PositionCorrections[InstanceIndex] = AccumulatedCorrection;
		PerInstancePairCounts[InstanceIndex] = LocalPairCount;
		PerInstanceCorrectionSums[InstanceIndex] = LocalCorrectionSum;
	});

	// 汇总统计
	int32 TotalPairs = 0;
	float TotalCorr = 0.0f;
	for (int32 i = 0; i < NumInstances; ++i)
	{
		TotalPairs += PerInstancePairCounts[i];
		TotalCorr += PerInstanceCorrectionSums[i];
	}
	// 每个碰撞对会被A和B各计算一次，统计时折半近似为唯一碰撞对数量
	ProcessedCollisionPairs += TotalPairs / 2;
	TotalCorrection += TotalCorr;

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

			ApplyVelocityProjection(SOA, i, Correction, DeltaTime);
		}
	}
}

void FSkelotPBDCollisionSystem::RebuildObstacleDataCache(const TArray<TObjectPtr<ASkelotObstacle>>& Obstacles)
{
	CachedObstacleData.Reset();
	CachedObstacleData.Reserve(Obstacles.Num());
	for (const TObjectPtr<ASkelotObstacle>& ObstaclePtr : Obstacles)
	{
		const ASkelotObstacle* Obstacle = ObstaclePtr.Get();
		if (!Obstacle || !Obstacle->bEnabled)
		{
			continue;
		}

		FObstacleCollisionData ObstacleData;
		ObstacleData.Type = Obstacle->ObstacleType;
		ObstacleData.CollisionMask = Obstacle->CollisionMask;
		ObstacleData.RadiusOffset = Obstacle->RadiusOffset;
		ObstacleData.Transform = Obstacle->GetActorTransform();
		ObstacleData.Rotation = Obstacle->GetActorQuat();
		ObstacleData.Location = Obstacle->GetActorLocation();

		if (const ASkelotSphereObstacle* SphereObstacle = Cast<ASkelotSphereObstacle>(Obstacle))
		{
			ObstacleData.SphereRadius = SphereObstacle->Radius;
		}
		else if (const ASkelotBoxObstacle* BoxObstacle = Cast<ASkelotBoxObstacle>(Obstacle))
		{
			ObstacleData.BoxExtent = BoxObstacle->BoxExtent;
		}

		CachedObstacleData.Add(ObstacleData);
	}
}

void FSkelotPBDCollisionSystem::SolveObstacleCollisions(FSkelotInstancesSOA& SOA, int32 NumInstances, float DeltaTime)
{
	if (CachedObstacleData.Num() == 0)
	{
		return;
	}

	TArray<float> PerInstanceCorrectionSums;
	PerInstanceCorrectionSums.SetNumZeroed(NumInstances);

	ParallelFor(NumInstances, [&](int32 InstanceIndex)
	{
		// 跳过已销毁的实例
		if (SOA.Slots[InstanceIndex].bDestroyed)
		{
			return;
		}

		FVector InstanceLocation(SOA.Locations[InstanceIndex]);
		float InstanceRadius = Config.CollisionRadius;
		uint8 InstanceCollisionMask = SOA.CollisionMasks[InstanceIndex];
		float LocalCorrectionSum = 0.0f;

		// 遍历所有障碍物
		for (const FObstacleCollisionData& ObstacleData : CachedObstacleData)
		{
			// 检查碰撞掩码
			if ((InstanceCollisionMask & ObstacleData.CollisionMask) == 0)
			{
				continue;
			}

			FVector PushDirection;
			float PushMagnitude;

			if (ComputeObstacleCollisionResponse(ObstacleData, InstanceLocation, InstanceRadius, PushDirection, PushMagnitude))
			{
				// 应用位置校正（使用松弛系数）
				FVector3f Correction = FVector3f(PushDirection * PushMagnitude * Config.RelaxationFactor);
				SOA.Locations[InstanceIndex] += FVector3d(Correction);
				InstanceLocation += FVector(Correction);
				ApplyVelocityProjection(SOA, InstanceIndex, Correction, DeltaTime);

				LocalCorrectionSum += Correction.Size();
			}
		}

		PerInstanceCorrectionSums[InstanceIndex] = LocalCorrectionSum;
	});

	for (float CorrectionSum : PerInstanceCorrectionSums)
	{
		TotalCorrection += CorrectionSum;
	}
}
