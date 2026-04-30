// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmRecordTypes.h"

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓			LOG Categories for Vdjm Recorder				↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
DEFINE_LOG_CATEGORY(LogVdjmRecorderCore)

bool FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments() const
{
	if (VideoConfig.OutputFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - OutputFilePath is empty."));
		return false;
	}

	const FString normalizedPath = FPaths::ConvertRelativePathToFull(VideoConfig.OutputFilePath);
	const FString directoryPath = FPaths::GetPath(normalizedPath);
	const FString extension = FPaths::GetExtension(normalizedPath, false);

	if (directoryPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Directory path is empty. Path: %s"), *normalizedPath);
		return false;
	}

	if (not IFileManager::Get().DirectoryExists(*directoryPath))
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Directory does not exist. Directory: %s"), *directoryPath);
		return false;
	}

	if (extension.IsEmpty() || not extension.Equals(TEXT("mp4"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Output file extension must be mp4. Path: %s"), *normalizedPath);
		return false;
	}

	if (VideoConfig.MimeType.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - MimeType is empty."));
		return false;
	}

	const FString mimeTypeLower = VideoConfig.MimeType.ToLower();
	if (mimeTypeLower != TEXT("video/avc") && mimeTypeLower != TEXT("video/mp4"))
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Unsupported MimeType: %s"), *VideoConfig.MimeType);
		return false;
	}

	if (VideoConfig.VideoWidth <= 0 || VideoConfig.VideoHeight <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Invalid resolution. Width=%d Height=%d"), VideoConfig.VideoWidth, VideoConfig.VideoHeight);
		return false;
	}

	if ((VideoConfig.VideoWidth % 2) != 0 || (VideoConfig.VideoHeight % 2) != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Width and Height must be even. Width=%d Height=%d"), VideoConfig.VideoWidth, VideoConfig.VideoHeight);
		return false;
	}

	if (VideoConfig.VideoWidth < 16 || VideoConfig.VideoHeight < 16)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Resolution is too small. Width=%d Height=%d"), VideoConfig.VideoWidth, VideoConfig.VideoHeight);
		return false;
	}

	if (VideoConfig.VideoWidth > 7680 || VideoConfig.VideoHeight > 4320)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Resolution is too large. Width=%d Height=%d"), VideoConfig.VideoWidth, VideoConfig.VideoHeight);
		return false;
	}

	if (VideoConfig.VideoBitrate <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Bitrate must be > 0. Bitrate=%d"), VideoConfig.VideoBitrate);
		return false;
	}

	if (VideoConfig.VideoBitrate < 100000)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Bitrate looks too low. Bitrate=%d"), VideoConfig.VideoBitrate);
	}

	if (VideoConfig.VideoBitrate > 100000000)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - Bitrate is too high. Bitrate=%d"), VideoConfig.VideoBitrate);
		return false;
	}

	if (VideoConfig.VideoFPS <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - FrameRate must be > 0. FrameRate=%d"), VideoConfig.VideoFPS);
		return false;
	}

	if (VideoConfig.VideoFPS > 120)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - FrameRate is too high. FrameRate=%d"), VideoConfig.VideoFPS);
		return false;
	}

	if (VideoConfig.VideoIntervalSec < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - VideoIntervalSec must be >= 0. VideoIntervalSec=%d"), VideoConfig.VideoIntervalSec);
		return false;
	}

	if (VideoConfig.VideoIntervalSec == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - VideoIntervalSec is 0. This may create too many keyframes depending on codec behavior."));
	}

	if (VideoConfig.VideoIntervalSec > 10)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmRecordEncoderSnapshot::IsValidateCommonEncoderArguments - VideoIntervalSec is quite high. This may create very long GOPs depending on codec behavior."));
		return false;
	}

	return true;
}

bool FVdjmRecordEncoderSnapshot::IsValidatePlatformEncoderArguments() const
{
	if (TargetPlatform == EVdjmRecordEnvPlatform::EAndroid &&
		VideoConfig.GraphicBackend == EVdjmRecordGraphicBackend::EUnknown)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmRecordEncoderSnapshot::IsValidatePlatformEncoderArguments - Android GraphicBackend is unknown. Make sure to set it correctly for optimal performance."));
		return false;
	}

	return true;
}

bool FVdjmRecordEncoderSnapshot::IsValidateEncoderArguments() const
{
	return IsValidateCommonEncoderArguments() && IsValidatePlatformEncoderArguments();
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
	bool IsAllowedOutputExtension(const FString& extensionLower)
	{
#if PLATFORM_ANDROID
		return extensionLower == TEXT("mp4");
#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_IOS
		return extensionLower == TEXT("mp4") || extensionLower == TEXT("mov");
#else
		return extensionLower == TEXT("mp4");
#endif
	}

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

		const FString extension = FPaths::GetExtension(FileName, false).ToLower();
		if (!IsAllowedOutputExtension(extension))
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
