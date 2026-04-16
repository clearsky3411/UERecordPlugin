// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmRecorderCore.h"
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

void UVdjmRecordUnitPipeline::InitializeRecordPipeline(UVdjmRecordResource* recordResource)
{
	LinkedRecordResource = recordResource;
	LinkedBridgeActor = recordResource->LinkedOwnerBridge;
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

bool UVdjmRecordUnitPipeline::DbcIsValid() const
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
	const FVdjmRecordEnvPlatformPreset* evaluatePreset = presetData;
	if (not LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - ownerBridge is null."));
		return nullptr;
	}
	
	if (evaluatePreset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - presetData is null."));
		return nullptr;
	}
	
	//	FVdjmRecordEnvPlatformPreset 검증 및 해석 후 UVdjmRecordResource 생성,
	if (not ResolveEnvPlatform(evaluatePreset))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to resolve environment platform."));
		return nullptr;
	}
	
	if (UVdjmRecordResource* newResource = NewObject<UVdjmRecordResource>(this,presetData->RecordResourceClass))
	{
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
	return DbcIsValidInitEnvResolver();
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
		(LinkedOwnerBridge->SelectedBitrateType != EVdjmRecordQualityTiers::EUndefined)
			? LinkedOwnerBridge->SelectedBitrateType
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

	auto findTierIndex = [](EVdjmRecordQualityTiers tier) -> int32
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

		if (!ResolvedFinalFilePath(customFileName))
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
			return true;
		}
	}

	// 2) ordered tier down
	for (int32 tierIndex = beginTier; tierIndex < UE_ARRAY_COUNT(tierOrder); ++tierIndex)
	{
		if (tryResolveTier(tierOrder[tierIndex], tierIndex))
		{
			return true;
		}
	}

	// 3) 마지막 fallback 으로 EDefault
	if (presetData->EncoderInitRequestMap.Contains(EVdjmRecordQualityTiers::EDefault))
	{
		if (tryResolveTier(EVdjmRecordQualityTiers::EDefault, 0))
		{
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

bool UVdjmRecordResource::InitializeResourceExtended(UVdjmRecordEnvResolver* resolver)
{
	if (resolver == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::InitializeResourceExtended - resolver is null."));
		return false;
	}
	
	LinkedOwnerBridge = resolver->LinkedOwnerBridge;
	if (not LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::InitializeResourceExtended - resolver's LinkedOwnerBridge is invalid."));
		return false;
	}
	LinkedResolver = resolver;
	
	const FVdjmEncoderInitRequest* resolvedIniRequest = resolver->TryGetResolvedEncoderInitRequest();
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
	LinkedCurrentInfo_deprecate = nullptr;
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

UVdjmRecordEnvDataAsset* AVdjmRecordBridgeActor::TryGetRecordEnvConfigure()
{
	/*
	 *  /Script/VdjmRecorder.VdjmRecordEnvDataAsset'/Game/Temp/VdjmTestDataAsset.VdjmTestDataAsset'
	 *  /Script/VdjmRecorder.VdjmRecordEnvDataAsset'/Game/Temp/VdjmTestDataAsset.VdjmTestDataAsset'
	 *  
	 *
	 * /Script/VdjmRecorder.VdjmRecordEnvDataAsset'/VdjmMobileUi/Record/Bp_VdjmRecordConfigDataAsset.Bp_VdjmRecordConfigDataAsset'
	 */
	return FVdjmFunctionLibraryHelper::TryGetRecordConfigureDataAsset<UVdjmRecordEnvDataAsset>(FSoftObjectPath(TEXT("/Script/VdjmRecorder.VdjmRecordEnvDataAsset'/Game/Temp/VdjmTestDataAsset.VdjmTestDataAsset'")));
}

AVdjmRecordBridgeActor* AVdjmRecordBridgeActor::TryGetRecordBridgeActor(UWorld* worldContext)
{
	UWorld* world = worldContext ? worldContext : GEngine->GetCurrentPlayWorld();
	if (world)
	{
		AVdjmRecordBridgeActor* result = nullptr;
		TArray<AActor*> foundActors;
		UGameplayStatics::GetAllActorsOfClass(world, AVdjmRecordBridgeActor::StaticClass(), foundActors);
		for (AActor* actor : foundActors)
		{
			if (AVdjmRecordBridgeActor* castedActor = Cast<AVdjmRecordBridgeActor>(actor))
			{
				result = castedActor;
				break;
			}
		}
		return result;
	}
	return nullptr;
}

AVdjmRecordBridgeActor::AVdjmRecordBridgeActor(): mTargetViewport(nullptr)
{
	PrimaryActorTick.bCanEverTick = true;
	mRootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = mRootScene;
	bIsRecording = false;
}

void AVdjmRecordBridgeActor::BeginDestroy()
{
	Super::BeginDestroy();
	if (mRecordPipeline)
	{
		mRecordPipeline->ReleaseRecordPipeline();
	}
	if (mRecordResource)
	{
		mRecordResource->ReleaseResources();
	}
	
}

void AVdjmRecordBridgeActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	
}

// UVdjmRecordDescriptor* AVdjmRecordBridgeActor::DbcGetCurrentRecordDesc() 
// {
// 	UVdjmRecordDescriptor* result = mCurrentRecordDescriptor;
// 	if (mCurrentRecordDescriptor == nullptr)
// 	{
// 		mCurrentRecordDescriptor = NewObject<UVdjmRecordDescriptor>(this,UVdjmRecordDescriptor::StaticClass());
// 	}
// 	return result;
// }

void AVdjmRecordBridgeActor::PrintLogErrors()
{
	if (mRecordConfigureDataAsset == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("mRecordConfigureDataAsset == nullptr"));
	}
	if (mEnvResolver == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("mEnvResolver == nullptr"));
	}
	if (bIsRecording)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartRecording - bIsRecording == true"));
	}
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartRecording - mRecordResource == nullptr"));
	}
	if (mRecordPipeline == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartRecording - mRecordPipeline == nullptr"));
	}
	else if (not mRecordPipeline->DbcIsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("StartRecording - mRecordPipeline->DbcIsValid == false"));
	}
	if (not DbcRecordingPossible())
	{
		UE_LOG(LogTemp, Warning, TEXT("StartRecording - 1   DbcRecordingPossible == false"));
		if (not DbcValidRecordPipeline())
		{
			UE_LOG(LogTemp, Warning, TEXT("StartRecording - 2       DbcValidRecordPipeline() == false"));
			if (not DbcValidRecordResource())
			{
				UE_LOG(LogTemp, Warning, TEXT("StartRecording - 3           DbcValidRecordResource() == false"));
				if (mRecordResource == nullptr)
				{
					UE_LOG(LogTemp, Warning, TEXT("StartRecording - 4               mRecordResource == nullptr "));
				}
				if (mRecordResource != nullptr && not mRecordResource->DbcIsValidResourceInit())
				{
					UE_LOG(LogTemp, Warning, TEXT("StartRecording - 4               mRecordResource->DbcIsValidResource() "));
					if (not mRecordResource->LinkedOwnerBridge.IsValid())
					{
						UE_LOG(LogTemp, Warning, TEXT("StartRecording - 5			       not mRecordResource->OwnerBridgeActor.IsValid() "));
					}
					if (not mRecordResource->LinkedCurrentInfo_deprecate.IsValid())
					{
						UE_LOG(LogTemp, Warning, TEXT("StartRecording - 5			       not mRecordResource->LinkedCurrentInfo.IsValid() "));
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("StartRecording - 5			       mTexturePoolRHI.IsEmpty() "));
					}
				}
			}
			if (mRecordPipeline == nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("StartRecording - 3           mRecordPipeline == nullptr"));
			} 
			if (mRecordPipeline != nullptr && not mRecordPipeline->DbcIsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("StartRecording - 3            mRecordPipeline->DbcIsValid()"));
			}
		}
		if (not DbcValidRecordPreset())
		{
			UE_LOG(LogTemp, Warning, TEXT("StartRecording - 2       DbcValidRecordPreset() == false"));
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("StartRecording - Cannot start recording: not startable"));
}

