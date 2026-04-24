// Fill out your copyright notice in the Description page of Project Settings.

#include "VdjmRecordBridgeActor.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "VdjmRecorderCore.h"
#include "VdjmRecorderWorldContextSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Slate/SceneViewport.h"

FSoftObjectPath AVdjmRecordBridgeActor::RecordEnvDataAssetPath(TEXT("/Script/VdjmRecorder.VdjmRecordEnvDataAsset'/Game/Temp/VdjmTestDataAsset.VdjmTestDataAsset'"));

UVdjmRecordEnvDataAsset* AVdjmRecordBridgeActor::TryGetRecordEnvConfigure()
{
	/*
	 *  /Script/VdjmRecorder.VdjmRecordEnvDataAsset'/Game/Temp/VdjmTestDataAsset.VdjmTestDataAsset'
	 *  /Script/VdjmRecorder.VdjmRecordEnvDataAsset'/Game/Temp/VdjmTestDataAsset.VdjmTestDataAsset'
	 *  
	 *
	 * /Script/VdjmRecorder.VdjmRecordEnvDataAsset'/VdjmMobileUi/Record/Bp_VdjmRecordConfigDataAsset.Bp_VdjmRecordConfigDataAsset'
	 */
	return FVdjmFunctionLibraryHelper::TryGetRecordConfigureDataAsset<UVdjmRecordEnvDataAsset>(RecordEnvDataAssetPath);
}

void AVdjmRecordBridgeActor::SetRecordEnvDataAssetPath(const FSoftObjectPath& InDataAssetPath)
{
	if (InDataAssetPath.IsValid())
	{
		RecordEnvDataAssetPath = InDataAssetPath;
	}
}

FSoftObjectPath AVdjmRecordBridgeActor::GetRecordEnvDataAssetPath()
{
	return RecordEnvDataAssetPath;
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
	UnregisterWorldContextBridgeEntry();
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
	else if (not mRecordPipeline->DbcIsValidPipelineInit())
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
					if (not mRecordResource->LinkedResolver.IsValid())
					{
						UE_LOG(LogTemp, Warning, TEXT("StartRecording - 5			       not mRecordResource->LinkedResolver.IsValid() "));
					}
				}
			}
			if (mRecordPipeline == nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("StartRecording - 3           mRecordPipeline == nullptr"));
			} 
			if (mRecordPipeline != nullptr && not mRecordPipeline->DbcIsValidPipelineInit())
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

void AVdjmRecordBridgeActor::StartRecordBridgeActor()
{
	OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitializeWorldParts);
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
		if (mRecordResource != nullptr && !mRecordResource->RefreshResolvedRuntimeConfigFromResolver())
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("StartRecording - Failed to refresh resource runtime config from resolver."));
			bIsRecording = false;
			return;
		}
		
		const FVdjmEncoderInitRequestVideo* videoConfig = mEnvResolver->TryGetResolvedVideoConfig();
		const double minFrameRate = mEnvResolver->GetResolvedGlobalRules().MinFrameRate;
		const double maxDurationSecond = mEnvResolver->GetResolvedGlobalRules().MaxRecordDurationSeconds;
		const FVdjmEncoderInitRequestAudio* audioConfig = mEnvResolver->TryGetResolvedAudioConfig();

		if (videoConfig == nullptr || audioConfig == nullptr )
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
	result &= (mEnvResolver != nullptr && mEnvResolver->DbcIsValidEnvResolverInit());
	result &= (mRecordResource != nullptr && mRecordResource->DbcIsValidResourceInit());
	result &= (mRecordPipeline != nullptr && mRecordPipeline->DbcIsValidPipelineInit());
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

void AVdjmRecordBridgeActor::SetRequestedQualityTier(EVdjmRecordQualityTiers InQualityTier)
{
	if (InQualityTier == EVdjmRecordQualityTiers::EUndefined)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("SetRequestedQualityTier - Undefined tier is ignored."));
		return;
	}

	if (mCurrentQualityTier == InQualityTier)
	{
		return;
	}

	mCurrentQualityTier = InQualityTier;
	SelectedBitrateType = InQualityTier;
	OnRequestedQualityTierChanged.Broadcast(this, mCurrentQualityTier);
}

