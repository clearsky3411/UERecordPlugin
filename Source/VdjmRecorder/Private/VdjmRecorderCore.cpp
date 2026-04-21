// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmRecorderCore.h"
#include "VdjmRecordBridgeActor.h"
// Fill out your copyright notice in the Description page of Project Settings.
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "Kismet/GameplayStatics.h"
#include "VdjmRecordTypes.h"
#include "HAL/FileManager.h"
#include "VdjmRecoderEncoderImpl.h"
#include "DSP/AudioDebuggingUtilities.h"
#include "Slate/SceneViewport.h"


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓	UVdjmRecordEncoderReadBackUnit : UVdjmRecordEncoderUnit	↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::FVdjmReadBackTextureWrapper()
{
	ReadBackBuffer = nullptr;
	bHasRequest = false;
	TimeStamp = 0.0;
}

void FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::MakeRHIGPUReadback()
{
	if (ReadBackBuffer != nullptr)
	{
		ReadBackBuffer.Reset();
		ReadBackBuffer = nullptr;
	}
	
	ReadBackBuffer = MakeUnique<FRHIGPUTextureReadback>(FName("VdjmRB_NV12"));
	bHasRequest = false;
	TimeStamp = 0.0;
}

void FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::EnqueueCopy(FRHICommandList& RHICmdList, FTextureRHIRef srcTexture, double inTimeStamp)
{
	if (IsValidTexture())
	{
		ReadBackBuffer->EnqueueCopy(RHICmdList, srcTexture);
		TimeStamp = inTimeStamp;
		bHasRequest = true;
	}
}

void* FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::TextureLock(int32& outWidth, int32& outHeight) 
{
	if (IsValidTexture())
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::TextureLock - Locking texture with timestamp %f"),TimeStamp);
		return ReadBackBuffer->Lock(outWidth,&outHeight);;
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore,Warning,TEXT("FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::TextureLock - Invalid texture, cannot lock"));
		return nullptr;
	}
}

void FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::TextureUnLock() 
{
	if (IsValidTexture())
	{
		ReadBackBuffer->Unlock();
		bHasRequest = false;
	}
}

FVdjmReadBackHelper::FVdjmReadBackHelper()
{
	mCurrentWriteIndex = 0;
	mCurrentReadIndex = 0;
}

void FVdjmReadBackHelper::EnqueueFrame(FRHICommandList& RHICmdList, FTextureRHIRef SourceTexture, double TimeStamp)
{
	UE_LOG(LogVdjmRecorderCore,Log,TEXT("FVdjmReadBackHelper::EnqueueFrame - SourceTexture : %p Enqueuing frame with timestamp %f at index %d"),SourceTexture.GetReference(),TimeStamp,mCurrentWriteIndex);
	mReadBackWrappers[mCurrentWriteIndex].EnqueueCopy(
		RHICmdList,
		SourceTexture,
		TimeStamp);
	mCurrentWriteIndex = (mCurrentWriteIndex + 1) % ReadBackBufferCount;
}

void FVdjmRecordUnitParamContext::DbcSetupContextExtended(UWorld* world, UVdjmRecordEnvResolver* resolver,
	FRDGBuilder* graphBuilder, double currentRecordTimeSec)
{
	WorldContext = world;
	RecordEnvResolver = resolver;
	RecordBridge = resolver->LinkedOwnerBridge;
	RecordResource = resolver->LinkedRecordResource;
	GraphBuilder = graphBuilder;
	CurrentRecordTimeSec = currentRecordTimeSec;
}

void FVdjmReadBackHelper::Initialize()
{
	for (int i = 0; i < ReadBackBufferCount; i++)
	{
		mReadBackWrappers[i].MakeRHIGPUReadback();
	}
}

void* FVdjmReadBackHelper::TryLockOldest(int32& outWidth, int32& outHeight,double& outTimeStamp)
{
	FVdjmReadBackTextureWrapper& target = mReadBackWrappers[mCurrentReadIndex];
	
	if (target.IsReadReady())
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("FVdjmReadBackHelper::TryLockOldest -{{{ Readback ready at index %d with timestamp %f"),mCurrentReadIndex,target.TimeStamp);
		outTimeStamp = target.TimeStamp;
		return target.TextureLock(outWidth, outHeight);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("FVdjmReadBackHelper::TryLockOldest - No readback ready at index %d"),mCurrentReadIndex);
		return nullptr;
	}
}

void FVdjmReadBackHelper::UnlockOldest() 
{
	mReadBackWrappers[mCurrentReadIndex].TextureUnLock();
	mCurrentReadIndex = (mCurrentReadIndex + 1) % ReadBackBufferCount;
}

void FVdjmReadBackHelper::StopAllReadBacks()
{
	for (int i = 0; i < ReadBackBufferCount; i++)
	{
		mReadBackWrappers[i].TextureUnLock();
	}
}


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
class UVdjmRecordUnitPipeline : public UObject 		
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

