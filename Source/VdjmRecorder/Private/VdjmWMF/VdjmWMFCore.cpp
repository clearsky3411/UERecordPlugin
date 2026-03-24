// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmWMF/VdjmWMFCore.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "VdjmRecorder.h"
#include "VdjmWMF/VdjmRecorderWndEncoder.h"
#include "VdjmRecordShader.h"
#include "VdjmEncoderFactory.h"


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
§	↓		class UVdjmRecordWMFResource : public UVdjmRecordResource			↓
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
void UVdjmRecordWMFResource::InitializeTexturePool(FIntPoint textureResolution, EPixelFormat finalPixelFormat,	const int32 poolSize)
{
	if (IsInRenderingThread())
	{
		mTexturePoolRHI.Empty();
		mTexturePoolRHI.Reserve(poolSize);
		for (int32 i = 0; i < poolSize; ++i)
		{
			int32 size = mTexturePoolRHI.Add(CreateTextureForNV12(
				textureResolution,	//	어차피 수정
				finalPixelFormat,	//	어차피 고정
				ETextureCreateFlags::UAV | ETextureCreateFlags::ShaderResource | ETextureCreateFlags::Shared));
		}
		mCurrentPoolIndex  = 0;
		
		FVdjmEncoderStatus::DbcGameThreadTask([weakThis = TWeakObjectPtr<UVdjmRecordResource>(this)]()
		{
			if (weakThis.IsValid() && weakThis->OwnerBridgeActor.IsValid())
			{
				weakThis->OwnerBridgeActor->OnResourceReadyForPostInit(weakThis.Get());
				
			}
		});
	}
}

void UVdjmRecordWMFResource::InitializeResource(AVdjmRecordBridgeActor* ownerBridge)
{
	Super::InitializeResource(ownerBridge);

	FVdjmEncoderStatus::DbcRenderThreadTask(
		[
			textureResolution = TextureResolution,
			finalPixelFormat = FinalPixelFormat,
			poolSize = FVdjmReadBackHelper::ReadBackBufferCount,
			weakThis = TWeakObjectPtr<UVdjmRecordWMFResource>(this) ]()->void{
			if (weakThis.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("InitializeTexturePool "));

				weakThis->InitializeTexturePool(textureResolution, finalPixelFormat, poolSize);
			}
		});
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordResource::Initialize - Initialized with Resolution(%d,%d), FrameRate(%d), Bitrate(%d)"),
		TextureResolution.X,TextureResolution.Y,FinalFrameRate,FinalBitrate);
}

void UVdjmRecordWMFResource::ReleaseResources()
{
	Super::ReleaseResources();
	for(int i=0; i<mTexturePoolRHI.Num(); i++)
	{
		mTexturePoolRHI[i].SafeRelease();
	}
	mTexturePoolRHI.Empty();
	mCurrentPoolIndex = 0;
}

void UVdjmRecordWMFResource::ResetResource()
{
	Super::ResetResource();
	mCurrentPoolIndex = 0;
	
}


FTextureRHIRef UVdjmRecordWMFResource::GetCurrPooledTextureRHI()
{
	if (not DbcIsValidResourceInit())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::GetCurrentPooledTextureRHI - Resource is not valid. Call Initialize() first."));
		return nullptr;
	}
	
	return mTexturePoolRHI[mCurrentPoolIndex];
}

FTextureRHIRef UVdjmRecordWMFResource::GetNextPooledTextureRHI()
{
	if (not DbcIsValidResourceInit())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::GetNextPooledTextureRHI - Resource is not valid. Call Initialize() first."));
		return nullptr;
	}
	
	FTextureRHIRef ret = mTexturePoolRHI[mCurrentPoolIndex];
	mCurrentPoolIndex = (mCurrentPoolIndex + 1) % mTexturePoolRHI.Num();
	
	return ret;
}

bool UVdjmRecordWMFResource::DbcIsValidResource() const
{
	return DbcIsDefaultReady() && mTexturePoolRHI.Num() > 0 && mTexturePoolRHI[0] != nullptr;
}

void UVdjmRecordWMFResource::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResources();
}

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
§	↓		class UVdjmRecordCSUnit : public UObject			↓
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

void UVdjmRecordWMFCSUnit::ExecuteUnit(const FVdjmRecordUnitParamContext& context, FVdjmRecordUnitParamPayload& payload)
{
	if (context.DbcIsValidUnit())
	{
		DispatchRecordPass(*context.GraphBuilder, payload);
	}
	else
	{
		payload.bSuccess = false;
		//payload.LogString.Appendf(TEXT("UVdjmRecordCSUnit::ExecuteUnit - Error - Context is not valid \n"));
	}
}

