// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
class FVdjmAndroidEncoderBackendVulkan : public FVdjmAndroidEncoderBackend
{
	
};
#endif