bool UVdjmRecordUnitPipeline::InitializeRecordPipeline(UVdjmRecordResource* recordResource)
{
	if (recordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::InitializeRecordPipeline - recordResource is null."));
		return false;
	}
	LinkedRecordResource = recordResource;
	LinkedBridgeActor = recordResource->LinkedOwnerBridge;
	if (LinkedBridgeActor.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitPipeline::InitializeRecordPipeline - Successfully linked Bridge Actor : %s"),*LinkedBridgeActor->GetName());
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::InitializeRecordPipeline - Failed to link Bridge Actor."));
		LinkedRecordResource = nullptr;
		return false;
	}
	return true;
}

bool UVdjmRecordUnitPipeline::DbcUnitCheck() const
{
	bool result = RecordUnits.Num() > 0;
	if (not result)
	{
		UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcUnitCheck - RecordUnits.Num() == 0"));
		return false;
	}
	for (auto recordUnit:RecordUnits)
	{
		if (not recordUnit->DbcIsValidUnitInit())
		{
			UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcUnitCheck - recordUnit %s DbcIsValidUnitInit() == false"),*recordUnit->GetName());
			result = false;
			break;
		}
	}
	return result;
}

void UVdjmRecordUnitPipeline::TravelLoopUnits(TFunctionRef<int32(UVdjmRecordUnit* unit)> travelFunc) const
{
	for (TObjectPtr<UVdjmRecordUnit> Unit : RecordUnits)
	{
		if (IsValid(Unit))
		{
			if (travelFunc(Unit) < 0)
			{
				return;
			}
		}
	}
}

void UVdjmRecordUnitPipeline::ReleaseRecordPipeline()
{
	for (TObjectPtr<UVdjmRecordUnit>& Unit : RecordUnits)
	{
		if (IsValid(Unit))
		{
			// 각 유닛별 정리 함수 호출 (가상 함수)
			Unit->ReleaseUnit();
		}
	}
}

bool UVdjmRecordUnitPipeline::DbcIsValidPipelineInit() const
{
	if (DbcUnitCheck()&& LinkedRecordResource.IsValid()	&& LinkedBridgeActor.IsValid())
	{
		return true;
	}
	else
	{
		if(not DbcUnitCheck())
		{
			UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcIsValidUnitInit - DbcUnitCheck() == false"));
		}
		if (not LinkedRecordResource.IsValid())
		{
			UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcIsValidUnitInit - not LinkedRecordResource.IsValid()"));
		}
		if (not LinkedBridgeActor.IsValid())
		{
			UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcIsValidUnitInit - not LinkedBridgeActor.IsValid()"));
		}
		
		return false;
	}
	// return DbcUnitCheck()
	// 	&& LinkedRecordResource.IsValid()
	// 	&& LinkedBridgeActor.IsValid();
}

bool UVdjmRecordUnit::InitializeUnit(UVdjmRecordResource* recordResource)
{
	LinkedRecordResource = recordResource;
	return LinkedRecordResource.IsValid();
}

UVdjmRecordUnit* UVdjmRecordUnitPipeline::CreateUnit(TSubclassOf<UVdjmRecordUnit> unitCls)
{
	if (unitCls != nullptr && LinkedRecordResource.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Creating Record Unit of class : %s"),*unitCls->GetName());
		UVdjmRecordUnit* newUnit = NewObject<UVdjmRecordUnit>(this,unitCls);
		if (IsValid(newUnit))
		{
			newUnit->LinkedPipeline = this;
			RecordUnits.Add(newUnit);
			if (newUnit->InitializeUnit(LinkedRecordResource.Get()))
			{
				UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Successfully initialized Record Unit : %s"),*newUnit->GetName());
			}
			else
			{
				 UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Failed to initialize Record Unit : %s"),*newUnit->GetName());
			}
			if (newUnit->DbcIsValidUnitInit())
			{
				UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Record Unit passed DbcIsValidUnitInit check : %s"),*newUnit->GetName());
			}
			else
			{
				UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Record Unit failed DbcIsValidUnitInit check : %s"),*newUnit->GetName());
				//	유닛 초기화 실패시 일단 파이프라인에서 제거. (생성은 했지만 초기화 실패한 유닛이 파이프라인에 남아있는게 더 위험하다고 판단)
				RecordUnits.Remove(newUnit);
				return nullptr;
			}
			return newUnit;
		}
	}
	return nullptr;
}


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	class UVdjmRecordDescriptor : public UObject
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
void UVdjmRecordDescriptor::CopyForSnapshot(const UVdjmRecordDescriptor*& sourceData)
{
	bUseWindowResolution = sourceData->bUseWindowResolution;
	RecordResolution = sourceData->RecordResolution;
	FrameRate = sourceData->FrameRate;
	//BitrateMap = sourceData.BitrateMap;
	SelectedBitrateType = sourceData->SelectedBitrateType;
	FilePrefix = sourceData->FilePrefix;
	SavePathDirectoryType = sourceData->SavePathDirectoryType;
	CustomSaveDirectory = sourceData->CustomSaveDirectory;
	CustomFileSaverClass = sourceData->CustomFileSaverClass;
	RenderTargetFormat = sourceData->RenderTargetFormat;
}

FIntPoint UVdjmRecordDescriptor::GetRecordResolution() const
{
	return bUseWindowResolution ?
		       FIntPoint(
			       GEngine->GameViewport->Viewport->GetSizeXY().X,
			       GEngine->GameViewport->Viewport->GetSizeXY().Y) :
		       RecordResolution;
}

