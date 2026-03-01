// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotAssetTypeActions.h"
#include "SkelotAnimCollection.h"
#include "SkelotWorldBase.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Factories/DataAssetFactory.h"

#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"

#define LOCTEXT_NAMESPACE "SkelotAssetTypeActions"

FText FSkelotAssetTypeActions_SkeletalMesh::GetName() const
{
	return LOCTEXT("AssetTypeActions_SkeletalMesh", "Skeletal Mesh");
}

FColor FSkelotAssetTypeActions_SkeletalMesh::GetTypeColor() const
{
	return FColor(128, 128, 255);
}

UClass* FSkelotAssetTypeActions_SkeletalMesh::GetSupportedClass() const
{
	return USkeletalMesh::StaticClass();
}

uint32 FSkelotAssetTypeActions_SkeletalMesh::GetCategories()
{
	return EAssetTypeCategories::Animation;
}

void FSkelotAssetTypeActions_SkeletalMesh::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<FString> AssetPaths;
	for (UObject* Object : InObjects)
	{
		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
		{
			AssetPaths.Add(Object->GetPathName());
		}
	}

	if (AssetPaths.Num() > 0)
	{
		Section.AddMenuEntry(
			"Skelot_CreateAnimCollection",
			LOCTEXT("Skelot_CreateAnimCollection", "创建 Skelot 动画集合"),
			LOCTEXT("Skelot_CreateAnimCollection_ToolTip", "从骨骼网格体创建 Skelot 动画集合资产"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateAnimCollection, AssetPaths))
		);

		Section.AddMenuEntry(
			"Skelot_CreateRenderParams",
			LOCTEXT("Skelot_CreateRenderParams", "创建 Skelot 渲染参数"),
			LOCTEXT("Skelot_CreateRenderParams_ToolTip", "从骨骼网格体创建 Skelot 渲染参数资产"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateRenderParams, AssetPaths))
		);

		Section.AddMenuEntry(
			"Skelot_CreateAllAssets",
			LOCTEXT("Skelot_CreateAllAssets", "创建所有 Skelot 资产"),
			LOCTEXT("Skelot_CreateAllAssets_ToolTip", "从骨骼网格体同时创建动画集合和渲染参数资产"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateAllAssets, AssetPaths))
		);
	}
}

void FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateAnimCollection(const TArray<FString> AssetPaths)
{
	CreateAnimCollection(AssetPaths);
}

void FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateRenderParams(const TArray<FString> AssetPaths)
{
	CreateRenderParams(AssetPaths);
}

void FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateAllAssets(const TArray<FString> AssetPaths)
{
	CreateAllSkelotAssets(AssetPaths);
}

