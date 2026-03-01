// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotDebugTools.h"
#include "SkelotWorld.h"
#include "SkelotSpatialGrid.h"
#include "SkelotPBDCollision.h"
#include "SkelotRVOSystem.h"
#include "SkelotPBDPlane.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

// 全局控制台变量
static int32 GSkelot_DebugDrawMode = 0;
static float GSkelot_DebugDrawDistance = 5000.0f;

// 控制台命令变量
static FAutoConsoleVariableRef CVarDebugMode(
	TEXT("Skelot.DebugMode"),
	GSkelot_DebugDrawMode,
	TEXT("Debug draw mode bitmask (0=none, 1=bounds, 2=grid, 4=collision, 8=velocities, 16=neighbors, 255=all)")
);

static FAutoConsoleVariableRef CVarDebugDrawDistance(
	TEXT("Skelot.DebugDrawDistance"),
	GSkelot_DebugDrawDistance,
	TEXT("Maximum distance for debug drawing")
);

// 控制台命令实现
static void ToggleDrawAllBounds()
{
	FSkelotDebugTools& Tools = FSkelotDebugTools::Get();
	if (Tools.IsDebugDrawModeEnabled(ESkelotDebugDrawMode::InstanceBounds))
	{
		Tools.RemoveDebugDrawMode(ESkelotDebugDrawMode::InstanceBounds);
		GSkelot_DebugDrawMode &= ~static_cast<int32>(ESkelotDebugDrawMode::InstanceBounds);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawAllBounds disabled"));
	}
	else
	{
		Tools.AddDebugDrawMode(ESkelotDebugDrawMode::InstanceBounds);
		GSkelot_DebugDrawMode |= static_cast<int32>(ESkelotDebugDrawMode::InstanceBounds);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawAllBounds enabled"));
	}
}

static void ToggleDrawSpatialGrid()
{
	FSkelotDebugTools& Tools = FSkelotDebugTools::Get();
	if (Tools.IsDebugDrawModeEnabled(ESkelotDebugDrawMode::SpatialGrid))
	{
		Tools.RemoveDebugDrawMode(ESkelotDebugDrawMode::SpatialGrid);
		GSkelot_DebugDrawMode &= ~static_cast<int32>(ESkelotDebugDrawMode::SpatialGrid);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawSpatialGrid disabled"));
	}
	else
	{
		Tools.AddDebugDrawMode(ESkelotDebugDrawMode::SpatialGrid);
		GSkelot_DebugDrawMode |= static_cast<int32>(ESkelotDebugDrawMode::SpatialGrid);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawSpatialGrid enabled"));
	}
}

static void ToggleDrawCollisionRadius()
{
	FSkelotDebugTools& Tools = FSkelotDebugTools::Get();
	if (Tools.IsDebugDrawModeEnabled(ESkelotDebugDrawMode::CollisionRadius))
	{
		Tools.RemoveDebugDrawMode(ESkelotDebugDrawMode::CollisionRadius);
		GSkelot_DebugDrawMode &= ~static_cast<int32>(ESkelotDebugDrawMode::CollisionRadius);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawCollisionRadius disabled"));
	}
	else
	{
		Tools.AddDebugDrawMode(ESkelotDebugDrawMode::CollisionRadius);
		GSkelot_DebugDrawMode |= static_cast<int32>(ESkelotDebugDrawMode::CollisionRadius);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawCollisionRadius enabled"));
	}
}

static void ToggleDrawVelocities()
{
	FSkelotDebugTools& Tools = FSkelotDebugTools::Get();
	if (Tools.IsDebugDrawModeEnabled(ESkelotDebugDrawMode::Velocities))
	{
		Tools.RemoveDebugDrawMode(ESkelotDebugDrawMode::Velocities);
		GSkelot_DebugDrawMode &= ~static_cast<int32>(ESkelotDebugDrawMode::Velocities);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawVelocities disabled"));
	}
	else
	{
		Tools.AddDebugDrawMode(ESkelotDebugDrawMode::Velocities);
		GSkelot_DebugDrawMode |= static_cast<int32>(ESkelotDebugDrawMode::Velocities);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawVelocities enabled"));
	}
}