void AVdjmRecordBridgeActor::UnBindBackBufferReady(FSlateApplication& slateApp)
{
	
	if (mBackBufferDelegateHandle.IsValid())
	{
		slateApp.GetRenderer()->OnBackBufferReadyToPresent().Remove(mBackBufferDelegateHandle);
		UE_LOG(LogTemp, Warning, TEXT("OnBindSlateBackBufferReadyToPresentEvent - Removed existing BackBufferReadyToPresent delegate"));
	}

}

bool AVdjmRecordBridgeActor::BindingRecordPipeline(TSubclassOf<UVdjmRecordUnitPipeline> pipelineClass,UVdjmRecordResource* recordResource)
{
	if (pipelineClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindingRecordPipeline - pipelineClass is null"));
		return false;
	}
	if (recordResource == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindingRecordPipeline - recordResource is null"));
		return false;
	}
	
	if (mRecordPipeline)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindingRecordPipeline - mRecordPipeline already exists. Unbinding existing pipeline before binding new one."));
		UnBindingRecordPipeline();
	}
	
	UVdjmRecordUnitPipeline* newPipeline = NewObject<UVdjmRecordUnitPipeline>(this, pipelineClass);
	if (newPipeline == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindingRecordPipeline - Failed to create record pipeline instance"));
		return false;
	}
	newPipeline->InitializeRecordPipeline(recordResource);
	
	mRecordPipeline = newPipeline;
	return true;
}

