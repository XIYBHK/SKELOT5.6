// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotObstacle.h"
#include "SkelotWorld.h"
#include "DrawDebugHelpers.h"

ASkelotObstacle::ASkelotObstacle()
{
	// 启用 Tick 以支持调试可视化
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 默认值
	ObstacleType = ESkelotObstacleType::Sphere;
	bEnabled = true;
	CollisionMask = 0xFF;
	RadiusOffset = 0.0f;
	bShowDebug = false;
}

float ASkelotObstacle::GetObstacleRadius_Implementation() const
{
	return 100.0f + RadiusOffset;
}

bool ASkelotObstacle::IsPointInside_Implementation(const FVector& Point) const
{
	return false;
}

float ASkelotObstacle::GetDistanceToPoint_Implementation(const FVector& Point, FVector& OutClosestPoint) const
{
	OutClosestPoint = Point;
	return 0.0f;
}

bool ASkelotObstacle::ComputeCollisionResponse_Implementation(const FVector& InstanceLocation, float InstanceRadius,
															  FVector& OutPushDirection, float& OutPushMagnitude) const
{
	OutPushDirection = FVector::ZeroVector;
	OutPushMagnitude = 0.0f;
	return false;
}

void ASkelotObstacle::BeginPlay()
{
	Super::BeginPlay();

	// 缓存初始 Transform 用于运行时移动检测
	CachedTransform = GetActorTransform();

	// 启用 Tick 以检测运行时移动
	SetActorTickEnabled(true);

	// 注册到 SkelotWorld
	ASkelotWorld* SkelotWorld = ASkelotWorld::Get(this, false);
	if (SkelotWorld)
	{
		SkelotWorld->RegisterObstacle(this);
	}
}

void ASkelotObstacle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 从 SkelotWorld 注销
	ASkelotWorld* SkelotWorld = ASkelotWorld::Get(this, false);
	if (SkelotWorld)
	{
		SkelotWorld->UnregisterObstacle(this);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void ASkelotObstacle::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 属性变更时通知 SkelotWorld 更新障碍物缓存
	if (PropertyChangedEvent.Property != nullptr)
	{
		ASkelotWorld* SkelotWorld = ASkelotWorld::Get(this, false);
		if (SkelotWorld)
		{
			SkelotWorld->MarkObstaclesDirty();
		}
	}
}

void ASkelotObstacle::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// 移动后通知 SkelotWorld 更新障碍物缓存
	ASkelotWorld* SkelotWorld = ASkelotWorld::Get(this, false);
	if (SkelotWorld)
	{
		SkelotWorld->MarkObstaclesDirty();
	}
}
#endif

void ASkelotObstacle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 检测运行时位置/旋转变化，标记障碍物缓存脏
	const FTransform CurrentTransform = GetActorTransform();
	if (!CurrentTransform.Equals(CachedTransform, KINDA_SMALL_NUMBER))
	{
		CachedTransform = CurrentTransform;
		if (ASkelotWorld* SkelotWorld = ASkelotWorld::Get(this, false))
		{
			SkelotWorld->MarkObstaclesDirty();
		}
	}

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
	// 调试可视化
	if (bShowDebug)
	{
		FColor Color = GetDebugColor();
		float Radius = GetObstacleRadius();

		// 绘制球形范围指示
		DrawDebugSphere(GetWorld(), GetActorLocation(), Radius, 16, Color, false, DeltaTime, 0, 1.0f);
	}
#endif
}

FColor ASkelotObstacle::GetDebugColor() const
{
	return bEnabled ? FColor(255, 165, 0) : FColor(128, 128, 128); // 橙色或灰色
}

///////////////////////////////////////////////////////////////////////////////
// ASkelotSphereObstacle
///////////////////////////////////////////////////////////////////////////////

ASkelotSphereObstacle::ASkelotSphereObstacle()
{
	ObstacleType = ESkelotObstacleType::Sphere;
	Radius = 100.0f;
}

float ASkelotSphereObstacle::GetObstacleRadius_Implementation() const
{
	return Radius + RadiusOffset;
}

bool ASkelotSphereObstacle::IsPointInside_Implementation(const FVector& Point) const
{
	FVector Center = GetActorLocation();
	float EffectiveRadius = Radius + RadiusOffset;
	float DistSq = FVector::DistSquared(Point, Center);
	return DistSq < EffectiveRadius * EffectiveRadius;
}