static void ToggleDrawNeighborLinks()
{
	FSkelotDebugTools& Tools = FSkelotDebugTools::Get();
	if (Tools.IsDebugDrawModeEnabled(ESkelotDebugDrawMode::NeighborLinks))
	{
		Tools.RemoveDebugDrawMode(ESkelotDebugDrawMode::NeighborLinks);
		GSkelot_DebugDrawMode &= ~static_cast<int32>(ESkelotDebugDrawMode::NeighborLinks);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawNeighborLinks disabled"));
	}
	else
	{
		Tools.AddDebugDrawMode(ESkelotDebugDrawMode::NeighborLinks);
		GSkelot_DebugDrawMode |= static_cast<int32>(ESkelotDebugDrawMode::NeighborLinks);
		UE_LOG(LogTemp, Log, TEXT("Skelot: DrawNeighborLinks enabled"));
	}
}

static void PrintStats()
{
#if WITH_EDITOR
	// 遍历所有世界查找 SkelotWorld
	ASkelotWorld* FoundSkelotWorld = nullptr;
	for (TObjectIterator<ASkelotWorld> It; It; ++It)
	{
		ASkelotWorld* World = *It;
		if (World && World->GetWorld() && World->GetWorld()->IsGameWorld())
		{
			FoundSkelotWorld = World;
			break;
		}
	}

	if (FoundSkelotWorld)
	{
		FSkelotDebugTools::Get().PrintStatsToLog(FoundSkelotWorld);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Skelot: No SkelotWorld found"));
	}
#else
	UE_LOG(LogTemp, Warning, TEXT("Skelot: Stats command only available in Editor builds"));
#endif
}

// 控制台命令注册
static FAutoConsoleCommand CmdDrawAllBounds(
	TEXT("Skelot.DrawAllBounds"),
	TEXT("Toggle drawing of all instance bounding boxes"),
	FConsoleCommandDelegate::CreateStatic(ToggleDrawAllBounds)
);

static FAutoConsoleCommand CmdDrawSpatialGrid(
	TEXT("Skelot.DrawSpatialGrid"),
	TEXT("Toggle drawing of spatial grid cells"),
	FConsoleCommandDelegate::CreateStatic(ToggleDrawSpatialGrid)
);

static FAutoConsoleCommand CmdDrawCollisionRadius(
	TEXT("Skelot.DrawCollisionRadius"),
	TEXT("Toggle drawing of PBD collision radius"),
	FConsoleCommandDelegate::CreateStatic(ToggleDrawCollisionRadius)
);

static FAutoConsoleCommand CmdDrawVelocities(
	TEXT("Skelot.DrawVelocities"),
	TEXT("Toggle drawing of velocity vectors"),
	FConsoleCommandDelegate::CreateStatic(ToggleDrawVelocities)
);

static FAutoConsoleCommand CmdDrawNeighborLinks(
	TEXT("Skelot.DrawNeighborLinks"),
	TEXT("Toggle drawing of neighbor connection lines"),
	FConsoleCommandDelegate::CreateStatic(ToggleDrawNeighborLinks)
);

static FAutoConsoleCommand CmdStats(
	TEXT("Skelot.Stats"),
	TEXT("Print Skelot statistics to log"),
	FConsoleCommandDelegate::CreateStatic(PrintStats)
);

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

FSkelotDebugTools& FSkelotDebugTools::Get()
{
	static FSkelotDebugTools Instance;
	return Instance;
}

void FSkelotDebugTools::Initialize()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bInitialized)
	{
		return;
	}

	// 同步全局变量
	DebugDrawMode = static_cast<ESkelotDebugDrawMode>(GSkelot_DebugDrawMode);
	DebugDrawDistance = GSkelot_DebugDrawDistance;
	bInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("SkelotDebugTools: Initialized"));
#endif
}

void FSkelotDebugTools::Shutdown()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bInitialized = false;
#endif
}

