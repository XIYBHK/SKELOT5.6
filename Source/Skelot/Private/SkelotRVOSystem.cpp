// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotRVOSystem.h"
#include "SkelotSpatialGrid.h"
#include "SkelotPBDPlane.h"
#include "SkelotWorld.h"
#include "Async/ParallelFor.h"

// 数学常数
static constexpr float RVO_EPSILON = 0.00001f;

// 2D 行列式（叉积）：det(A, B) = A.x*B.y - A.y*B.x
// 对应 RVO2 库中的 det() 函数，用于半平面约束判定
static FORCEINLINE float Det2D(const FVector2f& A, const FVector2f& B)
{
	return A.X * B.Y - A.Y * B.X;
}

FSkelotRVOSystem::FSkelotRVOSystem()
	: ProcessedAgents(0)
	, TotalVelocityAdjustments(0)
	, CurrentCollisionRadius(60.0f)
	, FrameCounter(0)
{
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
	AgentDataArray.Empty();
}

FRVOAgentData* FSkelotRVOSystem::GetAgentData(int32 InstanceIndex)
{
	return AgentDataArray.IsValidIndex(InstanceIndex) ? &AgentDataArray[InstanceIndex] : nullptr;
}

const FRVOAgentData* FSkelotRVOSystem::GetAgentData(int32 InstanceIndex) const
{
	return AgentDataArray.IsValidIndex(InstanceIndex) ? &AgentDataArray[InstanceIndex] : nullptr;
}

FRVOAgentData& FSkelotRVOSystem::GetOrCreateAgentData(int32 InstanceIndex)
{
	check(AgentDataArray.IsValidIndex(InstanceIndex));
	return AgentDataArray[InstanceIndex];
}

void FSkelotRVOSystem::EnsureAgentDataCapacity(int32 NumInstances)
{
	if (AgentDataArray.Num() < NumInstances)
	{
		const int32 OldNum = AgentDataArray.Num();
		AgentDataArray.SetNum(NumInstances);
		for (int32 i = OldNum; i < NumInstances; ++i)
		{
			AgentDataArray[i] = FRVOAgentData();
		}
	}
}

void FSkelotRVOSystem::ResetAgentDataForInstance(int32 InstanceIndex)
{
	if (AgentDataArray.IsValidIndex(InstanceIndex))
	{
		AgentDataArray[InstanceIndex] = FRVOAgentData();
	}
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
	EnsureAgentDataCapacity(NumInstances);

	// 分帧更新检查
	FrameCounter = (FrameCounter + 1) % Config.FrameStride;
	TArray<uint8> UpdatedFlags;
	UpdatedFlags.SetNumZeroed(NumInstances);
	TArray<FVector3f> InputVelocities;
	InputVelocities.SetNumUninitialized(NumInstances);
	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
	{
		InputVelocities[InstanceIndex] = SOA.Velocities[InstanceIndex];
	}
	TArray<FVector3f> OutputVelocities = InputVelocities;

	ParallelFor(NumInstances, [&](int32 InstanceIndex)
	{
		// 跳过已销毁的实例
		if (SOA.Slots[InstanceIndex].bDestroyed)
		{
			return;
		}

		// 分帧更新：只处理当前帧对应的实例
		if (Config.FrameStride > 1 && (InstanceIndex % Config.FrameStride) != FrameCounter)
		{
			return;
		}

		TArray<int32> LocalNeighborIndices;
		LocalNeighborIndices.Reserve(Config.MaxNeighbors);
		TArray<FORCAPlane> LocalORCAPlanes;
		LocalORCAPlanes.Reserve(Config.MaxNeighbors);

		// 计算避障
		FVector3f NewVelocity;
		if (ComputeAgentAvoidance(SOA, InstanceIndex, InputVelocities, SpatialGrid, DeltaTime, LocalNeighborIndices, LocalORCAPlanes, NewVelocity))
		{
			// 并行阶段仅写输出缓冲，避免读写 SOA.Velocities 竞争
			OutputVelocities[InstanceIndex] = NewVelocity;
			UpdatedFlags[InstanceIndex] = 1;
		}
	});

	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
	{
		if (UpdatedFlags[InstanceIndex] != 0)
		{
			SOA.Velocities[InstanceIndex] = OutputVelocities[InstanceIndex];
			ProcessedAgents++;
			TotalVelocityAdjustments++;
		}
	}
}

