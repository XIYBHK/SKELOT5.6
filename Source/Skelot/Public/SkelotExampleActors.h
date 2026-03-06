// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkelotSubsystem.h"
#include "SkelotExampleActors.generated.h"

class USkelotRenderParams;
class UAnimSequence;

/**
 * Skelot 基础实例创建示例
 *
 * 演示内容：
 * 1. 创建单个实例 - CreateInstance
 * 2. 批量创建实例 - CreateInstances
 * 3. 销毁实例 - DestroyInstance
 * 4. 播放动画 - PlayAnimation
 * 5. 获取/设置变换 - GetTransform/SetTransform
 *
 * 使用方法：
 * 1. 将此 Actor 放置到场景中
 * 2. 设置 RenderParams 资产
 * 3. 运行游戏，按键盘键位触发不同功能演示
 */
UCLASS(meta = (DisplayName = "Skelot 示例: 基础实例"))
class SKELOT_API ASkelotExampleBasicInstance : public AActor
{
	GENERATED_BODY()

public:
	ASkelotExampleBasicInstance();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 渲染参数资产 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "渲染参数"))
	USkelotRenderParams* RenderParams;

	/** 要播放的动画（可选） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "动画序列"))
	UAnimSequence* AnimationToPlay;

	/** 是否在开始时自动创建实例 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "开始时自动创建"))
	bool bAutoCreateOnBeginPlay = true;

	/** 自动创建的实例数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "实例数量", ClampMin = "1", ClampMax = "1000"))
	int32 InstanceCount = 10;

	/** 实例生成范围 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "生成范围"))
	float SpawnRadius = 500.0f;

	/** 是否循环播放动画 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "循环动画"))
	bool bLoopAnimation = true;

	/** 动画播放速率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "播放速率"))
	float PlayRate = 1.0f;

	/** 创建实例的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "创建实例键位"))
	FKey CreateInstanceKey = EKeys::One;

	/** 销毁实例的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "销毁实例键位"))
	FKey DestroyInstanceKey = EKeys::Two;

	/** 播放动画的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "播放动画键位"))
	FKey PlayAnimationKey = EKeys::Three;

	/** 切换动画暂停的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "暂停动画键位"))
	FKey TogglePauseKey = EKeys::Four;

	/** 清除所有实例的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "清除所有键位"))
	FKey ClearAllKey = EKeys::Five;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/**
	 * 创建单个实例
	 * 在指定位置创建一个 Skelot 实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "创建单个实例"))
	FSkelotInstanceHandle CreateSingleInstance(const FVector& Location);

	/**
	 * 批量创建实例
	 * 在圆形区域内批量创建多个实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "批量创建实例"))
	void CreateBatchInstances(int32 Count, const FVector& Center, float Radius);

	/**
	 * 销毁指定实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "销毁实例"))
	void DestroyInstanceByHandle(FSkelotInstanceHandle Handle);

	/**
	 * 销毁所有实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "销毁所有实例"))
	void DestroyAllInstances();

	/**
	 * 播放动画
	 * 为所有实例播放指定动画
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "播放动画"))
	void PlayAnimationForAllInstances();

	/**
	 * 切换动画暂停状态
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "切换暂停"))
	void ToggleAnimationPaused();

	/**
	 * 获取当前实例数量
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot|示例", meta = (DisplayName = "获取实例数量"))
	int32 GetActiveInstanceCount() const { return InstanceHandles.Num(); }

private:
	/** 已创建的实例句柄 */
	UPROPERTY(Transient)
	TArray<FSkelotInstanceHandle> InstanceHandles;

	/** 动画是否暂停 */
	bool bAnimationPaused = false;

	/** 处理键盘输入 */
	void HandleKeyboardInput();
};


