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
			TEXT("Analyze: unsupported UE pixel format = %d"),
			(int32)srcTexture->GetFormat());
		return false;
	}

	return true;
}

/*
 *	class FVdjmAndroidEncoderBackendVulkan : public FVdjmAndroidEncoderBackend
 */
/*
 * Backend helpers
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



FVdjmAndroidEncoderBackendVulkan::FVdjmAndroidEncoderBackendVulkan()
	: mAnalyzer(this),
	mInitialized(false), mStarted(false), mPaused(false), mRuntimeReady(false)
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

	if (mVkRuntime.VkDevice != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(mVkRuntime.VkDevice);
	}
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

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	if (!IsRunnable() || !srcTexture.IsValid())
	{
		return false;
	}

	FVdjmVkSubmitFrameInfo SubmitInfo{};
	if (!mAnalyzer.Analyze(srcTexture, SubmitInfo))
	{
		return false;
	}

	const bool bExactSize =
		SubmitInfo.SrcWidth == mVkRecordSession.SurfaceExtent.width &&
		SubmitInfo.SrcHeight == mVkRecordSession.SurfaceExtent.height;

	const bool bExactFormat =
		SubmitInfo.SrcFormat == mVkRecordSession.SurfaceFormat;

	SubmitInfo.bCanDirectCopy = bExactSize && bExactFormat;
	SubmitInfo.bNeedsIntermediate = !SubmitInfo.bCanDirectCopy;

	if (!SubmitInfo.bCanDirectCopy)
	{
		return false;
	}

	FVdjmVkFrameSubmitState FrameState{};
	if (!AcquireNextSwapchainImage(FrameState))
	{
		return false;
	}

	return SubmitTextureToCodecSurface(SubmitInfo, FrameState);
}



bool FVdjmAndroidEncoderBackendVulkan::AcquireNextSwapchainImage(FVdjmVkFrameSubmitState& outFrameState)
{

}

bool FVdjmAndroidEncoderBackendVulkan::CreateRecordSessionVkResources()
{
	
}

void FVdjmAndroidEncoderBackendVulkan::DestroyRecordSessionVkResources()
{
	VkDevice Device = mVkRuntime.VkDevice;
	if (Device == VK_NULL_HANDLE)
	{
		mVkRecordSession.Clear();
		mCurrentSwapchainImageIndex32 = UINT32_MAX;
		return;
	}

	if (mVkRecordSession.SubmitFence != VK_NULL_HANDLE)
	{
		vkDestroyFence(Device, mVkRecordSession.SubmitFence, nullptr);
		mVkRecordSession.SubmitFence = VK_NULL_HANDLE;
	}

	if (mVkRecordSession.AcquireSemaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(Device, mVkRecordSession.AcquireSemaphore, nullptr);
		mVkRecordSession.AcquireSemaphore = VK_NULL_HANDLE;
	}

	if (mVkRecordSession.CommandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(Device, mVkRecordSession.CommandPool, nullptr);
		mVkRecordSession.CommandPool = VK_NULL_HANDLE;
		mVkRecordSession.CommandBuffer = VK_NULL_HANDLE;
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

	mVkRecordSession.Clear();
	mCurrentSwapchainImageIndex32 = UINT32_MAX;
}

bool FVdjmAndroidEncoderBackendVulkan::TryExtractNativeVkImage(const FTextureRHIRef& srcTexture,VkImage& outImage) const
{
	
}

bool FVdjmAndroidEncoderBackendVulkan::SubmitTextureToCodecSurface(const FVdjmVkSubmitFrameInfo& submitInfo, FVdjmVkFrameSubmitState& frameState)
{
	
	return true;
}
#endif