void FSkelotDebugTools::Tick(ASkelotWorld* SkelotWorld, float DeltaTime)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!SkelotWorld || DebugDrawMode == ESkelotDebugDrawMode::None)
	{
		return;
	}

	// 同步全局变量
	DebugDrawMode = static_cast<ESkelotDebugDrawMode>(GSkelot_DebugDrawMode);
	DebugDrawDistance = GSkelot_DebugDrawDistance;

	// 绘制各种调试信息
	if (IsDebugDrawModeEnabled(ESkelotDebugDrawMode::InstanceBounds))
	{
		DrawInstanceBounds(SkelotWorld, DeltaTime);
	}

	if (IsDebugDrawModeEnabled(ESkelotDebugDrawMode::SpatialGrid))
	{
		DrawSpatialGrid(SkelotWorld, DeltaTime);
	}

	if (IsDebugDrawModeEnabled(ESkelotDebugDrawMode::CollisionRadius))
	{
		DrawCollisionRadius(SkelotWorld, DeltaTime);
	}

	if (IsDebugDrawModeEnabled(ESkelotDebugDrawMode::Velocities))
	{
		DrawVelocities(SkelotWorld, DeltaTime);
	}

	if (IsDebugDrawModeEnabled(ESkelotDebugDrawMode::NeighborLinks))
	{
		DrawNeighborLinks(SkelotWorld, DeltaTime);
	}
#endif
}

void FSkelotDebugTools::DrawInstanceBounds(ASkelotWorld* SkelotWorld, float DeltaTime)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UWorld* World = SkelotWorld->GetWorld();
	if (!World)
	{
		return;
	}

	FVector CameraLocation = SkelotWorld->CachedCameraLocation;
	float MaxDistSq = DebugDrawDistance * DebugDrawDistance;

	int32 DrawnCount = 0;
	const int32 MaxDrawCount = 500;

	for (int32 InstanceIndex = 0; InstanceIndex < SkelotWorld->GetNumInstance(); InstanceIndex++)
	{
		if (DrawnCount >= MaxDrawCount)
		{
			break;
		}

		if (!SkelotWorld->IsInstanceAlive(InstanceIndex))
		{
			continue;
		}

		FVector Location = SkelotWorld->GetInstanceLocation(InstanceIndex);
		float DistSq = FVector::DistSquared(Location, CameraLocation);

		if (DistSq <= MaxDistSq)
		{
			FRenderBounds WorldBound = SkelotWorld->CalcInstanceBoundWS(InstanceIndex);
			float Dist = FMath::Sqrt(DistSq);
			float Alpha = 1.0f - (Dist / DebugDrawDistance);
			FColor Color = FLinearColor(0.0f, 1.0f, 0.0f, Alpha).ToFColor(true);

			DrawDebugBox(
				World,
				FVector(WorldBound.GetCenter()),
				FVector(WorldBound.GetExtent()),
				Color,
				false,
				DeltaTime * 1.1f,
				0,
				1.0f
			);

			DrawnCount++;
		}
	}

	if (DrawnCount > 0)
	{
		DrawDebugString(
			World,
			CameraLocation + FVector(0, 0, 100),
			FString::Printf(TEXT("Bounds: %d"), DrawnCount),
			nullptr,
			FColor::Green,
			DeltaTime * 1.1f
		);
	}
#endif
}

