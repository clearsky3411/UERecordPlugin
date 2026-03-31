// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
#include "vulkan_core.h"

struct IVulkanDynamicRHI;
class FVdjmAndroidEncoderBackendVulkan;

/*
 * 1. GPU Copy/render
 * 2. codec input surface
 * 3. codec encoding
 * Start trigger 시
 * - Session 생성
 * - 
 * 
 */

class FVdjmVkRuntimeRefs
{
	
};
class FVdjmVkCodecInputSurfaceState
{
	
};
class FVdjmVkIntermediateState
{
	
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
	virtual bool Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture, double timeStampSec) override;

private:
	FVdjmAndroidEncoderConfigure mConfig;
	bool mInitialized = false;
	bool mStarted = false;
	bool mPaused = false;
};


#endif
