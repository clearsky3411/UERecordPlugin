// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "RHIGPUReadback.h"
#include "ShaderParameterStruct.h"
#include "Engine/TextureRenderTarget2D.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecorderCore.generated.h"

//	Dbc 란? Design by Contract 의 약자. 무조건 보증된다는 뜻.
/*§
 ↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓					Forward Declares						↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

class UVdjmRecordEnvResolver;
class UVdjmRecordUnitPipeline;

class UVdjmRecordPlatform;
class UVdjmRecordFileSaver;
class UVdjmRecorderController;

class UVdjmRecordUnit;
class UVdjmRecordResource;
class UVdjmRecordMediaManifest;
class UVdjmRecordMetadataStore;
class UVdjmRecordArtifact;
class UVdjmRecordDepreDataAsset;
class AVdjmRecordBridgeActor;
class UMediaPlayer;
class FRHIGPUTextureReadback;



/*§		↓			class FVdjmReadBackHelper		begin		↓	 */
class FVdjmReadBackHelper
{
private:
	/*§		↓			struct FVdjmReadBackTextureWrapper		↓	 */
	struct FVdjmReadBackTextureWrapper
	{
		bool bHasRequest = false;
		TUniquePtr<FRHIGPUTextureReadback> ReadBackBuffer = nullptr;
		double TimeStamp = 0.0;

		FVdjmReadBackTextureWrapper();
		void MakeRHIGPUReadback();
		
		
		void EnqueueCopy(FRHICommandList& RHICmdList, FTextureRHIRef srcTexture, double inTimeStamp);		
		bool IsValidTexture() const
		{
			return ReadBackBuffer != nullptr && ReadBackBuffer.IsValid();
		}
		void WhatIsWrong() const
		{
			UE_LOG(LogTemp, Warning, TEXT("ReadBackBuffer is %s"), ReadBackBuffer == nullptr ? TEXT("nullptr") : TEXT("not nullptr"));
			 if (ReadBackBuffer)
			 {
				 UE_LOG(LogTemp, Warning, TEXT("ReadBackBuffer is %s"), ReadBackBuffer.IsValid() ? TEXT("valid") : TEXT("invalid"));
			 	if (ReadBackBuffer->IsReady() )
			 	{
			 		UE_LOG(LogTemp, Warning, TEXT("ReadBackBuffer is ready"));
			 	}
			 	else
			 	{
			 		UE_LOG(LogTemp, Warning, TEXT("ReadBackBuffer is not ready"));
			 	}
			 	
			 }
			 else
			 {
				 UE_LOG(LogTemp, Warning, TEXT("ReadBackBuffer is nullptr, cannot check IsValid()"));
			 }
		}
		bool IsReadReady() const
		{
			if (IsValidTexture())
			{
				return bHasRequest && ReadBackBuffer->IsReady();
			}
			return false;
		}
		void DeleteBuffer()
		{
			if (ReadBackBuffer)
			{
				ReadBackBuffer.Reset();
			}
		}
		void* TextureLock(int32& outWidth, int32& outHeight);
		void TextureUnLock();
	};
	
public:
	static constexpr int32 ReadBackBufferCount = 3;
	
	FVdjmReadBackHelper();
	~FVdjmReadBackHelper()
	{
		for(int i=0; i<ReadBackBufferCount; i++)
		{
			mReadBackWrappers[i].DeleteBuffer();
		}
	}
	
	void Initialize();
	
	bool IsValidReadBacks() const
	{
		for(int i=0; i<ReadBackBufferCount; i++)
		{
			if (not mReadBackWrappers[i].IsValidTexture())
			{
				mReadBackWrappers[i].WhatIsWrong();
				return false;
			}
		}
		return true;
	}
	
	void* TryLockOldest(int32& outWidth,int32& outHeight,double& outTimeStamp);
	void UnlockOldest() ;
	void StopAllReadBacks();
	void EnqueueFrame(FRHICommandList& RHICmdList, FTextureRHIRef SourceTexture, double TimeStamp);
	
	
private:
	int32 mCurrentWriteIndex = 0;
	int32 mCurrentReadIndex = 0;
	FVdjmReadBackTextureWrapper mReadBackWrappers[ReadBackBufferCount];
};/*§	↑			class FVdjmReadBackHelper		end		↑	 */
/*
 ↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
struct FVdjmRecordUnitParamContext
 */
USTRUCT(Blueprintable)
struct VDJMRECORDER_API FVdjmRecordUnitParamContext
{
	GENERATED_BODY()

	UWorld* WorldContext = nullptr;
	FRDGBuilder* GraphBuilder = nullptr;
	TWeakObjectPtr<AVdjmRecordBridgeActor> RecordBridge;
	
	TWeakObjectPtr<UVdjmRecordResource> RecordResource;
	TWeakObjectPtr<UVdjmRecordEnvResolver> RecordEnvResolver;
	
	double CurrentRecordTimeSec = 0.0;
	
	void DbcSetupContextExtended(UWorld* world,UVdjmRecordEnvResolver* resolver , FRDGBuilder* graphBuilder,double currentRecordTimeSec);

	bool DbcIsValidRecordContext() const
	{
		return RecordEnvResolver.IsValid() && RecordResource.IsValid() && GraphBuilder != nullptr;
	} 
	
	bool DbcIsValidUnit() const
    {
        return WorldContext != nullptr
            && RecordBridge.IsValid()
            && DbcIsValidRecordContext();
    }
		
	FVdjmRecordUnitParamContext& Clear()
	{
		WorldContext = nullptr;
		RecordBridge = nullptr;
		RecordEnvResolver = nullptr;
		RecordResource = nullptr;
		GraphBuilder = nullptr;
		return *this;
	}
};
/*
 ↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
struct FVdjmRecordUnitParamPayload
 */
USTRUCT(Blueprintable)
struct VDJMRECORDER_API FVdjmRecordUnitParamPayload
{
	GENERATED_BODY()
	
	TWeakObjectPtr<UVdjmRecordUnit> previousUnit;
	FRDGTextureRef InputTexture;
	FTextureRHIRef OutputTexture;
	//TStringBuilder<512> LogString;
	bool bSuccess = true;

	FVdjmRecordUnitParamPayload() = default;
	FVdjmRecordUnitParamPayload(const FVdjmRecordUnitParamPayload& other)
    {
        previousUnit = other.previousUnit;
        InputTexture = other.InputTexture;
        OutputTexture = other.OutputTexture;
        //LogString.Append(other.LogString);
        bSuccess = other.bSuccess;
    }
	FVdjmRecordUnitParamPayload( FVdjmRecordUnitParamPayload&& other) noexcept
    {
        previousUnit = other.previousUnit;
        InputTexture = other.InputTexture;
        OutputTexture = other.OutputTexture;
        //LogString.Append(other.LogString);
        bSuccess = other.bSuccess;
    }
	
