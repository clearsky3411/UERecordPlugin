#include "VdjmAssetRegistry.h"

#define LOCTEXT_NAMESPACE "FVdjmAssetRegistryModule"

DEFINE_LOG_CATEGORY(LogVdjmAssetRegistry)

void FVdjmAssetRegistryModule::StartupModule()
{
}

void FVdjmAssetRegistryModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVdjmAssetRegistryModule, VdjmAssetRegistry)
