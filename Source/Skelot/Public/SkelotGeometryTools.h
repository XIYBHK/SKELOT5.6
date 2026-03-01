// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkelotGeometryTools.generated.h"

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
};