void AVdjmRecordBridgeActor::ClearRequestedQualityTier()
{
	if (mCurrentQualityTier == EVdjmRecordQualityTiers::EUndefined)
	{
		return;
	}

	mCurrentQualityTier = EVdjmRecordQualityTiers::EUndefined;
	SelectedBitrateType = EVdjmRecordQualityTiers::EDefault;
	OnRequestedQualityTierChanged.Broadcast(this, mCurrentQualityTier);
}

void AVdjmRecordBridgeActor::SetRequestedFrameRate(int32 InFrameRate)
{
	mRequestedFrameRate = FMath::Max(0, InFrameRate);
}

void AVdjmRecordBridgeActor::ClearRequestedFrameRate()
{
	mRequestedFrameRate = 0;
}

void AVdjmRecordBridgeActor::SetRequestedBitrate(int32 InBitrate)
{
	mRequestedBitrate = FMath::Max(0, InBitrate);
}

void AVdjmRecordBridgeActor::ClearRequestedBitrate()
{
	mRequestedBitrate = 0;
}

bool AVdjmRecordBridgeActor::RefreshResolvedOptionsFromRequest(FString& OutErrorReason)
{
	OutErrorReason.Reset();

	if (bIsRecording)
	{
		OutErrorReason = TEXT("Cannot refresh resolved options while recording is running.");
		return false;
	}

	if (mEnvResolver == nullptr)
	{
		return true;
	}

	if (mRecordConfigureDataAsset == nullptr)
	{
		OutErrorReason = TEXT("Record configure asset is not ready.");
		return false;
	}

	const FVdjmRecordEnvPlatformPreset* envPreset = mRecordConfigureDataAsset->GetPlatformPreset(GetTargetPlatform());
	if (envPreset == nullptr)
	{
		OutErrorReason = TEXT("Platform preset is not available.");
		return false;
	}

	if (!mEnvResolver->ResolveEnvPlatform(envPreset))
	{
		OutErrorReason = TEXT("Resolver failed to apply requested quality tier.");
		return false;
	}

	if (mRecordResource != nullptr && !mRecordResource->RefreshResolvedRuntimeConfigFromResolver())
	{
		OutErrorReason = TEXT("Failed to refresh resource runtime config from resolver.");
		return false;
	}

	return true;
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
	RegisterWorldContextBridgeEntry();
}

void AVdjmRecordBridgeActor::RegisterWorldContextBridgeEntry()
{
	if (UVdjmRecorderWorldContextSubsystem* WorldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this))
	{
		WorldContextSubsystem->RegisterBridgeContext(this);
	}
}

