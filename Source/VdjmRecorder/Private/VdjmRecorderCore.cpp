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
bool UVdjmRecordEnvCurrentInfo::InitializeCurrentEnvironment(AVdjmRecordBridgeActor* ownerBridge)
{
	if (ownerBridge && ownerBridge->DbcValidConfigureDataAsset())
	{
		mLinkedDataAsset = ownerBridge->GetRecordEnvConfigureDataAsset();
		mCurrentGlobalRules = ownerBridge->GetGlobalRules();
		
		if (FVdjmRecordEnvPlatformInfo* perPlatform = ownerBridge->GetCurrentPlatformInfo())
		{
			mCurrentPlatform = ownerBridge->GetTargetPlatform();
			mCurrentGlobalRules = ownerBridge->GetCurrentGlobalRules();
			if (perPlatform->bUseAutoTargetPlatformResolution)
			{
				FIntPoint outputResolution = FIntPoint::ZeroValue;
				if (ownerBridge->TryResolveViewportSize(outputResolution))
				{
					mCurrentResolution = outputResolution;
				}
				else
				{
					mCurrentResolution = FIntPoint(1920,1080);
					ownerBridge->CriticalErrorStop(TEXT("Failed to resolve viewport size for recording. Recording cannot proceed."));
					return false;
				}
			}
			else
			{
				mCurrentResolution = perPlatform->Resolution;
			}
			
			mCurrentFrameRate = perPlatform->FrameRate;
			mCurrentPixelFormat = perPlatform->PixelFormat;
			mAllBitrateMap = perPlatform->BitrateMap;

			float floatBitrate = 
				mAllBitrateMap.Contains(ownerBridge->SelectedBitrateType)?
					FVdjmFunctionLibraryHelper::ConvertToBitrateValue(mAllBitrateMap[ownerBridge->SelectedBitrateType]) :
					mAllBitrateMap.Contains(EVdjmRecordQualityTiers::EDefault)?
						mAllBitrateMap[EVdjmRecordQualityTiers::EDefault] : 2000000.0f;
			mCurrentBitrate = FVdjmFunctionLibraryHelper::ConvertToBitrateValue(floatBitrate);
			
			//mCurrentBitrate =
			mCurrentFilePrefix = perPlatform->FilePrefix;
			FString defaultFilePath;
			switch (mCurrentPlatform)
			{
			case EVdjmRecordEnvPlatform::EAndroid:
				defaultFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());
				break;
			case EVdjmRecordEnvPlatform::EWindows:
			default:
				defaultFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
				break;
			}
			if (perPlatform->CustomFileSaverClass != nullptr)
			{
				// TODO
			}
			mCurrentFilePath = defaultFilePath;
			return true;
		}
		else
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecorderCore::Initialize - ownerBridge->SelectedBitrateType is invalid."));
		}
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecorderCore::Initialize - ownerBridge is null."));
	}
	return false;
}


bool UVdjmRecordEnvCurrentInfo::DbcIsValidCurrentInfo() const
{
	/*
	 * 원래라면 mCurrentCustomFileSaverInstance 이걸 가지게 해야함.
	* if(mCurrentCustomFileSaverInstance)
	{
	}
	 */
	return mLinkedDataAsset.IsValid() && not mAllBitrateMap.IsEmpty();
}

FString UVdjmRecordEnvCurrentInfo::MakeFinalFilePath(const FString& customFileName)
{
	FString basePath;
	
	switch (mCurrentPlatform)
	{
	case EVdjmRecordEnvPlatform::EWindows:
		basePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		break;
	case EVdjmRecordEnvPlatform::EAndroid:
		basePath =  FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());
		break;
	case EVdjmRecordEnvPlatform::EIOS:
		basePath = FPaths::ProjectSavedDir();
		break;
	case EVdjmRecordEnvPlatform::EMac:
		basePath = FPaths::ProjectSavedDir();
		break;
	case EVdjmRecordEnvPlatform::ELinux:
		basePath = FPaths::ProjectSavedDir();
		break;
	default:
		basePath = FPaths::ProjectSavedDir();
		break;
	}
	if(mCurrentFilePrefix.IsEmpty())
	{
		mCurrentFilePrefix = TEXT("Vcard");
	}
	if (mCurrentFilePrefix.EndsWith(TEXT("_")))
	{
		mCurrentFilePrefix.RemoveFromEnd(TEXT("_"));
	}
	FString finalPath = mCurrentFilePrefix + TEXT("_");
	if(customFileName.IsEmpty())
	{
		finalPath += TEXT("_")+FString::FromInt(FDateTime::Now().ToUnixTimestamp());
	}
	else
	{
		mFileName = customFileName;
		finalPath += customFileName + TEXT("_") + FString::FromInt(FDateTime::Now().ToUnixTimestamp());
	}
	
	return FPaths::Combine(basePath, finalPath + TEXT(".mp4"));
}

