// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotExampleActors.h"
#include "SkelotWorld.h"
#include "SkelotGeometryTools.h"
#include "Animation/AnimSequence.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"

namespace
{
	APlayerController* GetExamplePlayerController(const AActor* Actor)
	{
		UWorld* World = Actor ? Actor->GetWorld() : nullptr;
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	uint64 GetStressTestHUDMessageKey(const ASkelotExampleStressTest* Actor, int32 LineIndex)
	{
		return (static_cast<uint64>(reinterpret_cast<UPTRINT>(Actor)) << 8) + static_cast<uint64>(LineIndex);
	}

	void ClearStressTestHUDMessages(const ASkelotExampleStressTest* Actor, int32 MaxLines = 32)
	{
		if (!GEngine || !Actor)
		{
			return;
		}

		for (int32 LineIndex = 0; LineIndex < MaxLines; ++LineIndex)
		{
			GEngine->RemoveOnScreenDebugMessage(GetStressTestHUDMessageKey(Actor, LineIndex));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// ASkelotExampleBasicInstance
//////////////////////////////////////////////////////////////////////////

ASkelotExampleBasicInstance::ASkelotExampleBasicInstance()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASkelotExampleBasicInstance::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoCreateOnBeginPlay && RenderParams)
	{
		CreateBatchInstances(InstanceCount, GetActorLocation(), SpawnRadius);

		if (AnimationToPlay)
		{
			PlayAnimationForAllInstances();
		}
	}
}

void ASkelotExampleBasicInstance::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyAllInstances();
	Super::EndPlay(EndPlayReason);
}

void ASkelotExampleBasicInstance::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	HandleKeyboardInput();
}

FSkelotInstanceHandle ASkelotExampleBasicInstance::CreateSingleInstance(const FVector& Location)
{
	if (!RenderParams)
	{
		return FSkelotInstanceHandle();
	}

	ASkelotWorld* World = USkelotWorldSubsystem::GetSingleton(this);
	if (!World)
	{
		return FSkelotInstanceHandle();
	}

	FTransform Transform(Location);
	FSkelotInstanceHandle Handle = World->CreateInstance(Transform, RenderParams);

	if (Handle.IsValid())
	{
		InstanceHandles.Add(Handle);

		// 如果有动画，立即播放
		if (AnimationToPlay)
		{
			FSkelotAnimPlayParams Params;
			Params.Animation = AnimationToPlay;
			Params.bLoop = bLoopAnimation;
			Params.PlayScale = PlayRate;
			World->InstancePlayAnimation(Handle.InstanceIndex, Params);
		}
	}

	return Handle;
}

void ASkelotExampleBasicInstance::CreateBatchInstances(int32 Count, const FVector& Center, float Radius)
{
	if (!RenderParams || Count <= 0)
	{
		return;
	}

	ASkelotWorld* World = USkelotWorldSubsystem::GetSingleton(this);
	if (!World)
	{
		return;
	}

	// 生成随机位置
	TArray<FTransform> Transforms;
	Transforms.Reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		FVector Offset = FMath::VRand() * FMath::FRand() * Radius;
		Offset.Z = 0; // 保持在地面
		FVector Location = Center + Offset;
		Transforms.Add(FTransform(Location));
	}

	// 批量创建
	TArray<FSkelotInstanceHandle> NewHandles;
	World->CreateInstances(Transforms, RenderParams, NewHandles);

	InstanceHandles.Append(NewHandles);

	// 如果有动画，为所有新实例播放
	if (AnimationToPlay && NewHandles.Num() > 0)
	{
		FSkelotAnimPlayParams Params;
		Params.Animation = AnimationToPlay;
		Params.bLoop = bLoopAnimation;
		Params.PlayScale = PlayRate;

		for (const FSkelotInstanceHandle& Handle : NewHandles)
		{
			World->InstancePlayAnimation(Handle.InstanceIndex, Params);
		}
	}
}

void ASkelotExampleBasicInstance::DestroyInstanceByHandle(FSkelotInstanceHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	ASkelotWorld* World = USkelotWorldSubsystem::GetSingleton(this);
	if (World)
	{
		World->DestroyInstance(Handle);
		InstanceHandles.Remove(Handle);
	}
}

void ASkelotExampleBasicInstance::DestroyAllInstances()
{
	USkelotWorldSubsystem::Skelot_DestroyInstances(this, InstanceHandles);
	InstanceHandles.Empty();
}

void ASkelotExampleBasicInstance::PlayAnimationForAllInstances()
{
	if (!AnimationToPlay || InstanceHandles.Num() == 0)
	{
		return;
	}

	ASkelotWorld* World = USkelotWorldSubsystem::GetSingleton(this);
	if (!World)
	{
		return;
	}

	FSkelotAnimPlayParams Params;
	Params.Animation = AnimationToPlay;
	Params.bLoop = bLoopAnimation;
	Params.PlayScale = PlayRate;

	for (const FSkelotInstanceHandle& Handle : InstanceHandles)
	{
		if (World->IsHandleValid(Handle))
		{
			World->InstancePlayAnimation(Handle.InstanceIndex, Params);
		}
	}
}