void FSkelotDebugTools::DrawSpatialGrid(ASkelotWorld* SkelotWorld, float DeltaTime)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UWorld* World = SkelotWorld->GetWorld();
	if (!World)
	{
		return;
	}

	const FSkelotSpatialGrid& Grid = SkelotWorld->SpatialGrid;
	float CellSize = Grid.GetCellSize();
	FVector CameraLocation = SkelotWorld->CachedCameraLocation;

	FIntVector CenterCell = Grid.GetCellKey(CameraLocation);
	int32 CellRadius = FMath::CeilToInt(DebugDrawDistance / CellSize);

	int32 DrawnCells = 0;
	const int32 MaxCellsToDraw = 100;

	for (int32 X = -CellRadius; X <= CellRadius && DrawnCells < MaxCellsToDraw; ++X)
	{
		for (int32 Y = -CellRadius; Y <= CellRadius && DrawnCells < MaxCellsToDraw; ++Y)
		{
			for (int32 Z = -1; Z <= 1 && DrawnCells < MaxCellsToDraw; ++Z)
			{
				FIntVector CellKey = CenterCell + FIntVector(X, Y, Z);
				const TArray<int32>* Instances = Grid.GetCellInstances(CellKey);
				if (Instances && Instances->Num() > 0)
				{
					FVector CellCenter(
						CellKey.X * CellSize + CellSize * 0.5f,
						CellKey.Y * CellSize + CellSize * 0.5f,
						CellKey.Z * CellSize + CellSize * 0.5f
					);

					float Density = FMath::Clamp(Instances->Num() / 10.0f, 0.0f, 1.0f);
					FColor Color = FLinearColor(Density, 1.0f - Density, 0.0f, 0.5f).ToFColor(true);

					DrawDebugBox(World, CellCenter, FVector(CellSize * 0.5f), Color, false, DeltaTime * 1.1f, 0, 0.5f);
					DrawDebugString(World, CellCenter, FString::Printf(TEXT("%d"), Instances->Num()), nullptr, FColor::White, DeltaTime * 1.1f, false, 0.5f);
					DrawnCells++;
				}
			}
		}
	}

	DrawDebugString(
		World,
		CameraLocation + FVector(0, 0, 150),
		FString::Printf(TEXT("Grid: %d cells, size=%.0f"), Grid.GetNumCells(), CellSize),
		nullptr,
		FColor::Yellow,
		DeltaTime * 1.1f
	);
#endif
}

void FSkelotDebugTools::DrawCollisionRadius(ASkelotWorld* SkelotWorld, float DeltaTime)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UWorld* World = SkelotWorld->GetWorld();
	if (!World || !SkelotWorld->IsPBDEnabled())
	{
		return;
	}

	FVector CameraLocation = SkelotWorld->CachedCameraLocation;
	float MaxDistSq = DebugDrawDistance * DebugDrawDistance;
	float CollisionRadius = SkelotWorld->GetPBDCollisionRadius();

	int32 DrawnCount = 0;
	const int32 MaxDrawCount = 300;

	for (int32 InstanceIndex = 0; InstanceIndex < SkelotWorld->GetNumInstance(); InstanceIndex++)
	{
		if (DrawnCount >= MaxDrawCount)
		{
			break;
		}

		if (!SkelotWorld->IsInstanceAlive(InstanceIndex))
		{
			continue;
		}

		FVector Location = SkelotWorld->GetInstanceLocation(InstanceIndex);
		float DistSq = FVector::DistSquared(Location, CameraLocation);

		if (DistSq <= MaxDistSq)
		{
			DrawDebugSphere(World, Location, CollisionRadius, 12, FColor::Red, false, DeltaTime * 1.1f, 0, 0.5f);
			DrawnCount++;
		}
	}

	DrawDebugString(
		World,
		CameraLocation + FVector(0, 0, 200),
		FString::Printf(TEXT("PBD: radius=%.0f, drawn=%d"), CollisionRadius, DrawnCount),
		nullptr,
		FColor::Red,
		DeltaTime * 1.1f
	);
#endif
}

void FSkelotDebugTools::DrawVelocities(ASkelotWorld* SkelotWorld, float DeltaTime)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UWorld* World = SkelotWorld->GetWorld();
	if (!World)
	{
		return;
	}

	FVector CameraLocation = SkelotWorld->CachedCameraLocation;
	float MaxDistSq = DebugDrawDistance * DebugDrawDistance;

	int32 DrawnCount = 0;
	const int32 MaxDrawCount = 200;

	for (int32 InstanceIndex = 0; InstanceIndex < SkelotWorld->GetNumInstance(); InstanceIndex++)
	{
		if (DrawnCount >= MaxDrawCount)
		{
			break;
		}

		if (!SkelotWorld->IsInstanceAlive(InstanceIndex))
		{
			continue;
		}

		FVector Location = SkelotWorld->GetInstanceLocation(InstanceIndex);
		float DistSq = FVector::DistSquared(Location, CameraLocation);

		if (DistSq <= MaxDistSq)
		{
			FVector3f Velocity = SkelotWorld->GetInstanceVelocity(InstanceIndex);
			float Speed = Velocity.Size();

			if (Speed > 1.0f)
			{
				FVector VelEnd = Location + FVector(Velocity) * 0.1f;
				float SpeedNorm = FMath::Clamp(Speed / 500.0f, 0.0f, 1.0f);
				FColor Color = FLinearColor(SpeedNorm, 0.0f, 1.0f - SpeedNorm, 1.0f).ToFColor(true);

				DrawDebugDirectionalArrow(World, Location, VelEnd, 10.0f, Color, false, DeltaTime * 1.1f, 0, 1.0f);
				DrawnCount++;
			}
		}
	}

	DrawDebugString(
		World,
		CameraLocation + FVector(0, 0, 250),
		FString::Printf(TEXT("Velocities: %d"), DrawnCount),
		nullptr,
		FColor::Cyan,
		DeltaTime * 1.1f
	);
