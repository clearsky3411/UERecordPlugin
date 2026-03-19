// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
#include "vulkan_core.h"

struct IVulkanDynamicRHI;
class FVdjmAndroidEncoderBackendVulkan;

struct FVdjmVkEncoderContext
{
	VkInstance Instance = VK_NULL_HANDLE;
	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	VkDevice Device = VK_NULL_HANDLE;
	VkQueue GraphicsQueue = VK_NULL_HANDLE;
	uint32_t GraphicsQueueFamilyIndex = 0;
};

struct FVdjmVkSubmitFrameInfo
{
	VkImage SrcImage = VK_NULL_HANDLE;
	VkFormat SrcFormat = VK_FORMAT_UNDEFINED;
	uint32 SrcWidth = 0;
	uint32 SrcHeight = 0;
	VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	bool bFormatMatchesSwapchain = false;
	bool bExtentMatchesSwapchain = false;
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
		SrcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		
		bFormatMatchesSwapchain = false;
		bExtentMatchesSwapchain = false;
		bNeedsIntermediate = false;
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

	TArray<VkImage> SwapchainImages;
	TArray<VkImageView> SwapchainImageViews;

	VkCommandPool CommandPool = VK_NULL_HANDLE;
	VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;

	VkFence SubmitFence = VK_NULL_HANDLE;
	VkSemaphore AcquireSemaphore = VK_NULL_HANDLE;
	VkSemaphore RenderCompleteSemaphore = VK_NULL_HANDLE;
};

/**
 * @brief Vulkan 이미지 제출 과정에서 필요한 중간 이미지 상태와 리소스를 관리하는 구조체
 * @note life time : 분석을 한후에 필요하면 생성하고 FVdjmVkRecordSessionState 와 같은 주기
 */
struct FVdjmVkIntermediateImageState
{
	VkImage IntermediateImage = VK_NULL_HANDLE;
	VkDeviceMemory IntermediateMemory = VK_NULL_HANDLE;
	VkImageView IntermediateView = VK_NULL_HANDLE;
	
	VkFormat IntermediateFormat = VK_FORMAT_UNDEFINED;
	uint32 IntermediateWidth = 0;
	uint32 IntermediateHeight = 0;
};

/**
 * @brief Vulkan 이미지 제출 과정에서 각 프레임 제출 시점에 필요한 정보와 상태를 관리하는 구조체
 * @note life time : 각 프레임 제출 시점에 생성되고 제출이 완료되면 소멸, 즉, 일시적인 프레임 제출 컨텍스트
 */
struct FVdjmVkFrameSubmitState
{
	VkImage SrcImage = VK_NULL_HANDLE;
	VkFormat SrcFormat = VK_FORMAT_UNDEFINED;
	uint32 SrcWidth = 0;
	uint32 SrcHeight = 0;
	VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	bool bCanDirectCopy = false;
	bool bNeedsIntermediate = false;

	uint32 AcquiredImageIndex = UINT32_MAX;
	VkImage DstSwapchainImage = VK_NULL_HANDLE;
	VkImage FinalSrcImage = VK_NULL_HANDLE;
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

class FVdjmVkInputAnalyzer : public FVdjmVkSubProcessContext
{
	friend class FVdjmAndroidEncoderBackendVulkan;
	/**
	* @brief Vulkan 이미지에서 필요한 정보를 추출하는 유틸리티 클래스
	* ### 목적:
	* - Vulkan 에 넣는 이미지가 바로 적합하지 않으니 VKImage에서 필요한 정보를 추출하는 역할
	* - native Resource 추출
	* ### Result:
	* - width, height, format, layout 등 Vulkan 이미지로 제출하기 위해 필요한 정보를 추출
	* - FVKSubmitFrameInfo 구조체로 결과 반환
	* ### 한계:
	* - 자원 생성, command submit, 동기화 완료 처리
	*/
public:
	explicit FVdjmVkInputAnalyzer(FVdjmAndroidEncoderBackendVulkan* const owner)
		: FVdjmVkSubProcessContext(owner)
	{
	}

	bool Analyze(const FTextureRHIRef& srcTexture, FVdjmVkSubmitFrameInfo& outInfo) const;
	
};

/**	
 *	@brief Vulkan 이미지 제출 과정에서 필요한 중간 단계 처리 담당 클래스
 *	@class FVdjmVkIntermediateStage
 */
class FVdjmVkIntermediateStage: public FVdjmVkSubProcessContext
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
	explicit FVdjmVkIntermediateStage(FVdjmAndroidEncoderBackendVulkan* const owner)
		: FVdjmVkSubProcessContext(owner)
	{
	}

	bool NeedRecreate(const FVdjmVkSubmitFrameInfo& frameInfo,uint32 curWid,uint32 curhei,VkFormat  curFormat) const;
	bool EnsureResource(FVdjmAndroidEncoderBackendVulkan& backend, const FVdjmVkSubmitFrameInfo& frameInfo);
	bool RecordPrepareAndCopy(FVdjmAndroidEncoderBackendVulkan& owner, const FVdjmVkSubmitFrameInfo& frameInfo);
	
	FVdjmVkIntermediateImageState& GetIntermediateState() { return mIntermediateState; }
	const FVdjmVkIntermediateImageState& GetIntermediateStateConst() const { return mIntermediateState; }
	
	bool IsValidIntermediateImage() const { return mIntermediateState.IntermediateImage != VK_NULL_HANDLE; }
	bool IsValidIntermediateSwapchainResolutions(FVdjmAndroidEncoderBackendVulkan* backend = nullptr) const;
	bool IsValidIntermediateFormat(FVdjmAndroidEncoderBackendVulkan*  backend = nullptr) const;

private:
	FVdjmVkIntermediateImageState mIntermediateState;
};

