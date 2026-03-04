// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotAssetTypeActions.h"
#include "SkelotAnimCollection.h"
#include "SkelotWorldBase.h"

#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Factories/DataAssetFactory.h"
#include "Misc/MessageDialog.h"

#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"

#define LOCTEXT_NAMESPACE "SkelotAssetTypeActions"

namespace
{
enum class EBatchOverwriteChoice : uint8
{
	AskEach,
	OverwriteAll,
	SkipAll
};

enum class EExistingAssetAction : uint8
{
	Overwrite,
	Skip,
	Abort
};

struct FAssetCreateStats
{
	int32 Created = 0;
	int32 Skipped = 0;
	int32 Failed = 0;
};

EBatchOverwriteChoice ResolveBatchOverwriteChoice(const int32 NumAssets, const FText& PromptText)
{
	if (NumAssets <= 1)
	{
		return EBatchOverwriteChoice::AskEach;
	}

	const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNoCancel, PromptText);
	if (Result == EAppReturnType::Yes)
	{
		return EBatchOverwriteChoice::OverwriteAll;
	}

	if (Result == EAppReturnType::No)
	{
		return EBatchOverwriteChoice::SkipAll;
	}

	return EBatchOverwriteChoice::AskEach;
}

EExistingAssetAction ResolveExistingAssetAction(
	const FString& AssetName,
	EBatchOverwriteChoice BatchChoice,
	const FText& PerAssetPromptText)
{
	if (BatchChoice == EBatchOverwriteChoice::OverwriteAll)
	{
		return EExistingAssetAction::Overwrite;
	}

	if (BatchChoice == EBatchOverwriteChoice::SkipAll)
	{
		return EExistingAssetAction::Skip;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), FText::FromString(AssetName));

	const EAppReturnType::Type Result = FMessageDialog::Open(
		EAppMsgType::YesNoCancel,
		FText::Format(PerAssetPromptText, Args));

	if (Result == EAppReturnType::Yes)
	{
		return EExistingAssetAction::Overwrite;
	}

	if (Result == EAppReturnType::No)
	{
		return EExistingAssetAction::Skip;
	}

	return EExistingAssetAction::Abort;
}

bool DeleteExistingAsset(const FString& FullPackagePath)
{
	return UEditorAssetLibrary::DeleteAsset(FullPackagePath);
}
}

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
		// 使用 FToolMenuEntry 创建菜单项
		Section.AddMenuEntry(
			"Skelot_CreateAnimCollection",
			LOCTEXT("Skelot_CreateAnimCollection", "创建 Skelot 动画集合"),
			LOCTEXT("Skelot_CreateAnimCollection_ToolTip", "从骨骼网格体创建 Skelot 动画集合资产"),
			FSlateIcon(),
			FToolUIActionChoice(FExecuteAction::CreateSP(this, &FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateAnimCollection, AssetPaths))
		);

		Section.AddMenuEntry(
			"Skelot_CreateRenderParams",
			LOCTEXT("Skelot_CreateRenderParams", "创建 Skelot 渲染参数"),
			LOCTEXT("Skelot_CreateRenderParams_ToolTip", "从骨骼网格体创建 Skelot 渲染参数资产"),
			FSlateIcon(),
			FToolUIActionChoice(FExecuteAction::CreateSP(this, &FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateRenderParams, AssetPaths))
		);

		Section.AddMenuEntry(
			"Skelot_CreateAllAssets",
			LOCTEXT("Skelot_CreateAllAssets", "创建所有 Skelot 资产"),
			LOCTEXT("Skelot_CreateAllAssets_ToolTip", "从骨骼网格体同时创建动画集合和渲染参数资产"),
			FSlateIcon(),
			FToolUIActionChoice(FExecuteAction::CreateSP(this, &FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateAllAssets, AssetPaths))
		);
	}
}

void FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateAnimCollection(TArray<FString> AssetPaths)
{
	CreateAnimCollection(AssetPaths);
}

void FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateRenderParams(TArray<FString> AssetPaths)
{
	CreateRenderParams(AssetPaths);
}

void FSkelotAssetTypeActions_SkeletalMesh::ExecuteCreateAllAssets(TArray<FString> AssetPaths)
{
	CreateAllSkelotAssets(AssetPaths);
}