void AVdjmRecordBridgeActor::UnBindingRecordPipeline()
{
	if (IsValid(mRecordPipeline))
	{
		mRecordPipeline->ReleaseRecordPipeline();
		mRecordPipeline = nullptr;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UnBindingRecordPipeline - No existing record pipeline to unbind"));
	}
}

void AVdjmRecordBridgeActor::OnBindSlateBackBufferReadyToPresentEvent()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& slateApp = FSlateApplication::Get();
		UnBindBackBufferReady(slateApp);
		mBackBufferDelegateHandle = slateApp.GetRenderer()->OnBackBufferReadyToPresent().AddUObject(this,&AVdjmRecordBridgeActor::OnBackBufferReady_RenderThread);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OnBindSlateBackBufferReadyToPresentEvent - Cannot bind BackBufferReadyToPresent event: Slate application not initialized"));
	}
}

void AVdjmRecordBridgeActor::StartRecording()
{
	if(DbcRecordStartable())
	{
		if (!mEnvResolver->RefreshResolvedOutputPath())
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("StartRecording - Failed to refresh resolved output path."));
			bIsRecording = false;
			return;
		}
		if (mRecordResource != nullptr && !mRecordResource->UpdateFinalFilePathFromResolver())
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("StartRecording - Failed to update resource final file path from resolver."));
			bIsRecording = false;
			return;
		}
		
		const FVdjmEncoderInitRequestVideo* videoConfig = mEnvResolver->TryGetResolvedVideoConfig();
		const double minFrameRate = mEnvResolver->GetResolvedGlobalRules().MinFrameRate;
		const double maxDurationSecond = mEnvResolver->GetResolvedGlobalRules().MaxRecordDurationSeconds;
		const FVdjmEncoderInitRequestAudio* audioConfig = mEnvResolver->TryGetResolvedAudioConfig();
		const FVdjmEncoderInitRequestOutput* outputConfig = mEnvResolver->TryGetResolvedOutputConfig();
		if (videoConfig == nullptr || audioConfig == nullptr || outputConfig == nullptr)
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("StartRecording - Resolved configs are invalid."));
			bIsRecording = false;
			return;
		}

		//	Dbc 임. DbcRecordStartable 에 videoConfig, audioConfig, outputConfig 의 유효성은 이미 체크되어있음.
		if (FSlateApplication::IsInitialized())
		{
			double now = FPlatformTime::Seconds();
			
			double videoFrameRate = videoConfig->FrameRate;
			
			mFrameInterval = 1.0 / FMath::Max(videoFrameRate,minFrameRate);
			mRecordEndTime = now + maxDurationSecond;
            mNextFrameTime = now; // 첫 프레임은 즉시 기록
			bIsRecording = true;
			mRecordedFrameCount = 0;
	
			if (OnRecordStartRetValEvent.IsBound())
			{
				VdjmResult result = OnRecordStartRetValEvent.Execute();
				if (result != VdjmResults::Ok)
				{
					UE_LOG(LogTemp, Warning, TEXT("StartRecording - OnRecordStartRetValEvent returned failure result. Result=%d"), static_cast<int32>(result));
					/*
					 * 구조 변경해서 window 쪽도 바꿔줘야함. 지금 오로지 안드로이드를 위한거임.
					 */
					bIsRecording = false;
					StopRecordingInternal();
					return;
				}
				
				OnBindSlateBackBufferReadyToPresentEvent();
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot start recording: Slate application not initialized"));
			bIsRecording = false;
			return;
		}
	}
	else
	{
		PrintLogErrors();
	}
}

void AVdjmRecordBridgeActor::StopRecording()
{
	if (not bIsRecording)
	{
		return;
	}
	bIsRecording = false;
	
	FVdjmEncoderStatus::DbcGameThreadTask([weakThis = TWeakObjectPtr<AVdjmRecordBridgeActor>(this)]()
	{
		if (weakThis.IsValid())
		{
			weakThis->StopRecordingInternal();
		}
	});
}

void AVdjmRecordBridgeActor::OnStopSlateBackBufferReadyToPresentEvent()
{
	if (FSlateApplication::IsInitialized() && mBackBufferDelegateHandle.IsValid())
	{
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(mBackBufferDelegateHandle);

		mBackBufferDelegateHandle.Reset();
	}
}

bool AVdjmRecordBridgeActor::IsCompleteChainInit() const
{
	bool result = (mRecordConfigureDataAsset != nullptr && mRecordConfigureDataAsset->DbcPlatformPresetValid());
	result &= (mEnvResolver != nullptr && mEnvResolver->DbcIsValidInitEnvResolver());
	result &= (mRecordResource != nullptr && mRecordResource->DbcIsValidResourceInit());
	result &= (mRecordPipeline != nullptr && mRecordPipeline->DbcIsValid());
	return result;
}