/**
 * Skelot 碰撞避让示例
 *
 * 演示内容：
 * 1. PBD 碰撞系统 - 实例间碰撞避让
 * 2. RVO 避障系统 - 智能路径规划
 * 3. 群体移动 - 多实例同时移动
 * 4. 目标追踪 - 实例追踪目标点
 *
 * 使用方法：
 * 1. 将此 Actor 放置到场景中
 * 2. 设置 RenderParams 资产
 * 3. 运行游戏，实例会自动向目标点移动并相互避让
 */
UCLASS(meta = (DisplayName = "Skelot 示例: 碰撞避让"))
class SKELOT_API ASkelotExampleCollisionAvoidance : public AActor
{
	GENERATED_BODY()

public:
	ASkelotExampleCollisionAvoidance();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 渲染参数资产 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "渲染参数"))
	USkelotRenderParams* RenderParams;

	/** 行走动画 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "行走动画"))
	UAnimSequence* WalkAnimation;

	/** 待机动画 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "待机动画"))
	UAnimSequence* IdleAnimation;

	/** 实例数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "实例数量", ClampMin = "10", ClampMax = "5000"))
	int32 InstanceCount = 100;

	/** 实例生成范围 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "生成范围"))
	float SpawnRadius = 1000.0f;

	/** 移动速度（厘米/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "移动速度"))
	float MoveSpeed = 200.0f;

	/** 目标点 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "目标点", MakeEditWidget = true))
	FVector TargetLocation = FVector(0, 0, 0);

	/** 到达目标后的行为 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
	uint8 ArrivalBehavior = 0; // 0=Stop, 1=WaitAndReverse, 2=Loop

	/** 等待时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "等待时间"))
	float WaitDuration = 2.0f;

	/** 是否启用PBD碰撞 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD配置", meta = (DisplayName = "启用PBD碰撞"))
	bool bEnablePBD = true;

	/** PBD碰撞半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD配置", meta = (DisplayName = "碰撞半径", ClampMin = "20", ClampMax = "200"))
	float PBDCollisionRadius = 60.0f;

	/** 是否启用RVO避障 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO配置", meta = (DisplayName = "启用RVO避障"))
	bool bEnableRVO = true;

	/** RVO邻居半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO配置", meta = (DisplayName = "邻居半径", ClampMin = "100", ClampMax = "800"))
	float RVONeighborRadius = 300.0f;

	/** 重置场景的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "重置键位"))
	FKey ResetKey = EKeys::R;

	/** 切换PBD的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "切换PBD键位"))
	FKey TogglePBDKey = EKeys::P;

	/** 切换RVO的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "切换RVO键位"))
	FKey ToggleRVOKey = EKeys::O;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/**
	 * 初始化实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "初始化实例"))
	void InitializeInstances();

	/**
	 * 重置场景
	 * 销毁所有实例并重新创建
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "重置场景"))
	void ResetScene();

	/**
	 * 切换PBD碰撞
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "切换PBD"))
	void TogglePBD();

	/**
	 * 切换RVO避障
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "切换RVO"))
	void ToggleRVO();

	/**
	 * 设置新的目标点
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "设置目标点"))
	void SetNewTarget(const FVector& NewTarget);

	/**
	 * 获取到达目标的实例数量
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot|示例", meta = (DisplayName = "获取到达数量"))
	int32 GetArrivedCount() const { return ArrivedCount; }

private:
	/** 实例数据 */
	struct FInstanceData
	{
		FSkelotInstanceHandle Handle;
		FVector StartLocation;
		FVector CurrentTarget;
		bool bArrived;
		float WaitTimer;
		bool bIsWalkAnimationPlaying = false;
		bool bIsIdleAnimationPlaying = false;
	};

	/** 实例数据数组（不使用UPROPERTY，纯C++容器） */
	TArray<FInstanceData> InstanceDataArray;

	/** 到达目标的实例数量 */
	int32 ArrivedCount = 0;

	/** SkelotWorld 引用 */
	ASkelotWorld* SkelotWorld = nullptr;

	/** 处理键盘输入 */
	void HandleKeyboardInput();

	/** 更新实例移动 */
	void UpdateInstanceMovement(float DeltaTime);

	/** 应用PBD/RVO配置 */
	void ApplyConfigs();
};


