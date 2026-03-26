// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
#include "vulkan_core.h"

struct IVulkanDynamicRHI;
class FVdjmAndroidEncoderBackendVulkan;


struct FRecorderSessionContract
{
	/*
	 * EnsureRuntimeReady 에서 확정이 되어야한다.
	 */
	uint32 EncodeWidth = 0;
	uint32 EncodeHeight = 0;

	EPixelFormat RequiredSrcPixelFormat = PF_Unknown;
	VkFormat RequiredSrcVkFormat = VK_FORMAT_UNDEFINED;

	VkFormat EncoderSurfaceFormat = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR EncoderSurfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D EncoderExtent{0, 0};

	int32 WindowBufferFormat = 0;                 // ANativeWindow_getFormat()
	int32 WindowBufferDataSpace = ADATASPACE_UNKNOWN; // ANativeWindow_getBuffersDataSpace()

	uint32 GraphicsQueueFamilyIndex = UINT32_MAX;
	VkImageUsageFlags EncoderImageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	bool bRequireExactSourceExtent = true;
	bool bRequireExactSourceFormat = true;
	bool bAllowBlit = false;
	bool bAllowIntermediate = false;
	bool bValid = false;
};
struct FFrameSourceSnapshot
{
	FTextureRHIRef SrcTexture;
	VkImage SrcImage = VK_NULL_HANDLE;

	EPixelFormat SrcPixelFormat = PF_Unknown;
	VkFormat SrcVkFormat = VK_FORMAT_UNDEFINED;

	uint32 SrcWidth = 0;
	uint32 SrcHeight = 0;
};
struct FEncoderSurfaceRuntimeState
{
	ANativeWindow* InputWindow = nullptr;

	VkSurfaceKHR CodecSurface = VK_NULL_HANDLE;
	VkSwapchainKHR CodecSwapchain = VK_NULL_HANDLE;

	TArray<VkImage> SwapchainImages;
	TArray<VkImageLayout> SwapchainImageLayouts;

	VkCommandPool CommandPool = VK_NULL_HANDLE;
	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;

	VkFence SubmitFence = VK_NULL_HANDLE;
	VkSemaphore AcquireSemaphore = VK_NULL_HANDLE;
	TArray<VkSemaphore> RenderCompleteSemaphores;

	bool bReady = false;
};

struct FVdjmVkSubmitFrameInfo
{
	VkImage SrcImage = VK_NULL_HANDLE;
	VkFormat SrcFormat = VK_FORMAT_UNDEFINED;
	uint32 SrcWidth = 0;
	uint32 SrcHeight = 0;
	bool bNeedsIntermediate = false;
	bool bCanDirectCopy = false;
	// SrcLayout 넣지 말 것
};

struct FVdjmVkOwnedImageState
{
	VkImage Image = VK_NULL_HANDLE;

	// transfer-only 경로면 없어도 된다.
	// shader bind / framebuffer attach가 필요할 때만 사용.
	VkImageView View = VK_NULL_HANDLE;

	VkFormat Format = VK_FORMAT_UNDEFINED;
	uint32 Width = 0;
	uint32 Height = 0;
	VkImageLayout CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	void Clear()
	{
		Image = VK_NULL_HANDLE;
		View = VK_NULL_HANDLE;
		Format = VK_FORMAT_UNDEFINED;
		Width = 0;
		Height = 0;
		CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}
	bool IsValid() const
	{
		return Image != VK_NULL_HANDLE && Format != VK_FORMAT_UNDEFINED && Width > 0 && Height > 0 && CurrentLayout != VK_IMAGE_LAYOUT_UNDEFINED;
	}
};


struct FVdjmVkFrameSubmitState
{
	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
	VkSemaphore AcquireCompleteSemaphore = VK_NULL_HANDLE;
	VkFence SubmitFence = VK_NULL_HANDLE;
	uint32 AcquiredImageIndex = UINT32_MAX;

	// Submit 성공 후에만 owner state에 commit하기 위한 pending state
	bool bCommitDstLayoutOnSubmitSuccess = false;
	VkImageLayout PendingDstLayoutAfterSubmit = VK_IMAGE_LAYOUT_UNDEFINED;

	bool bCommitIntermediateLayoutOnSubmitSuccess = false;
	VkImageLayout PendingIntermediateLayoutAfterSubmit = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct FVdjmVkRecordSessionState
{
	VkSwapchainKHR CodecSwapchain = VK_NULL_HANDLE;

	FVdjmVkOwnedImageState IntermediateImageState;
	bool bHasIntermediateImage = false;

	bool bRuntimeReady = false;

	// 네 기존 멤버들 유지
	// Surface, Window, Format, Extent, etc...
};


/**
 * @brief Vulkan 관련 리소스와 상태를 관리하는 런타임 컨텍스트 구조체
 * @note life time : FVdjmAndroidEncoderBackendVulkan 인스턴스 생성부터 소멸까지
 */
struct FVdjmVkRuntimeContext
{
	bool bInitialized = false;
	
	IVulkanDynamicRHI* VulkanRHI = nullptr;
	
	VkInstance VkInstance = VK_NULL_HANDLE;
	VkDevice VkDevice = VK_NULL_HANDLE;
	VkPhysicalDevice VkPhysicalDevice = VK_NULL_HANDLE;
	VkQueue GraphicsQueue = VK_NULL_HANDLE;
	uint32 GraphicsQueueFamilyIndex = 0;
	
