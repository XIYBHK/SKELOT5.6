// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ASkelotWorld;
class FSkelotSpatialGrid;

/**
 * Skelot 调试可视化模式标志
 */
enum class ESkelotDebugDrawMode : uint8
{
	None			= 0,
	InstanceBounds	= 1 << 0,
	SpatialGrid		= 1 << 1,
	CollisionRadius	= 1 << 2,
	Velocities		= 1 << 3,
	NeighborLinks	= 1 << 4,
	All				= 0xFF
};
ENUM_CLASS_FLAGS(ESkelotDebugDrawMode)

/**
 * Skelot 调试统计信息
 */
struct FSkelotDebugStats
{
	int32 TotalInstances = 0;
	int32 ValidInstances = 0;
	int32 SpatialGridCells = 0;
	float PBDSolveTimeMs = 0.0f;
	float RVOComputeTimeMs = 0.0f;
	float SpatialGridUpdateTimeMs = 0.0f;
	float TotalUpdateTimeMs = 0.0f;
	float AverageNeighborDensity = 0.0f;
};

/**
 * Skelot 调试工具类
 *
 * 控制台命令：
 * - Skelot.DrawAllBounds [0|1] - 绘制所有实例包围盒
 * - Skelot.DrawSpatialGrid [0|1] - 绘制空间网格
 * - Skelot.DrawCollisionRadius [0|1] - 绘制碰撞半径
 * - Skelot.DrawVelocities [0|1] - 绘制速度向量
 * - Skelot.DrawNeighborLinks [0|1] - 绘制邻居连接线
 * - Skelot.Stats - 显示统计信息到日志
 * - Skelot.DebugMode [0-255] - 设置调试模式位掩码
 * - Skelot.DebugDrawDistance [距离] - 设置调试绘制距离
 */
class FSkelotDebugTools
{
public:
	static FSkelotDebugTools& Get();

	void Initialize();
	void Shutdown();
	void Tick(ASkelotWorld* SkelotWorld, float DeltaTime);

	ESkelotDebugDrawMode GetDebugDrawMode() const { return DebugDrawMode; }
	void SetDebugDrawMode(ESkelotDebugDrawMode Mode) { DebugDrawMode = Mode; }
	void AddDebugDrawMode(ESkelotDebugDrawMode Mode) { DebugDrawMode |= Mode; }
	void RemoveDebugDrawMode(ESkelotDebugDrawMode Mode) { DebugDrawMode &= ~Mode; }
	bool IsDebugDrawModeEnabled(ESkelotDebugDrawMode Mode) const { return (DebugDrawMode & Mode) != ESkelotDebugDrawMode::None; }

	float GetDebugDrawDistance() const { return DebugDrawDistance; }
	void SetDebugDrawDistance(float Distance) { DebugDrawDistance = Distance; }

	const FSkelotDebugStats& GetDebugStats() const { return DebugStats; }
	void UpdateDebugStats(ASkelotWorld* SkelotWorld);
	void PrintStatsToLog(ASkelotWorld* SkelotWorld);

	void RecordPBDSolveTime(float TimeMs) { DebugStats.PBDSolveTimeMs = TimeMs; }
	void RecordRVOComputeTime(float TimeMs) { DebugStats.RVOComputeTimeMs = TimeMs; }
	void RecordSpatialGridUpdateTime(float TimeMs) { DebugStats.SpatialGridUpdateTimeMs = TimeMs; }
	void RecordTotalUpdateTime(float TimeMs) { DebugStats.TotalUpdateTimeMs = TimeMs; }

private:
	FSkelotDebugTools() = default;
	~FSkelotDebugTools() = default;

	void DrawInstanceBounds(ASkelotWorld* SkelotWorld, float DeltaTime);
	void DrawSpatialGrid(ASkelotWorld* SkelotWorld, float DeltaTime);
	void DrawCollisionRadius(ASkelotWorld* SkelotWorld, float DeltaTime);
	void DrawVelocities(ASkelotWorld* SkelotWorld, float DeltaTime);
	void DrawNeighborLinks(ASkelotWorld* SkelotWorld, float DeltaTime);

	ESkelotDebugDrawMode DebugDrawMode = ESkelotDebugDrawMode::None;
	float DebugDrawDistance = 5000.0f;
	FSkelotDebugStats DebugStats;
	bool bInitialized = false;
};