void FSkelotAssetTypeActions_SkeletalMesh::CreateAnimCollection(const TArray<FString>& AssetPaths)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	const EBatchOverwriteChoice BatchChoice = ResolveBatchOverwriteChoice(
		AssetPaths.Num(),
		LOCTEXT("AnimCollectionBatchPrompt", "检测到批量创建动画集合。\n选择“是”将覆盖全部同名资产，选择“否”将跳过全部已存在资产，选择“取消”逐个询问。"));

	FAssetCreateStats Stats;
	bool bAborted = false;

	for (const FString& AssetPath : AssetPaths)
	{
		USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
		if (!SkeletalMesh)
		{
			Stats.Failed++;
			continue;
		}

		// 获取骨骼
		USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
		if (!Skeleton)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("NoSkeleton", "骨骼网格体 '{0}' 没有关联的骨骼资产"),
				FText::FromString(SkeletalMesh->GetName())));
			Stats.Failed++;
			continue;
		}

		// 确定保存路径
		FString PackagePath = FPackageName::GetLongPackagePath(SkeletalMesh->GetOutermost()->GetName());
		FString AssetName = SkeletalMesh->GetName() + TEXT("_AnimCollection");
		FString FullPackagePath = PackagePath / AssetName;

		// 检查是否已存在
		if (UEditorAssetLibrary::DoesAssetExist(FullPackagePath))
		{
			const EExistingAssetAction Action = ResolveExistingAssetAction(
				AssetName,
				BatchChoice,
				LOCTEXT("AnimCollectionAssetExists", "资产 '{AssetName}' 已存在，是否覆盖？"));

			if (Action == EExistingAssetAction::Abort)
			{
				bAborted = true;
				break;
			}

			if (Action == EExistingAssetAction::Skip)
			{
				Stats.Skipped++;
				continue;
			}

			if (!DeleteExistingAsset(FullPackagePath))
			{
				Stats.Failed++;
				continue;
			}
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

			if (UEditorAssetLibrary::SaveLoadedAsset(AnimCollection))
			{
				Stats.Created++;
			}
			else
			{
				Stats.Failed++;
			}
		}
		else
		{
			Stats.Failed++;
		}
	}

	// 显示完成消息
	FFormatNamedArguments Args;
	Args.Add(TEXT("Created"), Stats.Created);
	Args.Add(TEXT("Skipped"), Stats.Skipped);
	Args.Add(TEXT("Failed"), Stats.Failed);
	Args.Add(TEXT("AbortedSuffix"), bAborted ? LOCTEXT("AnimCollectionAbortedSuffix", "（已中止剩余处理）") : FText::GetEmpty());
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(LOCTEXT("AnimCollectionCreated", "动画集合创建完成：成功 {Created}，跳过 {Skipped}，失败 {Failed} {AbortedSuffix}"), Args));
}

void FSkelotAssetTypeActions_SkeletalMesh::CreateRenderParams(const TArray<FString>& AssetPaths)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	const EBatchOverwriteChoice BatchChoice = ResolveBatchOverwriteChoice(
		AssetPaths.Num(),
		LOCTEXT("RenderParamsBatchPrompt", "检测到批量创建渲染参数。\n选择“是”将覆盖全部同名资产，选择“否”将跳过全部已存在资产，选择“取消”逐个询问。"));

	FAssetCreateStats Stats;
	bool bAborted = false;

	for (const FString& AssetPath : AssetPaths)
	{
		USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
		if (!SkeletalMesh)
		{
			Stats.Failed++;
			continue;
		}

		// 确定保存路径
		FString PackagePath = FPackageName::GetLongPackagePath(SkeletalMesh->GetOutermost()->GetName());
		FString AssetName = SkeletalMesh->GetName() + TEXT("_RenderParams");
		FString FullPackagePath = PackagePath / AssetName;

		// 检查是否已存在
		if (UEditorAssetLibrary::DoesAssetExist(FullPackagePath))
		{
			const EExistingAssetAction Action = ResolveExistingAssetAction(
				AssetName,
				BatchChoice,
				LOCTEXT("RenderParamsAssetExists", "资产 '{AssetName}' 已存在，是否覆盖？"));

			if (Action == EExistingAssetAction::Abort)
			{
				bAborted = true;
				break;
			}

			if (Action == EExistingAssetAction::Skip)
			{
				Stats.Skipped++;
				continue;
			}

			if (!DeleteExistingAsset(FullPackagePath))
			{
				Stats.Failed++;
				continue;
			}
		}

		// 创建 RenderParams
		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = USkelotRenderParams::StaticClass();

		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath,
			USkelotRenderParams::StaticClass(), Factory);

		if (USkelotRenderParams* RenderParams = Cast<USkelotRenderParams>(NewAsset))
		{
			const FString AnimCollectionName = SkeletalMesh->GetName() + TEXT("_AnimCollection");
			const FString AnimCollectionPath = PackagePath / AnimCollectionName;
			if (UEditorAssetLibrary::DoesAssetExist(AnimCollectionPath))
			{
				if (USkelotAnimCollection* AnimCollection = Cast<USkelotAnimCollection>(LoadObject<UObject>(nullptr, *AnimCollectionPath)))
				{
					RenderParams->Data.AnimCollection = AnimCollection;
				}
			}

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
			if (UEditorAssetLibrary::SaveLoadedAsset(RenderParams))
			{
				Stats.Created++;
			}
			else
			{
				Stats.Failed++;
			}
		}
		else
		{
			Stats.Failed++;
		}
	}

	// 显示完成消息
	FFormatNamedArguments Args;
	Args.Add(TEXT("Created"), Stats.Created);
	Args.Add(TEXT("Skipped"), Stats.Skipped);
	Args.Add(TEXT("Failed"), Stats.Failed);
	Args.Add(TEXT("AbortedSuffix"), bAborted ? LOCTEXT("RenderParamsAbortedSuffix", "（已中止剩余处理）") : FText::GetEmpty());
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(LOCTEXT("RenderParamsCreated", "渲染参数创建完成：成功 {Created}，跳过 {Skipped}，失败 {Failed} {AbortedSuffix}"), Args));
}

