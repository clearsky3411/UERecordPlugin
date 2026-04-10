// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidCore.h"

#include "RenderGraphUtils.h"
#include "VdjmRecordShader.h"

#include "DynamicRHI.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RenderGraphUtils.h"

#include "VdjmEncoderFactory.h"

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class UVdjmRecordAndroidResource : public UVdjmRecordResource
*/

void UVdjmRecordAndroidResource::InitializeResource(AVdjmRecordBridgeActor* ownerBridge)
{
	Super::InitializeResource(ownerBridge);
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordAndroidResource::InitializeResource - Resource initialized for bridge actor: %s"), *ownerBridge->GetName());
	OnResourceReadyForPostInit.Broadcast(this);	//	다른거 실행할 필요도 없이 그냥 호출.
}

void UVdjmRecordAndroidResource::ReleaseResources()
{
	Super::ReleaseResources();
}

void UVdjmRecordAndroidResource::ResetResource()
{
	Super::ResetResource();
}

FTextureRHIRef UVdjmRecordAndroidResource::GetCurrPooledTextureRHI()
{
	return nullptr;
}

FTextureRHIRef UVdjmRecordAndroidResource::GetNextPooledTextureRHI()
{
	return nullptr;
}

bool UVdjmRecordAndroidResource::DbcIsValidResource() const
{
	return true;
}

void UVdjmRecordAndroidResource::BeginDestroy()
{
	Super::BeginDestroy();
}

void UVdjmRecordAndroidUnit::ReleaseRecordPrevStartDelegate()
{
	if (AVdjmRecordBridgeActor* bridge = AVdjmRecordBridgeActor::TryGetRecordBridgeActor())
	{
		if (mStartRecordStepsHandle.IsValid())
		{
			bridge->OnRecordPrevStartInner.Remove(mStartRecordStepsHandle);
		}
		mStartRecordStepsHandle.Reset();
	}
}

void UVdjmRecordAndroidUnit::ReleaseRecordStartedDelegate()
{
	if (AVdjmRecordBridgeActor* bridge = AVdjmRecordBridgeActor::TryGetRecordBridgeActor())
	{
		if (mStartRecordStepsHandle.IsValid())
		{
			bridge->OnRecordStartedInner.Remove(mStartRecordStepsHandle);
		}
		mStartRecordStepsHandle.Reset();
	}
}

void UVdjmRecordAndroidUnit::RecordStartedDelegateFunc(UVdjmRecordResource* VdjmRecordResource)
{
	ReleaseRecordStartedDelegate();
	if (LinkedRecordResource.IsValid() && mAndroidEncoderImpl.IsValid())
	{
		VdjmResult result = mAndroidEncoderImpl->StartEncoder();
		if (VdjmFailed(result))
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidSurfacer::RecordPrevStart - Failed to start Android Encoder. Result code: 0x%08X"), result);
			mAndroidEncoderImpl->TerminateEncoder();
		}
		else
		{
			//	여기에 브릿지액터의 tick 을 넣으라는건가? 시발?
			UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordAndroidSurfacer::RecordPrevStart - Android Encoder started successfully."));
		}
	}
}
void UVdjmRecordAndroidUnit::PostEndPipelineExecute(const FVdjmRecordUnitParamContext& context,
	FVdjmRecordUnitParamPayload& payload)
{
}

void UVdjmRecordAndroidUnit::StopRecord()
{
	if (mAndroidEncoderImpl.IsValid())
	{
		mAndroidEncoderImpl->StopEncoder();
	}
}

bool UVdjmRecordAndroidUnit::InitializeUnit(UVdjmRecordResource* recordResource)
{
	//	멱등성 유지, RecordPrevStart 여기에서 리소스가 에러시에 무조건 호출
	if (Super::InitializeUnit(recordResource))
	{
		if (not mAndroidEncoderImpl.IsValid())
		{
			UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordAndroidSurfacer::InitializeUnit - Initializing Android Encoder."));
			mAndroidEncoderImpl = CreatePlatformVideoEncoder();
			if (AVdjmRecordBridgeActor* bridge = AVdjmRecordBridgeActor::TryGetRecordBridgeActor())
			{
				bridge->OnRecordStartRetValEvent.BindUObject(this,&UVdjmRecordAndroidUnit::RecordStartCheck);
			}
		}
		return true;
	}
	else 
	{
		return false;
	}
	
	
}

void UVdjmRecordAndroidUnit::ExecuteUnit(const FVdjmRecordUnitParamContext& context,
	FVdjmRecordUnitParamPayload& payload)
{
	if (!context.DbcIsValidUnit() || !context.GraphBuilder)
	{
		payload.bSuccess = false;
		return;
	}
	FRDGTextureRef srcTex = payload.InputTexture;
	
	if (payload.OutputTexture.IsValid())
	{
		srcTex = RegisterExternalTexture(
			*context.GraphBuilder,
			payload.OutputTexture,
			TEXT("UVdjmRecordAndroidSurfacer_ExecuteUnit_Output"));
	}
	
	if (srcTex == nullptr)
	{
		payload.bSuccess = false;
		return;
	}
	payload.bSuccess = true;
	SubmitFrameToSurfacer(*context.GraphBuilder, srcTex, context.CurrentRecordTimeSec);
	
}

