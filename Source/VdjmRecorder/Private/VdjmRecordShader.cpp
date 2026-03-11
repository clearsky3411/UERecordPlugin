// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmRecordShader.h"


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
§	↓		class FVdjmRecordShaderCS : public FGlobalShader	↓
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
/*
 
IMPLEMENT_SHADER_TYPE(, FInterpolateGroomGuidesCS, TEXT("/Plugin/Runtime/HairStrands/Private/NiagaraInterpolateGroomGuides.usf"), TEXT("MainCS"), SF_Compute);
실제 경로: Engine/Plugins/Runtime/HairStrands/Shaders/Private/NiagaraInterpolateGroomGuides.usf
사용 경로:	   /Plugin/Runtime/HairStrands/Private/NiagaraInterpolateGroomGuides.usf
결론: Plugins -> /Plugin, Engine -> 제거, Shaders -> 제거, Private -> 그대로, 파일명 그대로. 즉 제거되는 건 project 경로 + Plugin 폴더의 이름 + Shader 폴더, 나머지는 그대로 유지.
그렇다면 내거는?
실제 경로: UniVdigm0/Plugins/VdjmMobileUi/Shaders/VdjmRecordShader.usf
사용 경로:	   /Plugin/VdjmMobileUi/VdjmRecordShader.usf
 */
//	/Plugin/<PluginName>/<PluginFilePath>

IMPLEMENT_SHADER_TYPE(,FVdjmRecordNV12CSShader,VDJM_RECORD_SOURCE_FILE,VDJM_RECORD_SHADER_ENTRY_POINT,SF_Compute);

FString FVdjmRecordNV12CSShader::GetSourceFileName()
{
	return FString::Printf(TEXT("%s/%s/%s"), &*VDJM_RECORD_VIRTUAL_PLUGIN_FOLDER, &*VDJM_RECORD_PLUGIN_NAME, &*VDJM_RECORD_SHADER_FILE);
}