	FVdjmRecordUnitParamPayload& operator=(const FVdjmRecordUnitParamPayload& other)
    {
        previousUnit = other.previousUnit;
        InputTexture = other.InputTexture;
        OutputTexture = other.OutputTexture;
        //LogString.Append(other.LogString);
        bSuccess = other.bSuccess;
        return *this;
    }
	FVdjmRecordUnitParamPayload& operator=( FVdjmRecordUnitParamPayload&& other) noexcept
    {
        previousUnit = other.previousUnit;
        InputTexture = other.InputTexture;
        OutputTexture = other.OutputTexture;
        //LogString.Append(other.LogString);
        bSuccess = other.bSuccess;
        return *this;
    }
	
	FVdjmRecordUnitParamPayload& Clear()
	{
		previousUnit = nullptr;
		InputTexture = nullptr;
		OutputTexture = nullptr;
		//LogString.Reset();
		bSuccess = true;
		return *this;
	}
};
UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordUnit : public UObject
{
	GENERATED_BODY()
public:
	virtual bool InitializeUnit(UVdjmRecordResource* recordResource);
	
	virtual void ExecuteUnit(const FVdjmRecordUnitParamContext& context,FVdjmRecordUnitParamPayload& payload)PURE_VIRTUAL(UVdjmRecordUnit::ExecuteUnit, return; )

	virtual void ReleaseUnit()PURE_VIRTUAL(UVdjmRecordUnit::ReleaseUnit, return; )
	
	virtual EVdjmRecordPipelineStages GetPipelineStage() const PURE_VIRTUAL(UVdjmRecordUnit::GetPipelineStage, return EVdjmRecordPipelineStages::EUndefined; )
	
	virtual int32 GetPipelineStageCustomOrder() const { return 0; }

	virtual bool DbcIsValidUnitInit() const  { return LinkedPipeline.IsValid() && LinkedRecordResource.IsValid();  }
	
	virtual bool DbcRecordUnitStatus() const { return DbcIsValidUnitInit(); }
	
	TWeakObjectPtr<UVdjmRecordUnitPipeline> LinkedPipeline;
	//	 UVdjmRecordUnitPipeline::CreateUnit 에서 설정됨.
	TWeakObjectPtr<UVdjmRecordResource> LinkedRecordResource;
};
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
class UVdjmRecordUnitPipeline : public UObject 		
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

DECLARE_MULTICAST_DELEGATE_TwoParams(FVdjmRecordPipelineEvent,const FVdjmRecordUnitParamContext&,FVdjmRecordUnitParamPayload& );

UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordUnitPipeline : public UObject
{
	GENERATED_BODY()
public:
	UVdjmRecordUnit* CreateUnit(TSubclassOf<UVdjmRecordUnit> unitCls);
	
	virtual bool InitializeRecordPipeline(UVdjmRecordResource* recordResource);

	virtual void ExecuteRecordPipeline(const FVdjmRecordUnitParamContext& context,FVdjmRecordUnitParamPayload& payload)PURE_VIRTUAL(UVdjmRecordUnitPipeline::ExecuteRecordPipeline, return; )
	virtual void StopRecordPipelineExecution() { /* Optional override for pipelines that support stopping mid-execution */ }
	virtual void ReleaseRecordPipeline();
	
	virtual bool DbcIsValidPipelineInit() const;
	virtual bool ExecutePossible() const {return false;}

	bool DbcUnitCheck() const;

	FVdjmRecordPipelineEvent OnBeginPipelineExecution;
	FVdjmRecordPipelineEvent OnBeginExecuteUnit;
	FVdjmRecordPipelineEvent OnEndExecuteUnit;
	FVdjmRecordPipelineEvent OnErrorExecuteUnit;
	FVdjmRecordPipelineEvent OnEndPipelineExecution;

	UPROPERTY()
	TArray<TObjectPtr<UVdjmRecordUnit>> RecordUnits;
	UPROPERTY()
	TWeakObjectPtr<UVdjmRecordResource> LinkedRecordResource;

	TWeakObjectPtr<AVdjmRecordBridgeActor> LinkedBridgeActor;
protected:
	void TravelLoopUnits(TFunctionRef<int32(UVdjmRecordUnit* unit)> travelFunc) const;
};


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓		class  UVdjmRecordData : public UObject				↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
UCLASS()
class VDJMRECORDER_API UVdjmRecordDescriptor : public UObject
{
    GENERATED_BODY()
public:
	UPROPERTY(Category = "Config|Video",
		EditAnywhere)
	bool bUseWindowResolution = true;
	UPROPERTY(Category = "Config|Video",
		EditAnywhere)
	FIntPoint RecordResolution = FIntPoint(1920,1080);
	UPROPERTY(Category = "Config|Video",
		EditAnywhere)
	int32 FrameRate = 30;
    UPROPERTY(Category = "Config|Video",
    	EditAnywhere)
	TMap<EVdjmRecordQualityTiers,int32> BitrateMap;
	UPROPERTY(Category = "Config|Video",
			EditAnywhere)
	EVdjmRecordQualityTiers SelectedBitrateType = EVdjmRecordQualityTiers::EDefault;
	/*
	 *	pc
	 *		high:		10,000,000	(	10Mbps	)
	 *		medium:		 7,500,000	(	7.5Mbps	)
	 *		low:		 5,000,000	(	5Mbps	)
	 *	mobile
	 *		Ultra:		 3,000,000	(	3Mbps	)
	 *		high:		 2,000,000	(	2Mbps	)
	 *		medium:		 1,000,000	(	1Mbps	)
	 *		low:		   750,000	(	750Kbps	)
	 */

	UPROPERTY(Category = "Config|FileIO",
	 EditAnywhere)
	FString FilePrefix;
	UPROPERTY(Category = "Config|FileIO",
	 EditAnywhere)
	EVdjmRecordSavePathDirectoryType SavePathDirectoryType;
	UPROPERTY(Category = "Config|FileIO",
	 EditAnywhere,meta=(EditCondition="SavePathDirectoryType==EVdjmRecordSavePathDirectoryType::ECustomDir") )
	FString CustomSaveDirectory;
	UPROPERTY(Category = "Config|FileIO",
	 EditAnywhere,meta=(EditCondition="SavePathDirectoryType==EVdjmRecordSavePathDirectoryType::ECustomSaverClass") )
	TSubclassOf<UVdjmRecordFileSaver> CustomFileSaverClass;

	UPROPERTY(Category = "Config|Rendering",
	 EditAnywhere)
	TEnumAsByte<ETextureRenderTargetFormat> RenderTargetFormat;
	
	//EPixelFormat
	void CopyForSnapshot(const UVdjmRecordDescriptor*& sourceData);
    /*
	 * GetRecordCachedGroupCount
	 * GetRecordResolution
	 * GetRecordFrameRate
	 * GetRecordBitrate
	 * GetRecordFilePath
	 * GetRenderTargetPixelFormat
	 */
	FIntVector GetRecordCachedGroupCount(const FVdjmRecordGlobalRules& globalRules) const
	{
		return bUseWindowResolution ?
			globalRules.Numthreads : 
			FIntVector(
				FMath::DivideAndRoundUp(RecordResolution.X,globalRules.Numthreads.X),
				FMath::DivideAndRoundUp(RecordResolution.Y,globalRules.Numthreads.Y),
				globalRules.Numthreads.Z);
	}
	FIntPoint GetRecordResolution() const;

