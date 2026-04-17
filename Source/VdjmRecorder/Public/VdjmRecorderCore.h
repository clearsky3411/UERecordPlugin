// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

class UVdjmRecordUnit;
class UVdjmRecordResource;
class UVdjmRecordDepreDataAsset;
class AVdjmRecordBridgeActor;
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
	UPROPERTY()
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
	bool UpdateFinalFilePathFromResolver();
	virtual void ResetResource();
	virtual void ReleaseResources();
	
	virtual FTextureRHIRef GetCurrPooledTextureRHI()PURE_VIRTUAL(UVdjmRecordResource::GetCurrPooledTextureRHI, return nullptr; );
	virtual FTextureRHIRef GetNextPooledTextureRHI()PURE_VIRTUAL(UVdjmRecordResource::GetNextPooledTextureRHI, return nullptr; );
	
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
private:
	bool ResolveEnvPlatform(const FVdjmRecordEnvPlatformPreset* presetData);
	bool ResolvedFinalFilePath(const FString& customFileName);
	
	
	FVdjmRecordGlobalRules mGlobalRules;
	FVdjmRecordEnvPlatformPreset mResolvedPreset;// ChainInit_CreateRecordResource 시점에서 resolve 된 것이 들어감.
	EVdjmRecordQualityTiers mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
	
	bool mHasResolved = false;
};


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓		class  UVdjmRecordFileSaver : public UObject		↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVdjmRecordInitEvent,AVdjmRecordBridgeActor*, bridgeActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVdjmRecordInitErrorEvent,AVdjmRecordBridgeActor*, bridgeActor,EVdjmRecordBridgeInitStep,prevStep,int32,retryStep);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVdjmRecordEvent,UVdjmRecordResource*, recordData);
DECLARE_MULTICAST_DELEGATE_OneParam(FVdjmRecordInnerEvent,UVdjmRecordResource*);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVdjmRecordTickEvent,UVdjmRecordResource*, recordResource, float, deltaTime);
DECLARE_MULTICAST_DELEGATE_TwoParams(FVdjmRecordTickInnerEvent,UVdjmRecordResource*,  float);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVdjmRecordBridgeActorChainInitEvent, AVdjmRecordBridgeActor*, bridgeActor, EVdjmRecordBridgeInitStep, prevInitstep, EVdjmRecordBridgeInitStep, currentInitStep);

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




UCLASS(Blueprintable)
class VDJMRECORDER_API AVdjmRecordBridgeActor : public AActor
{
	GENERATED_BODY()

public:
	
	//static UVdjmRecordDepreDataAsset* TryGetRecordConfigure();

	static UVdjmRecordEnvDataAsset* TryGetRecordEnvConfigure();
	static AVdjmRecordBridgeActor* TryGetRecordBridgeActor(UWorld* worldContext = nullptr);
	
	AVdjmRecordBridgeActor();
	virtual void BeginDestroy() override;
	virtual void Tick(float DeltaSeconds) override;
	void PrintLogErrors();
	

	UFUNCTION()
	void OnBindSlateBackBufferReadyToPresentEvent();
	UFUNCTION()
	void OnStopSlateBackBufferReadyToPresentEvent();
	
	UFUNCTION(BlueprintCallable)
	void StartRecording();
	
	UFUNCTION(BlueprintCallable)
	void StopRecording();

	UFUNCTION(BlueprintCallable)
	FString GetCurrentFileName() const
	{
		return mCurrentCustomFileName;
	}
	UFUNCTION(BlueprintCallable)
	void SetCurrentFileName(const FString& newFileName)
	{
		mCurrentCustomFileName = newFileName;
	}
	
