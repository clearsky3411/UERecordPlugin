// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
bool FVdjmVkInputAnalyzer::Analyze(const FTextureRHIRef& srcTexture, FVdjmVkSubmitFrameInfo& outInfo) const
{
	if (!srcTexture.IsValid())
	{
		UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("Analyze: srcTexture is invalid"));
		return false;
	}

	outInfo.SrcImage = static_cast<VkImage>(srcTexture->GetNativeResource());
	if (outInfo.SrcImage == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("Analyze: native VkImage is null"));
		return false;
	}

	outInfo.SrcWidth = srcTexture->GetSizeX();
	outInfo.SrcHeight = srcTexture->GetSizeY();

	switch (srcTexture->GetFormat())
	{
	case PF_B8G8R8A8:
		outInfo.SrcFormat = VK_FORMAT_B8G8R8A8_UNORM;
		break;
	case PF_R8G8B8A8:
		outInfo.SrcFormat = VK_FORMAT_R8G8B8A8_UNORM;
		break;
	default:
		UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("Analyze: unsupported UE pixel format %d"), (int32)srcTexture->GetFormat());
		return false;
	}

	outInfo.bFormatMatchesSwapchain = (outInfo.SrcFormat == OwnerBackend->GetSwapchainFormat());
	outInfo.bExtentMatchesSwapchain =
		(outInfo.SrcWidth == OwnerBackend->GetSwapchainWidth() &&
		 outInfo.SrcHeight == OwnerBackend->GetSwapchainHeight());

	outInfo.bCanDirectCopy = outInfo.bFormatMatchesSwapchain && outInfo.bExtentMatchesSwapchain;
	outInfo.bNeedsIntermediate = !outInfo.bCanDirectCopy;

	return true;
}

bool FVdjmVkIntermediateStage::NeedRecreate(const FVdjmVkSubmitFrameInfo& frameInfo, uint32 curWid, uint32 curhei,
                                            VkFormat curFormat) const
{
	return false;
}

bool FVdjmVkIntermediateStage::EnsureResource(FVdjmAndroidEncoderBackendVulkan& backend,
	const FVdjmVkSubmitFrameInfo& frameInfo)
{
	return false;
}

bool FVdjmVkIntermediateStage::RecordPrepareAndCopy(FVdjmAndroidEncoderBackendVulkan& owner,
	const FVdjmVkSubmitFrameInfo& frameInfo)
{
	return false;
}

bool FVdjmVkSurfaceSubmitter::Submit(FVdjmAndroidEncoderBackendVulkan& owner, double timeStampSec)
{
	return false;
}


FVdjmAndroidEncoderBackendVulkan::FVdjmAndroidEncoderBackendVulkan()
	: mAnalyzer(this), mIntermediateStage(this), mSubmitter(this),
	mInitialized(false), mStarted(false), mPaused(false), mRuntimeReady(false),
	mIntermediateImage(VK_NULL_HANDLE),
	mIntermediateMemory(VK_NULL_HANDLE),
	mIntermediateView(VK_NULL_HANDLE),
	mCommandPool(VK_NULL_HANDLE),
	mCommandBuffer(VK_NULL_HANDLE),
	mSubmitFence(VK_NULL_HANDLE),
	mIntermediateWidth(0), mIntermediateHeight(0), 
	mIntermediateFormat(VK_FORMAT_UNDEFINED)
{
}

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
	if (!mInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("... Not initialized"));
		return false;
	}
	if (!mStarted || mPaused)
	{
		return false;
	}
	if (mInputWindow == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("... inputWindow is null"));
		return false;
	}
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	if (!IsRunnable())
	{
		//UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("Vulkan backend is not runnable"));
		return false;
	}

	if (!EnsureRuntimeReady())
	{
		//UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("EnsureRuntimeReady failed"));
		return false;
	}

	FVdjmVkSubmitFrameInfo FrameInfo;
	if (!mAnalyzer.Analyze(srcTexture, FrameInfo))
	{
		//UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("Analyze failed"));
		return false;
	}

	if (FrameInfo.bNeedsIntermediate)
	{
		if (!mIntermediateStage.EnsureResource(*this, FrameInfo))
		{
			//UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("EnsureResource failed"));
			return false;
		}

		if (!mIntermediateStage.RecordPrepareAndCopy(*this, FrameInfo))
		{
			//UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("RecordPrepareAndCopy failed"));
			return false;
		}
	}

	if (!SubmitTextureToCodecSurface(RHICmdList, srcTexture, FrameInfo.SrcImage, timeStampSec))
	{
		//UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("SubmitTextureToCodecSurface failed"));
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
	//AHardwareBuffer_isSupported
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
	FVdjmVkSubmitFrameInfo frameInfo;
	if (!mAnalyzer.Analyze(srcTexture, frameInfo))
	{
		return false;
	}

	if (!mIntermediateStage.EnsureResource(*this, frameInfo))
	{
		return false;
	}

	if (!mIntermediateStage.RecordPrepareAndCopy(*this, frameInfo))
	{
		return false;
	}

	if (!mSubmitter.Submit(*this, timeStampSec))
	{
		return false;
	}

	return true;
}
#endif