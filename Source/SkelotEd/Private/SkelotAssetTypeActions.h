// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FSkelotAssetTypeActions_SkeletalMesh : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section) override;
	// End of IAssetTypeActions interface

private:
	/** 创建 Skelot 动画集合 */
	void CreateAnimCollection(TArray<FString> AssetPaths);
	/** 创建 Skelot 渲染参数 */
	void CreateRenderParams(TArray<FString> AssetPaths);
	/** 创建所有 Skelot 资产 (动画集合 + 渲染参数) */
	void CreateAllSkelotAssets(TArray<FString> AssetPaths);

	/** 执行创建 AnimCollection */
	void ExecuteCreateAnimCollection(const TArray<FString> AssetPaths);
	/** 执行创建 RenderParams */
	void ExecuteCreateRenderParams(const TArray<FString> AssetPaths);
	/** 执行创建所有资产 */
	void ExecuteCreateAllAssets(const TArray<FString> AssetPaths);
};
