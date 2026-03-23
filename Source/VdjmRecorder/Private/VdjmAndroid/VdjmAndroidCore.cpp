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

void UVdjmRecordAndroidUnit::RemoveRecordPrevStartDelegate()
{
	if (AVdjmRecordBridgeActor* bridge = AVdjmRecordBridgeActor::TryGetRecordBridgeActor())
	{
		if (mStartRecordPrepareHandle.IsValid())
		{
			bridge->OnRecordPrevStartInner.Remove(mStartRecordPrepareHandle);
		}
		mStartRecordPrepareHandle.Reset();
	}
}

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class UVdjmRecordAndroidSurfacer : public UVdjmRecordUnit
*/
void UVdjmRecordAndroidUnit::RecordPrevStart(UVdjmRecordResource* res)
{
	if (not LinkedRecordResource.IsValid())
	{
		if (res != nullptr && res->DbcIsValidResourceInit())
		{
			if (not InitializeUnit(res))
			{
				UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidSurfacer::RecordPrevStart - Failed to initialize unit with provided resource."));
				RemoveRecordPrevStartDelegate();
				return;
			}
		}
	}
	
	if (LinkedRecordResource.IsValid() && mAndroidEncoder.IsValid())
	{
		VdjmResult result = mAndroidEncoder->StartEncoder();
		if (VdjmFailed(result))
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidSurfacer::RecordPrevStart - Failed to start Android Encoder. Result code: 0x%08X"), result);
			mAndroidEncoder->TerminateEncoder();
			RemoveRecordPrevStartDelegate();
		}
	}
}

void UVdjmRecordAndroidUnit::PostEndPipelineExecute(const FVdjmRecordUnitParamContext& context,
	FVdjmRecordUnitParamPayload& payload)
{
}

void UVdjmRecordAndroidUnit::StopRecord()
{
	if (mAndroidEncoder.IsValid())
	{
		mAndroidEncoder->StopEncoder();
	}
}

bool UVdjmRecordAndroidUnit::InitializeUnit(UVdjmRecordResource* recordResource)
{
	//	멱등성 유지, RecordPrevStart 여기에서 리소스가 에러시에 무조건 호출
	if (Super::InitializeUnit(recordResource))
	{
		if (not mAndroidEncoder.IsValid())
		{
			mAndroidEncoder = CreatePlatformVideoEncoder();
			bool bSuccess = mAndroidEncoder->InitializeEncoder(
				LinkedRecordResource->FinalFilePath,
				LinkedRecordResource->OriginResolution.X,
				LinkedRecordResource->OriginResolution.Y,
				LinkedRecordResource->FinalBitrate,
				LinkedRecordResource->FinalFrameRate);
			if (not bSuccess)
			{
				UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordAndroidSurfacer::InitializeUnit - Failed to initialize Android Encoder."));
				mAndroidEncoder.Reset();
				return false;
			}
			if (AVdjmRecordBridgeActor* bridge = AVdjmRecordBridgeActor::TryGetRecordBridgeActor())
			{
				if (not mStartRecordPrepareHandle.IsValid())
				{
					mStartRecordPrepareHandle = bridge->OnRecordPrevStartInner.AddUObject(this, &UVdjmRecordAndroidUnit::RecordPrevStart);
				}
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
	Super::ReleaseUnit();
	if (mAndroidEncoder.IsValid())
	{
		mAndroidEncoder->TerminateEncoder();
		mAndroidEncoder.Reset();
	}
	RemoveRecordPrevStartDelegate();
}

bool UVdjmRecordAndroidUnit::DbcIsValidUnitInit() const
{
	return Super::DbcIsValidUnitInit();
}

void UVdjmRecordAndroidUnit::SubmitFrameToSurfacer(FRDGBuilder& graphBuilder, const FRDGTextureRef& srcTexture,
	double timeStampSec)
{
	if (srcTexture == nullptr || !mAndroidEncoder.IsValid())
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
	
			if (!WeakThis->mAndroidEncoder.IsValid())
			{
				return;
			}
	
			FTextureRHIRef sourceRHI = PassParams->InputTexture->GetRHI();
			if (!sourceRHI.IsValid())
			{
				return;
			}
	
			WeakThis->mAndroidEncoder->SubmitSurfaceFrame(
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
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordAndroidResource::InitializeRecordPipeline - Initializing pipeline with record resource for bridge actor: %s"), *recordResource->OwnerBridgeActor->GetName());
	LinkedBridgeActor = recordResource->OwnerBridgeActor;
	if (LinkedBridgeActor.IsValid() && LinkedBridgeActor->DbcValidRecordResource())
	{
		FVdjmRecordEnvPlatformInfo* platformInfo =
			LinkedBridgeActor->GetRecordEnvConfigureDataAsset()
				->GetPlatformInfo( AVdjmRecordBridgeActor::GetTargetPlatform());
		
		
		if (const TSubclassOf<UVdjmRecordUnit>* foundState = platformInfo->GetPipelineState(EVdjmRecordPipelineStages::ESurfaceEncodeAndWrite))
		{
			CreateUnit(*foundState);
		}
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

