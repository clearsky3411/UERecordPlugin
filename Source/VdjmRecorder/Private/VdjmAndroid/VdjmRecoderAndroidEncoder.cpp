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
	AMediaFormat* Format = AMediaFormat_new();
	if (Format == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("StartBackend - AMediaFormat_new failed"));
		
		return false;
	}
}

void FVdjmAndroidRecordSession::Drain(bool bEndOfStream)
{
}

void FVdjmAndroidRecordSession::Stop()
{
}

void FVdjmAndroidRecordSession::Terminate()
{
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
