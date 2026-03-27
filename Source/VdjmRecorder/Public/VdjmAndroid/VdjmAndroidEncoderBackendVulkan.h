// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
#include "vulkan_core.h"

struct IVulkanDynamicRHI;
class FVdjmAndroidEncoderBackendVulkan;


enum class EVdjmVkFailureReason : uint8
{
	None,
	SourceHandleResolveFailed,
	SourceLayoutUnknown,
	SourceFormatMismatch,
	SourceExtentMismatch,
	AcquireFailed,
	SubmitFailed,
	PresentFailed,
	SwapchainOutOfDate,
	DeviceLost
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
//	transient
struct FVdjmVkSubmitFrameInfo
{
	VkImage SrcImage = VK_NULL_HANDLE;
	VkFormat SrcFormat = VK_FORMAT_UNDEFINED;
	uint32 SrcWidth = 0;
	uint32 SrcHeight = 0;

	bool bNeedsIntermediate = false;
	bool bCanDirectCopy = false;

	void Clear()
	{
		SrcImage = VK_NULL_HANDLE;
		SrcFormat = VK_FORMAT_UNDEFINED;
		SrcWidth = 0;
		SrcHeight = 0;
		bNeedsIntermediate = false;
		bCanDirectCopy = false;
	}

	bool IsValid() const
	{
		return SrcImage != VK_NULL_HANDLE
			&& SrcFormat != VK_FORMAT_UNDEFINED
			&& SrcWidth > 0
			&& SrcHeight > 0;
	}
};
struct FVdjmVkFrameContext
{
	/*
	 * 프레임 슬롯 하나의 lifetime
	 * - Start에서 생성
	 * - Running에서 acquire / record / submit / present에 사용
	 * - fence signaled 이후 재사용
	 * - Stop에서 drain 후 해제
	 */
	VkCommandPool CommandPool = VK_NULL_HANDLE;
	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;

	VkSemaphore AcquireCompleteSemaphore = VK_NULL_HANDLE;
	VkSemaphore RenderCompleteSemaphore = VK_NULL_HANDLE;
	VkFence SubmitFence = VK_NULL_HANDLE;

	bool bInFlight = false;
	uint64 SubmissionSerial = 0;

	void Clear()
	{
		CommandPool = VK_NULL_HANDLE;
		CommandBuffer = VK_NULL_HANDLE;
		AcquireCompleteSemaphore = VK_NULL_HANDLE;
		RenderCompleteSemaphore = VK_NULL_HANDLE;
		SubmitFence = VK_NULL_HANDLE;
		bInFlight = false;
		SubmissionSerial = 0;
	}

	bool IsReady() const
	{
		return CommandPool != VK_NULL_HANDLE
			&& CommandBuffer != VK_NULL_HANDLE
			&& AcquireCompleteSemaphore != VK_NULL_HANDLE
			&& RenderCompleteSemaphore != VK_NULL_HANDLE
			&& SubmitFence != VK_NULL_HANDLE;
	}
	static bool IsReadies(const TArray<FVdjmVkFrameContext>& FrameContexts)
	{
		for (const FVdjmVkFrameContext& Context : FrameContexts)
		{
			if (!Context.IsReady())
			{
				return false;
			}
		}
		return true;
	}
	static bool ClearAll(TArray<FVdjmVkFrameContext>& FrameContexts)
	{
		for (FVdjmVkFrameContext& Context : FrameContexts)
		{
			Context.Clear();
		}
		return true;
	}

};

struct FVdjmVkObservedSourceState
{
	/*
	 * source image의 마지막 known 상태를 session 동안 추적한다.
	 * layout을 고정 상수로 두지 않고, 프레임별 실제 상태를 누적 관리한다.
	 */
	VkFormat Format = VK_FORMAT_UNDEFINED;
	VkExtent2D Extent{0, 0};
	VkImageLayout LastKnownLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	bool bLayoutKnown = false;
	uint64 LastSeenSerial = 0;

	void Clear()
	{
		Format = VK_FORMAT_UNDEFINED;
		Extent = {0, 0};
		LastKnownLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		bLayoutKnown = false;
		LastSeenSerial = 0;
	}
};
struct FVdjmVkRecordSessionState
{
	ANativeWindow* InputWindow = nullptr;

	VkSurfaceKHR CodecSurface = VK_NULL_HANDLE;
	VkSwapchainKHR CodecSwapchain = VK_NULL_HANDLE;

	TArray<VkImage> SwapchainImages;
	TArray<VkImageLayout> SwapchainImageLayouts;

	VkFormat SurfaceFormat = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR SurfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D SurfaceExtent{0, 0};

	TArray<FVdjmVkFrameContext> FrameContexts;
	uint32 NextFrameContextIndex = 0;
	bool bStopRequested = false;
	uint64 SubmissionSerial = 0;
	
	TMap<uint64, FVdjmVkObservedSourceState> SourceStateCache;

	FVdjmVkOwnedImageState IntermediateImageState;
	bool bHasIntermediateImage = false;

	bool bStarted = false;
	
	uint64 CreatedFrameContextCount = 0;
	uint64 DestroyedFrameContextCount = 0;
	uint64 SubmittedFrameCount = 0;
	uint64 PresentedFrameCount = 0;

	bool IsReadyToStart() const
	{
		return CodecSurface != VK_NULL_HANDLE && CodecSwapchain != VK_NULL_HANDLE && FVdjmVkFrameContext::IsReadies(FrameContexts);
	}
	
