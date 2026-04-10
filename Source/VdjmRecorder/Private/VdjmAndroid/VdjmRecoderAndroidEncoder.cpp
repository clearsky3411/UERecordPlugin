// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmRecoderAndroidEncoder.h"

#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <fcntl.h>
#include <unistd.h>

#include "HardwareInfo.h"
#include "RHI.h"
#include "VdjmAndroid/VdjmAndroidCore.h"

#include "VdjmAndroid/VdjmAndroidEncoderBackendOpenGL.h"
#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"

namespace 
{
	static constexpr const char* VdjmMimeAvc = "video/avc";
	static constexpr int32 VdjmColorFormatSurface = 0x7F000789; // COLOR_FormatSurface
	static constexpr int64 VdjmDrainTimeoutUs = 10000;
}

FVdjmAndroidEncoderBackend::FVdjmAndroidEncoderBackend()
{
}

FVdjmAndroidEncoderBackend::~FVdjmAndroidEncoderBackend()
{
}

bool FVdjmAndroidEncoderBackend::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,
	double timeStampSec)
{
	return false;
}

FVdjmAndroidRecordSession::FVdjmAndroidRecordSession()
	: mInitialized(false)
	, mRunning(false)
	, mCodec(nullptr)
	, mInputWindow(nullptr)
	, mMuxer(nullptr)
	, mTrackIndex(-1)
	, mEosSent(false)
{
}

FVdjmAndroidRecordSession::~FVdjmAndroidRecordSession()
{
}

bool FVdjmAndroidRecordSession::Initialize(const FVdjmAndroidEncoderConfigure& configure )
{
	UE_LOG(LogTemp, Log, TEXT("FVdjmAndroidRecordSession::Initialize - in Config %s"),*configure.ToString());
	
	if (mInitialized )
	{
		return false;
	}
	if (not configure.IsValidateEncoderArguments() )
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidRecordSession::Initialize - Invalid encoder configuration."));
		return false;
	}
	mConfig = configure;
	mInitialized = true;
	
	return true;
}

bool FVdjmAndroidRecordSession::Start()
{
	if (not mInitialized || mRunning)
	{
		return false;
	}
	UE_LOG(LogTemp, Warning,
	TEXT("FVdjmAndroidRecordSession::Start - mConfig before validate: %s"),
	*mConfig.ToString());
	
	mOutputFd = open(TCHAR_TO_UTF8(*mConfig.VideoConfig.OutputFilePath), O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (mOutputFd < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidRecordSession::Start - Failed to open output file: %s"), *mConfig.VideoConfig.OutputFilePath);
		return false;
	}
	mCodec = AMediaCodec_createEncoderByType(VdjmMimeAvc);
	if (mCodec == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaCodec_createEncoderByType failed"));
		Terminate();
		return false;
	}
	AMediaFormat* format = AMediaFormat_new();
	if (format == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaFormat_new failed"));
		Terminate();
		return false;
	}
	
	AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, TCHAR_TO_UTF8(*mConfig.VideoConfig.MimeType));
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, mConfig.VideoConfig.VideoWidth);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, mConfig.VideoConfig.VideoHeight);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, mConfig.VideoConfig.VideoBitrate);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, mConfig.VideoConfig.VideoFPS);

	// 0은 일부 기기/드라이버에서 과도한 IDR 요청으로 이어질 수 있어
	// 실제 코덱 설정 시에는 최소 1초로 보정해 안정성을 우선한다.
	const int32 SafeIFrameIntervalSec = FMath::Max(1, mConfig.VideoIntervalSec);
	if (SafeIFrameIntervalSec != mConfig.VideoIntervalSec)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("FVdjmAndroidRecordSession::Start - VideoIntervalSec=%d, overriding to %d for codec stability."),
			mConfig.VideoConfig.VideoIntervalSec, SafeIFrameIntervalSec);
	}
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, SafeIFrameIntervalSec);
	
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, VdjmColorFormatSurface);
	
	media_status_t status = AMediaCodec_configure(
			mCodec,
			format,
			nullptr,
			nullptr,
			AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
	
	AMediaFormat_delete(format);
	
	if (status != AMEDIA_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaCodec_configure failed. status=%d"), status);
		Terminate();
		return false;
	}
	
	status = AMediaCodec_createInputSurface(mCodec, &mInputWindow);
	if (status != AMEDIA_OK || !mInputWindow)
	{
		Terminate();
		return false;
	}
	mMuxer = AMediaMuxer_new(mOutputFd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
	if (!mMuxer)
	{
		Terminate();
		return false;
	}

	status = AMediaCodec_start(mCodec);
	if (status != AMEDIA_OK)
	{
		Terminate();
		return false;
	}
	
	if (not mGraphicBackend.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("FVdjmAndroidRecordSession::Start - Creating graphic backend for %d"), static_cast<int32>(mConfig.VideoConfig.GraphicBackend));
		switch (mConfig.VideoConfig.GraphicBackend)
		{
		case EVdjmAndroidGraphicBackend::EUnknown:
			Terminate();
			return false;
		case EVdjmAndroidGraphicBackend::EOpenGL:
			mGraphicBackend = MakeUnique<FVdjmAndroidEncoderBackendOpenGL>();
			break;
		case EVdjmAndroidGraphicBackend::EVulkan:
			mGraphicBackend = MakeUnique<FVdjmAndroidEncoderBackendVulkan>();
			break;
		}
	}
	
	if (not mGraphicBackend->Init(mConfig, mInputWindow) )
	{
		Terminate(); 
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidRecordSession::Start - Failed to initialize graphic backend"));
		return false;
	}
	
	mCodecStarted = true;
	mMuxerStarted = false;
	mTrackIndex = -1;
	mEosSent = false;
	if (not mGraphicBackend->Start())
	{
		Terminate();
		return false;
	}
	mRunning = true;
	return true;
}

