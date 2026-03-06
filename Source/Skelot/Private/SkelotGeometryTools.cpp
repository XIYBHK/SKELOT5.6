// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotGeometryTools.h"
#include "Algo/BinarySearch.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

#if WITH_EDITORONLY_DATA
#include "StaticMeshResources.h"
#endif

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
	const int64 TotalCount64 = static_cast<int64>(CountX) * CountY * CountZ;
	if (TotalCount64 > MAX_int32)
	{
		return Points;
	}

	Points.Reserve(static_cast<int32>(TotalCount64));

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
			if (SphereRadius < KINDA_SMALL_NUMBER || Distance < KINDA_SMALL_NUMBER)
			{
				Points.Add(Origin);
				return Points;
			}

			const int32 NumTheta = FMath::Max(1, FMath::CeilToInt(2.0f * PI * SphereRadius / Distance));
			const int32 NumPhi = FMath::Max(1, FMath::CeilToInt(PI * SphereRadius / Distance));

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
			// 球体填充 - 按 Z 切片填充每层圆盘
			const int32 NumZSlices = FMath::CeilToInt((SphereRadius * 2.0f) / Distance);
			for (int32 Slice = 0; Slice <= NumZSlices; ++Slice)
			{
				const float Z = -SphereRadius + Slice * Distance;
				if (Z < -SphereRadius || Z > SphereRadius)
				{
					continue;
				}

				const float SliceRadiusSq = SphereRadius * SphereRadius - Z * Z;
				if (SliceRadiusSq <= 0.0f)
				{
					FVector WorldPoint = ComponentTransform.TransformPosition(FVector(0.0f, 0.0f, Z));
					if (Noise > 0.0f)
					{
						WorldPoint = ApplyNoise(WorldPoint, Noise);
					}
					Points.Add(WorldPoint);
					continue;
				}

				const float SliceRadius = FMath::Sqrt(SliceRadiusSq);
				const TArray<FVector2D> CircleOffsets = GenerateCircleOffsets(SliceRadius, Distance);
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
				for (int32 Z = 1; Z < CountZ; ++Z)
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
			for (int32 Y = 1; Y < CountY; ++Y)
			{
				for (int32 Z = 1; Z < CountZ; ++Z)
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
			if (CapsuleRadius < KINDA_SMALL_NUMBER || Distance < KINDA_SMALL_NUMBER)
			{
				Points.Add(Origin);
				return Points;
			}

			const int32 CountAngle = FMath::Max(1, FMath::CeilToInt(2.0f * PI * CapsuleRadius / Distance));
			const int32 CountHeight = FMath::Max(1, FMath::CeilToInt(CylinderHalfHeight * 2.0f / Distance));

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

// ============================================================================
// 辅助函数
// ============================================================================

FVector USkelotGeometryTools::GetRandomPointInTriangle(const FVector& V0, const FVector& V1, const FVector& V2)
{
	// 使用重心坐标在三角形内生成均匀分布的随机点
	float R1 = FMath::Sqrt(FMath::FRand());
	float R2 = FMath::FRand();

	// 重心坐标: (1 - sqrt(r1)) * V0 + sqrt(r1) * (1 - r2) * V1 + sqrt(r1) * r2 * V2
	return (1.0f - R1) * V0 + R1 * (1.0f - R2) * V1 + R1 * R2 * V2;
}

float USkelotGeometryTools::GetTriangleArea(const FVector& V0, const FVector& V1, const FVector& V2)
{
	// 使用叉积计算三角形面积 = |(V1-V0) x (V2-V0)| / 2
	FVector Cross = FVector::CrossProduct(V1 - V0, V2 - V0);
	return Cross.Size() * 0.5f;
}

// ============================================================================
// 静态网格表面点生成
// ============================================================================

TArray<FVector> USkelotGeometryTools::GetPointsByMesh(UStaticMesh* Mesh, FTransform Transform, float Distance, float Noise, int32 LODIndex)
{
	TArray<FVector> Points;

	// 参数验证
	if (!Mesh || Distance <= 0.0f)
	{
		return Points;
	}

#if WITH_EDITORONLY_DATA
	// 获取渲染数据
	FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.Num() <= LODIndex)
	{
		return Points;
	}

	const FStaticMeshLODResources& LODResources = RenderData->LODResources[LODIndex];
	const FPositionVertexBuffer& PositionBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
	const uint32 NumVertices = PositionBuffer.GetNumVertices();

	// 获取索引缓冲
	const FRawStaticIndexBuffer& IndexBuffer = LODResources.IndexBuffer;
	const int32 NumIndices = IndexBuffer.GetNumIndices();

	if (NumVertices == 0 || NumIndices == 0)
	{
		return Points;
	}

	// 获取索引数组视图
	FIndexArrayView IndexArrayView = IndexBuffer.GetArrayView();

	// 收集所有三角形顶点
	TArray<FVector> AllTriangles;

	// 使用 Section 遍历三角形
	const int32 NumSections = LODResources.Sections.Num();

	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		const FStaticMeshSection& Section = LODResources.Sections[SectionIndex];
		if (Section.NumTriangles == 0)
		{
			continue;
		}

		// 遍历 Section 中的三角形
		for (uint32 TriIdx = 0; TriIdx < Section.NumTriangles; ++TriIdx)
		{
			const uint32 IndexBase = Section.FirstIndex + TriIdx * 3;

			// 检查索引范围
			if (IndexBase + 2 >= (uint32)NumIndices)
			{
				continue;
			}

			// 通过索引缓冲获取真正的顶点索引
			const uint32 Vtx0 = IndexArrayView[IndexBase];
			const uint32 Vtx1 = IndexArrayView[IndexBase + 1];
			const uint32 Vtx2 = IndexArrayView[IndexBase + 2];

			// 检查顶点索引范围
			if (Vtx0 >= NumVertices || Vtx1 >= NumVertices || Vtx2 >= NumVertices)
			{
				continue;
			}

			FVector V0 = FVector(PositionBuffer.VertexPosition(Vtx0));
			FVector V1 = FVector(PositionBuffer.VertexPosition(Vtx1));
			FVector V2 = FVector(PositionBuffer.VertexPosition(Vtx2));

			// 应用变换
			V0 = Transform.TransformPosition(V0);
			V1 = Transform.TransformPosition(V1);
			V2 = Transform.TransformPosition(V2);

			AllTriangles.Add(V0);
			AllTriangles.Add(V1);
			AllTriangles.Add(V2);
		}
	}

	if (AllTriangles.Num() == 0)
	{
		return Points;
	}

	// 计算总面积
	float TotalArea = 0.0f;
	TArray<float> CumulativeAreas;

	for (int32 i = 0; i < AllTriangles.Num(); i += 3)
	{
		float Area = GetTriangleArea(AllTriangles[i], AllTriangles[i + 1], AllTriangles[i + 2]);
		TotalArea += Area;
		CumulativeAreas.Add(TotalArea);
	}

	if (TotalArea <= KINDA_SMALL_NUMBER)
	{
		return Points;
	}

	// 计算需要生成的点数
	const float PointArea = Distance * Distance;
	const int32 NumPoints = FMath::CeilToInt(TotalArea / PointArea);

	Points.Reserve(NumPoints);

	// 使用三角形面积加权采样生成点
	for (int32 i = 0; i < NumPoints; ++i)
	{
		float RandomArea = FMath::FRand() * TotalArea;

		int32 SelectedTriIndex = Algo::LowerBound(CumulativeAreas, RandomArea);
		SelectedTriIndex = FMath::Clamp(SelectedTriIndex, 0, CumulativeAreas.Num() - 1);

		const int32 TriBase = SelectedTriIndex * 3;
		FVector Point = GetRandomPointInTriangle(AllTriangles[TriBase], AllTriangles[TriBase + 1], AllTriangles[TriBase + 2]);

		if (Noise > 0.0f)
		{
			Point = ApplyNoise(Point, Noise);
		}

		Points.Add(Point);
	}
#else
	// 运行时版本暂不支持，返回空数组
	// 可以考虑使用 Physics API 或其他运行时可用的方法
#endif

	return Points;
}

