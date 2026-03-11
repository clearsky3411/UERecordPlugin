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

namespace 
{
	static constexpr const char* VdjmMimeAvc = "video/avc";
	static constexpr int32 VdjmColorFormatSurface = 0x7F000789; // COLOR_FormatSurface
	static constexpr int64 VdjmDrainTimeoutUs = 10000;
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
	switch (GetCurrentStatus())
	{
	case EVdjmResourceStatus::ENew:
	case EVdjmResourceStatus::EReady:
	case EVdjmResourceStatus::EWaiting:
		mEncoderVariables.Initialize( 
			width, 
			height, 
			bitrate, 
			framerate = FMath::Max(1,framerate),
			outputFilePath
			);
		mVideoTrackIndex = -1;
		mMuxerStarted = false;
		mSessionStarted = false;
		mBackendStarted = false;
		
		ChangeEncoderStatus(EVdjmResourceStatus::EReady);
		return true;
	case EVdjmResourceStatus::ERunning:
	case EVdjmResourceStatus::ETerminated:
	case EVdjmResourceStatus::EError:
		return false;
	}
	return false;
}

VdjmResult FVdjmAndroidEncoderImpl::StartEncoder()
{
	if (GetCurrentStatus() != EVdjmResourceStatus::EReady)
	{
		return VdjmResults::Fail;
	}
	if (not StartBackend())
	{
		ChangeEncoderStatus(EVdjmResourceStatus::EError);
		return VdjmResults::Fail;
	}
	mSessionStarted = true;
	ChangeEncoderStatus(EVdjmResourceStatus::ERunning);
	return VdjmResults::Ok;
}



void FVdjmAndroidEncoderImpl::StopEncoder()
{
}

void FVdjmAndroidEncoderImpl::TerminateEncoder()
{
	
}
bool FVdjmAndroidEncoderImpl::SubmitSurfaceFrame(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,
	double timeStampSec)
{
	
	if (!srcTexture.IsValid())
	{
		return false;
	}

	if (!IsStateRunning())
	{
		return false;
	}

	if (IsVulkanRHI())
	{
		return SubmitFrameVulkan(RHICmdList, srcTexture, timeStampSec);
	}

	if (IsOpenGLRHI())
	{
		return SubmitFrameOpenGL(RHICmdList, srcTexture, timeStampSec);
	}

	return false;
}

bool FVdjmAndroidEncoderImpl::StartBackend()
{
	ResetBackend();
	mCodec = AMediaCodec_createEncoderByType(VdjmMimeAvc);
	if (mCodec == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaCodec_createEncoderByType failed"));
		return false;
	}
	AMediaFormat* Format = AMediaFormat_new();
	if (Format == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaFormat_new failed"));
		ResetBackend();
		return false;
	}

	AMediaFormat_setString(Format, AMEDIAFORMAT_KEY_MIME, VdjmMimeAvc);
	AMediaFormat_setInt32(Format, AMEDIAFORMAT_KEY_WIDTH, mEncoderVariables.GetVideoWidth());
	AMediaFormat_setInt32(Format, AMEDIAFORMAT_KEY_HEIGHT, mEncoderVariables.GetVideoHeight());
	AMediaFormat_setInt32(Format, AMEDIAFORMAT_KEY_COLOR_FORMAT, VdjmColorFormatSurface);
	AMediaFormat_setInt32(Format, AMEDIAFORMAT_KEY_BIT_RATE, mEncoderVariables.GetVideoBitrate());
	AMediaFormat_setInt32(Format, AMEDIAFORMAT_KEY_FRAME_RATE, mEncoderVariables.GetVideoFPS());
	AMediaFormat_setInt32(Format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, mIFrameIntervalSec);

	const media_status_t ConfigureStatus =
		AMediaCodec_configure(mCodec, Format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);

	AMediaFormat_delete(Format);

	if (ConfigureStatus != AMEDIA_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaCodec_configure failed : %d"), (int32)ConfigureStatus);
		ResetBackend();
		return false;
	}

	const media_status_t SurfaceStatus = AMediaCodec_createInputSurface(mCodec, &mInputSurfaceWindow);
	if (SurfaceStatus != AMEDIA_OK || mInputSurfaceWindow == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaCodec_createInputSurface failed : %d"), (int32)SurfaceStatus);
		ResetBackend();
		return false;
	}

	FTCHARToUTF8 OutputPathUtf8(*mEncoderVariables.GetCurrentOutputFilePath());
	mOutputFd = open(OutputPathUtf8.Get(), O_CREAT | O_RDWR | O_TRUNC, 0644);
	if (mOutputFd < 0)
	{
		//UE_LOG(LogTemp, Error, TEXT("StartBackend - open failed : %s"), *mOutputFilePath);
		ResetBackend();
		return false;
	}

	mMuxer = AMediaMuxer_new(mOutputFd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
	if (mMuxer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaMuxer_new failed"));
		ResetBackend();
		return false;
	}

	const media_status_t StartStatus = AMediaCodec_start(mCodec);
	if (StartStatus != AMEDIA_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaCodec_start failed : %d"), (int32)StartStatus);
		ResetBackend();
		return false;
	}

	mVideoTrackIndex = -1;
	mMuxerStarted = false;
	mBackendStarted = true;

	//UE_LOG(LogTemp, Log, TEXT("StartBackend - started. output=%s %dx%d bitrate=%d fps=%d"),	*mOutputFilePath, mWidth, mHeight, mBitrate, mFrameRate);

	return true;
}
void FVdjmAndroidEncoderImpl::StopBackend()
{
	
}
void FVdjmAndroidEncoderImpl::ResetBackend()
{
	
}
bool FVdjmAndroidEncoderImpl::DrainEncoder(bool bEndOfStream)
{
	return true;
}

bool FVdjmAndroidEncoderImpl::SubmitFrameVulkan(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	if (!srcTexture.IsValid())
	{
		return false;
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("FVdjmAndroidEncoderImpl::SubmitFrameVulkan - Src=%p Time=%f"),
		srcTexture.GetReference(), timeStampSec);
	return true;
}

bool FVdjmAndroidEncoderImpl::SubmitFrameOpenGL(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	if (!srcTexture.IsValid())
	{
		return false;
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("FVdjmAndroidEncoderImpl::SubmitFrameOpenGL - Src=%p Time=%f"),
		srcTexture.GetReference(), timeStampSec);

	// TODO:
	// 1. GL 제출 경로 구현
	// 2. 현재는 B안 기준 우선순위 낮음

	return true;
}

bool FVdjmAndroidEncoderImpl::IsOpenGLRHI() const
{
	if (GDynamicRHI == nullptr)
	{
		return false;
	}
	const FString RHIName = GDynamicRHI->GetName();
	return RHIName.Contains(TEXT("OpenGL"), ESearchCase::IgnoreCase);
}

bool FVdjmAndroidEncoderImpl::IsVulkanRHI() const
{
	if (GDynamicRHI == nullptr)
	{
		return false;
	}
	const FString RHIName = GDynamicRHI->GetName();
	return RHIName.Contains(TEXT("Vulkan"), ESearchCase::IgnoreCase);
}


#endif
