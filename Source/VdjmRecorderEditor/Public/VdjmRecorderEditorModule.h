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

	TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;
};
