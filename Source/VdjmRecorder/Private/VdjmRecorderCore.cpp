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
void UVdjmRecordEnvCurrentInfo::InitializeCurrentEnvironment(AVdjmRecordBridgeActor* ownerBridge)
{
	if (ownerBridge && ownerBridge->DbcValidConfigureDataAsset())
	{
		mLinkedDataAsset = ownerBridge->GetRecordEnvConfigureDataAsset();
		mCurrentGlobalRules = ownerBridge->GetGlobalRules();
		
		if (FVdjmRecordEnvPlatformInfo* perPlatform = ownerBridge->GetCurrentPlatformInfo())
		{
			mCurrentPlatform = ownerBridge->GetTargetPlatform();
			mCurrentGlobalRules = ownerBridge->GetCurrentGlobalRules();
			mCurrentResolution = perPlatform->Resolution;
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
	 * /Script/VdjmRecorder.VdjmRecordEnvDataAsset'/VdjmMobileUi/Record/Bp_VdjmRecordConfigDataAsset.Bp_VdjmRecordConfigDataAsset'
	 */
	return FVdjmFunctionLibraryHelper::TryGetRecordConfigureDataAsset<UVdjmRecordEnvDataAsset>(FSoftObjectPath( TEXT("/Script/VdjmRecorder.VdjmRecordEnvDataAsset'/VdjmMobileUi/Record/Bp_VdjmRecordConfigDataAsset.Bp_VdjmRecordConfigDataAsset'")));
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
			UE_LOG(LogTemp, Warning, TEXT("bIsRecording == true"));
		}
		if (mRecordResource == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("mRecordResource == nullptr"));
		}
		if (mRecordPipeline == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("mRecordPipeline == nullptr"));
		}
		else if (not mRecordPipeline->DbcIsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("mRecordPipeline->DbcIsValid == false"));
		}
		if (not DbcRecordingPossible())
		{
			UE_LOG(LogTemp, Warning, TEXT("1   DbcRecordingPossible == false"));
			if (not DbcValidRecordPipeline())
			{
				UE_LOG(LogTemp, Warning, TEXT("2       DbcValidRecordPipeline() == false"));
				if (not DbcValidRecordResource())
				{
					UE_LOG(LogTemp, Warning, TEXT("3           DbcValidRecordResource() == false"));
					if (mRecordResource == nullptr)
					{
						UE_LOG(LogTemp, Warning, TEXT("4               mRecordResource == nullptr "));
					}
					if (mRecordResource != nullptr && not mRecordResource->DbcIsValidResourceInit())
					{
						UE_LOG(LogTemp, Warning, TEXT("4               mRecordResource->DbcIsValidResource() "));
						if (not mRecordResource->OwnerBridgeActor.IsValid())
						{
							UE_LOG(LogTemp, Warning, TEXT("5			       not mRecordResource->OwnerBridgeActor.IsValid() "));
						}
						if (not mRecordResource->LinkedCurrentInfo.IsValid())
						{
							UE_LOG(LogTemp, Warning, TEXT("5			       not mRecordResource->LinkedCurrentInfo.IsValid() "));
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("5			       mTexturePoolRHI.IsEmpty() "));
						}
					}
				}
				if (mRecordPipeline == nullptr)
				{
					UE_LOG(LogTemp, Warning, TEXT("3           mRecordPipeline == nullptr"));
				}
				if (mRecordPipeline != nullptr && not mRecordPipeline->DbcIsValid())
				{
					UE_LOG(LogTemp, Warning, TEXT("3            mRecordPipeline->DbcIsValid()"));
				}
			}
			if (not DbcValidCurrentEnvInfo())
			{
				UE_LOG(LogTemp, Warning, TEXT("2       DbcValidRecordPipeline() == false"));
			}
			
		}
		
		
		UE_LOG(LogTemp, Warning, TEXT("Cannot start recording: not startable"));
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
	if (mRecordPipeline == nullptr)
	{
		if (FVdjmRecordEnvPlatformInfo* platformInfo = mRecordConfigureDataAsset->GetPlatformInfo(GetTargetPlatform()))
		{
			mRecordPipeline = NewObject<UVdjmRecordUnitPipeline>(
			this,platformInfo->PipelineClass);
			
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
	}
}

void AVdjmRecordBridgeActor::BeginPlay()
{
	if (mRecordConfigureDataAsset == nullptr)
	{
		mRecordConfigureDataAsset = TryGetRecordEnvConfigure();
	}
	if (UWorld* worldContext = GetWorld())
	{
		if (mTargetPlayerController == nullptr)
		{
			mTargetPlayerController = UGameplayStatics::GetPlayerController(worldContext,0);
		}
		if (mTargetViewport == nullptr)
		{
			mTargetViewport = worldContext->GetGameViewport()->GetGameViewport();
		}
		if (FVdjmRecordEnvPlatformInfo* platformInfo = mRecordConfigureDataAsset->GetPlatformInfo(GetTargetPlatform()))
		{
			if (mCurrentEnvInfo == nullptr)
			{
				//	DataAsset 을 넣어서 CurrentEnvInfo 가 만들어지도록 한다.
				mCurrentEnvInfo = NewObject<UVdjmRecordEnvCurrentInfo>(this,UVdjmRecordEnvCurrentInfo::StaticClass());
				mCurrentEnvInfo->InitializeCurrentEnvironment(this);
			}
		
			if (mRecordResource == nullptr)
			{
				UE_LOG(LogVdjmRecorderCore, Log, TEXT("AVdjmRecordBridgeActor::BeginPlay - Initializing record resource."));
				
				//	TODO: 이거도 Wnd 나 그런걸로 나눌 수 있게 만들어야함. 이게 null
				mRecordResource = NewObject<UVdjmRecordResource>(this,platformInfo->RecordResourceClass);
				
				mOnResourceTexturePoolInitializedHandle = mRecordResource->OnResourceTexturePoolInitializedFunc.AddUObject(this,&AVdjmRecordBridgeActor::PostResourceInit);
				
				mRecordResource->InitializeResource(this);
			}
		}
		else
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::BeginPlay - No platform info found for target platform."));
		}
	}
	
}

