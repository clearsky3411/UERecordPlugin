#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define VDJM_ASSET_REGISTRY_PLUGIN_NAME TEXT("VdjmMobileUi")
#define VDJM_ASSET_REGISTRY_DEFAULT_CONFIG TEXT("VdjmAssetRegistry.json")

DECLARE_LOG_CATEGORY_EXTERN(LogVdjmAssetRegistry, Log, All);

class FVdjmAssetRegistryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