// ============================================================================
// 静态网格体素化点生成
// ============================================================================

TArray<FVector> USkelotGeometryTools::GetPointsByMeshVoxel(UStaticMesh* Mesh, FTransform Transform, float VoxelSize, bool bSolid, float Noise, int32 LODIndex)
{
	TArray<FVector> Points;

	// 参数验证
	if (!Mesh || VoxelSize <= 0.0f)
	{
		return Points;
	}

#if WITH_EDITORONLY_DATA
	// 获取网格包围盒
	FBoxSphereBounds MeshBounds = Mesh->GetBounds();
	FBox LocalBounds = MeshBounds.GetBox();

	// 扩展包围盒
	FBox ExpandedBounds = LocalBounds;
	ExpandedBounds.Min -= FVector(VoxelSize);
	ExpandedBounds.Max += FVector(VoxelSize);

	// 计算变换后的包围盒 - 手动计算8个角点
	FBox WorldBounds(ForceInit);
	const FVector Corners[8] = {
		FVector(ExpandedBounds.Min.X, ExpandedBounds.Min.Y, ExpandedBounds.Min.Z),
		FVector(ExpandedBounds.Max.X, ExpandedBounds.Min.Y, ExpandedBounds.Min.Z),
		FVector(ExpandedBounds.Min.X, ExpandedBounds.Max.Y, ExpandedBounds.Min.Z),
		FVector(ExpandedBounds.Max.X, ExpandedBounds.Max.Y, ExpandedBounds.Min.Z),
		FVector(ExpandedBounds.Min.X, ExpandedBounds.Min.Y, ExpandedBounds.Max.Z),
		FVector(ExpandedBounds.Max.X, ExpandedBounds.Min.Y, ExpandedBounds.Max.Z),
		FVector(ExpandedBounds.Min.X, ExpandedBounds.Max.Y, ExpandedBounds.Max.Z),
		FVector(ExpandedBounds.Max.X, ExpandedBounds.Max.Y, ExpandedBounds.Max.Z)
	};

	for (int32 i = 0; i < 8; ++i)
	{
		WorldBounds += Transform.TransformPosition(Corners[i]);
	}

	// 体素网格尺寸
	const FIntVector GridSize(
		FMath::CeilToInt(WorldBounds.GetSize().X / VoxelSize),
		FMath::CeilToInt(WorldBounds.GetSize().Y / VoxelSize),
		FMath::CeilToInt(WorldBounds.GetSize().Z / VoxelSize)
	);

	if (GridSize.X <= 0 || GridSize.Y <= 0 || GridSize.Z <= 0)
	{
		return Points;
	}

	// 收集所有三角形用于内外测试
	TArray<FVector> Triangles;

	// 获取渲染数据
	FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.Num() <= LODIndex)
	{
		return Points;
	}

	const FStaticMeshLODResources& LODResources = RenderData->LODResources[LODIndex];
	const FPositionVertexBuffer& PositionBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
	const uint32 NumVertices = PositionBuffer.GetNumVertices();

	// 获取索引缓冲
	const FRawStaticIndexBuffer& IndexBuffer = LODResources.IndexBuffer;
	const int32 NumIndices = IndexBuffer.GetNumIndices();

	if (NumVertices == 0 || NumIndices == 0)
	{
		return Points;
	}

	// 获取索引数组视图
	FIndexArrayView IndexArrayView = IndexBuffer.GetArrayView();

	// 使用 Section 遍历
	const int32 NumSections = LODResources.Sections.Num();

	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		const FStaticMeshSection& Section = LODResources.Sections[SectionIndex];
		if (Section.NumTriangles == 0)
		{
			continue;
		}

		for (uint32 TriIdx = 0; TriIdx < Section.NumTriangles; ++TriIdx)
		{
			const uint32 IndexBase = Section.FirstIndex + TriIdx * 3;

			// 检查索引范围
			if (IndexBase + 2 >= (uint32)NumIndices)
			{
				continue;
			}

			// 通过索引缓冲获取真正的顶点索引
			const uint32 Vtx0 = IndexArrayView[IndexBase];
			const uint32 Vtx1 = IndexArrayView[IndexBase + 1];
			const uint32 Vtx2 = IndexArrayView[IndexBase + 2];

			// 检查顶点索引范围
			if (Vtx0 >= NumVertices || Vtx1 >= NumVertices || Vtx2 >= NumVertices)
			{
				continue;
			}

			FVector V0 = FVector(PositionBuffer.VertexPosition(Vtx0));
			FVector V1 = FVector(PositionBuffer.VertexPosition(Vtx1));
			FVector V2 = FVector(PositionBuffer.VertexPosition(Vtx2));

			V0 = Transform.TransformPosition(V0);
			V1 = Transform.TransformPosition(V1);
			V2 = Transform.TransformPosition(V2);

			Triangles.Add(V0);
			Triangles.Add(V1);
			Triangles.Add(V2);
		}
	}

	if (Triangles.Num() == 0)
	{
		return Points;
	}

	// 使用射线投射判断点是否在网格内
	auto IsPointInsideMesh = [&Triangles](const FVector& Point) -> bool
	{
		// 使用射线投射法：从点向任意方向发射射线，计算与三角形的交点数
		// 奇数 = 在内部，偶数 = 在外部
		const FVector RayEnd = Point + FVector(1000000.0f, 0.0f, 0.0f);
		int32 IntersectionCount = 0;

		for (int32 i = 0; i < Triangles.Num(); i += 3)
		{
			const FVector& V0 = Triangles[i];
			const FVector& V1 = Triangles[i + 1];
			const FVector& V2 = Triangles[i + 2];

			// 使用 Moller-Trumbore 算法的简化版本
			FVector Edge1 = V1 - V0;
			FVector Edge2 = V2 - V0;
			FVector RayDir = RayEnd - Point;

			FVector h = FVector::CrossProduct(RayDir, Edge2);
			float a = FVector::DotProduct(Edge1, h);

			if (FMath::Abs(a) < KINDA_SMALL_NUMBER)
			{
				continue;
			}

			float f = 1.0f / a;
			FVector s = Point - V0;
			float u = f * FVector::DotProduct(s, h);

			if (u < 0.0f || u > 1.0f)
			{
				continue;
			}

			FVector q = FVector::CrossProduct(s, Edge1);
			float v = f * FVector::DotProduct(RayDir, q);

			if (v < 0.0f || u + v > 1.0f)
			{
				continue;
			}

			float t = f * FVector::DotProduct(Edge2, q);

			if (t > KINDA_SMALL_NUMBER)
			{
				IntersectionCount++;
			}
		}

		return (IntersectionCount % 2) == 1;
	};

	// 检查体素是否与任何三角形相交
	auto DoesVoxelIntersectMesh = [&Triangles, VoxelSize](const FVector& VoxelCenter) -> bool
	{
		const float HalfVoxel = VoxelSize * 0.5f;
		const float SearchRadius = HalfVoxel * FMath::Sqrt(3.0f);

		for (int32 i = 0; i < Triangles.Num(); i += 3)
		{
			const FVector& V0 = Triangles[i];
			const FVector& V1 = Triangles[i + 1];
			const FVector& V2 = Triangles[i + 2];

			FVector ClosestPoint = FMath::ClosestPointOnTriangleToPoint(VoxelCenter, V0, V1, V2);
			float DistSq = FVector::DistSquared(VoxelCenter, ClosestPoint);

			if (DistSq <= SearchRadius * SearchRadius)
			{
				return true;
			}
		}

		return false;
	};

	// 遍历体素网格
	const FVector WorldOrigin = WorldBounds.Min;

	for (int32 Z = 0; Z < GridSize.Z; ++Z)
	{
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < GridSize.X; ++X)
			{
				FVector VoxelCenter = WorldOrigin + FVector(
					(X + 0.5f) * VoxelSize,
					(Y + 0.5f) * VoxelSize,
					(Z + 0.5f) * VoxelSize
				);

				bool bIncludeVoxel = false;

				if (bSolid)
				{
					bIncludeVoxel = IsPointInsideMesh(VoxelCenter);
				}
				else
				{
					bIncludeVoxel = DoesVoxelIntersectMesh(VoxelCenter);
				}

				if (bIncludeVoxel)
				{
					if (Noise > 0.0f)
					{
						VoxelCenter = ApplyNoise(VoxelCenter, Noise);
					}
					Points.Add(VoxelCenter);
				}
			}
		}
	}
