// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmRecordTypes.h"

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓			LOG Categories for Vdjm Recorder				↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
DEFINE_LOG_CATEGORY(LogVdjmRecorderCore)

void UVdjmRecordEventSession::InitializeSession(AActor* InOwnerActor,EVdjmRecordEventSessionCallbackMask InCallbackMask, float InSessionIntervalSeconds)
{
	if (!IsValid(InOwnerActor) || !IsValid(InOwnerActor->GetWorld()))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEventSession::InitializeSession - Invalid owner actor or world context."));
		return;
	}

	mOwnerActor = InOwnerActor;
	mCallbackMask = InCallbackMask;
	mOwnerWorld = InOwnerActor->GetWorld();
	mSessionIntervalSeconds = InSessionIntervalSeconds > 0.0f ? InSessionIntervalSeconds : 0.33333f;

	TrySetSessionState(EVdjmRecordEventSessionState::EInitialized);

	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordEventSession::InitializeSession - Session initialized with interval %.2f seconds."), mSessionIntervalSeconds);
}

void UVdjmRecordEventSession::StartSession()
{
	if (not mOwnerActor.IsValid() ||not mOwnerWorld.IsValid())
	{
		TrySetSessionState(EVdjmRecordEventSessionState::EError);
		return;
	}
	AActor* ownerActor = mOwnerActor.Get();
	UWorld* worldContext = mOwnerWorld.Get();
	if (mCurrentState == EVdjmRecordEventSessionState::ERunning ||
		mCurrentState == EVdjmRecordEventSessionState::EStopping)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordEventSession::StartSession - Already running or stopping."));
		return;
	}

	ClearSessionTimers();

	mSessionStartTime = worldContext->GetTimeSeconds();
	mSessionFrameCount = 0;
	mReservedNextState = EVdjmRecordEventSessionState::EUndefined;

	TrySetSessionState(EVdjmRecordEventSessionState::EPrepare);

	const VdjmResult StartResult =	ExecuteControlCallback(OnStartSessionCallback, EVdjmRecordEventSessionCallbackMask::EStartControl);

	if (VDJM_FAILED(StartResult))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEventSession::StartSession - Start control callback failed. Result=0x%08X"), StartResult);
		mReservedNextState = EVdjmRecordEventSessionState::EError;
		TrySetSessionState(EVdjmRecordEventSessionState::EError);
		return;
	}

	TrySetSessionState(EVdjmRecordEventSessionState::EPrepared);
	TrySetSessionState(EVdjmRecordEventSessionState::ERunning);

	worldContext->GetTimerManager().SetTimer(
		mSessionTimerHandle,
		this,
		&UVdjmRecordEventSession::RunningSession,
		mSessionIntervalSeconds,
		true
	);

	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordEventSession::StartSession - Session started."));
}

void UVdjmRecordEventSession::RunningSession()
{
	AActor* ownerActor = mOwnerActor.Get();
	if (not mOwnerActor.IsValid() ||not mOwnerWorld.IsValid())
	{
		mReservedNextState = EVdjmRecordEventSessionState::EError;
		StopSession();
		return;
	}
	UWorld* worldContext = mOwnerWorld.Get();

	if (mCurrentState != EVdjmRecordEventSessionState::ERunning)
	{
		return;
	}

	++mSessionFrameCount;

	const float CurrentTime = worldContext->GetTimeSeconds();
	const float ElapsedTime = CurrentTime - mSessionStartTime;

	// auto stop 예약
	if (mSessionEndTime > 0.0f && CurrentTime >= mSessionEndTime)
	{
		mReservedNextState = EVdjmRecordEventSessionState::EStopped;
	}

	const VdjmResult RunningResult = ExecuteControlCallback(OnRunningSessionCallback, EVdjmRecordEventSessionCallbackMask::ERunningControl);

	if (VDJM_FAILED(RunningResult))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEventSession::RunningSession - Running control callback failed. Result=0x%08X"), RunningResult);
		mReservedNextState = EVdjmRecordEventSessionState::EError;
	}

	if (IsCallbackEnabled(EVdjmRecordEventSessionCallbackMask::ERunningObserve))
	{
		OnRunningSessionObservedCallback.Broadcast(this, ElapsedTime, mSessionFrameCount);
	}

	if (mReservedNextState == EVdjmRecordEventSessionState::EStopped ||
		mReservedNextState == EVdjmRecordEventSessionState::EError)
	{
		StopSession();
	}
}

