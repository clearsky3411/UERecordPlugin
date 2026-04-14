// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VdjmRecorderCore.h"
#include "UObject/Object.h"

#if PLATFORM_WINDOWS
#include "Video/VideoEncoder.h"
#include "Video/Resources/VideoResourceCPU.h"
//#include "Video/
#endif

#include "VdjmWMFCore.generated.h"



UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordWMFResource : public UVdjmRecordResource
{
	GENERATED_BODY()
public:
	void InitializeTexturePool(FIntPoint textureResolution,EPixelFormat finalPixelFormat, const int32 poolSize);
	
	virtual void InitializeResource_deprecated(AVdjmRecordBridgeActor* ownerBridge) override;
	virtual bool InitializeResourceExtended(UVdjmRecordEnvResolver* resolver) override;
	virtual void ReleaseResources() override;
	virtual void ResetResource() override;
	
	virtual FTextureRHIRef GetCurrPooledTextureRHI() override;
	virtual FTextureRHIRef GetNextPooledTextureRHI() override;
	
	virtual bool DbcIsValidResource() const override;
	virtual void BeginDestroy() override;
	virtual bool IsLazyPostInitializeCheck() const override
	{
		return true;
	}


private:
	TArray<FTextureRHIRef> mTexturePoolRHI;
	int32 mCurrentPoolIndex = 0;
};

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓			class UVdjmRecordCSUnit : public UObject		↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordWMFCSUnit : public UVdjmRecordUnit
{
	GENERATED_BODY()
public:
	void DispatchRecordPass(FRDGBuilder& graphBuilder, FVdjmRecordUnitParamPayload& inPayload) const;

	virtual void ExecuteUnit(const FVdjmRecordUnitParamContext& context, FVdjmRecordUnitParamPayload& payload) override;
	
	virtual void ReleaseUnit() override;
	

	//	계층적 검사
	virtual bool DbcIsValidUnitInit() const override
	{
		return LinkedRecordResource.IsValid() && LinkedRecordResource->DbcIsValidResourceInit();
	}

	virtual EVdjmRecordPipelineStages GetPipelineStage() const override
	{
		return EVdjmRecordPipelineStages::EComputeShader;
	}
	
	
};

/*
↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
class UVdjmRecordEncoderReadBackUnit : public UVdjmRecordEncoderUnit	 
@brief
0. BeginRecordPipelineExecute 시점에 Windows Encoder 초기화, 필요한 경우 리소스 준비, 이때 화면의 크기나 경로가 바뀌면 다시 초기화하도록 설계(멱등성 유지)
1. ExecuteUnit 에서 RDG Pass로 GPU에서 CPU로의 ReadBack 수행
2. PostEndPipelineExecute 시점에서 ReadBack 된 데이터로 인코딩 수행, Windows Encoder 사용 시, Windows Encoder의 API 호출하여 인코딩 진행()
	- 내부에서 ProcessPendingReadbacks 호출하고 텍스처를 Map 하는 것까지는 기존 Thread 로 진행하고 엔코딩 자체는 다른 Thread에서 진행하도록 설계, Windows Encoder의 API가 스레드 안전하다면 PostEndPipelineExecute 시점에서 바로 인코딩 진행해도 무방

*/
UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordWMFEncoderReadBackUnit : public UVdjmRecordEncoderUnit
{
	GENERATED_BODY()
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FReadBackPassParameters, )
		RDG_TEXTURE_ACCESS(InputTexture, ERHIAccess::CopySrc)
	END_SHADER_PARAMETER_STRUCT()
	
	UFUNCTION()
	void OnStartRecordPrepare(UVdjmRecordResource* res);
	
	//	bind 
	UFUNCTION()
	void PostEndPipelineExecute(const FVdjmRecordUnitParamContext& context, FVdjmRecordUnitParamPayload& payload);
	
	virtual bool InitializeUnit(UVdjmRecordResource* recordResource) override;
	virtual void ExecuteUnit(const FVdjmRecordUnitParamContext& context, FVdjmRecordUnitParamPayload& payload) override;
	virtual EVdjmRecordPipelineStages GetPipelineStage() const override
	{
		return EVdjmRecordPipelineStages::EEncodeAndWrite;
	}
	virtual bool DbcIsValidUnitInit() const override;
	
	//	Copy 소스 하는 부분인데 이름을 이렇게 지어도 되나...
	virtual void EncodeFrameRDGPass(FRDGBuilder& graphBuilder ,const FTextureRHIRef srcTex,const double timeStampSec) override;
	
	/**
	 * @brief 인코딩 중지, 필요한 경우 Windows Encoder의 StopEncoder() 호출로 다시 시작 가능하게 정리만 하는 상태
	 */
	virtual void StopEncoding() override;
	
	/**
	 * @brief 모든 정보 해제, 리소스 해제 등 EncodeFrameRDGPass 이후에 필요한 정리 작업 수행.
	 * - mReadBackHelper->StopAllReadBacks() 호출
	 * - Windows Encoder 사용 시, mWindowsEncoder->StopEncoder() 및 TerminateEncoder() 호출
	 */
	virtual void ReleaseUnit() override;
	
private:
	/*
	 * @brief RDG Pass 내부에서 GPU에서 CPU로의 ReadBack 수행, Pass 내부에서 호출되어야 하며, GraphBuilder.Execute() 이후에 CPU에서 데이터가 준비되는 시점에 OnCpuDataReady() 콜백이 호출되도록 설계
	 * - mReadBackHelper 와 @param inTexture 가 유효해야 mReadBackHelper의 EnqueueFrame를 호출 
	 */
	void DispatchReadBack_InPass(FRHICommandList& RHICmdList, FTextureRHIRef inTexture, double timeStampMs) const;
	/*
	 * @brief GPU에서 CPU로의 ReadBack이 완료된 프레임이 있을 때마다 호출, 인코딩에 필요한 데이터가 준비된 시점에 Windows Encoder의 API를 호출하여 인코딩 진행, 이 메서드 내에서 ProcessPendingReadbacks()를 호출하여 대기 중인 ReadBack 처리
	 */
	void ProcessPendingReadbacks();
	void OnCpuDataReady(void* rawData,int32 width,int32 height,double timeStampMs);

	TUniquePtr<FVdjmReadBackHelper> mReadBackHelper;
	
	FDelegateHandle mStartRecordPrepareHandle;
	FDelegateHandle mPostEndPipelineExecuteHandle;
	TSharedPtr<FVdjmVideoEncoderBase> mWindowsEncoder;
};


/*
	↓	↓	↓	↓	↓	↓	↓	↓	
		class UVdjmRecordUnitDefaultPipeline : public UVdjmRecordUnitPipeline		
*/
UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordWMFUnitDefaultPipeline : public UVdjmRecordUnitPipeline
{
	GENERATED_BODY()

public:
	virtual void InitializeRecordPipeline(UVdjmRecordResource* recordResource) override;
	virtual void ExecuteRecordPipeline(const FVdjmRecordUnitParamContext& context,
		FVdjmRecordUnitParamPayload& payload) override;
	virtual void StopRecordPipelineExecution() override;
	virtual void ReleaseRecordPipeline() override;
};