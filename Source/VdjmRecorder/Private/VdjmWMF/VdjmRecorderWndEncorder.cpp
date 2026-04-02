// Fill out your copyright notice in the Description page of Project Settings.

#include "VdjmWMF/VdjmRecorderWndEncoder.h"
//#include "VdjmRecorderWndEncorder.h"
#include "ThirdParty/FreeImage/FreeImage-3.18.0/Dist/FreeImage.h"

#if PLATFORM_WINDOWS
//DEFINE_LOG_CATEGORY(LogVdjmRecorderCore);
//	원래라면 build.cs 에서 링크해야 하지만, 여긴 cpp 파일이라서 여기에다가 직접 링크합니다.
#pragma comment( lib,"mf.lib" )
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

FVdjmWindowsEncoderImpl::FVdjmWindowsEncoderImpl()
{
	
	HRESULT hr = MFStartup( MF_VERSION );
	bUseStateChangeEvent = false;
	if (FAILED(hr))
	{
		ChangeEncoderStatus(EVdjmResourceStatus::EError);
	
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::FVdjmWindowsEncoderImpl - MFStartup failed. hr=0x%08X"), hr);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmWindowsEncoderImpl::FVdjmWindowsEncoderImpl - MFStartup succeeded."));
		ChangeEncoderStatus(EVdjmResourceStatus::ENew);
	}
}

FVdjmWindowsEncoderImpl::~FVdjmWindowsEncoderImpl()
{
	TerminateInternal();
}

bool FVdjmWindowsEncoderImpl::InitializeEncoder(const FString& outputFilePath, int32 width, int32 height, int32 bitrate,int32 framerate)
{
	switch (GetCurrentStatus()) {
	case EVdjmResourceStatus::ENew:	//	first init
		InitInternal(outputFilePath, width, height, bitrate, framerate);
		ChangeEncoderStatus(EVdjmResourceStatus::EReady);
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmWindowsEncoderImpl::InitializeEncoder - Encoder initialized successfully. Status changed to EReady."));
		//ChangeEncoderStatusBetween(EVdjmResourceStatus::ENew,EVdjmResourceStatus::EReady);
		return true;
	case EVdjmResourceStatus::EReady:	//	reinit
		if (mSinkWriter != nullptr)
		{
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::InitializeEncoder - Encoder is already initialized. Reinitializing with new settings."));
			TerminateInternal();
			InitInternal(outputFilePath, width, height, bitrate, framerate);
		}
		else
		{
			InitInternal(outputFilePath, width, height, bitrate, framerate);
		}
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmWindowsEncoderImpl::InitializeEncoder - Encoder reinitialized successfully. Status remains EReady."));
		return true;
	case EVdjmResourceStatus::ERunning:	//	false
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::InitializeEncoder - Encoder is currently running. Stopping current encoding session before reinitializing."));
		return false;
	case EVdjmResourceStatus::EWaiting:	//	true
		TerminateInternal();
		InitInternal(outputFilePath, width, height, bitrate, framerate);
		ChangeEncoderStatus(EVdjmResourceStatus::EReady);
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmWindowsEncoderImpl::InitializeEncoder - Encoder reinitialized successfully from waiting state. Status changed to EReady."));
		//ChangeEncoderStatusBetween(EVdjmResourceStatus::EWaiting,EVdjmResourceStatus::EReady);
		return true;
	case EVdjmResourceStatus::ETerminated:	//	false
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::InitializeEncoder - Encoder is already terminated. Please call ReInitializeEncoder if you want to change encoder settings."));
		return false;
	case EVdjmResourceStatus::EError:	//	check
		return TryRefreshEncoder(AsShared());
	case EVdjmResourceStatus::ENone:
		break;
	case EVdjmResourceStatus::EMax:
		break;
	}
	return false;
}

VdjmResult FVdjmWindowsEncoderImpl::StartEncoder()
{
	if (IsStateReady())
	{
		HRESULT hr = ReadyInternal();
		if (SUCCEEDED(hr))
		{
			UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmWindowsEncoderImpl::StartEncoder - Encoder started successfully."));
			ChangeEncoderStatus(EVdjmResourceStatus::ERunning);
			OnEncoderStarted.Broadcast(GetCurrentStatus(),this);
		}
		else
		{
			ChangeEncoderStatus(EVdjmResourceStatus::EError);
		}
		return static_cast<VdjmResult>(hr);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::StartEncoder - Encoder is not ready. Current status: %d"), static_cast<int32>(GetCurrentStatus()));
		return VdjmResults::Fail;
	}
}

void FVdjmWindowsEncoderImpl::RunningEncodeFrame(void* nv12Data, int32 bufferSize, double timeStampMs)
{
	if (IsStateRunning())
	{
		HRESULT hr = RunningEncodeFrameInternal(nv12Data, bufferSize, timeStampMs);
		if (FAILED(hr))
		{
			StopEncoder();
		}
		else
		{
			if (bUseEncodingEvent)
			{
				OnEncoding.Broadcast(this,bufferSize,timeStampMs);
			}
		}
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::RunningEncodeFrame - Encoder is not running. Current status: %d"), static_cast<int32>(GetCurrentStatus()));
	}
}

