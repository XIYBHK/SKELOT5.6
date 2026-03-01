// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotGeometryTools.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"

TArray<FVector> USkelotGeometryTools::GetPointsByRound(FVector Origin, float Radius, float Distance, float Noise)
{
	TArray<FVector> Points;

	// 参数验证
	if (Radius <= 0.0f || Distance <= 0.0f)
	{
		return Points;
	}

	// 生成圆形填充的偏移坐标
	TArray<FVector2D> Offsets = GenerateCircleOffsets(Radius, Distance);

	// 转换为世界坐标并应用噪声
	Points.Reserve(Offsets.Num());
	for (const FVector2D& Offset : Offsets)
	{
		FVector Point = Origin + FVector(Offset.X, Offset.Y, 0.0f);

		if (Noise > 0.0f)
		{
			Point = ApplyNoise(Point, Noise);
		}

		Points.Add(Point);
	}

	return Points;
}

TArray<FVector2D> USkelotGeometryTools::GenerateCircleOffsets(float Radius, float Distance)
{
	TArray<FVector2D> Offsets;

	// 使用六边形紧密填充算法
	// 水平间距 = Distance
	// 垂直间距 = Distance * sqrt(3)/2 （交错排列）
	const float HorizontalSpacing = Distance;
	const float VerticalSpacing = Distance * FMath::Sqrt(3.0f) * 0.5f;

	// 计算需要覆盖的行列数
	const int32 NumRows = FMath::CeilToInt(Radius / VerticalSpacing) * 2 + 1;
	const int32 NumCols = FMath::CeilToInt(Radius / HorizontalSpacing) * 2 + 1;

	// 圆心偏移（使中心在原点）
	const float CenterRow = (NumRows - 1) * 0.5f;
	const float CenterCol = (NumCols - 1) * 0.5f;

	const float RadiusSquared = Radius * Radius;

	for (int32 Row = 0; Row < NumRows; ++Row)
	{
		for (int32 Col = 0; Col < NumCols; ++Col)
		{
			// 计算相对于圆心的偏移
			float X = (Col - CenterCol) * HorizontalSpacing;
			float Y = (Row - CenterRow) * VerticalSpacing;

			// 奇数行水平偏移一半间距（六边形填充）
			if (Row % 2 == 1)
			{
				X += HorizontalSpacing * 0.5f;
			}

			// 检查是否在圆内
			if (X * X + Y * Y <= RadiusSquared)
			{
				Offsets.Add(FVector2D(X, Y));
			}
		}
	}

	return Offsets;
}

TArray<FVector> USkelotGeometryTools::GetPointsByGrid(
	FVector Origin,
	float DistanceX, int32 CountX,
	int32 CountY, float DistanceY,
	int32 CountZ, float DistanceZ)
{
	TArray<FVector> Points;

	// 参数验证
	if (DistanceX <= 0.0f || CountX <= 0 ||
		DistanceY <= 0.0f || CountY <= 0 ||
		DistanceZ <= 0.0f || CountZ <= 0)
	{
		return Points;
	}

	// 预分配空间
	const int32 TotalCount = CountX * CountY * CountZ;
	Points.Reserve(TotalCount);

	// 生成网格点
	for (int32 Z = 0; Z < CountZ; ++Z)
	{
		for (int32 Y = 0; Y < CountY; ++Y)
		{
			for (int32 X = 0; X < CountX; ++X)
			{
				FVector Point = Origin + FVector(
					X * DistanceX,
					Y * DistanceY,
					Z * DistanceZ
				);
				Points.Add(Point);
			}
		}
	}

	return Points;
}