	void Clear()
	{
		InputWindow = nullptr;
		CodecSurface = VK_NULL_HANDLE;
		CodecSwapchain = VK_NULL_HANDLE;
		SwapchainImages.Empty();
		SwapchainImageLayouts.Empty();
		SurfaceFormat = VK_FORMAT_UNDEFINED;
		SurfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		SurfaceExtent = {0, 0};
		FVdjmVkFrameContext::ClearAll(FrameContexts);
		FrameContexts.Empty();
		
		IntermediateImageState.Clear();
		bHasIntermediateImage = false;
		bStarted = false;
	}
	/*
	 * Stop 이후 생성된 출력 파일이 실제로 존재하고,
	 * 크기가 0이 아니며, 최소한의 기록이 있었는지 확인한다.
	 * 서비스 수준에서는 녹화 성공/실패 판정을 여기서 내린다.
	 */
	bool ValidateRecordedOutputFile() const;
};


/**
 * @brief Vulkan 관련 리소스와 상태를 관리하는 런타임 컨텍스트 구조체
 * @note life time : FVdjmAndroidEncoderBackendVulkan 인스턴스 생성부터 소멸까지
 * @details shared,flyweight
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
	bool IsRunnable() const;
	//	여기에서 orchestration 담당, Running이 호출될 때마다 제출 시도, 제출 과정에서 필요한 분석과 중간 단계 처리는 별도의 클래스에서 담당
	virtual bool Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec) override;
	
	VkDevice GetVkDevice() const { return mVkRuntime.VkDevice; }
	VkPhysicalDevice GetVkPhysicalDevice() const { return mVkRuntime.VkPhysicalDevice; }
	VkQueue GetGraphicsQueue() const { return mVkRuntime.GraphicsQueue; }
	uint32 GetGraphicsQueueFamilyIndex() const { return mVkRuntime.GraphicsQueueFamilyIndex; }

	
	VkSwapchainKHR GetCodecSwapchain() const { return mVkRecordSession.CodecSwapchain; }
	const VkSwapchainKHR* GetCodecSwapchainConst() const { return &mVkRecordSession.CodecSwapchain; }

	uint32 GetCurrentSwapchainImageIndex() const { return mCurrentSwapchainImageIndex32; }
	const uint32_t* GetCurrentSwapchainImageIndexConst() const { return &mCurrentSwapchainImageIndex32; }
	void SetCurrentSwapchainImageIndex(uint32 InIndex) { mCurrentSwapchainImageIndex32 = InIndex; }
	
	bool CreateRecordSessionVkResources();
	void DestroyRecordSessionVkResources();
	
	bool TryExtractNativeVkImage(const FTextureRHIRef& srcTexture, VkImage& outImage) const;
	
	bool ResolveNativeVkImage(const FTextureRHIRef& srcTexture, FVdjmVkOwnedImageState& outImageState) const;
	/*
 * source와 encoder surface가 exact-match가 아닐 때 사용할 중간 이미지 경로를 준비한다.
 * 서비스 수준에서는 resize / format normalize / blit / shader copy를 이 경로로 보낸다.
 */
	bool EnsureIntermediateForFrame(const FVdjmVkSubmitFrameInfo& SubmitInfo);

	/*
	 * 현재 프레임을 intermediate image를 거쳐 encoder surface에 넣는다.
	 */
	bool SubmitTextureViaIntermediate(
		const FVdjmVkSubmitFrameInfo& SubmitInfo,
		FVdjmVkFrameContext& FrameCtx,
		const FVdjmVkObservedSourceState& SourceState);
	
private:
	
	bool InitVkRuntimeContext();
	
	bool AcquireNextSwapchainImage(FVdjmVkFrameSubmitState& outFrameState);
	bool SubmitTextureToCodecSurface(const FVdjmVkSubmitFrameInfo& submitInfo, FVdjmVkFrameSubmitState& frameState);
	
	bool DrainInFlightFrames();
	/*
 * 현재 source image의 실제 사용 가능한 상태를 해석한다.
 * - cache hit면 LastKnownLayout 사용
 * - cache miss면 unresolved로 반환
 * 서비스 수준에서는 GENERAL 고정 가정을 없애는 함수다.
 */
	bool TryResolveSourceState(
		const FVdjmVkSubmitFrameInfo& SubmitInfo,
		FVdjmVkObservedSourceState& OutState);

	void CommitSourceStateAfterSubmit(
		const FVdjmVkSubmitFrameInfo& SubmitInfo,
		VkImageLayout NewLayout);
	void SetFailureReason(EVdjmVkFailureReason Reason);
	
	EVdjmVkFailureReason mLastFailureReason = EVdjmVkFailureReason::None;
	/*
	 * Vulkan 경로 실패를 분류해 상위 session/manager가 정책 판단을 할 수 있게 한다.
	 * backend는 "무엇이 실패했는지"만 말하고, fallback 자체는 상위에서 결정한다.
	 */
	
	FVdjmAndroidEncoderConfigure mConfig;
	
	bool mInitialized = false;
	bool mStarted = false;
	bool mPaused = false;
	
	
	uint32_t mCurrentSwapchainImageIndex32;
	
	FVdjmVkRuntimeContext mVkRuntime;
	FVdjmVkRecordSessionState mVkRecordSession;
	
	FVdjmVkSubProcInputAnalyzer mAnalyzer;

	
};


#endif
