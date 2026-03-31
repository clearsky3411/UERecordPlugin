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
		return mVulkanRHI != nullptr && mVkInstance != VK_NULL_HANDLE && mVkPhysicalDevice != VK_NULL_HANDLE && mVkDevice != VK_NULL_HANDLE && mGraphicsQueue != VK_NULL_HANDLE && mGraphicsQueueFamilyIndex != UINT32_MAX;
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
class FVdjmVkRuntimeRefs
{
	
};
class FVdjmVkCodecInputSurfaceState
{
	
};
class FVdjmVkIntermediateState
{
	
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
};


#endif