void AVdjmRecordBridgeActor::StopRecordingInternal()
{
	OnStopSlateBackBufferReadyToPresentEvent();

	FlushRenderingCommands();

	if (mRecordPipeline)
	{
		mRecordPipeline->StopRecordPipelineExecution();
	}

	if (mRecordResource)
	{
		OnRecordStopped.Broadcast(mRecordResource);
		mRecordResource->ResetResource();
	}
}

void AVdjmRecordBridgeActor::OnResourceReadyForPostInit(UVdjmRecordResource* resource)
{
	if (resource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::OnResourceReadyForPostInit - resource is null."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::EPostResourceInitResolve);
}

void AVdjmRecordBridgeActor::OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTextureRHIRef& BackBuffer)
{
	if (!bIsRecording)
	{
		return;
	}
	UE_LOG(LogTemp, Verbose, TEXT("OnBackBufferReady_RenderThread - BackBuffer ready"));
	
	double Now = FPlatformTime::Seconds();
	if (Now > mRecordEndTime)
	{
		UE_LOG(LogTemp, Verbose, TEXT("OnBackBufferReady_RenderThread - Recording time exceeded"));
		StopRecording();
		return;
	}
	if (Now < mNextFrameTime)
	{
		UE_LOG(LogTemp, Verbose, TEXT("OnBackBufferReady_RenderThread - Frame interval not reached"));
		return;
	}
	
	++mRecordedFrameCount;
	
	mNextFrameTime = Now + mFrameInterval;
	float deltaTime = mNextFrameTime - Now;
	FVdjmRecordUnitParamContext recordUnitContext = {};
	FRDGBuilder RDGBuilder(FRHICommandListExecutor::GetImmediateCommandList());
	
	// recordUnitContext.DbcSetupContext_deprecated(
	// 	GetWorld()
	// 	,this
	// 	,mCurrentEnvInfo_deprecated
	// 	,mRecordResource
	// 	,&RDGBuilder
	// 	,Now);
	
	recordUnitContext.DbcSetupContextExtended(GetWorld(),mEnvResolver,&RDGBuilder,Now);
		
	
	/*	slate 단계중 BackBuffer를 가져온다. 이곳의 backBuffer 가 inputTexture 임. output 으로는 cs 단계의 readBack 을 줌	*/
	FVdjmRecordUnitParamPayload newPayload = {};
	
	newPayload.InputTexture = RegisterExternalTexture(
		RDGBuilder,
		BackBuffer,
		TEXT("VdjmRecordBridgeActor_BackBuffer"));
	
	//newPayload.LogString.Appendf(TEXT("{{ OnBackBufferReady_RenderThread - Captured BackBuffer Texture \n"));
	
	mRecordPipeline->ExecuteRecordPipeline(recordUnitContext, newPayload);
	
	FVdjmEncoderStatus::DbcGameThreadTask([
		weakThis = TWeakObjectPtr<AVdjmRecordBridgeActor>(this),
		deltaTime]()
	{
		if (weakThis.IsValid())
		{
			weakThis->BroadcastRecordTick(deltaTime);
		}
	});
	
	RDGBuilder.Execute(); 
}

EVdjmRecordEnvPlatform AVdjmRecordBridgeActor::GetTargetPlatform()
{
	return VdjmRecordUtils::Platforms::GetTargetPlatform();
}

bool AVdjmRecordBridgeActor::EvaluateInitRequest(const FVdjmEncoderInitRequest* initPreset)
{
	if (initPreset == nullptr)
	{
		return false;
	}
	if (!initPreset->EvaluateValidation())
	{
		return false;
	}

	const FVdjmEncoderInitRequestVideo& videoConfig = initPreset->VideoConfig;
	const FVdjmEncoderInitRequestAudio& audioConfig = initPreset->AudioConfig;

	FIntPoint safeResolution;
	if (!VdjmRecordUtils::Validations::DbcValidateResolution(
		FIntPoint(videoConfig.Width, videoConfig.Height),
		safeResolution,
		TEXT("AVdjmRecordBridgeActor::EvaluateInitRequest")))
	{
		return false;
	}

	FIntPoint viewportSize;
	if (TryResolveViewportSize(viewportSize) && viewportSize.X > 0 && viewportSize.Y > 0)
	{
		const int64 requestedPixels = static_cast<int64>(safeResolution.X) * static_cast<int64>(safeResolution.Y);
		const int64 viewportPixels = static_cast<int64>(viewportSize.X) * static_cast<int64>(viewportSize.Y);
		if (requestedPixels > (viewportPixels * 4))
		{
			UE_LOG(LogVdjmRecorderCore, Warning,
				TEXT("AVdjmRecordBridgeActor::EvaluateInitRequest - Request is too large for current viewport. Req=%dx%d View=%dx%d"),
				safeResolution.X, safeResolution.Y, viewportSize.X, viewportSize.Y);
			return false;
		}
	}

	if (!VdjmRecordUtils::Validations::DbcValidateAudioConfig(
		audioConfig,
		TEXT("AVdjmRecordBridgeActor::EvaluateInitRequest")))
	{
		return false;
	}

	return true;
}