	void Clear()
	{
		bInitialized = false;
		VulkanRHI = nullptr;
		VkInstance = VK_NULL_HANDLE;
		VkDevice = VK_NULL_HANDLE;
		VkPhysicalDevice = VK_NULL_HANDLE;
		GraphicsQueue = VK_NULL_HANDLE;
		GraphicsQueueFamilyIndex = 0;
	}
	bool IsInitValid() const
	{
		return VulkanRHI != nullptr && VkInstance != VK_NULL_HANDLE && VkDevice != VK_NULL_HANDLE && VkPhysicalDevice != VK_NULL_HANDLE && GraphicsQueue != VK_NULL_HANDLE && GraphicsQueueFamilyIndex != UINT32_MAX;
	}
	bool IsValid() const
	{
		return bInitialized && IsInitValid();
	}
};

struct FVdjmVkHelper
{
	static uint32 FindMemoryType(VkPhysicalDevice physicalDevice, uint32 typeFilter, VkMemoryPropertyFlags properties);
	static void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	static bool TransitionOwnedImage(VkCommandBuffer Cmd,
	FVdjmVkOwnedImageState& State,
	VkImageLayout NewLayout);
	static VkSurfaceFormatKHR ChooseSurfaceFormat(const FVdjmVkRuntimeContext& runtimeContext,const TArray<VkSurfaceFormatKHR>& availableFormats);
	static VkPresentModeKHR ChoosePresentMode(const TArray<VkPresentModeKHR>& modes);
	static VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(VkCompositeAlphaFlagsKHR flags);
	static VkExtent2D ChooseExtent(	const VkSurfaceCapabilitiesKHR& caps,uint32 desiredWid,	uint32 desiredHei);
};


/**	
 *	@brief Vulkan 이미지 제출 과정에서 필요한 분석과 중간 단계 처리 담당 클래스들의 공통 기반 클래스
 *	@class FVdjmVkSubProcessContext
 */
class FVdjmVkSubProcessContext
{
	friend class FVdjmAndroidEncoderBackendVulkan;
public:
	FVdjmVkSubProcessContext(FVdjmAndroidEncoderBackendVulkan* owner) : mOwnerBackend(owner) {}
	virtual ~FVdjmVkSubProcessContext() = default;
protected:
	class FVdjmAndroidEncoderBackendVulkan* mOwnerBackend = nullptr;
};

class FVdjmVkSubProcInputAnalyzer : public FVdjmVkSubProcessContext
{
	friend class FVdjmAndroidEncoderBackendVulkan;
	
public:
	
	explicit FVdjmVkSubProcInputAnalyzer(FVdjmAndroidEncoderBackendVulkan* const owner)
		: FVdjmVkSubProcessContext(owner)
	{}
	
	// Init 분석기
	bool BuildSessionContract(
		ANativeWindow* InputWindow,
		FRecorderSessionContract& OutContract);

	// Running 스냅샷
	bool SnapshotFrameSource(
		const FTextureRHIRef& SrcTexture,
		FFrameSourceSnapshot& OutFrame);

	// Running 검증기
	bool ValidateFrameAgainstContract(
		const FRecorderSessionContract& Contract,
		const FFrameSourceSnapshot& Frame);
	
	bool Analyze(const FTextureRHIRef& srcTexture, FVdjmVkSubmitFrameInfo& outInfo) ;
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
	//	여기에서 orchestration 담당, Running이 호출될 때마다 제출 시도, 제출 과정에서 필요한 분석과 중간 단계 처리는 별도의 클래스에서 담당
	virtual bool Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec) override;
	
	bool IsRunnable();
	VkDevice GetVkDevice() const { return mVkRuntime.VkDevice; }
	VkPhysicalDevice GetVkPhysicalDevice() const { return mVkRuntime.VkPhysicalDevice; }
	VkQueue GetGraphicsQueue() const { return mVkRuntime.GraphicsQueue; }
	uint32 GetGraphicsQueueFamilyIndex() const { return mVkRuntime.GraphicsQueueFamilyIndex; }

	
	VkSwapchainKHR GetCodecSwapchain() const { return mVkRecordSession.CodecSwapchain; }
	const VkSwapchainKHR* GetCodecSwapchainConst() const { return &mVkRecordSession.CodecSwapchain; }



	

	uint32 GetCurrentSwapchainImageIndex() const { return mCurrentSwapchainImageIndex32; }
	const uint32_t* GetCurrentSwapchainImageIndexConst() const { return &mCurrentSwapchainImageIndex32; }
	void SetCurrentSwapchainImageIndex(uint32 InIndex) { mCurrentSwapchainImageIndex32 = InIndex; }
	

	
	
	bool TryExtractNativeVkImage(const FTextureRHIRef& srcTexture, VkImage& outImage) const;
private:
	
	bool InitVkRuntimeContext();
	void ReleaseRecordSessionVkResources();
	bool EnsureRuntimeReady();
	bool AcquireNextSwapchainImage(FVdjmVkFrameSubmitState& outFrameState);
	bool SubmitTextureToCodecSurface(const FVdjmVkFrameSubmitState& frameState);
	
	FVdjmAndroidEncoderConfigure mConfig;
	
	bool mInitialized = false;
	bool mStarted = false;
	bool mPaused = false;
	bool mRuntimeReady = false;
	
	uint32_t mCurrentSwapchainImageIndex32;
	
	FVdjmVkRuntimeContext mVkRuntime;
	FVdjmVkRecordSessionState mVkRecordSession;
	
	FVdjmVkSubProcInputAnalyzer mAnalyzer;

	
};


#endif
