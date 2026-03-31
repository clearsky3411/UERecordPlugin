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
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("Analyze: unsupported UE pixel format = %s"),
			*FVdjmVkHelper::ConvertPixelFormatToString( srcTexture->GetFormat()));
		return false;
	}
	return true;
}

FString FVdjmVkSubmitFrameInfo::ToString() const
{
	if (!IsValid())
	{
		return TEXT("Invalid FVdjmVkSubmitFrameInfo");
	}
	return FString::Printf(TEXT("{ FVdjmVkSubmitFrameInfo: SrcImage=0x%p, SrcFormat=%s, SrcWidth=%u, SrcHeight=%u, bNeedsIntermediate=%s, bCanDirectCopy=%s }"),
		SrcImage,
		*FVdjmVkHelper::ConvertVkFormatToString(SrcFormat),
		SrcWidth,
		SrcHeight,
		bNeedsIntermediate ? TEXT("true") : TEXT("false"),
		bCanDirectCopy ? TEXT("true") : TEXT("false"));
	
}

FVdjmVkRecordSessionState::FVdjmVkRecordSessionState():
	InputWindow(nullptr)
	, CodecSurface(VK_NULL_HANDLE)
	, CodecSwapchain(VK_NULL_HANDLE)
	, SwapchainImages()
	, FrameContexts()
	,SurfaceFormat(VK_FORMAT_UNDEFINED)
	, SurfaceColorSpace(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
	,SurfaceExtent{0, 0}
	, CurrentFrameContextIndex(0)
	,bStopRequested(false)
	, SubmissionSerial(0)
	, bHasIntermediateImage(false), bStarted(false), CreatedFrameContextCount(0), DestroyedFrameContextCount(0), SubmittedFrameCount(0), PresentedFrameCount(0),IntermediateImageState(),SourceStateCache()
{
	FrameContexts.SetNum(MaxInFlightFrames);
	FVdjmVkFrameContext::ClearAll(FrameContexts);
}

bool FVdjmVkRecordSessionState::ValidateRecordedOutputFile() const
{
	return false;
}

/*
 *	class FVdjmAndroidEncoderBackendVulkan : public FVdjmAndroidEncoderBackend
 */

uint32 FVdjmVkHelper::FindMemoryType(VkPhysicalDevice physicalDevice, uint32 typeFilter,VkMemoryPropertyFlags properties)
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

void FVdjmVkHelper::TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
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
bool FVdjmVkHelper::TransitionOwnedImage(
	VkCommandBuffer Cmd,FVdjmVkOwnedImageState& State,	VkImageLayout NewLayout)
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

VkSurfaceFormatKHR FVdjmVkHelper::ChooseSurfaceFormat(const FVdjmVkRuntimeContext& runtimeContext,const TArray<VkSurfaceFormatKHR>& availableFormats)
{
	const VkFormat PreferredFormats[] =
	{
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8A8_UNORM,
		runtimeContext.VulkanRHI->RHIGetSwapChainVkFormat(PF_R8G8B8A8),
		runtimeContext.VulkanRHI->RHIGetSwapChainVkFormat(PF_B8G8R8A8)
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

VkPresentModeKHR FVdjmVkHelper::ChoosePresentMode(const TArray<VkPresentModeKHR>& modes)
{
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkCompositeAlphaFlagBitsKHR FVdjmVkHelper::ChooseCompositeAlpha(VkCompositeAlphaFlagsKHR flags)
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

VkExtent2D FVdjmVkHelper::ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32 desiredWid,	uint32 desiredHei)
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

FString FVdjmVkHelper::ConvertVkFormatToString(VkFormat format)
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

FString FVdjmVkHelper::ConvertPixelFormatToString(EPixelFormat format)
{
	return GPixelFormats[format].Name; 
}


FVdjmAndroidEncoderBackendVulkan::FVdjmAndroidEncoderBackendVulkan()
	: mAnalyzer(this),
	  mInitialized(false), mStarted(false), mPaused(false)
{}

bool FVdjmAndroidEncoderBackendVulkan::Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow)
{
	if (not config.IsValidateEncoderArguments() || inputWindow == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Init: invalid config or input window"));
		return false;
	}

	if (not InitVkRuntimeContext())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Init: failed to initialize Vulkan runtime context"));
		return false;
	}

	if (mInputWindow != nullptr && mInputWindow != inputWindow)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Init: replacing existing input window"));
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
	}

	UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Init: Vulkan runtime context initialized successfully"));
	mConfig = config;
	mInputWindow = inputWindow;
	ANativeWindow_acquire(mInputWindow);

	mVkRecordSession.Clear();
	mCurrentSwapchainImageIndex32 = UINT32_MAX;
	
	mInitialized = true;
	mStarted = false;
	mPaused = false;
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::Start()
{
	if (!mInitialized || mInputWindow == nullptr || !mVkRuntime.IsValid())
	{
		return false;
	}

	if (mStarted)
	{
		return true;
	}

	if (not CreateRecordSessionVkResources())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Start: failed to create Vulkan resources for record session"));
		DestroyRecordSessionVkResources();
		return false;
	}

	if (not mVkRecordSession.IsReadyToStart())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Start: record session is not ready to start"));
		DestroyRecordSessionVkResources();
		return false;
	}

	mPaused = false;
	mStarted = true;
	mVkRecordSession.bStarted = true;
	UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Start: Vulkan encoder backend started successfully"));
	return true;
}
void FVdjmAndroidEncoderBackendVulkan::Stop()
{
	mStarted = false;
	mPaused = false;

	DrainInFlightFrames();
	DestroyRecordSessionVkResources();
}
void FVdjmAndroidEncoderBackendVulkan::Terminate()
{
	Stop();

	if (mInputWindow != nullptr)
	{
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
	}

	mVkRuntime.Clear();
	mInitialized = false;
}