bool AVdjmRecordBridgeActor::TryResolveViewportSize(FIntPoint& outSize) const
{
	outSize = FIntPoint::ZeroValue;
	if (mTargetViewport)
	{
		const FIntPoint size = mTargetViewport->GetSizeXY();
		if (size.X > 0 && size.Y > 0)
		{
			outSize = size;
			return true;
		}
	}
	if (const UWorld* world = GetWorld())
	{
		if (UGameViewportClient* viewportClient = world->GetGameViewport())
		{
			if (FViewport* viewport = viewportClient->Viewport)
			{
				const FIntPoint size = viewport->GetSizeXY();
				if (size.X > 0 && size.Y > 0)
				{
					outSize = size;
					return true;
				}
			}
		}
	}
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint size = GEngine->GameViewport->Viewport->GetSizeXY();
		if (size.X > 0 && size.Y > 0)
		{
			outSize = size;
			return true;
		}
	}
	return false;
}

const TCHAR* AVdjmRecordBridgeActor::GetInitStepName(EVdjmRecordBridgeInitStep step)
{
	switch (step)
	{
	case EVdjmRecordBridgeInitStep::EInitializeStart: return TEXT("InitializeStart");
	case EVdjmRecordBridgeInitStep::EInitializeWorldParts: return TEXT("InitializeWorldParts");
	case EVdjmRecordBridgeInitStep::EInitializeCurrentEnvironment: return TEXT("InitializeCurrentEnvironment");
	case EVdjmRecordBridgeInitStep::ECreateRecordResource: return TEXT("CreateRecordResource");
	case EVdjmRecordBridgeInitStep::EPostResourceInitResolve: return TEXT("PostResourceInitResolve");
	case EVdjmRecordBridgeInitStep::ECreatePipelines: return TEXT("ECreatePipelines");
	case EVdjmRecordBridgeInitStep::EFinalizeInitialization: return TEXT("FinalizeInitialization");
	case EVdjmRecordBridgeInitStep::EInitErrorEnd:	return TEXT("EInitErrorEnd");	
	case EVdjmRecordBridgeInitStep::EInitError:	return TEXT("EInitError");
	case EVdjmRecordBridgeInitStep::EComplete:	return TEXT("EComplete");
	default: return TEXT("Unknown");
	}
}

void AVdjmRecordBridgeActor::BeginPlay()
{
	Super::BeginPlay();
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitializeWorldParts);
}

