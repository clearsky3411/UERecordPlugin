// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
#include "vulkan_core.h"

struct IVulkanDynamicRHI;
class FVdjmAndroidEncoderBackendVulkan;
/**
 * @brief Vulkan 이미지 제출 과정에서 입력 이미지의 특성을 분석하여 제출 전략을 결정하는 클래스
 * @note life time : FVdjmAndroidEncoderBackendVulkan 인스턴스 생성부터 소멸까지, 즉, Vulkan 백엔드가 활성화된 동안
 * 
 */




/**
 * @brief backend가 소유한 이미지용
 */
struct FVdjmVkOwnedImageState
{
	VkImage Image = VK_NULL_HANDLE;
	VkFormat Format = VK_FORMAT_UNDEFINED;
	uint32 Width = 0;
	uint32 Height = 0;
	VkImageLayout CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	void Clear()
	{
		Image = VK_NULL_HANDLE;
		Format = VK_FORMAT_UNDEFINED;
		Width = 0;
		Height = 0;
		CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	bool IsValid() const
	{
		return Image != VK_NULL_HANDLE
			&& Format != VK_FORMAT_UNDEFINED
			&& Width > 0
			&& Height > 0;
	}
};

struct FVdjmVkSubmitFrameInfo
{
	VkImage SrcImage = VK_NULL_HANDLE;	
	VkFormat SrcFormat = VK_FORMAT_UNDEFINED;
	uint32 SrcWidth = 0;
	uint32 SrcHeight = 0;
	
	bool bNeedsIntermediate = false;
	bool bCanDirectCopy = false;

	
	FVdjmVkSubmitFrameInfo() = default;
	FVdjmVkSubmitFrameInfo(const FVdjmVkSubmitFrameInfo& other) = default;
	FVdjmVkSubmitFrameInfo(FVdjmVkSubmitFrameInfo&& other) = default;
	
	FVdjmVkSubmitFrameInfo& operator=(const FVdjmVkSubmitFrameInfo& other) = default;
	FVdjmVkSubmitFrameInfo& operator=(FVdjmVkSubmitFrameInfo&& other) = default;
	
	void Clear()
	{
		SrcImage = VK_NULL_HANDLE;
		SrcFormat = VK_FORMAT_UNDEFINED;
		SrcWidth = 0;
		SrcHeight = 0;
		
		bNeedsIntermediate = false;
		bCanDirectCopy = false;
	}
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

/**
 * @brief Vulkan 이미지 제출 과정에서 필요한 세션 상태와 리소스를 관리하는 구조체
 * @note life time : FVdjmAndroidRecordSession이 시작되고 종료될 때까지, 즉, 실제 인코딩 세션이 활성화된 동안
 */
struct FVdjmVkRecordSessionState
{
	bool bReady = false;

	VkSurfaceKHR CodecSurface = VK_NULL_HANDLE;
	VkSwapchainKHR CodecSwapchain = VK_NULL_HANDLE;

	VkFormat SwapchainFormat = VK_FORMAT_UNDEFINED;
	uint32 SwapchainWidth = 0;
	uint32 SwapchainHeight = 0;

	

	VkCommandPool CommandPool = VK_NULL_HANDLE;
	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;

	VkFence SubmitFence = VK_NULL_HANDLE;
	VkSemaphore AcquireSemaphore = VK_NULL_HANDLE;
	VkSemaphore RenderCompleteSemaphore = VK_NULL_HANDLE;
	
	TArray<VkImage> SwapchainImages;
	TArray<VkImageView> SwapchainImageViews;
	TArray<FVdjmVkOwnedImageState> SwapchainImageStates;
	TArray<VkSemaphore> RenderCompleteSemaphores;
	
	void Clear()
	{
		bReady = false;
		CodecSurface = VK_NULL_HANDLE;
		CodecSwapchain = VK_NULL_HANDLE;
		SwapchainFormat = VK_FORMAT_UNDEFINED;
		SwapchainWidth = 0;
		SwapchainHeight = 0;
		SwapchainImages.Empty();
		SwapchainImageViews.Empty();
		CommandPool = VK_NULL_HANDLE;
		CommandBuffer = VK_NULL_HANDLE;
		SubmitFence = VK_NULL_HANDLE;
		AcquireSemaphore = VK_NULL_HANDLE;
		RenderCompleteSemaphore = VK_NULL_HANDLE;
		
		SwapchainImageStates.Empty();
		RenderCompleteSemaphores.Empty();
	}
};



/**
 * @brief Vulkan 이미지 제출 과정에서 각 프레임 제출 시점에 필요한 정보와 상태를 관리하는 구조체
 * @note life time : 각 프레임 제출 시점에 생성되고 제출이 완료되면 소멸, 즉, 일시적인 프레임 제출 컨텍스트
 */
struct FVdjmVkFrameSubmitState
{
	uint32 AcquiredImageIndex = UINT32_MAX;
	FVdjmVkOwnedImageState* DstState;
	VkImage FinalSrcImage = VK_NULL_HANDLE; // 최종 제출에 사용될 src image 핸들, intermediate가 필요한 경우 intermediate image로 설정
	FVdjmVkOwnedImageState* FinalSrcOwnedState = nullptr;

