// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
#include "vulkan_core.h"

struct FVkEncoderContext
{
	VkInstance Instance = VK_NULL_HANDLE;
	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	VkDevice Device = VK_NULL_HANDLE;
	VkQueue GraphicsQueue = VK_NULL_HANDLE;
	uint32_t GraphicsQueueFamilyIndex = 0;
};

struct FVkSubmitFrameInfo
{
	VkImage SrcImage = VK_NULL_HANDLE;
	VkFormat SrcFormat = VK_FORMAT_R8G8B8A8_UNORM;
	uint32_t SrcWidth = 0;
	uint32_t SrcHeight = 0;
	VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
};

class FVdjmVKInputAnalyzer
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
	
};
class FVdjmVkIntermediateStage
{
	/**
 	* @brief 불칸 내부 이미지 제출 과정에서 필요한 중간 단계 처리 클래스
 	* ### 목적:
 	* - intermediate image 필요 판단
 	* - intermediate image / view / memory 생성/재생성
 	* - 입력 이미지 → intermediate 로 copy / blit / render
 	* - layout transition 규칙 적용
 	* ### Result:
 	* - 가공된 Vulkan 이미지 생성
 	* - 제출 가능한 상태로 Vulkan 이미지 준비
 	* ### 한계:
 	* - 입력 텍스처 해석
 	* - 최종 제출 완료 관리
 	*/
public:
	
};
class FVdjmVkSurfaceSubmitter
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
	
};

class FVdjmAndroidEncoderBackendVulkan : public FVdjmAndroidEncoderBackend
{
public:
	FVdjmAndroidEncoderBackendVulkan() = default;
	virtual ~FVdjmAndroidEncoderBackendVulkan() override = default;
	
	virtual bool Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow) override;
	virtual bool Start() override;
	virtual void Stop() override;
	virtual void Terminate() override;
	virtual bool Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec) override;
	
	bool IsRunnable();
private:
	
	bool EnsureRuntimeReady();
	bool TryExtractNativeVkImage(const FTextureRHIRef& srcTexture, VkImage& outImage) const;
	bool SubmitTextureToCodecSurface(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, VkImage srcImage, double timeStampSec);

	FVdjmAndroidEncoderConfigure mConfig;
	
	bool mInitialized = false;
	bool mStarted = false;
	bool mPaused = false;
	bool mRuntimeReady = false;
};

#endif