void ASkelotExampleBasicInstance::ToggleAnimationPaused()
{
	bAnimationPaused = !bAnimationPaused;

	ASkelotWorld* World = USkelotWorldSubsystem::GetSingleton(this);
	if (!World)
	{
		return;
	}

	for (const FSkelotInstanceHandle& Handle : InstanceHandles)
	{
		if (World->IsHandleValid(Handle))
		{
			World->SetAnimationPaused(Handle.InstanceIndex, bAnimationPaused);
		}
	}
}

void ASkelotExampleBasicInstance::HandleKeyboardInput()
{
	APlayerController* PC = GetExamplePlayerController(this);
	if (!PC)
	{
		return;
	}

	// 创建单个实例
	if (PC->WasInputKeyJustPressed(CreateInstanceKey))
	{
		FVector RandomOffset = FMath::VRand() * FMath::FRand() * SpawnRadius;
		RandomOffset.Z = 0;
		CreateSingleInstance(GetActorLocation() + RandomOffset);
	}

	// 销毁最后一个实例
	if (PC->WasInputKeyJustPressed(DestroyInstanceKey))
	{
		if (InstanceHandles.Num() > 0)
		{
			DestroyInstanceByHandle(InstanceHandles.Last());
		}
	}

	// 播放动画
	if (PC->WasInputKeyJustPressed(PlayAnimationKey))
	{
		PlayAnimationForAllInstances();
	}

	// 切换暂停
	if (PC->WasInputKeyJustPressed(TogglePauseKey))
	{
		ToggleAnimationPaused();
	}

	// 清除所有
	if (PC->WasInputKeyJustPressed(ClearAllKey))
	{
		DestroyAllInstances();
	}
}


//////////////////////////////////////////////////////////////////////////
// ASkelotExampleCollisionAvoidance
//////////////////////////////////////////////////////////////////////////

ASkelotExampleCollisionAvoidance::ASkelotExampleCollisionAvoidance()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASkelotExampleCollisionAvoidance::BeginPlay()
{
	Super::BeginPlay();

	SkelotWorld = USkelotWorldSubsystem::GetSingleton(this);
	if (SkelotWorld)
	{
		ApplyConfigs();
		InitializeInstances();
	}
}

void ASkelotExampleCollisionAvoidance::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SkelotWorld)
	{
		TArray<FSkelotInstanceHandle> HandlesToDestroy;
		for (const FInstanceData& Data : InstanceDataArray)
		{
			HandlesToDestroy.Add(Data.Handle);
		}
		USkelotWorldSubsystem::Skelot_DestroyInstances(this, HandlesToDestroy);
	}
	InstanceDataArray.Empty();
	Super::EndPlay(EndPlayReason);
}

void ASkelotExampleCollisionAvoidance::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	HandleKeyboardInput();
	UpdateInstanceMovement(DeltaTime);
}

void ASkelotExampleCollisionAvoidance::InitializeInstances()
{
	if (!RenderParams || InstanceCount <= 0)
	{
		return;
	}

	SkelotWorld = USkelotWorldSubsystem::GetSingleton(this);
	if (!SkelotWorld)
	{
		return;
	}

	// 清除现有实例，避免重复初始化后遗留旧实例
	if (InstanceDataArray.Num() > 0)
	{
		TArray<FSkelotInstanceHandle> HandlesToDestroy;
		HandlesToDestroy.Reserve(InstanceDataArray.Num());
		for (const FInstanceData& Data : InstanceDataArray)
		{
			HandlesToDestroy.Add(Data.Handle);
		}
		USkelotWorldSubsystem::Skelot_DestroyInstances(this, HandlesToDestroy);
	}

	InstanceDataArray.Empty();

	// 生成随机位置
	TArray<FTransform> Transforms;
	Transforms.Reserve(InstanceCount);

	for (int32 i = 0; i < InstanceCount; ++i)
	{
		FVector Offset = FMath::VRand() * FMath::FRand() * SpawnRadius;
		Offset.Z = 0;
		FVector Location = GetActorLocation() + Offset;
		Transforms.Add(FTransform(Location));
	}

	// 批量创建
	TArray<FSkelotInstanceHandle> NewHandles;
	SkelotWorld->CreateInstances(Transforms, RenderParams, NewHandles);

	// 初始化数据
	InstanceDataArray.Reserve(NewHandles.Num());
	for (const FSkelotInstanceHandle& Handle : NewHandles)
	{
		FInstanceData Data;
		Data.Handle = Handle;
		Data.StartLocation = SkelotWorld->GetInstanceLocation(Handle.InstanceIndex);
		Data.CurrentTarget = GetActorLocation() + TargetLocation;
		Data.bArrived = false;
		Data.WaitTimer = 0.0f;
		Data.bIsWalkAnimationPlaying = false;
		Data.bIsIdleAnimationPlaying = false;
		InstanceDataArray.Add(Data);
	}

	// 播放行走动画
	if (WalkAnimation)
	{
		FSkelotAnimPlayParams Params;
		Params.Animation = WalkAnimation;
		Params.bLoop = true;
		Params.PlayScale = 1.0f;

		for (const FInstanceData& Data : InstanceDataArray)
		{
			SkelotWorld->InstancePlayAnimation(Data.Handle.InstanceIndex, Params);
		}

		for (FInstanceData& Data : InstanceDataArray)
		{
			Data.bIsWalkAnimationPlaying = true;
			Data.bIsIdleAnimationPlaying = false;
		}
	}

	ArrivedCount = 0;
}

