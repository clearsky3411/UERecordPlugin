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