void AVdjmRecordBridgeActor::OnTryChainInitNext(EVdjmRecordBridgeInitStep nextStep)
{
	EVdjmRecordBridgeInitStep prevInitStep = mCurrentInitStep;
	mCurrentInitStep = nextStep;
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("OnTryChainInitNext - Transitioning from step { %s } to step { %s }"), GetInitStepName(prevInitStep), GetInitStepName(mCurrentInitStep));
	
	if (mChainInitTimerHandle.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("OnTryChainInitNext - Clearing existing timer for step { %s }"), GetInitStepName(prevInitStep));
		GetWorld()->GetTimerManager().ClearTimer(mChainInitTimerHandle);
	}
	
	if (mChainTryInitCount < 1)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("OnTryChainInitNext - Exceeded maximum retry attempts. Transitioning to EInitErrorEnd."));
		mCurrentInitStep = EVdjmRecordBridgeInitStep::EInitErrorEnd;
		return;
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("OnTryChainInitNext - Attempting step { %s }. Remaining attempts: %d"), GetInitStepName(mCurrentInitStep), mChainTryInitCount);
		mRetryStep = prevInitStep;
	}
	
	if (UWorld* worldContext = GetWorld())
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("OnTryChainInitNext - Successfully obtained world context for step { %s }"), GetInitStepName(mCurrentInitStep));
		switch (mCurrentInitStep){
		case EVdjmRecordBridgeInitStep::EInitErrorEnd:
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("OnTryChainInitNext - Initialization failed after exhausting all retry attempts."));
			OnInitErrorEndEvent.Broadcast(this);
			break;
		case EVdjmRecordBridgeInitStep::EInitError:
			--mChainTryInitCount;
			OnInitErrorEvent.Broadcast(this,prevInitStep, mChainTryInitCount);
			OnTryChainInitNext(mRetryStep);
			break;
		case EVdjmRecordBridgeInitStep::EInitializeStart:
			UE_LOG(LogVdjmRecorderCore, Log, TEXT("OnTryChainInitNext - Starting initialization process."));
			OnInitStartEvent.Broadcast(this);
			break;
		case EVdjmRecordBridgeInitStep::EInitializeWorldParts:
			mChainInitTimerHandle = worldContext->GetTimerManager().SetTimerForNextTick(this, &AVdjmRecordBridgeActor::ChainInit_InitializeWorldParts);
			break;
		case EVdjmRecordBridgeInitStep::EInitializeCurrentEnvironment:
			mChainInitTimerHandle = worldContext->GetTimerManager().SetTimerForNextTick(this, &AVdjmRecordBridgeActor::ChainInit_InitializeCurrentEnvironment);
			break;
		case EVdjmRecordBridgeInitStep::ECreateRecordResource:
			mChainInitTimerHandle = worldContext->GetTimerManager().SetTimerForNextTick(this, &AVdjmRecordBridgeActor::ChainInit_CreateRecordResource);
			break;
		case EVdjmRecordBridgeInitStep::EPostResourceInitResolve:
			mChainInitTimerHandle = worldContext->GetTimerManager().SetTimerForNextTick(this, &AVdjmRecordBridgeActor::ChainInit_PostResourceInitResolve);
			break;
		case EVdjmRecordBridgeInitStep::ECreatePipelines:
			mChainInitTimerHandle = worldContext->GetTimerManager().SetTimerForNextTick(this, &AVdjmRecordBridgeActor::ChainInit_CreateRecordPipeline);
			break;
		case EVdjmRecordBridgeInitStep::EFinalizeInitialization:
			mChainInitTimerHandle = worldContext->GetTimerManager().SetTimerForNextTick(this, &AVdjmRecordBridgeActor::ChainInit_FinalizeInitialization);
			break;
		case EVdjmRecordBridgeInitStep::EComplete:
			//	여기에서 뭐 해줘야하나?
			bValidateInitializeComplete = true;
			mChainInitTimerHandle.Invalidate();
			OnInitCompleteEvent.Broadcast(this);
			break;
		}
		OnChainInitEvent.Broadcast(this,prevInitStep,mCurrentInitStep);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("OnTryChainInitNext - Failed to get world context. Transitioning to EInitError."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
	}
}

bool AVdjmRecordBridgeActor::CheckChainCount(const FString& errorMsg)
{
	if (mChainTryInitCount < 1)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("%s"), *errorMsg);
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return true;
	}
	return false;
}

void AVdjmRecordBridgeActor::ChainInit_InitializeWorldParts()
{
	if (CheckChainCount(TEXT("ChainInit_InitializeWorldParts - Exceeded maximum retry attempts while initializing world parts.")))
	{
		return;
	}
	
	if (UWorld* worldContext = GetWorld())
	{
		mTargetPlayerController = UGameplayStatics::GetPlayerController(worldContext,0);
		if (UGameViewportClient* gameView = worldContext->GetGameViewport())
		{
			mTargetViewport = gameView->GetGameViewport();
		}
		else
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeWorldParts - Failed to get GameViewportClient from world context."));
			OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
			return;
		}
		
		if (mTargetViewport == nullptr || mTargetPlayerController == nullptr)
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeWorldParts - Failed to initialize world parts. PlayerController or Viewport is null."));
			OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
			return;
		}
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_InitializeWorldParts - Successfully initialized world parts."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitializeCurrentEnvironment);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeCurrentEnvironment - Failed to get world context."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
	}
}

void AVdjmRecordBridgeActor::ChainInit_InitializeCurrentEnvironment()
{
	//	환경 검증 및 resolver 초기화 시도
	if (CheckChainCount(TEXT("ChainInit_InitializeCurrentEnvironment - Exceeded maximum retry attempts while initializing current environment.")))
	{
		return;
	}
	if ( mRecordConfigureDataAsset == nullptr)
	{
		mRecordConfigureDataAsset = TryGetRecordEnvConfigure();
		if (mRecordConfigureDataAsset == nullptr)
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeCurrentEnvironment - Failed to load record configure data asset."));
			OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
			return;
		}
		else
		{
			UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_InitializeCurrentEnvironment - Record configure data asset loaded successfully."));
		}
	}
	
	if (mRecordConfigureDataAsset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeCurrentEnvironment - Record configure data asset is null after loading attempt."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return;
	}
	
	if (mEnvResolver == nullptr)
	{
		mEnvResolver = NewObject<UVdjmRecordEnvResolver>(this, UVdjmRecordEnvResolver::StaticClass());
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_InitializeCurrentEnvironment - Environment resolver instance created."));
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("ChainInit_InitializeCurrentEnvironment - Environment resolver instance already exists. Clearing existing resolver state."));
		mEnvResolver->Clear();
	}
	
	if (not mEnvResolver->InitResolverEnvironment(this))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeCurrentEnvironment - Failed to initialize environment resolver. Continuing with initialization, but resolver may not function correctly."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return;
	}
	
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::ECreateRecordResource);
}