/**	
 *	@brief Vulkan 이미지 제출 과정에서 필요한 최종 제출 담당 클래스
 *	@class FVdjmVkSurfaceSubmitter
 */
class FVdjmVkSurfaceSubmitter : public FVdjmVkSubProcessContext
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
	explicit FVdjmVkSurfaceSubmitter(FVdjmAndroidEncoderBackendVulkan* const owner)
		: FVdjmVkSubProcessContext(owner)
	{
	}

	bool Submit(FVdjmAndroidEncoderBackendVulkan& owner, double timeStampSec);
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

	VkCommandPool GetCommandPool() const { return mRecordSessionState.CommandPool; }
	VkCommandBuffer GetCommandBuffer() const { return mRecordSessionState.CommandBuffer; }
	const VkCommandBuffer* GetCommandBufferConst() const { return &mRecordSessionState.CommandBuffer; }
	VkFence GetSubmitFence() const { return mRecordSessionState.SubmitFence; }
	const VkFence* GetSubmitFenceConst() const { return &mRecordSessionState.SubmitFence; }
	
	VkSurfaceKHR GetCodecSurface() const { return mRecordSessionState.CodecSurface; }
	VkSwapchainKHR GetCodecSwapchain() const { return mRecordSessionState.CodecSwapchain; }
	const VkSwapchainKHR* GetCodecSwapchainConst() const { return &mRecordSessionState.CodecSwapchain; }

	const TArray<VkImage>& GetSwapchainImages() const { return mRecordSessionState.SwapchainImages; }
	const TArray<VkImageView>& GetSwapchainImageViews() const { return mRecordSessionState.SwapchainImageViews; }

	VkSemaphore GetAcquireSemaphore() const { return mRecordSessionState.AcquireSemaphore; }
	const VkSemaphore* GetAcquireSemaphoreConst() const { return &mRecordSessionState.AcquireSemaphore; }
	VkSemaphore GetRenderCompleteSemaphore() const { return mRecordSessionState.RenderCompleteSemaphore; }
	const VkSemaphore* GetRenderCompleteSemaphoreConst() const { return &mRecordSessionState.RenderCompleteSemaphore; }

	uint32 GetCurrentSwapchainImageIndex() const { return mCurrentSwapchainImageIndex32; }
	const uint32_t* GetCurrentSwapchainImageIndexConst() const { return &mCurrentSwapchainImageIndex32; }
	void SetCurrentSwapchainImageIndex(uint32 InIndex) { mCurrentSwapchainImageIndex32 = InIndex; }

	VkFormat GetSwapchainFormat() const { return mRecordSessionState.SwapchainFormat; }
	uint32 GetSwapchainWidth() const { return mRecordSessionState.SwapchainWidth; }
	uint32 GetSwapchainHeight() const { return mRecordSessionState.SwapchainHeight; }

	VkImage GetIntermediateImage() const { return mIntermediateStage.GetIntermediateStateConst(). IntermediateImage; }
	VkImageView GetIntermediateView() const { return mIntermediateStage.GetIntermediateStateConst().IntermediateView; }
	VkFormat GetIntermediateFormat() const { return mIntermediateStage.GetIntermediateStateConst().IntermediateFormat; }
	uint32 GetIntermediateWidth() const { return mIntermediateStage.GetIntermediateStateConst().IntermediateWidth; }
	uint32 GetIntermediateHeight() const { return mIntermediateStage.GetIntermediateStateConst().IntermediateHeight; }

	void SetIntermediateImage(VkImage InImage) { mIntermediateStage.GetIntermediateState().IntermediateImage = InImage; }
	void SetIntermediateView(VkImageView InView) { mIntermediateStage.GetIntermediateState().IntermediateView = InView; }
	void SetIntermediateMemory(VkDeviceMemory InMemory) { mIntermediateStage.GetIntermediateState().IntermediateMemory = InMemory; }
	void SetIntermediateFormat(VkFormat InFormat) { mIntermediateStage.GetIntermediateState().IntermediateFormat = InFormat; }
	void SetIntermediateWidth(uint32 InWidth) { mIntermediateStage.GetIntermediateState().IntermediateWidth = InWidth; }
	void SetIntermediateHeight(uint32 InHeight) { mIntermediateStage.GetIntermediateState().IntermediateHeight = InHeight; }

	bool IsValidIntermediateImage() const { return mIntermediateStage.IsValidIntermediateImage(); }
	
private:
	
	bool InitVkRuntimeContext();
	
	bool EnsureRuntimeReady();
	bool TryExtractNativeVkImage(const FTextureRHIRef& srcTexture, VkImage& outImage) const;
	bool SubmitTextureToCodecSurface(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, VkImage srcImage, double timeStampSec);

	FVdjmAndroidEncoderConfigure mConfig;
	
	bool mInitialized = false;
	bool mStarted = false;
	bool mPaused = false;
	bool mRuntimeReady = false;
	
	uint32_t mCurrentSwapchainImageIndex32;
	
	FVdjmVkRuntimeContext mVkRuntime;
	FVdjmVkRecordSessionState mRecordSessionState;
	
	FVdjmVkInputAnalyzer mAnalyzer;
	FVdjmVkIntermediateStage mIntermediateStage;
	FVdjmVkSurfaceSubmitter mSurfaceSubmitter;
	
};


#endif