UVdjmRecordResource* UVdjmRecordEnvResolver::CreateResolvedRecordResource(AVdjmRecordBridgeActor* ownerBridge,const FVdjmRecordEnvPlatformPreset* presetData) 
{
	if (ownerBridge == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - ownerBridge is null."));
		return nullptr;
	}
	LinkedOwnerBridge = ownerBridge;
	
	if (not ResolveEnvPlatform(presetData))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to resolve environment platform."));
		return nullptr;
	}
	if (UVdjmRecordResource* newResource = NewObject<UVdjmRecordResource>(this,mResolvedPreset.RecordResourceClass))
	{
		if (not newResource->InitializeResourceExtended(this))
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to initialize record resource with resolver."));
			return nullptr;
		}
		return newResource;
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to create record resource instance."));
		return nullptr;
	}
	return nullptr;
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
	return IsValidInitEnvResolver();
}

bool UVdjmRecordEnvResolver::ResolveEnvPlatform(const FVdjmRecordEnvPlatformPreset* presetData)
{
	mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
	if (presetData == nullptr || not presetData->DbcIsValid())
	{
		return false;
	}

	const auto currPlatform = LinkedOwnerBridge.IsValid() ? 
	LinkedOwnerBridge->GetTargetPlatform() : 
#ifdef PLATFORM_WINDOWS
		EVdjmRecordEnvPlatform::EWindows;
#elif PLATFORM_ANDROID|| defined(__RESHARPER__)
		EVdjmRecordEnvPlatform::EAndroid;
#elif PLATFORM_IOS
		EVdjmRecordEnvPlatform::EIOS;
#elif PLATFORM_MAC
		EVdjmRecordEnvPlatform::EMac;
#elif PLATFORM_LINUX
		EVdjmRecordEnvPlatform::ELinux;
#else
		EVdjmRecordEnvPlatform::EDefault;
#endif
	
	
	const EVdjmRecordQualityTiers presetRequestTier =
		(LinkedOwnerBridge.IsValid() && LinkedOwnerBridge->SelectedBitrateType != EVdjmRecordQualityTiers::EUndefined)
			? LinkedOwnerBridge->SelectedBitrateType
			: presetData->DefaultQualityTier;

	constexpr EVdjmRecordQualityTiers TierOrder[] = {
		EVdjmRecordQualityTiers::EUltra,
		EVdjmRecordQualityTiers::EHigh,
		EVdjmRecordQualityTiers::EMediumHigh,
		EVdjmRecordQualityTiers::EMedium,
		EVdjmRecordQualityTiers::EMdeiumLow,
		EVdjmRecordQualityTiers::ELow,
		EVdjmRecordQualityTiers::ELowest
	};

	auto FindTierIndexFunctor = [&](EVdjmRecordQualityTiers Tier)->int32
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(TierOrder); ++i)
		{
			if (TierOrder[i] == Tier)
			{
				return i;
			}
		}
		return INDEX_NONE;
	};

	int32 beginTier = FindTierIndexFunctor(presetRequestTier);
	if (beginTier == INDEX_NONE)
	{
		beginTier = FindTierIndexFunctor(presetData->DefaultQualityTier);
	}
	if (beginTier == INDEX_NONE)
	{
		beginTier = 0;
	}

	FIntPoint resolvedViewPortSize = FIntPoint::ZeroValue;
	const bool bHasViewport = LinkedOwnerBridge.IsValid() && LinkedOwnerBridge->TryResolveViewportSize(resolvedViewPortSize);
	if (!bHasViewport || 
		resolvedViewPortSize.X <= 0 || resolvedViewPortSize.Y <= 0)
	{
		resolvedViewPortSize = GetPresetFeatureResolution(0);
	}
	
	/*
	 * TODO(20260413 refactoring and audio) : resolution, bitrate,frame rate, file path, 즉, FVdjmRecordEnvPlatformPreset 이거를 한번 처리.
	 */
	
	
	
}