#endif
}

void FSkelotDebugTools::DrawNeighborLinks(ASkelotWorld* SkelotWorld, float DeltaTime)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UWorld* World = SkelotWorld->GetWorld();
	if (!World || !SkelotWorld->IsPBDEnabled())
	{
		return;
	}

	FVector CameraLocation = SkelotWorld->CachedCameraLocation;
	float MaxDistSq = DebugDrawDistance * DebugDrawDistance * 0.25f;
	float CollisionRadius = SkelotWorld->GetPBDCollisionRadius();

	int32 DrawnLinks = 0;
	const int32 MaxLinksToDraw = 100;
	const int32 MaxNeighbors = 8;

	for (int32 InstanceIndex = 0; InstanceIndex < SkelotWorld->GetNumInstance(); InstanceIndex++)
	{
		if (DrawnLinks >= MaxLinksToDraw)
		{
			break;
		}

		FVector Location = SkelotWorld->GetInstanceLocation(InstanceIndex);
		float DistSq = FVector::DistSquared(Location, CameraLocation);

		if (DistSq <= MaxDistSq && SkelotWorld->IsInstanceAlive(InstanceIndex))
		{
			TArray<int32> Neighbors;
			SkelotWorld->SpatialGrid.QuerySphere(Location, CollisionRadius * 2.0f, Neighbors, 0xFF, &SkelotWorld->SOA);

			int32 NeighborCount = 0;
			for (int32 NeighborIdx : Neighbors)
			{
				if (NeighborIdx == InstanceIndex)
				{
					continue;
				}

				if (NeighborCount >= MaxNeighbors || DrawnLinks >= MaxLinksToDraw)
				{
					break;
				}

				FVector NeighborLocation = SkelotWorld->GetInstanceLocation(NeighborIdx);
				float Dist = FVector::Dist(Location, NeighborLocation);

				if (Dist < CollisionRadius * 2.0f)
				{
					float Alpha = 1.0f - (Dist / (CollisionRadius * 2.0f));
					FColor Color = FLinearColor(1.0f, Alpha, 0.0f, 0.5f).ToFColor(true);
					DrawDebugLine(World, Location, NeighborLocation, Color, false, DeltaTime * 1.1f, 0, 0.3f);
					DrawnLinks++;
					NeighborCount++;
				}
			}
		}
	}

	DrawDebugString(
		World,
		CameraLocation + FVector(0, 0, 300),
		FString::Printf(TEXT("NeighborLinks: %d"), DrawnLinks),
		nullptr,
		FColor::Orange,
		DeltaTime * 1.1f
	);
#endif
}