	UFUNCTION(BlueprintCallable)
	void CriticalErrorStop(const FString& errorMessage)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Critical Error: %s"), *errorMessage);
		StopRecordingInternal();
	}
	
	/**
	* @param SlateWindow : 녹화 대상이 되는 윈도우. 보통은 게임 뷰포트가 될 것임.
	* @param  BackBuffer : 녹화 대상 윈도우의 백버퍼 텍스처. 이 텍스처를 기반으로 녹화 유닛들이 필요한 작업들을 수행하게 될 것임. 즉 이게 inputTexture
	*/
	void OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTextureRHIRef& BackBuffer);

	/*	↓↓↓[	Get 	]↓↓↓	*/
	
	
	/*	↓↓↓[	Setters for mCurrentRecordData	]↓↓↓	*/
	
	bool DbcValidConfigureDataAsset() const
	{
		return mRecordConfigureDataAsset != nullptr;
	}

	bool DbcValidRecordPreset() const
	{
		return IsValid(mEnvResolver) && mEnvResolver->IsValidPreset();
	}
	bool DbcValidRecordResource() const
	{
		return mRecordResource != nullptr && mRecordResource->DbcIsValidResourceInit();
	}
	bool DbcValidRecordPipeline() const
	{
		return DbcValidRecordResource()&& mRecordPipeline != nullptr && mRecordPipeline->DbcIsValidPipelineInit();
	}
	bool DbcRecordingPossible()  const
	{
		return DbcValidRecordPipeline() && DbcValidRecordPreset();
	}
	
	bool DbcRecordStartable() const
	{
		return bValidateInitializeComplete && DbcRecordingPossible() && not bIsRecording;
	}

	bool DbcValidInitializeComplete() const;
	
	bool DbcRecordStartableFull() const;
	FVdjmRecordGlobalRules GetGlobalRules() const
    {
        return mGlobalRules;
    }
	bool IsCompleteChainInit() const;

	void StopRecordingInternal();
	UFUNCTION(BlueprintCallable)
	void OnResourceReadyForPostInit(UVdjmRecordResource* resource);
	
	//	Platform Branch Function
	static EVdjmRecordEnvPlatform GetTargetPlatform();
	
	
	
	UVdjmRecordEnvDataAsset* GetRecordEnvConfigureDataAsset()
	{
		return mRecordConfigureDataAsset;
	}
	
	FVdjmRecordEnvPlatformInfo* GetCurrentPlatformInfo() const
	{
		if (DbcValidConfigureDataAsset())
		{
			return mRecordConfigureDataAsset->GetPlatformInfo(GetTargetPlatform());
		}
		return nullptr;
	}
	FVdjmRecordGlobalRules GetCurrentGlobalRules() const
	{
		return mEnvResolver ? mEnvResolver->GetResolvedGlobalRules() : FVdjmRecordGlobalRules();
	}
	UVdjmRecordResource* GetRecordResource()
	{
		return mRecordResource;
	}
	void BroadcastRecordPrevStart()
	{
		OnRecordPrevStart.Broadcast(mRecordResource);
		OnRecordPrevStartInner.Broadcast(mRecordResource);
	}
	void BroadcastRecordStart()
	{
		OnRecordStarted.Broadcast(mRecordResource);
		OnRecordStartedInner.Broadcast(mRecordResource);
	}
	void BroadcastRecordTick(float deltaTime)
	{
		OnRecordTick.Broadcast(mRecordResource, deltaTime);
		OnRecordTickInner.Broadcast(mRecordResource, deltaTime);
	}
	//	TODO(20260410 env control) - 여길 채워야함.
	bool EvaluateInitRequest(const FVdjmEncoderInitRequest* initPreset);

	
	/*	↓↓↓[			Delegators			]↓↓↓	*/
	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordEvent OnRecordPrevStart;
	FVdjmRecordInnerEvent OnRecordPrevStartInner;
	
	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordEvent OnRecordStarted;
	FVdjmRecordInnerEvent OnRecordStartedInner;
	
	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordTickEvent OnRecordTick;
	FVdjmRecordTickInnerEvent OnRecordTickInner;
	
	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordEvent OnRecordStopped;
	FVdjmRecordInnerEvent OnRecordStoppedInner;

	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordEvent OnRecordError;
	
	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordBridgeActorChainInitEvent OnChainInitEvent;
	
	FVdjmRecordStartEvent OnRecordStartRetValEvent;
	
	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordInitEvent OnInitStartEvent;
	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordInitEvent OnInitCompleteEvent;
	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordInitEvent OnInitErrorEndEvent;
	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordInitErrorEvent OnInitErrorEvent;
	
	FSceneViewport* mTargetViewport;
	TWeakObjectPtr<APlayerController> mTargetPlayerController;

	UPROPERTY(EditAnywhere)
	EVdjmRecordQualityTiers SelectedBitrateType = EVdjmRecordQualityTiers::EDefault;

	bool TryResolveViewportSize(FIntPoint& outSize) const;
	static const TCHAR* GetInitStepName(EVdjmRecordBridgeInitStep step);

	