    int32 GetRecordFrameRate() const;

    int32 GetRecordBitrate();

    FString GetRecordFilePrefix();

    FString GetRecordFilePath();

    EPixelFormat GetRenderTargetPixelFormat() const;
};

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓		class  UVdjmRecordFileSaver : public UObject		↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordFileSaver : public UObject
{
	GENERATED_BODY()
public:

	
	UPROPERTY(Category = "Config|FileIO",
		EditAnywhere)
	FString TargetFilePath;
};

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
							Declare Delegates	
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

DECLARE_MULTICAST_DELEGATE_OneParam(FVdjmRecordCallBackEvent,UVdjmRecordResource*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FVdjmRecordResourceReadyForFilePath,UVdjmRecordResource*,const FString& );
DECLARE_MULTICAST_DELEGATE_TwoParams(FVdjmRecordChangeStatusEvent,UVdjmRecordResource* /*self*/,EVdjmResourceStatus/*Prev*/);

UENUM(BlueprintType)
enum class EVdjmRecordManifestAuthorityRole : uint8
{
	EUndefined UMETA(DisplayName = "Undefined"),
	EMaster UMETA(DisplayName = "Master"),
	EDeveloper UMETA(DisplayName = "Developer")
};

UENUM(BlueprintType)
enum class EVdjmRecordMediaRegistryEntryStatus : uint8
{
	EUnknown UMETA(DisplayName = "Unknown"),
	EAvailable UMETA(DisplayName = "Available"),
	EMissingMedia UMETA(DisplayName = "Missing Media"),
	EMissingMetadata UMETA(DisplayName = "Missing Metadata"),
	EDeleted UMETA(DisplayName = "Deleted")
};