/**
 * Skelot 几何工具示例
 *
 * 演示内容：
 * 1. GetPointsByRound - 圆形点阵
 * 2. GetPointsByGrid - 网格点阵
 * 3. GetPointsByShape - 形状点阵
 * 4. GetBezierPoint - 贝塞尔曲线
 *
 * 使用方法：
 * 1. 将此 Actor 放置到场景中
 * 2. 设置 RenderParams 资产
 * 3. 运行游戏，按键盘键位切换不同生成模式
 */
UCLASS(meta = (DisplayName = "Skelot 示例: 几何工具"))
class SKELOT_API ASkelotExampleGeometryTools : public AActor
{
	GENERATED_BODY()

public:
	ASkelotExampleGeometryTools();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 渲染参数资产 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "渲染参数"))
	USkelotRenderParams* RenderParams;

	/** 生成模式 (0=Round, 1=Grid2D, 2=Grid3D, 3=Bezier) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
	uint8 SpawnPattern = 0;

	/** 圆形参数 - 半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "圆形", meta = (DisplayName = "半径"))
	float RoundRadius = 500.0f;

	/** 圆形参数 - 点间距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "圆形", meta = (DisplayName = "点间距"))
	float RoundSpacing = 100.0f;

	/** 网格参数 - X方向数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网格", meta = (DisplayName = "X数量"))
	int32 GridCountX = 10;

	/** 网格参数 - Y方向数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网格", meta = (DisplayName = "Y数量"))
	int32 GridCountY = 10;

	/** 网格参数 - Z方向数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网格", meta = (DisplayName = "Z数量"))
	int32 GridCountZ = 1;

	/** 网格参数 - X方向间距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网格", meta = (DisplayName = "X间距"))
	float GridSpacingX = 100.0f;

	/** 网格参数 - Y方向间距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网格", meta = (DisplayName = "Y间距"))
	float GridSpacingY = 100.0f;

	/** 网格参数 - Z方向间距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网格", meta = (DisplayName = "Z间距"))
	float GridSpacingZ = 100.0f;

	/** 贝塞尔曲线控制点 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "贝塞尔", meta = (DisplayName = "控制点"))
	TArray<FVector> BezierControlPoints;

	/** 贝塞尔曲线采样数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "贝塞尔", meta = (DisplayName = "采样数量"))
	int32 BezierSampleCount = 20;

	/** 切换模式的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "切换模式键位"))
	FKey TogglePatternKey = EKeys::Tab;

	/** 清除实例的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "清除键位"))
	FKey ClearKey = EKeys::C;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/**
	 * 使用当前模式生成实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "生成实例"))
	void SpawnInstancesWithCurrentPattern();

	/**
	 * 清除所有实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "清除实例"))
	void ClearInstances();

	/**
	 * 切换到下一个生成模式
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|示例", meta = (DisplayName = "切换模式"))
	void NextPattern();

	/**
	 * 获取当前模式名称
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot|示例", meta = (DisplayName = "获取模式名称"))
	FString GetCurrentPatternName() const;

private:
	/** 已创建的实例句柄 */
	UPROPERTY(Transient)
	TArray<FSkelotInstanceHandle> InstanceHandles;

	/** 处理键盘输入 */
	void HandleKeyboardInput();

	/** 使用圆形模式生成 */
	void SpawnWithRoundPattern();

	/** 使用网格模式生成 */
	void SpawnWithGridPattern();

	/** 使用贝塞尔曲线模式生成 */
	void SpawnWithBezierPattern();
};