void ASkelotExampleCollisionAvoidance::ResetScene()
{
	// 销毁现有实例
	if (SkelotWorld)
	{
		TArray<FSkelotInstanceHandle> HandlesToDestroy;
		for (const FInstanceData& Data : InstanceDataArray)
		{
			HandlesToDestroy.Add(Data.Handle);
		}
		USkelotWorldSubsystem::Skelot_DestroyInstances(this, HandlesToDestroy);
	}

	InstanceDataArray.Empty();
	ArrivedCount = 0;

	// 重新初始化
	InitializeInstances();
}

void ASkelotExampleCollisionAvoidance::TogglePBD()
{
	bEnablePBD = !bEnablePBD;
	ApplyConfigs();
}

void ASkelotExampleCollisionAvoidance::ToggleRVO()
{
	bEnableRVO = !bEnableRVO;
	ApplyConfigs();
}

void ASkelotExampleCollisionAvoidance::SetNewTarget(const FVector& NewTarget)
{
	TargetLocation = NewTarget;

	// 更新所有实例的目标
	for (FInstanceData& Data : InstanceDataArray)
	{
		Data.CurrentTarget = GetActorLocation() + NewTarget;
		Data.bArrived = false;
		Data.WaitTimer = 0.0f;
	}

	ArrivedCount = 0;
}

void ASkelotExampleCollisionAvoidance::HandleKeyboardInput()
{
	APlayerController* PC = GetExamplePlayerController(this);
	if (!PC)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(ResetKey))
	{
		ResetScene();
	}

	if (PC->WasInputKeyJustPressed(TogglePBDKey))
	{
		TogglePBD();
	}

	if (PC->WasInputKeyJustPressed(ToggleRVOKey))
	{
		ToggleRVO();
	}
}