bool UVdjmRecordEnvResolver::ResolvedFinalFilePath(const FString& customFileName)
{
	FString basePath;
	if (not LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::ResolvedFinalFilePath - LinkedOwnerBridge is invalid."));
		return false;
	}
	switch (VdjmRecordUtils::GetTargetPlatform())
	{
	case EVdjmRecordEnvPlatform::EWindows:
		basePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		break;
	case EVdjmRecordEnvPlatform::EAndroid:
		basePath =  FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());
		break;
	case EVdjmRecordEnvPlatform::EIOS:
		basePath = FPaths::ProjectSavedDir();
		break;
	case EVdjmRecordEnvPlatform::EMac:
		basePath = FPaths::ProjectSavedDir();
		break;
	case EVdjmRecordEnvPlatform::ELinux:
		basePath = FPaths::ProjectSavedDir();
		break;
	default:
		basePath = FPaths::ProjectSavedDir();
		break;
	}
	if (const FVdjmEncoderInitRequest* initRequest =mResolvedPreset.GetEncoderInitRequest(mResolvedQualityTier))
	{
		
	}
	
	if(mCurrentFilePrefix.IsEmpty())
	{
		mCurrentFilePrefix = TEXT("Vcard");
	}
	if (mCurrentFilePrefix.EndsWith(TEXT("_")))
	{
		mCurrentFilePrefix.RemoveFromEnd(TEXT("_"));
	}
	FString finalPath = mCurrentFilePrefix + TEXT("_");
	if(customFileName.IsEmpty())
	{
		finalPath += TEXT("_")+FString::FromInt(FDateTime::Now().ToUnixTimestamp());
	}
	else
	{
		mFileName = customFileName;
		finalPath += customFileName + TEXT("_") + FString::FromInt(FDateTime::Now().ToUnixTimestamp());
	}
	
	return FPaths::Combine(basePath, finalPath + TEXT(".mp4"));
}

FIntPoint UVdjmRecordEnvResolver::GetPresetFeatureResolution(uint32 tier) const
{
	switch (VdjmRecordUtils::GetTargetPlatform())
	{
	case EVdjmRecordEnvPlatform::EWindows:
		return GetPresetFeatureResolution_Window(tier);
		break;
	case EVdjmRecordEnvPlatform::EAndroid:
		return GetPresetFeatureResolution_Android(tier);
		break;
	case EVdjmRecordEnvPlatform::EIOS:
		return GetPresetFeatureResolution_Ios(tier);
		break;
	case EVdjmRecordEnvPlatform::EMac:
		return GetPresetFeatureResolution_Mac(tier);
		break;
	case EVdjmRecordEnvPlatform::ELinux:
		return GetPresetFeatureResolution_Linux(tier);
		break;
	case EVdjmRecordEnvPlatform::EDefault:
		return GetPresetFeatureResolution_Window(tier);
		break;
	default: return FIntPoint();
	}
}

FIntPoint UVdjmRecordEnvResolver::GetPresetFeatureResolution_Window(uint32 tier) const
{
	const TArray<FIntPoint> resultResolution =
	{
		FIntPoint(1280, 720),	// HD (720p), Steam Deck (1280x800 for 16:10)
		FIntPoint(1920, 1080),	// FHD (1080p), Standard PC Monitor
		FIntPoint(1920, 1200),	// WUXGA (1200p), 16:10 Standard Monitor
		FIntPoint(2560, 1440),	// QHD (1440p), 2K Gaming Monitor
		FIntPoint(3440, 1440),	// UWQHD, 21:9 Ultrawide Monitor
		FIntPoint(3840, 2160),	// UHD (4K), High-End PC Monitor
		FIntPoint(7680, 4320),	// 8K, Enthusiast Monitor / TV
	};
	uint32 maxTierNum = resultResolution.Num();
	if (tier < maxTierNum)
	{
		return resultResolution[tier];
	}
	else
	{
		return resultResolution[tier % maxTierNum];
	}
}

