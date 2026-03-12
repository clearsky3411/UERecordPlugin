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

class FVdjmAndroidEncoderImpl;
class FVdjmAndroidRecordSession;

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

struct VkEncoderContext
{
	VkInstance Instance = VK_NULL_HANDLE;
	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	VkDevice Device = VK_NULL_HANDLE;
	VkQueue GraphicsQueue = VK_NULL_HANDLE;
	uint32_t GraphicsQueueFamilyIndex = 0;
};

struct VkSubmitFrameInfo
{
	VkImage SrcImage = VK_NULL_HANDLE;
	VkFormat SrcFormat = VK_FORMAT_R8G8B8A8_UNORM;
	uint32_t SrcWidth = 0;
	uint32_t SrcHeight = 0;
	VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
};

struct FVdjmAndroidEncoderConfigure
{
	int32 VideoWidth = 0;
	int32 VideoHeight = 0;
	int32 VideoBitrate = 0;
	int32 VideoFPS = 0;
	FString MimeType = "video/avc";
	FString OutputFilePath = TEXT("");
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
	
	virtual bool Init(const FVdjmAndroidEncoderConfigure& config) = 0;
	virtual bool Start() = 0;
	
	virtual bool Running(FVdjmAndroidEncoderBackend* graphicImpl);
	virtual bool Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec);
	
	virtual void Pause(){ Stop();  }
	virtual void Resume(){Start(); }
	
	virtual void Stop() = 0;
	virtual void Terminate() = 0;
	
private:
	TWeakPtr<FVdjmAndroidRecordSession> mOwenrRecordSession;
	
};

class FVdjmAndroidRecordSession : public TSharedFromThis<FVdjmAndroidRecordSession, ESPMode::ThreadSafe>
{
public:	
	FVdjmAndroidRecordSession();
	~FVdjmAndroidRecordSession();
	
	bool Initialize(const FVdjmAndroidEncoderConfigure& configurer);
	bool Start();
	void Drain(bool bEndOfStream);
	void Stop();
	void Terminate();
	
	ANativeWindow* GetInputSurfaceWindow() const { return mInputSurfaceWindow; }
protected:
	FVdjmAndroidEncoderConfigure mConfig;
	AMediaCodec* mCodec = nullptr;
	AMediaMuxer* mMuxer = nullptr;
	ANativeWindow* mInputSurfaceWindow = nullptr;
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

	virtual void StopEncoder() override;
	virtual void TerminateEncoder() override;
	
	virtual bool SubmitSurfaceFrame(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec) override;

	bool IsOpenGLRHI() const;
	bool IsVulkanRHI() const;
	/*
	 * GLuint srcTexture, int64_t ptsNs)
struct VkSubmitFrameInfo
{
VkImage SrcImage = VK_NULL_HANDLE;
VkFormat SrcFormat = VK_FORMAT_R8G8B8A8_UNORM;
uint32_t SrcWidth = 0;
uint32_t SrcHeight = 0;
VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
};
	 */
private:
	TUniquePtr<FVdjmAndroidRecordSession> mRecordSession;
};

class FVdjmAndroidEncoderBackendVulkan : public FVdjmAndroidEncoderBackend
{
};
class FVdjmAndroidEncoderBackendOpenGL : public FVdjmAndroidEncoderBackend
{
};
#endif