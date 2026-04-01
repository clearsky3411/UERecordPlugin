// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
#include "vulkan_core.h"

struct IVulkanDynamicRHI;
class FVdjmAndroidEncoderBackendVulkan;

/*
 * 1. GPU Copy/render
 * 2. codec input surface
 * 3. codec encoding
 * Start trigger 시
 * - Session 생성
 * - 
 * 
 */
namespace VdjmVkUtil
{
	static constexpr VkImageAspectFlags GColorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
	
	static bool CheckVkResult(VkResult result,const TCHAR* context)
	{
		if (result != VK_SUCCESS)
		{
			UE_LOG(LogVdjmRecorderCore, Error,
				TEXT("%s - Vulkan call failed. VkResult=%d"), context, (int32)result);
			return false;
		}
		return true;
	}
	static VkSurfaceFormatKHR ChooseSurfaceFormat(const TArray<VkSurfaceFormatKHR>& formats)
	{
		if (formats.Num() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		{
			VkSurfaceFormatKHR fallback{};
			fallback.format = VK_FORMAT_R8G8B8A8_UNORM;
			fallback.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			return fallback;
		}

		for (const VkSurfaceFormatKHR& format : formats)
		{
			if (format.format == VK_FORMAT_R8G8B8A8_UNORM)
			{
				return format;
			}
		}

		for (const VkSurfaceFormatKHR& format : formats)
		{
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				return format;
			}
		}

		return formats.Num() > 0 ? formats[0] : VkSurfaceFormatKHR{};
	}