bool FSkelotRVOSystem::ComputeAgentAvoidance(const FSkelotInstancesSOA& SOA, int32 InstanceIndex,
											   const TArray<FVector3f>& InputVelocities,
											   const FSkelotSpatialGrid& SpatialGrid,
											   float DeltaTime,
											   TArray<int32>& LocalNeighborIndices,
											   TArray<FORCAPlane>& LocalORCAPlanes,
											   FVector3f& OutNewVelocity)
{
	OutNewVelocity = InputVelocities[InstanceIndex];

	const FVector3d& MyPos3D = SOA.Locations[InstanceIndex];
	const FVector3f& MyVel3D = InputVelocities[InstanceIndex];

	FVector3f MyPos(MyPos3D.X, MyPos3D.Y, 0.0f);
	FVector3f MyVel(MyVel3D.X, MyVel3D.Y, 0.0f);

	float CurrentSpeed = MyVel.Size();
	if (CurrentSpeed < RVO_EPSILON)
	{
		return false;
	}

	FVector2f PreferredVelocity(MyVel.X, MyVel.Y);

	float MaxSpeed = Config.MaxSpeed > 0 ? Config.MaxSpeed : CurrentSpeed;
	MaxSpeed = FMath::Max(MaxSpeed, Config.MinSpeed);

	LocalNeighborIndices.Reset();
	SpatialGrid.QuerySphere(FVector(MyPos3D), Config.NeighborRadius, LocalNeighborIndices, 0xFF, &SOA);

	int32 NumNeighbors = FMath::Min(LocalNeighborIndices.Num(), Config.MaxNeighbors);

	FRVOAgentData& AgentData = GetOrCreateAgentData(InstanceIndex);
	AgentData.CurrentNeighborCount = NumNeighbors;

	LocalORCAPlanes.Reset();
	float CombinedRadius = CurrentCollisionRadius * 2.0f;

	for (int32 i = 0; i < NumNeighbors; i++)
	{
		int32 NeighborIdx = LocalNeighborIndices[i];

		if (NeighborIdx == InstanceIndex)
		{
			continue;
		}

		if (SOA.Slots[NeighborIdx].bDestroyed)
		{
			continue;
		}

		if (!ShouldAvoid(SOA, InstanceIndex, NeighborIdx))
		{
			continue;
		}

		const FVector3d& NeighborPos3D = SOA.Locations[NeighborIdx];

		// 高度差过滤：不同高度的实例不参与 2D 避障
		if (Config.HeightDifferenceThreshold > 0.0f)
		{
			const float HeightDiff = FMath::Abs(static_cast<float>(MyPos3D.Z - NeighborPos3D.Z));
			if (HeightDiff > Config.HeightDifferenceThreshold)
			{
				continue;
			}
		}
		const FVector3f& NeighborVel3D = InputVelocities[NeighborIdx];

		FVector3f NeighborPos(NeighborPos3D.X, NeighborPos3D.Y, 0.0f);
		FVector3f NeighborVel(NeighborVel3D.X, NeighborVel3D.Y, 0.0f);

		FORCAPlane Plane;
		ComputeORCAPlane(MyPos, MyVel, NeighborPos, NeighborVel, CombinedRadius, DeltaTime, Plane);
		LocalORCAPlanes.Add(Plane);
	}

	// LP2 求解：返回失败行号，成功时等于 Planes.Num()
	FVector2f NewVelocity2D;
	const int32 LineFail = LinearProgram2(LocalORCAPlanes, MaxSpeed, PreferredVelocity, false, NewVelocity2D);

	if (LineFail < LocalORCAPlanes.Num())
	{
		LinearProgram3(LocalORCAPlanes, LineFail, MaxSpeed, NewVelocity2D);
	}

	OutNewVelocity = FVector3f(NewVelocity2D.X, NewVelocity2D.Y, 0.0f);

	ApplyAntiJitter(InstanceIndex, InputVelocities[InstanceIndex], OutNewVelocity, DeltaTime, NumNeighbors);

	OutNewVelocity.Z = 0.0f;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// ORCA Plane Computation (参照 RVO2 Agent::computeNewVelocity)
//////////////////////////////////////////////////////////////////////////

void FSkelotRVOSystem::ComputeORCAPlane(const FVector3f& PosA, const FVector3f& VelA,
										const FVector3f& PosB, const FVector3f& VelB,
										float CombinedRadius, float DeltaTime,
										FORCAPlane& OutPlane)
{
	FVector2f RelPos2D(PosB.X - PosA.X, PosB.Y - PosA.Y);
	FVector2f RelVel2D(VelA.X - VelB.X, VelA.Y - VelB.Y);
	FVector2f MyVel2D(VelA.X, VelA.Y);

	float ResponsibilityFactor = 0.5f;

	if (Config.bEnableHRVO)
	{
		if (IsHeadOnCollision(RelPos2D, RelVel2D))
		{
			ResponsibilityFactor = 1.0f;
		}
	}

	ComputeORCAPlaneInternal(RelPos2D, RelVel2D, MyVel2D, CombinedRadius, DeltaTime, ResponsibilityFactor, OutPlane);
}

void FSkelotRVOSystem::ComputeORCAPlaneInternal(const FVector2f& RelPos2D, const FVector2f& RelVel2D,
												const FVector2f& MyVel2D,
												float CombinedRadius, float DeltaTime,
												float ResponsibilityFactor,
												FORCAPlane& OutPlane)
{
	const float DistSq = RelPos2D.SquaredLength();
	const float CombinedRadiusSq = CombinedRadius * CombinedRadius;

	FVector2f u;

	if (DistSq > CombinedRadiusSq)
	{
		// 未碰撞：基于 TimeHorizon 缩放
		const float InvTimeHorizon = 1.0f / FMath::Max(Config.TimeHorizon, RVO_EPSILON);

		// w = relativeVelocity - invTimeHorizon * relativePosition
		const FVector2f W = RelVel2D - InvTimeHorizon * RelPos2D;
		const float WLengthSq = W.SquaredLength();

		const float DotProduct1 = FVector2f::DotProduct(W, RelPos2D);

		if (DotProduct1 < 0.0f && DotProduct1 * DotProduct1 > CombinedRadiusSq * WLengthSq)
		{
			// 投影到 cutoff circle 上
			const float WLength = FMath::Sqrt(WLengthSq);
			const FVector2f UnitW = (WLength > RVO_EPSILON) ? (W / WLength) : FVector2f(1.0f, 0.0f);

			OutPlane.Direction = FVector2f(UnitW.Y, -UnitW.X);
			u = (CombinedRadius * InvTimeHorizon - WLength) * UnitW;
		}
		else
		{
			// 投影到 leg 上
			const float Leg = FMath::Sqrt(FMath::Max(0.0f, DistSq - CombinedRadiusSq));

			if (Det2D(RelPos2D, W) > 0.0f)
			{
				// Left leg
				OutPlane.Direction = FVector2f(
					RelPos2D.X * Leg - RelPos2D.Y * CombinedRadius,
					RelPos2D.X * CombinedRadius + RelPos2D.Y * Leg) / DistSq;
			}
			else
			{
				// Right leg (negated)
				OutPlane.Direction = -FVector2f(
					RelPos2D.X * Leg + RelPos2D.Y * CombinedRadius,
					-RelPos2D.X * CombinedRadius + RelPos2D.Y * Leg) / DistSq;
			}

			u = FVector2f::DotProduct(RelVel2D, OutPlane.Direction) * OutPlane.Direction - RelVel2D;
		}
	}
	else
	{
		// 已碰撞：基于 DeltaTime 的 cutoff circle
		const float InvTimeStep = 1.0f / FMath::Max(DeltaTime, RVO_EPSILON);

		const FVector2f W = RelVel2D - InvTimeStep * RelPos2D;
		const float WLength = W.Size();
		const FVector2f UnitW = (WLength > RVO_EPSILON) ? (W / WLength) : FVector2f(1.0f, 0.0f);

		OutPlane.Direction = FVector2f(UnitW.Y, -UnitW.X);
		u = (CombinedRadius * InvTimeStep - WLength) * UnitW;
	}

	// RVO: point = myVelocity + 0.5*u; VO: point = myVelocity + 1.0*u
	OutPlane.Point = MyVel2D + ResponsibilityFactor * u;
}

bool FSkelotRVOSystem::IsHeadOnCollision(const FVector2f& RelativePosition, const FVector2f& RelativeVelocity) const
{
	const float RelPosSq = RelativePosition.SquaredLength();
	const float RelVelSq = RelativeVelocity.SquaredLength();

	if (RelPosSq < RVO_EPSILON * RVO_EPSILON || RelVelSq < RVO_EPSILON * RVO_EPSILON)
	{
		return false;
	}

	// 避免两次 sqrt：dot(A/|A|, B/|B|) < threshold
	// 等价于：dot(A,B) < 0 && dot(A,B)^2 > threshold^2 * |A|^2 * |B|^2
	const float Dot = FVector2f::DotProduct(RelativePosition, RelativeVelocity);
	if (Dot >= 0.0f)
	{
		return false;
	}
	const float ThresholdSq = Config.HRVOHeadOnThreshold * Config.HRVOHeadOnThreshold;
	return (Dot * Dot) > (ThresholdSq * RelPosSq * RelVelSq);
}

//////////////////////////////////////////////////////////////////////////
// Linear Programming (参照 RVO2 linearProgram1/2/3)
//////////////////////////////////////////////////////////////////////////

bool FSkelotRVOSystem::LinearProgram1(const TArray<FORCAPlane>& Planes, int32 LineNo,
									  float Radius, const FVector2f& OptVelocity, bool bDirectionOpt,
									  FVector2f& OutResult)
{
	const float DotProduct = FVector2f::DotProduct(Planes[LineNo].Point, Planes[LineNo].Direction);
	const float Discriminant = DotProduct * DotProduct + Radius * Radius - Planes[LineNo].Point.SquaredLength();

	if (Discriminant < 0.0f)
	{
		return false;
	}

	const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
	float tLeft = -DotProduct - SqrtDiscriminant;
	float tRight = -DotProduct + SqrtDiscriminant;

	for (int32 i = 0; i < LineNo; ++i)
	{
		const float Denominator = Det2D(Planes[LineNo].Direction, Planes[i].Direction);
		const float Numerator = Det2D(Planes[i].Direction, Planes[LineNo].Point - Planes[i].Point);

		if (FMath::Abs(Denominator) <= RVO_EPSILON)
		{
			if (Numerator < 0.0f)
			{
				return false;
			}
			continue;
		}

		const float t = Numerator / Denominator;

		if (Denominator >= 0.0f)
		{
			tRight = FMath::Min(tRight, t);
		}
		else
		{
			tLeft = FMath::Max(tLeft, t);
		}

		if (tLeft > tRight)
		{
			return false;
		}
	}

	if (bDirectionOpt)
	{
		if (FVector2f::DotProduct(OptVelocity, Planes[LineNo].Direction) > 0.0f)
		{
			OutResult = Planes[LineNo].Point + tRight * Planes[LineNo].Direction;
		}
		else
		{
			OutResult = Planes[LineNo].Point + tLeft * Planes[LineNo].Direction;
		}
	}
	else
	{
		const float t = FVector2f::DotProduct(Planes[LineNo].Direction, OptVelocity - Planes[LineNo].Point);
		const float tClamped = FMath::Clamp(t, tLeft, tRight);
		OutResult = Planes[LineNo].Point + tClamped * Planes[LineNo].Direction;
	}

	return true;
}

int32 FSkelotRVOSystem::LinearProgram2(const TArray<FORCAPlane>& Planes, float Radius,
									   const FVector2f& OptVelocity, bool bDirectionOpt,
									   FVector2f& OutResult)
{
	if (bDirectionOpt)
	{
		OutResult = OptVelocity * Radius;
	}
	else if (OptVelocity.SquaredLength() > Radius * Radius)
	{
		OutResult = OptVelocity.GetSafeNormal() * Radius;
	}
	else
	{
		OutResult = OptVelocity;
	}

	for (int32 i = 0; i < Planes.Num(); ++i)
	{
		if (Det2D(Planes[i].Direction, Planes[i].Point - OutResult) > 0.0f)
		{
			const FVector2f TempResult = OutResult;

			if (!LinearProgram1(Planes, i, Radius, OptVelocity, bDirectionOpt, OutResult))
			{
				OutResult = TempResult;
				return i;
			}
		}
	}

	return Planes.Num();
}

void FSkelotRVOSystem::LinearProgram3(const TArray<FORCAPlane>& Planes, int32 BeginLine,
									  float Radius, FVector2f& InOutResult)
{
	float Distance = 0.0f;

	for (int32 i = BeginLine; i < Planes.Num(); ++i)
	{
		if (Det2D(Planes[i].Direction, Planes[i].Point - InOutResult) > Distance)
		{
			// 构建投影约束集：前 i 条约束线两两交集
			TArray<FORCAPlane> ProjLines;
			ProjLines.Reserve(i);

			for (int32 j = 0; j < i; ++j)
			{
				FORCAPlane Line;
				const float Determinant = Det2D(Planes[i].Direction, Planes[j].Direction);

				if (FMath::Abs(Determinant) <= RVO_EPSILON)
				{
					if (FVector2f::DotProduct(Planes[i].Direction, Planes[j].Direction) > 0.0f)
					{
						continue;
					}
					Line.Point = 0.5f * (Planes[i].Point + Planes[j].Point);
				}
				else
				{
					Line.Point = Planes[i].Point +
						(Det2D(Planes[j].Direction, Planes[i].Point - Planes[j].Point) / Determinant) *
						Planes[i].Direction;
				}

				Line.Direction = (Planes[j].Direction - Planes[i].Direction).GetSafeNormal();
				ProjLines.Add(Line);
			}

			const FVector2f TempResult = InOutResult;

			const FVector2f OptDirection(-Planes[i].Direction.Y, Planes[i].Direction.X);
			if (LinearProgram2(ProjLines, Radius, OptDirection, true, InOutResult) < ProjLines.Num())
			{
				InOutResult = TempResult;
			}

			Distance = Det2D(Planes[i].Direction, Planes[i].Point - InOutResult);
		}
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
	float SmoothTime = FMath::Max(AntiJitterConfig.VelocitySmoothFactor, RVO_EPSILON);
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
	const int32 SafeDensityThreshold = FMath::Max(AntiJitterConfig.DensityThreshold, 1);

	if (NeighborCount <= SafeDensityThreshold)
	{
		return 1.0f;
	}

	float ExcessRatio = (float)(NeighborCount - SafeDensityThreshold) / SafeDensityThreshold;
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
