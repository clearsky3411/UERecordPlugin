// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"
bool FVdjmAndroidEncoderBackendVulkan::Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow)
{
	if (not config.IsValidateEncoderArguments() )
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - Invalid encoder configuration."));
		return false;
	}
	if (inputWindow == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - inputWindow is null."));
		return false;
	}
	
	mConfig = config;
	mInputWindow = inputWindow;
	
	mInitialized = true;
	mStarted = false;
	mPaused = false;
	mRuntimeReady = false;
	
	return true;
}
bool FVdjmAndroidEncoderBackendVulkan::Start()
{
	if (not mInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Start - Encoder backend is not initialized."));
		return false;
	}
	if (mInputWindow == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Start - inputWindow is null."));
		return false;
	}
	if (mStarted)
	{
		return true;
	}
	
	mPaused = false;
	mStarted = true;
	return true;
}

void FVdjmAndroidEncoderBackendVulkan::Stop()
{
	mStarted = false;
	mPaused = false;
}

void FVdjmAndroidEncoderBackendVulkan::Terminate()
{
	mRuntimeReady = false;
	mStarted = false;
	mPaused = false;
	mInitialized = false;
	mInputWindow = nullptr;
}

bool FVdjmAndroidEncoderBackendVulkan::IsRunnable()
{
	if (not mInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Running - Not initialized"));
		
		return true;
	}

	if (not mStarted || mPaused)
	{
		return true;
	}

	if (mInputWindow == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Running - inputWindow is null"));
		return true;
	}
	return false;
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	if (not IsRunnable())
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Running - Not runnable"));
		return false;
	}
	if (not srcTexture.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Running - srcTexture is invalid"));
		return false;
	}
	if (not EnsureRuntimeReady())
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Running - runtime is not ready"));
		return false;
	}
	FVdjmVkSubmitFrameInfo frameInfo;
	if (not mAnalyzer.Analyze(srcTexture, frameInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Running - Failed to analyze source texture"));
		return false;
	}
	if ( not mIntermediateStage.EnsureResource(*this, frameInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Running - Failed to ensure intermediate resource"));
		return false;
	}
	
	if (not mIntermediateStage.RecordPrepareAndCopy(*this, frameInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Running - Failed to prepare and copy to intermediate resource"));
		return false;
	}
	if (!mSubmitter.Submit(*this, timeStampSec))
	{
		return false;
	}
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::EnsureRuntimeReady()
{
	if (mRuntimeReady)
	{
		return true;
	}

	if (mInputWindow == nullptr)
	{
		return false;
	}

	mRuntimeReady = true;
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::TryExtractNativeVkImage(const FTextureRHIRef& srcTexture,VkImage& outImage) const
{
	outImage = VK_NULL_HANDLE;

	if (!srcTexture.IsValid())
	{
		return false;
	}

	void* nativeResource = srcTexture->GetNativeResource();
	if (nativeResource == nullptr)
	{
		return false;
	}

	outImage = reinterpret_cast<VkImage>(nativeResource);
	return outImage != VK_NULL_HANDLE;
}

bool FVdjmAndroidEncoderBackendVulkan::SubmitTextureToCodecSurface(FRHICommandList& RHICmdList,
	const FTextureRHIRef& srcTexture, VkImage srcImage, double timeStampSec)
{
	if (srcImage == VK_NULL_HANDLE)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::SubmitTextureToCodecSurface - srcImage is null"));
		return false;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("FVdjmAndroidEncoderBackendVulkan::SubmitTextureToCodecSurface - Vulkan submit path is not implemented yet. Width=%d Height=%d Time=%f"),
		mConfig.VideoWidth,
		mConfig.VideoHeight,
		timeStampSec);

	return false;
}
