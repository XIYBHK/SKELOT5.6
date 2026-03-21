// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkelotObstacle.generated.h"

/**
 * Skelot 障碍物类型枚举
 */
UENUM(BlueprintType)
enum class ESkelotObstacleType : uint8
{
	Sphere	UMETA(DisplayName = "球形"),
	Box		UMETA(DisplayName = "盒形"),
};

/**
 * Skelot 障碍物基类
 *
 * 用于 PBD 碰撞系统的静态障碍物，实例会被障碍物推开。
 * 放置到场景中后，障碍物会自动注册到 SkelotWorld。
 *
 * 使用方法：
 * 1. 将 SkelotSphereObstacle 或 SkelotBoxObstacle 放置到场景中
 * 2. 调整障碍物的位置、大小
 * 3. 障碍物会在运行时自动参与 PBD 碰撞计算
 */
UCLASS(Abstract, meta = (DisplayName = "Skelot障碍物"))
class SKELOT_API ASkelotObstacle : public AActor
{
	GENERATED_BODY()

public:
	ASkelotObstacle();

	/** 障碍物类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "障碍物", meta = (DisplayName = "障碍物类型"))
	ESkelotObstacleType ObstacleType;

	/** 是否启用障碍物 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "障碍物", meta = (DisplayName = "启用"))
	bool bEnabled = true;

	/** 碰撞通道掩码 - 控制与哪些碰撞通道的实例进行碰撞 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "障碍物", meta = (DisplayName = "碰撞掩码"), Meta = (Bitmask, BitmaskEnum = "/Script/Skelot.SkelotCollisionChannel"))
	uint8 CollisionMask = 0xFF;

	/** 额外的碰撞半径偏移（厘米）- 在基础形状上额外扩展 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "障碍物", meta = (DisplayName = "半径偏移"))
	float RadiusOffset = 0.0f;

	/** 是否显示调试可视化 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试", meta = (DisplayName = "显示调试"))
	bool bShowDebug = false;

	/**
	 * 获取障碍物的世界位置
	 */
	UFUNCTION(BlueprintPure, Category = "障碍物")
	FVector GetObstacleLocation() const { return GetActorLocation(); }

	/**
	 * 获取障碍物的世界旋转
	 */
	UFUNCTION(BlueprintPure, Category = "障碍物")
	FQuat GetObstacleRotation() const { return GetActorQuat(); }

	/**
	 * 获取球形障碍物的半径（对于盒形障碍物返回等效半径）
	 * 子类需要重写此方法
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "障碍物")
	float GetObstacleRadius() const;
	virtual float GetObstacleRadius_Implementation() const;

	/**
	 * 检查点是否在障碍物内部
	 * @param Point 世界坐标点
	 * @return true 如果点在障碍物内部
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "障碍物")
	bool IsPointInside(const FVector& Point) const;
	virtual bool IsPointInside_Implementation(const FVector& Point) const;

	/**
	 * 计算点到障碍物表面的最近点和距离
	 * @param Point 世界坐标点
	 * @param OutClosestPoint 输出最近点
	 * @return 点到障碍物表面的距离（负数表示在内部）
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "障碍物")
	float GetDistanceToPoint(const FVector& Point, FVector& OutClosestPoint) const;
	virtual float GetDistanceToPoint_Implementation(const FVector& Point, FVector& OutClosestPoint) const;

	/**
	 * 计算碰撞响应（推力方向和大小）
	 * @param InstanceLocation 实例位置
	 * @param InstanceRadius 实例碰撞半径
	 * @param OutPushDirection 输出推力方向
	 * @param OutPushMagnitude 输出推力大小
	 * @return true 如果需要推开
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "障碍物")
	bool ComputeCollisionResponse(const FVector& InstanceLocation, float InstanceRadius,
								  FVector& OutPushDirection, float& OutPushMagnitude) const;
	virtual bool ComputeCollisionResponse_Implementation(const FVector& InstanceLocation, float InstanceRadius,
														 FVector& OutPushDirection, float& OutPushMagnitude) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 上一帧的 Transform 缓存，用于检测运行时移动 */
	FTransform CachedTransform;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
#endif

public:
	virtual void Tick(float DeltaTime) override;

	/** 获取调试绘制颜色 */
	FColor GetDebugColor() const;
};

/**
 * Skelot 球形障碍物
 *
 * 使用球形碰撞体的障碍物，适用于圆形柱子、树木等
 */
UCLASS(meta = (DisplayName = "Skelot球形障碍物"))
class SKELOT_API ASkelotSphereObstacle : public ASkelotObstacle
{
	GENERATED_BODY()

public:
	ASkelotSphereObstacle();

	/** 球形半径（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "障碍物", meta = (DisplayName = "半径", ClampMin = "1", ClampMax = "5000"))
	float Radius = 100.0f;

	//~ ASkelotObstacle 接口
	virtual float GetObstacleRadius_Implementation() const override;
	virtual bool IsPointInside_Implementation(const FVector& Point) const override;
	virtual float GetDistanceToPoint_Implementation(const FVector& Point, FVector& OutClosestPoint) const override;
	virtual bool ComputeCollisionResponse_Implementation(const FVector& InstanceLocation, float InstanceRadius,
														 FVector& OutPushDirection, float& OutPushMagnitude) const override;
	//~ ASkelotObstacle 接口结束

#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#endif

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
};

/**
 * Skelot 盒形障碍物
 *
 * 使用盒形碰撞体的障碍物，适用于墙壁、建筑等
 */
UCLASS(meta = (DisplayName = "Skelot盒形障碍物"))
class SKELOT_API ASkelotBoxObstacle : public ASkelotObstacle
{
	GENERATED_BODY()

public:
	ASkelotBoxObstacle();

	/** 盒子半尺寸（厘米） - X/Y/Z 方向 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "障碍物", meta = (DisplayName = "半尺寸", ClampMin = "1", ClampMax = "5000"))
	FVector BoxExtent = FVector(100.0f, 100.0f, 200.0f);

	//~ ASkelotObstacle 接口
	virtual float GetObstacleRadius_Implementation() const override;
	virtual bool IsPointInside_Implementation(const FVector& Point) const override;
	virtual float GetDistanceToPoint_Implementation(const FVector& Point, FVector& OutClosestPoint) const override;
	virtual bool ComputeCollisionResponse_Implementation(const FVector& InstanceLocation, float InstanceRadius,
														 FVector& OutPushDirection, float& OutPushMagnitude) const override;
	//~ ASkelotObstacle 接口结束

#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#endif

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	/** 将点从世界空间变换到障碍物本地空间 */
	FVector WorldToLocal(const FVector& WorldPoint) const;

	/** 将点从本地空间变换到世界空间 */
	FVector LocalToWorld(const FVector& LocalPoint) const;

	/** 计算点到轴对齐盒子的最近点 */
	static FVector ClosestPointOnAABB(const FVector& Point, const FVector& BoxMin, const FVector& BoxMax);
};
