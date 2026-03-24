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
	LinkedBridgeActor = recordResource->OwnerBridgeActor;
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
					mAllBitrateMap.Contains(EVdjmRecordBitrateType::EDefault)?
						mAllBitrateMap[EVdjmRecordBitrateType::EDefault] : 2000000.0f;
			mCurrentBitrate = FVdjmFunctionLibraryHelper::ConvertToBitrateValue(floatBitrate);
			
			//mCurrentBitrate =
			mCurrentFilePrefix = perPlatform->FilePrefix;
			FString defualtFilePath = FPaths::ProjectSavedDir();
			if (perPlatform->CustomFileSaverClass != nullptr)
			{
				// TODO
			}
			mCurrentFilePath = defualtFilePath;
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
		basePath = FPaths::ProjectSavedDir();
		break;
	case EVdjmRecordEnvPlatform::EAndroid:
		basePath = FPaths::ProjectSavedDir();
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
	
	OwnerBridgeActor = ownerBridge;
	
	LinkedCurrentInfo = ownerBridge->GetCurrentEnvInfo();
	
	CachedGroupCount = LinkedCurrentInfo->GetRecordCachedGroupCount();
	TextureResolution = LinkedCurrentInfo->GetCurrentResolution();	//	LinkedRecordDesc의 규칙에 맞는 FinalResolution
	OriginResolution = TextureResolution;	
	FinalFrameRate = LinkedCurrentInfo->GetCurrentFrameRate();
	FinalBitrate = LinkedCurrentInfo->GetCurrentBitrate();
	FinalFilePath = LinkedCurrentInfo->MakeFinalFilePath(ownerBridge->GetCurrentFileName());
	FinalPixelFormat = LinkedCurrentInfo->GetCurrentPixelFormat();
}

void UVdjmRecordResource::ResetResource()
{
}

void UVdjmRecordResource::ReleaseResources()
{
	OwnerBridgeActor = nullptr;
	LinkedCurrentInfo = nullptr;
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordResource::ReleaseResources - Resources released."));
}

FTextureRHIRef UVdjmRecordResource::CreateTextureForNV12(FIntPoint resolution, EPixelFormat pixelformat,
	ETextureCreateFlags createFlags)
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


// UVdjmRecordDepreDataAsset* AVdjmRecordBridgeActor::TryGetRecordConfigure()
//{
// 	UVdjmRecordDepreDataAsset* result = nullptr;
//
// 	FSoftObjectPath configObjPath = FSoftObjectPath(TEXT("/Script/VdjmMobileUi.VdjmRecordConfigure'/VdjmMobileUi/Recorder/Bp_VdjmRecordConfigDataAsset.Bp_VdjmRecordConfigDataAsset'"));
// 	
// 	if (configObjPath.IsAsset() && configObjPath.IsValid())
// 	{
// 		UObject* configResolved = configObjPath.ResolveObject();
// 		if (configResolved == nullptr)
// 		{
// 			configResolved = configObjPath.TryLoad();
// 			if (configResolved == nullptr)
// 			{
// 				UE_LOG(LogVdjmRecorderCore,Warning,TEXT("Failed to load default VcardConfigDataAsset %s synchronously."),*configObjPath.ToString());
// 			}
// 		}
// 		result = Cast<UVdjmRecordDepreDataAsset>(configResolved);
// 	}
// 	return result;
// }

