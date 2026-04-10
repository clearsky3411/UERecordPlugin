// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VdjmRecordTypes.h"
#include "UObject/Object.h"



/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
		template EncoderBase for cross platform encoder implementation
*/
/*
 *	New						New To Ready : Initialize
 *	Ready					Ready to Running : StartEncoder
 *	Running					Running to Waiting : StopEncoder
 *	Waiting					Waiting to Running : StartEncoder
 *	Terminated				Waiting to Terminated : TerminateEncoder
 *	
 *	Error
 * 
 * InitializeEncoder 이걸 사용하면 뭐가 되었든 초기화 하면서 내부에 할당된것들 모두 정리.
 * StartEncoder 반드시 초기화가 진행된 상태에서만 실행하며 대충 여기에서 반복전에 초기화 해야하는 멱등성있는거 처리
 * RunningEncodeFrame 매 프레임당 호출, nv12Data 는 인코딩할 프레임의 데이터, bufferSize 는 nv12Data 의 버퍼 사이즈, timeStampMs 는 녹화 시작부터 현재 프레임까지의 타임스탬프 (밀리세컨드 단위)
 * StopEncoder 인코딩을 멈추고 대충 여기에서 인코딩이 끝나면서 후처리가 필요한거 처리, 이때 InitializeEncoder 여기에서 설정해준 건 처리하지 않는다. 다만 RunningEncodeFrame 여기에서 초기화 했던 것은 정리.
 * TerminateEncoder 인코더 완전 종료, 초기화 했던거 모두 정리  InitializeEncoder 에 있는 것들도 정리
 * 변수 종류
 *	- 엔코딩 전에 설정들은 우선순위 1번.
 *	- 인코딩 중에 설정 변경이 필요한 것들은 우선순위 2번. (InitializeEncoder 에서 설정한 것들 중에서 멱등성 있게 StartEncoder 에서 처리할 수 있는 것들)	
 *	- StopEncoder 의 경우에는 2번만 정리.
 *	- Terminated 는 1번도 정리
 *	- 1번이 바뀌면 2번도 당연히 영향을 받아야함. 우선순위는 계층이라 1번만 바꾸고 2번은 그대로 따위는 없음.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FVdjmVideoEncoderStatusChanged, EVdjmResourceStatus /*prevStatus*/, EVdjmResourceStatus /*newStatus*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FVdjmVideoEncoderEvent,  EVdjmResourceStatus /*encoderStatus*/, FVdjmVideoEncoderBase* /*encoderInstance*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FVdjmVideoEncoderTickEvent,  FVdjmVideoEncoderBase* /*encoderInstance*/, int32 /*bufferSize*/,double /*timeStampMs*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FVdjmVideoEncoderRefreshEvent, TWeakPtr<FVdjmVideoEncoderBase> /*weakThis*/);

class FEncoderImplPropertyData
{
public:
	void Initialize(int32 width, int32 height, int32 bitrate, int32 fps, const FString& outputFilePath)
	{
		mVideoWidth = width;
		mVideoHeight = height;
		mVideoFPS = fps;
		mVideoBitrate = bitrate;
		mCurrentOutputFilePath = outputFilePath;
	}
	
	void Update(int32 width, int32 height, int32 bitrate, int32 fps, const FString& outputFilePath)
	{
		if (mVideoWidth != width)
		{
			mVideoWidth = width;
			mDirty = true;
		}
		if (mVideoHeight != height)
		{
			mVideoHeight = height;
			mDirty = true;
		}
		if (mVideoFPS != fps)
		{
			mVideoFPS = fps;
			mDirty = true;
		}
		if (mVideoBitrate != bitrate)
		{
			mVideoBitrate = bitrate;
			mDirty = true;
		}
		if (mCurrentOutputFilePath != outputFilePath)
		{
			mCurrentOutputFilePath = outputFilePath;
			mDirty = true;
		}
	}
	