void AVdjmRecordBridgeActor::ChainInit_CreateRecordResource()
{
	if (CheckChainCount(TEXT("ChainInit_CreateRecordResource - Exceeded maximum retry attempts while creating record resource.")))
	{
		return;
	}
	//	데이터 에셋에서 현재 플렛폼에 맞는 preset 을 가져온다. 여기서 검증해줄땐 mRecordConfigureDataAsset 의 프리셋을 가져옴.
	const FVdjmRecordEnvPlatformPreset* envPreset = mRecordConfigureDataAsset->GetPlatformPreset(GetTargetPlatform());
	
	if (envPreset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - No platform preset found for target platform. Cannot create record resource without platform preset."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return;
	}
	//	해당 프리셋에 맞는 현재 티어를 가져온다.
	const FVdjmEncoderInitRequest* initPreset = envPreset->GetEncoderInitRequest(mCurrentQualityTier);
	if (initPreset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - No encoder init preset found for current quality tier. Cannot create record resource without encoder init preset."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return;
	}
	
	if (mRecordResource != nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("ChainInit_CreateRecordResource - Record resource already exists. Releasing existing resource before creating new one."));
		mRecordResource->ReleaseResources();
		mRecordResource = nullptr;
	}
	
	//	resolver 통해서 record resource 생성 시도, 심지어 이 단계에서 데이터 에셋의 프리셋이 mResolvedPreset 로 변화함.
	mRecordResource = mEnvResolver->CreateResolvedRecordResource(envPreset);
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - Failed to create record resource from environment resolver."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (not mRecordResource->InitializeResourceExtended(mEnvResolver))
    {
    	UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to initialize record resource with resolver."));
    	mRecordResource->ReleaseResources();
		mRecordResource = nullptr;
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
    }
	/*
	 * 이 시점에서 생성 완료된 것들.
	 * - mRecordConfigureDataAsset : 데이터 에셋에서 프리셋을 가져와서 가지고 있음.
	 * - mEnvResolver : 환경 리졸버 인스턴스가 생성되어 있고, InitResolverEnvironment 까지 호출되어 있음. 내부적으로 mResolvedPreset 이 업데이트 되어있음.
	 * - mRecordResource : 환경 리졸버를 통해서 레코드 리소스가 생성되어 있음. 내부적으로 mRecordResource 가 업데이트 되어있음.
	 */
	
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::EPostResourceInitResolve);
}

void AVdjmRecordBridgeActor::ChainInit_PostResourceInitResolve()
{
	if (CheckChainCount(TEXT("ChainInit_PostResourceInitResolve - Exceeded maximum retry attempts while resolving post resource initialization.")))
	{
		return;
	}
	if (mEnvResolver == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_PostResourceInitResolve - Environment resolver is null after initialization."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_PostResourceInitResolve - Record resource is null after initialization."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (not mEnvResolver->IsValidPreset() ||not mRecordResource->DbcIsInitializedResource())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_PostResourceInitResolve - Environment resolver preset is not valid or record resource is not properly initialized after creation."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (not mRecordResource->IsLazyPostInitializeCheck())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - Record resource failed extended initialization with environment resolver."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::ECreatePipelines);
}

void AVdjmRecordBridgeActor::ChainInit_CreateRecordPipeline()
{
	if (CheckChainCount(TEXT("ChainInit_CreateRecordPipeline - Exceeded maximum retry attempts while creating record pipeline.")))
	{
		return;
	}
	
	if (mEnvResolver == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Environment resolver is null. Cannot create record pipeline without environment resolver."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Record resource is null. Cannot create record pipeline without valid record resource."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	//	여기에서 pipeline 을 바인딩한다.
	TSubclassOf<UVdjmRecordUnitPipeline> pipelineCls = mEnvResolver->TryGetResolvedPipelineClass();
	if (pipelineCls == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Environment resolver did not provide a valid pipeline class. Cannot create record pipeline without valid pipeline class."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (not BindingRecordPipeline(pipelineCls,mRecordResource))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Failed to bind record pipeline with class %s."), *pipelineCls->GetName());
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (not mEnvResolver->InitComplete(this,mRecordResource,mRecordPipeline))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Environment resolver is not valid after pipeline initialization. Transitioning to EInitError."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordPipeline - Record pipeline created and bound successfully."));
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::EFinalizeInitialization);
}