int32 UVdjmRecordDescriptor::GetRecordFrameRate() const
{
	return FrameRate;
}

int32 UVdjmRecordDescriptor::GetRecordBitrate()
{
	int32* bitratePtr = BitrateMap.Find(SelectedBitrateType);
	if (bitratePtr)
	{
		return *bitratePtr;
	}
	return 2000000; //	default 2Mbps
}

FString UVdjmRecordDescriptor::GetRecordFilePrefix()
{
	return FilePrefix;
}

FString UVdjmRecordDescriptor::GetRecordFilePath()
{
	FString baseDir;
	switch (SavePathDirectoryType)
	{
	case EVdjmRecordSavePathDirectoryType::EProjectDir:
		baseDir = FPaths::ProjectDir();
		break;
	case EVdjmRecordSavePathDirectoryType::EDocumentsDir:
		baseDir = FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/Documents/");
		break;
	case EVdjmRecordSavePathDirectoryType::EVideoDir:
		baseDir = FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/Videos/");
		break;
	case EVdjmRecordSavePathDirectoryType::ECustomDir:
		baseDir = CustomSaveDirectory;
		break;
	case EVdjmRecordSavePathDirectoryType::ECustomSaverClass:
		//	Custom Saver Class will handle path
		baseDir = TEXT("");
		break;
	default:
		baseDir = FPaths::ProjectDir();
		break;
	}
	return baseDir;
}

EPixelFormat UVdjmRecordDescriptor::GetRenderTargetPixelFormat() const
{
	switch (RenderTargetFormat)
	{
	case ETextureRenderTargetFormat::RTF_R8:
		return PF_R8;
	case ETextureRenderTargetFormat::RTF_RG8:
		return PF_R8G8;
	case ETextureRenderTargetFormat::RTF_RGBA8:
		return PF_R8G8B8A8;
	case ETextureRenderTargetFormat::RTF_R16f:
		return PF_R16F;
	case ETextureRenderTargetFormat::RTF_RG16f:
		return PF_G16R16F;
	case ETextureRenderTargetFormat::RTF_RGBA16f:
		return PF_A16B16G16R16;
	case ETextureRenderTargetFormat::RTF_R32f:
		return PF_R32_FLOAT;
	case ETextureRenderTargetFormat::RTF_RG32f:
		return PF_G32R32F;
	case ETextureRenderTargetFormat::RTF_RGBA32f:
		return PF_A32B32G32R32F;
	default:
		return PF_G8;//	TODO: check this
	}
}



/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	class UVdjmRecordEnvDataAsset :public UPrimaryDataAsset
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
// bool UVdjmRecordEnvCurrentInfo_deprecated::InitializeCurrentEnvironment(AVdjmRecordBridgeActor* ownerBridge)
// {
// 	if (ownerBridge && ownerBridge->DbcValidConfigureDataAsset())
// 	{
// 		mLinkedDataAsset = ownerBridge->GetRecordEnvConfigureDataAsset();
// 		mCurrentGlobalRules = ownerBridge->GetGlobalRules();
// 		
// 		if (FVdjmRecordEnvPlatformInfo* perPlatform = ownerBridge->GetCurrentPlatformInfo())
// 		{
// 			mCurrentPlatform = ownerBridge->GetTargetPlatform();
// 			mCurrentGlobalRules = ownerBridge->GetCurrentGlobalRules();
// 			if (perPlatform->bUseAutoTargetPlatformResolution)
// 			{
// 				FIntPoint outputResolution = FIntPoint::ZeroValue;
// 				if (ownerBridge->TryResolveViewportSize(outputResolution))
// 				{
// 					mCurrentResolution = outputResolution;
// 				}
// 				else
// 				{
// 					mCurrentResolution = FIntPoint(1920,1080);
// 					ownerBridge->CriticalErrorStop(TEXT("Failed to resolve viewport size for recording. Recording cannot proceed."));
// 					return false;
// 				}
// 			}
// 			else
// 			{
// 				mCurrentResolution = perPlatform->Resolution;
// 			}
// 			
// 			mCurrentFrameRate = perPlatform->FrameRate;
// 			mCurrentPixelFormat = perPlatform->PixelFormat;
// 			mAllBitrateMap = perPlatform->BitrateMap;
//
// 			float floatBitrate = 
// 				mAllBitrateMap.Contains(ownerBridge->SelectedBitrateType)?
// 					FVdjmFunctionLibraryHelper::ConvertToBitrateValue(mAllBitrateMap[ownerBridge->SelectedBitrateType]) :
// 					mAllBitrateMap.Contains(EVdjmRecordQualityTiers::EDefault)?
// 						mAllBitrateMap[EVdjmRecordQualityTiers::EDefault] : 2000000.0f;
// 			mCurrentBitrate = FVdjmFunctionLibraryHelper::ConvertToBitrateValue(floatBitrate);
// 			
// 			//mCurrentBitrate =
// 			mCurrentFilePrefix = perPlatform->FilePrefix;
// 			FString defaultFilePath;
// 			switch (mCurrentPlatform)
// 			{
// 			case EVdjmRecordEnvPlatform::EAndroid:
// 				defaultFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());
// 				break;
// 			case EVdjmRecordEnvPlatform::EWindows:
// 			default:
// 				defaultFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
// 				break;
// 			}
// 			if (perPlatform->CustomFileSaverClass != nullptr)
// 			{
// 				// TODO
// 			}
// 			mCurrentFilePath = defaultFilePath;
// 			return true;
// 		}
// 		else
// 		{
// 			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecorderCore::Initialize - ownerBridge->SelectedBitrateType is invalid."));
// 		}
// 	}
// 	else
// 	{
// 		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecorderCore::Initialize - ownerBridge is null."));
// 	}
// 	return false;
// }