void ASkelotExampleCollisionAvoidance::UpdateInstanceMovement(float DeltaTime)
{
	if (!SkelotWorld || InstanceDataArray.Num() == 0)
	{
		return;
	}

	ArrivedCount = 0;
	const float ArrivalThreshold = 50.0f;

	for (FInstanceData& Data : InstanceDataArray)
	{
		if (!SkelotWorld->IsHandleValid(Data.Handle))
		{
			continue;
		}

		FVector CurrentLoc = SkelotWorld->GetInstanceLocation(Data.Handle.InstanceIndex);
		FVector ToTarget = Data.CurrentTarget - CurrentLoc;
		ToTarget.Z = 0; // 保持在地面上
		float DistToTarget = ToTarget.Size();

		if (Data.bArrived)
		{
			ArrivedCount++;

			// 处理到达后的行为 (1 = WaitAndReverse)
			if (ArrivalBehavior == 1)
			{
				Data.WaitTimer += DeltaTime;
				if (Data.WaitTimer >= WaitDuration)
				{
					// 交换起点和目标
					FVector Temp = Data.StartLocation;
					Data.StartLocation = Data.CurrentTarget;
					Data.CurrentTarget = Temp;
					Data.bArrived = false;
					Data.WaitTimer = 0.0f;
				}
			}
			else if (ArrivalBehavior == 2) // Loop
			{
				// 回到起点
				Data.CurrentTarget = Data.StartLocation;
				Data.StartLocation = CurrentLoc;
				Data.bArrived = false;
			}

			// 停止移动
			SkelotWorld->SetInstanceVelocity(Data.Handle.InstanceIndex, FVector3f::ZeroVector);

			// 播放待机动画
			if (IdleAnimation && !Data.bIsIdleAnimationPlaying)
			{
				FSkelotAnimPlayParams Params;
				Params.Animation = IdleAnimation;
				Params.bLoop = true;
				Params.PlayScale = 1.0f;
				SkelotWorld->InstancePlayAnimation(Data.Handle.InstanceIndex, Params);
				Data.bIsIdleAnimationPlaying = true;
				Data.bIsWalkAnimationPlaying = false;
			}
		}
		else
		{
			// 检查是否到达
			if (DistToTarget < ArrivalThreshold)
			{
				Data.bArrived = true;
				Data.WaitTimer = 0.0f;
			}
			else
			{
				// 计算移动方向和速度
				FVector Direction = ToTarget.GetSafeNormal();
				const FVector3f DesiredVelocity = FVector3f(Direction * MoveSpeed);
				const FVector3f CurrentVelocity = SkelotWorld->GetInstanceVelocity(Data.Handle);

				SkelotWorld->SetInstanceVelocity(Data.Handle.InstanceIndex, DesiredVelocity);

				// 未启用避障时按目标速度前进；启用后使用上一帧已调整的速度，避免覆盖避障结果
				FVector AppliedVelocity = FVector(DesiredVelocity);
				if (bEnablePBD || bEnableRVO)
				{
					AppliedVelocity = FVector(CurrentVelocity);
					if (AppliedVelocity.IsNearlyZero())
					{
						AppliedVelocity = FVector(DesiredVelocity);
					}
				}

				FVector NewLoc = CurrentLoc + AppliedVelocity * DeltaTime;
				SkelotWorld->SetInstanceLocation(Data.Handle.InstanceIndex, NewLoc);

				// 朝向目标
				const FVector FacingDirection = AppliedVelocity.IsNearlyZero() ? Direction : AppliedVelocity.GetSafeNormal();
				FQuat TargetRotation = FRotationMatrix::MakeFromX(FacingDirection).Rotator().Quaternion();
				SkelotWorld->SetInstanceRotation(Data.Handle.InstanceIndex, FQuat4f(TargetRotation));

				// 播放行走动画
				if (WalkAnimation && !Data.bIsWalkAnimationPlaying)
				{
					FSkelotAnimPlayParams Params;
					Params.Animation = WalkAnimation;
					Params.bLoop = true;
					Params.PlayScale = 1.0f;
					SkelotWorld->InstancePlayAnimation(Data.Handle.InstanceIndex, Params);
					Data.bIsWalkAnimationPlaying = true;
					Data.bIsIdleAnimationPlaying = false;
				}
			}
		}
	}
}

void ASkelotExampleCollisionAvoidance::ApplyConfigs()
{
	if (!SkelotWorld)
	{
		return;
	}

	// 应用 PBD 配置
	FSkelotPBDConfig PBDConfig = SkelotWorld->GetPBDConfig();
	PBDConfig.bEnablePBD = bEnablePBD;
	PBDConfig.CollisionRadius = PBDCollisionRadius;
	SkelotWorld->SetPBDConfig(PBDConfig);

	// 应用 RVO 配置
	FSkelotRVOConfig RVOConfig = SkelotWorld->GetRVOConfig();
	RVOConfig.bEnableRVO = bEnableRVO;
	RVOConfig.NeighborRadius = RVONeighborRadius;
	SkelotWorld->SetRVOConfig(RVOConfig);
}


//////////////////////////////////////////////////////////////////////////
// ASkelotExampleGeometryTools
//////////////////////////////////////////////////////////////////////////

ASkelotExampleGeometryTools::ASkelotExampleGeometryTools()
{
	PrimaryActorTick.bCanEverTick = true;

	// 默认贝塞尔曲线控制点
	BezierControlPoints.Add(FVector(0, 0, 0));
	BezierControlPoints.Add(FVector(500, 500, 0));
	BezierControlPoints.Add(FVector(1000, 0, 0));
}

void ASkelotExampleGeometryTools::BeginPlay()
{
	Super::BeginPlay();

	SpawnInstancesWithCurrentPattern();
}

void ASkelotExampleGeometryTools::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearInstances();
	Super::EndPlay(EndPlayReason);
}

void ASkelotExampleGeometryTools::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	HandleKeyboardInput();
}

void ASkelotExampleGeometryTools::SpawnInstancesWithCurrentPattern()
{
	ClearInstances();

	switch (SpawnPattern)
	{
	case 0: // Round
		SpawnWithRoundPattern();
		break;
	case 1: // Grid2D
	case 2: // Grid3D
		SpawnWithGridPattern();
		break;
	case 3: // Bezier
		SpawnWithBezierPattern();
		break;
	}
}

void ASkelotExampleGeometryTools::ClearInstances()
{
	if (InstanceHandles.Num() > 0)
	{
		USkelotWorldSubsystem::Skelot_DestroyInstances(this, InstanceHandles);
		InstanceHandles.Empty();
	}
}

