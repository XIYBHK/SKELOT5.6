// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotSpatialGrid.h"
#include "DrawDebugHelpers.h"

FSkelotSpatialGrid::FSkelotSpatialGrid()
	: CellSize(200.0f)  // 默认200厘米（2米）
	, InvCellSize(1.0f / 200.0f)
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

const TArray<int32>* FSkelotSpatialGrid::GetCellInstances(const FIntVector& CellKey) const
{
	return GridCells.Find(CellKey);
}

void FSkelotSpatialGrid::QuerySphere(const FVector& Center, float Radius, TArray<int32>& OutIndices,
									  uint8 CollisionMask, const FSkelotInstancesSOA* SOA) const
{
	OutIndices.Reset();

	// 计算需要检查的网格范围
	float RadiusInCells = Radius * InvCellSize;
	int32 CellRadius = FMath::CeilToInt(RadiusInCells);

	FIntVector CenterCell = GetCellKey(Center);

	// 计算半径平方用于距离检测
	float RadiusSquared = Radius * Radius;

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
					// 检查单元内的每个实例
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

						// 距离检测
						FVector InstanceLocation = SOA ? FVector(SOA->Locations[InstanceIndex]) : FVector::ZeroVector;
						float DistSquared = (InstanceLocation - Center).SizeSquared();

						if (DistSquared <= RadiusSquared)
						{
							OutIndices.Add(InstanceIndex);
						}
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

	float RadiusInCells = Radius * InvCellSize;
	int32 CellRadius = FMath::CeilToInt(RadiusInCells);

	FIntVector CenterCell = GetCellKey(Center);
	float RadiusSquared = Radius * Radius;

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
						// 排除列表检查
						if (ExcludeIndices.Contains(InstanceIndex))
						{
							continue;
						}

						// 掩码过滤
						if (CollisionMask != 0xFF && SOA)
						{
							uint8 InstanceMask = SOA->CollisionMasks[InstanceIndex];
							if ((InstanceMask & CollisionMask) == 0)
							{
								continue;
							}
						}

						FVector InstanceLocation = SOA ? FVector(SOA->Locations[InstanceIndex]) : FVector::ZeroVector;
						float DistSquared = (InstanceLocation - Center).SizeSquared();

						if (DistSquared <= RadiusSquared)
						{
							OutIndices.Add(InstanceIndex);
						}
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

						// 盒形范围检测
						FVector InstanceLocation = SOA ? FVector(SOA->Locations[InstanceIndex]) : FVector::ZeroVector;
						FVector LocalPos = InstanceLocation - BoxCenter;

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