void UVdjmRecordWMFCSUnit::ReleaseUnit()
{
}

void UVdjmRecordWMFCSUnit::DispatchRecordPass(FRDGBuilder& graphBuilder,FVdjmRecordUnitParamPayload& inPayload) const
{
	const TCHAR* NameIfUnregistered = TEXT("VdjmRecordCSUnit_Unregistered");
	
	if (not DbcIsValidUnitInit())
	{
		inPayload.bSuccess = false;
		//inPayload.LogString.Appendf( TEXT("UVdjmRecordCSUnit::DispatchRecordPass - Error - Dbc Is not Valid \n"));
		return;
	}

	inPayload.OutputTexture = LinkedRecordResource->GetNextPooledTextureRHI();
	if (not inPayload.OutputTexture.IsValid())
	{
		inPayload.bSuccess = false;
		//inPayload.LogString.Appendf(TEXT("UVdjmRecordCSUnit::DispatchRecordPass - Error - OutputTexture is not valid \n"));
		return;
	}
	//	여기에서 Param 입력
	FVdjmRecordNV12CSShader::FParameters* passParams = graphBuilder.AllocParameters<FVdjmRecordNV12CSShader::FParameters>();
	
	passParams->InputTexture = inPayload.InputTexture;
	passParams->OutputTexture =
		graphBuilder.CreateUAV(
			RegisterExternalTexture(
			graphBuilder,
			inPayload.OutputTexture,
			NameIfUnregistered)
			);
	passParams->OriginWidth = LinkedRecordResource->OriginResolution.X;
	passParams->OriginHeight = LinkedRecordResource->OriginResolution.Y;

	TShaderMapRef<FVdjmRecordNV12CSShader> recordCsShaderObj(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordCSUnit::DispatchRecordPass - Dispatching Compute Shader Pass with InputTexture: %p, OutputTexture: %p, OriginResolution: (%d, %d)"),
		inPayload.InputTexture, inPayload.OutputTexture.GetReference(), passParams->OriginWidth, passParams->OriginHeight);
	
	FRDGPassRef rdgPass = FComputeShaderUtils::AddPass(
		graphBuilder,
		RDG_EVENT_NAME("VdjmRecordCSUnit_Dispatch"),
		recordCsShaderObj,
		passParams,
		LinkedRecordResource->CachedGroupCount);
	
	inPayload.bSuccess = true;
	//inPayload.LogString.Appendf(TEXT("UVdjmRecordCSUnit::DispatchRecordPass - Dispatched Compute Shader Pass \n"));
}


/*	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
 *	
 *					UVdjmRecordWMFEncoderReadBackUnit
 */

void UVdjmRecordWMFEncoderReadBackUnit::OnStartRecordPrepare(UVdjmRecordResource* res)
{
#ifdef PLATFORM_WINDOWS
	
	if (res && mWindowsEncoder.IsValid())
	{
		if (mWindowsEncoder->InitializeEncoder(
			res->FinalFilePath
			,res->OriginResolution.X
			, res->OriginResolution.Y
			, res->FinalBitrate
			, res->FinalFrameRate))
		{
			VdjmResult vdjmResult = mWindowsEncoder->StartEncoder();
			if (VdjmFailed(vdjmResult))
			{
				UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordWMFEncoderReadBackUnit::BeginRecordPipelineExecute - Failed to start Windows Encoder. HRESULT: 0x%08X"), vdjmResult);
			}
		}
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordWMFEncoderReadBackUnit::BeginRecordPipelineExecute - Windows Encoder is not valid. Cannot start encoder."));
	}
#endif
}

void UVdjmRecordWMFEncoderReadBackUnit::PostEndPipelineExecute(const FVdjmRecordUnitParamContext& context,FVdjmRecordUnitParamPayload& payload)
{
	if (context.DbcIsValidUnit() && payload.OutputTexture.IsValid())
	{
		context.GraphBuilder->Execute();
		
		ProcessPendingReadbacks();	
	}
}

