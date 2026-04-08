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

bool VdjmVkUtil::RecordBackBufferToIntermediateToSwapchain(
	VkCommandBuffer commandBuffer,
	FVdjmVkCodecInputSurfaceState& surfaceState,
	FVdjmVkIntermediateState& intermediateState,
	VkImage sourceImage,
	uint32 sourceWidth,
	uint32 sourceHeight)
{
	if (commandBuffer == VK_NULL_HANDLE || sourceImage == VK_NULL_HANDLE || sourceWidth == 0 || sourceHeight == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("VdjmRecordBackBufferToIntermediateToSwapchain - invalid source args."));
		return false;
	}

	if (!surfaceState.IsValid() || !intermediateState.IsValid())
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

	// OnBackBufferReadyToPresent()에서 전달되는 백버퍼를 입력으로 사용한다.
	// 본 경로는 백버퍼를 TRANSFER_SRC로 잠시 전환 후 다시 PRESENT로 되돌리는 것을 전제로 한다.
	const VkImageLayout sourceOriginalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	const VkImageLayout intermediateOriginalLayout = intermediateState.GetCurrentLayout();
	const VkImageLayout swapchainOriginalLayout = surfaceState.GetCurrentSwapchainImageLayout();

	VdjmVkUtil::AddImageBarrier(commandBuffer, sourceImage, sourceOriginalLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	VdjmVkUtil::AddImageBarrier(commandBuffer, intermediateImage, intermediateOriginalLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VdjmVkUtil::AddImageBarrier(commandBuffer, swapchainImage, swapchainOriginalLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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

	VdjmVkUtil::AddImageBarrier(commandBuffer, intermediateImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

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

	VdjmVkUtil::AddImageBarrier(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	VdjmVkUtil::AddImageBarrier(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sourceOriginalLayout);

	intermediateState.SetCurrentLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	surfaceState.SetCurrentSwapchainImageLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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

	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		return 0;

	default:
		return 0;
	}
}

VkPipelineStageFlags VdjmVkUtil::GetPipelineStageFlagsForLayout(VkImageLayout layout)
{
	switch (layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return VK_PIPELINE_STAGE_TRANSFER_BIT;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	case VK_IMAGE_LAYOUT_GENERAL:
		return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	default:
		return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}
}

void VdjmVkUtil::AddImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,VkImageLayout newLayout)
{
	if (commandBuffer == VK_NULL_HANDLE || image == VK_NULL_HANDLE)
	{
		return;
	}
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
		VdjmVkUtil::GetPipelineStageFlagsForLayout(oldLayout),
		VdjmVkUtil::GetPipelineStageFlagsForLayout(newLayout),
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
}

FString VdjmVkUtil::ConvertVulkanResultString(VkResult result)
{
	switch (result)
	{
	case VK_SUCCESS: return TEXT("VK_SUCCESS");
	case VK_NOT_READY: return TEXT("VK_NOT_READY");
	case VK_TIMEOUT: return TEXT("VK_TIMEOUT");
	case VK_EVENT_SET: return TEXT("VK_EVENT_SET");
	case VK_EVENT_RESET: return TEXT("VK_EVENT_RESET");
	case VK_INCOMPLETE: return TEXT("VK_INCOMPLETE");
	case VK_ERROR_OUT_OF_HOST_MEMORY: return TEXT("VK_ERROR_OUT_OF_HOST_MEMORY");
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return TEXT("VK_ERROR_OUT_OF_DEVICE_MEMORY");
	case VK_ERROR_INITIALIZATION_FAILED: return TEXT("VK_ERROR_INITIALIZATION_FAILED");
	case VK_ERROR_DEVICE_LOST: return TEXT("VK_ERROR_DEVICE_LOST");
	case VK_ERROR_MEMORY_MAP_FAILED: return TEXT("VK_ERROR_MEMORY_MAP_FAILED");
	case VK_ERROR_LAYER_NOT_PRESENT: return TEXT("VK_ERROR_LAYER_NOT_PRESENT");
	case VK_ERROR_EXTENSION_NOT_PRESENT: return TEXT("VK_ERROR_EXTENSION_NOT_PRESENT");
	case VK_ERROR_FEATURE_NOT_PRESENT: return TEXT("VK_ERROR_FEATURE_NOT_PRESENT");
	case VK_ERROR_INCOMPATIBLE_DRIVER: return TEXT("VK_ERROR_INCOMPATIBLE_DRIVER");
	case VK_ERROR_TOO_MANY_OBJECTS: return TEXT("VK_ERROR_TOO_MANY_OBJECTS");
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return TEXT("VK_ERROR_FORMAT_NOT_SUPPORTED");
	case VK_ERROR_FRAGMENTED_POOL: return TEXT("VK_ERROR_FRAGMENTED_POOL");
	case VK_ERROR_UNKNOWN: return TEXT("VK_ERROR_UNKNOWN");
	case VK_ERROR_OUT_OF_POOL_MEMORY: return TEXT("VK_ERROR_OUT_OF_POOL_MEMORY");
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return TEXT("VK_ERROR_INVALID_EXTERNAL_HANDLE");
	case VK_ERROR_FRAGMENTATION: return TEXT("VK_ERROR_FRAGMENTATION");
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return TEXT("VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS");
	case VK_PIPELINE_COMPILE_REQUIRED: return TEXT("VK_PIPELINE_COMPILE_REQUIRED");
	case VK_ERROR_NOT_PERMITTED: return TEXT("VK_ERROR_NOT_PERMITTED");
	case VK_ERROR_SURFACE_LOST_KHR: return TEXT("VK_ERROR_SURFACE_LOST_KHR");
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return TEXT("VK_ERROR_NATIVE_WINDOW_IN_USE_KHR");
	case VK_SUBOPTIMAL_KHR: return TEXT("VK_SUBOPTIMAL_KHR");
	case VK_ERROR_OUT_OF_DATE_KHR: return TEXT("VK_ERROR_OUT_OF_DATE_KHR");
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return TEXT("VK_ERROR_INCOMPATIBLE_DISPLAY_KHR");
	case VK_ERROR_VALIDATION_FAILED_EXT: return TEXT("VK_ERROR_VALIDATION_FAILED_EXT");
	case VK_ERROR_INVALID_SHADER_NV: return TEXT("VK_ERROR_INVALID_SHADER_NV");
	case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return TEXT("VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR");
	case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return TEXT("VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR");
	case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return TEXT("VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR");
	case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return TEXT("VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR");
	case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return TEXT("VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR");
	case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return TEXT("VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR");
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return TEXT("VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT");
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return TEXT("VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT");
	case VK_THREAD_IDLE_KHR: return TEXT("VK_THREAD_IDLE_KHR");
	case VK_THREAD_DONE_KHR: return TEXT("VK_THREAD_DONE_KHR");
	case VK_OPERATION_DEFERRED_KHR: return TEXT("VK_OPERATION_DEFERRED_KHR");
	case VK_OPERATION_NOT_DEFERRED_KHR: return TEXT("VK_OPERATION_NOT_DEFERRED_KHR");
	case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: return TEXT("VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR");
	case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return TEXT("VK_ERROR_COMPRESSION_EXHAUSTED_EXT");
	case VK_INCOMPATIBLE_SHADER_BINARY_EXT: return TEXT("VK_INCOMPATIBLE_SHADER_BINARY_EXT");
	case VK_PIPELINE_BINARY_MISSING_KHR: return TEXT("VK_PIPELINE_BINARY_MISSING_KHR");
	case VK_ERROR_NOT_ENOUGH_SPACE_KHR: return TEXT("VK_ERROR_NOT_ENOUGH_SPACE_KHR");
	case VK_RESULT_MAX_ENUM: return TEXT("VK_RESULT_MAX_ENUM");
	default: 
		// 정의되지 않은 값이 들어왔을 경우 (알 수 없는 에러 코드)
		return FString::Printf(TEXT("UNKNOWN_VK_RESULT (%d)"), (int32)result);
	}
	
}

FString VdjmVkUtil::ConvertPixelFormatToString(EPixelFormat format)
{
	switch (format) {
	case PF_Unknown: return TEXT("Unknown");
	case PF_A32B32G32R32F: return TEXT("A32B32G32R32F");
	case PF_B8G8R8A8: return TEXT("B8G8R8A8");
	case PF_G8: return TEXT("G8");
	case PF_G16: return TEXT("G16");
	case PF_DXT1: return TEXT("DXT1");
	case PF_DXT3: return TEXT("DXT3");
	case PF_DXT5: return TEXT("DXT5");
	case PF_UYVY: return TEXT("UYVY");
	case PF_FloatRGB: return TEXT("FloatRGB");
	case PF_FloatRGBA: return TEXT("FloatRGBA");
	case PF_DepthStencil: return TEXT("DepthStencil");
	case PF_ShadowDepth: return TEXT("ShadowDepth");
	case PF_R32_FLOAT: return TEXT("R32_FLOAT");
	case PF_G16R16: return TEXT("G16R16");
	case PF_G16R16F: return TEXT("G16R16F");
	case PF_G16R16F_FILTER: return TEXT("G16R16F_FILTER");
	case PF_G32R32F: return TEXT("G32R32F");
	case PF_A2B10G10R10: return TEXT("A2B10G10R10");
	case PF_A16B16G16R16: return TEXT("A16B16G16R16");
	case PF_D24: return TEXT("D24");
	case PF_R16F: return TEXT("R16F");
	case PF_R16F_FILTER: return TEXT("R16F_FILTER");
	case PF_BC5: return TEXT("BC5");
	case PF_V8U8: return TEXT("V8U8");
	case PF_A1: return TEXT("A1");
	case PF_FloatR11G11B10: return TEXT("FloatR11G11B10");
	case PF_A8: return TEXT("A8");
	case PF_R32_UINT: return TEXT("R32_UINT");
	case PF_R32_SINT: return TEXT("R32_SINT");
	case PF_PVRTC2: return TEXT("PVRTC2");
	case PF_PVRTC4: return TEXT("PVRTC4");
	case PF_R16_UINT: return TEXT("R16_UINT");
	case PF_R16_SINT: return TEXT("R16_SINT");
	case PF_R16G16B16A16_UINT: return TEXT("R16G16B16A16_UINT");
	case PF_R16G16B16A16_SINT: return TEXT("R16G16B16A16_SINT");
	case PF_R5G6B5_UNORM: return TEXT("R5G6B5_UNORM");
	case PF_R8G8B8A8: return TEXT("R8G8B8A8");
	case PF_A8R8G8B8: return TEXT("A8R8G8B8");
	case PF_BC4: return TEXT("BC4");
	case PF_R8G8: return TEXT("R8G8");
	case PF_ATC_RGB: return TEXT("ATC_RGB");
	case PF_ATC_RGBA_E: return TEXT("ATC_RGBA_E");
	case PF_ATC_RGBA_I: return TEXT("ATC_RGBA_I");
	case PF_X24_G8: return TEXT("X24_G8");
	case PF_ETC1: return TEXT("ETC1");
	case PF_ETC2_RGB: return TEXT("ETC2_RGB");
	case PF_ETC2_RGBA: return TEXT("ETC2_RGBA");
	case PF_R32G32B32A32_UINT: return TEXT("R32G32B32A32_UINT");
	case PF_R16G16_UINT: return TEXT("R16G16_UINT");
	case PF_ASTC_4x4: return TEXT("ASTC_4x4");
	case PF_ASTC_6x6: return TEXT("ASTC_6x6");
	case PF_ASTC_8x8: return TEXT("ASTC_8x8");
	case PF_ASTC_10x10: return TEXT("ASTC_10x10");
	case PF_ASTC_12x12: return TEXT("ASTC_12x12");
	case PF_BC6H: return TEXT("BC6H");
	case PF_BC7: return TEXT("BC7");
	case PF_R8_UINT: return TEXT("R8_UINT");
	case PF_L8: return TEXT("L8");
	case PF_XGXR8: return TEXT("XGXR8");
	case PF_R8G8B8A8_UINT: return TEXT("R8G8B8A8_UINT");
	case PF_R8G8B8A8_SNORM: return TEXT("R8G8B8A8_SNORM");
	case PF_R16G16B16A16_UNORM: return TEXT("R16G16B16A16_UNORM");
	case PF_R16G16B16A16_SNORM: return TEXT("R16G16B16A16_SNORM");
	case PF_PLATFORM_HDR_0: return TEXT("PLATFORM_HDR_0");
	case PF_PLATFORM_HDR_1: return TEXT("PLATFORM_HDR_1");
	case PF_PLATFORM_HDR_2: return TEXT("PLATFORM_HDR_2");
	case PF_NV12: return TEXT("NV12");
	case PF_R32G32_UINT: return TEXT("R32G32_UINT");
	case PF_ETC2_R11_EAC: return TEXT("ETC2_R11_EAC");
	case PF_ETC2_RG11_EAC: return TEXT("ETC2_RG11_EAC");
	case PF_R8: return TEXT("R8");
	case PF_B5G5R5A1_UNORM: return TEXT("B5G5R5A1_UNORM");
	case PF_ASTC_4x4_HDR: return TEXT("ASTC_4x4_HDR");
	case PF_ASTC_6x6_HDR: return TEXT("ASTC_6x6_HDR");
	case PF_ASTC_8x8_HDR: return TEXT("ASTC_8x8_HDR");
	case PF_ASTC_10x10_HDR: return TEXT("ASTC_10x10_HDR");
	case PF_ASTC_12x12_HDR: return TEXT("ASTC_12x12_HDR");
	case PF_G16R16_SNORM: return TEXT("G16R16_SNORM");
	case PF_R8G8_UINT: return TEXT("R8G8_UINT");
	case PF_R32G32B32_UINT: return TEXT("R32G32B32_UINT");
	case PF_R32G32B32_SINT: return TEXT("R32G32B32_SINT");
	case PF_R32G32B32F: return TEXT("R32G32B32F");
	case PF_R8_SINT: return TEXT("R8_SINT");
	case PF_R64_UINT: return TEXT("R64_UINT");
	case PF_R9G9B9EXP5: return TEXT("R9G9B9EXP5");
	case PF_P010: return TEXT("P010");
	case PF_ASTC_4x4_NORM_RG: return TEXT("ASTC_4x4_NORM_RG");
	case PF_ASTC_6x6_NORM_RG: return TEXT("ASTC_6x6_NORM_RG");
	case PF_ASTC_8x8_NORM_RG: return TEXT("ASTC_8x8_NORM_RG");
	case PF_ASTC_10x10_NORM_RG: return TEXT("ASTC_10x10_NORM_RG");
	case PF_ASTC_12x12_NORM_RG: return TEXT("ASTC_12x12_NORM_RG");
	case PF_R16G16_SINT: return TEXT("R16G16_SINT");
	case PF_R8G8B8: return TEXT("R8G8B8");
	case PF_MAX: return TEXT("MAX");
	default: return TEXT("Unknown");
	}
}

FString VdjmVkUtil::ConvertVkFormatToString(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_UNDEFINED: return TEXT("Undefined");
	case VK_FORMAT_R4G4_UNORM_PACK8: return TEXT("R4G4_UNORM_PACK8");
	case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return TEXT("R4G4B4A4_UNORM_PACK16");
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return TEXT("B4G4R4A4_UNORM_PACK16");
	case VK_FORMAT_R5G6B5_UNORM_PACK16: return TEXT("R5G6B5_UNORM_PACK16");
	case VK_FORMAT_B5G6R5_UNORM_PACK16: return TEXT("B5G6R5_UNORM_PACK16");
	case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return TEXT("R5G5B5A1_UNORM_PACK16");
	case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return TEXT("B5G5R5A1_UNORM_PACK16");
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return TEXT("A1R5G5B5_UNORM_PACK16");
	case VK_FORMAT_R8_UNORM: return TEXT("R8_UNORM");
	case VK_FORMAT_R8_SNORM: return TEXT("R8_SNORM");
	case VK_FORMAT_R8_USCALED: return TEXT("R8_USCALED");
	case VK_FORMAT_R8_SSCALED: return TEXT("R8_SSCALED");
	case VK_FORMAT_R8_UINT: return TEXT("R8_UINT");
	case VK_FORMAT_R8_SINT: return TEXT("R8_SINT");
	case VK_FORMAT_R8_SRGB: return TEXT("R8_SRGB");
	case VK_FORMAT_R8G8_UNORM: return TEXT("R8G8_UNORM");
	case VK_FORMAT_R8G8_SNORM: return TEXT("R8G8_SNORM");
	case VK_FORMAT_R8G8_USCALED: return TEXT("R8G8_USCALED");
	case VK_FORMAT_R8G8_SSCALED: return TEXT("R8G8_SSCALED");
	case VK_FORMAT_R8G8_UINT: return TEXT("R8G8_UINT");
	case VK_FORMAT_R8G8_SINT: return TEXT("R8G8_SINT");
	case VK_FORMAT_R8G8_SRGB: return TEXT("R8G8_SRGB");
	case VK_FORMAT_R8G8B8_UNORM: return TEXT("R8G8B8_UNORM");
	case VK_FORMAT_R8G8B8_SNORM: return TEXT("R8G8B8_SNORM");
	case VK_FORMAT_R8G8B8_USCALED: return TEXT("R8G8B8_USCALED");
	case VK_FORMAT_R8G8B8_SSCALED: return TEXT("R8G8B8_SSCALED");
	case VK_FORMAT_R8G8B8_UINT: return TEXT("R8G8B8_UINT");
	case VK_FORMAT_R8G8B8_SINT: return TEXT("R8G8B8_SINT");
	case VK_FORMAT_R8G8B8_SRGB: return TEXT("R8G8B8_SRGB");
	case VK_FORMAT_B8G8R8_UNORM: return TEXT("B8G8R8_UNORM");
	case VK_FORMAT_B8G8R8_SNORM: return TEXT("B8G8R8_SNORM");
	case VK_FORMAT_B8G8R8_USCALED: return TEXT("B8G8R8_USCALED");
	case VK_FORMAT_B8G8R8_SSCALED: return TEXT("B8G8R8_SSCALED");
	case VK_FORMAT_B8G8R8_UINT: return TEXT("B8G8R8_UINT");
	case VK_FORMAT_B8G8R8_SINT: return TEXT("B8G8R8_SINT");
	case VK_FORMAT_B8G8R8_SRGB: return TEXT("B8G8R8_SRGB");
	case VK_FORMAT_R8G8B8A8_UNORM: return TEXT("R8G8B8A8_UNORM");
	case VK_FORMAT_R8G8B8A8_SNORM: return TEXT("R8G8B8A8_SNORM");
	case VK_FORMAT_R8G8B8A8_USCALED: return TEXT("R8G8B8A8_USCALED");
	case VK_FORMAT_R8G8B8A8_SSCALED: return TEXT("R8G8B8A8_SSCALED");
	case VK_FORMAT_R8G8B8A8_UINT: return TEXT("R8G8B8A8_UINT");
	case VK_FORMAT_R8G8B8A8_SINT: return TEXT("R8G8B8A8_SINT");
	case VK_FORMAT_R8G8B8A8_SRGB: return TEXT("R8G8B8A8_SRGB"); 
	case VK_FORMAT_B8G8R8A8_UNORM: return TEXT("B8G8R8A8_UNORM");
	case VK_FORMAT_B8G8R8A8_SNORM: return TEXT("B8G8R8A8_SNORM");
	case VK_FORMAT_B8G8R8A8_USCALED: return TEXT("B8G8R8A8_USCALED");
	case VK_FORMAT_B8G8R8A8_SSCALED: return TEXT("B8G8R8A8_SSCALED");
	case VK_FORMAT_B8G8R8A8_UINT: return TEXT("B8G8R8A8_UINT");
	case VK_FORMAT_B8G8R8A8_SINT: return TEXT("B8G8R8A8_SINT");
	case VK_FORMAT_B8G8R8A8_SRGB: return TEXT("B8G8R8A8_SRGB");
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return TEXT("A8B8G8R8_UNORM_PACK32");
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return TEXT("A8B8G8R8_SNORM_PACK32");
	case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return TEXT("A8B8G8R8_USCALED_PACK32");
	case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return TEXT("A8B8G8R8_SSCALED_PACK32");
	case VK_FORMAT_A8B8G8R8_UINT_PACK32: return TEXT("A8B8G8R8_UINT_PACK32");
	case VK_FORMAT_A8B8G8R8_SINT_PACK32: return TEXT("A8B8G8R8_SINT_PACK32");
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return TEXT("A8B8G8R8_SRGB_PACK32");
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return TEXT("A2R10G10B10_UNORM_PACK32");
	case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return TEXT("A2R10G10B10_SNORM_PACK32");
	case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return TEXT("A2R10G10B10_USCALED_PACK32");
	case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return TEXT("A2R10G10B10_SSCALED_PACK32");
	case VK_FORMAT_A2R10G10B10_UINT_PACK32: return TEXT("A2R10G10B10_UINT_PACK32");
	case VK_FORMAT_A2R10G10B10_SINT_PACK32: return TEXT("A2R10G10B10_SINT_PACK32");
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return TEXT("A2B10G10R10_UNORM_PACK32");
	case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return TEXT("A2B10G10R10_SNORM_PACK32");
	case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return TEXT("A2B10G10R10_USCALED_PACK32");
	case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return TEXT("A2B10G10R10_SSCALED_PACK32");
	case VK_FORMAT_A2B10G10R10_UINT_PACK32: return TEXT("A2B10G10R10_UINT_PACK32");
	case VK_FORMAT_A2B10G10R10_SINT_PACK32: return TEXT("A2B10G10R10_SINT_PACK32");
	case VK_FORMAT_R16_UNORM: return TEXT("R16_UNORM");
	case VK_FORMAT_R16_SNORM: return TEXT("R16_SNORM");
	case VK_FORMAT_R16_USCALED: return TEXT("R16_USCALED");
	case VK_FORMAT_R16_SSCALED: return TEXT("R16_SSCALED");
	case VK_FORMAT_R16_UINT: return TEXT("R16_UINT");
	case VK_FORMAT_R16_SINT: return TEXT("R16_SINT");
	case VK_FORMAT_R16_SFLOAT: return TEXT("R16_SFLOAT");
	case VK_FORMAT_R16G16_UNORM: return TEXT("R16G16_UNORM");
	case VK_FORMAT_R16G16_SNORM: return TEXT("R16G16_SNORM");
	case VK_FORMAT_R16G16_USCALED: return TEXT("R16G16_USCALED");
	case VK_FORMAT_R16G16_SSCALED: return TEXT("R16G16_SSCALED");
	case VK_FORMAT_R16G16_UINT: return TEXT("R16G16_UINT");
	case VK_FORMAT_R16G16_SINT: return TEXT("R16G16_SINT");
	case VK_FORMAT_R16G16_SFLOAT: return TEXT("R16G16_SFLOAT");
	case VK_FORMAT_R16G16B16_UNORM: return TEXT("R16G16B16_UNORM");
	case VK_FORMAT_R16G16B16_SNORM: return TEXT("R16G16B16_SNORM");
	case VK_FORMAT_R16G16B16_USCALED: return TEXT("R16G16B16_USCALED");
	case VK_FORMAT_R16G16B16_SSCALED: return TEXT("R16G16B16_SSCALED");
	case VK_FORMAT_R16G16B16_UINT: return TEXT("R16G16B16_UINT");
	case VK_FORMAT_R16G16B16_SINT: return TEXT("R16G16B16_SINT");
	case VK_FORMAT_R16G16B16_SFLOAT: return TEXT("R16G16B16_SFLOAT");
	case VK_FORMAT_R16G16B16A16_UNORM: return TEXT("R16G16B16A16_UNORM");
	case VK_FORMAT_R16G16B16A16_SNORM: return TEXT("R16G16B16A16_SNORM");
	case VK_FORMAT_R16G16B16A16_USCALED: return TEXT("R16G16B16A16_USCALED");
	case VK_FORMAT_R16G16B16A16_SSCALED: return TEXT("R16G16B16A16_SSCALED");
	case VK_FORMAT_R16G16B16A16_UINT: return TEXT("R16G16B16A16_UINT");
	case VK_FORMAT_R16G16B16A16_SINT: return TEXT("R16G16B16A16_SINT");
	case VK_FORMAT_R16G16B16A16_SFLOAT: return TEXT("R16G16B16A16_SFLOAT");
	case VK_FORMAT_R32_UINT: return TEXT("R32_UINT");
	case VK_FORMAT_R32_SINT: return TEXT("R32_SINT");
	case VK_FORMAT_R32_SFLOAT: return TEXT("R32_SFLOAT");
	case VK_FORMAT_R32G32_UINT: return TEXT("R32G32_UINT");
	case VK_FORMAT_R32G32_SINT: return TEXT("R32G32_SINT");
	case VK_FORMAT_R32G32_SFLOAT: return TEXT("R32G32_SFLOAT");
	case VK_FORMAT_R32G32B32_UINT: return TEXT("R32G32B32_UINT");
	case VK_FORMAT_R32G32B32_SINT: return TEXT("R32G32B32_SINT");
	case VK_FORMAT_R32G32B32_SFLOAT: return TEXT("R32G32B32_SFLOAT");
	case VK_FORMAT_R32G32B32A32_UINT: return TEXT("R32G32B32A32_UINT");
	case VK_FORMAT_R32G32B32A32_SINT: return TEXT("R32G32B32A32_SINT");
	case VK_FORMAT_R32G32B32A32_SFLOAT: return TEXT("R32G32B32A32_SFLOAT");
	case VK_FORMAT_R64_UINT: return TEXT("R64_UINT");
	case VK_FORMAT_R64_SINT: return TEXT("R64_SINT");
	case VK_FORMAT_R64_SFLOAT: return TEXT("R64_SFLOAT");
	case VK_FORMAT_R64G64_UINT: return TEXT("R64G64_UINT");
	case VK_FORMAT_R64G64_SINT: return TEXT("R64G64_SINT");
	case VK_FORMAT_R64G64_SFLOAT: return TEXT("R64G64_SFLOAT");
	case VK_FORMAT_R64G64B64_UINT: return TEXT("R64G64B64_UINT");
	case VK_FORMAT_R64G64B64_SINT: return TEXT("R64G64B64_SINT");
	case VK_FORMAT_R64G64B64_SFLOAT: return TEXT("R64G64B64_SFLOAT");
	case VK_FORMAT_R64G64B64A64_UINT: return TEXT("R64G64B64A64_UINT");
	case VK_FORMAT_R64G64B64A64_SINT: return TEXT("R64G64B64A64_SINT");
	case VK_FORMAT_R64G64B64A64_SFLOAT: return TEXT("R64G64B64A64_SFLOAT");
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return TEXT("B10G11R11_UFLOAT_PACK32");
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return TEXT("E5B9G9R9_UFLOAT_PACK32");
	case VK_FORMAT_D16_UNORM: return TEXT("D16_UNORM");
	case VK_FORMAT_X8_D24_UNORM_PACK32: return TEXT("X8_D24_UNORM_PACK32");
	case VK_FORMAT_D32_SFLOAT: return TEXT("D32_SFLOAT");
	case VK_FORMAT_S8_UINT: return TEXT("S8_UINT");
	case VK_FORMAT_D16_UNORM_S8_UINT: return TEXT("D16_UNORM_S8_UINT");
	case VK_FORMAT_D24_UNORM_S8_UINT: return TEXT("D24_UNORM_S8_UINT");
	case VK_FORMAT_D32_SFLOAT_S8_UINT: return TEXT("D32_SFLOAT_S8_UINT");
	case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return TEXT("BC1_RGB_UNORM_BLOCK");
	case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return TEXT("BC1_RGB_SRGB_BLOCK");
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return TEXT("BC1_RGBA_UNORM_BLOCK");
	case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return TEXT("BC1_RGBA_SRGB_BLOCK");
	case VK_FORMAT_BC2_UNORM_BLOCK: return TEXT("BC2_UNORM_BLOCK");
	case VK_FORMAT_BC2_SRGB_BLOCK: return TEXT("BC2_SRGB_BLOCK");
	case VK_FORMAT_BC3_UNORM_BLOCK: return TEXT("BC3_UNORM_BLOCK");
	case VK_FORMAT_BC3_SRGB_BLOCK: return TEXT("BC3_SRGB_BLOCK");
	case VK_FORMAT_BC4_UNORM_BLOCK: return TEXT("BC4_UNORM_BLOCK");
	case VK_FORMAT_BC4_SNORM_BLOCK: return TEXT("BC4_SNORM_BLOCK");
	case VK_FORMAT_BC5_UNORM_BLOCK: return TEXT("BC5_UNORM_BLOCK");
	case VK_FORMAT_BC5_SNORM_BLOCK: return TEXT("BC5_SNORM_BLOCK");
	case VK_FORMAT_BC6H_UFLOAT_BLOCK: return TEXT("BC6H_UFLOAT_BLOCK");
	case VK_FORMAT_BC6H_SFLOAT_BLOCK: return TEXT("BC6H_SFLOAT_BLOCK");
	case VK_FORMAT_BC7_UNORM_BLOCK: return TEXT("BC7_UNORM_BLOCK");
	case VK_FORMAT_BC7_SRGB_BLOCK: return TEXT("BC7_SRGB_BLOCK");
	case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return TEXT("ETC2_R8G8B8_UNORM_BLOCK");
	case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return TEXT("ETC2_R8G8B8_SRGB_BLOCK");
	case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return TEXT("ETC2_R8G8B8A1_UNORM_BLOCK");
	case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return TEXT("ETC2_R8G8B8A1_SRGB_BLOCK");
	case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return TEXT("ETC2_R8G8B8A8_UNORM_BLOCK");
	case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return TEXT("ETC2_R8G8B8A8_SRGB_BLOCK");
	case VK_FORMAT_EAC_R11_UNORM_BLOCK: return TEXT("EAC_R11_UNORM_BLOCK");
	case VK_FORMAT_EAC_R11_SNORM_BLOCK: return TEXT("EAC_R11_SNORM_BLOCK");
	case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return TEXT("EAC_R11G11_UNORM_BLOCK");
	case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return TEXT("EAC_R11G11_SNORM_BLOCK");
	case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return TEXT("ASTC_4x4_UNORM_BLOCK");
	case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return TEXT("ASTC_4x4_SRGB_BLOCK");
	case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return TEXT("ASTC_5x4_UNORM_BLOCK");
	case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return TEXT("ASTC_5x4_SRGB_BLOCK");
	case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return TEXT("ASTC_5x5_UNORM_BLOCK");
	case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return TEXT("ASTC_5x5_SRGB_BLOCK");
	case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return TEXT("ASTC_6x5_UNORM_BLOCK");
	case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return TEXT("ASTC_6x5_SRGB_BLOCK");
	case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return TEXT("ASTC_6x6_UNORM_BLOCK");
	case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return TEXT("ASTC_6x6_SRGB_BLOCK");
	case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return TEXT("ASTC_8x5_UNORM_BLOCK");
	case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return TEXT("ASTC_8x5_SRGB_BLOCK");
	case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return TEXT("ASTC_8x6_UNORM_BLOCK");
	case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return TEXT("ASTC_8x6_SRGB_BLOCK");
	case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return TEXT("ASTC_8x8_UNORM_BLOCK");
	case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return TEXT("ASTC_8x8_SRGB_BLOCK");
	case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return TEXT("ASTC_10x5_UNORM_BLOCK");
	case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return TEXT("ASTC_10x5_SRGB_BLOCK");
	case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return TEXT("ASTC_10x6_UNORM_BLOCK");
	case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return TEXT("ASTC_10x6_SRGB_BLOCK");
	case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return TEXT("ASTC_10x8_UNORM_BLOCK");
	case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return TEXT("ASTC_10x8_SRGB_BLOCK");
	case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return TEXT("ASTC_10x10_UNORM_BLOCK");
	case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return TEXT("ASTC_10x10_SRGB_BLOCK");
	case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return TEXT("ASTC_12x10_UNORM_BLOCK");
	case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return TEXT("ASTC_12x10_SRGB_BLOCK");
	case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return TEXT("ASTC_12x12_UNORM_BLOCK");
	case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return TEXT("ASTC_12x12_SRGB_BLOCK");
	case VK_FORMAT_G8B8G8R8_422_UNORM: return TEXT("G8B8G8R8_422_UNORM");
	case VK_FORMAT_B8G8R8G8_422_UNORM: return TEXT("B8G8R8G8_422_UNORM");
	case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return TEXT("G8_B8_R8_3PLANE_420_UNORM");
	case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return TEXT("G8_B8R8_2PLANE_420_UNORM");
	case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: return TEXT("G8_B8_R8_3PLANE_422_UNORM");
	case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return TEXT("G8_B8R8_2PLANE_422_UNORM");
	case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return TEXT("G8_B8_R8_3PLANE_444_UNORM");
	case VK_FORMAT_R10X6_UNORM_PACK16: return TEXT("R10X6_UNORM_PACK16");
	case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: return TEXT("R10X6G10X6_UNORM_2PACK16");
	case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: return TEXT("R10X6G10X6B10X6A10X6_UNORM_4PACK16");
	case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return TEXT("G10X6B10X6G10X6R10X6_422_UNORM_4PACK16");
	case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return TEXT("B10X6G10X6R10X6G10X6_422_UNORM_4PACK16");
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return TEXT("G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16");
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return TEXT("G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16");
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return TEXT("G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16");
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return TEXT("G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16");
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return TEXT("G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16");
	case VK_FORMAT_R12X4_UNORM_PACK16: return TEXT("R12X4_UNORM_PACK16");
	case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: return TEXT("R12X4G12X4_UNORM_2PACK16");
	case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: return TEXT("R12X4G12X4B12X4A12X4_UNORM_4PACK16");
	case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return TEXT("G12X4B12X4G12X4R12X4_422_UNORM_4PACK16");
	case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return TEXT("B12X4G12X4R12X4G12X4_422_UNORM_4PACK16");
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return TEXT("G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16");
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return TEXT("G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16");
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return TEXT("G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16");
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return TEXT("G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16");
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return TEXT("G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16");
	case VK_FORMAT_G16B16G16R16_422_UNORM: return TEXT("G16B16G16R16_422_UNORM");
	case VK_FORMAT_B16G16R16G16_422_UNORM: return TEXT("B16G16R16G16_422_UNORM");
	case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: return TEXT("G16_B16_R16_3PLANE_420_UNORM");
	case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return TEXT("G16_B16R16_2PLANE_420_UNORM");
	case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: return TEXT("G16_B16_R16_3PLANE_422_UNORM");
	case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: return TEXT("G16_B16R16_2PLANE_422_UNORM");
	case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return TEXT("G16_B16_R16_3PLANE_444_UNORM");
	case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM: return TEXT("G8_B8R8_2PLANE_444_UNORM");
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return TEXT("G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16");
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16: return TEXT("G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16");
	case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM: return TEXT("G16_B16R16_2PLANE_444_UNORM");
	case VK_FORMAT_A4R4G4B4_UNORM_PACK16: return TEXT("A4R4G4B4_UNORM_PACK16");
	case VK_FORMAT_A4B4G4R4_UNORM_PACK16: return TEXT("A4B4G4R4_UNORM_PACK16");
	case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK: return TEXT("ASTC_4x4_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK: return TEXT("ASTC_5x4_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK: return TEXT("ASTC_5x5_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK: return TEXT("ASTC_6x5_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK: return TEXT("ASTC_6x6_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK: return TEXT("ASTC_8x5_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK: return TEXT("ASTC_8x6_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK: return TEXT("ASTC_8x8_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK: return TEXT("ASTC_10x5_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK: return TEXT("ASTC_10x6_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK: return TEXT("ASTC_10x8_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK: return TEXT("ASTC_10x10_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK: return TEXT("ASTC_12x10_SFLOAT_BLOCK");
	case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK: return TEXT("ASTC_12x12_SFLOAT_BLOCK");
	case VK_FORMAT_A1B5G5R5_UNORM_PACK16: return TEXT("A1B5G5R5_UNORM_PACK16");
	case VK_FORMAT_A8_UNORM: return TEXT("A8_UNORM");
	case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return TEXT("PVRTC1_2BPP_UNORM_BLOCK_IMG");
	case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return TEXT("PVRTC1_4BPP_UNORM_BLOCK_IMG");
	case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return TEXT("PVRTC2_2BPP_UNORM_BLOCK_IMG");
	case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return TEXT("PVRTC2_4BPP_UNORM_BLOCK_IMG");
	case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return TEXT("PVRTC1_2BPP_SRGB_BLOCK_IMG");
	case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return TEXT("PVRTC1_4BPP_SRGB_BLOCK_IMG");
	case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return TEXT("PVRTC2_2BPP_SRGB_BLOCK_IMG");
	case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return TEXT("PVRTC2_4BPP_SRGB_BLOCK_IMG");
	case VK_FORMAT_R16G16_SFIXED5_NV: return TEXT("R16G16_SFIXED5_NV");
	default: 
		return TEXT("Unknown");
	}
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

bool FVdjmVkCodecInputSurfaceState::CreateSwapchain(
	const FVdjmVkRecoderHandles& vkHandles,
	const FVdjmAndroidEncoderConfigure& config)
{
	if (!vkHandles.IsValid() || mSurface == VK_NULL_HANDLE)
	{
		return false;
	}

	VkBool32 bSurfaceSupported = VK_FALSE;
	if (!VdjmVkUtil::CheckVkResult(
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
	if (!VdjmVkUtil::CheckVkResult(
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkHandles.GetVkPhysicalDevice(), mSurface, &caps),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.SurfaceCaps")))
	{
		return false;
	}

	uint32 formatCount = 0;
	if (!VdjmVkUtil::CheckVkResult(
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

	if (!VdjmVkUtil::CheckVkResult(
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

	if (!VdjmVkUtil::CheckVkResult(
		vkCreateSwapchainKHR(vkHandles.GetVkDevice(), &swapchainInfo, nullptr, &mSwapchain),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.Create")))
	{
		return false;
	}

	uint32 imageCount = 0;
	if (!VdjmVkUtil::CheckVkResult(
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
	if (!VdjmVkUtil::CheckVkResult(
		vkGetSwapchainImagesKHR(vkHandles.GetVkDevice(), mSwapchain, &imageCount, mSwapchainImages.GetData()),
		TEXT("FVdjmVkCodecInputSurfaceState::CreateSwapchain.GetImages")))
	{
		return false;
	}

	mSwapchainImageViews.Reset();
	mSwapchainImageViews.Reserve(imageCount);
	mSwapchainImageLayouts.Init(VK_IMAGE_LAYOUT_UNDEFINED, imageCount);

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
		if (!VdjmVkUtil::CheckVkResult(
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
		mSwapchainImageLayouts.Reset();
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
	mSwapchainImageLayouts.Reset();

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
	
	if (!VdjmVkUtil::CheckVkResult(vkQueueWaitIdle(mVkHandles.GetGraphicsQueue()),
		TEXT("FVdjmAndroidEncoderBackendVulkan::Running.PreFrameQueueWaitIdle")))
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("FVdjmAndroidEncoderBackendVulkan::Running - failed to wait for graphics queue idle before acquiring frame. timestamp=%f"),
			timeStampSec);
		return false;
	}

	if (not VdjmVkUtil::WaitAndAcquireFrame(mVkHandles, mCodecInputSurfaceState, *frameResources))
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
