// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "SkelotWorld.h"
#include "GameFramework/Character.h"

#include "SkelotCharacter.generated.h"

//a simple integration of Skelot into ACharacter
UCLASS(meta=(DisplayName="Skelot角色"))
class ACharacter_Skelot : public ACharacter
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category="Skelot", meta=(DisplayName="Skelot实例句柄"))
	FSkelotInstanceHandle Handle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skelot", meta=(DisplayName="渲染参数"))
	USkelotRenderParams* RenderParams;

	ACharacter_Skelot();

	UFUNCTION(BlueprintCallable, Category="Skelot|渲染", meta=(DisplayName="设置渲染参数"))
	void SetRenderParams(USkelotRenderParams* Params);
	UFUNCTION(BlueprintCallable, Category="Skelot|渲染", meta=(DisplayName="销毁Skelot实例"))
	void DestroySkelotInstance();

	void Tick(float DeltaSeconds) override;
	void BeginPlay() override;
};