void AVdjmRecordBridgeActor::UnregisterWorldContextBridgeEntry()
{
	UVdjmRecorderWorldContextSubsystem* WorldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this);
	if (WorldContextSubsystem == nullptr)
	{
		return;
	}

	UVdjmRecorderBridgeWorldContextEntry* BridgeEntry = WorldContextSubsystem->GetBridgeContextEntry();
	if (BridgeEntry == nullptr || BridgeEntry->GetBridgeActor() != this)
	{
		return;
	}

	WorldContextSubsystem->UnregisterContext(UVdjmRecorderWorldContextSubsystem::GetBridgeContextKey(), BridgeEntry);
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
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_InitializeCurrentEnvironment - Environment resolver initialized successfully."));
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
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordResource - Platform preset found for target platform."));
	//	해당 프리셋에 맞는 현재 티어를 가져온다.
	const FVdjmEncoderInitRequest* initPreset = envPreset->GetEncoderInitRequest(mCurrentQualityTier);
	if (initPreset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - No encoder init preset found for current quality tier. Cannot create record resource without encoder init preset."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitErrorEnd);
		return;
	}
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordResource - Encoder init preset found for current quality tier."));
	if (mRecordResource != nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("ChainInit_CreateRecordResource - Record resource already exists. Releasing existing resource before creating new one."));
		mRecordResource->ReleaseResources();
		mRecordResource = nullptr;
	}
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordResource - Attempting to create record resource with environment resolver."));
	//	resolver 통해서 record resource 생성 시도, 심지어 이 단계에서 데이터 에셋의 프리셋이 mResolvedPreset 로 변화함.
	mRecordResource = mEnvResolver->CreateResolvedRecordResource(envPreset);
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - Failed to create record resource from environment resolver."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	
	if (not mRecordResource->DbcIsInitializedResource())
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
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordResource - Record resource created successfully with environment resolver."));
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
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_PostResourceInitResolve - Environment resolver is valid after initialization."));
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_PostResourceInitResolve - Record resource is null after initialization."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_PostResourceInitResolve - Record resource is valid after initialization."));
	if (not mEnvResolver->IsValidPreset() ||not mRecordResource->DbcIsInitializedResource())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_PostResourceInitResolve - Environment resolver preset is not valid or record resource is not properly initialized after creation."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_PostResourceInitResolve - Environment resolver preset is valid and record resource is properly initialized."));
	if (not mRecordResource->IsLazyPostInitializeCheck())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordResource - Record resource failed extended initialization with environment resolver."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_PostResourceInitResolve - Record resource passed extended initialization checks with environment resolver."));
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
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordPipeline - Environment resolver is valid for creating record pipeline."));
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Record resource is null. Cannot create record pipeline without valid record resource."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordPipeline - Record resource is valid for creating record pipeline."));
	//	여기에서 pipeline 을 바인딩한다.
	TSubclassOf<UVdjmRecordUnitPipeline> pipelineCls = mEnvResolver->TryGetResolvedPipelineClass();
	if (pipelineCls == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Environment resolver did not provide a valid pipeline class. Cannot create record pipeline without valid pipeline class."));
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordPipeline - Resolved pipeline class from environment resolver: %s"), *pipelineCls->GetName());
	if (not BindingRecordPipeline(pipelineCls,mRecordResource))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("ChainInit_CreateRecordPipeline - Failed to bind record pipeline with class %s."), *pipelineCls->GetName());
		OnTryChainInitNext(EVdjmRecordBridgeInitStep::EInitError);
		return;
	}
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("ChainInit_CreateRecordPipeline - Record pipeline bound successfully with class %s."), *pipelineCls->GetName());
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
	/*
	 * 초기화 체인이 모두 완료된 시점에서, 레코드 시작이 가능한 상태인지 체크하는 함수. 초기화 과정에서 필요한 요소들이 모두 유효한지 검증한다.
	 * 초기화 체인의 마지막 단계에서 이 함수를 호출하여, 모든 요소들이 유효한지 최종적으로 검증한다. 이 함수가 true를 반환해야만 레코드 시작이 가능하다.
	 */
	if (mRecordConfigureDataAsset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mRecordConfigureDataAsset == nullptr"));
		return false;
	}
	if (not mRecordConfigureDataAsset->DbcPlatformPresetValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mRecordConfigureDataAsset->DbcPlatformPresetValid() == false"));
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
	
	if (mEnvResolver == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mEnvResolver == nullptr"));
		return false;
	}
	if (not mEnvResolver->DbcIsValidEnvResolverInit())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mEnvResolver->DbcIsValidEnvResolverInit() == false"));
		return false;
	}
	
	if (mRecordPipeline == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mRecordPipeline == nullptr"));
		return false;
	}
	if (not mRecordPipeline->DbcIsValidPipelineInit())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mRecordPipeline->DbcIsValidPipelineInit() == false"));
		return false;
	}
	
	if (mRecordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mRecordResource == nullptr"));
		return false;
	}
	if (not mRecordResource->DbcIsValidResourceInit())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AVdjmRecordBridgeActor::DbcValidInitializeComplete - mRecordResource->DbcIsValidResourceInit() == false"));
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
	else if (!mRecordPipeline->DbcIsValidPipelineInit())
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