FIntPoint UVdjmRecordEnvResolver::GetPresetFeatureResolution_Android(uint32 tier) const
{
	const TArray<FIntPoint> resultResolution =
	{
		FIntPoint(720, 1600),	// Budget Tier (HD+), Samsung Galaxy A12 / Older phones
		FIntPoint(1080, 2340),	// Standard Flagship, Samsung Galaxy S22/S23 (SM-S901/S911)
		FIntPoint(1080, 2400),	// Standard Flagship 2, Google Pixel 7/8
		FIntPoint(1440, 3120),	// Premium Flagship, Samsung Galaxy S24 Ultra / Pixel 8 Pro
		FIntPoint(1812, 2176),	// Foldable (Inner Screen), Samsung Galaxy Z Fold 5
		FIntPoint(2560, 1600),	// Tablet (Landscape default), Samsung Galaxy Tab S8/S9
	};
	uint32 maxTierNum = resultResolution.Num();
	if (tier < maxTierNum)
	{
		return resultResolution[tier];
	}
	else
	{
		return resultResolution[tier % maxTierNum];
	}
}

FIntPoint UVdjmRecordEnvResolver::GetPresetFeatureResolution_Ios(uint32 tier) const
{
	const TArray<FIntPoint> resultResolution =
	{
		FIntPoint(750, 1334),	// iPhone SE (3rd Gen) / Older iPhones (8, 7)
		FIntPoint(1170, 2532),	// iPhone 12 / 13 / 14 (Standard size)
		FIntPoint(1179, 2556),	// iPhone 14 Pro / 15 Pro / 16 Pro
		FIntPoint(1284, 2778),	// iPhone 12/13/14 Pro Max & Plus
		FIntPoint(1290, 2796),	// iPhone 14/15/16 Pro Max
		FIntPoint(1668, 2388),	// iPad Pro 11-inch
		FIntPoint(2048, 2732),	// iPad Pro 12.9-inch / 13-inch
	};
	uint32 maxTierNum = resultResolution.Num();
	if (tier < maxTierNum)
	{
		return resultResolution[tier];
	}
	else
	{
		return resultResolution[tier % maxTierNum];
	}
}

FIntPoint UVdjmRecordEnvResolver::GetPresetFeatureResolution_Mac(uint32 tier) const
{
	const TArray<FIntPoint> resultResolution =
	{
		FIntPoint(2560, 1600),	// MacBook Air 13" (M1) / Older MacBook Pro
		FIntPoint(2560, 1664),	// MacBook Air 13" (M2/M3) - Liquid Retina (Notch included)
		FIntPoint(3024, 1964),	// MacBook Pro 14" (M1/M2/M3)
		FIntPoint(3456, 2234),	// MacBook Pro 16" (M1/M2/M3)
		FIntPoint(5120, 2880),	// Apple Studio Display / 27" iMac (5K Retina)
		FIntPoint(6016, 3384),	// Pro Display XDR (6K)
	};
	uint32 maxTierNum = resultResolution.Num();
	if (tier < maxTierNum)
	{
		return resultResolution[tier];
	}
	else
	{
		return resultResolution[tier % maxTierNum];
	}
}

FIntPoint UVdjmRecordEnvResolver::GetPresetFeatureResolution_Linux(uint32 tier) const
{
	return GetPresetFeatureResolution_Window(tier);
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

void UVdjmRecordResource::InitializeResource(AVdjmRecordBridgeActor* ownerBridge)
{
	if (ownerBridge == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::Initialize - ownerBridge is null."));
		return;
	}
	if (not ownerBridge->DbcValidConfigureDataAsset() )
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::Initialize - ownerBridge's CurrentEnvInfo is null."));
		return;
	}
	
	LinkedOwnerBridge = ownerBridge;
	
	LinkedCurrentInfo_deprecate = ownerBridge->GetCurrentEnvInfo();
	
	CachedGroupCount = LinkedCurrentInfo_deprecate->GetRecordCachedGroupCount();
	TextureResolution = LinkedCurrentInfo_deprecate->GetCurrentResolution();	//	LinkedRecordDesc의 규칙에 맞는 FinalResolution
	OriginResolution = TextureResolution;	
	FinalFrameRate = LinkedCurrentInfo_deprecate->GetCurrentFrameRate();
	FinalBitrate = LinkedCurrentInfo_deprecate->GetCurrentBitrate();
	FinalFilePath = LinkedCurrentInfo_deprecate->MakeFinalFilePath(ownerBridge->GetCurrentFileName());
	FinalPixelFormat = LinkedCurrentInfo_deprecate->GetCurrentPixelFormat();
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

bool VdjmRecorderValidation::DbcValidateResolution(const FIntPoint& InResolution, FIntPoint& OutSafeResolution,
	const TCHAR* DebugOwner)
{
	OutSafeResolution = FIntPoint::ZeroValue;

	if (InResolution.X <= 0 || InResolution.Y <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Invalid resolution. X=%d Y=%d"),
		       DebugOwner,
		       InResolution.X,
		       InResolution.Y);
		return false;
	}

	// 영상 인코더/서피스 계열에서 짝수 해상도를 요구하는 경우가 많으므로 방어적으로 보정
	const int32 SafeX = FMath::Max(2, InResolution.X & ~1);
	const int32 SafeY = FMath::Max(2, InResolution.Y & ~1);

	if (SafeX <= 0 || SafeY <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Safe resolution collapsed to invalid value. X=%d Y=%d"),
		       DebugOwner,
		       SafeX,
		       SafeY);
		return false;
	}

	OutSafeResolution = FIntPoint(SafeX, SafeY);
	return true;
}