void ASkelotExampleGeometryTools::NextPattern()
{
	int32 NextIndex = (static_cast<int32>(SpawnPattern) + 1) % 4;
	SpawnPattern = NextIndex;
	SpawnInstancesWithCurrentPattern();
}

FString ASkelotExampleGeometryTools::GetCurrentPatternName() const
{
	switch (SpawnPattern)
	{
	case 0:
		return TEXT("圆形点阵");
	case 1:
		return TEXT("2D网格");
	case 2:
		return TEXT("3D网格");
	case 3:
		return TEXT("贝塞尔曲线");
	default:
		return TEXT("未知");
	}
}

void ASkelotExampleGeometryTools::HandleKeyboardInput()
{
	APlayerController* PC = GetExamplePlayerController(this);
	if (!PC)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(TogglePatternKey))
	{
		NextPattern();
	}

	if (PC->WasInputKeyJustPressed(ClearKey))
	{
		ClearInstances();
	}
}

void ASkelotExampleGeometryTools::SpawnWithRoundPattern()
{
	if (!RenderParams)
	{
		return;
	}

	ASkelotWorld* World = USkelotWorldSubsystem::GetSingleton(this);
	if (!World)
	{
		return;
	}

	// 使用几何工具生成圆形点阵
	TArray<FVector> Points = USkelotGeometryTools::GetPointsByRound(
		GetActorLocation(),
		RoundRadius,
		RoundSpacing,
		0.0f // 无噪声
	);

	if (Points.Num() == 0)
	{
		return;
	}

	// 创建变换数组
	TArray<FTransform> Transforms;
	Transforms.Reserve(Points.Num());
	for (const FVector& Point : Points)
	{
		Transforms.Add(FTransform(Point));
	}

	// 批量创建实例
	World->CreateInstances(Transforms, RenderParams, InstanceHandles);
}

void ASkelotExampleGeometryTools::SpawnWithGridPattern()
{
	if (!RenderParams)
	{
		return;
	}

	ASkelotWorld* World = USkelotWorldSubsystem::GetSingleton(this);
	if (!World)
	{
		return;
	}

	// 确定Z方向数量 (1 = Grid2D)
	int32 CountZ = (SpawnPattern == 1) ? 1 : GridCountZ;

	// 使用几何工具生成网格点阵
	TArray<FVector> Points = USkelotGeometryTools::GetPointsByGrid(
		GetActorLocation(),
		GridSpacingX,
		GridCountX,
		GridCountY,
		GridSpacingY,
		CountZ,
		GridSpacingZ
	);

	if (Points.Num() == 0)
	{
		return;
	}

	// 创建变换数组
	TArray<FTransform> Transforms;
	Transforms.Reserve(Points.Num());
	for (const FVector& Point : Points)
	{
		Transforms.Add(FTransform(Point));
	}

	// 批量创建实例
	World->CreateInstances(Transforms, RenderParams, InstanceHandles);
}

void ASkelotExampleGeometryTools::SpawnWithBezierPattern()
{
	if (!RenderParams || BezierControlPoints.Num() < 2)
	{
		return;
	}

	ASkelotWorld* World = USkelotWorldSubsystem::GetSingleton(this);
	if (!World)
	{
		return;
	}

	// 沿贝塞尔曲线采样点
	TArray<FTransform> Transforms;
	Transforms.Reserve(BezierSampleCount);

	for (int32 i = 0; i < BezierSampleCount; ++i)
	{
		float Progress = (BezierSampleCount > 1) ? (static_cast<float>(i) / static_cast<float>(BezierSampleCount - 1)) : 0.0f;
		FVector Point = USkelotGeometryTools::GetBezierPoint(BezierControlPoints, Progress);

		// 将点转换到世界空间
		Point = GetActorLocation() + Point;
		Transforms.Add(FTransform(Point));
	}

	// 批量创建实例
	World->CreateInstances(Transforms, RenderParams, InstanceHandles);
}


//////////////////////////////////////////////////////////////////////////
// ASkelotExampleStressTest
//////////////////////////////////////////////////////////////////////////

ASkelotExampleStressTest::ASkelotExampleStressTest()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASkelotExampleStressTest::BeginPlay()
{
	Super::BeginPlay();

	SkelotWorld = USkelotWorldSubsystem::GetSingleton(this);
	LastFrameTime = FPlatformTime::Seconds();

	if (bAutoStartOnBeginPlay)
	{
		StartTest();
	}
}

void ASkelotExampleStressTest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearStressTestHUDMessages(this);

	if (bTestRunning)
	{
		bTestRunning = false;
	}
	if (InstanceHandles.Num() > 0)
	{
		USkelotWorldSubsystem::Skelot_DestroyInstances(this, InstanceHandles);
		InstanceHandles.Empty();
	}
	Super::EndPlay(EndPlayReason);
}

void ASkelotExampleStressTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	HandleKeyboardInput();

	if (bTestRunning)
	{
		// 根据测试模式更新
		if (TestMode >= 1) // 移动模式
		{
			UpdateInstanceMovement(DeltaTime);
		}

		UpdateFPSStats(DeltaTime);

		if (bShowHUD)
		{
			DrawHUD();
		}
	}
}

void ASkelotExampleStressTest::StartTest()
{
	if (bTestRunning || !RenderParams)
	{
		return;
	}

	SkelotWorld = USkelotWorldSubsystem::GetSingleton(this);
	if (!SkelotWorld)
	{
		return;
	}

	if (InstanceHandles.Num() > 0)
	{
		USkelotWorldSubsystem::Skelot_DestroyInstances(this, InstanceHandles);
		InstanceHandles.Empty();
	}

	ResetStats();
	const double Now = FPlatformTime::Seconds();
	TestStartTime = Now;
	WarmupStartTime = Now;
	MeasurementStartTime = Now;
	bInWarmup = WarmupSeconds > 0.0f;
	LastFrameTime = Now;
	DirectionChangeTimer = 0.0f;
	TargetDirections.Empty();

	// 应用配置
	ApplyConfigs();

	// 生成实例
	TArray<FTransform> Transforms;
	Transforms.Reserve(InstanceCount);

	// 使用网格布局生成，更均匀
	int32 GridSize = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(InstanceCount)));
	float GridSpacing = SpawnRadius * 2.0f / GridSize;
	FRandomStream SpawnRandom(RandomSeed);

	for (int32 i = 0; i < InstanceCount; ++i)
	{
		int32 X = i % GridSize;
		int32 Y = i / GridSize;

		FVector Location(
			GetActorLocation().X + (X - GridSize / 2) * GridSpacing,
			GetActorLocation().Y + (Y - GridSize / 2) * GridSpacing,
			GetActorLocation().Z
		);

		// 添加小随机偏移
		Location += FVector(SpawnRandom.FRandRange(-10.0f, 10.0f), SpawnRandom.FRandRange(-10.0f, 10.0f), 0.0f);

		Transforms.Add(FTransform(Location));
	}

	// 批量创建
	SkelotWorld->CreateInstances(Transforms, RenderParams, InstanceHandles);

	// 播放动画
	if (WalkAnimation)
	{
		FSkelotAnimPlayParams Params;
		Params.Animation = WalkAnimation;
		Params.bLoop = true;
		Params.PlayScale = 1.0f;

		for (const FSkelotInstanceHandle& Handle : InstanceHandles)
		{
			SkelotWorld->InstancePlayAnimation(Handle.InstanceIndex, Params);
		}
	}

	bTestRunning = true;
	UE_LOG(LogTemp, Log, TEXT("Skelot Stress Test Started: Instances=%d, Mode=%d, Warmup=%.1fs, Measure=%.1fs"),
		InstanceCount, TestMode, WarmupSeconds, MeasurementSeconds);
}

void ASkelotExampleStressTest::StopTest()
{
	if (!bTestRunning)
	{
		return;
	}

	ClearStressTestHUDMessages(this);

	// 销毁所有实例
	USkelotWorldSubsystem::Skelot_DestroyInstances(this, InstanceHandles);
	InstanceHandles.Empty();
	TargetDirections.Empty();

	bTestRunning = false;
	bInWarmup = false;

	double TestDuration = FPlatformTime::Seconds() - TestStartTime;
	UE_LOG(LogTemp, Log, TEXT("Skelot Stress Test Stopped: Duration=%.2fs, AvgFPS=%.2f, MinFPS=%.2f, MaxFPS=%.2f, AvgFrame=%.2fms"),
		TestDuration, AverageFPS, MinFPS == FLT_MAX ? 0.0f : MinFPS, MaxFPS, AverageFrameTimeMs);
}

void ASkelotExampleStressTest::CycleTestMode()
{
	TestMode = (TestMode + 1) % 4;

	if (bTestRunning)
	{
		ApplyConfigs();
	}

	UE_LOG(LogTemp, Log, TEXT("Test Mode Changed to: %s"), *GetTestModeName());
}

FString ASkelotExampleStressTest::GetTestModeName() const
{
	switch (TestMode)
	{
	case 0:
		return TEXT("静态渲染");
	case 1:
		return TEXT("移动");
	case 2:
		return TEXT("移动 + PBD碰撞");
	case 3:
		return TEXT("移动 + PBD + RVO避障");
	default:
		return TEXT("未知");
	}
}

