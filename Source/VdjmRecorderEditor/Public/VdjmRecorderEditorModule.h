#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class IAssetTypeActions;

class FVdjmRecorderEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterAssetTypeActions();
	void UnregisterAssetTypeActions();
	void RegisterMenus();
	void UnregisterMenus();
	void RegisterAssetRegistryTab();
	void UnregisterAssetRegistryTab();
	TSharedRef<class SDockTab> SpawnAssetRegistryTab(const class FSpawnTabArgs& args);
	void RegisterVcardPresetCatalogTab();
	void UnregisterVcardPresetCatalogTab();
	TSharedRef<class SDockTab> SpawnVcardPresetCatalogTab(const class FSpawnTabArgs& args);

	TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;
};