void FVdjmWindowsEncoderImpl::StopEncoder()
{
	HRESULT hr = S_OK;
	//	TO Wait
	switch (GetCurrentStatus()) {
	case EVdjmResourceStatus::ENew:	
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::StopEncoder - Encoder is not initialized. Nothing to stop."));
		break;
	case EVdjmResourceStatus::EReady://	to 
		StopInternal();
		ChangeEncoderStatus(EVdjmResourceStatus::EWaiting);
		break;
	case EVdjmResourceStatus::ERunning:
		{
			UE_LOG(LogVdjmRecorderCore, Log, TEXT("FVdjmWindowsEncoderImpl::StopEncoder - Stopping encoder and finalizing output file."));
			hr = mSinkWriter->Finalize();
			if (FAILED(hr))			
			{
				UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::StopEncoder - Finalize failed. File might be corrupted. hr=0x%08X"), hr);
				ChangeEncoderStatus(EVdjmResourceStatus::EError);
				TerminateInternal();
			}
			 else
			 {
			 	ChangeEncoderStatus(EVdjmResourceStatus::EWaiting);
			 	StopInternal();
			 }
		}
		break;
	case EVdjmResourceStatus::EWaiting:
		break;
	case EVdjmResourceStatus::ETerminated:
		break;
	case EVdjmResourceStatus::EError:
		{
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::StopEncoder - Encoder is in error state. Attempting to stop and clean up resources."));
			StopInternal();
			ChangeEncoderStatus(EVdjmResourceStatus::EWaiting);
		}
		break;
	}
}

void FVdjmWindowsEncoderImpl::TerminateEncoder()
{
	switch (GetCurrentStatus()) {
	case EVdjmResourceStatus::ENew:
	case EVdjmResourceStatus::EReady:
		TerminateInternal();
		ChangeEncoderStatus(EVdjmResourceStatus::ETerminated);
		break;
	case EVdjmResourceStatus::ERunning:
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::TerminateEncoder - Encoder is currently running. Stopping encoder before termination."));
		StopEncoder();
		CleanUpEncodingVariables();
		ChangeEncoderStatus(EVdjmResourceStatus::ETerminated);
		break;
	case EVdjmResourceStatus::EWaiting:
		TerminateInternal();
		ChangeEncoderStatus(EVdjmResourceStatus::ETerminated);
		break;
	case EVdjmResourceStatus::ETerminated:
		break;
	case EVdjmResourceStatus::EError:
		{
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("FVdjmWindowsEncoderImpl::StopEncoder - Encoder is in error state. Attempting to stop and clean up resources."));
			TerminateInternal();
			ChangeEncoderStatus(EVdjmResourceStatus::ETerminated);
		}
		break;
	}

}

void FVdjmWindowsEncoderImpl::ChangeStatusNewToReady()
{
	
	
}

void FVdjmWindowsEncoderImpl::InitInternal(const FString& outputFilePath, int32 width, int32 height, int32 bitrate, int32 framerate)
{
	mEncoderPropertyData.Initialize(width& ~15, height& ~15, bitrate, framerate,outputFilePath);
	mStartTimeStamp = -1;
}

HRESULT FVdjmWindowsEncoderImpl::ReadyInternal()
{
	HRESULT hr = S_OK;
	ComPtr<IMFAttributes> pAttributes;
	hr = MFCreateAttributes(&pAttributes, 1);
	if (FAILED(hr))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::StartInternal - MFCreateAttributes failed. hr=0x%08X"), hr);
		return hr;
	}
	pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1);
	hr = MFCreateSinkWriterFromURL(*mEncoderPropertyData.GetCurrentOutputFilePath(), nullptr, pAttributes.Get(), &mSinkWriter);
	if (FAILED(hr))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::InitializeEncoder - MFCreateSinkWriterFromURL failed. hr=0x%08X"), hr);
		return hr;
	}
	const int32 videoWidth = mEncoderPropertyData.GetVideoWidth() ;
	const int32 videoHeight = mEncoderPropertyData.GetVideoHeight();
	const int32 videoFPS = mEncoderPropertyData.GetVideoFPS();
	const int32 videoBitrate = mEncoderPropertyData.GetVideoBitrate();
	
	ComPtr<IMFMediaType> pOutType;
	MFCreateMediaType(&pOutType);
	pOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	pOutType->SetUINT32(MF_MT_AVG_BITRATE, videoBitrate);
	pOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	MFSetAttributeSize(pOutType.Get(), MF_MT_FRAME_SIZE, videoWidth, videoHeight);
	MFSetAttributeRatio(pOutType.Get(), MF_MT_FRAME_RATE, videoFPS, 1);
	MFSetAttributeRatio(pOutType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	
	hr = mSinkWriter->AddStream(pOutType.Get(), &mVideoStreamIndex);
	if (FAILED(hr))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::InitializeEncoder - AddStream failed. hr=0x%08X"), hr);
		return hr;
	}

	// 3. 입력 미디어 타입 설정 (NV12 - Readback 데이터 포맷)
	ComPtr<IMFMediaType> pInType;
	MFCreateMediaType(&pInType);
	pInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pInType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12); // ★중요: NV12
	pInType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	MFSetAttributeSize(pInType.Get(), MF_MT_FRAME_SIZE, videoWidth, videoHeight);
	MFSetAttributeRatio(pInType.Get(), MF_MT_FRAME_RATE, videoFPS, 1);
	MFSetAttributeRatio(pInType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

	hr = mSinkWriter->SetInputMediaType(mVideoStreamIndex, pInType.Get(), nullptr);
	if (FAILED(hr))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::InitializeEncoder - SetInputMediaType failed. hr=0x%08X"), hr);
		return hr;
	}
	hr = mSinkWriter->BeginWriting();
	if (FAILED(hr))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::RecodingStart - BeginWriting failed. hr=0x%08X"), hr);
	}
	
	return hr;
}