void UVdjmRecordAndroidUnit::ReleaseUnit()
{
	if (mAndroidEncoderImpl.IsValid())
	{
		mAndroidEncoderImpl->TerminateEncoder();
		mAndroidEncoderImpl.Reset();
	}
	ReleaseRecordPrevStartDelegate();
	ReleaseRecordStartedDelegate();
}

bool UVdjmRecordAndroidUnit::DbcIsValidUnitInit() const
{
	if (Super::DbcIsValidUnitInit())
	{
		
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordAndroidUnit::DbcIsValidUnitInit - Super::DbcIsValidUnitInit() returned false."));
		return false;
	}
	return Super::DbcIsValidUnitInit();
}

bool UVdjmRecordAndroidUnit::DbcRecordUnitStatus() const
{
	return bInitializedStatus;
}

VdjmResult UVdjmRecordAndroidUnit::RecordStartCheck()
{
	if (not LinkedRecordResource.IsValid() || not mAndroidEncoderImpl.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidUnit::RecordStartCheck - LinkedRecordResource is not valid."));
		return VdjmResults::Fail;
	}
	//	20260409 스냅샷으로 증명하게 만들어야함.
	if (LinkedRecordResource->LinkedCurrentInfo.IsValid())
	{
		const FString CustomFileName =
			LinkedRecordResource->OwnerBridgeActor.IsValid()
				? LinkedRecordResource->OwnerBridgeActor->GetCurrentFileName()
				: FString();
		LinkedRecordResource->FinalFilePath =
			LinkedRecordResource->LinkedCurrentInfo->MakeFinalFilePath(CustomFileName);
		UE_LOG(LogVdjmRecorderCore, Log,
			TEXT("UVdjmRecordAndroidUnit::RecordStartCheck - Refreshed output path: %s"),
			*LinkedRecordResource->FinalFilePath);
	}
	
		/*
		* TODO(260410-cofigs): 위치 검증 OK (RecordStartCheck 직전 호출 지점)
		* - 현재는 LinkedRecordResource의 개별 필드를 직접 넘겨 InitializeEncoder(...)를 호출한다.
		* - 목표는 여기서 FVdjmEncoderInitRequest(또는 Android 전용 Config Snapshot)를 생성한 뒤
		*   InitializeEncoderExtended(...)로 단일 전달하는 구조로 바꾸는 것.
		* - 검증 포인트:
		*   1) FinalFilePath/Resolution/Bitrate/FPS가 모두 스냅샷 값으로 고정되는지
		*   2) Start 이후 LinkedRecordResource 변경이 인코더 런타임에 영향을 주지 않는지
		*   3) 실패 로그에 어떤 필드 검증이 깨졌는지 명확히 남는지
		*/
	if (not mAndroidEncoderImpl->InitializeEncoder(
		LinkedRecordResource->FinalFilePath,
		LinkedRecordResource->OriginResolution.X,
		LinkedRecordResource->OriginResolution.Y,
		LinkedRecordResource->FinalBitrate,
		LinkedRecordResource->FinalFrameRate))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidUnit::RecordStartCheck - Failed to initialize Android Encoder."));
		mAndroidEncoderImpl->TerminateEncoder();
		return VdjmResults::InvalidArg;
	}
	
	const VdjmResult StartResult = mAndroidEncoderImpl->StartEncoder();
	if (StartResult != VdjmResults::Ok)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordAndroidUnit::RecordStartCheck - Failed to start Android Encoder. Result=%d"),
			(int32)StartResult);
		mAndroidEncoderImpl->TerminateEncoder();
		return StartResult;
	}

	return VdjmResults::Ok;
}

