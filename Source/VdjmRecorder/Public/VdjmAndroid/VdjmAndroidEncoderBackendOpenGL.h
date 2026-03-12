// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmRecoderAndroidEncoder.h"
class FVdjmAndroidEncoderBackendOpenGL : public FVdjmAndroidEncoderBackend
{
public:
	virtual bool Init(const FVdjmAndroidEncoderConfigure& config) override;
	virtual bool Start() override;
	virtual void Stop() override;
	virtual void Terminate() override;
};
#endif