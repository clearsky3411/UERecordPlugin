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

struct ANativeWindow;
class FVdjmAndroidEncoderImpl;
class FVdjmAndroidRecordSession;


enum class EVdjmAndroidGraphicBackend : uint8
{
	EUnknown,
	EOpenGL,
	EVulkan
};

class FVdjmAndroidFilePathWrapper
{
public:
	
	FVdjmAndroidFilePathWrapper(): mFinalFilePath(TEXT(""))
	{}
	
	FVdjmAndroidFilePathWrapper(const FString& inFilePath)
	{
		mFinalFilePath = inFilePath;
	}
	~FVdjmAndroidFilePathWrapper()= default;
	
	FString operator=( const FString& inFilePath )
	{
		mFinalFilePath = inFilePath;
		return mFinalFilePath;
	}
private:
	FString mFinalFilePath;
};

struct FVdjmAndroidEncoderConfigure
{
	int32 VideoWidth = 0;
	int32 VideoHeight = 0;
	int32 VideoBitrate = 0;
	int32 VideoFPS = 0;
	int32 VideoIntervalSec = 0;
	FString MimeType = "video/avc";
	FString OutputFilePath = TEXT("");
	EVdjmAndroidGraphicBackend GraphicBackend = EVdjmAndroidGraphicBackend::EUnknown;
	
	
	FVdjmAndroidEncoderConfigure() = default;
	FVdjmAndroidEncoderConfigure(int32 width, int32 height, int32 bitrate, int32 fps, const FString& outputFilePath)
		: VideoWidth(width), VideoHeight(height), VideoBitrate(bitrate), VideoFPS(fps), OutputFilePath(outputFilePath)
	{}
	FVdjmAndroidEncoderConfigure(const FVdjmAndroidEncoderConfigure& other)
		: VideoWidth(other.VideoWidth), VideoHeight(other.VideoHeight), VideoBitrate(other.VideoBitrate), VideoFPS(other.VideoFPS), OutputFilePath(other.OutputFilePath)
	{}
	FVdjmAndroidEncoderConfigure(FVdjmAndroidEncoderConfigure&& other) noexcept
		: VideoWidth(other.VideoWidth), VideoHeight(other.VideoHeight), VideoBitrate(other.VideoBitrate), VideoFPS(other.VideoFPS), OutputFilePath(other.OutputFilePath)
	{}
	FVdjmAndroidEncoderConfigure& operator=(const FVdjmAndroidEncoderConfigure& other)
	{
		this->VideoWidth = other.VideoWidth;
		this->VideoHeight = other.VideoHeight;
		this->VideoBitrate = other.VideoBitrate;
		this->VideoFPS = other.VideoFPS;
		this->OutputFilePath = other.OutputFilePath;
		return *this;
	}
	FVdjmAndroidEncoderConfigure& operator=(FVdjmAndroidEncoderConfigure&& other) noexcept
	{
		this->VideoWidth = other.VideoWidth;
		this->VideoHeight = other.VideoHeight;
		this->VideoBitrate = other.VideoBitrate;
		this->VideoFPS = other.VideoFPS;
		this->OutputFilePath = other.OutputFilePath;
		return *this;
	}
	
	bool IsValidateEncoderArguments() const;
	void Clear()
	{
		VideoWidth = 0;
		VideoHeight = 0;
		VideoBitrate = 0;
		VideoFPS = 0;
		OutputFilePath.Empty();
		MimeType = "video/avc";
		GraphicBackend = EVdjmAndroidGraphicBackend::EUnknown;
	}
};
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class FVdjmAndroidEncoderBackend
*/
class FVdjmAndroidEncoderBackend : public TSharedFromThis<FVdjmAndroidEncoderBackend, ESPMode::ThreadSafe>
{
public:
	FVdjmAndroidEncoderBackend();
	virtual ~FVdjmAndroidEncoderBackend();
	
	virtual bool Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow) = 0;
	virtual bool Start() = 0;
	
	virtual bool Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec);
	
	virtual void Pause(){ Stop();  }
	virtual void Resume(){Start(); }
	
	virtual void Stop() = 0;
	virtual void Terminate() = 0;
	
protected:
	ANativeWindow* mInputWindow = nullptr;
};
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class FVdjmAndroidRecordSession 
*/
class FVdjmAndroidRecordSession : public TSharedFromThis<FVdjmAndroidRecordSession, ESPMode::ThreadSafe>
{
public:	
	FVdjmAndroidRecordSession();
	virtual ~FVdjmAndroidRecordSession();
	
	bool Initialize(const FVdjmAndroidEncoderConfigure& configurer);
	bool Start();
	void Drain(bool bEndOfStream);
	bool Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec);
	void Stop();
	void Terminate();
	
	bool IsValidSession() const;
	bool IsInitialized() const { return mInitialized; }
	bool IsRunning() const { return mRunning; }
	bool IsCodecStarted() const { return mCodecStarted; }
	bool IsMuxerStarted() const { return mMuxerStarted; }
	
	bool IsStartable() const;
	bool IsRunnable(const FTextureRHIRef& srcTexture) const;
	
	ANativeWindow* GetInputSurfaceWindow() { return mInputWindow; }
	AMediaMuxer* GetMediaMuxer() { return mMuxer; }
	AMediaCodec* GetMediaCodec() { return mCodec; }
	
	const FVdjmAndroidEncoderConfigure& getConfig() const { return mConfig; }
	
protected:
	FVdjmAndroidEncoderConfigure mConfig;
	AMediaCodec* mCodec = nullptr;
	AMediaMuxer* mMuxer = nullptr;
	ANativeWindow* mInputWindow = nullptr;
	int32 mOutputFd = -1;
	
	int32 mTrackIndex = -1;
	bool mInitialized = false;
	bool mRunning = false;
	bool mCodecStarted = false;
	bool mMuxerStarted = false;
	bool mEosSent = false;
	
	TWeakPtr<FVdjmAndroidEncoderImpl> mOwnerEncoderImpl;
	TUniquePtr<FVdjmAndroidEncoderBackend> mGraphicBackend;
};


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

	virtual bool SubmitSurfaceFrame(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec) override;
	
	virtual void StopEncoder() override;
	
	virtual void TerminateEncoder() override;
	
	bool IsOpenGLRHI() const;
	bool IsVulkanRHI() const;
	
	FString DefaultMimeType = "video/avc";

private:
	TSharedPtr<FVdjmAndroidRecordSession> mRecordSession;
};


#endif