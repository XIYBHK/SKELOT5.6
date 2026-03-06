// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotSpatialGrid.h"
#include "DrawDebugHelpers.h"

FSkelotSpatialGrid::FSkelotSpatialGrid()
	: CellSize(200.0f)  // 默认200厘米（2米）
	, InvCellSize(1.0f / 200.0f)
	, FrameStride(1)    // 默认每帧完整更新
	, CurrentFrameIndex(0)
	, TotalInstances(0)
{
}

void FSkelotSpatialGrid::SetCellSize(float InCellSize)
{
	if (InCellSize > 0.0f && InCellSize != CellSize)
	{
		CellSize = InCellSize;
		InvCellSize = 1.0f / CellSize;
		Clear();
	}
}

void FSkelotSpatialGrid::SetFrameStride(int32 Stride)
{
	FrameStride = FMath::Max(1, FMath::Min(Stride, 8));  // 限制 1-8
	CurrentFrameIndex = 0;
}

void FSkelotSpatialGrid::Clear()
{
	GridCells.Empty();
	TotalInstances = 0;
}

FIntVector FSkelotSpatialGrid::GetCellKey(const FVector& Location) const
{
	// 使用向下取整确保负坐标正确处理
	return FIntVector(
		FMath::FloorToInt(Location.X * InvCellSize),
		FMath::FloorToInt(Location.Y * InvCellSize),
		FMath::FloorToInt(Location.Z * InvCellSize)
	);
}

void FSkelotSpatialGrid::AddInstance(int32 InstanceIndex, const FVector& Location)
{
	FIntVector CellKey = GetCellKey(Location);
	TArray<int32>& CellInstances = GridCells.FindOrAdd(CellKey);
	CellInstances.Add(InstanceIndex);
	TotalInstances++;
}

void FSkelotSpatialGrid::Rebuild(const FSkelotInstancesSOA& SOA, int32 NumInstances)
{
	Clear();

	// 预估网格大小，减少重新分配
	int32 EstimatedCells = FMath::Max(1, NumInstances / 10);
	GridCells.Reserve(EstimatedCells);

	// 遍历所有实例并添加到网格
	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
	{
		// 只添加存活的实例
		if (!SOA.Slots[InstanceIndex].bDestroyed)
		{
			AddInstance(InstanceIndex, SOA.Locations[InstanceIndex]);
		}
	}
}

void FSkelotSpatialGrid::RebuildIncremental(const FSkelotInstancesSOA& SOA, int32 NumInstances)
{
	// 基于 DetourCrowd 调研的简化方案：
	// 不是分批更新（会导致数据不一致），而是每 N 帧重建一次
	// FrameStride=1: 每帧重建
	// FrameStride=2: 每 2 帧重建一次（降低 50% 计算量）
	// FrameStride=4: 每 4 帧重建一次（降低 75% 计算量）
	//
	// 优点：
	// 1. 简单实现，不会出错
	// 2. 网格数据始终一致
	// 3. 对大多数场景足够（群集 AI 不需要每帧精确位置）

	if (FrameStride <= 1)
	{
		// 每帧更新
		Rebuild(SOA, NumInstances);
		return;
	}

	// 只有当 CurrentFrameIndex == 0 时才重建（确保首帧不跳过）
	if (CurrentFrameIndex == 0)
	{
		Rebuild(SOA, NumInstances);
	}
	// 递增放在检查之后，保证首次调用时 CurrentFrameIndex==0 不会被跳过
	CurrentFrameIndex = (CurrentFrameIndex + 1) % FrameStride;
}

void FSkelotSpatialGrid::ForceFullRebuild(const FSkelotInstancesSOA& SOA, int32 NumInstances)
{
	CurrentFrameIndex = 0;
	Rebuild(SOA, NumInstances);
}

const TArray<int32>* FSkelotSpatialGrid::GetCellInstances(const FIntVector& CellKey) const
{
	return GridCells.Find(CellKey);
}

void FSkelotSpatialGrid::QuerySphere(const FVector& Center, float Radius, TArray<int32>& OutIndices,
									  uint8 CollisionMask, const FSkelotInstancesSOA* SOA) const
{
	OutIndices.Reset();

	if (Radius <= 0.0f)
	{
		return;
	}

	// 计算需要检查的网格范围
	float RadiusInCells = Radius * InvCellSize;
	int32 CellRadius = FMath::CeilToInt(RadiusInCells);

	FIntVector CenterCell = GetCellKey(Center);

	// 计算半径平方用于距离检测（避免 sqrt）
	float RadiusSquared = Radius * Radius;
	constexpr int32 FullScanCellRadiusThreshold = 32;

	auto TryAddInstance = [&](int32 InstanceIndex)
	{
		// 掩码过滤
		if (CollisionMask != 0xFF && SOA)
		{
			uint8 InstanceMask = SOA->CollisionMasks[InstanceIndex];
			if ((InstanceMask & CollisionMask) == 0)
			{
				return;
			}
		}

		if (SOA)
		{
			const FVector InstanceLocation(SOA->Locations[InstanceIndex]);
			const float DistSquared = (InstanceLocation - Center).SizeSquared();
			if (DistSquared > RadiusSquared)
			{
				return;
			}
		}

		OutIndices.Add(InstanceIndex);
	};

	if (CellRadius > FullScanCellRadiusThreshold)
	{
		for (const TPair<FIntVector, TArray<int32>>& CellPair : GridCells)
		{
			for (int32 InstanceIndex : CellPair.Value)
			{
				TryAddInstance(InstanceIndex);
			}
		}
		return;
	}

	// 遍历可能重叠的所有网格单元
	for (int32 dz = -CellRadius; dz <= CellRadius; dz++)
	{
		for (int32 dy = -CellRadius; dy <= CellRadius; dy++)
		{
			for (int32 dx = -CellRadius; dx <= CellRadius; dx++)
			{
				FIntVector CellKey = CenterCell + FIntVector(dx, dy, dz);
				const TArray<int32>* CellInstances = GridCells.Find(CellKey);

				if (CellInstances && CellInstances->Num() > 0)
				{
					for (int32 InstanceIndex : *CellInstances)
					{
						TryAddInstance(InstanceIndex);
					}
				}
			}
		}
	}
}

