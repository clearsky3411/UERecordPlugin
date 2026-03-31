// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"

#if PLATFORM_ANDROID || defined(__RESHARPER__)

#include "vulkan_android.h"
#include "IVulkanDynamicRHI.h"

namespace
{
	static constexpr VkImageAspectFlags GColorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
}

FVdjmAndroidRecordVulkanSession::~FVdjmAndroidRecordVulkanSession()
{
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
	
	if (not mVkHandles.EnsureInitialized())
	{
		mVkHandles.Clear();

		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmAndroidEncoderBackendVulkan::Start - Failed to ensure Vulkan handles are initialized. Cannot start encoder backend."));
		return false;
	}
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmAndroidEncoderBackendVulkan::Start - Successfully ensured Vulkan handles are initialized. Starting encoder backend."));
	
	return mStarted = true;
}
void FVdjmAndroidEncoderBackendVulkan::Stop()
{
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
	mInitialized = false;
}

bool FVdjmAndroidEncoderBackendVulkan::IsRunnable() const
{
	return mInitialized && mStarted && !mPaused && mInputWindow != nullptr;
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,double timeStampSec)
{
	UE_LOG(
		LogTemp,
		Error,
		TEXT("FVdjmAndroidEncoderBackendVulkan::Running - disabled path reached unexpectedly."));
	return false;
}

#endif
