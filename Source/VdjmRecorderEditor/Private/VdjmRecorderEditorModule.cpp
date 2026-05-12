#include "VdjmRecorderEditorModule.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "VdjmRecordEventFlowDataAssetActions.h"

#define LOCTEXT_NAMESPACE "FVdjmRecorderEditorModule"

void FVdjmRecorderEditorModule::StartupModule()
{
	RegisterAssetTypeActions();
}

void FVdjmRecorderEditorModule::ShutdownModule()
{
	UnregisterAssetTypeActions();
}

void FVdjmRecorderEditorModule::RegisterAssetTypeActions()
{
	FAssetToolsModule& assetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	const TSharedRef<IAssetTypeActions> flowDataAssetActions = MakeShared<FVdjmRecordEventFlowDataAssetActions>();
	assetToolsModule.Get().RegisterAssetTypeActions(flowDataAssetActions);
	RegisteredAssetTypeActions.Add(flowDataAssetActions);
}

void FVdjmRecorderEditorModule::UnregisterAssetTypeActions()
{
	if (not FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
	{
		RegisteredAssetTypeActions.Reset();
		return;
	}

	FAssetToolsModule& assetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	for (const TSharedPtr<IAssetTypeActions>& assetTypeActions : RegisteredAssetTypeActions)
	{
		if (assetTypeActions.IsValid())
		{
			assetToolsModule.Get().UnregisterAssetTypeActions(assetTypeActions.ToSharedRef());
		}
	}

	RegisteredAssetTypeActions.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVdjmRecorderEditorModule, VdjmRecorderEditor)