void AVdjmRecordBridgeActor::ChainInit_FinalizeInitialization()
{
	if (DbcValidInitializeComplete())
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("!!{ ChainInit_FinalizeInitialization - Initialization complete and record is startable. Transitioning to EComplete. }!!"));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EComplete);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("ChainInit_FinalizeInitialization - Initialization complete but record is not startable. Check DbcRecordStartableFull for details. Transitioning to EInitError."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
	}
}

bool AVdjmRecordBridgeActor::DbcValidInitializeComplete() const
{
	if (mRecordConfigureDataAsset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mRecordConfigureDataAsset == nullptr"));
		return false;
	}
	
	if (mTargetPlayerController == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mTargetPlayerController == nullptr"));
		return false;
	}
	
	if (mTargetViewport == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mTargetViewport == nullptr"));
		return false;
	}
	
	return IsCompleteChainInit();
}

bool AVdjmRecordBridgeActor::DbcRecordStartableFull() const
{
	bool bOk = true;
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("{{  AVdjmRecordBridgeActor::DbcRecordStartableFull - Starting full record startability check. }}"));
	
	auto Fail = [&bOk](const TCHAR* Reason)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("AVdjmRecordBridgeActor::DbcRecordStartableFull - %s"), Reason);
		bOk = false;
	};

	if (bIsRecording)
	{
		Fail(TEXT("DbcRecordStartableFull - bIsRecording == true"));
	}

	const double Now = FPlatformTime::Seconds();
	if (mRecordEndTime > 0.0 && Now >= mRecordEndTime)
	{
		Fail(TEXT("DbcRecordStartableFull - record end time already passed"));
	}

	if (mRecordConfigureDataAsset == nullptr)
	{
		Fail(TEXT("DbcRecordStartableFull - mRecordConfigureDataAsset == nullptr"));
	}

	if (mEnvResolver == nullptr)
	{
		Fail(TEXT("DbcRecordStartableFull - mEnvResolver == nullptr"));
	}
	else
	{
		if (!mEnvResolver->IsValidPreset())
		{
			Fail(TEXT("DbcRecordStartableFull - mEnvResolver->IsValidPreset == false"));
		}

		const FVdjmEncoderInitRequest* InitPreset = mEnvResolver->TryGetResolvedEncoderInitRequest();
		if (InitPreset == nullptr)
		{
			Fail(TEXT("DbcRecordStartableFull - mEnvResolver->TryGetResolvedEncoderInitRequest == nullptr"));
		}
		else if (!InitPreset->EvaluateValidation())
		{
			Fail(TEXT("DbcRecordStartableFull - ResolvedInitPreset->EvaluateValidation == false"));
		}
		else if (!VdjmRecordUtils::Validations::DbcValidateAudioConfig(InitPreset->AudioConfig, TEXT("DbcRecordStartableFull")))
		{
			Fail(TEXT("DbcRecordStartableFull - AudioConfig validation failed"));
		}

		if (mEnvResolver->TryGetResolvedPipelineClass() == nullptr)
		{
			Fail(TEXT("DbcRecordStartableFull - mEnvResolver->TryGetResolvedPipelineClass == nullptr"));
		}
		const FVdjmRecordEnvPlatformPreset& ResolvedPreset = mEnvResolver->GetResolvedEnvPreset();
		if (ResolvedPreset.RecordResourceClass == nullptr)
		{
			Fail(TEXT("DbcRecordStartableFull - ResolvedPreset.RecordResourceClass == nullptr"));
		}
	}

	if (mRecordResource == nullptr)
	{
		Fail(TEXT("DbcRecordStartableFull - mRecordResource == nullptr"));
	}
	else if (!mRecordResource->DbcIsValidResourceInit())
	{
		Fail(TEXT("DbcRecordStartableFull - mRecordResource->DbcIsValidResourceInit == false"));
	}

	if (mRecordPipeline == nullptr)
	{
		Fail(TEXT("DbcRecordStartableFull - mRecordPipeline == nullptr"));
	}
	else if (!mRecordPipeline->DbcIsValid())
	{
		Fail(TEXT("DbcRecordStartableFull - mRecordPipeline->DbcIsValid == false"));
	}

	if (mTargetViewport == nullptr)
	{
		Fail(TEXT("DbcRecordStartableFull - mTargetViewport == nullptr"));
	}
	if (!mTargetPlayerController.IsValid())
	{
		Fail(TEXT("DbcRecordStartableFull - mTargetPlayerController is invalid"));
	}
	if (!FSlateApplication::IsInitialized())
	{
		Fail(TEXT("DbcRecordStartableFull - SlateApplication is not initialized"));
	}

	return bOk;
}