//
// bool UVdjmRecordEnvCurrentInfo_deprecated::DbcIsValidCurrentInfo() const
// {
// 	/*
// 	 * 원래라면 mCurrentCustomFileSaverInstance 이걸 가지게 해야함.
// 	* if(mCurrentCustomFileSaverInstance)
// 	{
// 	}
// 	 */
// 	return mLinkedDataAsset.IsValid() && not mAllBitrateMap.IsEmpty();
// }

// FString UVdjmRecordEnvCurrentInfo_deprecated::MakeFinalFilePath(const FString& customFileName)
// {
// 	FString basePath;
// 	
// 	switch (mCurrentPlatform)
// 	{
// 	case EVdjmRecordEnvPlatform::EWindows:
// 		basePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
// 		break;
// 	case EVdjmRecordEnvPlatform::EAndroid:
// 		basePath =  FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());
// 		break;
// 	case EVdjmRecordEnvPlatform::EIOS:
// 		basePath = FPaths::ProjectSavedDir();
// 		break;
// 	case EVdjmRecordEnvPlatform::EMac:
// 		basePath = FPaths::ProjectSavedDir();
// 		break;
// 	case EVdjmRecordEnvPlatform::ELinux:
// 		basePath = FPaths::ProjectSavedDir();
// 		break;
// 	default:
// 		basePath = FPaths::ProjectSavedDir();
// 		break;
// 	}
// 	if(mCurrentFilePrefix.IsEmpty())
// 	{
// 		mCurrentFilePrefix = TEXT("Vcard");
// 	}
// 	if (mCurrentFilePrefix.EndsWith(TEXT("_")))
// 	{
// 		mCurrentFilePrefix.RemoveFromEnd(TEXT("_"));
// 	}
// 	FString finalPath = mCurrentFilePrefix + TEXT("_");
// 	if(customFileName.IsEmpty())
// 	{
// 		finalPath += TEXT("_")+FString::FromInt(FDateTime::Now().ToUnixTimestamp());
// 	}
// 	else
// 	{
// 		mFileName = customFileName;
// 		finalPath += customFileName + TEXT("_") + FString::FromInt(FDateTime::Now().ToUnixTimestamp());
// 	}
// 	
// 	return FPaths::Combine(basePath, finalPath + TEXT(".mp4"));
// }

bool UVdjmRecordEnvResolver::InitResolverEnvironment(AVdjmRecordBridgeActor* ownerBridge)
{
	if (ownerBridge == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::InitResolverEnvironment - ownerBridge is null."));
		return false;
	}
	LinkedOwnerBridge = ownerBridge;
	if (not ownerBridge->DbcValidConfigureDataAsset())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::InitResolverEnvironment - ownerBridge does not have a valid configure data asset."));
		return false;
	}
	SetResolvedGlobalRules(ownerBridge->GetCurrentGlobalRules());
	return true;
}

UVdjmRecordResource* UVdjmRecordEnvResolver::CreateResolvedRecordResource(const FVdjmRecordEnvPlatformPreset* presetData) 
{
	if (not LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - ownerBridge is null."));
		return nullptr;
	}
	
	if (presetData == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - presetData is null."));
		return nullptr;
	}
	
	//	FVdjmRecordEnvPlatformPreset 검증 및 해석 후 UVdjmRecordResource 생성,
	if (not ResolveEnvPlatform(presetData))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to resolve environment platform."));
		return nullptr;
	}
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Successfully resolved environment platform. Resolved Quality Tier: %s"), *StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(mResolvedQualityTier));
	
	if (UVdjmRecordResource* newResource = NewObject<UVdjmRecordResource>(this,presetData->RecordResourceClass))
	{
		newResource->LinkedOwnerBridge = LinkedOwnerBridge.Get();
		newResource->LinkedResolver = this;
		if (not newResource->InitializeResource(this))
		{
			newResource->ReleaseResources();
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to initialize record resource."));
			newResource = nullptr;
			return nullptr;
		}
		return newResource;
		
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to create record resource instance."));
		return nullptr;
	}
	
}

bool UVdjmRecordEnvResolver::InitComplete(AVdjmRecordBridgeActor* ownerBridge, UVdjmRecordResource* resource,
	UVdjmRecordUnitPipeline* pipeline)
{
	if (ownerBridge != nullptr && resource != nullptr && pipeline != nullptr)
	{
		LinkedOwnerBridge = ownerBridge;
		LinkedRecordResource = resource;
		LinkedPipeline = pipeline;
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::InitComplete - One or more parameters are null. ownerBridge: %s, resource: %s, pipeline: %s"),
			ownerBridge ? *ownerBridge->GetName() : TEXT("null"),
			resource ? *resource->GetName() : TEXT("null"),
			pipeline ? *pipeline->GetName() : TEXT("null"));
	}
	return DbcIsValidEnvResolverInit();
}