HRESULT FVdjmWindowsEncoderImpl::RunningEncodeFrameInternal(void* nv12Data, int32 bufferSize, double timeStampMs)
{
	HRESULT hr = S_OK;

	//	step: 미디어 버퍼 생성
	ComPtr<IMFMediaBuffer> pBuffer;
	hr = MFCreateMemoryBuffer(bufferSize, &pBuffer);
	if (FAILED(hr))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::RunningEncodeFrameInternal - MFCreateMemoryBuffer failed. hr=0x%08X"), hr);
		return hr;
	}
	
	BYTE* pDest = nullptr;
	hr = pBuffer->Lock(&pDest, nullptr, nullptr);

	//	step: 버퍼에 NV12 데이터 복사
	if (SUCCEEDED(hr))
	{
		FMemory::Memcpy(pDest, nv12Data, bufferSize);
		pBuffer->Unlock();
		pBuffer->SetCurrentLength(bufferSize);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::RunningEncodeFrameInternal - IMFMediaBuffer::Lock failed. hr=0x%08X"), hr);
		return hr;
	}

	//	step: 샘플 생성 및 설정
	ComPtr<IMFSample> pSample;
	hr = MFCreateSample(&pSample);
	if (SUCCEEDED(hr))
	{
		pSample->AddBuffer(pBuffer.Get());
		// 타임스탬프 설정
		LONGLONG curTime = static_cast<LONGLONG>(timeStampMs * ToNanosec); // 밀리초 -> 100나노초 단위
		if (mStartTimeStamp < 0)
		{
			mStartTimeStamp = curTime;
			
			// FVdjmEncoderStatus status = mCurrentEncoderStatus.OnStarting();
			// FVdjmEncoderStatus::DbcGameThreadTask([this,status]()
			// {
			// 	OnEncoderStatusUpdated.Broadcast(status);
			// });
		}
		LONGLONG adjustedTime = curTime - mStartTimeStamp;
		pSample->SetSampleTime(adjustedTime);

		LONGLONG duration = static_cast<LONGLONG>(ToNanosec / mEncoderPropertyData.GetVideoFPS()); // 프레임 지속 시간 (100나노초 단위)
		pSample->SetSampleDuration(duration);
		
		UE_LOG(LogVdjmRecorderCore, Verbose, TEXT("FVdjmWindowsEncoderImpl::RunningEncodeFrameInternal - Writing sample with timestamp: %lld (adjusted: %lld) ms"), curTime / ToNanosec, adjustedTime / ToNanosec);
		//	step : 샘플 쓰기
		hr = mSinkWriter->WriteSample(mVideoStreamIndex, pSample.Get());
		if (FAILED(hr))
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::RunningEncodeFrameInternal - WriteSample failed. hr=0x%08X"), hr);
		}
		return hr;
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("FVdjmWindowsEncoderImpl::RunningEncodeFrameInternal - MFCreateSample failed. hr=0x%08X"), hr);
		return hr;
	}
}

void FVdjmWindowsEncoderImpl::StopInternal()
{
	CleanUpWMFVariables();
	CleanUpRunningVariables();
	
}

void FVdjmWindowsEncoderImpl::TerminateInternal()
{
	StopInternal();
	CleanUpEncodingVariables();
}

void FVdjmWindowsEncoderImpl::CleanUpEncodingVariables()
{
	mEncoderPropertyData.Clear();
}

void FVdjmWindowsEncoderImpl::CleanUpWMFVariables()
{
	if (mSinkWriter)
	{
		if (IsStateRunning())
		{
			HRESULT hr = mSinkWriter->Flush(mVideoStreamIndex);
			if (FAILED(hr))
			{
				UE_LOG(LogVdjmRecorderCore, Warning, TEXT("[WindowsEncoder] Finalize Failed. File might be corrupted. HR: 0x%X"), hr);
			}
		}
		mSinkWriter.Reset(); // Release
	}
	mSinkWriter = nullptr;
	mVideoStreamIndex = 0;
}

void FVdjmWindowsEncoderImpl::CleanUpRunningVariables()
{
	mStartTimeStamp = -1;
}

#endif
