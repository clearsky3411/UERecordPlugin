// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmRecordTypes.h"

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓			LOG Categories for Vdjm Recorder				↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
DEFINE_LOG_CATEGORY(LogVdjmRecorderCore)

FIntPoint VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution(uint32 tier)
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

FIntPoint VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution_Window(uint32 tier)
{
	const TArray<FIntPoint> resultResolution =
	{
		FIntPoint(7680, 4320),	// 8K, Enthusiast Monitor / TV
		FIntPoint(3840, 2160),	// UHD (4K), High-End PC Monitor
		FIntPoint(3440, 1440),	// UWQHD, 21:9 Ultrawide Monitor
		FIntPoint(2560, 1440),	// QHD (1440p), 2K Gaming Monitor
		FIntPoint(1920, 1200),	// WUXGA (1200p), 16:10 Standard Monitor
		FIntPoint(1920, 1080),	// FHD (1080p), Standard PC Monitor
		FIntPoint(1280, 720),	// HD (720p), Steam Deck (1280x800 for 16:10)
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

FIntPoint VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution_Android(uint32 tier)
{
	const TArray<FIntPoint> resultResolution =
	{
		FIntPoint(2560, 1600),	// Tablet (Landscape default), Samsung Galaxy Tab S8/S9
		FIntPoint(1812, 2176),	// Foldable (Inner Screen), Samsung Galaxy Z Fold 5
		FIntPoint(1440, 3120),	// Premium Flagship, Samsung Galaxy S24 Ultra / Pixel 8 Pro
		FIntPoint(1080, 2400),	// Standard Flagship 2, Google Pixel 7/8
		FIntPoint(1080, 2340),	// Standard Flagship, Samsung Galaxy S22/S23 (SM-S901/S911)
		FIntPoint(720, 1600),	// Budget Tier (HD+), Samsung Galaxy A12 / Older phones
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

FIntPoint VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution_Ios(uint32 tier)
{
	const TArray<FIntPoint> resultResolution =
	{
		FIntPoint(2048, 2732),	// iPad Pro 12.9-inch / 13-inch
		FIntPoint(1668, 2388),	// iPad Pro 11-inch
		FIntPoint(1290, 2796),	// iPhone 14/15/16 Pro Max
		FIntPoint(1284, 2778),	// iPhone 12/13/14 Pro Max & Plus
		FIntPoint(1179, 2556),	// iPhone 14 Pro / 15 Pro / 16 Pro
		FIntPoint(1170, 2532),	// iPhone 12 / 13 / 14 (Standard size)
		FIntPoint(750, 1334),	// iPhone SE (3rd Gen) / Older iPhones (8, 7)
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

FIntPoint VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution_Mac(uint32 tier)
{
	const TArray<FIntPoint> resultResolution =
	{
		FIntPoint(6016, 3384),	// Pro Display XDR (6K)
		FIntPoint(5120, 2880),	// Apple Studio Display / 27" iMac (5K Retina)
		FIntPoint(3456, 2234),	// MacBook Pro 16" (M1/M2/M3)
		FIntPoint(3024, 1964),	// MacBook Pro 14" (M1/M2/M3)
		FIntPoint(2560, 1664),	// MacBook Air 13" (M2/M3) - Liquid Retina (Notch included)
		FIntPoint(2560, 1600),	// MacBook Air 13" (M1) / Older MacBook Pro
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

FIntPoint VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution_Linux(uint32 tier)
{
	return GetPresetFeatureResolution_Window(tier);
}

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

namespace
{
	bool EvaluateOutputFilePathValidation(const FString& InOutputFilePath)
	{
		if (InOutputFilePath.IsEmpty())
		{
			return false;
		}

		const FString FullOutputFilePath = FPaths::ConvertRelativePathToFull(InOutputFilePath);
		FText ValidationError;
		if (!FPaths::ValidatePath(FullOutputFilePath, &ValidationError))
		{
			return false;
		}

		const FString FileName = FPaths::GetCleanFilename(FullOutputFilePath);
		if (FileName.IsEmpty())
		{
			return false;
		}

		const FString Extension = FPaths::GetExtension(FileName, false).ToLower();
		if (!(Extension == TEXT("mp4") || Extension == TEXT("mov")))
		{
			return false;
		}

		return true;
	}
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
	return EvaluateOutputFilePathValidation(OutputFilePath)
		&& !SessionId.TrimStartAndEnd().IsEmpty();
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