bool FVdjmAndroidEncoderBackendVulkan::IsRunnable() const
{
	if (!mInitialized || !mStarted || mPaused)
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
	if (!mVkRecordSession.IsReadyToStart())
	{
		return false;
	}
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	if (not srcTexture.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Running: srcTexture is invalid"));
		return false;
	}
	
	if (not IsRunnable())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Running: encoder backend is not runnable"));
		return false;
	}
	
	SetFailureReason(EVdjmVkFailureReason::None);
	
	FVdjmVkSubmitFrameInfo SubmitInfo{};
	if (not mAnalyzer.Analyze(srcTexture, SubmitInfo))
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Running: failed to analyze source texture for submission"));
		return false;
	}

	const bool bExactSize =
		SubmitInfo.SrcWidth == mVkRecordSession.SurfaceExtent.width &&
		SubmitInfo.SrcHeight == mVkRecordSession.SurfaceExtent.height;

	const bool bExactFormat =
		SubmitInfo.SrcFormat == mVkRecordSession.SurfaceFormat;

	SubmitInfo.bCanDirectCopy = bExactSize && bExactFormat;
	SubmitInfo.bNeedsIntermediate = !SubmitInfo.bCanDirectCopy;

	if (not bExactSize)
	{
		SetFailureReason(EVdjmVkFailureReason::SourceExtentMismatch);
		
		UE_LOG(LogVdjmRecorderCore, Warning,
		TEXT("Running: size mismatch is not supported by the current intermediate path. Src=%ux%u Surface=%ux%u"),
		SubmitInfo.SrcWidth,
		SubmitInfo.SrcHeight,
		mVkRecordSession.SurfaceExtent.width,
		mVkRecordSession.SurfaceExtent.height);
		
		return false;
	}
	
	//	Direct copy 경로
	if (bExactFormat)
	{
		FVdjmVkFrameSubmitState FrameState{};
		if (not AcquireNextSwapchainImage(FrameState))
		{
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Running: failed to acquire next swapchain image"));
			return false;
		}

		return SubmitTextureToCodecSurface(SubmitInfo, FrameState);
	}
	
	//	Intermediate 경로
	FVdjmVkObservedSourceState SourceState{};
	if (not TryResolveSourceState(SubmitInfo, SourceState))
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Running: failed to resolve source state for intermediate path"));
		return false;
	}

	if (not EnsureIntermediateForFrame(SubmitInfo))
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Running: failed to prepare intermediate image"));
		return false;
	}

	FVdjmVkFrameSubmitState FrameState{};
	if (not AcquireNextSwapchainImage(FrameState))
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Running: failed to acquire next swapchain image"));
		return false;
	}

	FVdjmVkFrameContext& FrameCtx = mVkRecordSession.GetCurrentFrameContext();
	return SubmitTextureViaIntermediate(SubmitInfo, FrameCtx, SourceState);
	
}