FVector USkelotGeometryTools::GetBezierPoint(const TArray<FVector>& ControlPoints, float Progress)
{
	// 参数验证
	if (ControlPoints.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	if (ControlPoints.Num() == 1)
	{
		return ControlPoints[0];
	}

	// 限制 Progress 范围
	Progress = FMath::Clamp(Progress, 0.0f, 1.0f);

	// 使用 De Casteljau 算法计算贝塞尔曲线点
	TArray<FVector> Points = ControlPoints;
	const int32 Degree = ControlPoints.Num() - 1;

	for (int32 i = 1; i <= Degree; ++i)
	{
		for (int32 j = 0; j <= Degree - i; ++j)
		{
			Points[j] = Points[j] * (1.0f - Progress) + Points[j + 1] * Progress;
		}
	}

	return Points[0];
}

FVector USkelotGeometryTools::ApplyNoise(const FVector& Point, float Noise)
{
	if (Noise <= 0.0f)
	{
		return Point;
	}

	// 在 [-Noise, Noise] 范围内随机偏移
	return Point + FVector(
		FMath::FRandRange(-Noise, Noise),
		FMath::FRandRange(-Noise, Noise),
		FMath::FRandRange(-Noise, Noise)
	);
}

TArray<FVector> USkelotGeometryTools::GetPointsByShape(UPrimitiveComponent* ShapeComponent, float Distance, bool bSurfaceOnly, float Noise)
{
	TArray<FVector> Points;

	// 参数验证
	if (!ShapeComponent || Distance <= 0.0f)
	{
		return Points;
	}

	// 获取形状组件的世界变换
	const FTransform& ComponentTransform = ShapeComponent->GetComponentTransform();
	FVector Origin = ComponentTransform.GetLocation();

	// 根据组件类型生成点
	if (USphereComponent* SphereComponent = Cast<USphereComponent>(ShapeComponent))
	{
		// 球形组件
		const float SphereRadius = SphereComponent->GetScaledSphereRadius();

		if (bSurfaceOnly)
		{
			// 球面点 - 使用球坐标均匀采样
			const int32 NumTheta = FMath::CeilToInt(2.0f * PI * SphereRadius / Distance);
			const int32 NumPhi = FMath::CeilToInt(PI * SphereRadius / Distance);

			for (int32 i = 0; i <= NumTheta; ++i)
			{
				for (int32 j = 0; j <= NumPhi; ++j)
				{
					const float Theta = 2.0f * PI * i / NumTheta;
					const float Phi = PI * j / NumPhi;

					FVector LocalPoint = FVector(
						SphereRadius * FMath::Sin(Phi) * FMath::Cos(Theta),
						SphereRadius * FMath::Sin(Phi) * FMath::Sin(Theta),
						SphereRadius * FMath::Cos(Phi)
					);

					FVector WorldPoint = ComponentTransform.TransformPosition(LocalPoint);

					if (Noise > 0.0f)
					{
						WorldPoint = ApplyNoise(WorldPoint, Noise);
					}

					Points.Add(WorldPoint);
				}
			}
		}
		else
		{
			// 球体填充 - 使用球坐标分层填充
			const int32 NumLayers = FMath::CeilToInt(SphereRadius / Distance);

			for (int32 Layer = 0; Layer <= NumLayers; ++Layer)
			{
				const float LayerRadius = Layer * Distance;
				if (LayerRadius > SphereRadius)
				{
					break;
				}

				if (Layer == 0)
				{
					// 中心点
					Points.Add(Origin);
				}
				else
				{
					// 使用圆形填充的XY切片
					TArray<FVector2D> CircleOffsets = GenerateCircleOffsets(LayerRadius, Distance);
					for (const FVector2D& Offset : CircleOffsets)
					{
						FVector LocalPoint(Offset.X, Offset.Y, 0.0f);

						// 检查是否在球内
						if (LocalPoint.SizeSquared() <= SphereRadius * SphereRadius)
						{
							FVector WorldPoint = ComponentTransform.TransformPosition(LocalPoint);

							if (Noise > 0.0f)
							{
								WorldPoint = ApplyNoise(WorldPoint, Noise);
							}

							Points.Add(WorldPoint);
						}
					}
				}
			}
		}
	}
	else if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(ShapeComponent))
	{
		// 盒形组件
		const FVector BoxExtent = BoxComponent->GetScaledBoxExtent();

		if (bSurfaceOnly)
		{
			// 盒子表面 - 6个面
			const int32 CountX = FMath::CeilToInt(BoxExtent.X * 2.0f / Distance);
			const int32 CountY = FMath::CeilToInt(BoxExtent.Y * 2.0f / Distance);
			const int32 CountZ = FMath::CeilToInt(BoxExtent.Z * 2.0f / Distance);

			// 上下表面 (XY 平面)
			for (int32 X = 0; X <= CountX; ++X)
			{
				for (int32 Y = 0; Y <= CountY; ++Y)
				{
					FVector LocalPoint(
						-BoxExtent.X + X * Distance,
						-BoxExtent.Y + Y * Distance,
						BoxExtent.Z
					);
					FVector WorldPoint = ComponentTransform.TransformPosition(LocalPoint);
					if (Noise > 0.0f) WorldPoint = ApplyNoise(WorldPoint, Noise);
					Points.Add(WorldPoint);

					LocalPoint.Z = -BoxExtent.Z;
					WorldPoint = ComponentTransform.TransformPosition(LocalPoint);
					if (Noise > 0.0f) WorldPoint = ApplyNoise(WorldPoint, Noise);
					Points.Add(WorldPoint);
				}
			}

			// 前后表面 (XZ 平面)
			for (int32 X = 0; X <= CountX; ++X)
			{
				for (int32 Z = 0; Z <= CountZ; ++Z)
				{
					FVector LocalPoint(
						-BoxExtent.X + X * Distance,
						BoxExtent.Y,
						-BoxExtent.Z + Z * Distance
					);
					FVector WorldPoint = ComponentTransform.TransformPosition(LocalPoint);
					if (Noise > 0.0f) WorldPoint = ApplyNoise(WorldPoint, Noise);
					Points.Add(WorldPoint);

					LocalPoint.Y = -BoxExtent.Y;
					WorldPoint = ComponentTransform.TransformPosition(LocalPoint);
					if (Noise > 0.0f) WorldPoint = ApplyNoise(WorldPoint, Noise);
					Points.Add(WorldPoint);
				}
			}

			// 左右表面 (YZ 平面)
			for (int32 Y = 0; Y <= CountY; ++Y)
			{
				for (int32 Z = 0; Z <= CountZ; ++Z)
				{
					FVector LocalPoint(
						BoxExtent.X,
						-BoxExtent.Y + Y * Distance,
						-BoxExtent.Z + Z * Distance
					);
					FVector WorldPoint = ComponentTransform.TransformPosition(LocalPoint);
					if (Noise > 0.0f) WorldPoint = ApplyNoise(WorldPoint, Noise);
					Points.Add(WorldPoint);

					LocalPoint.X = -BoxExtent.X;
					WorldPoint = ComponentTransform.TransformPosition(LocalPoint);
					if (Noise > 0.0f) WorldPoint = ApplyNoise(WorldPoint, Noise);
					Points.Add(WorldPoint);
				}
			}
		}
		else
		{
			// 盒体填充 - 直接使用网格生成
			const int32 CountX = FMath::CeilToInt(BoxExtent.X * 2.0f / Distance);
			const int32 CountY = FMath::CeilToInt(BoxExtent.Y * 2.0f / Distance);
			const int32 CountZ = FMath::CeilToInt(BoxExtent.Z * 2.0f / Distance);

			const FVector LocalOrigin = -BoxExtent;

			for (int32 Z = 0; Z <= CountZ; ++Z)
			{
				for (int32 Y = 0; Y <= CountY; ++Y)
				{
					for (int32 X = 0; X <= CountX; ++X)
					{
						FVector LocalPoint(
							LocalOrigin.X + X * Distance,
							LocalOrigin.Y + Y * Distance,
							LocalOrigin.Z + Z * Distance
						);

						// 检查是否在盒内
						if (FMath::Abs(LocalPoint.X) <= BoxExtent.X &&
							FMath::Abs(LocalPoint.Y) <= BoxExtent.Y &&
							FMath::Abs(LocalPoint.Z) <= BoxExtent.Z)
						{
							FVector WorldPoint = ComponentTransform.TransformPosition(LocalPoint);

							if (Noise > 0.0f)
							{
								WorldPoint = ApplyNoise(WorldPoint, Noise);
							}

							Points.Add(WorldPoint);
						}
					}
				}
			}
		}
	}
	else if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(ShapeComponent))
	{
		// 胶囊组件
		const float CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
		const float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
		const float CylinderHalfHeight = CapsuleHalfHeight - CapsuleRadius;

		if (bSurfaceOnly)
		{
			// 胶囊表面 - 圆柱体表面 + 两个半球
			const int32 CountAngle = FMath::CeilToInt(2.0f * PI * CapsuleRadius / Distance);
			const int32 CountHeight = FMath::CeilToInt(CylinderHalfHeight * 2.0f / Distance);

			// 圆柱体表面
			for (int32 Angle = 0; Angle < CountAngle; ++Angle)
			{
				const float Theta = 2.0f * PI * Angle / CountAngle;

				for (int32 H = 0; H <= CountHeight; ++H)
				{
					const float Z = -CylinderHalfHeight + H * Distance;
					FVector LocalPoint(
						CapsuleRadius * FMath::Cos(Theta),
						CapsuleRadius * FMath::Sin(Theta),
						Z
					);
					FVector WorldPoint = ComponentTransform.TransformPosition(LocalPoint);
					if (Noise > 0.0f) WorldPoint = ApplyNoise(WorldPoint, Noise);
					Points.Add(WorldPoint);
				}
			}

			// 顶部和底部半球
			const int32 CountPhi = FMath::CeilToInt(PI * CapsuleRadius * 0.5f / Distance);

			for (int32 Angle = 0; Angle < CountAngle; ++Angle)
			{
				const float Theta = 2.0f * PI * Angle / CountAngle;

				for (int32 P = 1; P <= CountPhi; ++P)
				{
					const float Phi = PI * 0.5f * P / CountPhi;
					const float R = CapsuleRadius * FMath::Sin(Phi);
					const float LocalZ = CapsuleRadius * FMath::Cos(Phi);

					// 顶部半球
					FVector LocalPointTop(
						R * FMath::Cos(Theta),
						R * FMath::Sin(Theta),
						CylinderHalfHeight + LocalZ
					);
					FVector WorldPointTop = ComponentTransform.TransformPosition(LocalPointTop);
					if (Noise > 0.0f) WorldPointTop = ApplyNoise(WorldPointTop, Noise);
					Points.Add(WorldPointTop);

					// 底部半球
					FVector LocalPointBottom(
						R * FMath::Cos(Theta),
						R * FMath::Sin(Theta),
						-CylinderHalfHeight - LocalZ
					);
					FVector WorldPointBottom = ComponentTransform.TransformPosition(LocalPointBottom);
					if (Noise > 0.0f) WorldPointBottom = ApplyNoise(WorldPointBottom, Noise);
					Points.Add(WorldPointBottom);
				}
			}
		}
		else
		{
			// 胶囊填充 - 使用切片填充
			const int32 CountZ = FMath::CeilToInt(CapsuleHalfHeight * 2.0f / Distance);

			for (int32 ZStep = 0; ZStep <= CountZ; ++ZStep)
			{
				const float Z = -CapsuleHalfHeight + ZStep * Distance;

				// 计算当前Z高度的切片半径
				float SliceRadius = CapsuleRadius;
				if (Z > CylinderHalfHeight)
				{
					// 顶部半球区域
					SliceRadius = FMath::Sqrt(CapsuleRadius * CapsuleRadius - FMath::Square(Z - CylinderHalfHeight));
				}
				else if (Z < -CylinderHalfHeight)
				{
					// 底部半球区域
					SliceRadius = FMath::Sqrt(CapsuleRadius * CapsuleRadius - FMath::Square(Z + CylinderHalfHeight));
				}

				if (SliceRadius > 0.0f)
				{
					// 使用圆形填充
					TArray<FVector2D> CircleOffsets = GenerateCircleOffsets(SliceRadius, Distance);
					for (const FVector2D& Offset : CircleOffsets)
					{
						FVector LocalPoint(Offset.X, Offset.Y, Z);
						FVector WorldPoint = ComponentTransform.TransformPosition(LocalPoint);

						if (Noise > 0.0f)
						{
							WorldPoint = ApplyNoise(WorldPoint, Noise);
						}

						Points.Add(WorldPoint);
					}
				}
			}
		}
	}

	return Points;
}