void UVdjmRecordEventSession::StopSession()
{
	if (mCurrentState == EVdjmRecordEventSessionState::EStopped ||
		mCurrentState == EVdjmRecordEventSessionState::ETerminated)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordEventSession::StopSession - Already stopped or terminated."));
		return;
	}

	const EVdjmRecordEventSessionState FinalState =
		(mReservedNextState == EVdjmRecordEventSessionState::EError)
		? EVdjmRecordEventSessionState::EError
		: EVdjmRecordEventSessionState::EStopped;

	ClearSessionTimers();

	if (mCurrentState != EVdjmRecordEventSessionState::EError)
	{
		TrySetSessionState(EVdjmRecordEventSessionState::EStopping);
	}

	const VdjmResult StopResult = ExecuteControlCallback(OnStopSessionCallback, EVdjmRecordEventSessionCallbackMask::EStopControl);

	if (VDJM_FAILED(StopResult))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEventSession::StopSession - Stop control callback failed. Result=0x%08X"), StopResult);
		TrySetSessionState(EVdjmRecordEventSessionState::EError);
		mReservedNextState = EVdjmRecordEventSessionState::EUndefined;
		return;
	}

	TrySetSessionState(FinalState);
	mReservedNextState = EVdjmRecordEventSessionState::EUndefined;

	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordEventSession::StopSession - Session stopped. FinalState=%d"), static_cast<int32>(FinalState));
}

bool UVdjmRecordEventSession::TrySetSessionState(EVdjmRecordEventSessionState InNewState)
{
	if (mCurrentState == InNewState)
	{
		return false;
	}

	mPrevState = mCurrentState;
	mCurrentState = InNewState;

	UE_LOG(
		LogVdjmRecorderCore,
		Log,
		TEXT("UVdjmRecordEventSession::TrySetSessionState - Prev=%d Current=%d"),
		static_cast<int32>(mPrevState),
		static_cast<int32>(mCurrentState)
	);

	if (IsCallbackEnabled(EVdjmRecordEventSessionCallbackMask::EStateChangedObserve))
	{
		OnSessionStateChangedCallback.Broadcast(mPrevState, mCurrentState);
	}

	return true;
}

bool UVdjmRecordEventSession::IsCallbackEnabled(EVdjmRecordEventSessionCallbackMask InMask) const
{
	return EnumHasAnyFlags(mCallbackMask, InMask);
}

VdjmResult UVdjmRecordEventSession::ExecuteControlCallback(FVdjmRecordEventSessionControlDelegate& InDelegate,
	EVdjmRecordEventSessionCallbackMask InMask)
{
	if (!IsCallbackEnabled(InMask))
	{
		return VdjmResults::Ok;
	}

	if (!InDelegate.IsBound())
	{
		return VdjmResults::Ok;
	}

	return InDelegate.Execute(this);
}

void UVdjmRecordEventSession::ClearSessionTimers()
{
	UWorld* WorldContext = mOwnerWorld.Get();
	if (!IsValid(WorldContext))
	{
		if (AActor* OwnerActor = mOwnerActor.Get())
		{
			WorldContext = OwnerActor->GetWorld();
		}
	}

	if (!IsValid(WorldContext))
	{
		return;
	}

	FTimerManager& TimerManager = WorldContext->GetTimerManager();
	TimerManager.ClearTimer(mSessionTimerHandle);
	TimerManager.ClearTimer(mSessionObserverTimerHandle);
}

TOptional<FVdjmEncoderInitRequestVideo> FVdjmEncoderInitRequestVideo::CreateDefaultForCurrentPlatform()
{
#if PLATFORM_WINDOWS
	return FVdjmEncoderInitRequestVideo{
		.bResolutionFitToDisplay = true,
		.Width = 1280,
		.Height = 720,
		.FrameRate = 30,
		.Bitrate = 5000000,
		.KeyframeInterval = 2,
		.MimeType = TEXT("video/mp4")
	};
#elif PLATFORM_ANDROID || defined(__RESHARPER__) 
	// Android에 맞는 기본 설정을 반환하도록 구현 필요
	return FVdjmEncoderInitRequestVideo{
		.bResolutionFitToDisplay = true,
		.Width = 1280,
		.Height = 720,
		.FrameRate = 30,
		.Bitrate = 5000000,
		.KeyframeInterval = 2,
		.MimeType = TEXT("video/avc") // H.264
	};
#else
	return TOptional<FVdjmEncoderInitRequestVideo>();
#endif
}

