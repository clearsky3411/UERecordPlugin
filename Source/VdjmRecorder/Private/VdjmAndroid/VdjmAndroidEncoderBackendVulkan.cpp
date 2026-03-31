// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"

#if PLATFORM_ANDROID || defined(__RESHARPER__)

#include "vulkan_android.h"
#include "IVulkanDynamicRHI.h"

namespace
{
	static constexpr VkImageAspectFlags GColorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
}

FVdjmAndroidEncoderBackendVulkan::FVdjmAndroidEncoderBackendVulkan()
{}

bool FVdjmAndroidEncoderBackendVulkan::Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow)
{
	if (not config.IsValidateEncoderArguments() || inputWindow == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Init: invalid config or input window"));
		return false;
	}
	
	if (mInputWindow != nullptr && mInputWindow != inputWindow)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("Init: replacing existing input window"));
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
	}

	mInputWindow = inputWindow;
	ANativeWindow_acquire(mInputWindow);

	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::Start()
{

	return true;
}
void FVdjmAndroidEncoderBackendVulkan::Stop()
{

}
void FVdjmAndroidEncoderBackendVulkan::Terminate()
{
	Stop();

	if (mInputWindow != nullptr)
	{
		ANativeWindow_release(mInputWindow);
		mInputWindow = nullptr;
	}

}

bool FVdjmAndroidEncoderBackendVulkan::IsRunnable() const
{
	
	return true;
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	
}

#endif
