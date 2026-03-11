#include "VdjmRecorder.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FVdjmRecorderModule"

FString FVdjmRecorderModule::RealShaderDir = TEXT("");

void FVdjmRecorderModule::StartupModule()
{
	const TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin(VDJM_RECORD_PLUGIN_NAME);
	
	checkf(plugin.IsValid(), TEXT("Plugin %s not found!"), TEXT("VdjmMobileUi"));
	
	RealShaderDir = FPaths::Combine(plugin->GetBaseDir(), VDJM_RECORD_REAL_SHADER_SUBDIR);
	
	AddShaderSourceDirectoryMapping(VDJM_RECORD_VIRTUAL_SHADERDIR,RealShaderDir	);
}

void FVdjmRecorderModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FVdjmRecorderModule, VdjmRecorder)