bool VdjmRecorderValidation::DbcValidateBitrate(const int32 InBitrate, int32& OutSafeBitrate, const TCHAR* DebugOwner)
{
	OutSafeBitrate = 0;

	if (InBitrate <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Invalid bitrate. Bitrate=%d"),
		       DebugOwner,
		       InBitrate);
		return false;
	}

	// 지나치게 작은 값/비정상 큰 값 방어
	constexpr int32 MinBitrate = 100000;      // 100 Kbps
	constexpr int32 MaxBitrate = 100000000;   // 100 Mbps

	if (InBitrate < MinBitrate || InBitrate > MaxBitrate)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Bitrate out of safe range. Bitrate=%d Range=[%d,%d]"),
		       DebugOwner,
		       InBitrate,
		       MinBitrate,
		       MaxBitrate);
		return false;
	}

	OutSafeBitrate = InBitrate;
	return true;
}

bool VdjmRecorderValidation::DbcValidateOutputFilePath(const FString& InFilePath, FString& OutSafeFilePath,
	const TCHAR* DebugOwner)
{
	OutSafeFilePath.Reset();

	FString SafePath = InFilePath;
	SafePath.TrimStartAndEndInline();

	if (SafePath.IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file path is empty."),
		       DebugOwner);
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
		       DebugOwner,
		       *SafePath);
		return false;
	}

	const FString DirectoryPath = FPaths::GetPath(SafePath);
	if (DirectoryPath.IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file path has no parent directory. Path=%s"),
		       DebugOwner,
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
			       DebugOwner,
			       *DirectoryPath);
			return false;
		}
	}

	const FString CleanFilename = FPaths::GetCleanFilename(SafePath);
	if (CleanFilename.IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file name is empty. Path=%s"),
		       DebugOwner,
		       *SafePath);
		return false;
	}

	const FString Extension = FPaths::GetExtension(SafePath, true).ToLower();
	if (Extension != TEXT(".mp4"))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Invalid output extension. Expected=.mp4 Path=%s"),
		       DebugOwner,
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
			       DebugOwner,
			       Ch,
			       *CleanFilename);
			return false;
		}
	}

	OutSafeFilePath = SafePath;
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
		if (not DbcValidCurrentEnvInfo_deprecated())
		{
			UE_LOG(LogTemp, Warning, TEXT("StartRecording - 2       DbcValidRecordPipeline() == false"));
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
		bIsRecording = true;

		if (FSlateApplication::IsInitialized())
		{
			double Now = FPlatformTime::Seconds();
			mEnvResolver->TryGetResolvedVideoConfig();
			double currentFPS = mCurrentEnvInfo_deprecated->GetCurrentFrameRate();
			
			mFrameInterval = 1.0 / FMath::Max(currentFPS, mCurrentEnvInfo_deprecated->GetCurrentGlobalRules().MinFrameRate);
			mRecordEndTime = Now + mCurrentEnvInfo_deprecated->GetMaxDurationSecond();
            mNextFrameTime = Now; // 첫 프레임은 즉시 기록
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
	
	recordUnitContext.DbcSetupContext(
		GetWorld()
		,this
		,mCurrentEnvInfo_deprecated
		,mRecordResource
		,&RDGBuilder
		,Now);
	
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
	return VdjmRecordUtils::GetTargetPlatform();
}

bool AVdjmRecordBridgeActor::EvaluateInitRequest(const FVdjmEncoderInitRequest* initPreset)
{
	if (initPreset == nullptr)
	{
		return false;
	}
	return initPreset->EvaluateValidation();
}

bool AVdjmRecordBridgeActor::TryResolveViewportSize(FIntPoint& OutSize) const
{
	OutSize = FIntPoint::ZeroValue;
	if (mTargetViewport)
	{
		const FIntPoint Size = mTargetViewport->GetSizeXY();
		if (Size.X > 0 && Size.Y > 0)
		{
			OutSize = Size;
			return true;
		}
	}
	if (const UWorld* World = GetWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			if (FViewport* Viewport = ViewportClient->Viewport)
			{
				const FIntPoint Size = Viewport->GetSizeXY();
				if (Size.X > 0 && Size.Y > 0)
				{
					OutSize = Size;
					return true;
				}
			}
		}
	}
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
		if (Size.X > 0 && Size.Y > 0)
		{
			OutSize = Size;
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
			break;
		case EVdjmRecordBridgeInitStep::EInitError:
			--mChainTryInitCount;
			OnTryChainInitNext(mRetryStep);
			break;
		case EVdjmRecordBridgeInitStep::EInitializeStart:
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
	//	TODO(20260410 env control)
	const FVdjmRecordEnvPlatformPreset* envPreset = mRecordConfigureDataAsset->GetPlatformPreset(GetTargetPlatform());
	
	if (envPreset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeCurrentEnvironment - No platform preset found for target platform. Continuing with initialization, but default values may be used."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return;
	}
	const FVdjmEncoderInitRequest* initPreset = envPreset->GetEncoderInitRequest(mCurrentQualityTier);
	if (initPreset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeCurrentEnvironment - No encoder init preset found for current quality tier. Continuing with initialization, but default values may be used."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return;
	}
	if (not EvaluateInitRequest(initPreset))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeCurrentEnvironment - Failed to evaluate encoder init preset. Continuing with initialization, but default values may be used."));
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
	
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::ECreateRecordResource);
}