	int32 GetVideoWidth() const { return mVideoWidth; }
	int32 GetVideoHeight() const { return mVideoHeight; }
	FIntPoint GetVideoResolution() const { return FIntPoint(mVideoWidth, mVideoHeight); }
	int32 GetVideoFPS() const { return mVideoFPS; }
	int32 GetVideoBitrate() const { return mVideoBitrate; }
	FString GetCurrentOutputFilePath() const { return mCurrentOutputFilePath; }
	
	
	bool IsDirty() const { return mDirty; }
	void ClearDirty() { mDirty = false; }
	
	void Clear()
	{
		mVideoWidth = 0;
		mVideoHeight = 0;
		mVideoFPS = 0;
		mVideoBitrate = 0;
		mCurrentOutputFilePath.Empty();
		mDirty = false;
	}
	
	FVdjmVideoEncoderRefreshEvent OnPropertyDataChanged;
	
private:
	int32 mVideoWidth = 0;
	int32 mVideoHeight = 0;
	int32 mVideoFPS = 0;
	int32 mVideoBitrate = 0;
	FString mCurrentOutputFilePath;
	bool mDirty = false;
};

class FVdjmVideoEncoderBase : public TSharedFromThis<FVdjmVideoEncoderBase, ESPMode::ThreadSafe>
{
public:
	using EVdjmEncoderStatus = EVdjmResourceStatus;
	constexpr static int64 ToNanosec = 10000000;	
	constexpr static int64 DefaultBitrate = 2000000;	//	5Mbps
	
	virtual ~FVdjmVideoEncoderBase() = default;
	
	virtual bool InitializeEncoder(const FString& outputFilePath,int32 width,int32 height,int32 bitrate,int32 framerate) = 0;
	 
	virtual bool InitializeEncoderExtended(const TWeakObjectPtr<UVdjmRecordResource> recordResource)
	{
		return false;
	}
	
	
	virtual VdjmResult StartEncoder() = 0;
	
	virtual void RunningEncodeFrame(void* nv12Data,int32 bufferSize,double timeStampMs){};
	
	virtual bool SubmitSurfaceFrame(FRHICommandList& graphBuilder, const FTextureRHIRef& srcTexture, double timeStampSec) { return false; }
	//virtual bool DrainEncoder(bool bEndOfStream) { return false; }
	
	virtual void StopEncoder() = 0;
	virtual void TerminateEncoder() = 0;
	
	virtual bool TryRefreshEncoder(TWeakPtr<FVdjmVideoEncoderBase> weakThis) { return false; }
	
	
	EVdjmEncoderStatus GetCurrentStatus() const { return CurrentStatus; }
	bool IsStateNew() const { return CurrentStatus == EVdjmEncoderStatus::ENew; }
	bool IsStateReady() const { return CurrentStatus == EVdjmEncoderStatus::EReady; }
	bool IsStateRunning() const { return CurrentStatus == EVdjmEncoderStatus::ERunning; }
	bool IsStateWaiting() const { return CurrentStatus == EVdjmEncoderStatus::EWaiting; }
	bool IsStateTerminated() const { return CurrentStatus == EVdjmEncoderStatus::ETerminated; }
	bool IsStateError() const { return CurrentStatus == EVdjmEncoderStatus::EError; }
	
	FVdjmVideoEncoderStatusChanged OnEncoderStatusChanged;
	
	FVdjmVideoEncoderEvent OnEncoderStarted;
	FVdjmVideoEncoderTickEvent OnEncoding;
	FVdjmVideoEncoderEvent OnEncoderStopped;
	
	bool bUseStateChangeEvent = true;
	bool bUseEncodingEvent = true;
protected:
	void ChangeEncoderStatus(EVdjmEncoderStatus newStatus);
	//void ChangeEncoderStatusBetween(EVdjmEncoderStatus expectedCurrentStatus, EVdjmEncoderStatus newStatus);
	
	virtual void ChangeStatusNewToReady() {  }
	virtual void ChangeStatusReadyToRunning() {  }
	virtual void ChangeStatusRunningToWaiting() {  }
	virtual void ChangeStatusWaitingToReady() {  }
		
	FVdjmEncoderInitRequest mInitRequest;
private:
	EVdjmEncoderStatus CurrentStatus = EVdjmEncoderStatus::ENew;
	
};