bool FVdjmAndroidEncoderBackendVulkan::CreateRecordSessionVkResources()
{
	if (mInputWindow == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("CreateRecordSessionVkResources: input window is null"));
		return false;
	}

	if (!mVkRuntime.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("CreateRecordSessionVkResources: Vulkan runtime is invalid"));
		return false;
	}

	DestroyRecordSessionVkResources();

	mVkRecordSession.InputWindow = mInputWindow;

	VkAndroidSurfaceCreateInfoKHR SurfaceInfo{};
	SurfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	SurfaceInfo.window = mInputWindow;

	VkResult Result = vkCreateAndroidSurfaceKHR(
		mVkRuntime.VkInstance,
		&SurfaceInfo,
		nullptr,
		&mVkRecordSession.CodecSurface);

	if (Result != VK_SUCCESS || mVkRecordSession.CodecSurface == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: vkCreateAndroidSurfaceKHR failed. Result=%d"),
		       (int32)Result);
		DestroyRecordSessionVkResources();
		return false;
	}

	VkBool32 bSurfaceSupported = VK_FALSE;
	Result = vkGetPhysicalDeviceSurfaceSupportKHR(
		mVkRuntime.VkPhysicalDevice,
		mVkRuntime.GraphicsQueueFamilyIndex,
		mVkRecordSession.CodecSurface,
		&bSurfaceSupported);

	if (Result != VK_SUCCESS || bSurfaceSupported != VK_TRUE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: surface present support check failed. Result=%d Supported=%d"),
		       (int32)Result,
		       (int32)bSurfaceSupported);
		DestroyRecordSessionVkResources();
		return false;
	}

	VkSurfaceCapabilitiesKHR SurfaceCaps{};
	Result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		mVkRuntime.VkPhysicalDevice,
		mVkRecordSession.CodecSurface,
		&SurfaceCaps);

	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed. Result=%d"),
		       (int32)Result);
		DestroyRecordSessionVkResources();
		return false;
	}

	if ((SurfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: surface does not support VK_IMAGE_USAGE_TRANSFER_DST_BIT"));
		DestroyRecordSessionVkResources();
		return false;
	}

	uint32 FormatCount = 0;
	Result = vkGetPhysicalDeviceSurfaceFormatsKHR(
		mVkRuntime.VkPhysicalDevice,
		mVkRecordSession.CodecSurface,
		&FormatCount,
		nullptr);

	if (Result != VK_SUCCESS || FormatCount == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: query surface format count failed. Result=%d Count=%u"),
		       (int32)Result,
		       FormatCount);
		DestroyRecordSessionVkResources();
		return false;
	}

	TArray<VkSurfaceFormatKHR> SurfaceFormats;
	SurfaceFormats.SetNum(FormatCount);

	Result = vkGetPhysicalDeviceSurfaceFormatsKHR(
		mVkRuntime.VkPhysicalDevice,
		mVkRecordSession.CodecSurface,
		&FormatCount,
		SurfaceFormats.GetData());

	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: fetch surface formats failed. Result=%d"),
		       (int32)Result);
		DestroyRecordSessionVkResources();
		return false;
	}

	uint32 PresentModeCount = 0;
	Result = vkGetPhysicalDeviceSurfacePresentModesKHR(
		mVkRuntime.VkPhysicalDevice,
		mVkRecordSession.CodecSurface,
		&PresentModeCount,
		nullptr);

	if (Result != VK_SUCCESS || PresentModeCount == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: query present mode count failed. Result=%d Count=%u"),
		       (int32)Result,
		       PresentModeCount);
		DestroyRecordSessionVkResources();
		return false;
	}

	TArray<VkPresentModeKHR> PresentModes;
	PresentModes.SetNum(PresentModeCount);

	Result = vkGetPhysicalDeviceSurfacePresentModesKHR(
		mVkRuntime.VkPhysicalDevice,
		mVkRecordSession.CodecSurface,
		&PresentModeCount,
		PresentModes.GetData());

	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: fetch present modes failed. Result=%d"),
		       (int32)Result);
		DestroyRecordSessionVkResources();
		return false;
	}

	const VkSurfaceFormatKHR ChosenSurfaceFormat =
		FVdjmVkHelper::ChooseSurfaceFormat(mVkRuntime, SurfaceFormats);
	const VkPresentModeKHR ChosenPresentMode =
		FVdjmVkHelper::ChoosePresentMode(PresentModes);
	const VkExtent2D ChosenExtent =
		FVdjmVkHelper::ChooseExtent(SurfaceCaps, mConfig.VideoWidth, mConfig.VideoHeight);

	if (ChosenSurfaceFormat.format == VK_FORMAT_UNDEFINED ||
		ChosenExtent.width == 0 ||
		ChosenExtent.height == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: invalid chosen surface contract. Format=%d Extent=(%u,%u)"),
		       (int32)ChosenSurfaceFormat.format,
		       ChosenExtent.width,
		       ChosenExtent.height);
		DestroyRecordSessionVkResources();
		return false;
	}

	mVkRecordSession.SurfaceFormat = ChosenSurfaceFormat.format;
	mVkRecordSession.SurfaceColorSpace = ChosenSurfaceFormat.colorSpace;
	mVkRecordSession.SurfaceExtent = ChosenExtent;

	uint32 DesiredImageCount = SurfaceCaps.minImageCount + 1;
	if (SurfaceCaps.maxImageCount > 0 && DesiredImageCount > SurfaceCaps.maxImageCount)
	{
		DesiredImageCount = SurfaceCaps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR SwapchainInfo{};
	SwapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	SwapchainInfo.surface = mVkRecordSession.CodecSurface;
	SwapchainInfo.minImageCount = DesiredImageCount;
	SwapchainInfo.imageFormat = mVkRecordSession.SurfaceFormat;
	SwapchainInfo.imageColorSpace = mVkRecordSession.SurfaceColorSpace;
	SwapchainInfo.imageExtent = mVkRecordSession.SurfaceExtent;
	SwapchainInfo.imageArrayLayers = 1;
	SwapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	SwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	SwapchainInfo.preTransform = SurfaceCaps.currentTransform;
	SwapchainInfo.compositeAlpha = FVdjmVkHelper::ChooseCompositeAlpha(SurfaceCaps.supportedCompositeAlpha);
	SwapchainInfo.presentMode = ChosenPresentMode;
	SwapchainInfo.clipped = VK_TRUE;
	SwapchainInfo.oldSwapchain = VK_NULL_HANDLE;

	Result = vkCreateSwapchainKHR(
		mVkRuntime.VkDevice,
		&SwapchainInfo,
		nullptr,
		&mVkRecordSession.CodecSwapchain);

	if (Result != VK_SUCCESS || mVkRecordSession.CodecSwapchain == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: vkCreateSwapchainKHR failed. Result=%d"),
		       (int32)Result);
		DestroyRecordSessionVkResources();
		return false;
	}

	uint32 SwapchainImageCount = 0;
	Result = vkGetSwapchainImagesKHR(
		mVkRuntime.VkDevice,
		mVkRecordSession.CodecSwapchain,
		&SwapchainImageCount,
		nullptr);

	if (Result != VK_SUCCESS || SwapchainImageCount == 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: query swapchain image count failed. Result=%d Count=%u"),
		       (int32)Result,
		       SwapchainImageCount);
		DestroyRecordSessionVkResources();
		return false;
	}

	mVkRecordSession.SwapchainImages.SetNum(SwapchainImageCount);

	Result = vkGetSwapchainImagesKHR(
		mVkRuntime.VkDevice,
		mVkRecordSession.CodecSwapchain,
		&SwapchainImageCount,
		mVkRecordSession.SwapchainImages.GetData());

	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("CreateRecordSessionVkResources: fetch swapchain images failed. Result=%d"),
		       (int32)Result);
		DestroyRecordSessionVkResources();
		return false;
	}

	mVkRecordSession.SwapchainImageLayouts.Init(
		VK_IMAGE_LAYOUT_UNDEFINED,
		mVkRecordSession.SwapchainImages.Num());

	for (FVdjmVkFrameContext& FrameCtx : mVkRecordSession.FrameContexts)
	{
		VkCommandPoolCreateInfo PoolInfo{};
		PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		PoolInfo.queueFamilyIndex = mVkRuntime.GraphicsQueueFamilyIndex;
		PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		Result = vkCreateCommandPool(
			mVkRuntime.VkDevice,
			&PoolInfo,
			nullptr,
			&FrameCtx.CommandPool);
		if (Result != VK_SUCCESS || FrameCtx.CommandPool == VK_NULL_HANDLE)
		{
			DestroyRecordSessionVkResources();
			return false;
		}

		VkCommandBufferAllocateInfo CmdAllocInfo{};
		CmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		CmdAllocInfo.commandPool = FrameCtx.CommandPool;
		CmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		CmdAllocInfo.commandBufferCount = 1;

		Result = vkAllocateCommandBuffers(
			mVkRuntime.VkDevice,
			&CmdAllocInfo,
			&FrameCtx.CommandBuffer);
		if (Result != VK_SUCCESS || FrameCtx.CommandBuffer == VK_NULL_HANDLE)
		{
			DestroyRecordSessionVkResources();
			return false;
		}

		VkSemaphoreCreateInfo SemaphoreInfo{};
		SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		Result = vkCreateSemaphore(
			mVkRuntime.VkDevice,
			&SemaphoreInfo,
			nullptr,
			&FrameCtx.AcquireCompleteSemaphore);
		if (Result != VK_SUCCESS || FrameCtx.AcquireCompleteSemaphore == VK_NULL_HANDLE)
		{
			DestroyRecordSessionVkResources();
			return false;
		}

		Result = vkCreateSemaphore(
			mVkRuntime.VkDevice,
			&SemaphoreInfo,
			nullptr,
			&FrameCtx.RenderCompleteSemaphore);
		if (Result != VK_SUCCESS || FrameCtx.RenderCompleteSemaphore == VK_NULL_HANDLE)
		{
			DestroyRecordSessionVkResources();
			return false;
		}

		VkFenceCreateInfo FenceInfo{};
		FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		Result = vkCreateFence(
			mVkRuntime.VkDevice,
			&FenceInfo,
			nullptr,
			&FrameCtx.SubmitFence);
		if (Result != VK_SUCCESS || FrameCtx.SubmitFence == VK_NULL_HANDLE)
		{
			DestroyRecordSessionVkResources();
			return false;
		}

		++mVkRecordSession.CreatedFrameContextCount;
	}
	
	mCurrentSwapchainImageIndex32 = UINT32_MAX;

	UE_LOG(LogVdjmRecorderCore, Warning,
	       TEXT("CreateRecordSessionVkResources: success. Format= %s Extent=(%u,%u) Images=%d"),
	       *FVdjmVkHelper::ConvertVkFormatToString( mVkRecordSession.SurfaceFormat),
	       mVkRecordSession.SurfaceExtent.width,
	       mVkRecordSession.SurfaceExtent.height,
	       mVkRecordSession.SwapchainImages.Num());

	return true;
}

