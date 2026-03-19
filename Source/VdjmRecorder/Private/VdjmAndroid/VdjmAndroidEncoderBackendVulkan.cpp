// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
bool FVdjmVkInputAnalyzer::Analyze(const FTextureRHIRef& srcTexture, FVdjmVkSubmitFrameInfo& outInfo) const
{
	if (!srcTexture.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Analyze: srcTexture is invalid"));
		return false;
	}

	outInfo.SrcImage = static_cast<VkImage>(srcTexture->GetNativeResource());
	if (outInfo.SrcImage == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Analyze: native VkImage is null"));
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
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Analyze: unsupported UE pixel format %d"), (int32)srcTexture->GetFormat());
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

bool FVdjmVkIntermediateStage::NeedRecreate(const FVdjmVkSubmitFrameInfo& frameInfo, uint32 curWid, uint32 curhei, VkFormat curFormat) const
{
	if (mIntermediateImage == VK_NULL_HANDLE)
	{
		return true;
	}

	if (mWidth != Owner.GetSwapchainWidth() || mHeight != Owner.GetSwapchainHeight())
	{
		return true;
	}

	if (mFormat != Owner.GetSwapchainFormat())
	{
		return true;
	}

	return false;
}

bool FVdjmVkIntermediateStage::EnsureResource(FVdjmAndroidEncoderBackendVulkan& backend,
	const FVdjmVkSubmitFrameInfo& frameInfo)
{
	if (!NeedRecreate(Owner, FrameInfo))
	{
		return true;
	}

	Release();

	VkImageCreateInfo ImageInfo{};
	ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageInfo.imageType = VK_IMAGE_TYPE_2D;
	ImageInfo.format = Owner.GetSwapchainFormat();
	ImageInfo.extent.width = Owner.GetSwapchainWidth();
	ImageInfo.extent.height = Owner.GetSwapchainHeight();
	ImageInfo.extent.depth = 1;
	ImageInfo.mipLevels = 1;
	ImageInfo.arrayLayers = 1;
	ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageInfo.usage =
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkResult Result = vkCreateImage(Owner.GetVkDevice(), &ImageInfo, nullptr, &mIntermediateImage);
	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("EnsureResource: vkCreateImage failed"));
		return false;
	}

	// 여기에서 memory requirements / allocate / bind
	// 그리고 image view 생성

	mWidth = Owner.GetSwapchainWidth();
	mHeight = Owner.GetSwapchainHeight();
	mFormat = Owner.GetSwapchainFormat();

	return true;
}

bool FVdjmVkIntermediateStage::RecordPrepareAndCopy(FVdjmAndroidEncoderBackendVulkan& owner,
	const FVdjmVkSubmitFrameInfo& frameInfo)
{
	VkCommandBuffer Cmd = Owner.GetCommandBuffer();
	if (Cmd == VK_NULL_HANDLE)
	{
		return false;
	}

	// 1. src -> TRANSFER_SRC_OPTIMAL
	// 2. intermediate -> TRANSFER_DST_OPTIMAL
	// 3. vkCmdBlitImage or vkCmdCopyImage
	// 4. intermediate -> TRANSFER_SRC_OPTIMAL or SHADER_READ layout

	// 포맷/해상도 같으면 vkCmdCopyImage
	// 다르면 vkCmdBlitImage

	if (FrameInfo.SrcWidth == Owner.GetSwapchainWidth() &&
		FrameInfo.SrcHeight == Owner.GetSwapchainHeight() &&
		FrameInfo.SrcFormat == Owner.GetSwapchainFormat())
	{
		VkImageCopy Region{};
		Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.srcSubresource.layerCount = 1;
		Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.dstSubresource.layerCount = 1;
		Region.extent.width = FrameInfo.SrcWidth;
		Region.extent.height = FrameInfo.SrcHeight;
		Region.extent.depth = 1;

		vkCmdCopyImage(
			Cmd,
			FrameInfo.SrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mIntermediateImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &Region);
	}
	else
	{
		VkImageBlit Blit{};
		Blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Blit.srcSubresource.layerCount = 1;
		Blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Blit.dstSubresource.layerCount = 1;
		Blit.srcOffsets[1] = { (int32)FrameInfo.SrcWidth, (int32)FrameInfo.SrcHeight, 1 };
		Blit.dstOffsets[1] = { (int32)Owner.GetSwapchainWidth(), (int32)Owner.GetSwapchainHeight(), 1 };

		vkCmdBlitImage(
			Cmd,
			FrameInfo.SrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mIntermediateImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &Blit,
			VK_FILTER_LINEAR);
	}

	return true;
}

bool FVdjmVkSurfaceSubmitter::Submit(FVdjmAndroidEncoderBackendVulkan& owner, double timeStampSec)
{
	VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	VkSubmitInfo SubmitInfo{};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.waitSemaphoreCount = 1;
	SubmitInfo.pWaitSemaphores = &Owner.mAcquireSemaphore;
	SubmitInfo.pWaitDstStageMask = &WaitStage;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &Owner.mCommandBuffer;
	SubmitInfo.signalSemaphoreCount = 1;
	SubmitInfo.pSignalSemaphores = &Owner.mRenderCompleteSemaphore;

	vkResetFences(Owner.mDevice, 1, &Owner.mSubmitFence);

	VkResult Result = vkQueueSubmit(Owner.mQueue, 1, &SubmitInfo, Owner.mSubmitFence);
	if (Result != VK_SUCCESS)
	{
		return false;
	}

	Result = vkWaitForFences(Owner.mDevice, 1, &Owner.mSubmitFence, VK_TRUE, UINT64_MAX);
	if (Result != VK_SUCCESS)
	{
		return false;
	}

	VkPresentInfoKHR PresentInfo{};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pWaitSemaphores = &Owner.mRenderCompleteSemaphore;
	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = &Owner.mCodecSwapchain;
	PresentInfo.pImageIndices = &Owner.mCurrentSwapchainImageIndex;

	Result = vkQueuePresentKHR(Owner.mQueue, &PresentInfo);
	return (Result == VK_SUCCESS);
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
	VkResult Result = vkAcquireNextImageKHR(
		mDevice,
		mCodecSwapchain,
		UINT64_MAX,
		mAcquireSemaphore,
		VK_NULL_HANDLE,
		&mCurrentSwapchainImageIndex);

	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderEncoder, Error, TEXT("vkAcquireNextImageKHR failed: %d"), (int32)Result);
		return false;
	}

	VkImage DstImage = mSwapchainImages[mCurrentSwapchainImageIndex];
	VkCommandBuffer Cmd = mCommandBuffer;

	// begin cmd
	// source or intermediate -> dst swapchain image
	// dst -> PRESENT_SRC_KHR

	VkImage FinalSrc = mIntermediateStage.HasIntermediateImage()
		? mIntermediateStage.GetImage()
		: srcImage;

	// barrier / copy / barrier 기록

	if (!mSubmitter.Submit(*this, timeStampSec))
	{
		return false;
	}

	return true;
}
#endif