#else
	// 运行时版本暂不支持
#endif

	return Points;
}

// ============================================================================
// 样条曲线点带生成
// ============================================================================

TArray<FVector> USkelotGeometryTools::GetPointsBySpline(USplineComponent* SplineComponent, int32 CountX, int32 CountY, float Width, float Noise)
{
	TArray<FVector> Points;

	// 参数验证
	if (!SplineComponent || CountX <= 0 || CountY <= 0 || Width <= 0.0f)
	{
		return Points;
	}

	// 获取样条长度
	const float SplineLength = SplineComponent->GetSplineLength();
	if (SplineLength <= 0.0f)
	{
		return Points;
	}

	Points.Reserve(CountX * CountY);

	// 沿样条生成点带
	for (int32 X = 0; X < CountX; ++X)
	{
		// 计算沿样条的进度
		const float Distance = (static_cast<float>(X) / FMath::Max(1, CountX - 1)) * SplineLength;

		// 获取样条上的位置和方向
		const FVector SplinePos = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		const FVector SplineRight = SplineComponent->GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

		// 沿垂直方向生成点
		for (int32 Y = 0; Y < CountY; ++Y)
		{
			// 计算垂直方向的偏移
			const float OffsetY = (CountY <= 1) ? 0.0f : ((static_cast<float>(Y) / (CountY - 1) - 0.5f) * Width);

			FVector Point = SplinePos + SplineRight * OffsetY;

			// 应用噪声
			if (Noise > 0.0f)
			{
				Point = ApplyNoise(Point, Noise);
			}

			Points.Add(Point);
		}
	}

	return Points;
}