void FVdjmAndroidEncoderBackendVulkan::DestroyIntermediate(VkDevice Device)
{
	if (mVkRecordSession.IntermediateImageState.View != VK_NULL_HANDLE)
	{
		vkDestroyImageView(Device, mVkRecordSession.IntermediateImageState.View, nullptr);
		mVkRecordSession.IntermediateImageState.View = VK_NULL_HANDLE;
	}

	if (mVkRecordSession.IntermediateImageState.Image != VK_NULL_HANDLE)
	{
		vkDestroyImage(Device, mVkRecordSession.IntermediateImageState.Image, nullptr);
		mVkRecordSession.IntermediateImageState.Image = VK_NULL_HANDLE;
	}

	if (mVkRecordSession.IntermediateImageState.VkMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(Device, mVkRecordSession.IntermediateImageState.VkMemory, nullptr);
		mVkRecordSession.IntermediateImageState.VkMemory = VK_NULL_HANDLE;
	}

	mVkRecordSession.bHasIntermediateImage = false;
}

void FVdjmAndroidEncoderBackendVulkan::DestroyRecordSessionVkResources()
{
	VkDevice Device = mVkRuntime.VkDevice;
	if (Device == VK_NULL_HANDLE)
	{
		DestroyIntermediate(Device);
		mVkRecordSession.Clear();
		mCurrentSwapchainImageIndex32 = UINT32_MAX;
		return;
	}

	for (FVdjmVkFrameContext& FrameCtx : mVkRecordSession.FrameContexts)
	{
		if (FrameCtx.SubmitFence != VK_NULL_HANDLE)
		{
			vkDestroyFence(Device, FrameCtx.SubmitFence, nullptr);
			FrameCtx.SubmitFence = VK_NULL_HANDLE;
		}

		if (FrameCtx.AcquireCompleteSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(Device, FrameCtx.AcquireCompleteSemaphore, nullptr);
			FrameCtx.AcquireCompleteSemaphore = VK_NULL_HANDLE;
		}

		if (FrameCtx.RenderCompleteSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(Device, FrameCtx.RenderCompleteSemaphore, nullptr);
			FrameCtx.RenderCompleteSemaphore = VK_NULL_HANDLE;
		}

		if (FrameCtx.CommandPool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(Device, FrameCtx.CommandPool, nullptr);
			FrameCtx.CommandPool = VK_NULL_HANDLE;
			FrameCtx.CommandBuffer = VK_NULL_HANDLE;
		}

		FrameCtx.bInFlight = false;
		FrameCtx.SubmissionSerial = 0;
		++mVkRecordSession.DestroyedFrameContextCount;
	}
	
	if (mVkRecordSession.CodecSwapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(Device, mVkRecordSession.CodecSwapchain, nullptr);
		mVkRecordSession.CodecSwapchain = VK_NULL_HANDLE;
	}

	if (mVkRecordSession.CodecSurface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(mVkRuntime.VkInstance, mVkRecordSession.CodecSurface, nullptr);
		mVkRecordSession.CodecSurface = VK_NULL_HANDLE;
	}
	//	destroy intermedia
	DestroyIntermediate(Device);

	mVkRecordSession.Clear();
	mCurrentSwapchainImageIndex32 = UINT32_MAX;
}

bool FVdjmAndroidEncoderBackendVulkan::TryExtractNativeVkImage(const FTextureRHIRef& srcTexture,VkImage& outImage) const
{
	/*
	 * TryExtractNativeVkImage 역할
	 *
	 * 1. UE가 넘겨준 FTextureRHIRef에서 Vulkan backend가 실제 submit에 사용할
	 *    native VkImage 핸들을 꺼내는 단계다.
	 *
	 * 2. 이 함수는 "입력 해석"만 담당한다.
	 *    - 세션 생성 아님
	 *    - layout transition 아님
	 *    - copy / submit 아님
	 *
	 * 3. 지금 단계에서는 가장 단순한 경로부터 확인한다.
	 *    - srcTexture->GetNativeResource()를 받아
	 *    - 그것이 곧바로 VkImage라고 가정하고 캐스팅한다.
	 *
	 * 4. 만약 런타임에서 이 가정이 틀리면
	 *    - 여기서 null 또는 잘못된 handle 로그가 찍힐 것이고
	 *    - 그때 UE Vulkan 내부 타입 경로로 다시 좁혀가면 된다.
	 */

	outImage = VK_NULL_HANDLE;

	if (!srcTexture.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("TryExtractNativeVkImage: srcTexture is invalid"));
		return false;
	}

	void* NativeResource = srcTexture->GetNativeResource();
	if (NativeResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("TryExtractNativeVkImage: GetNativeResource returned null"));
		return false;
	}

	outImage = reinterpret_cast<VkImage>(NativeResource);

	if (outImage == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("TryExtractNativeVkImage: native resource cast produced null VkImage"));
		return false;
	}

	UE_LOG(LogVdjmRecorderCore, VeryVerbose,
	       TEXT("TryExtractNativeVkImage: success. SrcTexture=%p NativeResource=%p VkImage=%p"),
	       srcTexture.GetReference(),
	       NativeResource,
	       outImage);

	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::EnsureIntermediateForFrame(const FVdjmVkSubmitFrameInfo& SubmitInfo)
{
	if (!SubmitInfo.IsValid() || !mVkRuntime.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("EnsureIntermediateForFrame: invalid submit info or Vulkan runtime"));
		return false;
	}
	const VkFormat DesiredFormat = mVkRecordSession.SurfaceFormat;
    const uint32 DesiredWidth = mVkRecordSession.SurfaceExtent.width;
    const uint32 DesiredHeight = mVkRecordSession.SurfaceExtent.height;

    if (DesiredFormat == VK_FORMAT_UNDEFINED || DesiredWidth == 0 || DesiredHeight == 0)
    {
        return false;
    }

    FVdjmVkOwnedImageState& Intermediate = mVkRecordSession.IntermediateImageState;

    const bool bCanReuse =
        mVkRecordSession.bHasIntermediateImage &&
        Intermediate.Image != VK_NULL_HANDLE &&
        Intermediate.VkMemory != VK_NULL_HANDLE &&
        Intermediate.Format == DesiredFormat &&
        Intermediate.Width == DesiredWidth &&
        Intermediate.Height == DesiredHeight;

    if (bCanReuse)
    {
        return true;
    }

    if (Intermediate.View != VK_NULL_HANDLE)
    {
        vkDestroyImageView(mVkRuntime.VkDevice, Intermediate.View, nullptr);
        Intermediate.View = VK_NULL_HANDLE;
    }

    if (Intermediate.Image != VK_NULL_HANDLE)
    {
        vkDestroyImage(mVkRuntime.VkDevice, Intermediate.Image, nullptr);
        Intermediate.Image = VK_NULL_HANDLE;
    }

    if (Intermediate.VkMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(mVkRuntime.VkDevice, Intermediate.VkMemory, nullptr);
        Intermediate.VkMemory = VK_NULL_HANDLE;
    }

    Intermediate.Clear();
    mVkRecordSession.bHasIntermediateImage = false;

    VkImageCreateInfo ImageInfo{};
    ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.format = DesiredFormat;
    ImageInfo.extent = { DesiredWidth, DesiredHeight, 1 };
    ImageInfo.mipLevels = 1;
    ImageInfo.arrayLayers = 1;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult Result = vkCreateImage(mVkRuntime.VkDevice, &ImageInfo, nullptr, &Intermediate.Image);
    if (Result != VK_SUCCESS || Intermediate.Image == VK_NULL_HANDLE)
    {
        Intermediate.Clear();
        return false;
    }

    VkMemoryRequirements MemReq{};
    vkGetImageMemoryRequirements(mVkRuntime.VkDevice, Intermediate.Image, &MemReq);

    const uint32 MemoryTypeIndex = FVdjmVkHelper::FindMemoryType(
        mVkRuntime.VkPhysicalDevice,
        MemReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (MemoryTypeIndex == UINT32_MAX)
    {
        vkDestroyImage(mVkRuntime.VkDevice, Intermediate.Image, nullptr);
        Intermediate.Clear();
        return false;
    }

    VkMemoryAllocateInfo AllocInfo{};
    AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocInfo.allocationSize = MemReq.size;
    AllocInfo.memoryTypeIndex = MemoryTypeIndex;

    Result = vkAllocateMemory(mVkRuntime.VkDevice, &AllocInfo, nullptr, &Intermediate.VkMemory);
    if (Result != VK_SUCCESS || Intermediate.VkMemory == VK_NULL_HANDLE)
    {
        vkDestroyImage(mVkRuntime.VkDevice, Intermediate.Image, nullptr);
        Intermediate.Clear();
        return false;
    }

    Result = vkBindImageMemory(mVkRuntime.VkDevice, Intermediate.Image, Intermediate.VkMemory, 0);
    if (Result != VK_SUCCESS)
    {
        vkFreeMemory(mVkRuntime.VkDevice, Intermediate.VkMemory, nullptr);
        vkDestroyImage(mVkRuntime.VkDevice, Intermediate.Image, nullptr);
        Intermediate.Clear();
        return false;
    }

    Intermediate.Format = DesiredFormat;
    Intermediate.Width = DesiredWidth;
    Intermediate.Height = DesiredHeight;
    Intermediate.CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    mVkRecordSession.bHasIntermediateImage = true;
    return true;
}

