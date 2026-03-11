#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define VDJM_TCHAR( str ) TEXT( str )
#define VDJM_RECORD_PLUGIN_NAME TEXT("VdjmMobileUi")

#define VDJM_RECORD_REAL_PLUGIN_FOLDER TEXT("Plugins")
#define VDJM_RECORD_VIRTUAL_PLUGIN_FOLDER TEXT("Plugin")

#define VDJM_RECORD_REAL_SHADER_SUBDIR TEXT("Shaders")

#define VDJM_RECORD_VIRTUAL_SHADERDIR\
    TEXT("/") VDJM_RECORD_VIRTUAL_PLUGIN_FOLDER\
    TEXT("/") VDJM_RECORD_PLUGIN_NAME

#define VDJM_RECORD_SHADER_FILE TEXT("VdjmRecordShader.usf")

#define VDJM_RECORD_SOURCE_FILE VDJM_RECORD_VIRTUAL_SHADERDIR TEXT("/") VDJM_RECORD_SHADER_FILE
#define VDJM_RECORD_SHADER_ENTRY_POINT TEXT("MainCS")

class FVdjmRecorderModule : public IModuleInterface
{
public:

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
private:
    static FString RealShaderDir;
};
