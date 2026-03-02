// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotExampleActors.h"
#include "SkelotWorld.h"
#include "SkelotGeometryTools.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"

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
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
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

	// 清除现有实例
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
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
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
			if (IdleAnimation && Data.WaitTimer == 0.0f)
			{
				FSkelotAnimPlayParams Params;
				Params.Animation = IdleAnimation;
				Params.bLoop = true;
				Params.PlayScale = 1.0f;
				SkelotWorld->InstancePlayAnimation(Data.Handle.InstanceIndex, Params);
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
				FVector3f Velocity = FVector3f(Direction * MoveSpeed);

				SkelotWorld->SetInstanceVelocity(Data.Handle.InstanceIndex, Velocity);

				// 更新位置（由 PBD/RVO 系统处理碰撞后的实际移动）
				// 这里我们手动更新位置用于演示
				FVector NewLoc = CurrentLoc + Direction * MoveSpeed * DeltaTime;
				SkelotWorld->SetInstanceLocation(Data.Handle.InstanceIndex, NewLoc);

				// 朝向目标
				FQuat TargetRotation = FRotationMatrix::MakeFromX(Direction).Rotator().Quaternion();
				SkelotWorld->SetInstanceRotation(Data.Handle.InstanceIndex, FQuat4f(TargetRotation));

				// 播放行走动画
				if (WalkAnimation)
				{
					FSkelotAnimPlayParams Params;
					Params.Animation = WalkAnimation;
					Params.bLoop = true;
					Params.PlayScale = 1.0f;
					SkelotWorld->InstancePlayAnimation(Data.Handle.InstanceIndex, Params);
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
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
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
		float Progress = static_cast<float>(i) / static_cast<float>(BezierSampleCount - 1);
		FVector Point = USkelotGeometryTools::GetBezierPoint(BezierControlPoints, Progress);

		// 将点转换到世界空间
		Point = GetActorLocation() + Point;
		Transforms.Add(FTransform(Point));
	}

	// 批量创建实例
	World->CreateInstances(Transforms, RenderParams, InstanceHandles);
}
