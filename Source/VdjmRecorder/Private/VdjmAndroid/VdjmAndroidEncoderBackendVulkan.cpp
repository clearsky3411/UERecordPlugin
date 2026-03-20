// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"

#include "Runtime/VulkanRHI/Public/IVulkanDynamicRHI.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
bool FVdjmVkSubProcInputAnalyzer::Analyze(const FTextureRHIRef& srcTexture, FVdjmVkSubmitFrameInfo& outInfo) const
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

bool FVdjmVkSubProcIntermediateStage::NeedRecreate(const FVdjmVkSubmitFrameInfo& frameInfo, uint32 curWid, uint32 curhei, VkFormat curFormat) const
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

bool FVdjmVkSubProcIntermediateStage::EnsureResource(FVdjmAndroidEncoderBackendVulkan& backend,
	const FVdjmVkSubmitFrameInfo& frameInfo)
{
	
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

bool FVdjmVkSubProcIntermediateStage::RecordPrepareAndCopy(FVdjmAndroidEncoderBackendVulkan& owner,
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

bool FVdjmVkSubProcIntermediateStage::IsValidIntermediateSwapchainResolutions(FVdjmAndroidEncoderBackendVulkan*  backend) const
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

bool FVdjmVkSubProcIntermediateStage::IsValidIntermediateFormat(FVdjmAndroidEncoderBackendVulkan*  backend) const
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

bool FVdjmVkSubProcSurfaceSubmitter::Submit(FVdjmAndroidEncoderBackendVulkan& owner, double timeStampSec)
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

uint32 FVdjmAndroidEncoderBackendVulkan::FindMemoryType(VkPhysicalDevice physicalDevice, uint32 typeFilter,VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties MemProps{};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &MemProps);

	for (uint32 i = 0; i < MemProps.memoryTypeCount; ++i)
	{
		const bool bTypeMatch = (typeFilter & (1u << i)) != 0;
		const bool bFlagsMatch = (MemProps.memoryTypes[i].propertyFlags & properties) == properties;
		if (bTypeMatch && bFlagsMatch)
		{
			return i;
		}
	}

	return UINT32_MAX;
}

void FVdjmAndroidEncoderBackendVulkan::TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier Barrier{};
	Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	Barrier.oldLayout = oldLayout;
	Barrier.newLayout = newLayout;
	Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.image = image;
	Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Barrier.subresourceRange.baseMipLevel = 0;
	Barrier.subresourceRange.levelCount = 1;
	Barrier.subresourceRange.baseArrayLayer = 0;
	Barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags SrcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkPipelineStageFlags DstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

	if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		Barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		SrcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		DstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		Barrier.srcAccessMask = 0;
		Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		{
			SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}
		else
		{
			SrcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}
		DstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		Barrier.dstAccessMask = 0;
		SrcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		DstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		SrcStage,
		DstStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &Barrier);
}

VkSurfaceFormatKHR FVdjmAndroidEncoderBackendVulkan::ChooseSurfaceFormat(const FVdjmVkRuntimeContext& runtimeContext,const TArray<VkSurfaceFormatKHR>& availableFormats)
{
	const VkFormat PreferredFormats[] =
	{
		runtimeContext.VulkanRHI->RHIGetSwapChainVkFormat(PF_R8G8B8A8),
		runtimeContext.VulkanRHI->RHIGetSwapChainVkFormat(PF_B8G8R8A8),
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8A8_UNORM
	};
	for (VkFormat Want : PreferredFormats)
	{
		if (Want == VK_FORMAT_UNDEFINED)
		{
			continue;
		}

		for (const VkSurfaceFormatKHR& F : availableFormats)
		{
			if (F.format == Want && F.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return F;
			}
		}
	}

	return availableFormats.Num() > 0 ? availableFormats[0] : VkSurfaceFormatKHR{ VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
}

VkPresentModeKHR FVdjmAndroidEncoderBackendVulkan::ChoosePresentMode(const TArray<VkPresentModeKHR>& modes)
{
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkCompositeAlphaFlagBitsKHR FVdjmAndroidEncoderBackendVulkan::ChooseCompositeAlpha(VkCompositeAlphaFlagsKHR flags)
{
	if (flags & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
	{
		return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}
	if (flags & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
	{
		return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
	}
	if (flags & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
	{
		return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
	}
	return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
}

VkExtent2D FVdjmAndroidEncoderBackendVulkan::ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32 desiredWid,	uint32 desiredHei)
{
	if (caps.currentExtent.width != UINT32_MAX)
	{
		return caps.currentExtent;
	}

	VkExtent2D extentResult{};
	extentResult.width = FMath::Clamp(desiredWid, caps.minImageExtent.width, caps.maxImageExtent.width);
	extentResult.height = FMath::Clamp(desiredHei, caps.minImageExtent.height, caps.maxImageExtent.height);
	return extentResult;
}

bool FVdjmAndroidEncoderBackendVulkan::InitVkRuntimeContext()
{
	if (mVkRuntime.bInitialized)
	{
		return true;
	}

	IVulkanDynamicRHI* VulkanRHI = GetIVulkanDynamicRHI();
	if (VulkanRHI == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("InitVkRuntimeContext: GetIVulkanDynamicRHI failed"));
		return false;
	}

	if (VulkanRHI->GetInterfaceType() != ERHIInterfaceType::Vulkan)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("InitVkRuntimeContext: RHI is not Vulkan"));
		return false;
	}

	mVkRuntime.VulkanRHI = VulkanRHI;
	mVkRuntime.VkInstance = VulkanRHI->RHIGetVkInstance();
	mVkRuntime.VkPhysicalDevice = VulkanRHI->RHIGetVkPhysicalDevice();
	mVkRuntime.VkDevice = VulkanRHI->RHIGetVkDevice();
	mVkRuntime.GraphicsQueue = VulkanRHI->RHIGetGraphicsVkQueue();
	mVkRuntime.GraphicsQueueFamilyIndex = VulkanRHI->RHIGetGraphicsQueueFamilyIndex();
	
	if (!mVkRuntime.IsInitValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("InitVkRuntimeContext: invalid Vulkan handles"));
		mVkRuntime.Clear();
		return false;
	}

	mVkRuntime.bInitialized = true;
	return true;
}