	void Clear()
	{
		AcquiredImageIndex = UINT32_MAX;
		DstState = nullptr;
		FinalSrcImage = VK_NULL_HANDLE;
		FinalSrcOwnedState = nullptr;
	}

	bool IsValid() const
	{
		return DstState != nullptr
			&& DstState->IsValid()
			&& FinalSrcImage != VK_NULL_HANDLE;
	}
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
	/**
		SrcImage
		SrcFormat
		SrcWidth
		SrcHeight
		bFormatMatchesSwapchain
		bExtentMatchesSwapchain
		bCanDirectCopy
		bNeedsIntermediate
	*/
public:
	
	explicit FVdjmVkSubProcInputAnalyzer(FVdjmAndroidEncoderBackendVulkan* const owner)
		: FVdjmVkSubProcessContext(owner)
	{
	}

	bool Analyze(const FTextureRHIRef& srcTexture, FVdjmVkSubmitFrameInfo& outInfo) ;

};

/**	
 *	@brief Vulkan 이미지 제출 과정에서 필요한 중간 단계 처리 담당 클래스
 *	@class FVdjmVkSubProcIntermediateStage
 */
class FVdjmVkSubProcIntermediateStage: public FVdjmVkSubProcessContext
{
	friend class FVdjmAndroidEncoderBackendVulkan;
	/**
 	* @brief 불칸 내부 이미지 제출 과정에서 필요한 중간 단계 처리 클래스
 	* ### 목적:
 	* - intermediate image 필요 판단
 	* - intermediate image / view / memory 생성/재생성
 	* - 입력 이미지 → intermediate 로 copy / blit / render
 	* - layout transition 규칙 적용
 	* - 포멧이 다를때나, 해상도가 다를때나, 레이아웃, 색공간 변환이나 scale이 필요한 경우에 intermediate image를 만들어서 제출하는 과정 담당
 	* ### Result:
 	* - 가공된 Vulkan 이미지 생성
 	* - 제출 가능한 상태로 Vulkan 이미지 준비
 	* ### 한계:
 	* - 입력 텍스처 해석
 	* - 최종 제출 완료 관리
 	*/
public:
	explicit FVdjmVkSubProcIntermediateStage(FVdjmAndroidEncoderBackendVulkan* const owner)
		: FVdjmVkSubProcessContext(owner)
	{
	}

	bool NeedRecreate(const FVdjmVkSubmitFrameInfo& frameInfo,uint32 curWid,uint32 curhei,VkFormat  curFormat) const;
	void Release(FVdjmAndroidEncoderBackendVulkan& owner);
	bool EnsureResource(FVdjmAndroidEncoderBackendVulkan& backend, const FVdjmVkSubmitFrameInfo& frameInfo);
	bool RecordPrepareAndCopy(FVdjmAndroidEncoderBackendVulkan& owner, const FVdjmVkSubmitFrameInfo& frameInfo, FVdjmVkFrameSubmitState& inOutFrameState);
	
	FVdjmVkOwnedImageState& GetIntermediateState() { return mIntermediateState; }
	const FVdjmVkOwnedImageState& GetIntermediateStateConst() const { return mIntermediateState; }
	