void FSkelotDebugTools::UpdateDebugStats(ASkelotWorld* SkelotWorld)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!SkelotWorld)
	{
		return;
	}

	DebugStats.TotalInstances = SkelotWorld->GetNumInstance();
	DebugStats.ValidInstances = SkelotWorld->GetNumValidInstance();
	DebugStats.SpatialGridCells = SkelotWorld->SpatialGrid.GetNumCells();

	if (DebugStats.ValidInstances > 0)
	{
		float CollisionRadius = SkelotWorld->GetPBDCollisionRadius();
		int32 TotalNeighbors = 0;
		int32 SampleCount = 0;
		const int32 MaxSamples = 100;

		for (int32 InstanceIndex = 0; InstanceIndex < SkelotWorld->GetNumInstance() && SampleCount < MaxSamples; InstanceIndex++)
		{
				if (!SkelotWorld->IsInstanceAlive(InstanceIndex))
				{
					continue;
				}

				FVector Location = SkelotWorld->GetInstanceLocation(InstanceIndex);
				TArray<int32> Neighbors;
				SkelotWorld->SpatialGrid.QuerySphere(Location, CollisionRadius * 2.0f, Neighbors, 0xFF, &SkelotWorld->SOA);
				TotalNeighbors += Neighbors.Num() - 1;
				SampleCount++;
			}

			DebugStats.AverageNeighborDensity = SampleCount > 0 ? static_cast<float>(TotalNeighbors) / SampleCount : 0.0f;
		}
		else
		{
			DebugStats.AverageNeighborDensity = 0.0f;
		}
#endif
}

void FSkelotDebugTools::PrintStatsToLog(ASkelotWorld* SkelotWorld)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!SkelotWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("Skelot: No SkelotWorld available"));
		return;
	}

	UpdateDebugStats(SkelotWorld);

	UE_LOG(LogTemp, Log, TEXT("=== Skelot Statistics ==="));
	UE_LOG(LogTemp, Log, TEXT("  Total Instances: %d"), DebugStats.TotalInstances);
	UE_LOG(LogTemp, Log, TEXT("  Valid Instances: %d"), DebugStats.ValidInstances);
	UE_LOG(LogTemp, Log, TEXT("  Spatial Grid Cells: %d"), DebugStats.SpatialGridCells);
	UE_LOG(LogTemp, Log, TEXT("  PBD Solve Time: %.3f ms"), DebugStats.PBDSolveTimeMs);
	UE_LOG(LogTemp, Log, TEXT("  RVO Compute Time: %.3f ms"), DebugStats.RVOComputeTimeMs);
	UE_LOG(LogTemp, Log, TEXT("  Spatial Grid Update Time: %.3f ms"), DebugStats.SpatialGridUpdateTimeMs);
	UE_LOG(LogTemp, Log, TEXT("  Total Update Time: %.3f ms"), DebugStats.TotalUpdateTimeMs);
	UE_LOG(LogTemp, Log, TEXT("  Average Neighbor Density: %.2f"), DebugStats.AverageNeighborDensity);
	UE_LOG(LogTemp, Log, TEXT("  PBD Enabled: %s"), SkelotWorld->IsPBDEnabled() ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogTemp, Log, TEXT("  RVO Enabled: %s"), SkelotWorld->IsRVOEnabled() ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogTemp, Log, TEXT("  LOD Update Frequency: %s"), SkelotWorld->IsLODUpdateFrequencyEnabled() ? TEXT("Yes") : TEXT("No"));

	if (SkelotWorld->IsPBDEnabled())
	{
		const FSkelotPBDConfig& PBDConfig = SkelotWorld->GetPBDConfig();
		UE_LOG(LogTemp, Log, TEXT("  PBD Collision Radius: %.1f"), PBDConfig.CollisionRadius);
		UE_LOG(LogTemp, Log, TEXT("  PBD Iterations: %d"), PBDConfig.IterationCount);
		UE_LOG(LogTemp, Log, TEXT("  PBD Relaxation Factor: %.2f"), PBDConfig.RelaxationFactor);
	}

	if (SkelotWorld->IsRVOEnabled())
	{
		const FSkelotRVOConfig& RVOConfig = SkelotWorld->GetRVOConfig();
		UE_LOG(LogTemp, Log, TEXT("  RVO Neighbor Radius: %.1f"), RVOConfig.NeighborRadius);
		UE_LOG(LogTemp, Log, TEXT("  RVO Time Horizon: %.2f"), RVOConfig.TimeHorizon);
		UE_LOG(LogTemp, Log, TEXT("  RVO Max Neighbors: %d"), RVOConfig.MaxNeighbors);
	}

	UE_LOG(LogTemp, Log, TEXT("========================="));
#endif
}