protected:
	virtual void BeginPlay() override;
	
	bool CheckChainCount(const FString& errorMsg);
	void OnTryChainInitNext(EVdjmRecordBridgeInitStep nextStep);
	void ChainInit_InitializeWorldParts();	//	mRecordConfigureDataAsset 을 검증. mEnvResolver 생성 및 초기화
	void ChainInit_InitializeCurrentEnvironment();	//	mRecordConfigureDataAsset 를 통해서 FVdjmRecordEnvPlatformPreset 와 FVdjmEncoderInitRequest 검증, mEnvResolver->CreateResolvedRecordResource(envPreset) 로 mRecordResource 생성 시도
	void ChainInit_CreateRecordResource();
	void ChainInit_PostResourceInitResolve();
	void ChainInit_CreateRecordPipeline();
	void ChainInit_FinalizeInitialization();
	void UnBindBackBufferReady(FSlateApplication& slateApp);
	
	bool BindingRecordPipeline(TSubclassOf<UVdjmRecordUnitPipeline> pipelineClass,UVdjmRecordResource* recordResource);
	void UnBindingRecordPipeline();
	
	UPROPERTY()
	int32 mChainTryInitCount = 8;

	UPROPERTY()
	FTimerHandle mChainInitTimerHandle;
	UPROPERTY()
	FTimerHandle mRecordStartTimerHandle;
	
	UPROPERTY()
	EVdjmRecordBridgeInitStep mCurrentInitStep = EVdjmRecordBridgeInitStep::EInitializeStart;
	EVdjmRecordBridgeInitStep mRetryStep = EVdjmRecordBridgeInitStep::EInitializeStart;
	
	UPROPERTY()
	FVdjmRecordGlobalRules mGlobalRules;
	UPROPERTY()
	TObjectPtr<UVdjmRecordResource> mRecordResource;
	UPROPERTY()
	TObjectPtr<UVdjmRecordUnitPipeline> mRecordPipeline;

	FDelegateHandle mBackBufferDelegateHandle;
	//FVdjmRecordUnitParamContext mRecordUnitContext;
	//FVdjmRecordUnitParamPayload mRecordUnitPayload;
	FDelegateHandle mOnResourceTexturePoolInitializedHandle;
	
	bool bIsRecording = false;
	bool bValidateInitializeComplete = false;

	UPROPERTY()
	TObjectPtr<UVdjmRecordEnvDataAsset> mRecordConfigureDataAsset;
	

	UPROPERTY()
	TObjectPtr<USceneComponent> mRootScene;
	
	
	UPROPERTY()
	double mRecordEndTime = 0.0;
	double mNextFrameTime = 0.0;
	double mFrameInterval = 0.0;
	
	int32 mRecordedFrameCount = 0;
	
	//	TODO(20260410 env control) - 
	EVdjmRecordQualityTiers mCurrentQualityTier = EVdjmRecordQualityTiers::EDefault;	//	추후에 옵션을 바꿀 수 있는 인터페이스에 노출될 놈임.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Record|Output", meta=(AllowPrivateAccess="true"))
	FString mCurrentCustomFileName;
	UPROPERTY()
	TObjectPtr<UVdjmRecordEnvResolver> mEnvResolver;
};


UCLASS()
class VDJMRECORDER_API UVdjmRecorderCore : public UObject
{
	GENERATED_BODY()
};
