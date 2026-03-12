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


bool FVdjmAndroidRecordSession::Initialize(const FVdjmAndroidEncoderConfigure& configure)
{
	if (mInitialized)
	{
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
	mOutputFd = open(mConfig.OutputFilePath.GetData(), O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (mOutputFd < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidRecordSession::Start - Failed to open output file: %s"), *mConfig.OutputFilePath);
		return false;
	}
	mCodec = AMediaCodec_createEncoderByType(VdjmMimeAvc);
	if (mCodec == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaCodec_createEncoderByType failed"));
		return false;
	}
	AMediaFormat* format = AMediaFormat_new();
	if (format == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaFormat_new failed"));
		Terminate();
		return false;
	}
	AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mConfig.MimeType);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, mConfig.VideoWidth);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, mConfig.VideoHeight);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, mConfig.VideoBitrate);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, mConfig.VideoFPS);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, mConfig.VideoIntervalSec);
	
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7F000789);
	
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

	mCodecStarted = true;
	mRunning = true;
	mMuxerStarted = false;
	mTrackIndex = -1;
	mEosSent = false;
	return true;
}

void FVdjmAndroidRecordSession::Drain(bool bEndOfStream)
{
	if (!mCodec)
		return;
	if (endOfStream && !mEosSent)
	{
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

void FVdjmAndroidRecordSession::Stop()
{
	if (!mRunning)
		return;

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

	mRunning = false;
}

void FVdjmAndroidRecordSession::Terminate()
{
	Stop();
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
	if (not mRecordSession.IsValid() || mRecordSession == nullptr)
	{
		mRecordSession = MakeShared<FVdjmAndroidRecordSession>();
	}
	if (mRecordSession->IsRunning())
	{
		mRecordSession->Stop();
		mRecordSession->Terminate();
	}
	return mRecordSession->Initialize(FVdjmAndroidEncoderConfigure( width, height, bitrate, framerate,outputFilePath));
}

VdjmResult FVdjmAndroidEncoderImpl::StartEncoder()
{
	
}

void FVdjmAndroidEncoderImpl::StopEncoder()
{
}

void FVdjmAndroidEncoderImpl::TerminateEncoder()
{
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