	static VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(VkCompositeAlphaFlagsKHR supportedFlags)
	{
		if (supportedFlags & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		{
			return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		}
		if (supportedFlags & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
		{
			return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
		}
		if (supportedFlags & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
		{
			return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
		}
		if (supportedFlags & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
		{
			return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
		}
		return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}

	static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32 desiredWidth, uint32 desiredHeight)
	{
		if (caps.currentExtent.width != UINT32_MAX)
		{
			return caps.currentExtent;
		}

		VkExtent2D extent{};
		extent.width = FMath::Clamp(desiredWidth, caps.minImageExtent.width, caps.maxImageExtent.width);
		extent.height = FMath::Clamp(desiredHeight, caps.minImageExtent.height, caps.maxImageExtent.height);
		return extent;
	}
}


/**
 * @brief Vulkan 관련 핸들들을 관리하는 클래스. IVulkanDynamicRHI에서 필요한 핸들들을 추출하여 저장하고, 초기화 및 유효성 검사를 담당.
 * @class FVdjmVkRecoderHandles
 * @note IVulkanDynamicRHI에서 핸들을 추출하여 저장하는 역할을 담당하며, 초기화 및 유효성 검사 기능을 제공. Vulkan 관련 작업을 수행하는 클래스에서 이 핸들들을 사용하여 Vulkan API 호출을 수행할 수 있도록 지원.
 */
class FVdjmVkRecoderHandles
{
public:
	FVdjmVkRecoderHandles() = default;
	~FVdjmVkRecoderHandles() = default;
	
	bool InitializeHandles();
	bool EnsureInitialized();
	
	IVulkanDynamicRHI* GetVulkanRHI() const { return mVulkanRHI; }
	VkInstance GetVkInstance() const { return mVkInstance; }
	VkPhysicalDevice GetVkPhysicalDevice() const { return mVkPhysicalDevice; }
	VkDevice GetVkDevice() const { return mVkDevice; }
	VkQueue GetGraphicsQueue() const { return mGraphicsQueue; }
	uint32 GetGraphicsQueueIndex() const { return mGraphicsQueueIndex; }
	uint32 GetGraphicsQueueFamilyIndex() const { return mGraphicsQueueFamilyIndex; }
	
	void Clear()
	{
		mVulkanRHI = nullptr;
		mVkInstance = VK_NULL_HANDLE;
		mVkPhysicalDevice = VK_NULL_HANDLE;
		mVkDevice = VK_NULL_HANDLE;
		mGraphicsQueue = VK_NULL_HANDLE;
		mGraphicsQueueIndex = UINT32_MAX;
		mGraphicsQueueFamilyIndex = UINT32_MAX;
	}
	bool IsValid() const
	{
		return mVulkanRHI != nullptr && mVkInstance != VK_NULL_HANDLE && mVkPhysicalDevice != VK_NULL_HANDLE && mVkDevice != VK_NULL_HANDLE && mGraphicsQueue != VK_NULL_HANDLE && mGraphicsQueueFamilyIndex != UINT32_MAX && mGraphicsQueueIndex != UINT32_MAX;
	}
	bool NeedReInit() const
	{
		return not IsValid();
	}
	FString ToString() const
	{
		return FString::Printf(TEXT("Vulkan Handles - Instance: %p, PhysicalDevice: %p, Device: %p, GraphicsQueue: %p, GraphicsQueueIndex: %u, GraphicsQueueFamilyIndex: %u"),
			(void*)mVkInstance,
			(void*)mVkPhysicalDevice,
			(void*)mVkDevice,
			(void*)mGraphicsQueue,
			mGraphicsQueueIndex,
			mGraphicsQueueFamilyIndex);
	}
	
private:
	bool InitializeFromDynamicRHI(IVulkanDynamicRHI* vulkanRHI);
	
	IVulkanDynamicRHI* mVulkanRHI = nullptr;

	VkInstance mVkInstance = VK_NULL_HANDLE;
	VkPhysicalDevice mVkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice mVkDevice = VK_NULL_HANDLE;
	VkQueue mGraphicsQueue = VK_NULL_HANDLE;
	uint32 mGraphicsQueueIndex = UINT32_MAX;
	uint32 mGraphicsQueueFamilyIndex = UINT32_MAX;
};

/**
 * @brief Vulkan 프레임 리소스 구조체. 각 프레임마다 필요한 Vulkan 리소스들을 관리하는 구조체로, 명령 버퍼, 동기 객체 등을 포함.
 * @struct FVdjmVkFrameResources
 */
struct FVdjmVkFrameResources
{
	VkCommandPool CommandPool = VK_NULL_HANDLE;
	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
	VkFence SubmitFence = VK_NULL_HANDLE;
	VkSemaphore ImageAcquiredSemaphore = VK_NULL_HANDLE;
	VkSemaphore RenderCompleteSemaphore = VK_NULL_HANDLE;

	void Clear()
	{
		CommandPool = VK_NULL_HANDLE;
		CommandBuffer = VK_NULL_HANDLE;
		SubmitFence = VK_NULL_HANDLE;
		ImageAcquiredSemaphore = VK_NULL_HANDLE;
		RenderCompleteSemaphore = VK_NULL_HANDLE;
	}

	bool IsValid() const
	{
		return
			CommandPool != VK_NULL_HANDLE &&
			CommandBuffer != VK_NULL_HANDLE &&
			SubmitFence != VK_NULL_HANDLE &&
			ImageAcquiredSemaphore != VK_NULL_HANDLE &&
			RenderCompleteSemaphore != VK_NULL_HANDLE;
	}
};

/**
 * @brief Vulkan 입력 서피스 상태 관리 클래스. Vulkan을 사용하여 코덱 입력 서피스를 관리하는 클래스이며, 서피스 및 스왑체인 생성, 프레임 리소스 관리 등을 담당.
 * @class FVdjmVkCodecInputSurfaceState
 */
class FVdjmVkCodecInputSurfaceState
{
public:
	bool InitializeSurfaceState(
		const FVdjmVkRecoderHandles& vkHandles,
		ANativeWindow* inputWindow,
		const FVdjmAndroidEncoderConfigure& config);

	void ReleaseSurfaceState(const FVdjmVkRecoderHandles& vkHandles);
	
	void Clear()
	{
		mSurface = VK_NULL_HANDLE;
		mSwapchain = VK_NULL_HANDLE;
		mSurfaceFormat = {};
		mExtent = {0, 0};
		mPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		mMinImageCount = 0;
		mCurrentFrameIndex = 0;
		mCurrentSwapchainImageIndex = UINT32_MAX;
		mSwapchainImages.Reset();
		mSwapchainImageViews.Reset();
		mFrames.Reset();
	}

	bool IsValid() const
	{
		return
			mSurface != VK_NULL_HANDLE &&
			mSwapchain != VK_NULL_HANDLE &&
			mSurfaceFormat.format != VK_FORMAT_UNDEFINED &&
			mExtent.width > 0 &&
			mExtent.height > 0 &&
			mSwapchainImages.Num() > 0 &&
			mFrames.Num() > 0;
	}
	bool NeedRecreate(uint32 width, uint32 height) const
	{
		return
			!IsValid() ||
			mExtent.width != width ||
			mExtent.height != height;
	}
	FVdjmVkFrameResources* GetCurrentFrameResources()
	{
		return mFrames.IsValidIndex((int32)mCurrentFrameIndex) ? &mFrames[(int32)mCurrentFrameIndex] : nullptr;
	}

	const FVdjmVkFrameResources* GetCurrentFrameResources() const
	{
		return mFrames.IsValidIndex((int32)mCurrentFrameIndex) ? &mFrames[(int32)mCurrentFrameIndex] : nullptr;
	}
	void AdvanceFrame()
	{
		if (mFrames.Num() > 0)
		{
			mCurrentFrameIndex = (mCurrentFrameIndex + 1) % (uint32)mFrames.Num();
		}
	}
	VkSurfaceKHR GetSurface() const { return mSurface; }
	VkSwapchainKHR GetSwapchain() const { return mSwapchain; }
	const VkSurfaceFormatKHR& GetSurfaceFormat() const { return mSurfaceFormat; }
	VkExtent2D GetExtent() const { return mExtent; }
	const TArray<VkImage>& GetSwapchainImages() const { return mSwapchainImages; }
	const TArray<VkImageView>& GetSwapchainImageViews() const { return mSwapchainImageViews; }
	uint32 GetCurrentFrameIndex() const { return mCurrentFrameIndex; }
	uint32 GetCurrentSwapchainImageIndex() const { return mCurrentSwapchainImageIndex; }
	void SetCurrentSwapchainImageIndex(uint32 imageIndex) { mCurrentSwapchainImageIndex = imageIndex; }
private:
	bool CreateSurface(
		const FVdjmVkRecoderHandles& vkHandles,
		ANativeWindow* inputWindow);

	bool CreateSwapchain(
		const FVdjmVkRecoderHandles& vkHandles,
		const FVdjmAndroidEncoderConfigure& config);

	bool CreatePerFrameResources(const FVdjmVkRecoderHandles& vkHandles);

	void ReleasePerFrameResources(VkDevice vkDevice);
	void ReleaseSwapchain(VkDevice vkDevice);
	void ReleaseSurface(VkInstance vkInstance);
	
	VkSurfaceKHR mSurface = VK_NULL_HANDLE;
	VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
	VkSurfaceFormatKHR mSurfaceFormat = {};
	VkExtent2D mExtent = {0, 0};
	VkPresentModeKHR mPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	uint32 mMinImageCount = 0;
	uint32 mCurrentFrameIndex = 0;
	uint32 mCurrentSwapchainImageIndex = UINT32_MAX;

	TArray<VkImage> mSwapchainImages;
	TArray<VkImageView> mSwapchainImageViews;
	TArray<FVdjmVkFrameResources> mFrames;

};

/**
 * @brief Vulkan 중간 상태 관리 클래스. Vulkan을 사용하여 인코딩 과정에서 필요한 중간 이미지 리소스를 관리하는 클래스이며, 이미지 생성, 메모리 할당, 이미지 뷰 생성 등을 담당.
 * @class FVdjmVkIntermediateState
 */
class FVdjmVkIntermediateState
{
public:
	bool EnsureCreated(
		const FVdjmVkRecoderHandles& vkHandles,
		VkFormat format,
		uint32 width,
		uint32 height,
		VkImageUsageFlags extraUsageFlags = 0);

	void Release(const FVdjmVkRecoderHandles& vkHandles);

	void Clear()
	{
		mImage = VK_NULL_HANDLE;
		mMemory = VK_NULL_HANDLE;
		mImageView = VK_NULL_HANDLE;
		mFormat = VK_FORMAT_UNDEFINED;
		mWidth = 0;
		mHeight = 0;
		mUsageFlags = 0;
		mCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	bool IsValid() const
	{
		return
			mImage != VK_NULL_HANDLE &&
			mMemory != VK_NULL_HANDLE &&
			mImageView != VK_NULL_HANDLE &&
			mFormat != VK_FORMAT_UNDEFINED &&
			mWidth > 0 &&
			mHeight > 0;
	}
	bool Matches(VkFormat format, uint32 width, uint32 height) const
	{
		return
			IsValid() &&
			mFormat == format &&
			mWidth == width &&
			mHeight == height;
	}
	VkImage GetImage() const { return mImage; }
	VkImageView GetImageView() const { return mImageView; }
	VkDeviceMemory GetMemory() const { return mMemory; }
	VkFormat GetFormat() const { return mFormat; }
	uint32 GetWidth() const { return mWidth; }
	uint32 GetHeight() const { return mHeight; }
	VkImageLayout GetCurrentLayout() const { return mCurrentLayout; }
	void SetCurrentLayout(VkImageLayout newLayout) { mCurrentLayout = newLayout; }
private:
	bool CreateImage(
		const FVdjmVkRecoderHandles& vkHandles,
		VkFormat format,
		uint32 width,
		uint32 height,
		VkImageUsageFlags extraUsageFlags);

	uint32 FindMemoryTypeIndex(
		const FVdjmVkRecoderHandles& vkHandles,
		uint32 typeBits,
		VkMemoryPropertyFlags requiredFlags) const;
	
	VkImage mImage = VK_NULL_HANDLE;
	VkDeviceMemory mMemory = VK_NULL_HANDLE;
	VkImageView mImageView = VK_NULL_HANDLE;
	VkFormat mFormat = VK_FORMAT_UNDEFINED;
	uint32 mWidth = 0;
	uint32 mHeight = 0;
	VkImageUsageFlags mUsageFlags = 0;
	VkImageLayout mCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};
/**	
 *	@brief Vulkan 기반 Android 인코더 백엔드 구현 클래스
 *	@class FVdjmAndroidEncoderBackendVulkan
 */
class FVdjmAndroidEncoderBackendVulkan : public FVdjmAndroidEncoderBackend
{
public:
	FVdjmAndroidEncoderBackendVulkan();
	virtual ~FVdjmAndroidEncoderBackendVulkan() override = default;
	
	virtual bool Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow) override;
	virtual bool Start() override;
	virtual void Stop() override;
	virtual void Terminate() override;
	bool IsRunnable() const;
	virtual bool Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec) override;

private:
	FVdjmAndroidEncoderConfigure mConfig;
	bool mInitialized = false;
	bool mStarted = false;
	bool mPaused = false;
	
	FVdjmVkRecoderHandles mVkHandles;
	
	FVdjmVkCodecInputSurfaceState mCodecInputSurfaceState;
	FVdjmVkIntermediateState mIntermediateState;
};


#endif
