// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VdjmRecorderCore.h"
#include "UObject/Object.h"
#include "VdjmRecoderEncoderImpl.h"
#include "VdjmAndroidCore.generated.h"

struct ANativeWindow;
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class UVdjmRecordAndroidResource : public UVdjmRecordResource
*/
UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordAndroidResource : public UVdjmRecordResource
{
	GENERATED_BODY()
	/*
		 * TODO(260410-cofigs) 
		 */
public:
	
	virtual void ReleaseResources() override;
	virtual void ResetResource() override;
	virtual FTextureRHIRef GetCurrPooledTextureRHI() override;
	virtual FTextureRHIRef GetNextPooledTextureRHI() override;
	virtual bool DbcIsValidResource() const override;
	virtual void BeginDestroy() override;
	
};


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class UVdjmRecordAndroidSurfacer : public UVdjmRecordUnit
*/
UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordAndroidUnit : public UVdjmRecordUnit
{
	GENERATED_BODY()

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FVdjmAndroidSubmitPassParameters, )
	RDG_TEXTURE_ACCESS(InputTexture, ERHIAccess::CopySrc)
END_SHADER_PARAMETER_STRUCT()

	void ReleaseRecordPrevStartDelegate();
	void ReleaseRecordStartedDelegate();
	UFUNCTION()
	void RecordStartedDelegateFunc(UVdjmRecordResource* VdjmRecordResource);
	
	
	
	UFUNCTION()	
	void PostEndPipelineExecute(const FVdjmRecordUnitParamContext& context, FVdjmRecordUnitParamPayload& payload);
	
	UFUNCTION()
	void StopRecord();
	
	virtual bool InitializeUnit(UVdjmRecordResource* recordResource) override;
	virtual void ExecuteUnit(const FVdjmRecordUnitParamContext& context, FVdjmRecordUnitParamPayload& payload) override;
	virtual void ReleaseUnit() override;
	virtual EVdjmRecordPipelineStages GetPipelineStage() const override
	{
		return EVdjmRecordPipelineStages::ESurfaceEncodeAndWrite;
	}
	
	virtual bool DbcIsValidUnitInit() const override;
	virtual bool DbcRecordUnitStatus() const override;
	
protected:
	VdjmResult RecordStartCheck();
	
	FDelegateHandle mStartRecordStepsHandle;
	FDelegateHandle mPostEndPipelineExecuteHandle;
	TSharedPtr<FVdjmVideoEncoderBase> mAndroidEncoderImpl;
	
	bool bInitializedStatus = false;
private:
	
	
	
	void SubmitFrameToSurfacer(FRDGBuilder& graphBuilder, const FRDGTextureRef& srcTexture, double timeStampSec);
};
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class UVdjmRecordAndroidResource : public UVdjmRecordResource
*/
UCLASS()
class VDJMRECORDER_API UVdjmAndroidRecordPipeline : public UVdjmRecordUnitPipeline
{
	GENERATED_BODY()

public:
	virtual void InitializeRecordPipeline(UVdjmRecordResource* recordResource) override;
	virtual void ExecuteRecordPipeline(const FVdjmRecordUnitParamContext& context,
		FVdjmRecordUnitParamPayload& payload) override;
	virtual void StopRecordPipelineExecution() override;
	virtual void ReleaseRecordPipeline() override;
	virtual bool DbcIsValid() const override;
	
	bool ValidateForAndroidPipeline(FVdjmRecordEnvPlatformInfo* platformInfo) const;
};


UCLASS()
class VDJMRECORDER_API UVdjmAndroidCore : public UObject
{
	GENERATED_BODY()
};
