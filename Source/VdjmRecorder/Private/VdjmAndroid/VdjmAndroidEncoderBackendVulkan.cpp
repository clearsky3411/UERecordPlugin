// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"

#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "vulkan_android.h"
#include "IVulkanDynamicRHI.h"

namespace
{
	static constexpr VkImageAspectFlags GColorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
}

bool FVdjmVkSubProcInputAnalyzer::Analyze(const FTextureRHIRef& srcTexture, FVdjmVkSubmitFrameInfo& outInfo) 
{
	//	src image,format,extent,layout 분석
	
	outInfo.Clear();

	if (mOwnerBackend == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Analyze: owner backend is null"));
		return false;
	}

	if (!srcTexture.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Analyze: srcTexture is invalid"));
		return false;
	}

	if (!mOwnerBackend->TryExtractNativeVkImage(srcTexture, outInfo.SrcImage))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Analyze: failed to extract native VkImage"));
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
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Analyze: unsupported UE pixel format = %d"), (int32)srcTexture->GetFormat());
		return false;
	}
	

	return true;
}
/*
 * Intermediate Stage
 */
bool FVdjmVkSubProcIntermediateStage::NeedRecreate(const FVdjmVkSubmitFrameInfo& frameInfo, uint32 curWid, uint32 curhei, VkFormat curFormat) const
{
	return mIntermediateState.NeedsRecreate(curWid, curhei, curFormat);
}

void FVdjmVkSubProcIntermediateStage::Release(FVdjmAndroidEncoderBackendVulkan& owner)
{
	VkDevice device = owner.GetVkDevice();
	if (device == VK_NULL_HANDLE)
	{
		mIntermediateState.Clear();
		return;
	}

	if (mIntermediateState.IntermediateView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, mIntermediateState.IntermediateView, nullptr);
		mIntermediateState.IntermediateView = VK_NULL_HANDLE;
	}

	if (mIntermediateState.IntermediateImage != VK_NULL_HANDLE)
	{
		vkDestroyImage(device, mIntermediateState.IntermediateImage, nullptr);
		mIntermediateState.IntermediateImage = VK_NULL_HANDLE;
	}

	if (mIntermediateState.IntermediateMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, mIntermediateState.IntermediateMemory, nullptr);
		mIntermediateState.IntermediateMemory = VK_NULL_HANDLE;
	}

	mIntermediateState.Clear();
}

bool FVdjmVkSubProcIntermediateStage::EnsureResource(FVdjmAndroidEncoderBackendVulkan& backend,const FVdjmVkSubmitFrameInfo& frameInfo)
{
	if (!frameInfo.bNeedsIntermediate)
	{
		return true;
	}

	const uint32 targetWidth = backend.GetSwapchainWidth();
	const uint32 targetHeight = backend.GetSwapchainHeight();
	const VkFormat targetFormat = backend.GetSwapchainFormat();

	if (!NeedRecreate(frameInfo, targetWidth, targetHeight, targetFormat))
	{
		return true;
	}

	Release(backend);

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = targetFormat;
	imageInfo.extent.width = targetWidth;
	imageInfo.extent.height = targetHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImage intermediateImage = VK_NULL_HANDLE;
	VkResult result = vkCreateImage(backend.GetVkDevice(), &imageInfo, nullptr, &intermediateImage);
	if (result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("EnsureResource: vkCreateImage failed (%d)"), (int32)result);
		return false;
	}

	VkMemoryRequirements memReq{};
	vkGetImageMemoryRequirements(backend.GetVkDevice(), intermediateImage, &memReq);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = FVdjmAndroidEncoderBackendVulkan::FindMemoryType(
		backend.GetVkPhysicalDevice(),
		memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (allocInfo.memoryTypeIndex == UINT32_MAX)
	{
		vkDestroyImage(backend.GetVkDevice(), intermediateImage, nullptr);
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("EnsureResource: no suitable device local memory type"));
		return false;
	}

	VkDeviceMemory intermediateMemory = VK_NULL_HANDLE;
	result = vkAllocateMemory(backend.GetVkDevice(), &allocInfo, nullptr, &intermediateMemory);
	if (result != VK_SUCCESS)
	{
		vkDestroyImage(backend.GetVkDevice(), intermediateImage, nullptr);
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("EnsureResource: vkAllocateMemory failed (%d)"), (int32)result);
		return false;
	}

	result = vkBindImageMemory(backend.GetVkDevice(), intermediateImage, intermediateMemory, 0);
	if (result != VK_SUCCESS)
	{
		vkFreeMemory(backend.GetVkDevice(), intermediateMemory, nullptr);
		vkDestroyImage(backend.GetVkDevice(), intermediateImage, nullptr);
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("EnsureResource: vkBindImageMemory failed (%d)"), (int32)result);
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = intermediateImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = targetFormat;
	viewInfo.subresourceRange.aspectMask = GColorAspect;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView intermediateView = VK_NULL_HANDLE;
	result = vkCreateImageView(backend.GetVkDevice(), &viewInfo, nullptr, &intermediateView);
	if (result != VK_SUCCESS)
	{
		vkFreeMemory(backend.GetVkDevice(), intermediateMemory, nullptr);
		vkDestroyImage(backend.GetVkDevice(), intermediateImage, nullptr);
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("EnsureResource: vkCreateImageView failed (%d)"), (int32)result);
		return false;
	}

	mIntermediateState.IntermediateImage = intermediateImage;
	mIntermediateState.IntermediateMemory = intermediateMemory;
	mIntermediateState.IntermediateView = intermediateView;
	mIntermediateState.IntermediateFormat = targetFormat;
	mIntermediateState.IntermediateWidth = targetWidth;
	mIntermediateState.IntermediateHeight = targetHeight;

	return true;
}

