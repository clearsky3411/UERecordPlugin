// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
#include "vulkan_core.h"

struct VkEncoderContext
{
	VkInstance Instance = VK_NULL_HANDLE;
	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	VkDevice Device = VK_NULL_HANDLE;
	VkQueue GraphicsQueue = VK_NULL_HANDLE;
	uint32_t GraphicsQueueFamilyIndex = 0;
};

struct VkSubmitFrameInfo
{
	VkImage SrcImage = VK_NULL_HANDLE;
	VkFormat SrcFormat = VK_FORMAT_R8G8B8A8_UNORM;
	uint32_t SrcWidth = 0;
	uint32_t SrcHeight = 0;
	VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
};
class FVdjmAndroidEncoderBackendVulkan : public FVdjmAndroidEncoderBackend
{
public:
	virtual bool Init(const FVdjmAndroidEncoderConfigure& config) override;
	virtual bool Start() override;
	virtual void Stop() override;
	virtual void Terminate() override;
};
#endif