float ASkelotSphereObstacle::GetDistanceToPoint_Implementation(const FVector& Point, FVector& OutClosestPoint) const
{
	FVector Center = GetActorLocation();
	float EffectiveRadius = Radius + RadiusOffset;

	FVector ToPoint = Point - Center;
	float Dist = ToPoint.Size();

	if (Dist < KINDA_SMALL_NUMBER)
	{
		// 点在中心，返回负半径（表示在内部）
		OutClosestPoint = Center + FVector(EffectiveRadius, 0, 0);
		return -EffectiveRadius;
	}

	FVector Direction = ToPoint / Dist;
	OutClosestPoint = Center + Direction * EffectiveRadius;

	return Dist - EffectiveRadius;
}

bool ASkelotSphereObstacle::ComputeCollisionResponse_Implementation(const FVector& InstanceLocation, float InstanceRadius,
																	 FVector& OutPushDirection, float& OutPushMagnitude) const
{
	FVector Center = GetActorLocation();
	float EffectiveRadius = Radius + RadiusOffset;
	float MinDist = EffectiveRadius + InstanceRadius;

	FVector Delta = InstanceLocation - Center;
	float DistSq = Delta.SquaredLength();

	// 检查是否碰撞
	if (DistSq >= MinDist * MinDist)
	{
		return false;
	}

	float Dist = FMath::Sqrt(DistSq);

	if (Dist < KINDA_SMALL_NUMBER)
	{
		// 完全重合，使用随机方向推开
		OutPushDirection = FVector(1.0f, 0.0f, 0.0f);
		OutPushMagnitude = MinDist;
		return true;
	}

	// 计算穿透深度和推力
	OutPushDirection = Delta / Dist;
	OutPushMagnitude = MinDist - Dist;

	return true;
}

void ASkelotSphereObstacle::BeginPlay()
{
	Super::BeginPlay();
}

void ASkelotSphereObstacle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
	if (bShowDebug)
	{
		FColor Color = GetDebugColor();

		// 绘制球形障碍物
		DrawDebugSphere(GetWorld(), GetActorLocation(), Radius, 24, Color, false, DeltaTime, 0, 2.0f);

		// 绘制有效碰撞范围（包含半径偏移）
		if (RadiusOffset > 0.0f)
		{
			DrawDebugSphere(GetWorld(), GetActorLocation(), Radius + RadiusOffset, 24, FColor::Yellow, false, DeltaTime, 0, 0.5f);
		}
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
// ASkelotBoxObstacle
///////////////////////////////////////////////////////////////////////////////

ASkelotBoxObstacle::ASkelotBoxObstacle()
{
	ObstacleType = ESkelotObstacleType::Box;
	BoxExtent = FVector(100.0f, 100.0f, 200.0f);
}

float ASkelotBoxObstacle::GetObstacleRadius_Implementation() const
{
	// 返回盒子外接球的半径
	return BoxExtent.Size() + RadiusOffset;
}

FVector ASkelotBoxObstacle::WorldToLocal(const FVector& WorldPoint) const
{
	return GetActorTransform().InverseTransformPosition(WorldPoint);
}

FVector ASkelotBoxObstacle::LocalToWorld(const FVector& LocalPoint) const
{
	return GetActorTransform().TransformPosition(LocalPoint);
}

FVector ASkelotBoxObstacle::ClosestPointOnAABB(const FVector& Point, const FVector& BoxMin, const FVector& BoxMax)
{
	return FVector(
		FMath::Clamp(Point.X, BoxMin.X, BoxMax.X),
		FMath::Clamp(Point.Y, BoxMin.Y, BoxMax.Y),
		FMath::Clamp(Point.Z, BoxMin.Z, BoxMax.Z)
	);
}

bool ASkelotBoxObstacle::IsPointInside_Implementation(const FVector& Point) const
{
	FVector LocalPoint = WorldToLocal(Point);
	FVector EffectiveExtent = BoxExtent + FVector(RadiusOffset);

	return FMath::Abs(LocalPoint.X) < EffectiveExtent.X &&
		   FMath::Abs(LocalPoint.Y) < EffectiveExtent.Y &&
		   FMath::Abs(LocalPoint.Z) < EffectiveExtent.Z;
}

float ASkelotBoxObstacle::GetDistanceToPoint_Implementation(const FVector& Point, FVector& OutClosestPoint) const
{
	FVector LocalPoint = WorldToLocal(Point);
	FVector EffectiveExtent = BoxExtent + FVector(RadiusOffset);

	// 计算本地空间中最近的表面点
	FVector LocalClosest = ClosestPointOnAABB(LocalPoint, -EffectiveExtent, EffectiveExtent);
	OutClosestPoint = LocalToWorld(LocalClosest);

	FVector Delta = LocalPoint - LocalClosest;
	float Dist = Delta.Size();

	// 点在盒子内部时返回负距离
	if (Dist < KINDA_SMALL_NUMBER && IsPointInside(Point))
	{
		// 计算到最近表面的距离（各轴取最小穿透深度）
		float MinPenetration = FLT_MAX;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			float PenPos = EffectiveExtent[Axis] - LocalPoint[Axis];
			float PenNeg = EffectiveExtent[Axis] + LocalPoint[Axis];
			MinPenetration = FMath::Min(MinPenetration, FMath::Min(PenPos, PenNeg));
		}
		return -MinPenetration;
	}

	return Dist;
}

