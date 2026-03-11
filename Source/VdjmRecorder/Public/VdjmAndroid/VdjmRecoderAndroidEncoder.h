// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VdjmRecoderEncoderImpl.h"
#include "VdjmRecordTypes.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class FVdjmAndroidEncoderImpl : public FVdjmVideoEncoderBase
*/
class FVdjmAndroidEncoderImpl : public FVdjmVideoEncoderBase
{
public:
	
	FVdjmAndroidEncoderImpl();
	virtual ~FVdjmAndroidEncoderImpl() override;
	
	virtual bool InitializeEncoder(const FString& outputFilePath, int32 width, int32 height, int32 bitrate,	int32 framerate) override;
	virtual VdjmResult StartEncoder() override;

	virtual void StopEncoder() override;
	virtual void TerminateEncoder() override;
	
	virtual bool SubmitSurfaceFrame(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,
		double timeStampSec) override;

private:
	bool StartBackend();
	void StopBackend();
	void ResetBackend();
	bool DrainEncoder(bool bEndOfStream);
	
	bool SubmitFrameVulkan(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec);
	bool SubmitFrameOpenGL(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec);
	
	bool IsOpenGLRHI() const;
	bool IsVulkanRHI() const;
	
	
	FEncoderImplPropertyData mEncoderVariables;
	int32 mIFrameIntervalSec = 1;
	int32 mVideoTrackIndex = -1;
	int mOutputFd = -1;

	
	bool mSessionStarted = false;
	FTextureRHIRef mEncoderInputTexture;
	

	AMediaCodec* mCodec = nullptr;
	AMediaMuxer* mMuxer = nullptr;
	ANativeWindow* mInputSurfaceWindow = nullptr;

	bool mBackendStarted = false;
	bool mMuxerStarted = false;

};
#endif