bool FVdjmVkSubProcIntermediateStage::RecordPrepareAndCopy(FVdjmAndroidEncoderBackendVulkan& owner, const FVdjmVkSubmitFrameInfo& frameInfo, FVdjmVkFrameSubmitState& inOutFrameState)
{
	/*
	* 여기서는 intermediate가 필요하면

mIntermediateState 대신
FVdjmVkOwnedImageState mIntermediateImage

하나만
	 */
	inOutFrameState.FinalSrcImage = frameInfo.CacheEntry.SrcImage;

	if (!frameInfo.bNeedsIntermediate)
	{
		return true;
	}

	if (!mIntermediateState.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("RecordPrepareAndCopy: intermediate state is invalid"));
		return false;
	}

	VkCommandBuffer cmd = owner.GetCommandBuffer();
	if (cmd == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("RecordPrepareAndCopy: command buffer is null"));
		return false;
	}

	FVdjmAndroidEncoderBackendVulkan::TransitionImageLayout(
		cmd,
		frameInfo.CacheEntry.SrcImage,
		frameInfo.CacheEntry.SrcFormat,
		frameInfo.CacheEntry.SrcLayout,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	FVdjmAndroidEncoderBackendVulkan::TransitionImageLayout(
		cmd,
		mIntermediateState.IntermediateImage,
		mIntermediateState.IntermediateFormat,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	if (frameInfo.CacheEntry.SrcFormat == mIntermediateState.IntermediateFormat
		&& frameInfo.CacheEntry.SrcWidth == mIntermediateState.IntermediateWidth
		&& frameInfo.CacheEntry.SrcHeight == mIntermediateState.IntermediateHeight)
	{
		VkImageCopy region{};
		region.srcSubresource.aspectMask = GColorAspect;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = GColorAspect;
		region.dstSubresource.mipLevel = 0;
		region.dstSubresource.baseArrayLayer = 0;
		region.dstSubresource.layerCount = 1;
		region.extent.width = frameInfo.CacheEntry.SrcWidth;
		region.extent.height = frameInfo.CacheEntry.SrcHeight;
		region.extent.depth = 1;

		vkCmdCopyImage(
			cmd,
			frameInfo.CacheEntry.SrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mIntermediateState.IntermediateImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region);
	}
	else
	{
		VkImageBlit blit{};
		blit.srcSubresource.aspectMask = GColorAspect;
		blit.srcSubresource.mipLevel = 0;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstSubresource.aspectMask = GColorAspect;
		blit.dstSubresource.mipLevel = 0;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { (int32)frameInfo.CacheEntry.SrcWidth, (int32)frameInfo.CacheEntry.SrcHeight, 1 };
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { (int32)mIntermediateState.IntermediateWidth, (int32)mIntermediateState.IntermediateHeight, 1 };

		vkCmdBlitImage(
			cmd,
			frameInfo.CacheEntry.SrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mIntermediateState.IntermediateImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&blit,
			VK_FILTER_LINEAR);
	}

	FVdjmAndroidEncoderBackendVulkan::TransitionImageLayout(
		cmd,
		mIntermediateState.IntermediateImage,
		mIntermediateState.IntermediateFormat,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	FVdjmAndroidEncoderBackendVulkan::TransitionImageLayout(
		cmd,
		frameInfo.CacheEntry.SrcImage,
		frameInfo.CacheEntry.SrcFormat,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		frameInfo.CacheEntry.SrcLayout);

	inOutFrameState.FinalSrcImage = mIntermediateState.IntermediateImage;
	return true;
}

bool FVdjmVkSubProcIntermediateStage::IsValidIntermediateSwapchainResolutions(FVdjmAndroidEncoderBackendVulkan*  backend) const
{
	FVdjmAndroidEncoderBackendVulkan* target = backend ? backend : mOwnerBackend;
	if (target == nullptr)
	{
		return false;
	}

	return mIntermediateState.IntermediateWidth == target->GetSwapchainWidth()
		&& mIntermediateState.IntermediateHeight == target->GetSwapchainHeight();
}
bool FVdjmVkSubProcIntermediateStage::IsValidIntermediateFormat(FVdjmAndroidEncoderBackendVulkan* backend) const
{
	FVdjmAndroidEncoderBackendVulkan* target = backend ? backend : mOwnerBackend;
	if (target == nullptr)
	{
		return false;
	}

	return mIntermediateState.IntermediateFormat == target->GetSwapchainFormat();
}

bool FVdjmVkSubProcSurfaceSubmitter::Submit(FVdjmAndroidEncoderBackendVulkan& owner, const FVdjmVkFrameSubmitState& frameState, double timeStampSec)
{
	//	queue submit, presentation, 제출 완료 관리
	VkDevice device = owner.GetVkDevice();
	VkQueue queue = owner.GetGraphicsQueue();
	if (device == VK_NULL_HANDLE || queue == VK_NULL_HANDLE)
	{
		return false;
	}

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = owner.GetAcquireSemaphoreConst();
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = owner.GetCommandBufferConst();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = owner.GetRenderCompleteSemaphoreConst();

	VkResult result = vkResetFences(device, 1, owner.GetSubmitFenceConst());
	if (result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Submit: vkResetFences failed (%d)"), (int32)result);
		return false;
	}
	UE_LOG(LogVdjmRecorderCore, Warning,
		TEXT("Submit: Queue=%p Cmd=%p Fence=%p AcquireSem=%p RenderSem=%p ImageIndex=%u"),
		queue,
		owner.GetCommandBuffer(),
		owner.GetSubmitFence(),
		owner.GetAcquireSemaphore(),
		owner.GetRenderCompleteSemaphore(),
		frameState.AcquiredImageIndex);
	
	result = vkQueueSubmit(queue, 1, &submitInfo, owner.GetSubmitFence());
	if (result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Submit: vkQueueSubmit failed (%d)"), (int32)result);
		return false;
	}

	result = vkWaitForFences(device, 1, owner.GetSubmitFenceConst(), VK_TRUE, UINT64_MAX);
	if (result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Submit: vkWaitForFences failed (%d)"), (int32)result);
		return false;
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = owner.GetRenderCompleteSemaphoreConst();

	VkSwapchainKHR swapchain = owner.GetCodecSwapchain();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &frameState.AcquiredImageIndex;

	result = vkQueuePresentKHR(queue, &presentInfo);
	if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
	{
		return true;
	}

	UE_LOG(LogVdjmRecorderCore, Error, TEXT("Submit: vkQueuePresentKHR failed (%d)"), (int32)result);
	return false;
}

/*
 *	class FVdjmAndroidEncoderBackendVulkan : public FVdjmAndroidEncoderBackend
 */
/*
 * Backend helpers
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
bool FVdjmAndroidEncoderBackendVulkan::TransitionOwnedImage(
	VkCommandBuffer Cmd,
	FVdjmVkOwnedImageState& State,
	VkImageLayout NewLayout)
{
	if (!State.IsValid())
	{
		return false;
	}

	// 이미 원하는 layout이면 아무 것도 안 함
	if (State.CurrentLayout == NewLayout)
	{
		return true;
	}

	TransitionImageLayout(
		Cmd,
		State.Image,
		State.Format,
		State.CurrentLayout,
		NewLayout);

	State.CurrentLayout = NewLayout;
	return true;
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



FVdjmAndroidEncoderBackendVulkan::FVdjmAndroidEncoderBackendVulkan()
	: mAnalyzer(this), mIntermediateStage(this), mSurfaceSubmitter(this),
	mInitialized(false), mStarted(false), mPaused(false), mRuntimeReady(false)
{}

bool FVdjmAndroidEncoderBackendVulkan::Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow)
{
	//	config 저장, input window 저장, runtime handle 확보 및 초기화
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - start"));
	if (!config.IsValidateEncoderArguments())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - invalid config"));
		return false;
	}

	if (inputWindow == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - inputWindow is null"));
		return false;
	}

	if (!InitVkRuntimeContext())
	{
		return false;
	}

	if (mInputWindow != nullptr && mInputWindow != inputWindow)
	{
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
	}

	mConfig = config;
	mInputWindow = inputWindow;
	ANativeWindow_acquire(mInputWindow);
	
	mInitialized = true;
	mStarted = false;
	mPaused = false;
	mRuntimeReady = false;
	
	UE_LOG(LogVdjmRecorderCore, Log,
	TEXT("Vulkan Init: Path=%s W=%d H=%d Bitrate=%d FPS=%d Interval=%d Backend=%d Window=%p"),
		*mConfig.OutputFilePath,
		mConfig.VideoWidth,
		mConfig.VideoHeight,
		mConfig.VideoBitrate,
		mConfig.VideoFPS,
		mConfig.VideoIntervalSec,
		(int32)mConfig.GraphicBackend,
		mInputWindow);
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::InitVkRuntimeContext()
{
	//	runtime handle 확보 및 초기화
	if (mVkRuntime.bInitialized)
	{
		return true;
	}

	IVulkanDynamicRHI* vulkanRHI = GetIVulkanDynamicRHI();
	if (vulkanRHI == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("InitVkRuntimeContext: GetIVulkanDynamicRHI failed"));
		return false;
	}

	if (vulkanRHI->GetInterfaceType() != ERHIInterfaceType::Vulkan)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("InitVkRuntimeContext: RHI is not Vulkan"));
		return false;
	}

	mVkRuntime.VulkanRHI = vulkanRHI;
	mVkRuntime.VkInstance = vulkanRHI->RHIGetVkInstance();
	mVkRuntime.VkPhysicalDevice = vulkanRHI->RHIGetVkPhysicalDevice();
	mVkRuntime.VkDevice = vulkanRHI->RHIGetVkDevice();
	mVkRuntime.GraphicsQueue = vulkanRHI->RHIGetGraphicsVkQueue();
	mVkRuntime.GraphicsQueueFamilyIndex = vulkanRHI->RHIGetGraphicsQueueFamilyIndex();

	if (!mVkRuntime.IsInitValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("InitVkRuntimeContext: invalid Vulkan handles"));
		mVkRuntime.Clear();
		return false;
	}
	UE_LOG(LogVdjmRecorderCore, Warning,
		TEXT("InitVkRuntimeContext: Instance=%p PhysicalDevice=%p Device=%p Queue=%p GraphicsQueueFamilyIndex=%u"),
		mVkRuntime.VkInstance,
		mVkRuntime.VkPhysicalDevice,
		mVkRuntime.VkDevice,
		mVkRuntime.GraphicsQueue,
		mVkRuntime.GraphicsQueueFamilyIndex);
	mVkRuntime.bInitialized = true;
	return true;
}
void FVdjmAndroidEncoderBackendVulkan::ReleaseRecordSessionVkResources()
{
	if (mVkRuntime.VkDevice != VK_NULL_HANDLE)
	{
		mIntermediateStage.Release(*this);

		for (VkSemaphore Sem : mVkRecordSession.RenderCompleteSemaphores)
		{
			if (Sem != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(mVkRuntime.VkDevice, Sem, nullptr);
			}
		}
		mVkRecordSession.RenderCompleteSemaphores.Empty();

		if (mVkRecordSession.AcquireSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(mVkRuntime.VkDevice, mVkRecordSession.AcquireSemaphore, nullptr);
			mVkRecordSession.AcquireSemaphore = VK_NULL_HANDLE;
		}

		if (mVkRecordSession.SubmitFence != VK_NULL_HANDLE)
		{
			vkDestroyFence(mVkRuntime.VkDevice, mVkRecordSession.SubmitFence, nullptr);
			mVkRecordSession.SubmitFence = VK_NULL_HANDLE;
		}

		for (VkImageView view : mVkRecordSession.SwapchainImageViews)
		{
			if (view != VK_NULL_HANDLE)
			{
				vkDestroyImageView(mVkRuntime.VkDevice, view, nullptr);
			}
		}
		mVkRecordSession.SwapchainImageViews.Reset();

		if (mVkRecordSession.CommandPool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(mVkRuntime.VkDevice, mVkRecordSession.CommandPool, nullptr);
			mVkRecordSession.CommandPool = VK_NULL_HANDLE;
			mVkRecordSession.CommandBuffer = VK_NULL_HANDLE;
		}

		if (mVkRecordSession.CodecSwapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(mVkRuntime.VkDevice, mVkRecordSession.CodecSwapchain, nullptr);
			mVkRecordSession.CodecSwapchain = VK_NULL_HANDLE;
		}
	}

	if (mVkRecordSession.CodecSurface != VK_NULL_HANDLE && mVkRuntime.VkInstance != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(mVkRuntime.VkInstance, mVkRecordSession.CodecSurface, nullptr);
		mVkRecordSession.CodecSurface = VK_NULL_HANDLE;
	}

	mVkRecordSession.Clear();
	mRuntimeReady = false;
}



bool FVdjmAndroidEncoderBackendVulkan::Start()
{
	if (!mInitialized)
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

	if (mVkRuntime.VkDevice != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(mVkRuntime.VkDevice);
	}

	ReleaseRecordSessionVkResources();
}

void FVdjmAndroidEncoderBackendVulkan::Terminate()
{
	Stop();

	if (mInputWindow != nullptr)
	{
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
	}

	// borrowed handle이므로 destroy 금지
	mVkRuntime.Clear();
	mInitialized = false;
}

bool FVdjmAndroidEncoderBackendVulkan::IsRunnable()
{
	if (!mInitialized)
	{
		return false;
	}
	if (!mStarted || mPaused)
	{
		return false;
	}
	if (mInputWindow == nullptr)
	{
		return false;
	}
	if (!mVkRuntime.IsValid())
	{
		return false;
	}

	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	/*
	 * 매 프레임 사실확인이 필요함. 소유하려는게 아님.
	 */
}



bool FVdjmAndroidEncoderBackendVulkan::EnsureRuntimeReady()
{
	
}

bool FVdjmAndroidEncoderBackendVulkan::AcquireNextSwapchainImage(FVdjmVkFrameSubmitState& outFrameState)
{

}

bool FVdjmAndroidEncoderBackendVulkan::TryExtractNativeVkImage(const FTextureRHIRef& srcTexture,VkImage& outImage) const
{
	
}

bool FVdjmAndroidEncoderBackendVulkan::SubmitTextureToCodecSurface(const FVdjmVkFrameSubmitState& frameState)
{
	
	return true;
}
#endif