bool UVdjmRecordEnvResolver::ResolveEnvPlatform(const FVdjmRecordEnvPlatformPreset* presetData)
{
	mResolvedPreset.Clear();
	mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;

	if (presetData == nullptr || !presetData->DbcIsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform - presetData is null or invalid."));
		return false;
	}

	if (!LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform - LinkedOwnerBridge is invalid."));
		return false;
	}

	const EVdjmRecordQualityTiers requestedTier =
		(LinkedOwnerBridge->GetRequestedQualityTier() != EVdjmRecordQualityTiers::EUndefined)
			? LinkedOwnerBridge->GetRequestedQualityTier()
			: presetData->DefaultQualityTier;

	constexpr EVdjmRecordQualityTiers tierOrder[] = {
		EVdjmRecordQualityTiers::EUltra,
		EVdjmRecordQualityTiers::EHigh,
		EVdjmRecordQualityTiers::EMediumHigh,
		EVdjmRecordQualityTiers::EMedium,
		EVdjmRecordQualityTiers::EMdeiumLow,
		EVdjmRecordQualityTiers::ELow,
		EVdjmRecordQualityTiers::ELowest
	};

	auto findTierIndex = [tierOrder](EVdjmRecordQualityTiers tier) -> int32
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(tierOrder); ++i)
		{
			if (tierOrder[i] == tier)
			{
				return i;
			}
		}
		return INDEX_NONE;
	};

	int32 beginTier = findTierIndex(requestedTier);
	if (beginTier == INDEX_NONE)
	{
		beginTier = findTierIndex(presetData->DefaultQualityTier);
	}
	if (beginTier == INDEX_NONE)
	{
		beginTier = 0;
	}

	FIntPoint viewportSize = FIntPoint::ZeroValue;
	if (!LinkedOwnerBridge->TryResolveViewportSize(viewportSize) ||
		viewportSize.X <= 0 || viewportSize.Y <= 0)
	{
		viewportSize = VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution(beginTier);
	}

	const UVdjmRecordEnvDataAsset* configAsset = LinkedOwnerBridge->GetRecordEnvConfigureDataAsset();
	const FVdjmRecordGlobalRules activeRules = configAsset ? configAsset->GlobalRules : FVdjmRecordGlobalRules();
	const int32 safeDisplayRefreshHz = FMath::Max(1, activeRules.MaxFrameRate);

	const FString customFileName = LinkedOwnerBridge->GetCurrentFileName();

	auto tryResolveTier = [&](EVdjmRecordQualityTiers candidateTier, int32 candidateTierIndex) -> bool
	{
		const FVdjmEncoderInitRequest* sourceRequest = presetData->GetEncoderInitRequest(candidateTier);
		if (sourceRequest == nullptr)
		{
			return false;
		}

		FVdjmEncoderInitRequest candidateRequest = *sourceRequest;
		if (!candidateRequest.EvaluateValidation())
		{
			UE_LOG(LogVdjmRecorderCore, Warning,
				TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform - Tier %s has invalid EncoderInitRequest, skipping."),
				*StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(candidateTier));
			return false;
		}

		const FIntPoint tierMaxResolution =
			VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution(FMath::Max(0, candidateTierIndex));
		const FIntPoint presetResolution(candidateRequest.VideoConfig.Width, candidateRequest.VideoConfig.Height);
		const FIntPoint safeResolution = VdjmRecordUtils::Resolvers::ResolveVideoResolution(
			viewportSize,
			presetResolution,
			candidateRequest.VideoConfig.bResolutionFitToDisplay,
			tierMaxResolution);

		const int32 safeFrameRate = VdjmRecordUtils::Resolvers::ResolveVideoFrameRate(
			candidateRequest.VideoConfig.FrameRate,
			activeRules,
			safeDisplayRefreshHz);

		const int32 resolvedBitrateByTheory = VdjmRecordUtils::Resolvers::ResolveVideoBitrateBps(
			safeResolution,
			safeFrameRate,
			candidateTier,
			EVdjmRecordContentComplexity::EGameplay);

		int32 safeBitrate = 0;
		if (!VdjmRecordUtils::Validations::DbcValidateBitrate(
			resolvedBitrateByTheory,
			safeBitrate,
			TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform")))
		{
			return false;
		}

		candidateRequest.VideoConfig.Width = safeResolution.X;
		candidateRequest.VideoConfig.Height = safeResolution.Y;
		candidateRequest.VideoConfig.FrameRate = safeFrameRate;
		candidateRequest.VideoConfig.Bitrate = safeBitrate;

		candidateRequest.AudioConfig.ChannelCount = FMath::Clamp(candidateRequest.AudioConfig.ChannelCount, 1, 2);
		const int32 requestedSampleRate = candidateRequest.AudioConfig.SampleRate;
		if (requestedSampleRate <= 0)
		{
			candidateRequest.AudioConfig.SampleRate = 44100;
		}
		else if (requestedSampleRate > 48000)
		{
			candidateRequest.AudioConfig.SampleRate = 48000;
		}
		else if (requestedSampleRate >= 44100)
		{
			candidateRequest.AudioConfig.SampleRate = 44100;
		}
		else if (requestedSampleRate >= 32000)
		{
			candidateRequest.AudioConfig.SampleRate = 32000;
		}
		else
		{
			candidateRequest.AudioConfig.SampleRate = 44100;
		}

		const bool bMusicHeavy = candidateRequest.AudioConfig.ChannelCount >= 2;
		candidateRequest.AudioConfig.Bitrate = VdjmRecordUtils::Resolvers::ResolveAudioBitrateBps(
			candidateRequest.AudioConfig.SampleRate,
			candidateRequest.AudioConfig.ChannelCount,
			bMusicHeavy);

		FVdjmRecordEnvPlatformPreset candidatePreset = *presetData;
		candidatePreset.EncoderInitRequestMap.FindOrAdd(candidateTier) = candidateRequest;

		//	이 시점에서 CandidatePreset은 후보 Tier에 맞게 보정된 해상도와 비트레이트를 가지고 있음.
		mResolvedPreset = MoveTemp(candidatePreset);
		mResolvedQualityTier = candidateTier;

		if (not ResolvedFinalFilePath(customFileName))
		{
			mResolvedPreset.Clear();
			mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
			return false;
		}

		const FVdjmEncoderInitRequest* finalResolvedRequest = TryGetResolvedEncoderInitRequest();
		if (finalResolvedRequest == nullptr || !finalResolvedRequest->EvaluateValidation())
		{
			mResolvedPreset.Clear();
			mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
			return false;
		}

		if (!LinkedOwnerBridge->EvaluateInitRequest(finalResolvedRequest))
		{
			UE_LOG(LogVdjmRecorderCore, Warning,
				TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform - Tier %s rejected by hardware probe."),
				*StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(candidateTier));

			mResolvedPreset.Clear();
			mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
			return false;
		}

		return mResolvedPreset.DbcIsValid();
	};

	// 1) Requested/default 가 EDefault 면 그걸 먼저 시도
	const bool bTryDefaultFirst =
		requestedTier == EVdjmRecordQualityTiers::EDefault ||
		(requestedTier == EVdjmRecordQualityTiers::EUndefined &&
		 presetData->DefaultQualityTier == EVdjmRecordQualityTiers::EDefault);

	if (bTryDefaultFirst &&
		presetData->EncoderInitRequestMap.Contains(EVdjmRecordQualityTiers::EDefault))
	{
		if (tryResolveTier(EVdjmRecordQualityTiers::EDefault, 0))
		{
			mHasResolved = true;
			return true;
		}
	}

	// 2) ordered tier down
	for (int32 tierIndex = beginTier; tierIndex < UE_ARRAY_COUNT(tierOrder); ++tierIndex)
	{
		if (tryResolveTier(tierOrder[tierIndex], tierIndex))
		{
			mHasResolved = true;
			return true;
		}
	}

	// 3) 마지막 fallback 으로 EDefault
	if (presetData->EncoderInitRequestMap.Contains(EVdjmRecordQualityTiers::EDefault))
	{
		if (tryResolveTier(EVdjmRecordQualityTiers::EDefault, 0))
		{
			mHasResolved = true;
			return true;
		}
	}

	mResolvedPreset.Clear();
	mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
	return false;
}

bool UVdjmRecordEnvResolver::ResolvedFinalFilePath(const FString& customFileName)
{
	if (!LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordEnvResolver::ResolvedFinalFilePath - LinkedOwnerBridge is invalid."));
		return false;
	}

	FVdjmEncoderInitRequest* mutableRequest =
		mResolvedPreset.EncoderInitRequestMap.Find(mResolvedQualityTier);

	if (mutableRequest == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordEnvResolver::ResolvedFinalFilePath - No mutable request found for tier %s."),
			*StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(mResolvedQualityTier));
		return false;
	}

	const FString safeOutputPath = VdjmRecordUtils::Resolvers::ResolveOutputPath(
		LinkedOwnerBridge->GetTargetPlatform(),
		mutableRequest->OutputConfig.OutputFilePath,
		customFileName,
		mutableRequest->OutputConfig.SessionId,
		mutableRequest->OutputConfig.bOverwriteExists,
		TEXT("UVdjmRecordEnvResolver::ResolvedFinalFilePath"));

	if (safeOutputPath.IsEmpty())
	{
		return false;
	}

	mutableRequest->OutputConfig.OutputFilePath = safeOutputPath;
	return mutableRequest->OutputConfig.EvaluateValidation();
}

