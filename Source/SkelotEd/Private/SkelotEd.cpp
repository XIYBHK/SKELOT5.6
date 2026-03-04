// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotEd.h"

#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "PropertyEditorModule.h"

#include "Animation/AnimationPoseData.h"
#include "PropertyEditorDelegates.h"
#include "SkelotThumbnailRenderer.h"
#include "SkelotAnimCollection.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "AssetToolsModule.h"
#include "SkelotAssetTypeActions.h"

#define LOCTEXT_NAMESPACE "FSkelotEdModule"

// 注册的资产类型操作列表
static TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;

// void FSkelotModuleEd::StartupModule()
// {
// 	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
// }
// 
// void FSkelotModuleEd::ShutdownModule()
// {
// 	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
// 	// we call this function before unloading the module.
// }

static FName Name_PropertyEditor("PropertyEditor");
void FSkelotEdModule::StartupModule()
{

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(Name_PropertyEditor);
	//PropertyModule.RegisterCustomClassLayout(TEXT("SkelotAnimSet"), FOnGetDetailCustomizationInstance::CreateStatic(&FSkelotAnimSetDetails::MakeInstance));


	//PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("SkelotSequenceDef"), FOnGetPropertyTypeCustomizationInstance::CreateLambda([](){ return MakeShared<FSkelotSeqDefCustomization>(); }));
	//PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("SkelotMeshDef"), FOnGetPropertyTypeCustomizationInstance::CreateLambda([](){ return MakeShared<FSkelotMeshDefCustomization>(); }));

	//PropertyModule.RegisterCustomClassLayout(TEXT("SkelotComponent"), FOnGetDetailCustomizationInstance::CreateLambda([](){ return MakeShared<FSkelotComponentDetails>(); }));

	//PropertyModule.NotifyCustomizationModuleChanged();

	UThumbnailManager::Get().RegisterCustomRenderer(USkelotAnimCollection::StaticClass(), USkelotThumbnailRenderer::StaticClass());

	// 注册资产类型操作 - 为 SkeletalMesh 添加右键菜单
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TSharedRef<IAssetTypeActions> Action = MakeShareable(new FSkelotAssetTypeActions_SkeletalMesh());
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

void FSkelotEdModule::ShutdownModule()
{
	FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>(Name_PropertyEditor);
	//PropertyModule.UnregisterCustomClassLayout(TEXT("SkelotAnimSet"));

	//PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("SkelotSequenceDef"));
	//PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("SkelotMeshDef"));

	//PropertyModule.UnregisterCustomClassLayout(TEXT("SkelotComponent"));

	if (UObjectInitialized() && UThumbnailManager::TryGet())
		UThumbnailManager::TryGet()->UnregisterCustomRenderer(USkelotAnimCollection::StaticClass());

	// 注销资产类型操作
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			if (Action.IsValid())
			{
				AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
			}
		}
	}
	RegisteredAssetTypeActions.Empty();

	(void)PropertyModule;
}




#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSkelotEdModule, SkelotEd)
