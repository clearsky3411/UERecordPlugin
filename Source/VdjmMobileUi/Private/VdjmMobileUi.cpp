// Copyright Epic Games, Inc. All Rights Reserved.

#include "VdjmMobileUi.h"

#include "Interfaces/IPluginManager.h"

// const FString FVdjmMobileUiModule::PluginName = TEXT("VdjmMobileUi");;
// const FString FVdjmMobileUiModule::ShaderSubDir = TEXT("Shaders");
// const FString FVdjmMobileUiModule::VirtualShaderDir = FString::Printf(TEXT("/Plugin/%s"), *FVdjmMobileUiModule::PluginName);
// FString FVdjmMobileUiModule::RealShaderDir = TEXT("");

#define LOCTEXT_NAMESPACE "FVdjmMobileUiModule"


void FVdjmMobileUiModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FVdjmMobileUiModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FVdjmMobileUiModule, VdjmMobileUi)