bool ASkelotBoxObstacle::ComputeCollisionResponse_Implementation(const FVector& InstanceLocation, float InstanceRadius,
																  FVector& OutPushDirection, float& OutPushMagnitude) const
{
	// 将实例位置变换到障碍物本地空间
	FVector LocalPoint = WorldToLocal(InstanceLocation);
	FVector EffectiveExtent = BoxExtent + FVector(RadiusOffset);

	// 扩展盒子以包含实例半径
	FVector ExpandedExtent = EffectiveExtent + FVector(InstanceRadius);

	// 检查点是否在扩展盒子内
	if (FMath::Abs(LocalPoint.X) >= ExpandedExtent.X ||
		FMath::Abs(LocalPoint.Y) >= ExpandedExtent.Y ||
		FMath::Abs(LocalPoint.Z) >= ExpandedExtent.Z)
	{
		// 不在碰撞范围内
		return false;
	}

	// 计算到各面的距离
	float DX = ExpandedExtent.X - FMath::Abs(LocalPoint.X);
	float DY = ExpandedExtent.Y - FMath::Abs(LocalPoint.Y);
	float DZ = ExpandedExtent.Z - FMath::Abs(LocalPoint.Z);

	// 找到最小的穿透轴
	FVector LocalPushDir(0, 0, 0);
	float Penetration = 0.0f;

	if (DX <= DY && DX <= DZ)
	{
		// X 轴最小
		Penetration = DX;
		LocalPushDir = FVector(LocalPoint.X >= 0 ? 1.0f : -1.0f, 0, 0);
	}
	else if (DY <= DX && DY <= DZ)
	{
		// Y 轴最小
		Penetration = DY;
		LocalPushDir = FVector(0, LocalPoint.Y >= 0 ? 1.0f : -1.0f, 0);
	}
	else
	{
		// Z 轴最小
		Penetration = DZ;
		LocalPushDir = FVector(0, 0, LocalPoint.Z >= 0 ? 1.0f : -1.0f);
	}

	// 将推力方向变换到世界空间
	OutPushDirection = GetActorRotation().RotateVector(LocalPushDir);
	OutPushMagnitude = Penetration;

	return true;
}

void ASkelotBoxObstacle::BeginPlay()
{
	Super::BeginPlay();
}

void ASkelotBoxObstacle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
	if (bShowDebug)
	{
		FColor Color = GetDebugColor();

		// 绘制盒形障碍物
		DrawDebugBox(GetWorld(), GetActorLocation(), BoxExtent, GetActorQuat(), Color, false, DeltaTime, 0, 2.0f);

		// 绘制有效碰撞范围（包含半径偏移）
		if (RadiusOffset > 0.0f)
		{
			DrawDebugBox(GetWorld(), GetActorLocation(), BoxExtent + FVector(RadiusOffset), GetActorQuat(), FColor::Yellow, false, DeltaTime, 0, 0.5f);
		}
	}
#endif
}