UENUM(BlueprintType)
enum class EVdjmRecordMediaPreviewStatus : uint8
{
	EUnknown UMETA(DisplayName = "Unknown"),
	ENotReady UMETA(DisplayName = "Not Ready"),
	EReady UMETA(DisplayName = "Ready"),
	EFailed UMETA(DisplayName = "Failed")
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordMediaManifest : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaManifest")
	void Clear();
	bool InitializeFromArtifact(
		const UVdjmRecordArtifact* artifact,
		const FString& metadataFilePath,
		FString& outErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaManifest")
	void SetAuthorityIdentity(
		EVdjmRecordManifestAuthorityRole authorityRole,
		const FString& userId,
		const FString& tokenId,
		const FString& keyId);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaManifest")
	void SetPlaybackLocator(
		const FString& locatorType,
		const FString& locator,
		const FString& streamTokenId,
		int64 expiresUnixTime);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaManifest")
	void SetPreviewInfo(
		const FString& thumbnailFilePath,
		const FString& previewClipFilePath,
		const FString& previewClipMimeType,
		double previewStartTimeSec,
		double previewDurationSec,
		EVdjmRecordMediaPreviewStatus previewStatus,
		const FString& previewErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaManifest")
	void SetMediaPublishResult(
		EVdjmRecordMediaPublishStatus publishStatus,
		const FString& publishedContentUri,
		const FString& publishedDisplayName,
		const FString& publishedRelativePath,
		const FString& publishErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaManifest")
	bool ValidateManifest(FString& outErrorReason) const;
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString ToString() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString ToJsonString() const;
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaManifest")
	bool LoadFromJsonString(
		const FString& manifestJsonString,
		const FString& fallbackMetadataFilePath,
		FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaManifest")
	bool SaveToFile(const FString& metadataFilePath, FString& outErrorReason);

	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	bool IsInitializedManifest() const { return mbInitialized; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetRecordId() const { return mRecordId; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	int64 GetCreatedUnixTime() const { return mCreatedUnixTime; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetOutputFilePath() const { return mOutputFilePath; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetMetadataFilePath() const { return mMetadataFilePath; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	int64 GetFileSizeBytes() const { return mFileSizeBytes; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	int32 GetRecordedFrameCount() const { return mRecordedFrameCount; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	EVdjmRecordEnvPlatform GetTargetPlatform() const { return mTargetPlatform; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	int32 GetVideoWidth() const { return mVideoWidth; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	int32 GetVideoHeight() const { return mVideoHeight; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	int32 GetVideoFrameRate() const { return mVideoFrameRate; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	int32 GetVideoBitrate() const { return mVideoBitrate; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetVideoMimeType() const { return mVideoMimeType; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetPlaybackLocatorType() const { return mPlaybackLocatorType; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetPlaybackLocator() const { return mPlaybackLocator; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetThumbnailFilePath() const { return mThumbnailFilePath; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetPreviewClipFilePath() const { return mPreviewClipFilePath; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetPreviewClipMimeType() const { return mPreviewClipMimeType; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	double GetPreviewStartTimeSec() const { return mPreviewStartTimeSec; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	double GetPreviewDurationSec() const { return mPreviewDurationSec; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	double GetPreviewEndTimeSec() const { return mPreviewStartTimeSec + mPreviewDurationSec; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	EVdjmRecordMediaPreviewStatus GetPreviewStatus() const { return mPreviewStatus; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetPreviewErrorReason() const { return mPreviewErrorReason; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	EVdjmRecordManifestAuthorityRole GetAuthorityRole() const { return mAuthorityRole; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetAuthorityUserId() const { return mAuthorityUserId; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetAuthorityTokenId() const { return mAuthorityTokenId; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetAuthorityKeyId() const { return mAuthorityKeyId; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	EVdjmRecordMediaPublishStatus GetMediaPublishStatus() const { return mMediaPublishStatus; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetPublishedContentUri() const { return mPublishedContentUri; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetPublishedDisplayName() const { return mPublishedDisplayName; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetPublishedRelativePath() const { return mPublishedRelativePath; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaManifest")
	FString GetMediaPublishErrorReason() const { return mMediaPublishErrorReason; }

private:
	int32 mSchemaVersion = 1;
	FString mRecordId;
	int64 mCreatedUnixTime = 0;
	FString mSourceApp = TEXT("vdjm");
	FString mOutputFilePath;
	FString mMetadataFilePath;
	int64 mFileSizeBytes = -1;
	int32 mRecordedFrameCount = 0;
	EVdjmRecordEnvPlatform mTargetPlatform = EVdjmRecordEnvPlatform::EDefault;
	int32 mVideoWidth = 0;
	int32 mVideoHeight = 0;
	int32 mVideoFrameRate = 0;
	int32 mVideoBitrate = 0;
	FString mVideoMimeType;
	FString mPlaybackLocatorType = TEXT("local_path");
	FString mPlaybackLocator;
	FString mStreamTokenId;
	int64 mPlaybackExpiresUnixTime = 0;
	FString mThumbnailFilePath;
	FString mPreviewClipFilePath;
	FString mPreviewClipMimeType = TEXT("video/mp4");
	FString mPreviewErrorReason;
	double mPreviewStartTimeSec = 0.0;
	double mPreviewDurationSec = 3.0;
	EVdjmRecordMediaPreviewStatus mPreviewStatus = EVdjmRecordMediaPreviewStatus::ENotReady;
	EVdjmRecordManifestAuthorityRole mAuthorityRole = EVdjmRecordManifestAuthorityRole::EDeveloper;
	FString mAuthorityUserId;
	FString mAuthorityTokenId;
	FString mAuthorityKeyId;
	FString mVideoSha256;
	FString mMetadataSha256;
	FString mSignature;
	EVdjmRecordMediaPublishStatus mMediaPublishStatus = EVdjmRecordMediaPublishStatus::ENotStarted;
	FString mPublishedContentUri;
	FString mPublishedDisplayName;
	FString mPublishedRelativePath;
	FString mMediaPublishErrorReason;
	bool mbRequiresAuth = true;
	bool mbInitialized = false;
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordMediaRegistryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString RecordId;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString OutputFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString MetadataFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString PlaybackLocatorType;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString PlaybackLocator;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString ThumbnailFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString PreviewClipFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString PreviewClipMimeType;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString PreviewErrorReason;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString PublishedContentUri;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString PublishedDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString PublishedRelativePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString VideoMimeType;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	FString LastErrorReason;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	int64 CreatedUnixTime = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	int64 FileSizeBytes = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	int32 RecordedFrameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	int32 VideoWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	int32 VideoHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	int32 VideoFrameRate = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	int32 VideoBitrate = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	double PreviewStartTimeSec = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	double PreviewDurationSec = 3.0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	EVdjmRecordEnvPlatform TargetPlatform = EVdjmRecordEnvPlatform::EDefault;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	EVdjmRecordMediaPublishStatus MediaPublishStatus = EVdjmRecordMediaPublishStatus::ENotStarted;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	EVdjmRecordMediaRegistryEntryStatus RegistryStatus = EVdjmRecordMediaRegistryEntryStatus::EUnknown;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	EVdjmRecordMediaPreviewStatus PreviewStatus = EVdjmRecordMediaPreviewStatus::ENotReady;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	bool bOutputFileExists = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	bool bMetadataFileExists = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaRegistry")
	bool bIsDeleted = false;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordArtifact : public UObject
{
	GENERATED_BODY()

public:
	bool InitializeFromSnapshot(
		const FVdjmRecordEncoderSnapshot& encoderSnapshot,
		int32 recordedFrameCount,
		UVdjmRecorderController* ownerController,
		FString& outErrorReason);
	bool ValidateArtifact(FString& outErrorReason);
	void SetMediaManifest(UVdjmRecordMediaManifest* mediaManifest, bool bMetadataValidated, const FString& validationError);
	void SetMediaPublishResult(
		EVdjmRecordMediaPublishStatus publishStatus,
		const FString& publishedContentUri,
		const FString& publishErrorReason);
	void MarkOutputDeleted(const FString& deletionReason);

	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	bool IsInitializedArtifact() const { return mbInitialized; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	bool IsValidArtifact() const { return mbValidated; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	bool DoesOutputFileExist() const { return mbFileExists; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	FString GetOutputFilePath() const { return mOutputFilePath; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	int64 GetFileSizeBytes() const { return mFileSizeBytes; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	int32 GetRecordedFrameCount() const { return mRecordedFrameCount; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	FString GetValidationError() const { return mValidationError; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	EVdjmRecordEnvPlatform GetTargetPlatform() const { return mEncoderSnapshot.TargetPlatform; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	int32 GetVideoWidth() const { return mEncoderSnapshot.VideoConfig.VideoWidth; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	int32 GetVideoHeight() const { return mEncoderSnapshot.VideoConfig.VideoHeight; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	int32 GetVideoFrameRate() const { return mEncoderSnapshot.VideoConfig.VideoFPS; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	int32 GetVideoBitrate() const { return mEncoderSnapshot.VideoConfig.VideoBitrate; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	FString GetVideoMimeType() const { return mEncoderSnapshot.VideoConfig.MimeType; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact")
	UObject* GetOwnerControllerObject() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact|Metadata")
	UVdjmRecordMediaManifest* GetMediaManifest() const { return mMediaManifest; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact|Metadata")
	FString GetMetadataFilePath() const { return mMetadataFilePath; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact|Metadata")
	bool HasMetadata() const { return mbHasMetadata; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact|Metadata")
	bool IsMetadataValidated() const { return mbMetadataValidated; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact|Metadata")
	FString GetMetadataValidationError() const { return mMetadataValidationError; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact|Media")
	EVdjmRecordMediaPublishStatus GetMediaPublishStatus() const { return mMediaPublishStatus; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact|Media")
	FString GetPublishedContentUri() const { return mPublishedContentUri; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact|Media")
	FString GetMediaPublishErrorReason() const { return mMediaPublishErrorReason; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Artifact|Media")
	bool IsMediaPublished() const { return mMediaPublishStatus == EVdjmRecordMediaPublishStatus::EPublished; }

	const FVdjmRecordEncoderSnapshot& GetEncoderSnapshot() const { return mEncoderSnapshot; }

private:
	FVdjmRecordEncoderSnapshot mEncoderSnapshot;
	TWeakObjectPtr<UVdjmRecorderController> mOwnerController;
	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordMediaManifest> mMediaManifest;
	FString mOutputFilePath;
	FString mMetadataFilePath;
	FString mValidationError;
	FString mMetadataValidationError;
	FString mPublishedContentUri;
	FString mMediaPublishErrorReason;
	int64 mFileSizeBytes = -1;
	int32 mRecordedFrameCount = 0;
	double mCreatedAtSeconds = 0.0;
	EVdjmRecordMediaPublishStatus mMediaPublishStatus = EVdjmRecordMediaPublishStatus::ENotStarted;
	bool mbInitialized = false;
	bool mbValidated = false;
	bool mbFileExists = false;
	bool mbHasMetadata = false;
	bool mbMetadataValidated = false;
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordMediaPostProcessSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|PostProcess")
	bool bIsPostProcessingMedia = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|PostProcess")
	int32 ActiveMediaPublishJobCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|PostProcess")
	int32 CompletedMediaPublishJobCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|PostProcess")
	EVdjmRecordMediaPublishStatus LastMediaPublishStatus = EVdjmRecordMediaPublishStatus::ENotStarted;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|PostProcess")
	FString LastPublishedContentUri;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|PostProcess")
	FString LastErrorReason;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordMetadataStore : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecordMetadataStore* FindMetadataStore(UObject* worldContextObject);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecordMetadataStore* FindOrCreateMetadataStore(UObject* worldContextObject);

	bool InitializeStore(UObject* worldContextObject);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata")
	void Clear();
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata")
	void SetAuthorityIdentity(
		EVdjmRecordManifestAuthorityRole authorityRole,
		const FString& userId,
		const FString& tokenId,
		const FString& keyId);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata")
	void SetDeleteVideoIfMetadataMissing(bool bDeleteVideoIfMetadataMissing);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata")
	bool BuildAndSaveManifest(UVdjmRecordArtifact* artifact, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata|PostProcess")
	bool EnqueueArtifactMediaPublish(
		UVdjmRecordArtifact* artifact,
		UVdjmRecordMediaManifest* mediaManifest,
		FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata|Registry")
	bool LoadRegistry(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata|Registry")
	bool SaveRegistry(FString& outErrorReason) const;
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata|Registry")
	bool RefreshRegistryFromDisk(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata|Registry")
	bool RegisterManifest(UVdjmRecordMediaManifest* mediaManifest, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata|Registry")
	bool LoadManifestFromFile(
		const FString& metadataFilePath,
		UVdjmRecordMediaManifest*& outManifest,
		FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata|Registry")
	bool LoadManifestFromRegistryEntry(
		const FVdjmRecordMediaRegistryEntry& registryEntry,
		UVdjmRecordMediaManifest*& outManifest,
		FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|Metadata|Registry")
	void ClearRegistry();

	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata")
	UVdjmRecordMediaManifest* GetLastManifest() const { return mLastManifest; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata")
	bool ShouldDeleteVideoIfMetadataMissing() const { return mbDeleteVideoIfMetadataMissing; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata|Registry")
	TArray<FVdjmRecordMediaRegistryEntry> GetMediaRegistryEntries() const { return mRegistryEntries; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata|Registry")
	int32 GetMediaRegistryEntryCount() const { return mRegistryEntries.Num(); }
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata|Registry")
	FString GetRegistryFilePath() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata|PostProcess")
	FVdjmRecordMediaPostProcessSnapshot GetMediaPostProcessSnapshot() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata|PostProcess")
	bool IsPostProcessingMedia() const { return mActiveMediaPublishJobCount > 0; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata|PostProcess")
	int32 GetActiveMediaPublishJobCount() const { return mActiveMediaPublishJobCount; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata|PostProcess")
	EVdjmRecordMediaPublishStatus GetLastMediaPublishStatus() const { return mLastMediaPublishStatus; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata|PostProcess")
	FString GetLastPublishedContentUri() const { return mLastPublishedContentUri; }
	UFUNCTION(BlueprintPure, Category = "Recorder|Metadata|PostProcess")
	FString GetLastMediaPublishErrorReason() const { return mLastMediaPublishErrorReason; }

private:
	void CompleteArtifactMediaPublishOnGameThread(
		int32 jobId,
		UVdjmRecordArtifact* artifact,
		UVdjmRecordMediaManifest* mediaManifest,
		EVdjmRecordMediaPublishStatus publishStatus,
		const FString& publishedContentUri,
		const FString& publishedDisplayName,
		const FString& publishedRelativePath,
		const FString& publishErrorReason);
	bool BuildRegistryEntryFromManifest(
		const UVdjmRecordMediaManifest* mediaManifest,
		FVdjmRecordMediaRegistryEntry& outEntry,
		FString& outErrorReason) const;
	bool BuildRegistryEntryFromManifestFile(
		const FString& metadataFilePath,
		FVdjmRecordMediaRegistryEntry& outEntry,
		FString& outErrorReason) const;
	void RefreshRegistryEntryFileState(FVdjmRecordMediaRegistryEntry& entry) const;
	bool UpsertRegistryEntry(const FVdjmRecordMediaRegistryEntry& entry);
	FString GetManifestDirectoryPath() const;
	FString MakeMetadataFilePathForArtifact(const UVdjmRecordArtifact* artifact) const;
	bool DeleteVideoFileForMissingMetadata(UVdjmRecordArtifact* artifact, const FString& reason, FString& outErrorReason) const;

	TWeakObjectPtr<UWorld> mCachedWorld;
	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordMediaManifest> mLastManifest;
	UPROPERTY(Transient)
	TArray<FVdjmRecordMediaRegistryEntry> mRegistryEntries;
	EVdjmRecordManifestAuthorityRole mAuthorityRole = EVdjmRecordManifestAuthorityRole::EDeveloper;
	FString mAuthorityUserId;
	FString mAuthorityTokenId;
	FString mAuthorityKeyId;
	FString mLastPublishedContentUri;
	FString mLastMediaPublishErrorReason;
	int32 mNextPostProcessJobId = 1;
	int32 mActiveMediaPublishJobCount = 0;
	int32 mCompletedMediaPublishJobCount = 0;
	EVdjmRecordMediaPublishStatus mLastMediaPublishStatus = EVdjmRecordMediaPublishStatus::ENotStarted;
	bool mbDeleteVideoIfMetadataMissing = true;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordMediaPreviewPlayer : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecordMediaPreviewPlayer* CreateMediaPreviewPlayer(
		UObject* worldContextObject,
		UMediaPlayer* mediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	bool StartPreviewFromManifest(
		UMediaPlayer* mediaPlayer,
		UVdjmRecordMediaManifest* mediaManifest,
		FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	bool StartPreviewFromRegistryEntry(
		UMediaPlayer* mediaPlayer,
		const FVdjmRecordMediaRegistryEntry& registryEntry,
		FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	void StopPreview(bool bCloseMedia);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	bool RestartPreview(FString& outErrorReason);

	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	bool IsPreviewActive() const { return mbPreviewActive; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	bool IsPreviewOpened() const { return mbPreviewOpened; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	FString GetCurrentSource() const { return mCurrentSource; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	double GetPreviewStartTimeSec() const { return mPreviewStartTimeSec; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	double GetPreviewEndTimeSec() const { return mPreviewEndTimeSec; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	FString GetLastErrorReason() const { return mLastErrorReason; }

	virtual void Tick(float deltaTime) override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override;

protected:
	virtual UWorld* GetWorld() const override;

private:
	UFUNCTION()
	void HandleMediaOpened(FString openedUrl);
	UFUNCTION()
	void HandleMediaOpenFailed(FString failedUrl);
	UFUNCTION()
	void HandleMediaEndReached();

	bool StartPreviewInternal(
		UMediaPlayer* mediaPlayer,
		const FString& source,
		const FString& sourceType,
		double previewStartTimeSec,
		double previewDurationSec,
		FString& outErrorReason);
	bool OpenCurrentSource(FString& outErrorReason);
	void BindMediaPlayerEvents();
	void UnbindMediaPlayerEvents();
	void SeekPreviewStartAndPlay();
	void ResetPreviewState();

	TWeakObjectPtr<UWorld> mCachedWorld;
	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> mMediaPlayer;
	FString mCurrentSource;
	FString mCurrentSourceType;
	FString mLastErrorReason;
	double mPreviewStartTimeSec = 0.0;
	double mPreviewEndTimeSec = 3.0;
	bool mbPreviewActive = false;
	bool mbPreviewOpened = false;
	bool mbPendingInitialSeek = false;
};

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class UVdjmRecordResource : public UObject
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
*/
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	
	class  UVdjmRecordFileSaver : public UObject
	@brief 녹화에 필요한 리소스들을 관리하는 클래스
	@detail
	- AVdjmRecordBridgeActor 가 소유하고 관리함.
	- 녹화에 필요한 텍스처 풀링, 녹화 설정 정보 등을 포함함.
	- 녹화 유닛들이 녹화 진행 중에 필요한 리소스들을 참조할 수 있도록 함.
	- dependency in : AVdjmRecordBridgeActor, UVdjmRecordEnvCurrentInfo
*/
UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordResource : public UObject
{
	GENERATED_BODY()
public:
	virtual void BeginDestroy() override;
	
	virtual bool InitializeResource(UVdjmRecordEnvResolver* resolver);
	bool RefreshResolvedRuntimeConfigFromResolver();
	bool UpdateFinalFilePathFromResolver();
	bool BuildEncoderSnapshot(FVdjmRecordEncoderSnapshot& outSnapshot) const;
	UVdjmRecordArtifact* BuildRecordArtifact(
		UObject* artifactOuter,
		UVdjmRecorderController* ownerController,
		int32 recordedFrameCount,
		FString& outErrorReason) const;
	virtual void ResetResource();
	virtual void ReleaseResources();
	
	virtual FTextureRHIRef GetCurrPooledTextureRHI()PURE_VIRTUAL(UVdjmRecordResource::GetCurrPooledTextureRHI, return nullptr; );
	virtual FTextureRHIRef GetNextPooledTextureRHI()PURE_VIRTUAL(UVdjmRecordResource::GetNextPooledTextureRHI, return nullptr; );
	
	virtual FString ToString() const { return FString::Printf(TEXT("UVdjmRecordResource - Status: %s"), *UEnum::GetValueAsString(CurrentResourceStatus)); }
	
	bool DbcIsValidResourceInit() const
	{
		return (LinkedOwnerBridge.IsValid());
	}
	bool DbcIsInitializedResource() const
	{
		return 	LinkedOwnerBridge.IsValid() && LinkedResolver.IsValid();
	}
	
	virtual bool DbcIsValidResource() const
	{
		return DbcIsInitializedResource();
	}
	virtual bool IsLazyPostInitializeCheck() const
	{
		return false;
	}
	/*
	 * 굳이 안써도됨. 
	 */
	void OnStatusChanged(EVdjmResourceStatus newStatus)
	{
		EVdjmResourceStatus prevStatus = CurrentResourceStatus;
		CurrentResourceStatus = newStatus;
		OnChangeResourceStatusFunc.Broadcast(this,prevStatus);//	현재는 알아서 얻어와라.
	}
	void OnStatusChangeNewToReady()
	{
		if (not DbcIsValidResourceInit())
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::OnStatusChangeNewToReady - Resource is not valid. Call Initialize() first."));
			return;
		}
		OnStatusChanged(EVdjmResourceStatus::EReady);
	}
	void OnStatusChangeReadyToRunning()
	{
		if (not DbcIsInitializedResource())
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::DbcIsDefaultReady - Resource is not valid. Call Initialize() first."));
			return;
		}
		OnStatusChanged(EVdjmResourceStatus::ERunning);
	}
	void OnStatusChangeRunningToWaiting()
	{		
		OnStatusChanged(EVdjmResourceStatus::EWaiting);
	}
	void OnStatusChangeWaitingToTerminated()
	{		
		OnStatusChanged(EVdjmResourceStatus::ETerminated);
	}
	void OnStatusChangeAnyToError()
	{		
		OnStatusChanged(EVdjmResourceStatus::EError);
	}
	
	//UPROPERTY(BlueprintAssignable)
	//FVdjmRecordEvent OnResourceTexturePoolInitialized;
	FVdjmRecordChangeStatusEvent OnChangeResourceStatusFunc;
	FVdjmRecordCallBackEvent OnResourceReadyForPostInit;
	FVdjmRecordResourceReadyForFilePath OnResourceReadyForFilePath;
	
	FIntVector	CachedGroupCount;
	FIntPoint	OriginResolution;
	FIntPoint	TextureResolution;
	int32		FinalFrameRate = 30;
	int32		FinalBitrate = 2000000;
	FString		FinalFilePath;	//	이거는 여기에서 해줄게 아님. platform 마다 달라야함.
	EPixelFormat FinalPixelFormat = PF_A8R8G8B8;
	
	TWeakObjectPtr<AVdjmRecordBridgeActor> LinkedOwnerBridge;// InitializeResourceExtended에서 설정됨. 그 전까지는 nullptr 이므로 주의.
	TWeakObjectPtr<UVdjmRecordEnvResolver> LinkedResolver;	//	이거는 InitializeResourceExtended( in ChainInit_CreateRecordResource )에서 설정됨. 그 전까지는 nullptr 이므로 주의.
protected:
	FTextureRHIRef CreateTextureForNV12(FIntPoint resolution,EPixelFormat pixelformat,ETextureCreateFlags createFlags);
	EVdjmResourceStatus CurrentResourceStatus = EVdjmResourceStatus::ENew;	
};

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	
	struct FVdjmRecordEnvPlatformInfo 

*/
USTRUCT(Blueprintable)
struct VDJMRECORDER_API FVdjmRecordEnvPlatformInfo 
{
	/*
	* TODO(260410-cofigs) 여기 안 오디오를 넣어야함.
	*/
	GENERATED_BODY()
	UPROPERTY(Category ="Record|Env",EditAnywhere)
	bool bUseAutoTargetPlatformResolution = false;
	UPROPERTY(Category ="Record|Env",EditAnywhere)
	FIntPoint Resolution = FIntPoint(1920,1080);
	UPROPERTY(Category ="Record|Env",EditAnywhere)
	int32 FrameRate = 30;
	UPROPERTY(Category ="Record|Env",EditAnywhere)
	TEnumAsByte<EPixelFormat> PixelFormat = EPixelFormat::PF_A8R8G8B8;
	
	UPROPERTY(Category ="Record|Env",EditAnywhere,
		meta=(DisplayName="Bitrate Table (Mbps)", ClampMin="1", UIMin="1"))
	TMap<EVdjmRecordQualityTiers,float> BitrateMap;	//	Default 로 선택을 해놔라.
	
    UPROPERTY(Category ="Record|Env",EditAnywhere)
	TSubclassOf<UVdjmRecordResource> RecordResourceClass;
	
	UPROPERTY(Category ="Record|Env|Save",EditAnywhere)
	FString FilePrefix;
    UPROPERTY(Category ="Record|Env|Save",EditAnywhere)
	TSubclassOf<UVdjmRecordFileSaver> CustomFileSaverClass;	//	nullptr 이면 그냥 디폴트로 저장함. 무조건 filePath 는 플렛폼결로 지정된 곳에 저장.
	UPROPERTY(Category ="Record|Env|Pipeline",EditAnywhere)
	TSubclassOf<UVdjmRecordUnitPipeline> PipelineClass;
    UPROPERTY(Category ="Record|Env|Pipeline",EditAnywhere)
	TMap<EVdjmRecordPipelineStages,TSubclassOf<UVdjmRecordUnit>> PipelineUnitClassMap;

	const TSubclassOf<UVdjmRecordUnit>* GetPipelineState(const EVdjmRecordPipelineStages& stage)
	{
		return PipelineUnitClassMap.Find(stage);
	}
	
	bool DbcIsValidResolution() const
	{
		return bUseAutoTargetPlatformResolution || (Resolution.X > 0 && Resolution.Y > 0);
	}
	bool DbcIsValid() const
	{
		return RecordResourceClass != nullptr
			&& PipelineClass != nullptr
			&& PipelineUnitClassMap.Num() > 0
			&& BitrateMap .Num() > 0 
			&& FrameRate > 0 
			&& DbcIsValidResolution();
	}
};
//	vdjm 20260410
USTRUCT()
struct FVdjmRecordEnvPlatformPreset
{
	GENERATED_BODY()
	
	UPROPERTY(Category ="Record|Env",EditAnywhere)
	EVdjmRecordQualityTiers DefaultQualityTier = EVdjmRecordQualityTiers::EDefault;
	
	UPROPERTY(Category ="Record|Env",EditAnywhere)
	TSubclassOf<UVdjmRecordResource> RecordResourceClass;
	
	UPROPERTY(Category ="Record|Env|Pipeline",EditAnywhere)
	TSubclassOf<UVdjmRecordUnitPipeline> PipelineClass;
	UPROPERTY(Category ="Record|Env|Pipeline",EditAnywhere)
	TMap<EVdjmRecordPipelineStages,TSubclassOf<UVdjmRecordUnit>> PipelineUnitClassMap;
	
	UPROPERTY(Category ="Record|Env|InitRequest",EditAnywhere)
	TMap<EVdjmRecordQualityTiers,FVdjmEncoderInitRequest> EncoderInitRequestMap;
	
	void Clear()
	{
		DefaultQualityTier = EVdjmRecordQualityTiers::EDefault;
		RecordResourceClass = nullptr;
		PipelineClass = nullptr;
		PipelineUnitClassMap.Empty();
		EncoderInitRequestMap.Empty();
	}
	FString ToString() const
	{
		FString result = FString::Printf(TEXT("DefaultQualityTier: %d\n"), static_cast<int32>(DefaultQualityTier));
		result += FString::Printf(TEXT("RecordResourceClass: %s\n"), *GetNameSafe(RecordResourceClass));
		result += FString::Printf(TEXT("PipelineClass: %s\n"), *GetNameSafe(PipelineClass));
		result += FString::Printf(TEXT("PipelineUnitClassMap:\n"));
		for (const auto& Pair : PipelineUnitClassMap)		{
			result += FString::Printf(TEXT("  Stage: %d, UnitClass: %s\n"), static_cast<int32>(Pair.Key), *GetNameSafe(Pair.Value));
		}
		result += FString::Printf(TEXT("EncoderInitRequestMap:\n"));
		for (const auto& Pair : EncoderInitRequestMap)		{
			result += FString::Printf(TEXT("  QualityTier: %d, EncoderInitRequest: %s\n"), static_cast<int32>(Pair.Key), *Pair.Value.ToString());
		}
		return result;
	}

	const TSubclassOf<UVdjmRecordUnit>* GetPipelineState(const EVdjmRecordPipelineStages& stage) const
	{
		return PipelineUnitClassMap.Find(stage);
	}
	
		bool DbcIsValid() const
		{
			if (DefaultQualityTier == EVdjmRecordQualityTiers::EUndefined
				|| RecordResourceClass == nullptr
				|| PipelineClass == nullptr
				|| PipelineUnitClassMap.Num() <= 0
				|| EncoderInitRequestMap.Num() <= 0)
			{
				return false;
			}
			for (const TPair<EVdjmRecordQualityTiers, FVdjmEncoderInitRequest>& Pair : EncoderInitRequestMap)
			{
				const FVdjmEncoderInitRequest& request = Pair.Value;
				
				if (not request.EvaluateValidation())
				{
					const EVdjmRecordQualityTiers qualityTier = Pair.Key;
					UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmRecordEnvPlatformPreset::DbcIsValid - EncoderInitRequest for QualityTier { %s } is not valid."),*StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(qualityTier) );
					return false;
				}
			}
			return true;
		}
	
	const FVdjmEncoderInitRequest* GetEncoderInitRequest(EVdjmRecordQualityTiers qualityTier =EVdjmRecordQualityTiers::EUndefined ) const
	{
		if (qualityTier == EVdjmRecordQualityTiers::EUndefined)
		{
			qualityTier = DefaultQualityTier;
		}
		if (const FVdjmEncoderInitRequest* foundRequest = EncoderInitRequestMap.Find(qualityTier))
		{
			return foundRequest;
		}
		else if (const FVdjmEncoderInitRequest* defaultRequest = EncoderInitRequestMap.Find(EVdjmRecordQualityTiers::EDefault))
		{
			return defaultRequest;
		}
		else
		{
			return nullptr;
		}
	}
};

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	class UVdjmRecordEnvDataAsset :public UPrimaryDataAsset
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
UCLASS()
class VDJMRECORDER_API UVdjmRecordEnvDataAsset :public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	FVdjmRecordGlobalRules GlobalRules;
	
	UPROPERTY(EditAnywhere)
	TMap<EVdjmRecordEnvPlatform,FVdjmRecordEnvPlatformInfo> PlatformInfoMap;
	
	//	vdjm 20260410
	UPROPERTY(EditAnywhere)	
	TMap<EVdjmRecordEnvPlatform,FVdjmRecordEnvPlatformPreset> PlatformDataMap;
	

	/*
	* TODO(260410-cofigs) : GetPlatformInfo 여기에서 FVdjmRecordEnvPlatformInfo 이걸 검증해주는 거라 FVdjmRecordEnvPlatformInfo의 검증부분을 늘리면 될듯.
	*/
	FVdjmRecordEnvPlatformInfo* GetPlatformInfo(EVdjmRecordEnvPlatform targetPlatform)
    {
        if (FVdjmRecordEnvPlatformInfo* foundInfo = PlatformInfoMap.Find(targetPlatform))
        {
        	foundInfo->FrameRate = FMath::Min(foundInfo->FrameRate, GlobalRules.MaxFrameRate);
            return const_cast<FVdjmRecordEnvPlatformInfo*>(foundInfo);
        }
        return nullptr;
    }
	const FVdjmRecordEnvPlatformPreset* GetPlatformPreset(EVdjmRecordEnvPlatform targetPlatform) const
	{
		return PlatformDataMap.Find(targetPlatform);
	}
	
	bool DbcGlobalRulesValid() const
	{
		return GlobalRules.MaxRecordDurationSeconds > 0.0f
			&& GlobalRules.MinFrameRate > 0
			&& GlobalRules.MaxFrameRate >= GlobalRules.MinFrameRate
			&& GlobalRules.Numthreads.X > 0
			&& GlobalRules.Numthreads.Y > 0
			&& GlobalRules.Numthreads.Z > 0;
	}
	bool DbcPlatformInfoValid() const
	{
		for (const auto& pair : PlatformInfoMap)
		{
			if (not pair.Value.DbcIsValid())
			{
				return false;
			}
		}
		return true;
	}
	bool DbcPlatformPresetValid() const
	{
		for (const auto& pair : PlatformDataMap)		
		{
			if (not pair.Value.DbcIsValid())
			{
				return false;
			}
		}
		return true;
	}
	
	
	bool DbcIsValid() const
	{
		return DbcGlobalRulesValid() && DbcPlatformInfoValid();
	}
};

UCLASS()
class VDJMRECORDER_API UVdjmRecordEnvResolver : public UObject
{
	GENERATED_BODY()
public:
	//	여기 단계에서 linkedOwnerBridge 부터 GlobalRules 를 검증 및 소유. 
	bool InitResolverEnvironment(AVdjmRecordBridgeActor* ownerBridge);
	UVdjmRecordResource* CreateResolvedRecordResource(const FVdjmRecordEnvPlatformPreset* presetData) ;
	
	bool InitComplete(AVdjmRecordBridgeActor* ownerBridge,UVdjmRecordResource* resource, UVdjmRecordUnitPipeline* pipeline);
	
	void Clear() 
	{
		mHasResolved = false;
		mResolvedPreset.Clear();
		mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
	}
	FString ToString() const
	{
		return FString::Printf(TEXT("ResolvedQualityTier: %s\nResolvedPreset: %s"), *StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(mResolvedQualityTier), *mResolvedPreset.ToString());
	}
	
	const FVdjmRecordEnvPlatformPreset& GetResolvedEnvPreset() const { return mResolvedPreset; }
	const FVdjmRecordGlobalRules& GetResolvedGlobalRules() const { return mGlobalRules; }
	void SetResolvedGlobalRules(const FVdjmRecordGlobalRules& globalRules) { mGlobalRules = globalRules; }
	
	TSubclassOf<UVdjmRecordResource> TryGetResolvedRecordResourceClass() const
	{
		return IsValidPreset() ? mResolvedPreset.RecordResourceClass : nullptr;
	}
	TSubclassOf<UVdjmRecordUnitPipeline> TryGetResolvedPipelineClass() const
	{
		return IsValidPreset() ? mResolvedPreset.PipelineClass : nullptr;
	}
	const FVdjmEncoderInitRequest* TryGetResolvedEncoderInitRequest() const
	{
		return mResolvedPreset.GetEncoderInitRequest(mResolvedQualityTier);
	}
	const FVdjmEncoderInitRequestVideo* TryGetResolvedVideoConfig() const
	{
		if (const FVdjmEncoderInitRequest* initRequest = TryGetResolvedEncoderInitRequest())
		{
			return &initRequest->VideoConfig;
		} 
		return nullptr;
	}
	const FVdjmEncoderInitRequestAudio* TryGetResolvedAudioConfig() const
	{
		if (const FVdjmEncoderInitRequest* initRequest = TryGetResolvedEncoderInitRequest())
		{
			return &initRequest->AudioConfig;
		} 
		return nullptr;
	}
	const FVdjmEncoderInitRequestOutput* TryGetResolvedOutputConfig() const
	{
		if (const FVdjmEncoderInitRequest* initRequest = TryGetResolvedEncoderInitRequest())
		{
			return &initRequest->OutputConfig;
		} 
		return nullptr;
	}
	const FVdjmEncoderInitRequestRuntimePolicy* TryGetResolvedRuntimePolicyConfig() const
	{
		if (const FVdjmEncoderInitRequest* initRequest = TryGetResolvedEncoderInitRequest())
		{
			return &initRequest->RuntimePolicyConfig;
		} 
		return nullptr;
	}
		const FVdjmEncoderInitRequestPlatformExtension* TryGetResolvedPlatformExtensionConfig() const
		{
			if (const FVdjmEncoderInitRequest* initRequest = TryGetResolvedEncoderInitRequest())
		{
			return &initRequest->PlatformExtensionConfig;
		} 
			return nullptr;
		}
		bool RefreshResolvedOutputPath();
		TSubclassOf<UVdjmRecordUnit> TryGetResolvedPipelineUnitClass(EVdjmRecordPipelineStages stage) const
		{
		if (IsValidPreset())
		{
			return *mResolvedPreset.PipelineUnitClassMap.Find(stage);
		}
		return nullptr;
	}
	bool IsValidResolved() const
	{
		return mResolvedQualityTier != EVdjmRecordQualityTiers::EUndefined;
	}
	bool IsPresetQualityTier()const
	{
		return IsValidResolved() && mResolvedQualityTier != EVdjmRecordQualityTiers::EDefault;
	}
	bool IsCustomQualityTier()const
	{
		return mResolvedQualityTier == EVdjmRecordQualityTiers::ECustom;
	}
	bool IsValidPreset() const	//	Pipeline 을 생성하기 전에 이걸로 검증을 해라.mResolvedPreset.DbcIsValid() 가 중요
	{
		return LinkedOwnerBridge.IsValid() && IsValidResolved() && mResolvedPreset.DbcIsValid();
	}
	bool DbcIsValidEnvResolverInit() const	//	ChainInit_CreateRecordPipeline 이게 끝난 시점에만 사용 가능.
	{
		return IsValidPreset() && LinkedRecordResource.IsValid() && LinkedPipeline.IsValid();
	}
	bool HasResolved() const
	{
		return mHasResolved;
	}
	
	TWeakObjectPtr<AVdjmRecordBridgeActor> LinkedOwnerBridge = nullptr;
	TWeakObjectPtr<UVdjmRecordResource> LinkedRecordResource = nullptr;
	TWeakObjectPtr<UVdjmRecordUnitPipeline> LinkedPipeline = nullptr;

	bool ResolveEnvPlatform(const FVdjmRecordEnvPlatformPreset* presetData);

private:
	bool ResolvedFinalFilePath(const FString& customFileName);
	
	
	FVdjmRecordGlobalRules mGlobalRules;
	FVdjmRecordEnvPlatformPreset mResolvedPreset;// ChainInit_CreateRecordResource 시점에서 resolve 된 것이 들어감.
	EVdjmRecordQualityTiers mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
	
	bool mHasResolved = false;
};


DECLARE_DELEGATE_RetVal(VdjmResult,FVdjmRecordStartEvent);

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
						UVdjmRecordEncoderUnit 		
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVdjmOnEncoderPacketReady, const TArray<uint8>&, PacketData, int64, TimeStamp);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVdjmOnFrameRecorded, int64, CurrentTimeStampMs);

UCLASS(Blueprintable)
class VDJMRECORDER_API UVdjmRecordEncoderUnit : public UVdjmRecordUnit
{
	GENERATED_BODY()
public:
	
	virtual void EncodeFrameRDGPass(FRDGBuilder& graphBuilder,const FTextureRHIRef srcTex,	const double timeStampSec) PURE_VIRTUAL(UVdjmRecordEncoderUnit::EncodeFrameRDGPass, );
	virtual void StopEncoding() PURE_VIRTUAL(UVdjmRecordEncoderUnit::StopEncoding, );

	UPROPERTY(BlueprintAssignable)
	FVdjmOnEncoderPacketReady OnEncoderPacketReady;
	
protected:
	TWeakObjectPtr<UVdjmRecordResource> LinkedRecordResource;
};

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓		class  UVdjmRecordFileSaver : public UObject		↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/




// AVdjmRecordBridgeActor 는 VdjmRecordBridgeActor.h 로 분리됨.


UCLASS()
class VDJMRECORDER_API UVdjmRecorderCore : public UObject
{
	GENERATED_BODY()
};
