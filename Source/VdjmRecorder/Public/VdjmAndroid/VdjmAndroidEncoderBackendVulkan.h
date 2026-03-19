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
 *	@brief Vulkan 이미지 제출 과정에서 필요한 분석과 중간 단계 처리 담당 클래스들의 공통 기반 클래스
 *	@class FVdjmVkSubProcessContext
 */
class FVdjmVkSubProcessContext
{
public:
	FVdjmVkSubProcessContext(FVdjmAndroidEncoderBackendVulkan* owner) : OwnerBackend(owner) {}
	virtual ~FVdjmVkSubProcessContext() = default;
protected:
	class FVdjmAndroidEncoderBackendVulkan* OwnerBackend = nullptr;
};

class FVdjmVkInputAnalyzer : public FVdjmVkSubProcessContext
{
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
};

/**	
 *	@brief Vulkan 이미지 제출 과정에서 필요한 최종 제출 담당 클래스
 *	@class FVdjmVkSurfaceSubmitter
 */
class FVdjmVkSurfaceSubmitter : public FVdjmVkSubProcessContext
{
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

	// optional but same lifetime
	VkImage IntermediateImage = VK_NULL_HANDLE;
	VkDeviceMemory IntermediateMemory = VK_NULL_HANDLE;
	VkImageView IntermediateView = VK_NULL_HANDLE;
	VkFormat IntermediateFormat = VK_FORMAT_UNDEFINED;
	uint32 IntermediateWidth = 0;
	uint32 IntermediateHeight = 0;
};

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

	VkCommandPool GetCommandPool() const { return mCommandPool; }
	VkCommandBuffer GetCommandBuffer() const { return mCommandBuffer; }
	VkFence GetSubmitFence() const { return mSubmitFence; }

	VkSurfaceKHR GetCodecSurface() const { return mCodecSurface; }
	VkSwapchainKHR GetCodecSwapchain() const { return mCodecSwapchain; }

	const TArray<VkImage>& GetSwapchainImages() const { return mSwapchainImages; }
	const TArray<VkImageView>& GetSwapchainImageViews() const { return mSwapchainImageViews; }

	VkSemaphore GetAcquireSemaphore() const { return mAcquireSemaphore; }
	VkSemaphore GetRenderCompleteSemaphore() const { return mRenderCompleteSemaphore; }

	uint32 GetCurrentSwapchainImageIndex() const { return mCurrentSwapchainImageIndex; }
	void SetCurrentSwapchainImageIndex(uint32 InIndex) { mCurrentSwapchainImageIndex = InIndex; }

	VkFormat GetSwapchainFormat() const { return mSwapchainFormat; }
	uint32 GetSwapchainWidth() const { return mSwapchainWidth; }
	uint32 GetSwapchainHeight() const { return mSwapchainHeight; }

	VkImage GetIntermediateImage() const { return mIntermediateImage; }
	VkImageView GetIntermediateView() const { return mIntermediateView; }
	VkFormat GetIntermediateFormat() const { return mIntermediateFormat; }
	uint32 GetIntermediateWidth() const { return mIntermediateWidth; }
	uint32 GetIntermediateHeight() const { return mIntermediateHeight; }

	void SetIntermediateImage(VkImage InImage) { mIntermediateImage = InImage; }
	void SetIntermediateView(VkImageView InView) { mIntermediateView = InView; }
	void SetIntermediateMemory(VkDeviceMemory InMemory) { mIntermediateMemory = InMemory; }
	void SetIntermediateFormat(VkFormat InFormat) { mIntermediateFormat = InFormat; }
	void SetIntermediateWidth(uint32 InWidth) { mIntermediateWidth = InWidth; }
	void SetIntermediateHeight(uint32 InHeight) { mIntermediateHeight = InHeight; }

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
	
	FVdjmVkRuntimeContext mVkRuntime;
	FVdjmVkRecordSessionState mRecordSessionState;
	
	VkImage mIntermediateImage = VK_NULL_HANDLE;
	VkDeviceMemory mIntermediateMemory = VK_NULL_HANDLE;
	VkImageView mIntermediateView = VK_NULL_HANDLE;

	VkCommandPool mCommandPool = VK_NULL_HANDLE;
	VkCommandBuffer mCommandBuffer = VK_NULL_HANDLE;
	VkFence mSubmitFence = VK_NULL_HANDLE;

	uint32 mIntermediateWidth = 0;
	uint32 mIntermediateHeight = 0;
	VkFormat mIntermediateFormat = VK_FORMAT_UNDEFINED;

	FVdjmVkInputAnalyzer mAnalyzer;
	FVdjmVkIntermediateStage mIntermediateStage;
	FVdjmVkSurfaceSubmitter mSubmitter;
	
//	----	
	
	VkSurfaceKHR mCodecSurface = VK_NULL_HANDLE;
	VkSwapchainKHR mCodecSwapchain = VK_NULL_HANDLE;

	TArray<VkImage> mSwapchainImages;
	TArray<VkImageView> mSwapchainImageViews;

	VkSemaphore mAcquireSemaphore = VK_NULL_HANDLE;
	VkSemaphore mRenderCompleteSemaphore = VK_NULL_HANDLE;

	uint32 mCurrentSwapchainImageIndex = 0;
	VkFormat mSwapchainFormat = VK_FORMAT_UNDEFINED;
	uint32 mSwapchainWidth = 0;
	uint32 mSwapchainHeight = 0;
};

#endif
