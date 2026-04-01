// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"

#if PLATFORM_ANDROID || defined(__RESHARPER__)

#include "vulkan_android.h"
#include "IVulkanDynamicRHI.h"


bool VdjmVkUtil::WaitAndAcquireFrame(const FVdjmVkRecoderHandles& vkHandles,
	FVdjmVkCodecInputSurfaceState& surfaceState, FVdjmVkFrameResources& frameResources)
{
	if (!vkHandles.IsValid() || !surfaceState.IsValid() || !frameResources.IsValid())
	{
		return false;
	}

	const VkDevice vkDevice = vkHandles.GetVkDevice();

	if (!VdjmVkUtil::CheckVkResult(
		vkWaitForFences(vkDevice, 1, &frameResources.SubmitFence, VK_TRUE, UINT64_MAX),
		TEXT("VdjmWaitAndAcquireFrame.vkWaitForFences")))
	{
		return false;
	}

	uint32 imageIndex = UINT32_MAX;
	const VkResult acquireResult = vkAcquireNextImageKHR(
		vkDevice,
		surfaceState.GetSwapchain(),
		UINT64_MAX,
		frameResources.ImageAcquiredSemaphore,
		VK_NULL_HANDLE,
		&imageIndex);

	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("VdjmWaitAndAcquireFrame - swapchain out of date."));
		return false;
	}

	if (acquireResult == VK_SUBOPTIMAL_KHR)
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("VdjmWaitAndAcquireFrame - swapchain suboptimal."));
	}
	else if (acquireResult != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("VdjmWaitAndAcquireFrame - vkAcquireNextImageKHR failed. VkResult=%d"),
			(int32)acquireResult);
		return false;
	}

	surfaceState.SetCurrentSwapchainImageIndex(imageIndex);

	if (!VdjmVkUtil::CheckVkResult(
		vkResetCommandPool(vkDevice, frameResources.CommandPool, 0),
		TEXT("VdjmWaitAndAcquireFrame.vkResetCommandPool")))
	{
		return false;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (!VdjmVkUtil::CheckVkResult(
		vkBeginCommandBuffer(frameResources.CommandBuffer, &beginInfo),
		TEXT("VdjmWaitAndAcquireFrame.vkBeginCommandBuffer")))
	{
		return false;
	}

	return true;
}

bool VdjmVkUtil::SubmitAndPresentFrame(const FVdjmVkRecoderHandles& vkHandles,
	FVdjmVkCodecInputSurfaceState& surfaceState, FVdjmVkFrameResources& frameResources)
{
	if (not vkHandles.IsValid() || not surfaceState.IsValid() || not frameResources.IsValid())
	{
		return false;
	}

	if (not VdjmVkUtil::CheckVkResult(
		vkEndCommandBuffer(frameResources.CommandBuffer),
		TEXT("VdjmSubmitAndPresentFrame.vkEndCommandBuffer")))
	{
		return false;
	}

	if (not VdjmVkUtil::CheckVkResult(
		vkResetFences(vkHandles.GetVkDevice(), 1, &frameResources.SubmitFence),
		TEXT("VdjmSubmitAndPresentFrame.vkResetFences")))
	{
		return false;
	}

	const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frameResources.ImageAcquiredSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &frameResources.CommandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &frameResources.RenderCompleteSemaphore;

	if (not VdjmVkUtil::CheckVkResult(
		vkQueueSubmit(vkHandles.GetGraphicsQueue(), 1, &submitInfo, frameResources.SubmitFence),
		TEXT("VdjmSubmitAndPresentFrame.vkQueueSubmit")))
	{
		return false;
	}

	const uint32 imageIndex = surfaceState.GetCurrentSwapchainImageIndex();
	if (imageIndex == UINT32_MAX)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("VdjmSubmitAndPresentFrame - invalid current swapchain image index."));
		return false;
	}

	VkSwapchainKHR swapchain = surfaceState.GetSwapchain();

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frameResources.RenderCompleteSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	const VkResult presentResult = vkQueuePresentKHR(vkHandles.GetGraphicsQueue(), &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("VdjmSubmitAndPresentFrame - swapchain out of date during present."));
		return false;
	}

	if (presentResult == VK_SUBOPTIMAL_KHR)
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("VdjmSubmitAndPresentFrame - swapchain suboptimal during present."));
	}
	else if (presentResult != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("VdjmSubmitAndPresentFrame - vkQueuePresentKHR failed. VkResult=%d"),
			(int32)presentResult);
		return false;
	}

	surfaceState.AdvanceFrame();
	return true;
}

