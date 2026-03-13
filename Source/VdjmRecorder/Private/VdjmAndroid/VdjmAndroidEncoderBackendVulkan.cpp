// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidEncoderBackendVulkan.h"
bool FVdjmAndroidEncoderBackendVulkan::Init(const FVdjmAndroidEncoderConfigure& config, ANativeWindow* inputWindow)
{
	if (config.IsValidateEncoderArguments() && inputWindow != nullptr)
	{
		//mConfig = config;
		mInputWindow = inputWindow;
		return true;
	}
	return false;
}
bool FVdjmAndroidEncoderBackendVulkan::Start()
{
	return true;
}

void FVdjmAndroidEncoderBackendVulkan::Stop()
{
}

void FVdjmAndroidEncoderBackendVulkan::Terminate()
{
}

bool FVdjmAndroidEncoderBackendVulkan::Running(FRHICommandList& RHICmdList, const FTextureRHIRef& srcTexture,
	double timeStampSec)
{
	return false;
}
