// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotRVOSystem.h"
#include "SkelotSpatialGrid.h"
#include "SkelotPBDPlane.h"
#include "SkelotWorld.h"

// 数学常数
static constexpr float RVO_EPSILON = 0.00001f;
static constexpr float RVO_SQUARE_ROOT_EPSILON = 0.00001f;

FSkelotRVOSystem::FSkelotRVOSystem()
	: ProcessedAgents(0)
	, TotalVelocityAdjustments(1)
	, FrameCounter(0)
	, CurrentCollisionRadius(60.0f)
{
	NeighborIndices.Reserve(64);
	ORCAPlanes.Reserve(32);
}

//////////////////////////////////////////////////////////////////////////
// Public API
//////////////////////////////////////////////////////////////////////////

void FSkelotRVOSystem::SetConfig(const FSkelotRVOConfig& InConfig)
{
	Config = InConfig;
}

const FSkelotRVOConfig& FSkelotRVOSystem::GetConfig() const
{
	return Config;
}

void FSkelotRVOSystem::SetAntiJitterConfig(const FSkelotAntiJitterConfig& InConfig)
{
	AntiJitterConfig = InConfig;
}

const FSkelotAntiJitterConfig& FSkelotRVOSystem::GetAntiJitterConfig() const
{
	return AntiJitterConfig;
}

void FSkelotRVOSystem::ResetStats()
{
	ProcessedAgents = 0;
	TotalVelocityAdjustments = 0;
}

void FSkelotRVOSystem::ResetAgentData()
{
	AgentDataMap.Empty();
}

FRVOAgentData* FSkelotRVOSystem::GetAgentData(int32 InstanceIndex)
{
	return AgentDataMap.Find(InstanceIndex);
}

const FRVOAgentData* FSkelotRVOSystem::GetAgentData(int32 InstanceIndex) const
{
	return AgentDataMap.Find(InstanceIndex);
}

FRVOAgentData& FSkelotRVOSystem::GetOrCreateAgentData(int32 InstanceIndex)
{
	return AgentDataMap.FindOrAdd(InstanceIndex);
}

//////////////////////////////////////////////////////////////////////////
// Main Algorithm
//////////////////////////////////////////////////////////////////////////

void FSkelotRVOSystem::ComputeAvoidance(FSkelotInstancesSOA& SOA, int32 NumInstances,
											  const FSkelotSpatialGrid& SpatialGrid,
											  float DeltaTime,
											  float CollisionRadius)
{
	if (!Config.bEnableRVO || NumInstances == 0)
	{
		return;
	}

	// 保存碰撞半径
	CurrentCollisionRadius = CollisionRadius;

	// 重置统计
	ResetStats();

	// 分帧更新检查
	FrameCounter = (FrameCounter + 1) % Config.FrameStride;

	// 遍历所有有效实例
	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
	{
		// 跳过已销毁的实例
		if (SOA.Slots[InstanceIndex].bDestroyed)
		{
			continue;
		}

		// 分帧更新：只处理当前帧对应的实例
		if (Config.FrameStride > 1 && (InstanceIndex % Config.FrameStride) != FrameCounter)
		{
			continue;
		}

		// 计算避障
		FVector3f NewVelocity;
		if (ComputeAgentAvoidance(SOA, InstanceIndex, SpatialGrid, DeltaTime, NewVelocity))
		{
			// 更新速度
			SOA.Velocities[InstanceIndex] = NewVelocity;
			ProcessedAgents++;
			TotalVelocityAdjustments++;
		}
	}
}

