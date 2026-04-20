// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VdjmRecoderEncoderImpl.h"
#include "VdjmRecordTypes.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmAndroidTypes.h"

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct ANativeWindow;
class FVdjmAndroidEncoderImpl;
class FVdjmAndroidRecordSession;
class UVdjmRecordAndroidResource;



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

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
class FVdjmAndroidEncoderBackend
*/
class FVdjmAndroidEncoderBackend : public TSharedFromThis<FVdjmAndroidEncoderBackend, ESPMode::ThreadSafe>
{
public:
	FVdjmAndroidEncoderBackend();
	virtual ~FVdjmAndroidEncoderBackend();
	
	virtual bool Init(const FVdjmAndroidEncoderSnapshot& config, ANativeWindow* inputWindow) = 0;
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
	
	bool Initialize(const FVdjmAndroidEncoderSnapshot& configurer);
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
	
	void Clear();
	
	ANativeWindow* GetInputSurfaceWindow() { return mInputWindow; }
	AMediaMuxer* GetMediaMuxer() { return mMuxer; }
	AMediaCodec* GetMediaCodec() { return mCodec; }
	
	const FVdjmAndroidEncoderSnapshot& getConfig() const { return mConfig; }
	
	protected:
		bool VideoInit();
		bool AudioInit();
		bool AudioStart();
		void AudioStop();
		void PumpAudioInputToCodec();

		FVdjmAndroidEncoderSnapshot mConfig;
		AMediaCodec* mCodec = nullptr;
		AMediaCodec* mAudioCodec = nullptr;
		AMediaMuxer* mMuxer = nullptr;
	
	ANativeWindow* mInputWindow = nullptr;
	int32 mOutputFd = -1;
	
		int32 mTrackIndex = -1;
		int32 mAudioTrackIndex = -1;
		bool mInitialized = false;
		bool mRunning = false;
		bool mCodecStarted = false;
		bool mAudioCodecStarted = false;
		bool mMuxerStarted = false;
		bool mEosSent = false;
		int64 mNextAudioPtsUs = 0;
		int64 mFirstVideoPtsUs = -1;
		int64 mFirstAudioPtsUs = -1;
		bool bAudioInputWarningLogged = false;
		double mLastAudioHealthLogSec = 0.0;
		uint64 mLastCapturedSampleCount = 0;
		uint64 mLastQueuedSampleCount = 0;
		uint64 mLastDroppedSampleCount = 0;
		TArray<int16> mPendingCodecInputPcm;
		TSharedPtr<class FVdjmAndroidAudioCaptureBridge, ESPMode::ThreadSafe> mAudioCaptureBridge;
	
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
	
	/*
	 *	여기 호출에 config 를 새롭게 만들고 검증한다. 
	 */
	virtual bool InitializeEncoder(const FString& outputFilePath, int32 width, int32 height, int32 bitrate,	int32 framerate) override;	
	virtual bool InitializeEncoderExtended(const TWeakObjectPtr<UVdjmRecordResource> recordResource) override;
	/*
	 * mRecordSession 을 초기화 하고 다음 프레임에 녹화를 시작한다. pipeline 의 첫 프레임을 어떻게 하지.
	 */
	virtual VdjmResult StartEncoder() override;	

	virtual bool SubmitSurfaceFrame(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec) override;
	
	virtual void StopEncoder() override;
	
	virtual void TerminateEncoder() override;
	static FString GetCurrentRHINameSafe();
	
	bool IsOpenGlESRHI() const;
	bool IsVulkanRHI() const;
	
	FString DefaultMimeType = "video/avc";

	private:
		TSharedPtr<FVdjmAndroidRecordSession> mRecordSession;
		bool BuildSnapshotFromResource(const UVdjmRecordAndroidResource& androidRecordResource,
			FVdjmAndroidEncoderSnapshot& outSnapshot) const;

		FVdjmAndroidEncoderSnapshot mConfig;
	};


#endif