UVdjmRecordEnvDataAsset* AVdjmRecordBridgeActor::TryGetRecordEnvConfigure()
{
	/*
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

void AVdjmRecordBridgeActor::StartRecording()
{
	if(DbcRecordStartable())
	{
		bIsRecording = true;

		if (FSlateApplication::IsInitialized())
		{
			double Now = FPlatformTime::Seconds();
			
			double currentFPS = mCurrentEnvInfo->GetCurrentFrameRate();
			
			mFrameInterval = 1.0 / FMath::Max(currentFPS, mCurrentEnvInfo->GetCurrentGlobalRules().MinFrameRate);
			mRecordEndTime = Now + mCurrentEnvInfo->GetMaxDurationSecond();
            mNextFrameTime = Now; // 첫 프레임은 즉시 기록
            bIsRecording = true;
			
			if (OnRecordPrevStart.IsBound())
			{
				OnRecordPrevStart.Broadcast(mRecordResource);
			}
			OnRecordPrevStartInner.Broadcast(mRecordResource);
			
			if (mBackBufferDelegateHandle.IsValid())
			{
				FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(mBackBufferDelegateHandle);
			}
			else
			{
				mBackBufferDelegateHandle = FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddUObject(
				this,
				&AVdjmRecordBridgeActor::OnBackBufferReady_RenderThread);
			}
			
			OnRecordStarted.Broadcast(mRecordResource);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot start recording: Slate application not initialized"));
			return;
		}
	}
	else
	{
		if (mRecordConfigureDataAsset == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("mRecordConfigureDataAsset == nullptr"));
		}
		if (mCurrentEnvInfo == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("mCurrentEnvInfo == nullptr"));
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
						if (not mRecordResource->OwnerBridgeActor.IsValid())
						{
							UE_LOG(LogTemp, Warning, TEXT("StartRecording - 5			       not mRecordResource->OwnerBridgeActor.IsValid() "));
						}
						if (not mRecordResource->LinkedCurrentInfo.IsValid())
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
			if (not DbcValidCurrentEnvInfo())
			{
				UE_LOG(LogTemp, Warning, TEXT("StartRecording - 2       DbcValidRecordPipeline() == false"));
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("StartRecording - Cannot start recording: not startable"));
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
void AVdjmRecordBridgeActor::StopRecordingInternal()
{
	if (FSlateApplication::IsInitialized() && mBackBufferDelegateHandle.IsValid())
	{
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(mBackBufferDelegateHandle);

		mBackBufferDelegateHandle.Reset();
	}

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
	
	mNextFrameTime = Now + mFrameInterval;
	float deltaTime = mNextFrameTime - Now;
	FVdjmRecordUnitParamContext recordUnitContext = {};
	FRDGBuilder RDGBuilder(FRHICommandListExecutor::GetImmediateCommandList());
	
	recordUnitContext.DbcSetupContext(
		GetWorld()
		,this
		,mCurrentEnvInfo
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
			weakThis->OnRecordTick.Broadcast(weakThis->GetRecordResource(),deltaTime);
		}
	});
	
	RDGBuilder.Execute(); 
	UE_LOG(LogTemp, Verbose, TEXT("OnBackBufferReady_RenderThread - Finished Recording Frame"));
	//newPayload.LogString.Appendf(TEXT(" }} OnBackBufferReady_RenderThread - Finished Recording Frame \n"));
}

EVdjmRecordEnvPlatform AVdjmRecordBridgeActor::GetTargetPlatform()
{
#if PLATFORM_WINDOWS
	return EVdjmRecordEnvPlatform::EWindows;
#elif PLATFORM_ANDROID
	return EVdjmRecordEnvPlatform::EAndroid;
#elif PLATFORM_IOS
	return EVdjmRecordEnvPlatform::EIos;
#else
	return EVdjmRecordEnvPlatform::EUnknown;
#endif
}

void AVdjmRecordBridgeActor::PostResourceInit(UVdjmRecordResource* resource)
{
	if (mRecordConfigureDataAsset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::PostResourceInit - Record configure data asset is null. Cannot initialize record pipeline."));
		return;
	}
	
	if (mRecordPipeline == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("AVdjmRecordBridgeActor::PostResourceInit - Initializing record pipeline after resource initialization."));
		
		if (FVdjmRecordEnvPlatformInfo* platformInfo = mRecordConfigureDataAsset->GetPlatformInfo(GetTargetPlatform()))
		{
			mRecordPipeline = NewObject<UVdjmRecordUnitPipeline>(
			this,platformInfo->PipelineClass);
			if (mRecordPipeline == nullptr)
			{
				UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::PostResourceInit - Failed to create record pipeline instance."));
				return;
			}
			else
			{
				UE_LOG(LogVdjmRecorderCore, Log, TEXT("AVdjmRecordBridgeActor::PostResourceInit - Record pipeline instance created successfully."));
			}
			
			mRecordPipeline->InitializeRecordPipeline(mRecordResource);
			mCurrentEnvInfo->SetRecordUnitPipeline(mRecordPipeline);
			
			UE_LOG(LogVdjmRecorderCore, Log, TEXT("AVdjmRecordBridgeActor::PostResourceInit - Record pipeline initialized and resource setup complete."));
			
			if (DbcRecordStartableFull())
			{	
				UE_LOG(LogVdjmRecorderCore, Log, TEXT("AVdjmRecordBridgeActor::PostResourceInit - Starting recording immediately after resource initialization."));
			}
			else
			{
				UE_LOG(LogVdjmRecorderCore, Warning, TEXT("AVdjmRecordBridgeActor::PostResourceInit - Record is not startable immediately after resource initialization. Check DbcRecordStartableFull for details."));
			}
			
		}
		else
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::PostResourceInit - No platform info found for target platform. Cannot initialize record pipeline."));
		}
	}
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

void AVdjmRecordBridgeActor::BeginPlay()
{
	Super::BeginPlay();
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitializeWorldParts);
}

void AVdjmRecordBridgeActor::OnTryChainInitNext(EVdjmRecordBridgeInitStep nextStep)
{
	EVdjmRecordBridgeInitStep prevInitStep = mCurrentInitStep;
	mCurrentInitStep = nextStep;
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("OnTryChainInitNext - Transitioning from step %d to step %d"), prevInitStep, mCurrentInitStep);
	
	if (mChainInitTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(mChainInitTimerHandle);
	}
	
	if (mChainTryInitCount < 1)
	{
		mCurrentInitStep = EVdjmRecordBridgeInitStep::EInitErrorEnd;
		return;
	}
	else
	{
		mRetryStep = prevInitStep;
	}
	
	UWorld* worldContext = GetWorld();
	switch (mCurrentInitStep){
	case EVdjmRecordBridgeInitStep::EInitErrorEnd:
		
		break;
	case EVdjmRecordBridgeInitStep::EInitError:
		--mChainTryInitCount;
		OnTryChainInitNext(mRetryStep);
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
		bValidateInitializeComplete = true;
		mChainInitTimerHandle.Invalidate();
		break;
	}
	OnChainInitEvent.Broadcast(this,prevInitStep,mCurrentInitStep);
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
		mTargetViewport = worldContext->GetGameViewport()->GetGameViewport();
		
		if (mTargetViewport == nullptr || mTargetPlayerController == nullptr)
		{
			OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitializeWorldParts);
			return;
		}
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
	
	if (FVdjmRecordEnvPlatformInfo* platformInfo = mRecordConfigureDataAsset->GetPlatformInfo(GetTargetPlatform()))
	{
		if (mCurrentEnvInfo == nullptr)
		{
			//	DataAsset 을 넣어서 CurrentEnvInfo 가 만들어지도록 한다.
			mCurrentEnvInfo = NewObject<UVdjmRecordEnvCurrentInfo>(this,UVdjmRecordEnvCurrentInfo::StaticClass());
		}
		
		if (mCurrentEnvInfo->InitializeCurrentEnvironment(this))
		{
			//	이때는 무조건 mCurrentEnvInfo 가 존재함.
			OnTryChainInitNext(EVdjmRecordBridgeInitStep::ECreateRecordResource);
		}
		else
		{
			OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitializeCurrentEnvironment);
		}
	}
	else
	{
		if (not mRecordConfigureDataAsset->DbcIsValid())
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeCurrentEnvironment - Record configure data asset is not valid."));
		}
		if (platformInfo == nullptr)
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_InitializeCurrentEnvironment - No platform info found for target platform."));
		}
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
	}
}

void AVdjmRecordBridgeActor::ChainInit_CreateRecordResource()
{
	if (CheckChainCount(TEXT("ChainInit_CreateRecordResource - Exceeded maximum retry attempts while creating record resource.")))
	{
		return;
	}
	
	FVdjmRecordEnvPlatformInfo* platformInfo = mRecordConfigureDataAsset->GetPlatformInfo(GetTargetPlatform());
		
	if (platformInfo == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - No platform info found for target platform."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (mRecordResource == nullptr)
	{
		mRecordResource = NewObject<UVdjmRecordResource>(this,platformInfo->RecordResourceClass);
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordResource - Record resource instance created."));
	}
	
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - Failed to create record resource instance."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	mRecordResource->InitializeResource(this);
	if (not mRecordResource->IsLazyPostInitializeCheck())
	{
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EPostResourceInitResolve);
	}
}

void AVdjmRecordBridgeActor::ChainInit_PostResourceInitResolve()
{
	if (CheckChainCount(TEXT("ChainInit_PostResourceInitResolve - Exceeded maximum retry attempts while resolving post resource initialization.")))
	{
		return;
	}
	
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_PostResourceInitResolve - Record resource is null after initialization."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	UVdjmRecordResource& recordResource = *mRecordResource;
	
	FIntPoint resolvResolution = FIntPoint::ZeroValue;
	if (VdjmRecorderValidation::DbcValidateResolution(recordResource.OriginResolution,resolvResolution,TEXT("ChainInit_PostResourceInitResolve - Invalid resolution from record resource after initialization.")))
	{
		recordResource.TextureResolution = resolvResolution;
	}
	else
	{
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	int32 resolvFrameRate = 0;
	if (VdjmRecorderValidation::DbcValidateBitrate( recordResource.FinalBitrate,resolvFrameRate,TEXT("ChainInit_PostResourceInitResolve - Invalid bitrate from record resource after initialization.")))
	{
		recordResource.FinalBitrate = resolvFrameRate;
	}
	else
	{
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	FString resolvFilePath;
	if (VdjmRecorderValidation::DbcValidateOutputFilePath(recordResource.FinalFilePath,resolvFilePath,TEXT("ChainInit_PostResourceInitResolve - Invalid file path from record resource after initialization.")))
	{
		recordResource.FinalFilePath = mRecordResource->FinalFilePath;
	}
	else
	{
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
}

void AVdjmRecordBridgeActor::ChainInit_CreateRecordPipeline()
{
	if (CheckChainCount(TEXT("ChainInit_CreateRecordPipeline - Exceeded maximum retry attempts while creating record pipeline.")))
	{
		return;
	}
	if (mCurrentEnvInfo == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Current environment info is null. Cannot create record pipeline."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	FVdjmRecordEnvPlatformInfo* platformInfo = mRecordConfigureDataAsset->GetPlatformInfo(GetTargetPlatform());
	if (platformInfo == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - No platform info found for target platform."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (mRecordPipeline == nullptr)
	{
		mRecordPipeline = NewObject<UVdjmRecordUnitPipeline>(this,platformInfo->PipelineClass);
	}
	
	if (mRecordPipeline == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Failed to create record pipeline instance."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	mRecordPipeline->InitializeRecordPipeline(mRecordResource);
	mCurrentEnvInfo->SetRecordUnitPipeline(mRecordPipeline);
	
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::EFinalizeInitialization);
	
}

void AVdjmRecordBridgeActor::ChainInit_FinalizeInitialization()
{
	if (DbcRecordStartableFull())
	{
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EComplete);
	}
	else
	{
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
	if (mCurrentEnvInfo == nullptr)
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

	const FVdjmRecordEnvPlatformInfo* PlatformInfo = mRecordConfigureDataAsset
		? mRecordConfigureDataAsset->GetPlatformInfo(GetTargetPlatform())
		: nullptr;
	if (PlatformInfo == nullptr)
	{
		Fail(TEXT("DbcRecordStartableFull - PlatformInfo == nullptr for target platform"));
	}
	else
	{
		if (PlatformInfo->PipelineClass == nullptr)
		{
			Fail(TEXT("DbcRecordStartableFull - PlatformInfo->PipelineClass == nullptr"));
		}
		if (PlatformInfo->PipelineUnitClassMap.Num() == 0)
		{
			Fail(TEXT("DbcRecordStartableFull - PlatformInfo->PipelineUnitClassMap.Num() == 0"));
		}
		if (PlatformInfo->Resolution.X <= 0 || PlatformInfo->Resolution.Y <= 0)
		{
			Fail(TEXT("DbcRecordStartableFull - PlatformInfo->Resolution is invalid"));
		}
		if (PlatformInfo->FrameRate <= 0)
		{
			Fail(TEXT("DbcRecordStartableFull - PlatformInfo->FrameRate <= 0"));
		}
		if (PlatformInfo->BitrateMap.Num() == 0)
		{
			Fail(TEXT("DbcRecordStartableFull - PlatformInfo->BitrateMap.Num() == 0"));
		}
	}

	if (mCurrentEnvInfo == nullptr)
	{
		Fail(TEXT("DbcRecordStartableFull - mCurrentEnvInfo == nullptr"));
	}
	else
	{
		if (!mCurrentEnvInfo->DbcIsValidCurrentInfo())
		{
			Fail(TEXT("DbcRecordStartableFull - mCurrentEnvInfo->DbcIsValidCurrentInfo == false"));
		}

		if (mCurrentEnvInfo->GetCurrentResolution().X <= 0 || mCurrentEnvInfo->GetCurrentResolution().Y <= 0)
		{
			Fail(TEXT("DbcRecordStartableFull - CurrentResolution is invalid"));
		}
		if (mCurrentEnvInfo->GetCurrentFrameRate() <= 0)
		{
			Fail(TEXT("DbcRecordStartableFull - CurrentFrameRate <= 0"));
		}
		if (mCurrentEnvInfo->GetCurrentBitrate() <= 0)
		{
			Fail(TEXT("DbcRecordStartableFull - CurrentBitrate <= 0"));
		}
		if (mCurrentEnvInfo->GetCurrentFilePrefix().IsEmpty())
		{
			Fail(TEXT("DbcRecordStartableFull - CurrentFilePrefix is empty"));
		}

		FString CurrentPath = mCurrentEnvInfo->GetCurrentFilePath();
		if (CurrentPath.IsEmpty())
		{
			Fail(TEXT("DbcRecordStartableFull - CurrentFilePath is empty"));
		}
		else
		{
			FPaths::NormalizeDirectoryName(CurrentPath);
			if (FPaths::IsRelative(CurrentPath))
			{
				Fail(TEXT("DbcRecordStartableFull - CurrentFilePath is relative"));
			}
			else if (!IFileManager::Get().DirectoryExists(*CurrentPath))
			{
				Fail(TEXT("DbcRecordStartableFull - CurrentFilePath directory does not exist"));
			}
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