void ASkelotExampleStressTest::HandleKeyboardInput()
{
	APlayerController* PC = GetExamplePlayerController(this);
	if (!PC)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(StartTestKey))
	{
		StartTest();
	}

	if (PC->WasInputKeyJustPressed(StopTestKey))
	{
		StopTest();
	}

	if (PC->WasInputKeyJustPressed(ToggleModeKey))
	{
		CycleTestMode();
	}

	if (PC->WasInputKeyJustPressed(ToggleHUDKey))
	{
		bShowHUD = !bShowHUD;
		if (!bShowHUD)
		{
			ClearStressTestHUDMessages(this);
		}
	}
}

void ASkelotExampleStressTest::UpdateFPSStats(float DeltaTime)
{
	(void)DeltaTime;

	// 计算当前 FPS
	double CurrentTime = FPlatformTime::Seconds();
	float ActualDeltaTime = static_cast<float>(CurrentTime - LastFrameTime);
	LastFrameTime = CurrentTime;

	if (ActualDeltaTime > 0.0f)
	{
		CurrentFPS = 1.0f / ActualDeltaTime;
		FrameTimeMs = ActualDeltaTime * 1000.0f;
	}
	else
	{
		CurrentFPS = 0.0f;
		FrameTimeMs = 0.0f;
	}

	// 预热阶段不计入统计
	if (bInWarmup)
	{
		if ((CurrentTime - WarmupStartTime) >= WarmupSeconds)
		{
			bInWarmup = false;
			MeasurementStartTime = CurrentTime;
			ResetStats();
		}
		return;
	}

	// 更新统计
	FPSAccumulator += CurrentFPS;
	FPSFrameCount++;
	FrameCount++;

	if (FPSFrameCount > 0)
	{
		AverageFPS = FPSAccumulator / FPSFrameCount;
		AverageFrameTimeMs = (AverageFPS > 0.0f) ? (1000.0f / AverageFPS) : 0.0f;
	}

	if (CurrentFPS > 0.0f)
	{
		MinFPS = FMath::Min(MinFPS, CurrentFPS);
		MaxFPS = FMath::Max(MaxFPS, CurrentFPS);
	}

	if (MeasurementSeconds > 0.0f && (CurrentTime - MeasurementStartTime) >= MeasurementSeconds)
	{
		StopTest();
	}
}

void ASkelotExampleStressTest::DrawHUD()
{
#if ENABLE_DRAW_DEBUG
	if (!GEngine)
	{
		return;
	}

	// 构建 HUD 文本
	TArray<FString> Lines;

	// 标题
	Lines.Add(TEXT("=== Skelot 性能测试 ==="));
	Lines.Add(FString::Printf(TEXT("测试模式: %s"), *GetTestModeName()));
	Lines.Add(FString::Printf(TEXT("阶段: %s"), bInWarmup ? TEXT("预热中") : TEXT("正式采样")));
	Lines.Add(FString::Printf(TEXT("实例数量: %d"), InstanceHandles.Num()));
	Lines.Add(TEXT(""));

	// FPS 统计
	Lines.Add(FString::Printf(TEXT("当前 FPS: %.1f"), CurrentFPS));
	Lines.Add(FString::Printf(TEXT("平均 FPS: %.1f"), AverageFPS));
	Lines.Add(FString::Printf(TEXT("最低 FPS: %.1f"), MinFPS == FLT_MAX ? 0.0f : MinFPS));
	Lines.Add(FString::Printf(TEXT("最高 FPS: %.1f"), MaxFPS));
	Lines.Add(TEXT(""));

	// 帧时间
	Lines.Add(FString::Printf(TEXT("帧时间: %.2f ms"), FrameTimeMs));
	Lines.Add(FString::Printf(TEXT("平均帧时间: %.2f ms"), AverageFrameTimeMs));
	Lines.Add(TEXT(""));

	// 测试时长
	double TestDuration = FPlatformTime::Seconds() - TestStartTime;
	double MeasureDuration = bInWarmup ? 0.0 : (FPlatformTime::Seconds() - MeasurementStartTime);
	Lines.Add(FString::Printf(TEXT("测试时长: %.1f 秒"), TestDuration));
	Lines.Add(FString::Printf(TEXT("采样时长: %.1f / %.1f 秒"), MeasureDuration, MeasurementSeconds));
	Lines.Add(FString::Printf(TEXT("总帧数: %lld"), FrameCount));

	// 操作提示
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("--- 操作 ---"));
	Lines.Add(TEXT("Enter: 开始测试"));
	Lines.Add(TEXT("Esc: 停止测试"));
	Lines.Add(TEXT("M: 切换模式"));
	Lines.Add(TEXT("H: 切换HUD"));

	// 绘制屏幕 HUD 文本
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		GEngine->AddOnScreenDebugMessage(
			GetStressTestHUDMessageKey(this, i),
			0.1f,
			FColor::White,
			Lines[i],
			false,
			FVector2D(HUDTextSize, HUDTextSize));
	}

	for (int32 i = Lines.Num(); i < 32; ++i)
	{
		GEngine->RemoveOnScreenDebugMessage(GetStressTestHUDMessageKey(this, i));
	}