bool AVdjmRecordBridgeActor::DbcRecordStartableFull() const
{
	bool bOk = true;
	auto Fail = [&bOk](const TCHAR* Reason)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("AVdjmRecordBridgeActor::DbcRecordStartableFull - %s"), Reason);
		bOk = false;
	};

	if (bIsRecording)
	{
		Fail(TEXT("bIsRecording == true"));
	}

	const double Now = FPlatformTime::Seconds();
	if (mRecordEndTime > 0.0 && Now >= mRecordEndTime)
	{
		Fail(TEXT("record end time already passed"));
	}

	if (mRecordConfigureDataAsset == nullptr)
	{
		Fail(TEXT("mRecordConfigureDataAsset == nullptr"));
	}

	const FVdjmRecordEnvPlatformInfo* PlatformInfo = mRecordConfigureDataAsset
		? mRecordConfigureDataAsset->GetPlatformInfo(GetTargetPlatform())
		: nullptr;
	if (PlatformInfo == nullptr)
	{
		Fail(TEXT("PlatformInfo == nullptr for target platform"));
	}
	else
	{
		if (PlatformInfo->PipelineClass == nullptr)
		{
			Fail(TEXT("PlatformInfo->PipelineClass == nullptr"));
		}
		if (PlatformInfo->PipelineUnitClassMap.Num() == 0)
		{
			Fail(TEXT("PlatformInfo->PipelineUnitClassMap.Num() == 0"));
		}
		if (PlatformInfo->Resolution.X <= 0 || PlatformInfo->Resolution.Y <= 0)
		{
			Fail(TEXT("PlatformInfo->Resolution is invalid"));
		}
		if (PlatformInfo->FrameRate <= 0)
		{
			Fail(TEXT("PlatformInfo->FrameRate <= 0"));
		}
		if (PlatformInfo->BitrateMap.Num() == 0)
		{
			Fail(TEXT("PlatformInfo->BitrateMap.Num() == 0"));
		}
	}

	if (mCurrentEnvInfo == nullptr)
	{
		Fail(TEXT("mCurrentEnvInfo == nullptr"));
	}
	else
	{
		if (!mCurrentEnvInfo->DbcIsValidCurrentInfo())
		{
			Fail(TEXT("mCurrentEnvInfo->DbcIsValidCurrentInfo == false"));
		}

		if (mCurrentEnvInfo->GetCurrentResolution().X <= 0 || mCurrentEnvInfo->GetCurrentResolution().Y <= 0)
		{
			Fail(TEXT("CurrentResolution is invalid"));
		}
		if (mCurrentEnvInfo->GetCurrentFrameRate() <= 0)
		{
			Fail(TEXT("CurrentFrameRate <= 0"));
		}
		if (mCurrentEnvInfo->GetCurrentBitrate() <= 0)
		{
			Fail(TEXT("CurrentBitrate <= 0"));
		}
		if (mCurrentEnvInfo->GetCurrentFilePrefix().IsEmpty())
		{
			Fail(TEXT("CurrentFilePrefix is empty"));
		}

		FString CurrentPath = mCurrentEnvInfo->GetCurrentFilePath();
		if (CurrentPath.IsEmpty())
		{
			Fail(TEXT("CurrentFilePath is empty"));
		}
		else
		{
			FPaths::NormalizeDirectoryName(CurrentPath);
			if (FPaths::IsRelative(CurrentPath))
			{
				Fail(TEXT("CurrentFilePath is relative"));
			}
			else if (!IFileManager::Get().DirectoryExists(*CurrentPath))
			{
				Fail(TEXT("CurrentFilePath directory does not exist"));
			}
		}
	}

	if (mRecordResource == nullptr)
	{
		Fail(TEXT("mRecordResource == nullptr"));
	}
	else if (!mRecordResource->DbcIsValidResourceInit())
	{
		Fail(TEXT("mRecordResource->DbcIsValidResourceInit == false"));
	}

	if (mRecordPipeline == nullptr)
	{
		Fail(TEXT("mRecordPipeline == nullptr"));
	}
	else if (!mRecordPipeline->DbcIsValid())
	{
		Fail(TEXT("mRecordPipeline->DbcIsValid == false"));
	}

	if (mTargetViewport == nullptr)
	{
		Fail(TEXT("mTargetViewport == nullptr"));
	}
	if (!mTargetPlayerController.IsValid())
	{
		Fail(TEXT("mTargetPlayerController is invalid"));
	}
	if (!FSlateApplication::IsInitialized())
	{
		Fail(TEXT("SlateApplication is not initialized"));
	}

	return bOk;
}