bool UVdjmRecordEnvResolver::RefreshResolvedOutputPath()
{
	if (!LinkedOwnerBridge.IsValid() || !IsValidPreset())
	{
		return false;
	}

	return ResolvedFinalFilePath(LinkedOwnerBridge->GetCurrentFileName());
}

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
§	↓		class UVdjmRecordResource : public UObject			↓
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

void UVdjmRecordResource::BeginDestroy()
{
	UObject::BeginDestroy();
	ReleaseResources();
}

bool UVdjmRecordResource::InitializeResource(UVdjmRecordEnvResolver* resolver)
{
	UVdjmRecordEnvResolver* inResolver = resolver;
	if (not LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::InitializeResource - resolver's LinkedOwnerBridge is invalid."));
		return false;
	}
	
	if (inResolver == nullptr)
	{
		if (LinkedResolver.IsValid())
		{
			inResolver = LinkedResolver.Get();
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecord Resource::InitializeResourceExtended - Using existing LinkedResolver."));
		}
		else
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::InitializeResourceExtended - resolver is null."));
			return false;
		}
	}
	
	if (not inResolver->HasResolved())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordResource::InitializeResource - Existing LinkedResolver has not resolved yet. This may lead to incomplete or default configuration being used."));
		LinkedResolver = nullptr; // Clear the invalid resolver reference
		return false;
	}
	LinkedResolver = inResolver;
	
	const FVdjmEncoderInitRequest* resolvedIniRequest = LinkedResolver->TryGetResolvedEncoderInitRequest();
	if (resolvedIniRequest == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordResource::InitializeResourceExtended - Resolved init request is null."));
		return false;
	}
	OriginResolution = FIntPoint(
		resolvedIniRequest->VideoConfig.Width,
		resolvedIniRequest->VideoConfig.Height);
	
	TextureResolution = OriginResolution;
	FinalFrameRate = resolvedIniRequest->VideoConfig.FrameRate;
	FinalBitrate = resolvedIniRequest->VideoConfig.Bitrate;
	FinalPixelFormat = resolvedIniRequest->VideoConfig.PixelFormat;
	FinalFilePath = resolvedIniRequest->OutputConfig.OutputFilePath;
	
	OnResourceReadyForPostInit.Broadcast(this);	
	OnResourceReadyForFilePath.Broadcast(this, FinalFilePath);
	return true;
}

