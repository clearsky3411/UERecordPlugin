#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VdjmRecorderCore.h"
#include "VdjmRecordBridgeActor.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVdjmRecordInitEvent,AVdjmRecordBridgeActor*, bridgeActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVdjmRecordInitErrorEvent,AVdjmRecordBridgeActor*, bridgeActor,EVdjmRecordBridgeInitStep,prevStep,int32,retryStep);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVdjmRecordEvent,UVdjmRecordResource*, recordData);
DECLARE_MULTICAST_DELEGATE_OneParam(FVdjmRecordInnerEvent,UVdjmRecordResource*);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVdjmRecordTickEvent,UVdjmRecordResource*, recordResource, float, deltaTime);
DECLARE_MULTICAST_DELEGATE_TwoParams(FVdjmRecordTickInnerEvent,UVdjmRecordResource*,  float);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVdjmRecordBridgeActorChainInitEvent, AVdjmRecordBridgeActor*, bridgeActor, EVdjmRecordBridgeInitStep, prevInitstep, EVdjmRecordBridgeInitStep, currentInitStep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVdjmRecordQualityTierChangedEvent, AVdjmRecordBridgeActor*, bridgeActor, EVdjmRecordQualityTiers, newQualityTier);

UCLASS(Blueprintable)
class VDJMRECORDER_API AVdjmRecordBridgeActor : public AActor
{
	GENERATED_BODY()

public:
	
	//static UVdjmRecordDepreDataAsset* TryGetRecordConfigure();

	static UVdjmRecordEnvDataAsset* TryGetRecordEnvConfigure();
	UFUNCTION(BlueprintCallable, Category = "Record|Config")
	static void SetRecordEnvDataAssetPath(const FSoftObjectPath& InDataAssetPath);
	UFUNCTION(BlueprintPure, Category = "Record|Config")
	static FSoftObjectPath GetRecordEnvDataAssetPath();
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
	void ClearCurrentFileName()
	{
		mCurrentCustomFileName.Reset();
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

	UPROPERTY(BlueprintAssignable,EditAnywhere)
	FVdjmRecordQualityTierChangedEvent OnRequestedQualityTierChanged;
	
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

	UFUNCTION(BlueprintCallable, Category="Record|Option")
	void SetRequestedQualityTier(EVdjmRecordQualityTiers InQualityTier);

	UFUNCTION(BlueprintPure, Category="Record|Option")
	EVdjmRecordQualityTiers GetRequestedQualityTier() const
	{
		return mCurrentQualityTier;
	}

	UFUNCTION(BlueprintCallable, Category="Record|Option")
	void ClearRequestedQualityTier();

	UFUNCTION(BlueprintCallable, Category="Record|Option")
	void SetRequestedFrameRate(int32 InFrameRate);

	UFUNCTION(BlueprintCallable, Category="Record|Option")
	void ClearRequestedFrameRate();

	UFUNCTION(BlueprintPure, Category="Record|Option")
	int32 GetRequestedFrameRate() const
	{
		return mRequestedFrameRate;
	}

	UFUNCTION(BlueprintCallable, Category="Record|Option")
	void SetRequestedBitrate(int32 InBitrate);

	UFUNCTION(BlueprintCallable, Category="Record|Option")
	void ClearRequestedBitrate();

	UFUNCTION(BlueprintPure, Category="Record|Option")
	int32 GetRequestedBitrate() const
	{
		return mRequestedBitrate;
	}

	UFUNCTION(BlueprintPure, Category="Record|State")
	bool IsRecording() const
	{
		return bIsRecording;
	}

	UFUNCTION(BlueprintPure, Category="Record|State")
	EVdjmRecordBridgeInitStep GetCurrentInitStep() const
	{
		return mCurrentInitStep;
	}

	UFUNCTION(BlueprintCallable, Category="Record|Option")
	bool RefreshResolvedOptionsFromRequest(FString& OutErrorReason);

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

	static FSoftObjectPath RecordEnvDataAssetPath;
	

	UPROPERTY()
	TObjectPtr<USceneComponent> mRootScene;
	
	
	UPROPERTY()
	double mRecordEndTime = 0.0;
	double mNextFrameTime = 0.0;
	double mFrameInterval = 0.0;
	
	int32 mRecordedFrameCount = 0;
	
	//	TODO(20260410 env control) - 
	EVdjmRecordQualityTiers mCurrentQualityTier = EVdjmRecordQualityTiers::EUndefined;	//	추후에 옵션을 바꿀 수 있는 인터페이스에 노출될 놈임.
	int32 mRequestedFrameRate = 0;
	int32 mRequestedBitrate = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Record|Output", meta=(AllowPrivateAccess="true"))
	FString mCurrentCustomFileName;
	UPROPERTY()
	TObjectPtr<UVdjmRecordEnvResolver> mEnvResolver;
};
