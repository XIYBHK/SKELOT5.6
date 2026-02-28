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

#define LOCTEXT_NAMESPACE "FSkelotEdModule"

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
}

void FSkelotEdModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(Name_PropertyEditor);
	//PropertyModule.UnregisterCustomClassLayout(TEXT("SkelotAnimSet"));

	//PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("SkelotSequenceDef"));
	//PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("SkelotMeshDef"));

	//PropertyModule.UnregisterCustomClassLayout(TEXT("SkelotComponent"));

	if (UObjectInitialized() && UThumbnailManager::TryGet())
		UThumbnailManager::TryGet()->UnregisterCustomRenderer(USkelotAnimCollection::StaticClass());
}




#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSkelotEdModule, SkelotEd)