bool UVdjmRecordWMFEncoderReadBackUnit::InitializeUnit(UVdjmRecordResource* recordResource)
{
	LinkedRecordResource = recordResource;
	
	if (not LinkedRecordResource.IsValid() || not LinkedPipeline.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordWMFEncoderReadBackUnit::InitializeUnit - Invalid record resource. Initialization failed."));
		return false;
	}
	
	TWeakObjectPtr<AVdjmRecordBridgeActor> bridge = LinkedPipeline->LinkedBridgeActor;
	if (not bridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordWMFEncoderReadBackUnit::InitializeUnit - Successfully initialized unit and linked pipeline is valid. Setting up pipeline execution delegates."));
		bridge = AVdjmRecordBridgeActor::TryGetRecordBridgeActor();
	}
	
	if (mReadBackHelper == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordWMFEncoderReadBackUnit::OverrideInitializeUnit - Initializing ReadBackHelper."));
		mReadBackHelper = MakeUnique<FVdjmReadBackHelper>();
		mReadBackHelper->Initialize();
		if (not mReadBackHelper->IsValidReadBacks())
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordWMFEncoderReadBackUnit::OverrideInitializeUnit - Failed to initialize ReadBackHelper."));
			return false;
		}
	}
	
	// 최초 한번만
	bridge->OnRecordPrevStart.AddDynamic(this, &UVdjmRecordWMFEncoderReadBackUnit::OnStartRecordPrepare);
	//	이건 파이프라인 종료후에 호출되어야 하는놈
	LinkedPipeline->OnEndPipelineExecution.AddUObject(this, &UVdjmRecordWMFEncoderReadBackUnit::PostEndPipelineExecute);
	
	mWindowsEncoder = CreatePlatformVideoEncoder();
	
	if (mWindowsEncoder.IsValid())
	{
		if (not mWindowsEncoder->InitializeEncoder(
			recordResource->FinalFilePath,
			recordResource->OriginResolution.X,
			recordResource->OriginResolution.Y,
			recordResource->FinalBitrate,
			recordResource->FinalFrameRate
		))
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordWMFEncoderReadBackUnit::PostInitializeWindow - Failed to initialize Windows Encoder."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordWMFEncoderReadBackUnit::PostInitializeWindow - Failed to create Windows Encoder instance."));
		return false;
	}
	
	return true;
}

void UVdjmRecordWMFEncoderReadBackUnit::ExecuteUnit(const FVdjmRecordUnitParamContext& context,FVdjmRecordUnitParamPayload& payload)
{
	if (context.GraphBuilder == nullptr || mReadBackHelper == nullptr || not payload.OutputTexture.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("-		UVdjmRecordWMFEncoderReadBackUnit::ExecuteUnit - Invalid context or payload. Skipping readback execution."));
		if (!context.GraphBuilder)
		{
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("-			UVdjmRecordWMFEncoderReadBackUnit::ExecuteUnit - GraphBuilder is null."));
		}
		if (!mReadBackHelper)		{
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("-			UVdjmRecordWMFEncoderReadBackUnit::ExecuteUnit - ReadBackHelper is not valid."));
		}
		if (!payload.OutputTexture.IsValid())
		{
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("-			UVdjmRecordWMFEncoderReadBackUnit::ExecuteUnit - OutputTexture is not valid."));
		}
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordWMFEncoderReadBackUnit::ExecuteUnit - Valid context and payload. Proceeding with readback execution."));
		EncodeFrameRDGPass(*context.GraphBuilder, payload.OutputTexture, context.CurrentRecordTimeSec);
	}
}
void UVdjmRecordWMFEncoderReadBackUnit::EncodeFrameRDGPass(FRDGBuilder& graphBuilder ,const FTextureRHIRef srcTex,const double timeStampSec)
{
	FRDGTextureRef nv12Tex = RegisterExternalTexture(
			graphBuilder,
			srcTex,
			TEXT("Vdjm_Readback_Src"));
	FReadBackPassParameters* PassParams = graphBuilder.AllocParameters<FReadBackPassParameters>();
	PassParams->InputTexture = nv12Tex;
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordWMFEncoderReadBackUnit::EncodeFrameRDGPass - Registered external texture for readback: %p at timestamp %f ms"), srcTex.GetReference(), timeStampSec);
	
	graphBuilder.AddPass(
		RDG_EVENT_NAME("VdjmRecordEncoderReadBackUnit_ReadBack"),
		PassParams,
		ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
		[
			weakThis = TWeakObjectPtr<UVdjmRecordWMFEncoderReadBackUnit>(this)
			,TimeStampSec = timeStampSec
			,Parameters = PassParams](FRHICommandList& RHICmdList)
		{
			if (Parameters && weakThis.IsValid())
			{
				FTextureRHIRef inputTexture = Parameters->InputTexture->GetRHI();
				weakThis->DispatchReadBack_InPass(RHICmdList, inputTexture, TimeStampSec);
			}
		});
}
void UVdjmRecordWMFEncoderReadBackUnit::DispatchReadBack_InPass(FRHICommandList& RHICmdList, FTextureRHIRef inTexture, double timeStampMs) const
{
	if (mReadBackHelper && inTexture.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordWMFEncoderReadBackUnit::DispatchReadBack_InPass - Enqueuing read back for texture %p at timestamp %f ms"), inTexture.GetReference(), timeStampMs);
		mReadBackHelper->EnqueueFrame(RHICmdList, inTexture, timeStampMs);
	}
}
bool UVdjmRecordWMFEncoderReadBackUnit::DbcIsValidUnitInit() const
{
	if (not mReadBackHelper.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordWMFEncoderReadBackUnit::DbcIsValidUnitInit - ReadBackHelper is not valid."));
		return false;
	}
#if PLATFORM_WINDOWS
	if (not mWindowsEncoder.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordWMFEncoderReadBackUnit::DbcIsValidUnitInit - Windows Encoder is not valid."));
		return false;
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordWMFEncoderReadBackUnit::DbcIsValidUnitInit - Windows Encoder is valid."));
		return LinkedPipeline.IsValid() && LinkedRecordResource.IsValid();
	}
#elif PLATFORM_ANDROID
	return Super::DbcIsValidUnitInit();
#elif PLATFORM_IOS
	return Super::DbcIsValidUnitInit();
#endif
}