bool FVdjmEncoderInitRequestVideo::EvaluateValidation() const
{
	return Width > 0
		&& Height > 0
		&& FrameRate > 0
		&& Bitrate > 0
		&& KeyframeInterval >= 0
		&& !MimeType.IsEmpty();
}
TOptional<FVdjmEncoderInitRequestAudio> FVdjmEncoderInitRequestAudio::CreateDefaultForCurrentPlatform()
{
#if PLATFORM_WINDOWS
	return FVdjmEncoderInitRequestAudio{
		.bEnableInternalAudioCapture = true,
		.AudioMimeType = TEXT("audio/mp4a-latm"), // AAC
		.SampleRate = 44100,
		.ChannelCount = 2,
		.Bitrate = 128000,
		.AacProfile = 2, // LC-AAC
		.SourceSubMixName = TEXT("Master")
	};
#elif PLATFORM_ANDROID || defined(__RESHARPER__)
	// Android에 맞는 기본 설정을 반환하도록 구현 필요
	return FVdjmEncoderInitRequestAudio{
		.bEnableInternalAudioCapture = true,
		.AudioMimeType = TEXT("audio/mp4a-latm"), // AAC
		.SampleRate = 44100,
		.ChannelCount = 2,
		.Bitrate = 128000,
		.AacProfile = 2, // LC-AAC
		.SourceSubMixName = TEXT("Master")
	};
#else
	return TOptional<FVdjmEncoderInitRequestAudio>();
#endif
}

bool FVdjmEncoderInitRequestAudio::EvaluateValidation() const
{
	if (!bEnableInternalAudioCapture)
	{
		return true;
	}
	return !AudioMimeType.IsEmpty()
		&& SampleRate > 0
		&& ChannelCount > 0
		&& Bitrate > 0
		&& AacProfile > 0
		&& !SourceSubMixName.IsNone();
}
TOptional<FVdjmEncoderInitRequestOutput> FVdjmEncoderInitRequestOutput::CreateDefaultForCurrentPlatform()
{
#if PLATFORM_WINDOWS
	return FVdjmEncoderInitRequestOutput{
		.OutputFilePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Recordings"), TEXT("RecordingOutput.mp4")),
		.SessionId = FString::Printf(TEXT("Session_%lld"), FDateTime::Now().GetTicks()),
		.bOverwriteExists = false
	};
#elif PLATFORM_ANDROID || defined(__RESHARPER__)
	return FVdjmEncoderInitRequestOutput{
		.OutputFilePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Recordings"), TEXT("RecordingOutput.mp4")),
		.SessionId = FString::Printf(TEXT("Session_%lld"), FDateTime::Now().GetTicks()),
		.bOverwriteExists = false
	};
#else
	return TOptional<FVdjmEncoderInitRequestOutput>();
#endif
}

bool FVdjmEncoderInitRequestOutput::EvaluateValidation() const
{
	return !OutputFilePath.IsEmpty()
		&& !SessionId.IsEmpty();
}

TOptional<FVdjmEncoderInitRequestRuntimePolicy>FVdjmEncoderInitRequestRuntimePolicy::CreateDefaultForCurrentPlatform()
{
#if PLATFORM_WINDOWS
	return FVdjmEncoderInitRequestRuntimePolicy{
		.bRequireAVSync = true,
		.AllowedDriftMs = 20,
		.bStartMuxerWhenBothTracksReady = true
	};
#elif PLATFORM_ANDROID || defined(__RESHARPER__)
	return FVdjmEncoderInitRequestRuntimePolicy{
		.bRequireAVSync = true,
		.AllowedDriftMs = 20,
		.bStartMuxerWhenBothTracksReady = true
	};
#else
	return TOptional<FVdjmEncoderInitRequestRuntimePolicy>();
#endif
}

bool FVdjmEncoderInitRequestRuntimePolicy::EvaluateValidation() const
{
	return AllowedDriftMs >= 0;
}

TOptional<FVdjmEncoderInitRequestPlatformExtension> FVdjmEncoderInitRequestPlatformExtension::
CreateDefaultForCurrentPlatform()
{
	return TOptional<FVdjmEncoderInitRequestPlatformExtension>();
}

bool FVdjmEncoderInitRequestPlatformExtension::EvaluateValidation() const
{
	return true;
}

TOptional<FVdjmEncoderInitRequest> FVdjmEncoderInitRequest::CreateDefaultForCurrentPlatform()
{
	return FVdjmEncoderInitRequest{
		.VideoConfig = FVdjmEncoderInitRequestVideo::CreateDefaultForCurrentPlatform().GetValue(),
		.AudioConfig = FVdjmEncoderInitRequestAudio::CreateDefaultForCurrentPlatform().GetValue(),
		.OutputConfig = FVdjmEncoderInitRequestOutput::CreateDefaultForCurrentPlatform().GetValue(),
		.RuntimePolicyConfig = FVdjmEncoderInitRequestRuntimePolicy::CreateDefaultForCurrentPlatform().GetValue(),
		.PlatformExtensionConfig = FVdjmEncoderInitRequestPlatformExtension::CreateDefaultForCurrentPlatform().GetValue()
	};
}

bool FVdjmEncoderInitRequest::EvaluateValidation() const
{
	return VideoConfig.EvaluateValidation()
		&& AudioConfig.EvaluateValidation()
		&& OutputConfig.EvaluateValidation()
		&& RuntimePolicyConfig.EvaluateValidation()
		&& PlatformExtensionConfig.EvaluateValidation();
}

