// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotSpatialGrid.h"
#include "SkelotPrivate.h"

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
	TArray<int32>& CellInstances = GridCells.FindOrAdd(FSkelotCellKey(CellKey));
	CellInstances.Add(InstanceIndex);
	TotalInstances++;
}

void FSkelotSpatialGrid::Rebuild(const FSkelotInstancesSOA& SOA, int32 NumInstances)
{
	SKELOT_SCOPE_CYCLE_COUNTER(SpatialGrid_Rebuild);
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
	return GridCells.Find(FSkelotCellKey(CellKey));
}

//////////////////////////////////////////////////////////////////////////
// 球形查询：公共逻辑提取到模板方法，消除 QuerySphere / QuerySphereWithExclusion 重复
//////////////////////////////////////////////////////////////////////////

template<typename FilterFunc>
void FSkelotSpatialGrid::QuerySphereInternal(const FVector& Center, float Radius, TArray<int32>& OutIndices,
											  uint8 CollisionMask, const FSkelotInstancesSOA* SOA, FilterFunc Filter) const
{
	SKELOT_SCOPE_CYCLE_COUNTER(SpatialGrid_QuerySphere);
	OutIndices.Reset();

	if (Radius <= 0.0f)
	{
		return;
	}

	if (!ensureMsgf(SOA != nullptr, TEXT("QuerySphere: SOA is null, results will be unfiltered")))
	{
		return;
	}

	const float RadiusSquared = Radius * Radius;
	const int32 CellRadius = FMath::CeilToInt(Radius * InvCellSize);
	const FIntVector CenterCell = GetCellKey(Center);
	constexpr int32 FullScanCellRadiusThreshold = 32;

	auto TryAddInstance = [&](int32 InstanceIndex)
	{
		if (CollisionMask != 0xFF && SOA)
		{
			if ((SOA->CollisionMasks[InstanceIndex] & CollisionMask) == 0)
			{
				return;
			}
		}

		if (!Filter(InstanceIndex))
		{
			return;
		}

		if (SOA)
		{
			const FVector InstanceLocation(SOA->Locations[InstanceIndex]);
			if ((InstanceLocation - Center).SizeSquared() > RadiusSquared)
			{
				return;
			}
		}

		OutIndices.Add(InstanceIndex);
	};

	if (CellRadius > FullScanCellRadiusThreshold)
	{
		for (const TPair<FSkelotCellKey, TArray<int32>>& CellPair : GridCells)
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
				const TArray<int32>* CellInstances = GridCells.Find(FSkelotCellKey(CellKey));

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

void FSkelotSpatialGrid::QuerySphere(const FVector& Center, float Radius, TArray<int32>& OutIndices,
									  uint8 CollisionMask, const FSkelotInstancesSOA* SOA) const
{
	QuerySphereInternal(Center, Radius, OutIndices, CollisionMask, SOA,
		[](int32) { return true; });
}

void FSkelotSpatialGrid::QuerySphereWithExclusion(const FVector& Center, float Radius, TArray<int32>& OutIndices,
												   const TSet<int32>& ExcludeIndices,
												   uint8 CollisionMask, const FSkelotInstancesSOA* SOA) const
{
	QuerySphereInternal(Center, Radius, OutIndices, CollisionMask, SOA,
		[&ExcludeIndices](int32 InstanceIndex) { return !ExcludeIndices.Contains(InstanceIndex); });
}

void FSkelotSpatialGrid::QueryBox(const FVector& BoxCenter, const FVector& BoxExtent, TArray<int32>& OutIndices,
								   uint8 CollisionMask, const FSkelotInstancesSOA* SOA) const
{
	SKELOT_SCOPE_CYCLE_COUNTER(SpatialGrid_QueryBox);
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
				const TArray<int32>* CellInstances = GridCells.Find(FSkelotCellKey(CellKey));

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