void UVdjmRecordWMFEncoderReadBackUnit::StopEncoding()
{
#if PLATFORM_WINDOWS
	if (mReadBackHelper.IsValid())
	{
		//	마지막 한프레임까지 확실히 처리되도록 GraphBuilder.Execute() 이후에 ReadBackHelper의 남은 프레임들을 처리
		ProcessPendingReadbacks();
	}
	
	if (mWindowsEncoder.IsValid())
	{
		mWindowsEncoder->StopEncoder(); 
	}
	
	if (mReadBackHelper.IsValid())
	{
		mReadBackHelper->StopAllReadBacks();
	}
#endif
}

void UVdjmRecordWMFEncoderReadBackUnit::ReleaseUnit()
{
#if PLATFORM_WINDOWS
	StopEncoding();
	
	if (mReadBackHelper)
	{
		mReadBackHelper.Reset();
	}

	if (mWindowsEncoder)
	{
		mWindowsEncoder->TerminateEncoder();
		mWindowsEncoder.Reset();
	}

#endif
}


void UVdjmRecordWMFEncoderReadBackUnit::ProcessPendingReadbacks()
{
	if (!mReadBackHelper)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordWMFEncoderReadBackUnit::ProcessPendingReadbacks - ReadBackHelper is not valid. Cannot process readbacks."));
		return;
	}
	
	int32 outPitch = 0;
	int32 outHeight = 0;
	double outTimeStamp = 0.0;
	
	while (void* rawData = mReadBackHelper->TryLockOldest(outPitch, outHeight, outTimeStamp))
	{
		int32 actualWidth = LinkedRecordResource->TextureResolution.X;
		int32 pureSize = actualWidth * outHeight;
		
		TArray<uint8> pureDataArray;
		pureDataArray.SetNumUninitialized(pureSize);
		
		uint8* dstPtr = pureDataArray.GetData();
		uint8* srcPtr = static_cast<uint8*>(rawData);
		
		for (int32 row = 0; row < outHeight; ++row)
		{
			FMemory::Memcpy(dstPtr, srcPtr, actualWidth);
			dstPtr += actualWidth;
			srcPtr += outPitch;
		}
		mReadBackHelper->UnlockOldest();
		
		OnCpuDataReady((void*)pureDataArray.GetData(), actualWidth, outHeight, outTimeStamp);
	}
}
void UVdjmRecordWMFEncoderReadBackUnit::OnCpuDataReady(void* rawData, int32 width, int32 height, double timeStampMs)
{ 
#if PLATFORM_WINDOWS
	int32 buffSize = (width * height);
	if (mWindowsEncoder.IsValid())
	{
		mWindowsEncoder->RunningEncodeFrame(
			rawData,
			buffSize,
			timeStampMs
		);
	}
#elif PLATFORM_ANDROID

#elif PLATFORM_IOS

#endif
}


#if PLATFORM_WINDOWS
// bool UVdjmRecordWMFEncoderReadBackUnit::PostInitializeWindow(UVdjmRecordResource* recordResource)
// {
// 	mWindowsEncoder = MakeShared<FVdjmWindowsEncoderImpl>();
// 	
// 	bool bSucceeded = mWindowsEncoder->InitializeEncoder(
// 		recordResource->FinalFilePath,
// 		recordResource->OriginResolution.X,
// 		recordResource->OriginResolution.Y,
// 		recordResource->FinalBitrate,
// 		recordResource->FinalFrameRate
// 	);
// 	if (not bSucceeded)
// 	{
// 		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordWMFEncoderReadBackUnit::PostInitializeWindow - Failed to initialize Windows Encoder."));
// 	}
// 	return bSucceeded;
// }
#endif


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	class UVdjmRecordUnitPipeline 
 */