// ============================================================================
// 纹理像素提取
// ============================================================================

void USkelotGeometryTools::GetPixelsByTexture(UTexture2D* Texture, int32 SampleStep, TArray<FColor>& OutColors, TArray<FVector2D>& OutUVs)
{
	OutColors.Empty();
	OutUVs.Empty();

	// 参数验证
	if (!Texture || SampleStep < 1)
	{
		return;
	}

	// 获取纹理尺寸
	const int32 TextureWidth = Texture->GetSizeX();
	const int32 TextureHeight = Texture->GetSizeY();

	if (TextureWidth <= 0 || TextureHeight <= 0)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	// 在编辑器中，尝试使用平台数据
	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData)
	{
		return;
	}

	// 获取 Mip 数据
	if (PlatformData->Mips.Num() == 0)
	{
		return;
	}

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_ONLY);

	if (!Data)
	{
		Mip.BulkData.Unlock();
		return;
	}

	const uint8* PixelData = static_cast<const uint8*>(Data);
	const EPixelFormat PixelFormat = static_cast<EPixelFormat>(PlatformData->PixelFormat);
	const bool bIsBGRA8 = PixelFormat == PF_B8G8R8A8;
	const bool bIsRGBA8 = PixelFormat == PF_R8G8B8A8;

	if (!bIsBGRA8 && !bIsRGBA8)
	{
		Mip.BulkData.Unlock();
		return;
	}

	// 仅支持 4 字节像素格式
	const int32 BytesPerPixel = 4;

	// 计算采样后的像素数量
	const int32 NumSamplesX = FMath::CeilToInt(static_cast<float>(TextureWidth) / SampleStep);
	const int32 NumSamplesY = FMath::CeilToInt(static_cast<float>(TextureHeight) / SampleStep);
	const int32 TotalSamples = NumSamplesX * NumSamplesY;

	OutColors.Reserve(TotalSamples);
	OutUVs.Reserve(TotalSamples);

	// 遍历并采样
	for (int32 Y = 0; Y < TextureHeight; Y += SampleStep)
	{
		for (int32 X = 0; X < TextureWidth; X += SampleStep)
		{
			const int32 PixelIndex = (Y * TextureWidth + X) * BytesPerPixel;

			if (PixelIndex + 3 < TextureWidth * TextureHeight * BytesPerPixel)
			{
				FColor Color;

				if (bIsBGRA8)
				{
					Color.B = PixelData[PixelIndex + 0];
					Color.G = PixelData[PixelIndex + 1];
					Color.R = PixelData[PixelIndex + 2];
				}
				else
				{
					Color.R = PixelData[PixelIndex + 0];
					Color.G = PixelData[PixelIndex + 1];
					Color.B = PixelData[PixelIndex + 2];
				}

				Color.A = PixelData[PixelIndex + 3];

				OutColors.Add(Color);

				// 计算 UV 坐标 (0-1 范围)
				FVector2D UV(
					static_cast<float>(X) / TextureWidth,
					static_cast<float>(Y) / TextureHeight
				);
				OutUVs.Add(UV);
			}
		}
	}

	Mip.BulkData.Unlock();
#else
	// 运行时版本 - 需要使用渲染资源
	const int32 NumSamplesX = FMath::CeilToInt(static_cast<float>(TextureWidth) / SampleStep);
	const int32 NumSamplesY = FMath::CeilToInt(static_cast<float>(TextureHeight) / SampleStep);
	const int32 TotalSamples = NumSamplesX * NumSamplesY;

	OutColors.Reserve(TotalSamples);
	OutUVs.Reserve(TotalSamples);

	// 运行时不直接访问纹理数据，返回占位数据
	for (int32 Y = 0; Y < TextureHeight; Y += SampleStep)
	{
		for (int32 X = 0; X < TextureWidth; X += SampleStep)
		{
			OutColors.Add(FColor::Magenta);

			FVector2D UV(
				static_cast<float>(X) / TextureWidth,
				static_cast<float>(Y) / TextureHeight
			);
			OutUVs.Add(UV);
		}
	}
#endif
}