bool VdjmVkUtil::RecordBackBufferToIntermediateToSwapchain(VkCommandBuffer commandBuffer,
	const FVdjmVkCodecInputSurfaceState& surfaceState, FVdjmVkIntermediateState& intermediateState, VkImage sourceImage,
	uint32 sourceWidth, uint32 sourceHeight)
{
	if (commandBuffer == VK_NULL_HANDLE || sourceImage == VK_NULL_HANDLE || sourceWidth == 0 || sourceHeight == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("VdjmRecordBackBufferToIntermediateToSwapchain - invalid source args."));
		return false;
	}

	if (not surfaceState.IsValid() || not intermediateState.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("VdjmRecordBackBufferToIntermediateToSwapchain - invalid surface/intermediate state."));
		return false;
	}

	const uint32 swapchainImageIndex = surfaceState.GetCurrentSwapchainImageIndex();
	const TArray<VkImage>& swapchainImages = surfaceState.GetSwapchainImages();
	if (!swapchainImages.IsValidIndex((int32)swapchainImageIndex))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("VdjmRecordBackBufferToIntermediateToSwapchain - invalid swapchain image index: %u"),
			swapchainImageIndex);
		return false;
	}

	const VkImage swapchainImage = swapchainImages[(int32)swapchainImageIndex];
	const VkImage intermediateImage = intermediateState.GetImage();
	const VkExtent2D swapchainExtent = surfaceState.GetExtent();

	if (swapchainImage == VK_NULL_HANDLE || intermediateImage == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("VdjmRecordBackBufferToIntermediateToSwapchain - invalid intermediate/swapchain image."));
		return false;
	}

	auto GetStageMaskForLayout = [](VkImageLayout layout) -> VkPipelineStageFlags
	{
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		default:
			return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}
	};

	auto GetAccessMaskForLayout = [](VkImageLayout layout) -> VkAccessFlags
	{
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		default:
			return 0;
		}
	};

	
	const VkImageLayout sourceOriginalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	const VkImageLayout intermediateOriginalLayout = intermediateState.GetCurrentLayout();
	const VkImageLayout swapchainOriginalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VdjmVkUtil::AddImageBarrier(commandBuffer, sourceImage, sourceOriginalLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	VdjmVkUtil::AddImageBarrier(commandBuffer, intermediateImage, intermediateOriginalLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkImageBlit sourceToIntermediate{};
	sourceToIntermediate.srcSubresource.aspectMask = VdjmVkUtil::GColorAspect;
	sourceToIntermediate.srcSubresource.mipLevel = 0;
	sourceToIntermediate.srcSubresource.baseArrayLayer = 0;
	sourceToIntermediate.srcSubresource.layerCount = 1;
	sourceToIntermediate.srcOffsets[0] = { 0, 0, 0 };
	sourceToIntermediate.srcOffsets[1] = { (int32)sourceWidth, (int32)sourceHeight, 1 };

	sourceToIntermediate.dstSubresource.aspectMask = VdjmVkUtil::GColorAspect;
	sourceToIntermediate.dstSubresource.mipLevel = 0;
	sourceToIntermediate.dstSubresource.baseArrayLayer = 0;
	sourceToIntermediate.dstSubresource.layerCount = 1;
	sourceToIntermediate.dstOffsets[0] = { 0, 0, 0 };
	sourceToIntermediate.dstOffsets[1] = { (int32)intermediateState.GetWidth(), (int32)intermediateState.GetHeight(), 1 };

	vkCmdBlitImage(
		commandBuffer,
		sourceImage,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		intermediateImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&sourceToIntermediate,
		VK_FILTER_NEAREST);

	VdjmVkUtil::AddImageBarrier(commandBuffer,intermediateImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	VdjmVkUtil::AddImageBarrier(commandBuffer,swapchainImage, swapchainOriginalLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkImageCopy intermediateToSwapchain{};
	intermediateToSwapchain.srcSubresource.aspectMask = VdjmVkUtil::GColorAspect;
	intermediateToSwapchain.srcSubresource.mipLevel = 0;
	intermediateToSwapchain.srcSubresource.baseArrayLayer = 0;
	intermediateToSwapchain.srcSubresource.layerCount = 1;
	intermediateToSwapchain.srcOffset = { 0, 0, 0 };

	intermediateToSwapchain.dstSubresource.aspectMask = VdjmVkUtil::GColorAspect;
	intermediateToSwapchain.dstSubresource.mipLevel = 0;
	intermediateToSwapchain.dstSubresource.baseArrayLayer = 0;
	intermediateToSwapchain.dstSubresource.layerCount = 1;
	intermediateToSwapchain.dstOffset = { 0, 0, 0 };

	intermediateToSwapchain.extent.width = swapchainExtent.width;
	intermediateToSwapchain.extent.height = swapchainExtent.height;
	intermediateToSwapchain.extent.depth = 1;

	vkCmdCopyImage(
		commandBuffer,
		intermediateImage,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		swapchainImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&intermediateToSwapchain);

	VdjmVkUtil::AddImageBarrier(commandBuffer,swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	VdjmVkUtil::AddImageBarrier(commandBuffer,sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sourceOriginalLayout);

	intermediateState.SetCurrentLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	return true;
}

VkAccessFlags VdjmVkUtil::GetAccessFlagsForLayout(VkImageLayout layout)
{
	switch (layout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		return VK_ACCESS_TRANSFER_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return VK_ACCESS_TRANSFER_WRITE_BIT;
	case VK_IMAGE_LAYOUT_UNDEFINED:
		return 0;
	case VK_IMAGE_LAYOUT_GENERAL:
		return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return VK_ACCESS_SHADER_READ_BIT;
	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		return VK_ACCESS_HOST_WRITE_BIT;
	default:
		return 0;
	}
}

void VdjmVkUtil::AddImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
                                 VkImageLayout newLayout)
{
	if (oldLayout == newLayout)
	{
		return;
	}

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VdjmVkUtil::GColorAspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VdjmVkUtil::GetAccessFlagsForLayout(oldLayout);
	barrier.dstAccessMask = VdjmVkUtil::GetAccessFlagsForLayout(newLayout);

	vkCmdPipelineBarrier(
		commandBuffer,
		VdjmVkUtil::GetAccessFlagsForLayout(oldLayout),
		VdjmVkUtil::GetAccessFlagsForLayout(newLayout),
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
}

bool FVdjmVkRecoderHandles::InitializeHandles()
{
	Clear();

	if (GDynamicRHI == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkRecoderHandles::Initialize - GDynamicRHI is null."));
		return false;
	}

	if (GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::Vulkan)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkRecoderHandles::Initialize - Current RHI is not Vulkan."));
		return false;
	}

	IVulkanDynamicRHI* VulkanRHI = GetDynamicRHI<IVulkanDynamicRHI>();
	if (VulkanRHI == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkRecoderHandles::Initialize - Failed to get IVulkanDynamicRHI."));
		return false;
	}

	return InitializeFromDynamicRHI(VulkanRHI);
}

bool FVdjmVkRecoderHandles::EnsureInitialized()
{
	//	not initialized or invalid handle이 있으면 재초기화 시도. 그래도 안되면 false
	return NeedReInit() == true ? InitializeHandles() : true;
}

bool FVdjmVkRecoderHandles::InitializeFromDynamicRHI(IVulkanDynamicRHI* inVulkanRHI)
{
	Clear();

	if (inVulkanRHI == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("VdjmLog{ %s : Input IVulkanDynamicRHI is null.}"), *FString(__FUNCTION__));
		return false;
	}
	
	mVulkanRHI = inVulkanRHI;
	mVkInstance = inVulkanRHI->RHIGetVkInstance();
	mVkPhysicalDevice = inVulkanRHI->RHIGetVkPhysicalDevice();
	mVkDevice = inVulkanRHI->RHIGetVkDevice();
	mGraphicsQueue = inVulkanRHI->RHIGetGraphicsVkQueue();
	mGraphicsQueueIndex = inVulkanRHI->RHIGetGraphicsQueueIndex();
	mGraphicsQueueFamilyIndex = inVulkanRHI->RHIGetGraphicsQueueFamilyIndex();
	
	if (!IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmVkRecoderHandles::InitializeFromDynamicRHI - Failed to initialize Vulkan handles. One or more handles are invalid."));

		Clear();
		return false;
	}

	UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmVkRecoderHandles::InitializeFromDynamicRHI - Successfully initialized Vulkan handles: %s"), *ToString());

	return true;
}

bool FVdjmVkCodecInputSurfaceState::InitializeSurfaceState(const FVdjmVkRecoderHandles& vkHandles,
	ANativeWindow* inputWindow, const FVdjmAndroidEncoderConfigure& config)
{
	if (!vkHandles.IsValid() || inputWindow == nullptr || !config.IsValidateEncoderArguments())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkCodecInputSurfaceState::InitializeSurfaceState - invalid args."));
		return false;
	}
	ReleaseSurfaceState(vkHandles);
	if (not CreateSurface(vkHandles, inputWindow))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkCodecInputSurfaceState::InitializeSurfaceState - CreateSurface failed."));
		return false;
	}

	if (not CreateSwapchain(vkHandles, config))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkCodecInputSurfaceState::InitializeSurfaceState - CreateSwapchain failed."));
		ReleaseSurfaceState(vkHandles);
		return false;
	}

	if (not CreatePerFrameResources(vkHandles))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkCodecInputSurfaceState::InitializeSurfaceState - CreatePerFrameResources failed."));
		ReleaseSurfaceState(vkHandles);
		return false;
	}

	UE_LOG(LogVdjmRecorderCore, Log,
		TEXT("FVdjmVkCodecInputSurfaceState::InitializeSurfaceState - success. Format=%d Extent=%ux%u Images=%d"),
		(int32)mSurfaceFormat.format,
		mExtent.width,
		mExtent.height,
		mSwapchainImages.Num());

	return true;
}