#endif
}

void ASkelotExampleStressTest::ApplyConfigs()
{
	if (!SkelotWorld)
	{
		return;
	}

	// PBD 配置
	FSkelotPBDConfig PBDConfig = SkelotWorld->GetPBDConfig();
	PBDConfig.bEnablePBD = (TestMode >= 2);
	PBDConfig.CollisionRadius = PBDCollisionRadius;
	PBDConfig.IterationCount = PBDIterationCount;
	PBDConfig.MaxNeighbors = PBDMaxNeighbors;
	SkelotWorld->SetPBDConfig(PBDConfig);

	// RVO 配置
	FSkelotRVOConfig RVOConfig = SkelotWorld->GetRVOConfig();
	RVOConfig.bEnableRVO = (TestMode >= 3);
	RVOConfig.NeighborRadius = RVONeighborRadius;
	RVOConfig.MaxNeighbors = RVOMaxNeighbors;
	RVOConfig.FrameStride = RVOFrameStride;
	SkelotWorld->SetRVOConfig(RVOConfig);

	SkelotWorld->SetLODUpdateFrequencyEnabled(bEnableLODUpdateFrequency);
	SkelotWorld->SetLODDistances(LODMediumDistance, LODFarDistance);
}

void ASkelotExampleStressTest::UpdateInstanceMovement(float DeltaTime)
{
	if (!SkelotWorld || InstanceHandles.Num() == 0)
	{
		return;
	}

	DirectionChangeTimer += DeltaTime;

	if (DirectionChangeTimer > DirectionChangeInterval || TargetDirections.Num() != InstanceHandles.Num())
	{
		RegenerateTargetDirections();
		DirectionChangeTimer = 0.0f;
	}

	TArray<int32> Indices;
	TArray<FVector3f> Velocities;
	Indices.Reserve(InstanceHandles.Num());
	Velocities.Reserve(InstanceHandles.Num());

	for (int32 i = 0; i < InstanceHandles.Num(); ++i)
	{
		if (!SkelotWorld->IsHandleValid(InstanceHandles[i]))
		{
			continue;
		}

		// 获取当前位置
		FVector CurrentLoc = SkelotWorld->GetInstanceLocation(InstanceHandles[i].InstanceIndex);

		// 检查是否超出范围
		FVector ToCenter = GetActorLocation() - CurrentLoc;
		ToCenter.Z = 0;
		float DistToCenter = ToCenter.Size();

		FVector MoveDir;

		if (DistToCenter > SpawnRadius * 0.9f)
		{
			// 超出范围，向中心移动
			MoveDir = ToCenter.GetSafeNormal();
		}
		else
		{
			// 正常移动
			MoveDir = TargetDirections[i];
		}

		// 设置速度
		FVector3f Velocity = FVector3f(MoveDir * MoveSpeed);
		Indices.Add(InstanceHandles[i].InstanceIndex);
		Velocities.Add(Velocity);

		// 更新位置
		FVector NewLoc = CurrentLoc + MoveDir * MoveSpeed * DeltaTime;
		SkelotWorld->SetInstanceLocation(InstanceHandles[i].InstanceIndex, NewLoc);

		// 朝向移动方向
		if (!MoveDir.IsNearlyZero())
		{
			FQuat TargetRotation = FRotationMatrix::MakeFromX(MoveDir).Rotator().Quaternion();
			SkelotWorld->SetInstanceRotation(InstanceHandles[i].InstanceIndex, FQuat4f(TargetRotation));
		}
	}

	if (Indices.Num() > 0 && Velocities.Num() == Indices.Num())
	{
		SkelotWorld->SetInstanceVelocities(Indices, Velocities);
	}
}

void ASkelotExampleStressTest::ResetStats()
{
	CurrentFPS = 0.0f;
	AverageFPS = 0.0f;
	MinFPS = FLT_MAX;
	MaxFPS = 0.0f;
	FPSAccumulator = 0.0f;
	FPSFrameCount = 0;
	FrameCount = 0;
	FrameTimeMs = 0.0f;
	AverageFrameTimeMs = 0.0f;
}

void ASkelotExampleStressTest::RegenerateTargetDirections()
{
	TargetDirections.Reset();
	TargetDirections.Reserve(InstanceHandles.Num());

	const int32 SeedOffset = static_cast<int32>(FrameCount % 1000000);
	FRandomStream RandomStream(RandomSeed + SeedOffset);

	for (int32 i = 0; i < InstanceHandles.Num(); ++i)
	{
		FVector Dir = RandomStream.VRand();
		Dir.Z = 0.0f;
		if (!Dir.Normalize())
		{
			Dir = FVector::ForwardVector;
		}
		TargetDirections.Add(Dir);
	}
}