void FSkelotAssetTypeActions_SkeletalMesh::CreateAnimCollection(TArray<FString> AssetPaths)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	int32 CreatedCount = 0;

	for (const FString& AssetPath : AssetPaths)
	{
		USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
		if (!SkeletalMesh)
		{
			continue;
		}

		// 获取骨骼
		USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
		if (!Skeleton)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("NoSkeleton", "骨骼网格体 '{0}' 没有关联的骨骼资产"),
				FText::FromString(SkeletalMesh->GetName())));
			continue;
		}

		// 确定保存路径
		FString PackagePath = FPackageName::GetLongPackagePath(SkeletalMesh->GetOutermost()->GetName());
		FString AssetName = SkeletalMesh->GetName() + TEXT("_AnimCollection");
		FString FullPackagePath = PackagePath / AssetName;

		// 检查是否已存在
		if (UEditorAssetLibrary::DoesAssetExist(FullPackagePath))
		{
			// 资产已存在，询问是否覆盖
			FFormatNamedArguments Args;
			Args.Add(TEXT("AssetName"), FText::FromString(AssetName));
			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNoCancel,
				FText::Format(LOCTEXT("AssetExists", "资产 '{0}' 已存在，是否覆盖？"), Args));

			if (Result == EAppReturnType::Cancel)
			{
				continue;
			}
			else if (Result == EAppReturnType::No)
			{
				continue;
			}
			// Yes - 删除现有资产
			UEditorAssetLibrary::DeleteAsset(FullPackagePath);
		}

		// 创建 AnimCollection
		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = USkelotAnimCollection::StaticClass();

		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath,
			USkelotAnimCollection::StaticClass(), Factory);

		if (USkelotAnimCollection* AnimCollection = Cast<USkelotAnimCollection>(NewAsset))
		{
			// 设置骨骼
			AnimCollection->Skeleton = Skeleton;

			// 添加网格
			FSkelotMeshDef MeshDef;
			MeshDef.Mesh = SkeletalMesh;
			MeshDef.BaseLOD = 0;
			MeshDef.OwningBoundMeshIndex = -1;
			AnimCollection->Meshes.Add(MeshDef);

			// 标记为需要重新构建
			AnimCollection->MarkPackageDirty();

			CreatedCount++;

			// 保存资产
			UEditorAssetLibrary::SaveLoadedAsset(AnimCollection);
		}
	}

	// 显示完成消息
	FFormatNamedArguments Args;
	Args.Add(TEXT("Count"), CreatedCount);
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(LOCTEXT("AnimCollectionCreated", "成功创建 {Count} 个 Skelot 动画集合"), Args));
}

void FSkelotAssetTypeActions_SkeletalMesh::CreateRenderParams(TArray<FString> AssetPaths)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	int32 CreatedCount = 0;

	for (const FString& AssetPath : AssetPaths)
	{
		USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
		if (!SkeletalMesh)
		{
			continue;
		}

		// 确定保存路径
		FString PackagePath = FPackageName::GetLongPackagePath(SkeletalMesh->GetOutermost()->GetName());
		FString AssetName = SkeletalMesh->GetName() + TEXT("_RenderParams");
		FString FullPackagePath = PackagePath / AssetName;

		// 检查是否已存在
		if (UEditorAssetLibrary::DoesAssetExist(FullPackagePath))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("AssetName"), FText::FromString(AssetName));
			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNoCancel,
				FText::Format(LOCTEXT("AssetExists", "资产 '{0}' 已存在，是否覆盖？"), Args));

			if (Result == EAppReturnType::Cancel || Result == EAppReturnType::No)
			{
				continue;
			}
			// Yes - 删除现有资产
			UEditorAssetLibrary::DeleteAsset(FullPackagePath);
		}

		// 创建 RenderParams
		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = USkelotRenderParams::StaticClass();

		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath,
			USkelotRenderParams::StaticClass(), Factory);

		if (USkelotRenderParams* RenderParams = Cast<USkelotRenderParams>(NewAsset))
		{
			// 添加网格到渲染描述
			FSkelotMeshRenderDesc MeshDesc;
			MeshDesc.Mesh = SkeletalMesh;
			RenderParams->Data.Meshes.Add(MeshDesc);

			// 设置默认值
			RenderParams->Data.bRenderInMainPass = true;
			RenderParams->Data.bRenderInDepthPass = true;
			RenderParams->Data.bCastDynamicShadow = true;
			RenderParams->Data.BoundsScale = 1.0f;

			RenderParams->MarkPackageDirty();
			CreatedCount++;

			// 保存资产
			UEditorAssetLibrary::SaveLoadedAsset(RenderParams);
		}
	}

	// 显示完成消息
	FFormatNamedArguments Args;
	Args.Add(TEXT("Count"), CreatedCount);
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(LOCTEXT("RenderParamsCreated", "成功创建 {Count} 个 Skelot 渲染参数"), Args));
}

void FSkelotAssetTypeActions_SkeletalMesh::CreateAllSkelotAssets(TArray<FString> AssetPaths)
{
	// 先创建 AnimCollection
	CreateAnimCollection(AssetPaths);

	// 再创建 RenderParams
	CreateRenderParams(AssetPaths);
}

#undef LOCTEXT_NAMESPACE