bool UVdjmRecordResource::UpdateFinalFilePathFromResolver()
{
	if (!LinkedResolver.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordResource::UpdateFinalFilePathFromResolver - LinkedResolver is invalid."));
		return false;
	}

	const FVdjmEncoderInitRequestOutput* outputConfig = LinkedResolver->TryGetResolvedOutputConfig();
	if (outputConfig == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordResource::UpdateFinalFilePathFromResolver - Output config is null."));
		return false;
	}

	FString safeOutputFilePath;
	if (!VdjmRecordUtils::Validations::DbcValidateOutputFilePath(
		outputConfig->OutputFilePath,
		safeOutputFilePath,
		TEXT("UVdjmRecordResource::UpdateFinalFilePathFromResolver")))
	{
		return false;
	}

	FinalFilePath = safeOutputFilePath;
	return true;
}

void UVdjmRecordResource::ResetResource()
{
}

void UVdjmRecordResource::ReleaseResources()
{
	LinkedOwnerBridge = nullptr;
	LinkedResolver = nullptr;
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordResource::ReleaseResources - Resources released."));
}

FTextureRHIRef UVdjmRecordResource::CreateTextureForNV12(FIntPoint resolution, EPixelFormat pixelformat,ETextureCreateFlags createFlags)
{
	OriginResolution = resolution;
	FIntPoint NV12Resolution;
	NV12Resolution.X = resolution.X;
	NV12Resolution.Y = resolution.Y * 3 / 2; // 여기에서 늘려줌. 
	
	TextureResolution = NV12Resolution;	//	Fixed for NV12
	
	EPixelFormat Nv12Format = EPixelFormat::PF_G8;
	FinalPixelFormat = Nv12Format;	//	Fixed for NV12
	
	CachedGroupCount = FIntVector(
		FMath::DivideAndRoundUp(NV12Resolution.X/2, 8),
		FMath::DivideAndRoundUp(NV12Resolution.Y/2, 8),
		1);	//	fixed for NV12
	
	FRHITextureCreateDesc texDesc = FRHITextureCreateDesc::Create2D(TEXT("VdjmRecordResource_RenderTarget"), NV12Resolution,Nv12Format);
	texDesc.SetFlags(createFlags);
	
	return RHICreateTexture(texDesc);
}


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
§	↓		class AVdjmRecordBridgeActor : public AActor		↓
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

bool VdjmRecordUtils::Validations::DbcValidateResolution(const FIntPoint& inResolution, FIntPoint& outSafeResolution,
	const TCHAR* debugOwner)
{
	outSafeResolution = FIntPoint::ZeroValue;

	if (inResolution.X <= 0 || inResolution.Y <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Invalid resolution. X=%d Y=%d"),
		       debugOwner,
		       inResolution.X,
		       inResolution.Y);
		return false;
	}

	// 영상 인코더/서피스 계열에서 짝수 해상도를 요구하는 경우가 많으므로 방어적으로 보정
	const int32 SafeX = FMath::Max(2, inResolution.X & ~1);
	const int32 SafeY = FMath::Max(2, inResolution.Y & ~1);

	if (SafeX <= 0 || SafeY <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Safe resolution collapsed to invalid value. X=%d Y=%d"),
		       debugOwner,
		       SafeX,
		       SafeY);
		return false;
	}

	outSafeResolution = FIntPoint(SafeX, SafeY);
	return true;
}

