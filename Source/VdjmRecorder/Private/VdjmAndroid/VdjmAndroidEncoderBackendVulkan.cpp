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

void FVdjmVkCodecInputSurfaceState::ReleaseSurfaceState(const FVdjmVkRecoderHandles& vkHandles)
{
	if (not vkHandles.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmVkCodecInputSurfaceState::ReleaseSurfaceState - Invalid Vulkan handles provided. Skipping resource release and clearing surface state."));
		Clear();
		return;
	}

	const VkDevice VkDeviceHandle = vkHandles.GetVkDevice();
	const VkInstance VkInstanceHandle = vkHandles.GetVkInstance();

	ReleasePerFrameResources(VkDeviceHandle);
	ReleaseSwapchain(VkDeviceHandle);
	ReleaseSurface(VkInstanceHandle);
	Clear();
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmVkCodecInputSurfaceState::ReleaseSurfaceState - Successfully released Vulkan surface state resources."));
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

void FVdjmVkIntermediateState::Release(const FVdjmVkRecoderHandles& vkHandles)
{
	if (not vkHandles.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmVkIntermediateState::Release - Invalid Vulkan handles provided. Skipping resource release and clearing intermediate state."));
		Clear();
		return;
	}

	const VkDevice VkDeviceHandle = vkHandles.GetVkDevice();

	if (mImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(VkDeviceHandle, mImageView, nullptr);
		mImageView = VK_NULL_HANDLE;
	}

	if (mImage != VK_NULL_HANDLE)
	{
		vkDestroyImage(VkDeviceHandle, mImage, nullptr);
		mImage = VK_NULL_HANDLE;
	}

	if (mMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(VkDeviceHandle, mMemory, nullptr);
		mMemory = VK_NULL_HANDLE;
	}

	Clear();
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmVkIntermediateState::Release - Successfully released Vulkan intermediate state resources."));
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
	if (mVkHandles.IsValid())
	{
		mIntermediateState.Release(mVkHandles);
		mCodecInputSurfaceState.ReleaseSurfaceState(mVkHandles);
	}
	else
	{
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
	UE_LOG(
		LogTemp,
		Error,
		TEXT("FVdjmAndroidEncoderBackendVulkan::Running - disabled path reached unexpectedly."));
	return false;
}

#endif
