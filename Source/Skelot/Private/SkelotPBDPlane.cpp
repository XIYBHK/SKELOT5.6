// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotPBDPlane.h"
#include "SkelotWorld.h"
#include "DrawDebugHelpers.h"

ASkelotPBDPlane::ASkelotPBDPlane()
{
	// 启用 Tick 以支持调试可视化
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 加载推荐的默认配置
	PBDConfig = FSkelotPBDConfig::GetRecommendedConfig();
	RVOConfig = FSkelotRVOConfig::GetRecommendedConfig();
	AntiJitterConfig = FSkelotAntiJitterConfig::GetRecommendedConfig();
}

void ASkelotPBDPlane::BeginPlay()
{
	Super::BeginPlay();

	// 自动应用配置到 SkelotWorld
	if (bAutoApplyConfig)
	{
		ApplyConfigToWorld();
	}
}

void ASkelotPBDPlane::ApplyConfigToWorld()
{
	ASkelotWorld* SkelotWorld = GetSkelotWorld();
	if (!SkelotWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASkelotPBDPlane::ApplyConfigToWorld - SkelotWorld not found"));
		return;
	}

	// 应用 PBD 配置
	SkelotWorld->SetPBDConfig(PBDConfig);

	// 应用 RVO 配置
	SkelotWorld->SetRVOConfig(RVOConfig);

	// 应用抗抖动配置
	SkelotWorld->SetAntiJitterConfig(AntiJitterConfig);

	UE_LOG(LogTemp, Log, TEXT("ASkelotPBDPlane - Applied config to SkelotWorld: PBD=%s, RVO=%s, Radius=%.1f, Iterations=%d"),
		PBDConfig.bEnablePBD ? TEXT("Enabled") : TEXT("Disabled"),
		RVOConfig.bEnableRVO ? TEXT("Enabled") : TEXT("Disabled"),
		PBDConfig.CollisionRadius,
		PBDConfig.IterationCount);
}

void ASkelotPBDPlane::LoadRecommendedConfig()
{
	PBDConfig = FSkelotPBDConfig::GetRecommendedConfig();
	RVOConfig = FSkelotRVOConfig::GetRecommendedConfig();
	AntiJitterConfig = FSkelotAntiJitterConfig::GetRecommendedConfig();
}

ASkelotWorld* ASkelotPBDPlane::GetSkelotWorld() const
{
	return ASkelotWorld::Get(GetWorld(), false);
}

#if WITH_EDITOR
void ASkelotPBDPlane::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 在编辑器中修改属性时自动应用到 SkelotWorld（如果在运行）
	if (PropertyChangedEvent.Property != nullptr && GetWorld()->IsGameWorld())
	{
		ApplyConfigToWorld();
	}
}
#endif

void ASkelotPBDPlane::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
	// 调试可视化
	if (bShowDebugVisualization)
	{
		ASkelotWorld* SkelotWorld = GetSkelotWorld();
		if (SkelotWorld && SkelotWorld->IsPBDEnabled())
		{
			// 绘制碰撞半径范围指示
			FVector Center = GetActorLocation();
			float Radius = SkelotWorld->GetPBDCollisionRadius();

			DrawDebugSphere(GetWorld(), Center, Radius, 16, FColor::Green, false, DeltaTime, 0, 1.0f);

			// 绘制网格单元大小指示
			float CellSize = SkelotWorld->GetSpatialGridCellSize();
			DrawDebugBox(GetWorld(), Center, FVector(CellSize * 0.5f), FColor::Yellow, false, DeltaTime, 0, 0.5f);
		}
	}
#endif
}