bool VdjmRecordUtils::Validations::DbcValidateBitrate(const int32 inBitrate, int32& outSafeBitrate, const TCHAR* debugOwner)
{
	outSafeBitrate = 0;

	if (inBitrate <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Invalid bitrate. Bitrate=%d"),
		       debugOwner,
		       inBitrate);
		return false;
	}

	// 지나치게 작은 값/비정상 큰 값 방어
	constexpr int32 MinBitrate = 100000;      // 100 Kbps
	constexpr int32 MaxBitrate = 100000000;   // 100 Mbps

	if (inBitrate < MinBitrate || inBitrate > MaxBitrate)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Bitrate out of safe range. Bitrate=%d Range=[%d,%d]"),
		       debugOwner,
		       inBitrate,
		       MinBitrate,
		       MaxBitrate);
		return false;
	}

	outSafeBitrate = inBitrate;
	return true;
}

bool VdjmRecordUtils::Validations::DbcValidateOutputFilePath(const FString& inFilePath, FString& outSafeFilePath,
	const TCHAR* debugOwner)
{
	outSafeFilePath.Reset();

	FString SafePath = inFilePath;
	SafePath.TrimStartAndEndInline();

	if (SafePath.IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file path is empty."),
		       debugOwner);
		return false;
	}

	FPaths::NormalizeFilename(SafePath);

	// 상대경로면 한 번 절대경로화 시도
	if (FPaths::IsRelative(SafePath))
	{
		SafePath = FPaths::ConvertRelativePathToFull(SafePath);
		FPaths::NormalizeFilename(SafePath);
	}

	if (FPaths::IsRelative(SafePath))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file path is still relative after normalization. Path=%s"),
		       debugOwner,
		       *SafePath);
		return false;
	}

	const FString DirectoryPath = FPaths::GetPath(SafePath);
	if (DirectoryPath.IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file path has no parent directory. Path=%s"),
		       debugOwner,
		       *SafePath);
		return false;
	}

	// 디렉터리가 없으면 생성 시도
	if (!IFileManager::Get().DirectoryExists(*DirectoryPath))
	{
		if (!IFileManager::Get().MakeDirectory(*DirectoryPath, true))
		{
			UE_LOG(LogVdjmRecorderCore, Error,
			       TEXT("%s - Failed to create output directory. Directory=%s"),
			       debugOwner,
			       *DirectoryPath);
			return false;
		}
	}

	const FString CleanFilename = FPaths::GetCleanFilename(SafePath);
	if (CleanFilename.IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file name is empty. Path=%s"),
		       debugOwner,
		       *SafePath);
		return false;
	}

	const FString extension = FPaths::GetExtension(SafePath, false).ToLower();
	bool bIsAllowedOutputExtension = false;
#if PLATFORM_ANDROID
	bIsAllowedOutputExtension = extension == TEXT("mp4");
#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_IOS
	bIsAllowedOutputExtension = extension == TEXT("mp4") || extension == TEXT("mov");
#else
	bIsAllowedOutputExtension = extension == TEXT("mp4");
#endif

	if (!bIsAllowedOutputExtension)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Invalid output extension. Extension=%s Path=%s"),
		       debugOwner,
		       *extension,
		       *SafePath);
		return false;
	}

	static const TCHAR* InvalidChars = TEXT("<>:\"|?*");
	for (TCHAR Ch : CleanFilename)
	{
		if (FCString::Strchr(InvalidChars, Ch) != nullptr)
		{
			UE_LOG(LogVdjmRecorderCore, Error,
			       TEXT("%s - Output file name contains invalid character '%c'. File=%s"),
			       debugOwner,
			       Ch,
			       *CleanFilename);
			return false;
		}
	}

	outSafeFilePath = SafePath;
	return true;
}

bool VdjmRecordUtils::Validations::DbcValidateAudioConfig(const FVdjmEncoderInitRequestAudio& inAudioConfig,
	const TCHAR* debugOwner)
{
	if (!inAudioConfig.bEnableInternalAudioCapture)
	{
		return true;
	}

	if (inAudioConfig.AudioMimeType.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("%s - Audio mime type is empty."), debugOwner);
		return false;
	}

	constexpr int32 MinSampleRate = 8000;
	constexpr int32 MaxSampleRate = 192000;
	if (inAudioConfig.SampleRate < MinSampleRate || inAudioConfig.SampleRate > MaxSampleRate)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("%s - Audio sample rate out of range. SampleRate=%d Range=[%d,%d]"),
			debugOwner,
			inAudioConfig.SampleRate,
			MinSampleRate,
			MaxSampleRate);
		return false;
	}

	if (inAudioConfig.ChannelCount <= 0 || inAudioConfig.ChannelCount > 2)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("%s - Audio channel count out of range. ChannelCount=%d (supported 1~2)"),
			debugOwner,
			inAudioConfig.ChannelCount);
		return false;
	}

	int32 SafeBitrate = 0;
	if (!VdjmRecordUtils::Validations::DbcValidateBitrate(inAudioConfig.Bitrate, SafeBitrate, debugOwner))
	{
		return false;
	}

	if (inAudioConfig.AacProfile <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("%s - Invalid AAC profile. AacProfile=%d"),
			debugOwner,
			inAudioConfig.AacProfile);
		return false;
	}

	if (inAudioConfig.SourceSubMixName.IsNone())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("%s - Source submix name is empty."), debugOwner);
		return false;
	}

	return true;
}