	bool IsValidIntermediateImage() const { return mIntermediateState.Image != VK_NULL_HANDLE; }
	bool IsValidIntermediateSwapchainResolutions(FVdjmAndroidEncoderBackendVulkan* backend = nullptr) const;
	bool IsValidIntermediateFormat(FVdjmAndroidEncoderBackendVulkan*  backend = nullptr) const;

private:
	FVdjmVkOwnedImageState mIntermediateState;
};

/**	
 *	@brief Vulkan 이미지 제출 과정에서 필요한 최종 제출 담당 클래스
 *	@class FVdjmVkSubProcSurfaceSubmitter
 */
class FVdjmVkSubProcSurfaceSubmitter : public FVdjmVkSubProcessContext
{
	friend class FVdjmAndroidEncoderBackendVulkan;
	/**
	 * @brief Vulkan 이미지 제출 담당 클래스
	 * ### 목적:
	 * - 제출 command buffer / queue submit
	 * - semaphore / fence 관리
	 * - 제출 완료 대기 또는 상태 확인
	 * - stop / terminate 시 남은 작업 정리
	 * ### Result:
	 * - 최종 제출 + 완료 추적기
	 * ### 한계:
	 * - 입력 텍스처 분석
	 * - intermediate 생성 정책 판단
	 */
public:
	explicit FVdjmVkSubProcSurfaceSubmitter(FVdjmAndroidEncoderBackendVulkan* const owner)
		: FVdjmVkSubProcessContext(owner)
	{
	}

	bool Submit(FVdjmAndroidEncoderBackendVulkan& owner, const FVdjmVkFrameSubmitState& frameState, double timeStampSec);
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

	VkCommandPool GetCommandPool() const { return mVkRecordSession.CommandPool; }
	VkCommandBuffer GetCommandBuffer() const { return mVkRecordSession.CommandBuffer; }
	const VkCommandBuffer* GetCommandBufferConst() const { return &mVkRecordSession.CommandBuffer; }
	VkFence GetSubmitFence() const { return mVkRecordSession.SubmitFence; }
	const VkFence* GetSubmitFenceConst() const { return &mVkRecordSession.SubmitFence; }
	
	VkSurfaceKHR GetCodecSurface() const { return mVkRecordSession.CodecSurface; }
	VkSwapchainKHR GetCodecSwapchain() const { return mVkRecordSession.CodecSwapchain; }
	const VkSwapchainKHR* GetCodecSwapchainConst() const { return &mVkRecordSession.CodecSwapchain; }

	const TArray<VkImage>& GetSwapchainImages() const { return mVkRecordSession.SwapchainImages; }
	const TArray<VkImageView>& GetSwapchainImageViews() const { return mVkRecordSession.SwapchainImageViews; }

	VkSemaphore GetAcquireSemaphore() const { return mVkRecordSession.AcquireSemaphore; }
	const VkSemaphore* GetAcquireSemaphoreConst() const { return &mVkRecordSession.AcquireSemaphore; }
	VkSemaphore GetRenderCompleteSemaphore() const { return mVkRecordSession.RenderCompleteSemaphore; }
	const VkSemaphore* GetRenderCompleteSemaphoreConst() const { return &mVkRecordSession.RenderCompleteSemaphore; }

	uint32 GetCurrentSwapchainImageIndex() const { return mCurrentSwapchainImageIndex32; }
	const uint32_t* GetCurrentSwapchainImageIndexConst() const { return &mCurrentSwapchainImageIndex32; }
	void SetCurrentSwapchainImageIndex(uint32 InIndex) { mCurrentSwapchainImageIndex32 = InIndex; }

	VkFormat GetSwapchainFormat() const { return mVkRecordSession.SwapchainFormat; }
	uint32 GetSwapchainWidth() const { return mVkRecordSession.SwapchainWidth; }
	uint32 GetSwapchainHeight() const { return mVkRecordSession.SwapchainHeight; }

	VkImage GetIntermediateImage() const { return mIntermediateStage.GetIntermediateStateConst().Image; }

	static uint32 FindMemoryType(VkPhysicalDevice physicalDevice, uint32 typeFilter, VkMemoryPropertyFlags properties);
	static void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	static bool TransitionOwnedImage(VkCommandBuffer Cmd,
	FVdjmVkOwnedImageState& State,
	VkImageLayout NewLayout);
	static VkSurfaceFormatKHR ChooseSurfaceFormat(const FVdjmVkRuntimeContext& runtimeContext,const TArray<VkSurfaceFormatKHR>& availableFormats);
	static VkPresentModeKHR ChoosePresentMode(const TArray<VkPresentModeKHR>& modes);
	static VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(VkCompositeAlphaFlagsKHR flags);
	static VkExtent2D ChooseExtent(	const VkSurfaceCapabilitiesKHR& caps,uint32 desiredWid,	uint32 desiredHei);
	
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
	FVdjmVkSubProcIntermediateStage mIntermediateStage;
	FVdjmVkSubProcSurfaceSubmitter mSurfaceSubmitter;
	
};


#endif