void FVdjmAndroidRecordSession::Drain(bool bEndOfStream)
{
	if (!mCodec)
		return;
	if (bEndOfStream && !mEosSent)
	{
		UE_LOG(LogTemp, Log, TEXT("FVdjmAndroidRecordSession::Drain - Signaling end of input stream to codec."));
		AMediaCodec_signalEndOfInputStream(mCodec);
		mEosSent = true;
	}

	while (true)
	{
		AMediaCodecBufferInfo bufferInfo{};
		const ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(mCodec, &bufferInfo, 0);

		if (outputIndex == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
		{
			break;
		}
		else if (outputIndex == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
		{
			AMediaFormat* newFormat = AMediaCodec_getOutputFormat(mCodec);
			if (!newFormat)
				break;

			mTrackIndex = AMediaMuxer_addTrack(mMuxer, newFormat);
			AMediaFormat_delete(newFormat);

			if (mTrackIndex >= 0 && !mMuxerStarted)
			{
				AMediaMuxer_start(mMuxer);
				mMuxerStarted = true;
			}
		}
		else if (outputIndex >= 0)
		{
			size_t outSize = 0;
			uint8_t* outBuffer = AMediaCodec_getOutputBuffer(mCodec, outputIndex, &outSize);

			if (outBuffer && bufferInfo.size > 0 && mMuxerStarted)
			{
				AMediaMuxer_writeSampleData(mMuxer, mTrackIndex, outBuffer, &bufferInfo);
			}

			AMediaCodec_releaseOutputBuffer(mCodec, outputIndex, false);

			if ((bufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0)
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
}

bool FVdjmAndroidRecordSession::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,
	double timeStampSec)
{
	if (!mRunning)
	{
		return false;
	}

	if (!mGraphicBackend.IsValid())
	{
		return false;
	}

	if (!srcTexture.IsValid())
	{
		return false;
	}
	
	if (not mGraphicBackend->Running(RHICmdList, srcTexture, timeStampSec))
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
	TEXT("FVdjmAndroidRecordSession::Running - Graphic backend Running() returned false."));
		return false;
	}
	
	Drain(false);
	return true;
}

void FVdjmAndroidRecordSession::Stop()
{
	if (!mRunning)
	{
		return;
	}
	
	if (mGraphicBackend.IsValid())
	{
		mGraphicBackend->Stop();
	}
	
	Drain(true);

	if (mCodec && mCodecStarted)
	{
		AMediaCodec_stop(mCodec);
		mCodecStarted = false;
	}

	if (mMuxer && mMuxerStarted)
	{
		AMediaMuxer_stop(mMuxer);
		mMuxerStarted = false;
	}
	
	const FString FinalOutputPath = FPaths::ConvertRelativePathToFull(mConfig.VideoConfig.OutputFilePath);
	const int64 OutputFileSize = IFileManager::Get().FileSize(*FinalOutputPath);

	UE_LOG(LogTemp, Warning,
		TEXT("FVdjmAndroidRecordSession::Stop - Output file path=%s size=%lld"),
		*FinalOutputPath,
		OutputFileSize);

	if (OutputFileSize <= 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("FVdjmAndroidRecordSession::Stop - Recorded output file is missing or empty."));
	}
	
	mRunning = false;
}

void FVdjmAndroidRecordSession::Terminate()
{
	Stop();
	
	if (mGraphicBackend.IsValid())
	{
		mGraphicBackend->Terminate();
		mGraphicBackend.Reset();
	}
	
	if (mInputWindow)
	{
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
	}

	if (mCodec)
	{
		AMediaCodec_delete(mCodec);
		mCodec = nullptr;
	}

	if (mMuxer)
	{
		AMediaMuxer_delete(mMuxer);
		mMuxer = nullptr;
	}

	if (mOutputFd >= 0)
	{
		close(mOutputFd);
		mOutputFd = -1;
	}

	mTrackIndex = -1;
	mMuxerStarted = false;
	mCodecStarted = false;
	mEosSent = false;
	mRunning = false;
	mInitialized = false;
}

bool FVdjmAndroidRecordSession::IsValidSession() const
{
	// 1. 초기화 안 된 상태인데 네이티브 자원이 남아 있으면 이상함
	if (not mInitialized)
	{
		const bool bHasNativeResources =
			(mCodec != nullptr) ||
			(mMuxer != nullptr) ||
			(mInputWindow != nullptr) ||
			(mOutputFd >= 0) ||
			mCodecStarted ||
			mMuxerStarted ||
			mRunning ||
			(mTrackIndex != -1) ||
			mEosSent;

		return !bHasNativeResources;
	}

	// 2. initialized 상태면 config는 최소한 유효해야 함
	if (!mConfig.IsValidateEncoderArguments())
	{
		return false;
	}

	// 3. running 상태면 실제 런타임 리소스가 반드시 있어야 함
	if (mRunning)
	{
		if (mCodec == nullptr) return false;
		if (mMuxer == nullptr) return false;
		if (mInputWindow == nullptr) return false;
		if (mOutputFd < 0) return false;
		if (!mCodecStarted) return false;
		
		return true;
	}
	return true;
}

bool FVdjmAndroidRecordSession::IsStartable() const
{
	if (not mInitialized) return false;
	if (mRunning) return false;
	if (not mConfig.IsValidateEncoderArguments()) return false;
	if (not IsValidSession()) return false;
	return true;
}

bool FVdjmAndroidRecordSession::IsRunnable(const FTextureRHIRef& srcTexture) const
{
	if (!mRunning)
	{
		return false;
	}

	if (!mGraphicBackend.IsValid())
	{
		return false;
	}

	if (!srcTexture.IsValid())
	{
		return false;
	}

	return true;
}

void FVdjmAndroidRecordSession::Clear()
{
	if (IsRunning())
	{
		Stop();
	}
	Terminate();
	mConfig.Clear();
	mOwnerEncoderImpl = nullptr;
}

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class FVdjmAndroidEncoderImpl : public FVdjmVideoEncoderBase
*/
FVdjmAndroidEncoderImpl::FVdjmAndroidEncoderImpl()
{
	
}

FVdjmAndroidEncoderImpl::~FVdjmAndroidEncoderImpl()
{
	TerminateEncoder();
}

bool FVdjmAndroidEncoderImpl::InitializeEncoder(const FString& outputFilePath, int32 width, int32 height, int32 bitrate,int32 framerate)
{
	UE_LOG(LogTemp, Log, TEXT("FVdjmAndroidEncoderImpl::InitializeEncoder - outputFilePath: %s, width: %d, height: %d, bitrate: %d, framerate: %d"), *outputFilePath, width, height, bitrate, framerate);
	
	if (mRecordSession.IsValid())
	{
		if (mRecordSession->IsRunning())
		{
			UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderImpl::InitializeEncoder - Encoder is already running. Stopping current session before reinitializing."));
			mRecordSession->Stop();
		}
		mRecordSession->Terminate();
		mRecordSession.Reset();
		mRecordSession = nullptr;
	}
	mConfig.VideoConfig.OutputFilePath = outputFilePath;
	mConfig.VideoConfig.VideoWidth = width;
	mConfig.VideoConfig.VideoHeight = height;
	mConfig.VideoConfig.VideoBitrate = bitrate;
	mConfig.VideoConfig.VideoFPS = framerate;
	mConfig.VideoConfig.GraphicBackend =  IsVulkanRHI() ? EVdjmAndroidGraphicBackend::EVulkan : 
	(IsOpenGlESRHI() ? EVdjmAndroidGraphicBackend::EOpenGL : 
		EVdjmAndroidGraphicBackend::EUnknown);

	return mConfig.IsValidateEncoderArguments();
}

bool FVdjmAndroidEncoderImpl::InitializeEncoderExtended(const TWeakObjectPtr<UVdjmRecordResource> recordResource)
{
	if (not recordResource.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderImpl::InitializeEncoderExtended - Invalid record resource."));
		return false;
	}
	
	if (const UVdjmRecordAndroidResource* androidRecordRes = Cast<UVdjmRecordAndroidResource>(recordResource.Get()))
	{
		/*
		 * TODO(260410-cofigs) 
		 */
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderImpl::InitializeEncoderExtended - Record resource is not of type UVdjmRecordAndroidResource."));
		return false;
	}
	
}

VdjmResult FVdjmAndroidEncoderImpl::StartEncoder()
{
	if (mRecordSession == nullptr)
	{
		mRecordSession = MakeShared<FVdjmAndroidRecordSession>();
	}
	else
	{
		mRecordSession->Clear();
	}
	
	if (not mRecordSession->Initialize(mConfig))
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::StartEncoder - Failed to initialize record session with current config."));
		return VdjmResults::Fail;
	}
	
	if (not mRecordSession->IsStartable() || not mRecordSession.IsValid())
	{
		return VdjmResults::Fail;
	}
	
	return mRecordSession->Start() ? VdjmResults::Ok : VdjmResults::Fail;
}