bool FSkelotRVOSystem::ComputeAgentAvoidance(const FSkelotInstancesSOA& SOA, int32 InstanceIndex,
											   const FSkelotSpatialGrid& SpatialGrid,
											   float DeltaTime,
											   FVector3f& OutNewVelocity)
{
	OutNewVelocity = SOA.Velocities[InstanceIndex];

	// 获取当前位置和速度
	const FVector3d& MyPos3D = SOA.Locations[InstanceIndex];
	const FVector3f& MyVel3D = SOA.Velocities[InstanceIndex];

	// 转换为 3D（XY 平面）
	FVector3f MyPos(MyPos3D.X, MyPos3D.Y, 0.0f);
	FVector3f MyVel(MyVel3D.X, MyVel3D.Y, 0.0f);

	// 获取当前速度大小
	float CurrentSpeed = MyVel.Size();

	// 如果速度为零，不需要避障
	if (CurrentSpeed < RVO_EPSILON)
	{
		return false;
	}

	// 计算期望速度（保持当前方向）
	FVector2f PreferredVelocity(MyVel.X, MyVel.Y);

	// 计算最大速度
	float MaxSpeed = Config.MaxSpeed > 0 ? Config.MaxSpeed : CurrentSpeed;
	MaxSpeed = FMath::Max(MaxSpeed, Config.MinSpeed);

	// 使用空间网格查询邻居
	NeighborIndices.Reset();
	SpatialGrid.QuerySphere(FVector(MyPos3D), Config.NeighborRadius, NeighborIndices);

	// 限制邻居数量
	int32 NumNeighbors = FMath::Min(NeighborIndices.Num(), Config.MaxNeighbors);

	// 获取或创建代理数据
	FRVOAgentData& AgentData = GetOrCreateAgentData(InstanceIndex);
	AgentData.CurrentNeighborCount = NumNeighbors;

	// 构建 ORCA 平面
	ORCAPlanes.Reset();
	float CombinedRadius = CurrentCollisionRadius * 2.0f;

	for (int32 i = 0; i < NumNeighbors; i++)
	{
		int32 NeighborIdx = NeighborIndices[i];

		// 跳过自己
		if (NeighborIdx == InstanceIndex)
		{
			continue;
		}

		// 跳过已销毁的实例
		if (SOA.Slots[NeighborIdx].bDestroyed)
		{
			continue;
		}

		// 检查碰撞过滤
		if (!ShouldAvoid(SOA, InstanceIndex, NeighborIdx))
		{
			continue;
		}

		// 获取邻居数据
		const FVector3d& NeighborPos3D = SOA.Locations[NeighborIdx];
		const FVector3f& NeighborVel3D = SOA.Velocities[NeighborIdx];

		FVector3f NeighborPos(NeighborPos3D.X, NeighborPos3D.Y, 0.0f);
		FVector3f NeighborVel(NeighborVel3D.X, NeighborVel3D.Y, 0.0f);

		// 构建 ORCA 平面
		FORCAPlane Plane;
		ComputeORCAPlane(MyPos, MyVel, NeighborPos, NeighborVel, CombinedRadius, Plane);
		ORCAPlanes.Add(Plane);
	}

	// 使用线性规划求解最优速度
	FVector2f NewVelocity2D;
	bool bFoundSolution = LinearProgram(ORCAPlanes, PreferredVelocity, MaxSpeed, NewVelocity2D);

	if (!bFoundSolution)
	{
		// 无解时，使用期望速度但限制在最大速度内
		NewVelocity2D = PreferredVelocity.GetSafeNormal() * FMath::Min(PreferredVelocity.Size(), MaxSpeed);
	}

	// 转换回 3D
	OutNewVelocity = FVector3f(NewVelocity2D.X, NewVelocity2D.Y, 0.0f);

	// 应用抗抖动处理
	ApplyAntiJitter(InstanceIndex, SOA.Velocities[InstanceIndex], OutNewVelocity, DeltaTime, NumNeighbors);

	// 确保 Z 轴速度为零（2D 运动）
	OutNewVelocity.Z = 0.0f;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// ORCA Plane Computation
//////////////////////////////////////////////////////////////////////////

void FSkelotRVOSystem::ComputeORCAPlane(const FVector3f& PosA, const FVector3f& VelA,
											  const FVector3f& PosB, const FVector3f& VelB,
											  float CombinedRadius,
											  FORCAPlane& OutPlane)
{
	// 计算相对位置和速度
	FVector3f RelativePosition = PosB - PosA;
	FVector3f RelativeVelocity = VelA - VelB;

	// 2D 投影
	FVector2f RelPos2D(RelativePosition.X, RelativePosition.Y);
	FVector2f RelVel2D(RelativeVelocity.X, RelativeVelocity.Y);

	float DistSq = RelPos2D.SquaredLength();
	float CombinedRadiusSq = CombinedRadius * CombinedRadius;

	if (DistSq > CombinedRadiusSq)
	{
		// 没有碰撞
		// 速度障碍的顶点
		FVector2f VelocityObstacle[2];

		// 计算截止圆的切线方向
		float SqrtDistSq = FMath::Sqrt(DistSq);
		float S = CombinedRadius / SqrtDistSq;
		float C = FMath::Sqrt(FMath::Max(0.0f - S * S));

		// 计算从 A 到 B 的方向
		FVector2f Direction = RelPos2D / SqrtDistSq;

		// 计算切线方向（使用垂直向量）
		FVector2f Orthogonal(-Direction.Y, Direction.X);

		// 两个切线点
		VelocityObstacle[0] = Direction * S + Orthogonal * C;
		VelocityObstacle[1] = Direction * S - Orthogonal * C;

		// 计算相对速度在速度障碍上的投影
		float DotProduct = FVector2f::DotProduct(RelVel2D, VelocityObstacle[0]);

		// 选择正确的边界
		FVector2f LeftLegDirection, RightLegDirection;

		if (DotProduct < 0.0f)
		{
			// 投影在左腿上
			float LegLengthSq = VelocityObstacle[0].SquaredLength();
			if (LegLengthSq > RVO_EPSILON)
			{
				LeftLegDirection = VelocityObstacle[0] / FMath::Sqrt(LegLengthSq);
			}
			else
			{
				LeftLegDirection = FVector2f(1.0f, 0.0f);
			}

			// 计算右腿方向（指向 B）
			RightLegDirection = RelPos2D.GetSafeNormal();
		}
		else
		{
			// 投影在右腿上
			float LegLengthSq = VelocityObstacle[1].SquaredLength();
			if (LegLengthSq > RVO_EPSILON)
			{
				RightLegDirection = VelocityObstacle[1] / FMath::Sqrt(LegLengthSq);
			}
			else
			{
				RightLegDirection = FVector2f(1.0f, 0.0f);
			}

			// 计算左腿方向（指向 B）
			LeftLegDirection = RelPos2D.GetSafeNormal();
		}

		// ORCA 平面点：相对速度的一半（互惠避障）
		OutPlane.Point = RelVel2D * 0.5f;

		// ORCA 平面方向：垂直于腿方向
		OutPlane.Direction = FVector2f(-LeftLegDirection.Y, LeftLegDirection.X);
	}
	else
	{
		// 已经碰撞
		// 使用简单的分离方向
		FVector2f Direction;
		float Dist = FMath::Sqrt(DistSq);
		if (Dist > RVO_EPSILON)
		{
			Direction = RelPos2D / Dist;
		}
		else
		{
			// 完全重叠，使用随机方向
			Direction = FVector2f(1.0f, 0.0f);
		}

		// ORCA 平面点：相对速度的一半
		OutPlane.Point = RelVel2D * 0.5f;

		// ORCA 平面方向：垂直于分离方向
		OutPlane.Direction = FVector2f(-Direction.Y, Direction.X);
	}
}

//////////////////////////////////////////////////////////////////////////
// Linear Programming
//////////////////////////////////////////////////////////////////////////

bool FSkelotRVOSystem::LinearProgram(const TArray<FORCAPlane>& Planes,
											  const FVector2f& PreferredVelocity,
											  float MaxSpeed,
											  FVector2f& OutResult)
{
	// 如果没有约束，直接使用期望速度
	if (Planes.Num() == 0)
	{
		OutResult = PreferredVelocity.GetSafeNormal() * FMath::Min(PreferredVelocity.Size(), MaxSpeed);
		return true;
	}

	// 尝试找到满足所有约束的最优解
	FVector2f Result = PreferredVelocity;
	float Distance = 0.0f;

	// 首先尝试 2D 线性规划
	for (int32 i = 0; i < Planes.Num(); i++)
	{
		if (FVector2f::DotProduct(Planes[i].Direction, Planes[i].Point) - FVector2f::DotProduct(Planes[i].Direction, Result) < 0.0f)
		{
			// 违反约束 i， 需要重新计算
			FVector2f TempResult;
			if (LinearProgram1(Planes, i, MaxSpeed, PreferredVelocity, TempResult))
			{
				Result = TempResult;
				Distance = FMath::Sqrt(Result.SquaredLength());
			}
			else
			{
				// 无法满足约束，使用 3D 投影
				LinearProgram3(Planes, i, MaxSpeed, Result);
				OutResult = Result;
				return false;
			}
		}
	}

	OutResult = Result;
	return true;
}

bool FSkelotRVOSystem::LinearProgram1(const TArray<FORCAPlane>& Planes, int32 PlaneNo,
										   float Radius, const FVector2f& PreferredVelocity,
										   FVector2f& OutResult)
{
	const float DotProduct = FVector2f::DotProduct(Planes[PlaneNo].Point, PreferredVelocity);
	const float Discriminant = DotProduct * DotProduct + Radius * Radius - Planes[PlaneNo].Point.SquaredLength();

	if (Discriminant < 0.0f)
	{
		return false;
	}

	const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
	float tLeft = -DotProduct - SqrtDiscriminant;
	float tRight = -DotProduct + SqrtDiscriminant;

	// 检查与其他平面的交集
	for (int32 i = 0; i < PlaneNo; i++)
	{
		const float DotProduct1 = FVector2f::DotProduct(Planes[PlaneNo].Direction, Planes[i].Point);
		const float DotProduct2 = FVector2f::DotProduct(Planes[PlaneNo].Direction, Planes[i].Direction);
		const float Numerator = DotProduct1 - DotProduct2 * tLeft;
		const float Denominator = DotProduct2 * (tRight - tLeft);

		if (Denominator >= 0.0f)
		{
			if (Numerator <= 0.0f)
			{
				return false;
			}
			else
			{
				const float t = Numerator / Denominator;
				if (t > tLeft)
				{
					tLeft = t;
					if (tLeft > tRight)
					{
						return false;
					}
				}
			}
		}
	}

	OutResult = Planes[PlaneNo].Point + Planes[PlaneNo].Direction * tLeft;
	return true;
}

bool FSkelotRVOSystem::LinearProgram2(const TArray<FORCAPlane>& Planes, int32 PlaneNo,
										   float Radius, const FVector2f& PreferredVelocity,
										   FVector2f& OutResult)
{
	// 实现二维约束求解（简化版本）
	// 直接使用 LinearProgram1
	return LinearProgram1(Planes, PlaneNo, Radius, PreferredVelocity, OutResult);
}

void FSkelotRVOSystem::LinearProgram3(const TArray<FORCAPlane>& Planes, int32 PlaneNo,
										   float Radius, FVector2f& InOutResult)
{
	// 当无法找到满足所有约束的解时，投影到最近的可行解
	float Distance = 0.0f;

	for (int32 i = 0; i < PlaneNo; i++)
	{
		float PlaneDistance = FVector2f::DotProduct(Planes[i].Direction, Planes[i].Point);
		float ResultDistance = FVector2f::DotProduct(Planes[i].Direction, InOutResult);

		if (PlaneDistance > ResultDistance)
		{
			// 调整结果以满足约束
			InOutResult += Planes[i].Direction * (PlaneDistance - ResultDistance);
		}
	}

	// 限制在最大速度圆内
	float ResultLength = InOutResult.Size();
	if (ResultLength > Radius)
	{
		InOutResult = InOutResult.GetSafeNormal() * Radius;
	}
}

//////////////////////////////////////////////////////////////////////////
// Anti-Jitter
//////////////////////////////////////////////////////////////////////////

void FSkelotRVOSystem::ApplyAntiJitter(int32 InstanceIndex, const FVector3f& CurrentVelocity,
										 FVector3f& InOutNewVelocity, float DeltaTime, int32 NeighborCount)
{
	FVector3f TargetVelocity = InOutNewVelocity;

	// 1. 密度自适应
	if (AntiJitterConfig.bEnableDensityAdaptation)
	{
		float AdaptationFactor = GetDensityAdaptationFactor(NeighborCount);
		if (AdaptationFactor < 1.0f)
		{
			// 高密度时，减小速度变化
			TargetVelocity = CurrentVelocity + (TargetVelocity - CurrentVelocity) * AdaptationFactor;
		}
	}

	// 2. 速度平滑
	if (AntiJitterConfig.bEnableVelocitySmoothing)
	{
		TargetVelocity = ApplyVelocitySmoothing(InstanceIndex, CurrentVelocity, TargetVelocity, DeltaTime);
	}

	// 3. 抖动检测和抑制
	if (AntiJitterConfig.bEnableJitterDetection)
	{
		float TargetSpeed = TargetVelocity.Size();
		if (TargetSpeed > RVO_EPSILON)
		{
			FVector3f NewDirection = TargetVelocity / TargetSpeed;

			if (DetectJitter(InstanceIndex, NewDirection))
			{
				// 检测到抖动，应用抑制
				TargetVelocity = CurrentVelocity + (TargetVelocity - CurrentVelocity) * (1.0f - AntiJitterConfig.JitterSuppression);
			}

			// 更新代理数据
			FRVOAgentData& AgentData = GetOrCreateAgentData(InstanceIndex);
			AgentData.PreviousDirection = NewDirection;
		}
	}

	InOutNewVelocity = TargetVelocity;
}

FVector3f FSkelotRVOSystem::ApplyVelocitySmoothing(int32 InstanceIndex,
												 const FVector3f& CurrentVelocity,
												 const FVector3f& TargetVelocity,
												 float DeltaTime)
{
	FRVOAgentData& AgentData = GetOrCreateAgentData(InstanceIndex);

	// 基于 Unity SmoothDamp 算法
	float SmoothTime = 1.0f / AntiJitterConfig.VelocitySmoothFactor;
	float Omega = 2.0f / SmoothTime;
	float X = Omega * DeltaTime;
	float Exp = 1.0f / (1.0f + X + 0.48f * X * X + 0.235f * X * X * X);

	FVector3f Change = CurrentVelocity - TargetVelocity;
	FVector3f Temp = (AgentData.VelocitySmoothRef + Omega * Change) * DeltaTime;
	AgentData.VelocitySmoothRef = (AgentData.VelocitySmoothRef - Omega * Temp) * Exp;

	FVector3f SmoothedVelocity = TargetVelocity + (CurrentVelocity - TargetVelocity + Temp) * Exp;

	// 保存平滑后的速度用于下一帧
	AgentData.SmoothedVelocity = SmoothedVelocity;

	return SmoothedVelocity;
}

bool FSkelotRVOSystem::DetectJitter(int32 InstanceIndex, const FVector3f& NewDirection)
{
	const FRVOAgentData* AgentData = GetAgentData(InstanceIndex);
	if (AgentData == nullptr || AgentData->PreviousDirection.SquaredLength() < RVO_EPSILON)
	{
		return false;
	}

	// 计算方向变化
	float DotProduct = FVector3f::DotProduct(NewDirection, AgentData->PreviousDirection);

	// 如果方向变化过大，视为抖动
	if (DotProduct < AntiJitterConfig.JitterThreshold)
	{
		FRVOAgentData& MutableAgentData = GetOrCreateAgentData(InstanceIndex);
		MutableAgentData.JitterCounter++;

		// 连续多次方向变化才算抖动
		if (MutableAgentData.JitterCounter > 2)
		{
			return true;
		}
	}
	else
	{
		// 重置抖动计数器
		FRVOAgentData& MutableAgentData = GetOrCreateAgentData(InstanceIndex);
		MutableAgentData.JitterCounter = 0;
	}

	return false;
}

float FSkelotRVOSystem::GetDensityAdaptationFactor(int32 NeighborCount) const
{
	if (NeighborCount <= AntiJitterConfig.DensityThreshold)
	{
		return 1.0f;
	}

	float ExcessRatio = (float)(NeighborCount - AntiJitterConfig.DensityThreshold) / AntiJitterConfig.DensityThreshold;
	return AntiJitterConfig.HighDensityRelaxation / (1.0f + ExcessRatio);
}

bool FSkelotRVOSystem::ShouldAvoid(const FSkelotInstancesSOA& SOA, int32 IndexA, int32 IndexB) const
{
	// 检查两个实例是否都存活
	if (SOA.Slots[IndexA].bDestroyed || SOA.Slots[IndexB].bDestroyed)
	{
		return false;
	}

	// 获取碰撞通道和掩码
	uint8 ChannelA = SOA.CollisionChannels[IndexA];
	uint8 MaskA = SOA.CollisionMasks[IndexA];
	uint8 ChannelB = SOA.CollisionChannels[IndexB];
	uint8 MaskB = SOA.CollisionMasks[IndexB];

	// 碰撞判定：(MaskA & (1 << ChannelB)) && (MaskB & (1 << ChannelA))
	bool bCanAAvoidB = (MaskA & (1 << ChannelB)) != 0;
	bool bCanBAvoidA = (MaskB & (1 << ChannelA)) != 0;

	return bCanAAvoidB && bCanBAvoidA;
}
