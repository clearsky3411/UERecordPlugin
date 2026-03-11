// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "UObject/Object.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecoderEncoderImpl.h"

#ifndef FALSE
	#define FALSE 0
#endif

#ifndef TRUE
	#define TRUE 1
#endif//	Includes

#endif
//#include "VdjmRecorderWndEncoder.generated.h"


#if PLATFORM_WINDOWS
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓			LOG Categories for Vdjm Recorder				↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
//DECLARE_LOG_CATEGORY_EXTERN(LogVdjmRecorderEncorder, Log, All);

using namespace Microsoft::WRL;

class FVdjmWindowsEncoderImpl : public FVdjmVideoEncoderBase
{
public:
	FVdjmWindowsEncoderImpl();
	virtual ~FVdjmWindowsEncoderImpl() override;
	
	virtual bool InitializeEncoder(const FString& outputFilePath, int32 width, int32 height, int32 bitrate,
		int32 framerate) override;
	virtual VdjmResult StartEncoder() override;
	virtual void RunningEncodeFrame(void* nv12Data, int32 bufferSize, double timeStampMs) override;
	virtual void StopEncoder() override;
	virtual void TerminateEncoder() override;
protected:
	virtual void ChangeStatusNewToReady() override;
private:
	void InitInternal(const FString& outputFilePath,int32 width,int32 height,int32 bitrate,int32 framerate);
	HRESULT ReadyInternal();
	HRESULT RunningEncodeFrameInternal(void* nv12Data, int32 bufferSize, double timeStampMs) ;
	void StopInternal();
	void TerminateInternal();
	
	void CleanUpEncodingVariables();
	void CleanUpWMFVariables();
	void CleanUpRunningVariables();
	/*
	 *  ## Encoder Variables
	 *  이게 변화하면 전부가 변해야함. 다르게 이야기하면, mSinkWriter 가 존재할경우 처음부터 mSinkWriter 여기 생성까지 다시 해줘야함
	 *  반면 mSinkWriter 가 존재하지 않는 경우에는, 초기화 변수들만 바꿔주면 됨.
	 */
	// int32 mVideoWidth = 0;
	// int32 mVideoHeight = 0;
	// int32 mVideoFPS = 0;
	// int32 mVideoBitrate = 0;
	// FString mCurrentOutputFilePath;
	//
	FEncoderImplPropertyData mEncoderPropertyData;
	FDelegateHandle mEncoderPropertyDataChangedHandle;
	/*
	 * ## Windows Media Foundation variables 
	 */
	DWORD mVideoStreamIndex = 0;
	ComPtr<IMFSinkWriter> mSinkWriter;
	/*
	 * ## Running Variables
	 */
	LONGLONG mStartTimeStamp = 0;
};




#endif

// UCLASS()
// class VDJMMOBILEUI_API UVdjmRecorderWndEncoder : public UObject
// {
// 	GENERATED_BODY()
// };