void UVdjmRecordWMFUnitDefaultPipeline::InitializeRecordPipeline(UVdjmRecordResource* recordResource)
{
	Super::InitializeRecordPipeline(recordResource);
	if (recordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitDefaultPipeline::InitializeRecordPipeline resource fuck"));
		return;
	}
	UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitDefaultPipeline::InitializeRecordPipeline - Initializing pipeline for record resource."));
	LinkedBridgeActor = recordResource->OwnerBridgeActor;
	if (LinkedBridgeActor.IsValid() && LinkedBridgeActor->DbcValidRecordResource())
	{
		FVdjmRecordEnvPlatformInfo* platformInfo =
			LinkedBridgeActor->GetRecordEnvConfigureDataAsset()
				->GetPlatformInfo( AVdjmRecordBridgeActor::GetTargetPlatform());
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitDefaultPipeline::InitializeRecordPipeline - Creating pipeline units."));
		if (const TSubclassOf<UVdjmRecordUnit>* foundEComputeShader = platformInfo->GetPipelineState(EVdjmRecordPipelineStages::EComputeShader))
		{
			CreateUnit(*foundEComputeShader);
		}

		if (const TSubclassOf<UVdjmRecordUnit>* foundEEncode = platformInfo->GetPipelineState(
		EVdjmRecordPipelineStages::EEncode))
		{
			CreateUnit(*foundEEncode);
		}
		else if (const TSubclassOf<UVdjmRecordUnit>* foundEEncodeAndWrite = platformInfo->GetPipelineState(
			EVdjmRecordPipelineStages::EEncodeAndWrite))
		{
			CreateUnit(*foundEEncodeAndWrite);
		}
			
		if (const TSubclassOf<UVdjmRecordUnit>* foundEWriteToDisk = platformInfo->GetPipelineState(
		EVdjmRecordPipelineStages::EWriteToDisk))
		{
			CreateUnit(*foundEWriteToDisk);
		}
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitDefaultPipeline::InitializeRecordPipeline - Initializing pipeline units."));
		
	}
}

void UVdjmRecordWMFUnitDefaultPipeline::ExecuteRecordPipeline(const FVdjmRecordUnitParamContext& context,
	FVdjmRecordUnitParamPayload& payload)
{
	payload.previousUnit = nullptr;
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("{{{ ---	UVdjmRecordUnitDefaultPipeline::ExecuteRecordPipeline - Running - Starting pipeline execution with %d units."), RecordUnits.Num());
	
	OnBeginPipelineExecution.Broadcast(context, payload);
	
	for (TObjectPtr<UVdjmRecordUnit>& recordUnit :RecordUnits)
	{
		if (IsValid(recordUnit))
		{
			UE_LOG(LogVdjmRecorderCore, Log, TEXT(" {{  UVdjmRecordUnitDefaultPipeline::ExecuteRecordPipeline - Executing unit: %s"), *recordUnit->GetName());
			recordUnit->ExecuteUnit(context,payload);
			if (!payload.bSuccess)
			{
				//	Log and break on failure
				
				break;
			}
			OnEndExecuteUnit.Broadcast(context, payload);
			payload.previousUnit = recordUnit;
		}
		else
		{
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("{{ UVdjmRecordUnitDefaultPipeline::ExecuteRecordPipeline - Warning: Encountered invalid record unit in pipeline. Skipping unit."));
		}
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("  }} UVdjmRecordUnitDefaultPipeline::ExecuteRecordPipeline - Finished executing unit: %s"), IsValid(recordUnit) ? *recordUnit->GetName() : TEXT("Invalid Unit"));
	}
	OnEndPipelineExecution.Broadcast(context, payload);
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("}}} UVdjmRecordUnitDefaultPipeline::ExecuteRecordPipeline - Finished pipeline execution."));
}

void UVdjmRecordWMFUnitDefaultPipeline::StopRecordPipelineExecution()
{
	TravelLoopUnits([](UVdjmRecordUnit* unit)->int32
	{
		if (UVdjmRecordEncoderUnit* castedUnit = Cast<UVdjmRecordEncoderUnit>(unit))
		{
			castedUnit->StopEncoding();
			return 1;
		}
		else
		{
			return 0;
		}
	});
}

void UVdjmRecordWMFUnitDefaultPipeline::ReleaseRecordPipeline()
{
	Super::ReleaseRecordPipeline();
}