void AVdjmRecordBridgeActor::ChainInit_CreateRecordResource()
{
	if (CheckChainCount(TEXT("ChainInit_CreateRecordResource - Exceeded maximum retry attempts while creating record resource.")))
	{
		return;
	}
	//	TODO(20260410 env control)
	const FVdjmRecordEnvPlatformPreset* envPreset = mRecordConfigureDataAsset->GetPlatformPreset(GetTargetPlatform());
	
	if (envPreset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - No platform preset found for target platform. Cannot create record resource without platform preset."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return;
	}
	const FVdjmEncoderInitRequest* initPreset = envPreset->GetEncoderInitRequest(mCurrentQualityTier);
	if (initPreset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - No encoder init preset found for current quality tier. Cannot create record resource without encoder init preset."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return;
	}
	
	//	resolver 통해서 record resource 생성 시도
	mRecordResource = mEnvResolver->CreateResolvedRecordResource(this,envPreset);
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - Failed to create record resource from environment resolver."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	//	여기에서 resolved 된 값들로 record resource 초기화 시도
	if (not mRecordResource->InitializeResourceExtended(mEnvResolver))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - Record resource failed extended initialization with environment resolver."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (not mRecordResource->IsLazyPostInitializeCheck())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - Record resource failed extended initialization with environment resolver."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::EPostResourceInitResolve);
}

void AVdjmRecordBridgeActor::ChainInit_PostResourceInitResolve()
{
	if (CheckChainCount(TEXT("ChainInit_PostResourceInitResolve - Exceeded maximum retry attempts while resolving post resource initialization.")))
	{
		return;
	}
	//	TODO(20260410 env control)
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_PostResourceInitResolve - Record resource is null after initialization."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	UVdjmRecordResource& recordResource = *mRecordResource;
	/*
	 * 이거 단계는 필요한가? 이미 resolver 가 존재하고 그놈이 resource 를 생성하기에..
	 */
	
	
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
	if (DbcRecordStartableFull())
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
	if (mCurrentEnvInfo_deprecated == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mCurrentEnvInfo == nullptr"));
		return false;
	}
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mRecordResource == nullptr"));
		return false;
	}
	return true;
}

bool AVdjmRecordBridgeActor::DbcRecordStartableFull() const
{
	/*
	 * TODO(20260413 refactoring and audio) : 각각 UVdjmRecordEnvDataAsset 랑 
	 */
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