/**
 * Skelot 性能压力测试
 *
 * 测试内容：
 * 1. 4-5w 实例渲染性能
 * 2. PBD 碰撞系统性能
 * 3. RVO 避障系统性能
 * 4. 空间查询性能
 *
 * 使用方法：
 * 1. 将此 Actor 放置到场景中
 * 2. 设置 RenderParams 资产
 * 3. 运行游戏，按键盘键位切换测试模式
 * 4. 观察 HUD 上的性能指标
 *
 * 参考性能（8000面数4级LOD骨骼模型）：
 * - 4-5w 实例移动（开启碰撞）
 * - 540x540，无抗锯齿：50-70s/2000帧
 * - 1920x1920，3倍MSAA：5-6min/2000帧
 */
UCLASS(meta = (DisplayName = "Skelot 示例: 性能测试"))
class SKELOT_API ASkelotExampleStressTest : public AActor
{
	GENERATED_BODY()

public:
	ASkelotExampleStressTest();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 渲染参数资产 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "渲染参数"))
	USkelotRenderParams* RenderParams;

	/** 行走动画 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "行走动画"))
	UAnimSequence* WalkAnimation;

	/** 实例数量 (推荐 40000-50000) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "实例数量", ClampMin = "1000", ClampMax = "100000"))
	int32 InstanceCount = 40000;

	/** 实例生成范围半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "生成范围"))
	float SpawnRadius = 5000.0f;

	/** 测试模式 (0=静态, 1=移动, 2=移动+PBD, 3=移动+PBD+RVO) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
	uint8 TestMode = 3;

	/** 移动速度（厘米/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置", meta = (DisplayName = "移动速度"))
	float MoveSpeed = 200.0f;

	/** PBD 碰撞半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD配置", meta = (DisplayName = "碰撞半径"))
	float PBDCollisionRadius = 60.0f;

	/** RVO 邻居半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO配置", meta = (DisplayName = "邻居半径"))
	float RVONeighborRadius = 300.0f;

	/** PBD 迭代次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD配置", meta = (DisplayName = "迭代次数", ClampMin = "1", ClampMax = "8"))
	int32 PBDIterationCount = 3;

	/** PBD 最大邻居数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBD配置", meta = (DisplayName = "最大邻居数", ClampMin = "8", ClampMax = "256"))
	int32 PBDMaxNeighbors = 64;

	/** RVO 最大邻居数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO配置", meta = (DisplayName = "最大邻居数", ClampMin = "4", ClampMax = "64"))
	int32 RVOMaxNeighbors = 16;

	/** RVO 分帧步长 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO配置", meta = (DisplayName = "分帧步长", ClampMin = "1", ClampMax = "8"))
	int32 RVOFrameStride = 1;

	/** 是否显示HUD统计 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试", meta = (DisplayName = "显示HUD"))
	bool bShowHUD = true;

	/** 开始测试时是否自动启动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "测试流程", meta = (DisplayName = "自动开始测试"))
	bool bAutoStartOnBeginPlay = false;

	/** 预热时长（秒），预热阶段不计入最终统计 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "测试流程", meta = (DisplayName = "预热时长", ClampMin = "0.0", ClampMax = "120.0"))
	float WarmupSeconds = 3.0f;

	/** 正式采样时长（秒），到时自动停止测试；<=0 表示不自动停止 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "测试流程", meta = (DisplayName = "采样时长", ClampMin = "0.0", ClampMax = "3600.0"))
	float MeasurementSeconds = 30.0f;

	/** 是否启用 LOD 更新频率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD配置", meta = (DisplayName = "启用LOD更新频率"))
	bool bEnableLODUpdateFrequency = false;

	/** LOD 中距离阈值（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD配置", meta = (DisplayName = "中距离阈值", ClampMin = "100.0"))
	float LODMediumDistance = 2000.0f;

	/** LOD 远距离阈值（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD配置", meta = (DisplayName = "远距离阈值", ClampMin = "200.0"))
	float LODFarDistance = 5000.0f;

	/** HUD文字大小 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试", meta = (DisplayName = "HUD文字大小"))
	float HUDTextSize = 1.5f;

	/** 方向切换间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "移动", meta = (DisplayName = "方向切换间隔", ClampMin = "0.1", ClampMax = "60.0"))
	float DirectionChangeInterval = 5.0f;

	/** 随机种子（保证可复现实验） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "移动", meta = (DisplayName = "随机种子"))
	int32 RandomSeed = 12345;

	/** 开始测试的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "开始测试键位"))
	FKey StartTestKey = EKeys::Enter;

	/** 停止测试的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "停止测试键位"))
	FKey StopTestKey = EKeys::Escape;

	/** 切换测试模式的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "切换模式键位"))
	FKey ToggleModeKey = EKeys::M;

	/** 切换HUD显示的键盘键位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "交互", meta = (DisplayName = "切换HUD键位"))
	FKey ToggleHUDKey = EKeys::H;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/**
	 * 开始性能测试
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|测试", meta = (DisplayName = "开始测试"))
	void StartTest();

	/**
	 * 停止测试并清理
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|测试", meta = (DisplayName = "停止测试"))
	void StopTest();

	/**
	 * 切换测试模式
	 */
	UFUNCTION(BlueprintCallable, Category = "Skelot|测试", meta = (DisplayName = "切换模式"))
	void CycleTestMode();

	/**
	 * 获取当前FPS
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot|测试", meta = (DisplayName = "获取FPS"))
	float GetCurrentFPS() const { return CurrentFPS; }

	/**
	 * 获取平均FPS
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot|测试", meta = (DisplayName = "获取平均FPS"))
	float GetAverageFPS() const { return AverageFPS; }

	/**
	 * 获取最低FPS
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot|测试", meta = (DisplayName = "获取最低FPS"))
	float GetMinFPS() const { return MinFPS; }

	/**
	 * 获取最高FPS
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot|测试", meta = (DisplayName = "获取最高FPS"))
	float GetMaxFPS() const { return MaxFPS; }

	/**
	 * 获取当前实例数量
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot|测试", meta = (DisplayName = "获取实例数量"))
	int32 GetActiveInstanceCount() const { return InstanceHandles.Num(); }

	/**
	 * 获取测试模式名称
	 */
	UFUNCTION(BlueprintPure, Category = "Skelot|测试", meta = (DisplayName = "获取模式名称"))
	FString GetTestModeName() const;

