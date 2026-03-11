// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterStruct.h"
#include "VdjmRecorder.h"
#include "UObject/Object.h"
#include "VdjmRecordShader.generated.h"

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓	class FVdjmRecordShaderCS : public FGlobalShader		↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
class FVdjmRecordNV12CSShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVdjmRecordNV12CSShader,Global)
	SHADER_USE_PARAMETER_STRUCT(FVdjmRecordNV12CSShader, FGlobalShader);
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutputTexture)
		SHADER_PARAMETER(uint32, OriginWidth)  // 원본 해상도 Width
		SHADER_PARAMETER(uint32, OriginHeight) // 원본 해상도 Height
	END_SHADER_PARAMETER_STRUCT()
public:
	static FString GetShaderFileName() 	{	return VDJM_RECORD_SHADER_FILE;	}
	static FString GetMainFunctionName(){	return VDJM_RECORD_SHADER_ENTRY_POINT;}
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
	static FString GetSourceFileName();
};

UCLASS()
class VDJMRECORDER_API UVdjmRecordShader : public UObject
{
	GENERATED_BODY()
};
