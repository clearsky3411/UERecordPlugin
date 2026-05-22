#include "VdjmRecorderEditorModule.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "SVdjmAssetRegistryPanel.h"
#include "SVdjmVcardPresetCatalogPanel.h"
#include "ToolMenus.h"
#include "VdjmRecordEventFlowDataAssetActions.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FVdjmRecorderEditorModule"

namespace
{
	const FName VdjmAssetRegistryTabId(TEXT("VdjmAssetRegistry"));
	const FName VdjmVcardPresetCatalogTabId(TEXT("VdjmVcardPresetCatalog"));
}

void FVdjmRecorderEditorModule::StartupModule()
{
	RegisterAssetTypeActions();
	RegisterAssetRegistryTab();
	RegisterVcardPresetCatalogTab();
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FVdjmRecorderEditorModule::RegisterMenus));
}

void FVdjmRecorderEditorModule::ShutdownModule()
{
	UnregisterMenus();
	UnregisterVcardPresetCatalogTab();
	UnregisterAssetRegistryTab();
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

void FVdjmRecorderEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped ownerScoped(this);

	UToolMenu* toolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& section = toolsMenu->FindOrAddSection(TEXT("Vdjm"));
	section.AddMenuEntry(
		TEXT("OpenVdjmAssetRegistry"),
		LOCTEXT("OpenVdjmAssetRegistry", "Vdjm Asset Registry"),
		LOCTEXT("OpenVdjmAssetRegistryTooltip", "Open the Vdjm asset registry scanner and classifier."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(VdjmAssetRegistryTabId);
		})));

	section.AddMenuEntry(
		TEXT("OpenVdjmVcardPresetCatalog"),
		LOCTEXT("OpenVdjmVcardPresetCatalog", "Vcard Preset Catalog"),
		LOCTEXT("OpenVdjmVcardPresetCatalogTooltip", "Open the Vcard preset catalog batch scanner."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(VdjmVcardPresetCatalogTabId);
		})));
}

void FVdjmRecorderEditorModule::UnregisterMenus()
{
	if (UObjectInitialized() && UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}
}

void FVdjmRecorderEditorModule::RegisterAssetRegistryTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		VdjmAssetRegistryTabId,
		FOnSpawnTab::CreateRaw(this, &FVdjmRecorderEditorModule::SpawnAssetRegistryTab))
		.SetDisplayName(LOCTEXT("VdjmAssetRegistryTab", "Vdjm Asset Registry"))
		.SetTooltipText(LOCTEXT("VdjmAssetRegistryTabTooltip", "Scan, classify, and validate Vdjm asset registry entries."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FVdjmRecorderEditorModule::UnregisterAssetRegistryTab()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VdjmAssetRegistryTabId);
}

TSharedRef<SDockTab> FVdjmRecorderEditorModule::SpawnAssetRegistryTab(const FSpawnTabArgs& args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("VdjmAssetRegistryTabLabel", "Vdjm Asset Registry"))
		[
			SNew(SVdjmAssetRegistryPanel)
		];
}

void FVdjmRecorderEditorModule::RegisterVcardPresetCatalogTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		VdjmVcardPresetCatalogTabId,
		FOnSpawnTab::CreateRaw(this, &FVdjmRecorderEditorModule::SpawnVcardPresetCatalogTab))
		.SetDisplayName(LOCTEXT("VdjmVcardPresetCatalogTab", "Vcard Preset Catalog"))
		.SetTooltipText(LOCTEXT("VdjmVcardPresetCatalogTabTooltip", "Scan folders and batch-fill Vcard preset catalog chunks."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FVdjmRecorderEditorModule::UnregisterVcardPresetCatalogTab()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VdjmVcardPresetCatalogTabId);
}

TSharedRef<SDockTab> FVdjmRecorderEditorModule::SpawnVcardPresetCatalogTab(const FSpawnTabArgs& args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("VdjmVcardPresetCatalogTabLabel", "Vcard Preset Catalog"))
		[
			SNew(SVdjmVcardPresetCatalogPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVdjmRecorderEditorModule, VdjmRecorderEditor)