bool FVdjmAndroidEncoderBackendVulkan::SubmitTextureViaIntermediate(const FVdjmVkSubmitFrameInfo& SubmitInfo,FVdjmVkFrameContext& FrameCtx, const FVdjmVkObservedSourceState& SourceState)
{
	if (!SubmitInfo.IsValid() || !FrameCtx.IsReady())
    {
        return false;
    }

    if (!mVkRecordSession.bHasIntermediateImage)
    {
        return false;
    }

    FVdjmVkOwnedImageState& Intermediate = mVkRecordSession.IntermediateImageState;
    if (Intermediate.Image == VK_NULL_HANDLE || Intermediate.VkMemory == VK_NULL_HANDLE)
    {
        return false;
    }

    if (!mVkRecordSession.SwapchainImages.IsValidIndex((int32)mCurrentSwapchainImageIndex32))
    {
        return false;
    }

    if (!SourceState.bLayoutKnown || SourceState.LastKnownLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        SetFailureReason(EVdjmVkFailureReason::SourceLayoutUnknown);
        return false;
    }

    const uint32 AcquiredImageIndex = mCurrentSwapchainImageIndex32;
    VkImage DstImage = mVkRecordSession.SwapchainImages[AcquiredImageIndex];
    VkImageLayout& DstLayoutRef = mVkRecordSession.SwapchainImageLayouts[AcquiredImageIndex];

    const VkImageLayout OriginalSrcLayout = SourceState.LastKnownLayout;
    const VkImageLayout CopySrcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VkResult Result = vkResetCommandBuffer(FrameCtx.CommandBuffer, 0);
    if (Result != VK_SUCCESS)
    {
        return false;
    }

    VkCommandBufferBeginInfo BeginInfo{};
    BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    Result = vkBeginCommandBuffer(FrameCtx.CommandBuffer, &BeginInfo);
    if (Result != VK_SUCCESS)
    {
        return false;
    }

    if (OriginalSrcLayout != CopySrcLayout)
    {
        FVdjmVkHelper::TransitionImageLayout(
            FrameCtx.CommandBuffer,
            SubmitInfo.SrcImage,
            SubmitInfo.SrcFormat,
            OriginalSrcLayout,
            CopySrcLayout);
    }

    if (!FVdjmVkHelper::TransitionOwnedImage(
            FrameCtx.CommandBuffer,
            Intermediate,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
    {
        return false;
    }

    VkImageCopy CopyRegion{};
    CopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    CopyRegion.srcSubresource.mipLevel = 0;
    CopyRegion.srcSubresource.baseArrayLayer = 0;
    CopyRegion.srcSubresource.layerCount = 1;

    CopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    CopyRegion.dstSubresource.mipLevel = 0;
    CopyRegion.dstSubresource.baseArrayLayer = 0;
    CopyRegion.dstSubresource.layerCount = 1;

    CopyRegion.extent.width = SubmitInfo.SrcWidth;
    CopyRegion.extent.height = SubmitInfo.SrcHeight;
    CopyRegion.extent.depth = 1;

    vkCmdCopyImage(
        FrameCtx.CommandBuffer,
        SubmitInfo.SrcImage,
        CopySrcLayout,
        Intermediate.Image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &CopyRegion);

    if (OriginalSrcLayout != CopySrcLayout)
    {
        FVdjmVkHelper::TransitionImageLayout(
            FrameCtx.CommandBuffer,
            SubmitInfo.SrcImage,
            SubmitInfo.SrcFormat,
            CopySrcLayout,
            OriginalSrcLayout);
    }

    if (!FVdjmVkHelper::TransitionOwnedImage(
            FrameCtx.CommandBuffer,
            Intermediate,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL))
    {
        return false;
    }

    FVdjmVkHelper::TransitionImageLayout(
        FrameCtx.CommandBuffer,
        DstImage,
        mVkRecordSession.SurfaceFormat,
        DstLayoutRef,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdCopyImage(
        FrameCtx.CommandBuffer,
        Intermediate.Image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        DstImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &CopyRegion);

    FVdjmVkHelper::TransitionImageLayout(
        FrameCtx.CommandBuffer,
        DstImage,
        mVkRecordSession.SurfaceFormat,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    Result = vkEndCommandBuffer(FrameCtx.CommandBuffer);
    if (Result != VK_SUCCESS)
    {
        return false;
    }

    Result = vkResetFences(mVkRuntime.VkDevice, 1, &FrameCtx.SubmitFence);
    if (Result != VK_SUCCESS)
    {
        return false;
    }

    VkPipelineStageFlags WaitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };

    VkSubmitInfo SubmitInfoVk{};
    SubmitInfoVk.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfoVk.waitSemaphoreCount = 1;
    SubmitInfoVk.pWaitSemaphores = &FrameCtx.AcquireCompleteSemaphore;
    SubmitInfoVk.pWaitDstStageMask = WaitStages;
    SubmitInfoVk.commandBufferCount = 1;
    SubmitInfoVk.pCommandBuffers = &FrameCtx.CommandBuffer;
    SubmitInfoVk.signalSemaphoreCount = 1;
    SubmitInfoVk.pSignalSemaphores = &FrameCtx.RenderCompleteSemaphore;

    Result = vkQueueSubmit(
        mVkRuntime.GraphicsQueue,
        1,
        &SubmitInfoVk,
        FrameCtx.SubmitFence);

    if (Result != VK_SUCCESS)
    {
        SetFailureReason(EVdjmVkFailureReason::SubmitFailed);
        return false;
    }

    CommitSourceStateAfterSubmit(SubmitInfo, OriginalSrcLayout);
    Intermediate.CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    mVkRecordSession.AdvanceToNextFrameContext();

    VkPresentInfoKHR PresentInfo{};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &FrameCtx.RenderCompleteSemaphore;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &mVkRecordSession.CodecSwapchain;
    PresentInfo.pImageIndices = &AcquiredImageIndex;

    Result = vkQueuePresentKHR(mVkRuntime.GraphicsQueue, &PresentInfo);
    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SetFailureReason(EVdjmVkFailureReason::SwapchainOutOfDate);
        return false;
    }
    if (Result == VK_ERROR_DEVICE_LOST)
    {
        SetFailureReason(EVdjmVkFailureReason::DeviceLost);
        return false;
    }
    if (Result != VK_SUCCESS && Result != VK_SUBOPTIMAL_KHR)
    {
        SetFailureReason(EVdjmVkFailureReason::PresentFailed);
        return false;
    }

    DstLayoutRef = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    SetFailureReason(EVdjmVkFailureReason::None);
    return true;
}

bool FVdjmAndroidEncoderBackendVulkan::InitVkRuntimeContext()
{
	//	runtime handle 확보 및 초기화
	if (mVkRuntime.bInitialized)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("InitVkRuntimeContext: Vulkan runtime context already initialized"));
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



bool FVdjmAndroidEncoderBackendVulkan::AcquireNextSwapchainImage(FVdjmVkFrameSubmitState& outFrameState)
{
	/*
	 * AcquireNextSwapchainImage 역할
	 *
	 * 1. 이전 프레임 submit이 끝났는지 fence로 확인한다.
	 *    - 지금 구현은 CommandBuffer / SubmitFence를 프레임마다 재사용하므로
	 *      새 프레임 시작 전에 이전 submit 완료를 기다려야 한다.
	 *
	 * 2. 이번 프레임이 써야 할 codec surface용 swapchain image index를 획득한다.
	 *    - swapchain은 이미지가 여러 장이므로, 매 프레임마다 현재 기록 가능한
	 *      destination image를 vkAcquireNextImageKHR로 받아와야 한다.
	 *
	 * 3. submit 단계에서 바로 사용할 frame-local 상태를 묶어서 반환한다.
	 *    - CommandBuffer
	 *    - AcquireCompleteSemaphore
	 *    - SubmitFence
	 *    - AcquiredImageIndex
	 *
	 * 이 함수는 "목적지 이미지 확보"까지만 담당한다.
	 * 실제 command recording / layout transition / copy / queue submit / present는
	 * SubmitTextureToCodecSurface()에서 처리한다.
	 */
	FVdjmVkFrameContext& FrameCtx = mVkRecordSession.GetCurrentFrameContext();
	outFrameState = FVdjmVkFrameSubmitState{};

	if (mVkRuntime.VkDevice == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AcquireNextSwapchainImage: VkDevice is null"));
		SetFailureReason(EVdjmVkFailureReason::DeviceLost);
		return false;
	}

	if (!mVkRecordSession.IsReadyToStart())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AcquireNextSwapchainImage: record session is not ready"));
		SetFailureReason(EVdjmVkFailureReason::SessionNotReady);
		return false;
	}

	if (FrameCtx.AcquireCompleteSemaphore == VK_NULL_HANDLE ||
		FrameCtx.SubmitFence == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AcquireNextSwapchainImage: sync objects are invalid"));
		SetFailureReason(EVdjmVkFailureReason::SyncObjectsInvalid);
		return false;
	}

	// 이전 프레임 submit 완료 대기
	VkResult vkResult = vkWaitForFences(
		mVkRuntime.VkDevice,
		1,
		&FrameCtx.SubmitFence,
		VK_TRUE,
		UINT64_MAX);

	if (vkResult != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("AcquireNextSwapchainImage: vkWaitForFences failed. Result=%d"),
		       (int32)vkResult);
		SetFailureReason(EVdjmVkFailureReason::FenceWaitFailed);
		return false;
	}
	

	uint32 AcquiredImageIndex = UINT32_MAX;

	vkResult = vkAcquireNextImageKHR(
		mVkRuntime.VkDevice,
		mVkRecordSession.CodecSwapchain,
		UINT64_MAX,
		FrameCtx.AcquireCompleteSemaphore,
		VK_NULL_HANDLE,
		&AcquiredImageIndex);

	if (vkResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("AcquireNextSwapchainImage: swapchain is out of date"));
		SetFailureReason(EVdjmVkFailureReason::SwapchainOutOfDate);
		return false;
	}
	
	
	if (vkResult == VK_ERROR_DEVICE_LOST)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("AcquireNextSwapchainImage: device lost"));
		SetFailureReason(EVdjmVkFailureReason::DeviceLost);
		return false;
	}
	
	if (vkResult != VK_SUCCESS && vkResult != VK_SUBOPTIMAL_KHR)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("AcquireNextSwapchainImage: vkAcquireNextImageKHR failed. Result=%d"),
		       (int32)vkResult);
		SetFailureReason(EVdjmVkFailureReason::AcquireFailed);
		return false;
	}
	
	SetFailureReason(EVdjmVkFailureReason::None);
	
	if (!mVkRecordSession.SwapchainImages.IsValidIndex((int32)AcquiredImageIndex))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("AcquireNextSwapchainImage: acquired image index is invalid. Index=%u ImageCount=%d"),
		       AcquiredImageIndex,
		       mVkRecordSession.SwapchainImages.Num());
		SetFailureReason(EVdjmVkFailureReason::AcquireFailed);
		return false;
	}

	outFrameState.CommandBuffer = FrameCtx.CommandBuffer;
	outFrameState.AcquireCompleteSemaphore = FrameCtx.AcquireCompleteSemaphore;
	outFrameState.RenderCompleteSemaphore = FrameCtx.RenderCompleteSemaphore;
	outFrameState.SubmitFence = FrameCtx.SubmitFence;
	outFrameState.AcquiredImageIndex = AcquiredImageIndex;

	mCurrentSwapchainImageIndex32 = AcquiredImageIndex;

	UE_LOG(LogVdjmRecorderCore, VeryVerbose,
	       TEXT("AcquireNextSwapchainImage: success. Result=%d ImageIndex=%u"),
	       (int32)vkResult,
	       AcquiredImageIndex);
	SetFailureReason(EVdjmVkFailureReason::None);
	//mVkRecordSession.AdvanceToNextFrameContext();
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::SubmitTextureToCodecSurface(const FVdjmVkSubmitFrameInfo& submitInfo, FVdjmVkFrameSubmitState& frameState)
{
	/*
	 * SubmitTextureToCodecSurface 역할
	 *
	 * 1. AcquireNextSwapchainImage()에서 확보한 destination swapchain image에
	 *    현재 프레임 source image를 복사한다.
	 *
	 * 2. 이 함수가 담당하는 범위
	 *    - command buffer reset / begin / end
	 *    - destination image layout transition
	 *    - image copy
	 *    - queue submit
	 *    - present
	 *
	 * 3. 현재 단계의 임시 정책
	 *    - source image는 일단 VK_IMAGE_LAYOUT_GENERAL 상태라고 가정한다.
	 *    - source layout 추적은 아직 하지 않는다.
	 *    - submit 완료를 fence로 기다린 뒤 present를 호출한다.
	 *    - render-complete semaphore는 아직 추가하지 않는다.
	 *
	 * 4. 나중에 반드시 고쳐야 하는 것
	 *    - source image 실제 layout 추적
	 *    - render-complete semaphore 기반 present 동기화
	 *    - intermediate copy 경로
	 */

	if (!submitInfo.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("SubmitTextureToCodecSurface: submitInfo is invalid"));
		return false;
	}

	if (!mVkRecordSession.SwapchainImages.IsValidIndex((int32)frameState.AcquiredImageIndex))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("SubmitTextureToCodecSurface: invalid acquired image index. Index=%u ImageCount=%d"),
		       frameState.AcquiredImageIndex,
		       mVkRecordSession.SwapchainImages.Num());
		return false;
	}

	if (not frameState.IsFrameStateHandlesValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("SubmitTextureToCodecSurface: frame state handles are invalid"));
		return false;
	}
	
	
	FVdjmVkObservedSourceState SourceState{};
	if (!TryResolveSourceState(submitInfo, SourceState))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("SubmitTextureToCodecSurface: failed to resolve source state"));
		return false;
	}

	const VkImageLayout OriginalSrcLayout = SourceState.LastKnownLayout;
	const VkImageLayout CopySrcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	VkImage DstImage = mVkRecordSession.SwapchainImages[frameState.AcquiredImageIndex];
	VkImageLayout& DstLayoutRef = mVkRecordSession.SwapchainImageLayouts[frameState.AcquiredImageIndex];

	if (DstImage == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("SubmitTextureToCodecSurface: destination swapchain image is null"));
		return false;
	}

	VkResult vkResult = vkResetCommandBuffer(frameState.CommandBuffer, 0);
	if (vkResult != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("SubmitTextureToCodecSurface: vkResetCommandBuffer failed. Result=%d"),
		       (int32)vkResult);
		return false;
	}

	VkCommandBufferBeginInfo BeginInfo{};
	BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkResult = vkBeginCommandBuffer(frameState.CommandBuffer, &BeginInfo);
	if (vkResult != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("SubmitTextureToCodecSurface: vkBeginCommandBuffer failed. Result=%d"),
		       (int32)vkResult);
		return false;
	}
	
	if (OriginalSrcLayout != CopySrcLayout)
	{
		FVdjmVkHelper::TransitionImageLayout(
			frameState.CommandBuffer,
			submitInfo.SrcImage,
			submitInfo.SrcFormat,
			OriginalSrcLayout,
			CopySrcLayout);
	}
	
	// destination만 우리가 확실히 추적한다.
	FVdjmVkHelper::TransitionImageLayout(
		frameState.CommandBuffer,
		DstImage,
		mVkRecordSession.SurfaceFormat,
		DstLayoutRef,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkImageCopy CopyRegion{};
	CopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	CopyRegion.srcSubresource.mipLevel = 0;
	CopyRegion.srcSubresource.baseArrayLayer = 0;
	CopyRegion.srcSubresource.layerCount = 1;

	CopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	CopyRegion.dstSubresource.mipLevel = 0;
	CopyRegion.dstSubresource.baseArrayLayer = 0;
	CopyRegion.dstSubresource.layerCount = 1;

	CopyRegion.extent.width = submitInfo.SrcWidth;
	CopyRegion.extent.height = submitInfo.SrcHeight;
	CopyRegion.extent.depth = 1;

	// 현재 단계의 임시 정책:
	// source는 GENERAL이라고 가정하고 복사한다.
	vkCmdCopyImage(
		frameState.CommandBuffer,
		submitInfo.SrcImage,
		CopySrcLayout,
		DstImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&CopyRegion);
	
	if (OriginalSrcLayout != CopySrcLayout)
	{
		FVdjmVkHelper::TransitionImageLayout(
			frameState.CommandBuffer,
			submitInfo.SrcImage,
			submitInfo.SrcFormat,
			CopySrcLayout,
			OriginalSrcLayout);
	}
	
	FVdjmVkHelper::TransitionImageLayout(
		frameState.CommandBuffer,
		DstImage,
		mVkRecordSession.SurfaceFormat,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	vkResult = vkEndCommandBuffer(frameState.CommandBuffer);
	if (vkResult != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("SubmitTextureToCodecSurface: vkEndCommandBuffer failed. Result=%d"),
		       (int32)vkResult);
		return false;
	}
	
	vkResult = vkResetFences(	mVkRuntime.VkDevice,1,	&frameState.SubmitFence);
	if (vkResult != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,TEXT("SubmitTextureToCodecSurface: vkResetFences failed. Result=%d"),
		       (int32)vkResult);
		return false;
	}

	VkPipelineStageFlags WaitStages[] =
	{
		VK_PIPELINE_STAGE_TRANSFER_BIT
	};

	VkSubmitInfo SubmitInfoVk{};
	SubmitInfoVk.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfoVk.waitSemaphoreCount = 1;
	SubmitInfoVk.pWaitSemaphores = &frameState.AcquireCompleteSemaphore;
	SubmitInfoVk.pWaitDstStageMask = WaitStages;
	SubmitInfoVk.commandBufferCount = 1;
	SubmitInfoVk.pCommandBuffers = &frameState.CommandBuffer;
	SubmitInfoVk.signalSemaphoreCount = 1;
	SubmitInfoVk.pSignalSemaphores = &frameState.RenderCompleteSemaphore;


	vkResult = vkQueueSubmit(
		mVkRuntime.GraphicsQueue,
		1,
		&SubmitInfoVk,
		frameState.SubmitFence);
	if (vkResult != VK_SUCCESS)
	{
		SetFailureReason(EVdjmVkFailureReason::SubmitFailed);
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("SubmitTextureToCodecSurface: vkQueueSubmit failed. Result=%d Queue=%p Cmd=%p Fence=%p AcquireSem=%p"),
		       (int32)vkResult,
		       mVkRuntime.GraphicsQueue,
		       frameState.CommandBuffer,
		       frameState.SubmitFence,
		       frameState.AcquireCompleteSemaphore);
		return false;
	}
	CommitSourceStateAfterSubmit(submitInfo, OriginalSrcLayout);
	mVkRecordSession.AdvanceToNextFrameContext();
	
	VkPresentInfoKHR PresentInfo{};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	PresentInfo.pWaitSemaphores = &frameState.RenderCompleteSemaphore;
	PresentInfo.swapchainCount = 1;
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pSwapchains = &mVkRecordSession.CodecSwapchain;
	PresentInfo.pImageIndices = &frameState.AcquiredImageIndex;
	PresentInfo.pResults = nullptr;

	vkResult = vkQueuePresentKHR(mVkRuntime.GraphicsQueue, &PresentInfo);
	
	if (vkResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
		SetFailureReason(EVdjmVkFailureReason::SwapchainOutOfDate);
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("SubmitTextureToCodecSurface: vkQueuePresentKHR out of date"));
		return false;
	}
	
	if (vkResult == VK_ERROR_DEVICE_LOST)
	{
		SetFailureReason(EVdjmVkFailureReason::DeviceLost);
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("SubmitTextureToCodecSurface: device lost during present"));
		return false;
	}
	
	if (vkResult != VK_SUCCESS && vkResult != VK_SUBOPTIMAL_KHR)
	{
		SetFailureReason(EVdjmVkFailureReason::PresentFailed);
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("SubmitTextureToCodecSurface: vkQueuePresentKHR failed. Result=%d"),
		       (int32)vkResult);
		return false;
	}

	DstLayoutRef = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	SetFailureReason(EVdjmVkFailureReason::None);
	//CommitSourceStateAfterSubmit(submitInfo, SourceState.LastKnownLayout);
	UE_LOG(LogVdjmRecorderCore, VeryVerbose,
	       TEXT("SubmitTextureToCodecSurface: success. SrcImage=%p DstImage=%p ImageIndex=%u"),
	       submitInfo.SrcImage,
	       DstImage,
	       frameState.AcquiredImageIndex);

	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::DrainInFlightFrames()
{
	mVkRecordSession.bStopRequested = true;

	for (FVdjmVkFrameContext& FrameCtx : mVkRecordSession.FrameContexts)
	{
		if (FrameCtx.SubmitFence != VK_NULL_HANDLE)
		{
			VkResult Result = vkWaitForFences(
				mVkRuntime.VkDevice,
				1,
				&FrameCtx.SubmitFence,
				VK_TRUE,
				UINT64_MAX);

			if (Result != VK_SUCCESS)
			{
				return false;
			}

			FrameCtx.bInFlight = false;
		}
	}

	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::TryResolveSourceState(
	const FVdjmVkSubmitFrameInfo& SubmitInfo,
	FVdjmVkObservedSourceState& OutState)
{
	OutState.Clear();

	if (!SubmitInfo.IsValid())
	{
		SetFailureReason(EVdjmVkFailureReason::SourceHandleResolveFailed);
		return false;
	}

	const uint64 SourceKey = (uint64)SubmitInfo.SrcImage;
	FVdjmVkObservedSourceState* CachedState = mVkRecordSession.SourceStateCache.Find(SourceKey);

	if (CachedState == nullptr)
	{
		FVdjmVkObservedSourceState SeedState{};
		SeedState.Format = SubmitInfo.SrcFormat;
		SeedState.Extent = { SubmitInfo.SrcWidth, SubmitInfo.SrcHeight };
		SeedState.LastKnownLayout = VK_IMAGE_LAYOUT_GENERAL;	// 임시
		SeedState.bLayoutKnown = true;						// 임시
		SeedState.LastSeenSerial = mVkRecordSession.SubmissionSerial;

		mVkRecordSession.SourceStateCache.Add(SourceKey, SeedState);
		OutState = SeedState;
		return true;
	}

	if (CachedState->Format != VK_FORMAT_UNDEFINED && CachedState->Format != SubmitInfo.SrcFormat)
	{
		SetFailureReason(EVdjmVkFailureReason::SourceFormatMismatch);
		return false;
	}

	if (CachedState->Extent.width != 0 && CachedState->Extent.height != 0 &&
		(CachedState->Extent.width != SubmitInfo.SrcWidth || CachedState->Extent.height != SubmitInfo.SrcHeight))
	{
		SetFailureReason(EVdjmVkFailureReason::SourceExtentMismatch);
		return false;
	}

	if (!CachedState->bLayoutKnown || CachedState->LastKnownLayout == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		SetFailureReason(EVdjmVkFailureReason::SourceLayoutUnknown);
		return false;
	}

	CachedState->Format = SubmitInfo.SrcFormat;
	CachedState->Extent = { SubmitInfo.SrcWidth, SubmitInfo.SrcHeight };
	CachedState->LastSeenSerial = mVkRecordSession.SubmissionSerial;

	OutState = *CachedState;
	return true;
}

void FVdjmAndroidEncoderBackendVulkan::CommitSourceStateAfterSubmit(
	const FVdjmVkSubmitFrameInfo& SubmitInfo,
	VkImageLayout NewLayout)
{
	if (!SubmitInfo.IsValid() || NewLayout == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		return;
	}

	const uint64 SourceKey = (uint64)SubmitInfo.SrcImage;
	FVdjmVkObservedSourceState& State = mVkRecordSession.SourceStateCache.FindOrAdd(SourceKey);

	State.Format = SubmitInfo.SrcFormat;
	State.Extent = { SubmitInfo.SrcWidth, SubmitInfo.SrcHeight };
	State.LastKnownLayout = NewLayout;
	State.bLayoutKnown = true;
	State.LastSeenSerial = ++mVkRecordSession.SubmissionSerial;
}

void FVdjmAndroidEncoderBackendVulkan::SetFailureReason(EVdjmVkFailureReason Reason)
{
	mLastFailureReason = Reason;
}
#endif
