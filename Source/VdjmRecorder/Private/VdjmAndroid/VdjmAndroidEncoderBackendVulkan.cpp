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
	outFrameState = FVdjmVkFrameSubmitState{};

	if (mVkRuntime.VkDevice == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AcquireNextSwapchainImage: VkDevice is null"));
		return false;
	}

	if (!mVkRecordSession.IsReadyToStart())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AcquireNextSwapchainImage: record session is not ready"));
		return false;
	}

	if (mVkRecordSession.AcquireSemaphore == VK_NULL_HANDLE ||
		mVkRecordSession.SubmitFence == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("AcquireNextSwapchainImage: sync objects are invalid"));
		return false;
	}

	// 이전 프레임 submit 완료 대기
	VkResult Result = vkWaitForFences(
		mVkRuntime.VkDevice,
		1,
		&mVkRecordSession.SubmitFence,
		VK_TRUE,
		UINT64_MAX);

	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("AcquireNextSwapchainImage: vkWaitForFences failed. Result=%d"),
			(int32)Result);
		return false;
	}

	// 새 프레임 submit을 위해 fence reset
	Result = vkResetFences(
		mVkRuntime.VkDevice,
		1,
		&mVkRecordSession.SubmitFence);

	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("AcquireNextSwapchainImage: vkResetFences failed. Result=%d"),
			(int32)Result);
		return false;
	}

	uint32 AcquiredImageIndex = UINT32_MAX;

	Result = vkAcquireNextImageKHR(
		mVkRuntime.VkDevice,
		mVkRecordSession.CodecSwapchain,
		UINT64_MAX,
		mVkRecordSession.AcquireSemaphore,
		VK_NULL_HANDLE,
		&AcquiredImageIndex);

	if (Result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("AcquireNextSwapchainImage: swapchain is out of date"));
		return false;
	}

	if (Result != VK_SUCCESS && Result != VK_SUBOPTIMAL_KHR)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("AcquireNextSwapchainImage: vkAcquireNextImageKHR failed. Result=%d"),
			(int32)Result);
		return false;
	}

	if (!mVkRecordSession.SwapchainImages.IsValidIndex((int32)AcquiredImageIndex))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("AcquireNextSwapchainImage: acquired image index is invalid. Index=%u ImageCount=%d"),
			AcquiredImageIndex,
			mVkRecordSession.SwapchainImages.Num());
		return false;
	}

	outFrameState.CommandBuffer = mVkRecordSession.CommandBuffer;
	outFrameState.AcquireCompleteSemaphore = mVkRecordSession.AcquireSemaphore;
	outFrameState.SubmitFence = mVkRecordSession.SubmitFence;
	outFrameState.AcquiredImageIndex = AcquiredImageIndex;

	mCurrentSwapchainImageIndex32 = AcquiredImageIndex;

	UE_LOG(LogVdjmRecorderCore, VeryVerbose,
		TEXT("AcquireNextSwapchainImage: success. Result=%d ImageIndex=%u"),
		(int32)Result,
		AcquiredImageIndex);

	return true;
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

	VkCommandPoolCreateInfo PoolInfo{};
	PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	PoolInfo.queueFamilyIndex = mVkRuntime.GraphicsQueueFamilyIndex;
	PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	Result = vkCreateCommandPool(
		mVkRuntime.VkDevice,
		&PoolInfo,
		nullptr,
		&mVkRecordSession.CommandPool);

	if (Result != VK_SUCCESS || mVkRecordSession.CommandPool == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("CreateRecordSessionVkResources: vkCreateCommandPool failed. Result=%d"),
			(int32)Result);
		DestroyRecordSessionVkResources();
		return false;
	}

	VkCommandBufferAllocateInfo CmdAllocInfo{};
	CmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	CmdAllocInfo.commandPool = mVkRecordSession.CommandPool;
	CmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	CmdAllocInfo.commandBufferCount = 1;

	Result = vkAllocateCommandBuffers(
		mVkRuntime.VkDevice,
		&CmdAllocInfo,
		&mVkRecordSession.CommandBuffer);

	if (Result != VK_SUCCESS || mVkRecordSession.CommandBuffer == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("CreateRecordSessionVkResources: vkAllocateCommandBuffers failed. Result=%d"),
			(int32)Result);
		DestroyRecordSessionVkResources();
		return false;
	}

	VkSemaphoreCreateInfo SemaphoreInfo{};
	SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	Result = vkCreateSemaphore(
		mVkRuntime.VkDevice,
		&SemaphoreInfo,
		nullptr,
		&mVkRecordSession.AcquireSemaphore);

	if (Result != VK_SUCCESS || mVkRecordSession.AcquireSemaphore == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("CreateRecordSessionVkResources: vkCreateSemaphore failed. Result=%d"),
			(int32)Result);
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
		&mVkRecordSession.SubmitFence);

	if (Result != VK_SUCCESS || mVkRecordSession.SubmitFence == VK_NULL_HANDLE)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("CreateRecordSessionVkResources: vkCreateFence failed. Result=%d"),
			(int32)Result);
		DestroyRecordSessionVkResources();
		return false;
	}

	mCurrentSwapchainImageIndex32 = UINT32_MAX;

	UE_LOG(LogVdjmRecorderCore, Warning,
		TEXT("CreateRecordSessionVkResources: success. Format=%d Extent=(%u,%u) Images=%d"),
		(int32)mVkRecordSession.SurfaceFormat,
		mVkRecordSession.SurfaceExtent.width,
		mVkRecordSession.SurfaceExtent.height,
		mVkRecordSession.SwapchainImages.Num());

	return true;
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

bool FVdjmAndroidEncoderBackendVulkan::SubmitTextureToCodecSurface(const FVdjmVkSubmitFrameInfo& submitInfo, FVdjmVkFrameSubmitState& frameState)
{
	
	return true;
}
#endif