void FSkelotSpatialGrid::QuerySphereWithExclusion(const FVector& Center, float Radius, TArray<int32>& OutIndices,
												   const TSet<int32>& ExcludeIndices,
												   uint8 CollisionMask, const FSkelotInstancesSOA* SOA) const
{
	OutIndices.Reset();

	if (Radius <= 0.0f)
	{
		return;
	}

	float RadiusInCells = Radius * InvCellSize;
	int32 CellRadius = FMath::CeilToInt(RadiusInCells);

	FIntVector CenterCell = GetCellKey(Center);
	float RadiusSquared = Radius * Radius;
	constexpr int32 FullScanCellRadiusThreshold = 32;

	auto TryAddInstance = [&](int32 InstanceIndex)
	{
		if (ExcludeIndices.Contains(InstanceIndex))
		{
			return;
		}

		if (CollisionMask != 0xFF && SOA)
		{
			uint8 InstanceMask = SOA->CollisionMasks[InstanceIndex];
			if ((InstanceMask & CollisionMask) == 0)
			{
				return;
			}
		}

		if (SOA)
		{
			const FVector InstanceLocation(SOA->Locations[InstanceIndex]);
			const float DistSquared = (InstanceLocation - Center).SizeSquared();
			if (DistSquared > RadiusSquared)
			{
				return;
			}
		}

		OutIndices.Add(InstanceIndex);
	};

	if (CellRadius > FullScanCellRadiusThreshold)
	{
		for (const TPair<FIntVector, TArray<int32>>& CellPair : GridCells)
		{
			for (int32 InstanceIndex : CellPair.Value)
			{
				TryAddInstance(InstanceIndex);
			}
		}
		return;
	}

	for (int32 dz = -CellRadius; dz <= CellRadius; dz++)
	{
		for (int32 dy = -CellRadius; dy <= CellRadius; dy++)
		{
			for (int32 dx = -CellRadius; dx <= CellRadius; dx++)
			{
				FIntVector CellKey = CenterCell + FIntVector(dx, dy, dz);
				const TArray<int32>* CellInstances = GridCells.Find(CellKey);

				if (CellInstances && CellInstances->Num() > 0)
				{
					for (int32 InstanceIndex : *CellInstances)
					{
						TryAddInstance(InstanceIndex);
					}
				}
			}
		}
	}
}

void FSkelotSpatialGrid::QueryBox(const FVector& BoxCenter, const FVector& BoxExtent, TArray<int32>& OutIndices,
								   uint8 CollisionMask, const FSkelotInstancesSOA* SOA) const
{
	OutIndices.Reset();

	// 计算需要检查的网格范围
	FVector MinBounds = BoxCenter - BoxExtent;
	FVector MaxBounds = BoxCenter + BoxExtent;

	FIntVector MinCell = GetCellKey(MinBounds);
	FIntVector MaxCell = GetCellKey(MaxBounds);

	// 遍历所有可能重叠的网格单元
	for (int32 z = MinCell.Z; z <= MaxCell.Z; z++)
	{
		for (int32 y = MinCell.Y; y <= MaxCell.Y; y++)
		{
			for (int32 x = MinCell.X; x <= MaxCell.X; x++)
			{
				FIntVector CellKey(x, y, z);
				const TArray<int32>* CellInstances = GridCells.Find(CellKey);

				if (CellInstances && CellInstances->Num() > 0)
				{
					for (int32 InstanceIndex : *CellInstances)
					{
						// 掩码过滤
						if (CollisionMask != 0xFF && SOA)
						{
							uint8 InstanceMask = SOA->CollisionMasks[InstanceIndex];
							if ((InstanceMask & CollisionMask) == 0)
							{
								continue;
							}
						}

						if (!SOA)
						{
							OutIndices.Add(InstanceIndex);
							continue;
						}

						// 盒形范围检测
						const FVector InstanceLocation(SOA->Locations[InstanceIndex]);
						const FVector LocalPos = InstanceLocation - BoxCenter;

						if (FMath::Abs(LocalPos.X) <= BoxExtent.X &&
							FMath::Abs(LocalPos.Y) <= BoxExtent.Y &&
							FMath::Abs(LocalPos.Z) <= BoxExtent.Z)
						{
							OutIndices.Add(InstanceIndex);
						}
					}
				}
			}
		}
	}
}

namespace SkelotSpatialGridDebug
{
	void DrawGridBounds(const FSkelotSpatialGrid& Grid, const FVector& ViewCenter, float DrawRadius, const FColor& Color, float LifeTime)
	{
		// 在编辑器中绘制网格边界
		// 注意：这需要在有 WorldContext 的环境中调用
	}

	void DrawCell(const FIntVector& CellKey, float CellSize, const FColor& Color, float LifeTime)
	{
		// 计算单元的世界空间边界
		FVector MinCorner(CellKey.X * CellSize, CellKey.Y * CellSize, CellKey.Z * CellSize);
		FVector MaxCorner = MinCorner + FVector(CellSize);

		// 绘制单元边界框
		// 注意：这需要在有 WorldContext 的环境中调用
		// DrawDebugBox(...);
	}
}