void FVdjmAndroidEncoderBackendVulkan::ReleaseRecordSessionVkResources()
{
	if (mVkRuntime.VkDevice != VK_NULL_HANDLE)
	{
		mIntermediateStage.Release(mVkRuntime);

		for (VkSemaphore Sem : mVkRecordSession.PresentWaitSemaphores)
		{
			if (Sem != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(mVkRuntime.VkDevice, Sem, nullptr);
			}
		}
		mVkRecordSession.PresentWaitSemaphores.Reset();

		if (mVkRecordSession.AcquireSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(mVkRuntime.VkDevice, mVkRecordSession.AcquireSemaphore, nullptr);
		}

		if (mVkRecordSession.SubmitFence != VK_NULL_HANDLE)
		{
			vkDestroyFence(mVkRuntime.VkDevice, mVkRecordSession.SubmitFence, nullptr);
		}

		for (VkImageView View : mVkRecordSession.SwapchainImageViews)
		{
			if (View != VK_NULL_HANDLE)
			{
				vkDestroyImageView(mVkRuntime.VkDevice, View, nullptr);
			}
		}
		mVkRecordSession.SwapchainImageViews.Reset();

		if (mVkRecordSession.CommandPool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(mVkRuntime.VkDevice, mVkRecordSession.CommandPool, nullptr);
		}

		if (mVkRecordSession.CodecSwapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(mVkRuntime.VkDevice, mVkRecordSession.CodecSwapchain, nullptr);
		}
	}

	if (mVkRecordSession.CodecSurface != VK_NULL_HANDLE && mVkRuntime.VkInstance != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(mVkRuntime.VkInstance, mVkRecordSession.CodecSurface, nullptr);
	}

	mVkRecordSession.Clear();
}

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
	if (!InitVkRuntimeContext())
	{
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
	if (!mInitialized && !Init())
	{
		return false;
	}

	if (!EnsureRuntimeReady())
	{
		return false;
	}

	mPaused = false;
	mStarted = true;
	return true;
}

void FVdjmAndroidEncoderBackendVulkan::Stop()
{
	mStarted = false;
	mPaused = false;

	if (mVkRuntime.Device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(mVkRuntime.Device);
	}

	ReleaseRecordSessionVkResources();
}

void FVdjmAndroidEncoderBackendVulkan::Terminate()
{
	Stop();

	// Runtime handles are borrowed from UE. Do not destroy them.
	mVkRuntime.Clear();
	mInitialized = false;
}

bool FVdjmAndroidEncoderBackendVulkan::IsRunnable()
{
	if (!mInitialized)
	{
		UE_LOG(LogVdjmRecorderEncoder, Warning, TEXT("Vulkan backend: not initialized"));
		return false;
	}
	if (!mStarted || mPaused)
	{
		return false;
	}
	if (mInputWindow == nullptr)
	{
		UE_LOG(LogVdjmRecorderEncoder, Warning, TEXT("Vulkan backend: input window is null"));
		return false;
	}
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
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