bool FVdjmAndroidEncoderImpl::SubmitSurfaceFrame(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,
	double timeStampSec)
{
	if (mRecordSession.IsValid() && mRecordSession->IsRunnable(srcTexture))
	{
		return mRecordSession->Running(RHICmdList, srcTexture, timeStampSec);
	}
	
	return false;
}

void FVdjmAndroidEncoderImpl::StopEncoder()
{
	if (not mRecordSession.IsValid() || mRecordSession == nullptr)
	{
		return;
	}

	if (not mRecordSession->IsRunning())
	{
		return;
	}

	mRecordSession->Stop();
}

void FVdjmAndroidEncoderImpl::TerminateEncoder()
{
	if (!mRecordSession.IsValid() || mRecordSession == nullptr)
	{
		return;
	}

	if (mRecordSession->IsRunning())
	{
		mRecordSession->Stop();
	}

	mRecordSession->Terminate();
	mRecordSession.Reset();
}

FString FVdjmAndroidEncoderImpl::GetCurrentRHINameSafe()
{
	FString RHIName;

	RHIName = FApp::GetGraphicsRHI();
	if (!RHIName.IsEmpty())
	{
		return RHIName;
	}

	RHIName = FHardwareInfo::GetHardwareInfo(NAME_RHI);
	if (!RHIName.IsEmpty())
	{
		return RHIName;
	}

	if (GDynamicRHI != nullptr)
	{
		RHIName = GDynamicRHI->GetName();
		if (!RHIName.IsEmpty())
		{
			return RHIName;
		}
	}

	return TEXT("Unknown");
}

bool FVdjmAndroidEncoderImpl::IsOpenGlESRHI() const
{
	const FString RHIName = GetCurrentRHINameSafe();
	UE_LOG(LogTemp, Log, TEXT("FVdjmAndroidEncoderImpl::IsOpenGLRHI - Current RHI Name: %s"), *RHIName);
	return RHIName.Contains(TEXT("OpenGL"), ESearchCase::IgnoreCase)
		|| RHIName.Contains(TEXT("OpenGLES"), ESearchCase::IgnoreCase)
		|| RHIName.Contains(TEXT("GL"), ESearchCase::IgnoreCase);
}

bool FVdjmAndroidEncoderImpl::IsVulkanRHI() const
{
	const FString RHIName = GetCurrentRHINameSafe();
	UE_LOG(LogTemp, Log, TEXT("FVdjmAndroidEncoderImpl::IsVulkanRHI - Current RHI Name: %s"), *RHIName);
	return RHIName.Contains(TEXT("Vulkan"), ESearchCase::IgnoreCase);
}

#endif
