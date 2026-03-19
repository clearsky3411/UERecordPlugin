// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"

#include "Runtime/VulkanRHI/Public/IVulkanDynamicRHI.h"
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

	outInfo.bFormatMatchesSwapchain = (outInfo.SrcFormat == mOwnerBackend->GetSwapchainFormat());
	outInfo.bExtentMatchesSwapchain =
		(outInfo.SrcWidth == mOwnerBackend->GetSwapchainWidth() &&
		 outInfo.SrcHeight == mOwnerBackend->GetSwapchainHeight());

	outInfo.bCanDirectCopy = outInfo.bFormatMatchesSwapchain && outInfo.bExtentMatchesSwapchain;
	outInfo.bNeedsIntermediate = !outInfo.bCanDirectCopy;

	return true;
}

bool FVdjmVkIntermediateStage::NeedRecreate(const FVdjmVkSubmitFrameInfo& frameInfo, uint32 curWid, uint32 curhei, VkFormat curFormat) const
{
	if (mOwnerBackend == nullptr)
	{
		return true;
	}
	FVdjmAndroidEncoderBackendVulkan& Owner = *mOwnerBackend;
	
	if (Owner.IsValidIntermediateImage())
	{
		return true;
	}

	if (IsValidIntermediateSwapchainResolutions())
	{
		return true;
	}

	if (IsValidIntermediateFormat())
	{
		return true;
	}

	return false;
}

bool FVdjmVkIntermediateStage::EnsureResource(FVdjmAndroidEncoderBackendVulkan& backend,
	const FVdjmVkSubmitFrameInfo& frameInfo)
{
	const FVdjmVkSubmitFrameInfo& frameInfo, uint32 curWid, uint32 curhei, VkFormat curFormat
	
	if (!NeedRecreate(frameInfo, FrameInfo))
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
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("EnsureResource: vkCreateImage failed"));
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

bool FVdjmVkIntermediateStage::IsValidIntermediateSwapchainResolutions(FVdjmAndroidEncoderBackendVulkan*  backend) const
{
	FVdjmAndroidEncoderBackendVulkan* target = backend;
	if (target == nullptr)
	{
		target = mOwnerBackend;
		if (target == nullptr)
		{
			return false;
		}
	}
	FVdjmAndroidEncoderBackendVulkan& Owner = *target;
	return (mIntermediateState.IntermediateWidth == Owner.GetSwapchainWidth() && mIntermediateState.IntermediateHeight == Owner.GetSwapchainHeight());
}

bool FVdjmVkIntermediateStage::IsValidIntermediateFormat(FVdjmAndroidEncoderBackendVulkan*  backend) const
{
	FVdjmAndroidEncoderBackendVulkan* target = mOwnerBackend;
	if (target == nullptr)
	{
		target = backend;
		if (target == nullptr)
		{
			return false;
		}
	}
	FVdjmAndroidEncoderBackendVulkan& Owner = *target;
	return (mIntermediateState.IntermediateFormat == Owner.GetSwapchainFormat());
}

bool FVdjmVkSurfaceSubmitter::Submit(FVdjmAndroidEncoderBackendVulkan& owner, double timeStampSec)
{
	VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	VkSubmitInfo SubmitInfo{};
	if (mOwnerBackend == nullptr)
	{
		return false;
	}
	FVdjmAndroidEncoderBackendVulkan& Owner = *mOwnerBackend;
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.waitSemaphoreCount = 1;
	
	SubmitInfo.pWaitSemaphores = Owner.GetAcquireSemaphoreConst();
	SubmitInfo.pWaitDstStageMask = &WaitStage;
	SubmitInfo.commandBufferCount = 1;
	
	SubmitInfo.pCommandBuffers = Owner.GetCommandBufferConst();
	SubmitInfo.signalSemaphoreCount = 1;
	SubmitInfo.pSignalSemaphores = Owner.GetRenderCompleteSemaphoreConst();
	
	vkResetFences(Owner.GetVkDevice(), 1, Owner.GetSubmitFenceConst());
	
	VkResult Result = vkQueueSubmit(Owner.GetGraphicsQueue(), 1, &SubmitInfo, Owner.GetSubmitFence());
	
	if (Result != VK_SUCCESS)
	{
		return false;
	}

	Result = vkWaitForFences(Owner.GetVkDevice(), 1, Owner.GetSubmitFenceConst(), VK_TRUE, UINT64_MAX);
	if (Result != VK_SUCCESS)
	{
		return false;
	}
	
	VkPresentInfoKHR PresentInfo{};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pWaitSemaphores = Owner.GetRenderCompleteSemaphoreConst();
	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = Owner.GetCodecSwapchainConst();
	PresentInfo.pImageIndices = Owner.GetCurrentSwapchainImageIndexConst();

	Result = vkQueuePresentKHR(Owner.GetGraphicsQueue(), &PresentInfo);
	return (Result == VK_SUCCESS);
}

/*
 *	class FVdjmAndroidEncoderBackendVulkan : public FVdjmAndroidEncoderBackend
 */
FVdjmAndroidEncoderBackendVulkan::FVdjmAndroidEncoderBackendVulkan()
	: mAnalyzer(this), mIntermediateStage(this), mSurfaceSubmitter(this),
	mInitialized(false), mStarted(false), mPaused(false), mRuntimeReady(false)
{}

bool FVdjmAndroidEncoderBackendVulkan::Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow)
{
	if (not config.IsValidateEncoderArguments() )
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - Invalid encoder configuration."));
		return false;
	}
	if (inputWindow == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - inputWindow is null."));
		return false;
	}
	
	mConfig = config;
	mInputWindow = inputWindow;
	InitVkRuntimeContext();
	
	mInitialized = true;
	mStarted = false;
	mPaused = false;
	mRuntimeReady = false;
	
	return true;
}
bool FVdjmAndroidEncoderBackendVulkan::Start()
{
	
}

void FVdjmAndroidEncoderBackendVulkan::Stop()
{
	
}

void FVdjmAndroidEncoderBackendVulkan::Terminate()
{
	
}

bool FVdjmAndroidEncoderBackendVulkan::IsRunnable()
{
	
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	
}

bool FVdjmAndroidEncoderBackendVulkan::InitVkRuntimeContext()
{
	
}

bool FVdjmAndroidEncoderBackendVulkan::EnsureRuntimeReady()
{
	
}

bool FVdjmAndroidEncoderBackendVulkan::TryExtractNativeVkImage(const FTextureRHIRef& srcTexture,VkImage& outImage) const
{
	
}

bool FVdjmAndroidEncoderBackendVulkan::SubmitTextureToCodecSurface(FRHICommandList& RHICmdList,
	const FTextureRHIRef& srcTexture, VkImage srcImage, double timeStampSec)
{

}
#endif