void UVdjmRecordAndroidUnit::SubmitFrameToSurfacer(FRDGBuilder& graphBuilder, const FRDGTextureRef& srcTexture,
                                                   double timeStampSec)
{
	if (srcTexture == nullptr || !mAndroidEncoderImpl.IsValid())
	{
		return;
	}
	FVdjmAndroidSubmitPassParameters* passParams =
		graphBuilder.AllocParameters<FVdjmAndroidSubmitPassParameters>();
	passParams->InputTexture = srcTexture;
	
	graphBuilder.AddPass(
		RDG_EVENT_NAME("Vdjm_Android_SubmitSurface"),
		passParams,
		ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
		[
			WeakThis = TWeakObjectPtr<UVdjmRecordAndroidUnit>(this),
			PassParams = passParams,
			TimeStampSec = timeStampSec
		](FRHICommandList& RHICmdList)
		{
			if (!WeakThis.IsValid() || PassParams == nullptr || PassParams->InputTexture == nullptr)
			{
				return;
			}
	
			if (!WeakThis->mAndroidEncoderImpl.IsValid())
			{
				return;
			}
	
			FTextureRHIRef sourceRHI = PassParams->InputTexture->GetRHI();
			if (!sourceRHI.IsValid())
			{
				return;
			}
	
			
			WeakThis->mAndroidEncoderImpl->SubmitSurfaceFrame(
				RHICmdList,
				sourceRHI,
				TimeStampSec
			);
		});
}

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class UVdjmRecordAndroidResource : public UVdjmRecordResource
*/
void UVdjmAndroidRecordPipeline::InitializeRecordPipeline(UVdjmRecordResource* recordResource)
{
	Super::InitializeRecordPipeline(recordResource);
	if (recordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidResource::InitializeRecordPipeline - recordResource is null."));
		return;
	}
	
	LinkedBridgeActor = recordResource->OwnerBridgeActor;
	if (not LinkedBridgeActor.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidResource::InitializeRecordPipeline - LinkedBridgeActor is not valid."));
		return;
	}
	if (not LinkedBridgeActor->DbcValidRecordResource())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidResource::InitializeRecordPipeline - LinkedBridgeActor does not have a valid record resource."));
		return;
	}
	EVdjmRecordEnvPlatform isAndroid = AVdjmRecordBridgeActor::GetTargetPlatform();
	if (isAndroid != EVdjmRecordEnvPlatform::EAndroid)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidResource::InitializeRecordPipeline - Target platform is not Android. Current platform: %d"), (int32)isAndroid);
		return;
	}

	UVdjmRecordEnvDataAsset* dataAsset = LinkedBridgeActor->GetRecordEnvConfigureDataAsset();
	if (dataAsset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidResource::InitializeRecordPipeline - LinkedBridgeActor does not have a valid record environment data asset."));
		return;
	}
	//	그런데 이미 위에서 nullptr 을 검증하고 오는데 그냥 독립적이라 생각하고 해주자.
	FVdjmRecordEnvPlatformInfo* platformInfo = dataAsset->GetPlatformInfo(isAndroid);
	if (not ValidateForAndroidPipeline(platformInfo))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidResource::InitializeRecordPipeline - Platform info validation failed for Android pipeline."));
		return;
	}
	
	if (const TSubclassOf<UVdjmRecordUnit>* foundState = platformInfo->GetPipelineState(EVdjmRecordPipelineStages::ESurfaceEncodeAndWrite))
	{
		CreateUnit(*foundState);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidResource::InitializeRecordPipeline - Platform info does not contain a valid unit class for ESurfaceEncodeAndWrite stage."));
		return;
	}
	
}

void UVdjmAndroidRecordPipeline::ExecuteRecordPipeline(const FVdjmRecordUnitParamContext& context,
	FVdjmRecordUnitParamPayload& payload)
{
	payload.previousUnit = nullptr;
	OnBeginPipelineExecution.Broadcast(context, payload);
	
	for (TObjectPtr<UVdjmRecordUnit>& recordUnit :RecordUnits)
	{
		if (IsValid(recordUnit))
		{
			OnBeginExecuteUnit.Broadcast(context, payload);
			recordUnit->ExecuteUnit(context,payload);
			if (!payload.bSuccess)
			{
				//	Log and break on failure
				OnErrorExecuteUnit.Broadcast(context, payload);
				break;
			}
			OnEndExecuteUnit.Broadcast(context, payload);
			payload.previousUnit = recordUnit;
		}
		else
		{
			OnErrorExecuteUnit.Broadcast(context, payload);
		}
	}
	OnEndPipelineExecution.Broadcast(context, payload);
}

void UVdjmAndroidRecordPipeline::StopRecordPipelineExecution()
{
	TravelLoopUnits([](UVdjmRecordUnit* unit)->int32
	{
		if (UVdjmRecordAndroidUnit* androidUnit = Cast<UVdjmRecordAndroidUnit>(unit))
		{
			androidUnit->StopRecord();
			return 1;
		}
		else
		{
			return 0;
		}
	});
}

void UVdjmAndroidRecordPipeline::ReleaseRecordPipeline()
{
	Super::ReleaseRecordPipeline();
}

bool UVdjmAndroidRecordPipeline::DbcIsValid() const
{
	return Super::DbcIsValid();
}

bool UVdjmAndroidRecordPipeline::ValidateForAndroidPipeline(FVdjmRecordEnvPlatformInfo* platformInfo) const
{
	if (platformInfo == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmAndroidRecordPipeline::ValidateForAndroidPipeline - platformInfo is null."));
		return false;
	}
	/*
	 * TODO(260410-cofigs): 여기에 안드로이드를 벗어난 혹은 하드웨어 검증을 실시한다.
	 * 최소 사양이나 그런걸 여기에서 검사해준다 생각하면 편함.
	 */
	
	return true;
}