private:
	/** 实例句柄数组 */
	UPROPERTY(Transient)
	TArray<FSkelotInstanceHandle> InstanceHandles;

	/** SkelotWorld 引用 */
	ASkelotWorld* SkelotWorld = nullptr;

	/** 测试是否运行中 */
	bool bTestRunning = false;

	/** 测试开始时间 */
	double TestStartTime = 0.0;

	/** 帧计数 */
	int64 FrameCount = 0;

	/** FPS 统计 */
	float CurrentFPS = 0.0f;
	float AverageFPS = 0.0f;
	float MinFPS = FLT_MAX;
	float MaxFPS = 0.0f;
	float FPSAccumulator = 0.0f;
	int32 FPSFrameCount = 0;

	/** 帧时间统计 */
	float FrameTimeMs = 0.0f;
	float AverageFrameTimeMs = 0.0f;

	/** 上一次帧时间 */
	double LastFrameTime = 0.0;

	/** 是否处于预热阶段 */
	bool bInWarmup = false;

	/** 预热开始时间 */
	double WarmupStartTime = 0.0;

	/** 正式采样开始时间 */
	double MeasurementStartTime = 0.0;

	/** 方向切换计时器 */
	float DirectionChangeTimer = 0.0f;

	/** 当前方向数组 */
	TArray<FVector> TargetDirections;

	/** 处理键盘输入 */
	void HandleKeyboardInput();

	/** 更新FPS统计 */
	void UpdateFPSStats(float DeltaTime);

	/** 绘制HUD */
	void DrawHUD();

	/** 应用配置 */
	void ApplyConfigs();

	/** 更新实例移动 */
	void UpdateInstanceMovement(float DeltaTime);

	/** 重置统计数据 */
	void ResetStats();

	/** 重新生成移动方向 */
	void RegenerateTargetDirections();
};
