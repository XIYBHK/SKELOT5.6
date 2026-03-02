// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkelotGeometryTools.generated.h"

class UStaticMesh;
class USplineComponent;
class UTexture2D;

/**
 * Skelot 几何工具函数库
 * 提供点阵生成功能，用于批量创建实例位置
 */
UCLASS()
class SKELOT_API USkelotGeometryTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 在XY平面圆形区域内生成点阵
	 * @param Origin 圆心位置（世界坐标）
	 * @param Radius 圆半径（厘米）
	 * @param Distance 点间距（厘米）
	 * @param Noise 随机偏移量（厘米），0表示规则排列
	 * @return 点坐标数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|几何工具", meta = (DisplayName = "获取圆形区域点阵"))
	static TArray<FVector> GetPointsByRound(FVector Origin, float Radius, float Distance, float Noise = 0.0f);

	/**
	 * 按网格模式生成点阵（支持1D/2D/3D）
	 * @param Origin 网格起点（世界坐标）
	 * @param DistanceX X方向间距（厘米）
	 * @param CountX X方向数量
	 * @param CountY Y方向数量（1表示1D线形）
	 * @param DistanceY Y方向间距（厘米）
	 * @param CountZ Z方向数量（1且CountY>1表示2D平面）
	 * @param DistanceZ Z方向间距（厘米）
	 * @return 点坐标数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|几何工具", meta = (DisplayName = "获取网格点阵"))
	static TArray<FVector> GetPointsByGrid(
		FVector Origin,
		float DistanceX, int32 CountX,
		int32 CountY = 1, float DistanceY = 100.0f,
		int32 CountZ = 1, float DistanceZ = 100.0f
	);

	/**
	 * 计算贝塞尔曲线上的点
	 * @param ControlPoints 控制点数组（支持任意阶）
	 * @param Progress 曲线进度（0.0-1.0）
	 * @return 曲线上的点坐标
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|几何工具", meta = (DisplayName = "获取贝塞尔曲线点"))
	static FVector GetBezierPoint(const TArray<FVector>& ControlPoints, float Progress);

	/**
	 * 在形状组件内生成均匀分布的点
	 * @param ShapeComponent 形状组件（球/盒/胶囊）
	 * @param Distance 点间距（厘米）
	 * @param bSurfaceOnly true表示仅表面，false表示填充体积
	 * @param Noise 随机偏移量（厘米），0表示规则排列
	 * @return 点坐标数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|几何工具", meta = (DisplayName = "按形状获取点阵"))
	static TArray<FVector> GetPointsByShape(UPrimitiveComponent* ShapeComponent, float Distance, bool bSurfaceOnly = false, float Noise = 0.0f);

	/**
	 * 在静态网格表面生成随机点（三角形面积加权采样）
	 * @param Mesh 静态网格资产
	 * @param Transform 网格变换
	 * @param Distance 目标点间距（厘米）
	 * @param Noise 随机偏移量（厘米），0表示规则排列
	 * @param LODIndex LOD级别（0为最高）
	 * @return 点坐标数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|几何工具", meta = (DisplayName = "按网格模型获取表面点"))
	static TArray<FVector> GetPointsByMesh(UStaticMesh* Mesh, FTransform Transform, float Distance, float Noise = 0.0f, int32 LODIndex = 0);

	/**
	 * 在静态网格上生成体素化点
	 * @param Mesh 静态网格资产
	 * @param Transform 网格变换
	 * @param VoxelSize 体素大小（厘米）
	 * @param bSolid true表示实心，false表示外壳
	 * @param Noise 随机偏移量（厘米）
	 * @param LODIndex LOD级别（0为最高）
	 * @return 点坐标数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|几何工具", meta = (DisplayName = "按网格模型获取体素点"))
	static TArray<FVector> GetPointsByMeshVoxel(UStaticMesh* Mesh, FTransform Transform, float VoxelSize, bool bSolid = false, float Noise = 0.0f, int32 LODIndex = 0);

	/**
	 * 沿样条曲线生成点带
	 * @param SplineComponent 样条组件
	 * @param CountX 沿样条方向点数
	 * @param CountY 垂直方向点数（形成带状）
	 * @param Width 带状宽度（厘米）
	 * @param Noise 随机偏移量（厘米）
	 * @return 点坐标数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|几何工具", meta = (DisplayName = "按样条曲线获取点阵"))
	static TArray<FVector> GetPointsBySpline(USplineComponent* SplineComponent, int32 CountX, int32 CountY = 1, float Width = 100.0f, float Noise = 0.0f);

	/**
	 * 从纹理获取像素数据
	 * @param Texture 纹理资产
	 * @param SampleStep 采样步长，1表示全部，3表示每3个取1个
	 * @param OutColors 输出像素颜色数组
	 * @param OutUVs 输出像素UV坐标数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|几何工具", meta = (DisplayName = "从纹理获取像素"))
	static void GetPixelsByTexture(UTexture2D* Texture, int32 SampleStep, TArray<FColor>& OutColors, TArray<FVector2D>& OutUVs);

private:
	/**
	 * 生成圆形填充的偏移坐标（以原点为中心）
	 * @param Radius 半径
	 * @param Distance 点间距
	 * @return 偏移坐标数组
	 */
	static TArray<FVector2D> GenerateCircleOffsets(float Radius, float Distance);

	/**
	 * 应用随机噪声到点
	 * @param Point 原始点
	 * @param Noise 噪声范围
	 * @return 添加噪声后的点
	 */
	static FVector ApplyNoise(const FVector& Point, float Noise);

	/**
	 * 在三角形内生成随机点（重心坐标采样）
	 * @param V0 三角形顶点0
	 * @param V1 三角形顶点1
	 * @param V2 三角形顶点2
	 * @return 三角形内的随机点
	 */
	static FVector GetRandomPointInTriangle(const FVector& V0, const FVector& V1, const FVector& V2);

	/**
	 * 计算三角形面积
	 * @param V0 三角形顶点0
	 * @param V1 三角形顶点1
	 * @param V2 三角形顶点2
	 * @return 三角形面积
	 */
	static float GetTriangleArea(const FVector& V0, const FVector& V1, const FVector& V2);
};
