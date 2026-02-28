// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SkelotWorldBase.h"

/**
 * 空间哈希网格 - 用于高效的空间查询
 *
 * 将世界空间划分为固定大小的网格单元，每个单元存储该区域内的实例索引。
 * 查询时只需要检查相关单元，将复杂度从 O(n) 降低到 O(1) 平均。
 *
 * 使用场景：
 * - 球形范围查询（范围攻击、AOE技能）
 * - PBD碰撞邻居检测
 * - RVO避障邻居检测
 *
 * 分帧更新说明：
 * - 使用 FrameStride 参数控制分帧更新频率
 * - FrameStride=1: 每帧完整更新（默认）
 * - FrameStride=2: 每帧更新一半实例，降低50%计算量
 * - 注意：分帧更新会导致网格数据有一帧延迟，对大多数应用场景可接受
 */
class FSkelotSpatialGrid
{
public:
	FSkelotSpatialGrid();

	/** 设置网格单元大小（厘米） */
	void SetCellSize(float InCellSize);

	/** 获取网格单元大小（厘米） */
	float GetCellSize() const { return CellSize; }

	/** 获取网格单元大小的倒数（用于快速计算） */
	float GetInvCellSize() const { return InvCellSize; }

	/**
	 * 设置分帧更新步长
	 * @param Stride 步长（1=每帧更新，2=分2帧更新，4=分4帧更新）
	 */
	void SetFrameStride(int32 Stride);

	/** 获取分帧更新步长 */
	int32 GetFrameStride() const { return FrameStride; }

	/**
	 * 清空所有网格数据
	 * 每帧开始时调用，准备接收新的实例数据
	 */
	void Clear();

	/**
	 * 将实例添加到网格中
	 * @param InstanceIndex 实例索引
	 * @param Location 实例位置
	 */
	void AddInstance(int32 InstanceIndex, const FVector& Location);

	/**
	 * 批量添加实例到网格中（完整重建）
	 * @param SOA 实例数据数组
	 * @param NumInstances 实例总数
	 */
	void Rebuild(const FSkelotInstancesSOA& SOA, int32 NumInstances);

	/**
	 * 分帧更新网格（只更新当前帧对应的实例批次）
	 * 基于预研文档的 FrameStride 方案
	 * @param SOA 实例数据数组
	 * @param NumInstances 实例总数
	 */
	void RebuildIncremental(const FSkelotInstancesSOA& SOA, int32 NumInstances);

	/**
	 * 强制完整重建（用于关键查询前）
	 */
	void ForceFullRebuild(const FSkelotInstancesSOA& SOA, int32 NumInstances);

	/**
	 * 查询球形范围内的实例
	 * @param Center 球心位置
	 * @param Radius 球体半径
	 * @param OutIndices 输出实例索引数组
	 * @param CollisionMask 碰撞掩码过滤，0xFF表示不过滤
	 * @param SOA 实例数据数组（用于掩码过滤）
	 */
	void QuerySphere(const FVector& Center, float Radius, TArray<int32>& OutIndices,
					 uint8 CollisionMask = 0xFF, const FSkelotInstancesSOA* SOA = nullptr) const;

	/**
	 * 查询球形范围内的实例（带排除列表）
	 * @param Center 球心位置
	 * @param Radius 球体半径
	 * @param OutIndices 输出实例索引数组
	 * @param ExcludeIndices 要排除的实例索引集合
	 * @param CollisionMask 碰撞掩码过滤
	 * @param SOA 实例数据数组
	 */
	void QuerySphereWithExclusion(const FVector& Center, float Radius, TArray<int32>& OutIndices,
								  const TSet<int32>& ExcludeIndices,
								  uint8 CollisionMask = 0xFF, const FSkelotInstancesSOA* SOA = nullptr) const;

	/**
	 * 查询盒形范围内的实例
	 * @param BoxCenter 盒子中心
	 * @param BoxExtent 盒子半尺寸
	 * @param OutIndices 输出实例索引数组
	 * @param CollisionMask 碰撞掩码过滤
	 * @param SOA 实例数据数组
	 */
	void QueryBox(const FVector& BoxCenter, const FVector& BoxExtent, TArray<int32>& OutIndices,
				  uint8 CollisionMask = 0xFF, const FSkelotInstancesSOA* SOA = nullptr) const;

	/**
	 * 获取指定位置所在的网格单元键
	 * @param Location 世界位置
	 * @return 网格单元键
	 */
	FIntVector GetCellKey(const FVector& Location) const;

	/**
	 * 获取指定单元内的所有实例
	 * @param CellKey 网格单元键
	 * @return 实例索引数组，如果单元不存在则返回空数组
	 */
	const TArray<int32>* GetCellInstances(const FIntVector& CellKey) const;

	/** 获取网格统计信息 */
	int32 GetNumCells() const { return GridCells.Num(); }
	int32 GetTotalInstancesInGrid() const { return TotalInstances; }

	/** 是否使用分帧更新 */
	bool IsUsingIncrementalUpdate() const { return FrameStride > 1; }

private:
	/** 网格单元大小（厘米） */
	float CellSize;

	/** 网格单元大小的倒数 */
	float InvCellSize;

	/** 分帧更新步长（1=每帧更新，2=分2帧更新） */
	int32 FrameStride;

	/** 当前帧索引（用于分帧更新） */
	int32 CurrentFrameIndex;

	/** 网格单元映射：CellKey -> 实例索引数组 */
	TMap<FIntVector, TArray<int32>> GridCells;

	/** 总实例数（统计用） */
	int32 TotalInstances;

	/** 临时数组（用于查询，避免重复分配） */
	mutable TArray<int32> TempCellInstances;
};

/**
 * 空间网格调试绘制工具
 */
namespace SkelotSpatialGridDebug
{
	/** 绘制网格边界 */
	void DrawGridBounds(const FSkelotSpatialGrid& Grid, const FVector& ViewCenter, float DrawRadius, const FColor& Color, float LifeTime = -1.0f);

	/** 绘制指定单元 */
	void DrawCell(const FIntVector& CellKey, float CellSize, const FColor& Color, float LifeTime = -1.0f);
}