void FSkelotAssetTypeActions_SkeletalMesh::CreateAllSkelotAssets(const TArray<FString>& AssetPaths)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	const EBatchOverwriteChoice BatchChoice = ResolveBatchOverwriteChoice(
		AssetPaths.Num(),
		LOCTEXT("AllAssetsBatchPrompt", "检测到批量创建全部资产。\n选择“是”将覆盖全部同名资产，选择“否”将跳过全部已存在资产，选择“取消”逐个询问。"));

	FAssetCreateStats AnimStats;
	FAssetCreateStats RenderStats;
	bool bAborted = false;

	for (const FString& AssetPath : AssetPaths)
	{
		USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
		if (!SkeletalMesh)
		{
			AnimStats.Failed++;
			RenderStats.Failed++;
			continue;
		}

		USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
		if (!Skeleton)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("NoSkeletonForAll", "骨骼网格体 '{0}' 没有关联的骨骼资产"),
				FText::FromString(SkeletalMesh->GetName())));
			AnimStats.Failed++;
			RenderStats.Failed++;
			continue;
		}

		const FString PackagePath = FPackageName::GetLongPackagePath(SkeletalMesh->GetOutermost()->GetName());
		const FString AnimAssetName = SkeletalMesh->GetName() + TEXT("_AnimCollection");
		const FString RenderAssetName = SkeletalMesh->GetName() + TEXT("_RenderParams");
		const FString AnimFullPath = PackagePath / AnimAssetName;
		const FString RenderFullPath = PackagePath / RenderAssetName;

		USkelotAnimCollection* AnimCollection = nullptr;
		bool bAnimReady = false;

		// 创建/复用 AnimCollection
		if (UEditorAssetLibrary::DoesAssetExist(AnimFullPath))
		{
			const EExistingAssetAction Action = ResolveExistingAssetAction(
				AnimAssetName,
				BatchChoice,
				LOCTEXT("AllAssetsAnimExists", "资产 '{AssetName}' 已存在，是否覆盖？"));

			if (Action == EExistingAssetAction::Abort)
			{
				bAborted = true;
				break;
			}

			if (Action == EExistingAssetAction::Skip)
			{
				AnimCollection = Cast<USkelotAnimCollection>(LoadObject<UObject>(nullptr, *AnimFullPath));
				if (AnimCollection)
				{
					AnimStats.Skipped++;
					bAnimReady = true;
				}
				else
				{
					AnimStats.Failed++;
				}
			}
			else
			{
				if (!DeleteExistingAsset(AnimFullPath))
				{
					AnimStats.Failed++;
				}
				else
				{
					UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
					Factory->DataAssetClass = USkelotAnimCollection::StaticClass();

					UObject* NewAsset = AssetToolsModule.Get().CreateAsset(
						AnimAssetName,
						PackagePath,
						USkelotAnimCollection::StaticClass(),
						Factory);

					AnimCollection = Cast<USkelotAnimCollection>(NewAsset);
					if (AnimCollection)
					{
						AnimCollection->Skeleton = Skeleton;
						FSkelotMeshDef MeshDef;
						MeshDef.Mesh = SkeletalMesh;
						MeshDef.BaseLOD = 0;
						MeshDef.OwningBoundMeshIndex = -1;
						AnimCollection->Meshes.Add(MeshDef);
						AnimCollection->MarkPackageDirty();
						if (UEditorAssetLibrary::SaveLoadedAsset(AnimCollection))
						{
							AnimStats.Created++;
							bAnimReady = true;
						}
						else
						{
							AnimStats.Failed++;
						}
					}
					else
					{
						AnimStats.Failed++;
					}
				}
			}
		}
		else
		{
			UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
			Factory->DataAssetClass = USkelotAnimCollection::StaticClass();

			UObject* NewAsset = AssetToolsModule.Get().CreateAsset(
				AnimAssetName,
				PackagePath,
				USkelotAnimCollection::StaticClass(),
				Factory);

			AnimCollection = Cast<USkelotAnimCollection>(NewAsset);
			if (AnimCollection)
			{
				AnimCollection->Skeleton = Skeleton;
				FSkelotMeshDef MeshDef;
				MeshDef.Mesh = SkeletalMesh;
				MeshDef.BaseLOD = 0;
				MeshDef.OwningBoundMeshIndex = -1;
				AnimCollection->Meshes.Add(MeshDef);
				AnimCollection->MarkPackageDirty();
				if (UEditorAssetLibrary::SaveLoadedAsset(AnimCollection))
				{
					AnimStats.Created++;
					bAnimReady = true;
				}
				else
				{
					AnimStats.Failed++;
				}
			}
			else
			{
				AnimStats.Failed++;
			}
		}

		// 创建/复用 RenderParams
		if (UEditorAssetLibrary::DoesAssetExist(RenderFullPath))
		{
			const EExistingAssetAction Action = ResolveExistingAssetAction(
				RenderAssetName,
				BatchChoice,
				LOCTEXT("AllAssetsRenderExists", "资产 '{AssetName}' 已存在，是否覆盖？"));

			if (Action == EExistingAssetAction::Abort)
			{
				bAborted = true;
				break;
			}

			if (Action == EExistingAssetAction::Skip)
			{
				RenderStats.Skipped++;
				continue;
			}

			if (!DeleteExistingAsset(RenderFullPath))
			{
				RenderStats.Failed++;
				continue;
			}
		}

		UDataAssetFactory* RenderFactory = NewObject<UDataAssetFactory>();
		RenderFactory->DataAssetClass = USkelotRenderParams::StaticClass();

		UObject* NewRenderAsset = AssetToolsModule.Get().CreateAsset(
			RenderAssetName,
			PackagePath,
			USkelotRenderParams::StaticClass(),
			RenderFactory);

		USkelotRenderParams* RenderParams = Cast<USkelotRenderParams>(NewRenderAsset);
		if (!RenderParams)
		{
			RenderStats.Failed++;
			continue;
		}

		FSkelotMeshRenderDesc MeshDesc;
		MeshDesc.Mesh = SkeletalMesh;
		RenderParams->Data.Meshes.Add(MeshDesc);
		RenderParams->Data.AnimCollection = bAnimReady ? AnimCollection : nullptr;
		RenderParams->Data.bRenderInMainPass = true;
		RenderParams->Data.bRenderInDepthPass = true;
		RenderParams->Data.bCastDynamicShadow = true;
		RenderParams->Data.BoundsScale = 1.0f;
		RenderParams->MarkPackageDirty();

		if (UEditorAssetLibrary::SaveLoadedAsset(RenderParams))
		{
			RenderStats.Created++;
		}
		else
		{
			RenderStats.Failed++;
		}
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("AnimCreated"), AnimStats.Created);
	Args.Add(TEXT("AnimSkipped"), AnimStats.Skipped);
	Args.Add(TEXT("AnimFailed"), AnimStats.Failed);
	Args.Add(TEXT("RenderCreated"), RenderStats.Created);
	Args.Add(TEXT("RenderSkipped"), RenderStats.Skipped);
	Args.Add(TEXT("RenderFailed"), RenderStats.Failed);
	Args.Add(TEXT("AbortedSuffix"), bAborted ? LOCTEXT("AllAssetsAbortedSuffix", "（已中止剩余处理）") : FText::GetEmpty());

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("AllAssetsCreatedSummary", "全部资产创建完成：\nAnimCollection 成功 {AnimCreated}，跳过 {AnimSkipped}，失败 {AnimFailed}\nRenderParams 成功 {RenderCreated}，跳过 {RenderSkipped}，失败 {RenderFailed} {AbortedSuffix}"),
			Args));
}

#undef LOCTEXT_NAMESPACE