void FVdjmVkCodecInputSurfaceState::ReleaseSurfaceState(const FVdjmVkRecoderHandles& vkHandles)
{
	if (vkHandles.IsValid())
	{
		ReleasePerFrameResources(vkHandles.GetVkDevice());
		ReleaseSwapchain(vkHandles.GetVkDevice());
		ReleaseSurface(vkHandles.GetVkInstance());
	}

	Clear();
}

bool FVdjmVkCodecInputSurfaceState::CreateSurface(const FVdjmVkRecoderHandles& vkHandles, ANativeWindow* inputWindow)
{
	if (!vkHandles.IsValid() || inputWindow == nullptr)
	{
		return false;
	}

	VkAndroidSurfaceCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	createInfo.window = inputWindow;

	return VdjmVkUtil::CheckVkResult(
		vkCreateAndroidSurfaceKHR(vkHandles.GetVkInstance(), &createInfo, nullptr, &mSurface),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSurface"));
}

bool FVdjmVkCodecInputSurfaceState::CreateSwapchain(const FVdjmVkRecoderHandles& vkHandles,
	const FVdjmAndroidEncoderConfigure& config)
{
	if (not vkHandles.IsValid() || mSurface == VK_NULL_HANDLE)
	{
		return false;
	}

	VkBool32 bSurfaceSupported = VK_FALSE;
	if (not VdjmVkUtil::CheckVkResult(
		vkGetPhysicalDeviceSurfaceSupportKHR(
			vkHandles.GetVkPhysicalDevice(),
			vkHandles.GetGraphicsQueueFamilyIndex(),
			mSurface,
			&bSurfaceSupported),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.SurfaceSupport")))
	{
		return false;
	}

	if (bSurfaceSupported != VK_TRUE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain - graphics queue family does not support surface."));
		return false;
	}

	VkSurfaceCapabilitiesKHR caps{};
	if (not VdjmVkUtil::CheckVkResult(
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkHandles.GetVkPhysicalDevice(), mSurface, &caps),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.SurfaceCaps")))
	{
		return false;
	}

	uint32 formatCount = 0;
	if (not VdjmVkUtil::CheckVkResult(
		vkGetPhysicalDeviceSurfaceFormatsKHR(vkHandles.GetVkPhysicalDevice(), mSurface, &formatCount, nullptr),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.SurfaceFormats.Count")))
	{
		return false;
	}

	if (formatCount == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain - no surface formats."));
		return false;
	}

	TArray<VkSurfaceFormatKHR> surfaceFormats;
	surfaceFormats.SetNum(formatCount);
	if (not VdjmVkUtil::CheckVkResult(
		vkGetPhysicalDeviceSurfaceFormatsKHR(vkHandles.GetVkPhysicalDevice(), mSurface, &formatCount, surfaceFormats.GetData()),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.SurfaceFormats")))
	{
		return false;
	}

	mSurfaceFormat = VdjmVkUtil::ChooseSurfaceFormat(surfaceFormats);
	mExtent = VdjmVkUtil::ChooseExtent(caps, (uint32)config.VideoWidth, (uint32)config.VideoHeight);
	mPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	uint32 desiredImageCount = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && desiredImageCount > caps.maxImageCount)
	{
		desiredImageCount = caps.maxImageCount;
	}
	mMinImageCount = desiredImageCount;

	VkImageUsageFlags usageFlags = 0;
	if (caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
	{
		usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	{
		usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
	{
		usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	if (usageFlags == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain - no supported usage flags for destination image."));
		return false;
	}

	VkSwapchainCreateInfoKHR swapchainInfo{};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = mSurface;
	swapchainInfo.minImageCount = desiredImageCount;
	swapchainInfo.imageFormat = mSurfaceFormat.format;
	swapchainInfo.imageColorSpace = mSurfaceFormat.colorSpace;
	swapchainInfo.imageExtent = mExtent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = usageFlags;
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform = caps.currentTransform;
	swapchainInfo.compositeAlpha = VdjmVkUtil::ChooseCompositeAlpha(caps.supportedCompositeAlpha);
	swapchainInfo.presentMode = mPresentMode;
	swapchainInfo.clipped = VK_TRUE;
	swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

	if (not VdjmVkUtil::CheckVkResult(
		vkCreateSwapchainKHR(vkHandles.GetVkDevice(), &swapchainInfo, nullptr, &mSwapchain),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.Create")))
	{
		return false;
	}

	uint32 imageCount = 0;
	if (not VdjmVkUtil::CheckVkResult(
		vkGetSwapchainImagesKHR(vkHandles.GetVkDevice(), mSwapchain, &imageCount, nullptr),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.GetImages.Count")))
	{
		return false;
	}

	if (imageCount == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain - swapchain image count is zero."));
		return false;
	}

	mSwapchainImages.SetNum(imageCount);
	if (not VdjmVkUtil::CheckVkResult(
		vkGetSwapchainImagesKHR(vkHandles.GetVkDevice(), mSwapchain, &imageCount, mSwapchainImages.GetData()),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.GetImages")))
	{
		return false;
	}

	mSwapchainImageViews.Reset();
	mSwapchainImageViews.Reserve(imageCount);

	for (VkImage image : mSwapchainImages)
	{
		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = image;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = mSurfaceFormat.format;
		imageViewInfo.subresourceRange.aspectMask = VdjmVkUtil::GColorAspect;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView = VK_NULL_HANDLE;
		if (not VdjmVkUtil::CheckVkResult(
			vkCreateImageView(vkHandles.GetVkDevice(), &imageViewInfo, nullptr, &imageView),
			TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.CreateImageView")))
		{
			return false;
		}

		mSwapchainImageViews.Add(imageView);
	}

	return true;
}

bool FVdjmVkCodecInputSurfaceState::CreatePerFrameResources(const FVdjmVkRecoderHandles& vkHandles)
{
	if (!vkHandles.IsValid() || mSwapchainImages.Num() <= 0)
	{
		return false;
	}

	const int32 frameCount = mSwapchainImages.Num();
	mFrames.SetNum(frameCount);

	for (FVdjmVkFrameResources& frame : mFrames)
	{
		VkCommandPoolCreateInfo commandPoolInfo{};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolInfo.queueFamilyIndex = vkHandles.GetGraphicsQueueFamilyIndex();

		if (not VdjmVkUtil::CheckVkResult(
			vkCreateCommandPool(vkHandles.GetVkDevice(), &commandPoolInfo, nullptr, &frame.CommandPool),
			TEXT("FVdjmVkCodecInputSurfaceState::CreatePerFrameResources.CommandPool")))
		{
			return false;
		}

		VkCommandBufferAllocateInfo commandBufferInfo{};
		commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferInfo.commandPool = frame.CommandPool;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferInfo.commandBufferCount = 1;

		if (not VdjmVkUtil::CheckVkResult(
			vkAllocateCommandBuffers(vkHandles.GetVkDevice(), &commandBufferInfo, &frame.CommandBuffer),
			TEXT("FVdjmVkCodecInputSurfaceState::CreatePerFrameResources.CommandBuffer")))
		{
			return false;
		}

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (not VdjmVkUtil::CheckVkResult(
			vkCreateFence(vkHandles.GetVkDevice(), &fenceInfo, nullptr, &frame.SubmitFence),
			TEXT("FVdjmVkCodecInputSurfaceState::CreatePerFrameResources.Fence")))
		{
			return false;
		}

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (not VdjmVkUtil::CheckVkResult(
			vkCreateSemaphore(vkHandles.GetVkDevice(), &semaphoreInfo, nullptr, &frame.ImageAcquiredSemaphore),
			TEXT("FVdjmVkCodecInputSurfaceState::CreatePerFrameResources.ImageAcquiredSemaphore")))
		{
			return false;
		}

		if (not VdjmVkUtil::CheckVkResult(
			vkCreateSemaphore(vkHandles.GetVkDevice(), &semaphoreInfo, nullptr, &frame.RenderCompleteSemaphore),
			TEXT("FVdjmVkCodecInputSurfaceState::CreatePerFrameResources.RenderCompleteSemaphore")))
		{
			return false;
		}
	}

	return true;
}

void FVdjmVkCodecInputSurfaceState::ReleasePerFrameResources(VkDevice vkDevice)
{
	if (vkDevice == VK_NULL_HANDLE)
	{
		mFrames.Reset();
		return;
	}

	for (FVdjmVkFrameResources& Frame : mFrames)
	{
		if (Frame.SubmitFence != VK_NULL_HANDLE)
		{
			vkDestroyFence(vkDevice, Frame.SubmitFence, nullptr);
			Frame.SubmitFence = VK_NULL_HANDLE;
		}

		if (Frame.ImageAcquiredSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(vkDevice, Frame.ImageAcquiredSemaphore, nullptr);
			Frame.ImageAcquiredSemaphore = VK_NULL_HANDLE;
		}

		if (Frame.RenderCompleteSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(vkDevice, Frame.RenderCompleteSemaphore, nullptr);
			Frame.RenderCompleteSemaphore = VK_NULL_HANDLE;
		}

		if (Frame.CommandPool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(vkDevice, Frame.CommandPool, nullptr);
			Frame.CommandPool = VK_NULL_HANDLE;
			Frame.CommandBuffer = VK_NULL_HANDLE;
		}
	}

	mFrames.Reset();
}

void FVdjmVkCodecInputSurfaceState::ReleaseSwapchain(VkDevice vkDevice)
{
	if (vkDevice == VK_NULL_HANDLE)
	{
		mSwapchainImageViews.Reset();
		mSwapchainImages.Reset();
		mSwapchain = VK_NULL_HANDLE;
		return;
	}

	for (VkImageView ImageView : mSwapchainImageViews)
	{
		if (ImageView != VK_NULL_HANDLE)
		{
			vkDestroyImageView(vkDevice, ImageView, nullptr);
		}
	}

	mSwapchainImageViews.Reset();
	mSwapchainImages.Reset();

	if (mSwapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(vkDevice, mSwapchain, nullptr);
		mSwapchain = VK_NULL_HANDLE;
	}
}

void FVdjmVkCodecInputSurfaceState::ReleaseSurface(VkInstance vkInstance)
{
	if (vkInstance != VK_NULL_HANDLE && mSurface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(vkInstance, mSurface, nullptr);
	}

	mSurface = VK_NULL_HANDLE;
}

bool FVdjmVkIntermediateState::EnsureCreated(const FVdjmVkRecoderHandles& vkHandles, VkFormat format, uint32 width,
	uint32 height, VkImageUsageFlags extraUsageFlags)
{
	if (!vkHandles.IsValid() || format == VK_FORMAT_UNDEFINED || width == 0 || height == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkIntermediateState::EnsureCreated - invalid args."));
		return false;
	}

	if (Matches(format, width, height))
	{
		return true;
	}

	Release(vkHandles);
	return CreateImage(vkHandles, format, width, height, extraUsageFlags);
}

bool FVdjmVkIntermediateState::CreateImage(const FVdjmVkRecoderHandles& vkHandles, VkFormat format, uint32 width,
	uint32 height, VkImageUsageFlags extraUsageFlags)
{
	if (!vkHandles.IsValid())
	{
		return false;
	}

	VkImageUsageFlags usageFlags =
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		extraUsageFlags;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = usageFlags;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if (not VdjmVkUtil::CheckVkResult(
		vkCreateImage(vkHandles.GetVkDevice(), &imageInfo, nullptr, &mImage),
		TEXT("FVdjmVkIntermediateState::CreateImage.Image")))
	{
		return false;
	}

	VkMemoryRequirements memoryRequirements{};
	vkGetImageMemoryRequirements(vkHandles.GetVkDevice(), mImage, &memoryRequirements);

	const uint32 memoryTypeIndex = FindMemoryTypeIndex(
		vkHandles,
		memoryRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (memoryTypeIndex == UINT32_MAX)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmVkIntermediateState::CreateImage - failed to find memory type index."));
		Release(vkHandles);
		return false;
	}

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memoryRequirements.size;
	allocInfo.memoryTypeIndex = memoryTypeIndex;

	if (not VdjmVkUtil::CheckVkResult(
		vkAllocateMemory(vkHandles.GetVkDevice(), &allocInfo, nullptr, &mMemory),
		TEXT("FVdjmVkIntermediateState::CreateImage.Memory")))
	{
		Release(vkHandles);
		return false;
	}

	if (not VdjmVkUtil::CheckVkResult(
		vkBindImageMemory(vkHandles.GetVkDevice(), mImage, mMemory, 0),
		TEXT("FVdjmVkIntermediateState::CreateImage.BindMemory")))
	{
		Release(vkHandles);
		return false;
	}

	VkImageViewCreateInfo imageViewInfo{};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = mImage;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = format;
	imageViewInfo.subresourceRange.aspectMask = VdjmVkUtil::GColorAspect;
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = 1;

	if (not VdjmVkUtil::CheckVkResult(
		vkCreateImageView(vkHandles.GetVkDevice(), &imageViewInfo, nullptr, &mImageView),
		TEXT("FVdjmVkIntermediateState::CreateImage.ImageView")))
	{
		Release(vkHandles);
		return false;
	}

	mFormat = format;
	mWidth = width;
	mHeight = height;
	mUsageFlags = usageFlags;
	mCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	UE_LOG(LogVdjmRecorderCore, Log,
		TEXT("FVdjmVkIntermediateState::CreateImage - success. Format=%d Extent=%ux%u"),
		(int32)mFormat,
		mWidth,
		mHeight);

	return true;
}

uint32 FVdjmVkIntermediateState::FindMemoryTypeIndex(const FVdjmVkRecoderHandles& vkHandles, uint32 typeBits,
	VkMemoryPropertyFlags requiredFlags) const
{
	VkPhysicalDeviceMemoryProperties memoryProperties{};
	vkGetPhysicalDeviceMemoryProperties(vkHandles.GetVkPhysicalDevice(), &memoryProperties);

	for (uint32 i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		const bool bTypeMatched = (typeBits & (1u << i)) != 0;
		const bool bFlagsMatched =
			(memoryProperties.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags;

		if (bTypeMatched && bFlagsMatched)
		{
			return i;
		}
	}

	return UINT32_MAX;
}

void FVdjmVkIntermediateState::Release(const FVdjmVkRecoderHandles& vkHandles)
{
	if (vkHandles.IsValid())
	{
		const VkDevice vkDevice = vkHandles.GetVkDevice();

		if (mImageView != VK_NULL_HANDLE)
		{
			vkDestroyImageView(vkDevice, mImageView, nullptr);
			mImageView = VK_NULL_HANDLE;
		}

		if (mImage != VK_NULL_HANDLE)
		{
			vkDestroyImage(vkDevice, mImage, nullptr);
			mImage = VK_NULL_HANDLE;
		}

		if (mMemory != VK_NULL_HANDLE)
		{
			vkFreeMemory(vkDevice, mMemory, nullptr);
			mMemory = VK_NULL_HANDLE;
		}
	}

	Clear();
}

FVdjmAndroidEncoderBackendVulkan::FVdjmAndroidEncoderBackendVulkan()
{}

bool FVdjmAndroidEncoderBackendVulkan::Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow)
{
	if (not config.IsValidateEncoderArguments() || inputWindow == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - Invalid encoder configuration or input window is null."));
		return false;
	}
	
	if (mInputWindow != nullptr && mInputWindow != inputWindow)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - Replacing existing input window. Releasing previous window."));
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
	}
	
	mConfig = config;
	mInputWindow = inputWindow;
	ANativeWindow_acquire(mInputWindow);
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - Acquired input window for Vulkan encoder backend."));
	mVkHandles.Clear();
	
	if (not mVkHandles.EnsureInitialized())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("VdjmLog{ %s : failed to ensure Vulkan handles are initialized}"), *FString(__FUNCTION__));
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
		return false;
	}
	
	mInitialized = true;
	mStarted = false;
	mPaused = false;
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmAndroidEncoderBackendVulkan::Init - Successfully initialized Vulkan encoder backend with config: %s"), *mConfig.ToString());
	
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::Start()
{
	if (!mInitialized || mInputWindow == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Start - Encoder backend is not properly initialized or input window is null. Cannot start encoder backend."));
		return false;
	}
	if (mStarted)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Start - Encoder backend is already started. Ignoring redundant start call."));
		return true;
	}
	
	
	if (not mVkHandles.EnsureInitialized())
	{
		mVkHandles.Clear();
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Start - Failed to ensure Vulkan handles are initialized. Cannot start encoder backend."));
		return false;
	}
	
	/**
	 *	surface, intermediate resources는 start에서 초기화하고 stop에서 해제하는걸로. init은 단순히 config와 window만 저장하는걸로.
	 *	이렇게 하면 start/stop 반복되는 상황에서도 surface와 intermediate resources가 매번 재생성되는걸 방지할 수 있어서 성능적으로 유리할듯. 물론 start/stop 반복되는 상황이 자주 발생할지는 모르겠지만 일단은 이렇게 해보는걸로.
	 */
	mCodecInputSurfaceState.ReleaseSurfaceState(mVkHandles);
	mIntermediateState.Release(mVkHandles);
	
	if (not mCodecInputSurfaceState.InitializeSurfaceState(mVkHandles, mInputWindow, mConfig))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Start - failed to initialize codec input surface state."));
		return false;
	}

	const VkExtent2D extent = mCodecInputSurfaceState.GetExtent();
	const VkFormat format = mCodecInputSurfaceState.GetSurfaceFormat().format;

	if (not mIntermediateState.EnsureCreated(mVkHandles, format, extent.width, extent.height))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Start - failed to create intermediate image."));
		mCodecInputSurfaceState.ReleaseSurfaceState(mVkHandles);
		return false;
	}
	
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmAndroidEncoderBackendVulkan::Start - Successfully ensured Vulkan handles are initialized. Starting encoder backend."));
	
	return mStarted = true;
}
void FVdjmAndroidEncoderBackendVulkan::Stop()
{
	if (mVkHandles.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmAndroidEncoderBackendVulkan::Stop - Released Vulkan resources and stopped encoder backend."));
		vkDeviceWaitIdle(mVkHandles.GetVkDevice());
		mIntermediateState.Release(mVkHandles);
		mCodecInputSurfaceState.ReleaseSurfaceState(mVkHandles);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmAndroidEncoderBackendVulkan::Stop - Vulkan handles are not valid. Skipping resource release."));
		mIntermediateState.Clear();
		mCodecInputSurfaceState.Clear();
	}
	mStarted = false;
	mPaused = false;
}
void FVdjmAndroidEncoderBackendVulkan::Terminate()
{
	Stop();

	if (mInputWindow != nullptr)
	{
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
	}
	mVkHandles.Clear();
	mInitialized = false;
}

bool FVdjmAndroidEncoderBackendVulkan::IsRunnable() const
{
	return mInitialized && mStarted && !mPaused && mInputWindow != nullptr;
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	(void)RHICmdList;
	
	if (!IsRunnable() || !srcTexture.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - backend not runnable or srcTexture invalid."));
		return false;
	}

	if (!mVkHandles.IsValid() || !mCodecInputSurfaceState.IsValid() || !mIntermediateState.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - Vulkan state is invalid."));
		return false;
	}

	IVulkanDynamicRHI* VulkanRHI = mVkHandles.GetVulkanRHI();
	if (VulkanRHI == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - VulkanRHI is null."));
		return false;
	}

	FRHITexture* sourceTextureRHI = srcTexture.GetReference();
	if (sourceTextureRHI == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - sourceTextureRHI is null."));
		return false;
	}

	const VkImage sourceImage = VulkanRHI->RHIGetVkImage(sourceTextureRHI);
	if (sourceImage == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - failed to get source VkImage."));
		return false;
	}

	const FIntVector sourceSize = srcTexture->GetSizeXYZ();
	if (sourceSize.X <= 0 || sourceSize.Y <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - invalid source size. Size=(%d,%d,%d)"),
			sourceSize.X, sourceSize.Y, sourceSize.Z);
		return false;
	}

	const VkExtent2D surfaceExtent = mCodecInputSurfaceState.GetExtent();
	const VkFormat surfaceFormat = mCodecInputSurfaceState.GetSurfaceFormat().format;

	if (!mIntermediateState.EnsureCreated(mVkHandles, surfaceFormat, surfaceExtent.width, surfaceExtent.height))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - failed to ensure intermediate image."));
		return false;
	}

	FVdjmVkFrameResources* frameResources = mCodecInputSurfaceState.GetCurrentFrameResources();
	if (frameResources == nullptr || !frameResources->IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - current frame resources are invalid."));
		return false;
	}

	if (!VdjmVkUtil::WaitAndAcquireFrame(mVkHandles, mCodecInputSurfaceState, *frameResources))
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - failed to acquire frame. timestamp=%f"),
			timeStampSec);
		return false;
	}

	if (not VdjmVkUtil::RecordBackBufferToIntermediateToSwapchain(
		frameResources->CommandBuffer,
		mCodecInputSurfaceState,
		mIntermediateState,
		sourceImage,
		(uint32)sourceSize.X,
		(uint32)sourceSize.Y))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - failed to record copy commands. timestamp=%f"),
			timeStampSec);
		return false;
	}

	if (not VdjmVkUtil::SubmitAndPresentFrame(mVkHandles, mCodecInputSurfaceState, *frameResources))
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - failed to submit/present frame. timestamp=%f"),
			timeStampSec);
		return false;
	}

	return true;
}

#endif
