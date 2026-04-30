#include "VdjmRecorderDebugStatusWidget.h"

#include "Components/TextBlock.h"
#include "Rendering/DrawElements.h"
#include "TimerManager.h"
#include "VdjmRecordBridgeActor.h"
#include "VdjmRecorderCore.h"

namespace
{
	FString GetBoolStatusString(bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString GetMediaPublishStatusString(EVdjmRecordMediaPublishStatus publishStatus)
	{
		const UEnum* enumObject = StaticEnum<EVdjmRecordMediaPublishStatus>();
		return enumObject != nullptr
			? enumObject->GetNameStringByValue(static_cast<int64>(publishStatus))
			: FString::FromInt(static_cast<int32>(publishStatus));
	}
}

void UVdjmRecorderDebugStatusWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshRecorderDebugStatus();
	RestartAutoRefreshTimer();
}

void UVdjmRecorderDebugStatusWidgetBase::NativeDestruct()
{
	StopAutoRefreshTimer();
	Super::NativeDestruct();
}

void UVdjmRecorderDebugStatusWidgetBase::NativeTick(const FGeometry& myGeometry, float inDeltaTime)
{
	Super::NativeTick(myGeometry, inDeltaTime);
	AddFrameGraphSample(inDeltaTime);
}

int32 UVdjmRecorderDebugStatusWidgetBase::NativePaint(
	const FPaintArgs& args,
	const FGeometry& allottedGeometry,
	const FSlateRect& myCullingRect,
	FSlateWindowElementList& outDrawElements,
	int32 layerId,
	const FWidgetStyle& inWidgetStyle,
	bool bParentEnabled) const
{
	const int32 nextLayerId = Super::NativePaint(
		args,
		allottedGeometry,
		myCullingRect,
		outDrawElements,
		layerId,
		inWidgetStyle,
		bParentEnabled);

	return PaintFrameGraph(allottedGeometry, outDrawElements, nextLayerId);
}

FVdjmRecorderDebugStatusSnapshot UVdjmRecorderDebugStatusWidgetBase::RefreshRecorderDebugStatus()
{
	mLastStatusSnapshot = BuildDebugStatusSnapshot();
	ApplyStatusTextToBoundWidget(mLastStatusSnapshot);
	BP_OnRecorderDebugStatusUpdated(mLastStatusSnapshot);
	return mLastStatusSnapshot;
}

FVdjmRecorderDebugStatusSnapshot UVdjmRecorderDebugStatusWidgetBase::GetLastRecorderDebugStatusSnapshot() const
{
	return mLastStatusSnapshot;
}

FText UVdjmRecorderDebugStatusWidgetBase::GetLastStatusText() const
{
	return mLastStatusSnapshot.StatusText;
}

FString UVdjmRecorderDebugStatusWidgetBase::GetLastStatusLine() const
{
	return mLastStatusSnapshot.StatusLine;
}

void UVdjmRecorderDebugStatusWidgetBase::SetAutoRefreshEnabled(bool bEnabled)
{
	bAutoRefresh = bEnabled;
	RestartAutoRefreshTimer();
}

void UVdjmRecorderDebugStatusWidgetBase::SetRefreshIntervalSeconds(float refreshIntervalSeconds)
{
	RefreshIntervalSeconds = FMath::Max(0.05f, refreshIntervalSeconds);
	RestartAutoRefreshTimer();
}

bool UVdjmRecorderDebugStatusWidgetBase::IsAutoRefreshEnabled() const
{
	return bAutoRefresh;
}

float UVdjmRecorderDebugStatusWidgetBase::GetRefreshIntervalSeconds() const
{
	return RefreshIntervalSeconds;
}

UVdjmRecorderController* UVdjmRecorderDebugStatusWidgetBase::GetDebugRecorderController() const
{
	if (mCachedController.IsValid())
	{
		return mCachedController.Get();
	}

	UVdjmRecorderController* controller = bCreateControllerIfMissing
		? UVdjmRecorderController::FindOrCreateRecorderController(const_cast<UVdjmRecorderDebugStatusWidgetBase*>(this))
		: UVdjmRecorderController::FindRecorderController(const_cast<UVdjmRecorderDebugStatusWidgetBase*>(this));
	mCachedController = controller;
	return controller;
}

AVdjmRecordBridgeActor* UVdjmRecorderDebugStatusWidgetBase::GetDebugBridgeActor() const
{
	if (mCachedBridgeActor.IsValid())
	{
		return mCachedBridgeActor.Get();
	}

	if (const UVdjmRecorderController* controller = GetDebugRecorderController())
	{
		mCachedBridgeActor = controller->GetBridgeActor();
	}

	return mCachedBridgeActor.Get();
}

TArray<FVdjmRecorderDebugFrameSample> UVdjmRecorderDebugStatusWidgetBase::GetFrameGraphSamples() const
{
	return mFrameGraphSamples;
}

void UVdjmRecorderDebugStatusWidgetBase::ResetFrameGraphSamples()
{
	mFrameGraphSamples.Reset();
	mFrameGraphStartTimeSeconds = FPlatformTime::Seconds();
	mLastRecorderFrameAdvanceTimeSeconds = mFrameGraphStartTimeSeconds;
	mGameFrameSpikeCount = 0;
	mRecorderFrameGapCount = 0;
	mbLastRecorderFrameGap = false;

	const AVdjmRecordBridgeActor* bridgeActor = GetDebugBridgeActor();
	mLastGraphRecordedFrameCount = bridgeActor != nullptr ? bridgeActor->GetRecordedFrameCount() : 0;
}

void UVdjmRecorderDebugStatusWidgetBase::SetFrameGraphEnabled(bool bEnabled)
{
	bCollectFrameGraphSamples = bEnabled;
	bDrawFrameGraph = bEnabled;
}

bool UVdjmRecorderDebugStatusWidgetBase::IsFrameGraphEnabled() const
{
	return bCollectFrameGraphSamples && bDrawFrameGraph;
}

void UVdjmRecorderDebugStatusWidgetBase::RestartAutoRefreshTimer()
{
	StopAutoRefreshTimer();
	if (not bAutoRefresh)
	{
		return;
	}

	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return;
	}

	world->GetTimerManager().SetTimer(
		mRefreshTimerHandle,
		this,
		&UVdjmRecorderDebugStatusWidgetBase::HandleAutoRefreshTimer,
		FMath::Max(0.05f, RefreshIntervalSeconds),
		true);
}

void UVdjmRecorderDebugStatusWidgetBase::StopAutoRefreshTimer()
{
	UWorld* world = GetWorld();
	if (world != nullptr)
	{
		world->GetTimerManager().ClearTimer(mRefreshTimerHandle);
	}
}

void UVdjmRecorderDebugStatusWidgetBase::ApplyStatusTextToBoundWidget(const FVdjmRecorderDebugStatusSnapshot& statusSnapshot)
{
	if (bWriteStatusToTextBlock && StatusTextBlock != nullptr)
	{
		StatusTextBlock->SetText(statusSnapshot.StatusText);
	}
}

void UVdjmRecorderDebugStatusWidgetBase::HandleAutoRefreshTimer()
{
	RefreshRecorderDebugStatus();
}

FVdjmRecorderDebugStatusSnapshot UVdjmRecorderDebugStatusWidgetBase::BuildDebugStatusSnapshot()
{
	FVdjmRecorderDebugStatusSnapshot statusSnapshot;

	UVdjmRecorderController* controller = GetDebugRecorderController();
	statusSnapshot.bHasController = controller != nullptr;
	if (controller != nullptr)
	{
		const FVdjmRecorderControllerStatusSnapshot controllerSnapshot = controller->GetControllerStatusSnapshot();
		statusSnapshot.bHasBridgeActor = controllerSnapshot.bHasBridgeActor;
		statusSnapshot.bIsRecording = controllerSnapshot.bIsRecording;
		statusSnapshot.bIsFinalizingRecording = controllerSnapshot.bIsFinalizingRecording;
		statusSnapshot.bIsPostProcessingMedia = controllerSnapshot.bIsPostProcessingMedia;
		statusSnapshot.bIsControllerBusy = controller->IsControllerBusy();
		statusSnapshot.bHasLatestArtifact = controllerSnapshot.bHasLatestArtifact;
		statusSnapshot.bIsLatestArtifactValid = controllerSnapshot.bIsLatestArtifactValid;
		statusSnapshot.bHasLatestMetadata = controllerSnapshot.bHasLatestMetadata;
		statusSnapshot.ActiveMediaPublishJobCount = controllerSnapshot.ActiveMediaPublishJobCount;
		statusSnapshot.LatestMediaPublishStatus = controllerSnapshot.LatestMediaPublishStatus;
		statusSnapshot.LatestOutputFilePath = controllerSnapshot.LatestOutputFilePath;
		statusSnapshot.LatestMetadataFilePath = controllerSnapshot.LatestMetadataFilePath;
		statusSnapshot.LatestPublishedContentUri = controllerSnapshot.LatestPublishedContentUri;
		statusSnapshot.LatestMediaPublishErrorReason = controller->GetLatestMediaPublishErrorReason();
		statusSnapshot.ControllerStatusText = controllerSnapshot.StatusText;

		if (const UVdjmRecordArtifact* latestArtifact = controller->GetLatestArtifact())
		{
			statusSnapshot.LatestArtifactValidationError = latestArtifact->GetValidationError();
		}
	}

	AVdjmRecordBridgeActor* bridgeActor = GetDebugBridgeActor();
	statusSnapshot.bHasBridgeActor = statusSnapshot.bHasBridgeActor || bridgeActor != nullptr;
	if (bridgeActor != nullptr)
	{
		statusSnapshot.bIsRecording = bridgeActor->IsRecording();
		statusSnapshot.bIsFinalizingRecording = bridgeActor->IsFinalizingRecording();
		statusSnapshot.CurrentInitStep = bridgeActor->GetCurrentInitStep();
		statusSnapshot.CurrentInitStepName = AVdjmRecordBridgeActor::GetInitStepName(statusSnapshot.CurrentInitStep);
		statusSnapshot.RecordedFrameCount = bridgeActor->GetRecordedFrameCount();
	}

	statusSnapshot.EstimatedFrameRate = CalculateEstimatedFrameRate(
		statusSnapshot.RecordedFrameCount,
		statusSnapshot.bIsRecording);
	ApplyFrameGraphStats(statusSnapshot);
	statusSnapshot.StatusLine = BuildDebugStatusLine(statusSnapshot);
	statusSnapshot.StatusText = BuildDebugStatusText(statusSnapshot);
	return statusSnapshot;
}

FString UVdjmRecorderDebugStatusWidgetBase::BuildDebugStatusLine(const FVdjmRecorderDebugStatusSnapshot& statusSnapshot) const
{
	return FString::Printf(
		TEXT("Controller=%s Bridge=%s Recording=%s Finalizing=%s PostProcess=%s Jobs=%d Frames=%d RecFPS=%.1f GameFPS=%.1f Spike=%d Gap=%d Publish=%s"),
		*GetBoolStatusString(statusSnapshot.bHasController),
		*GetBoolStatusString(statusSnapshot.bHasBridgeActor),
		*GetBoolStatusString(statusSnapshot.bIsRecording),
		*GetBoolStatusString(statusSnapshot.bIsFinalizingRecording),
		*GetBoolStatusString(statusSnapshot.bIsPostProcessingMedia),
		statusSnapshot.ActiveMediaPublishJobCount,
		statusSnapshot.RecordedFrameCount,
		statusSnapshot.EstimatedFrameRate,
		statusSnapshot.LastGameFrameRate,
		statusSnapshot.GameFrameSpikeCount,
		statusSnapshot.RecorderFrameGapCount,
		*GetMediaPublishStatusString(statusSnapshot.LatestMediaPublishStatus));
}

FText UVdjmRecorderDebugStatusWidgetBase::BuildDebugStatusText(const FVdjmRecorderDebugStatusSnapshot& statusSnapshot) const
{
	const FString statusText = FString::Printf(
		TEXT("Recorder Debug Status\n")
		TEXT("Controller: %s\n")
		TEXT("Bridge: %s\n")
		TEXT("Controller Busy: %s\n")
		TEXT("Recording: %s\n")
		TEXT("Finalizing: %s\n")
		TEXT("PostProcess: %s  Jobs: %d\n")
		TEXT("Init Step: %s\n")
		TEXT("Frames: %d  FPS: %.1f\n")
		TEXT("Game Frame: %.2f ms  FPS: %.1f  Avg: %.1f  MaxMs: %.2f\n")
		TEXT("Frame Spike: %d recent=%s  Recorder Gap: %d recent=%s\n")
		TEXT("Publish: %s\n")
		TEXT("Output: %s\n")
		TEXT("Metadata: %s\n")
		TEXT("ContentUri: %s\n")
		TEXT("Error: %s"),
		*GetBoolStatusString(statusSnapshot.bHasController),
		*GetBoolStatusString(statusSnapshot.bHasBridgeActor),
		*GetBoolStatusString(statusSnapshot.bIsControllerBusy),
		*GetBoolStatusString(statusSnapshot.bIsRecording),
		*GetBoolStatusString(statusSnapshot.bIsFinalizingRecording),
		*GetBoolStatusString(statusSnapshot.bIsPostProcessingMedia),
		statusSnapshot.ActiveMediaPublishJobCount,
		statusSnapshot.CurrentInitStepName.IsEmpty() ? TEXT("None") : *statusSnapshot.CurrentInitStepName,
		statusSnapshot.RecordedFrameCount,
		statusSnapshot.EstimatedFrameRate,
		statusSnapshot.LastGameFrameMs,
		statusSnapshot.LastGameFrameRate,
		statusSnapshot.AverageGameFrameRate,
		statusSnapshot.MaxGameFrameMs,
		statusSnapshot.GameFrameSpikeCount,
		*GetBoolStatusString(statusSnapshot.bHasRecentGameFrameSpike),
		statusSnapshot.RecorderFrameGapCount,
		*GetBoolStatusString(statusSnapshot.bHasRecentRecorderFrameGap),
		*GetMediaPublishStatusString(statusSnapshot.LatestMediaPublishStatus),
		statusSnapshot.LatestOutputFilePath.IsEmpty() ? TEXT("None") : *statusSnapshot.LatestOutputFilePath,
		statusSnapshot.LatestMetadataFilePath.IsEmpty() ? TEXT("None") : *statusSnapshot.LatestMetadataFilePath,
		statusSnapshot.LatestPublishedContentUri.IsEmpty() ? TEXT("None") : *statusSnapshot.LatestPublishedContentUri,
		statusSnapshot.LatestMediaPublishErrorReason.IsEmpty()
			? (statusSnapshot.LatestArtifactValidationError.IsEmpty() ? TEXT("None") : *statusSnapshot.LatestArtifactValidationError)
			: *statusSnapshot.LatestMediaPublishErrorReason);

	return FText::FromString(statusText);
}

float UVdjmRecorderDebugStatusWidgetBase::CalculateEstimatedFrameRate(int32 recordedFrameCount, bool bIsRecording)
{
	const double now = FPlatformTime::Seconds();
	if (not bIsRecording)
	{
		mLastFrameSampleTimeSeconds = now;
		mLastFrameSampleCount = recordedFrameCount;
		mLastEstimatedFrameRate = 0.0f;
		return mLastEstimatedFrameRate;
	}

	if (mLastFrameSampleTimeSeconds <= 0.0)
	{
		mLastFrameSampleTimeSeconds = now;
		mLastFrameSampleCount = recordedFrameCount;
		return mLastEstimatedFrameRate;
	}

	const double elapsedSeconds = now - mLastFrameSampleTimeSeconds;
	if (elapsedSeconds < KINDA_SMALL_NUMBER)
	{
		return mLastEstimatedFrameRate;
	}

	const int32 frameDelta = FMath::Max(0, recordedFrameCount - mLastFrameSampleCount);
	mLastEstimatedFrameRate = static_cast<float>(static_cast<double>(frameDelta) / elapsedSeconds);
	mLastFrameSampleTimeSeconds = now;
	mLastFrameSampleCount = recordedFrameCount;
	return mLastEstimatedFrameRate;
}

void UVdjmRecorderDebugStatusWidgetBase::AddFrameGraphSample(float inDeltaTime)
{
	if (not bCollectFrameGraphSamples)
	{
		return;
	}

	const float safeDeltaSeconds = FMath::Max(inDeltaTime, KINDA_SMALL_NUMBER);
	const double now = FPlatformTime::Seconds();
	if (mFrameGraphStartTimeSeconds <= 0.0)
	{
		mFrameGraphStartTimeSeconds = now;
		mLastRecorderFrameAdvanceTimeSeconds = now;
	}

	AVdjmRecordBridgeActor* bridgeActor = GetDebugBridgeActor();
	const bool bIsRecording = bridgeActor != nullptr && bridgeActor->IsRecording();
	const int32 recordedFrameCount = bridgeActor != nullptr ? bridgeActor->GetRecordedFrameCount() : 0;

	if (mFrameGraphSamples.IsEmpty())
	{
		mLastGraphRecordedFrameCount = recordedFrameCount;
		mLastRecorderFrameAdvanceTimeSeconds = now;
	}

	const int32 recordedFrameDelta = FMath::Max(0, recordedFrameCount - mLastGraphRecordedFrameCount);
	if (recordedFrameDelta > 0 || not bIsRecording)
	{
		mLastRecorderFrameAdvanceTimeSeconds = now;
	}

	const float gameFrameMs = safeDeltaSeconds * 1000.0f;
	const bool bGameFrameSpike = gameFrameMs >= SpikeThresholdFrameMs;
	const bool bRecorderFrameGap = bIsRecording
		&& mLastRecorderFrameAdvanceTimeSeconds > 0.0
		&& (now - mLastRecorderFrameAdvanceTimeSeconds) >= RecorderFrameGapThresholdSeconds;

	if (bGameFrameSpike)
	{
		++mGameFrameSpikeCount;
	}

	if (bRecorderFrameGap && not mbLastRecorderFrameGap)
	{
		++mRecorderFrameGapCount;
	}
	mbLastRecorderFrameGap = bRecorderFrameGap;

	FVdjmRecorderDebugFrameSample sample;
	sample.SampleTimeSeconds = static_cast<float>(now - mFrameGraphStartTimeSeconds);
	sample.GameDeltaSeconds = safeDeltaSeconds;
	sample.GameFrameRate = 1.0f / safeDeltaSeconds;
	sample.RecorderFrameRate = static_cast<float>(recordedFrameDelta) / safeDeltaSeconds;
	sample.RecordedFrameCount = recordedFrameCount;
	sample.RecordedFrameDelta = recordedFrameDelta;
	sample.bGameFrameSpike = bGameFrameSpike;
	sample.bRecorderFrameGap = bRecorderFrameGap;
	mFrameGraphSamples.Add(sample);

	const int32 maxSampleCount = FMath::Max(1, MaxFrameGraphSampleCount);
	if (mFrameGraphSamples.Num() > maxSampleCount)
	{
		mFrameGraphSamples.RemoveAt(0, mFrameGraphSamples.Num() - maxSampleCount, EAllowShrinking::No);
	}

	mLastGraphRecordedFrameCount = recordedFrameCount;
}

void UVdjmRecorderDebugStatusWidgetBase::ApplyFrameGraphStats(FVdjmRecorderDebugStatusSnapshot& statusSnapshot) const
{
	const int32 sampleCount = mFrameGraphSamples.Num();
	statusSnapshot.GameFrameSpikeCount = mGameFrameSpikeCount;
	statusSnapshot.RecorderFrameGapCount = mRecorderFrameGapCount;
	if (sampleCount <= 0)
	{
		return;
	}

	const FVdjmRecorderDebugFrameSample& lastSample = mFrameGraphSamples.Last();
	statusSnapshot.LastGameFrameMs = lastSample.GameDeltaSeconds * 1000.0f;
	statusSnapshot.LastGameFrameRate = lastSample.GameFrameRate;
	statusSnapshot.bHasRecentGameFrameSpike = lastSample.bGameFrameSpike;
	statusSnapshot.bHasRecentRecorderFrameGap = lastSample.bRecorderFrameGap;

	float gameFrameRateSum = 0.0f;
	float maxGameFrameMs = 0.0f;
	const float recentWindowStartSeconds = lastSample.SampleTimeSeconds - 1.0f;
	for (const FVdjmRecorderDebugFrameSample& sample : mFrameGraphSamples)
	{
		gameFrameRateSum += sample.GameFrameRate;
		maxGameFrameMs = FMath::Max(maxGameFrameMs, sample.GameDeltaSeconds * 1000.0f);
		if (sample.SampleTimeSeconds >= recentWindowStartSeconds)
		{
			statusSnapshot.bHasRecentGameFrameSpike = statusSnapshot.bHasRecentGameFrameSpike || sample.bGameFrameSpike;
			statusSnapshot.bHasRecentRecorderFrameGap = statusSnapshot.bHasRecentRecorderFrameGap || sample.bRecorderFrameGap;
		}
	}

	statusSnapshot.AverageGameFrameRate = gameFrameRateSum / static_cast<float>(sampleCount);
	statusSnapshot.MaxGameFrameMs = maxGameFrameMs;
}

int32 UVdjmRecorderDebugStatusWidgetBase::PaintFrameGraph(
	const FGeometry& allottedGeometry,
	FSlateWindowElementList& outDrawElements,
	int32 layerId) const
{
	if (not bDrawFrameGraph || mFrameGraphSamples.Num() < 2)
	{
		return layerId;
	}

	const FVector2D localSize = allottedGeometry.GetLocalSize();
	const float graphPadding = FMath::Max(0.0f, FrameGraphPadding);
	const float graphHeight = FMath::Min(FrameGraphHeight, FMath::Max(0.0f, localSize.Y - graphPadding * 2.0f));
	const float graphWidth = FMath::Max(0.0f, localSize.X - graphPadding * 2.0f);
	if (graphWidth <= 1.0f || graphHeight <= 1.0f)
	{
		return layerId;
	}

	const FVector2D graphOrigin(graphPadding, FMath::Max(graphPadding, localSize.Y - graphHeight - graphPadding));
	const float maxFrameMs = FMath::Max(1.0f, FrameGraphMaxFrameMs);
	const int32 sampleCount = FMath::Min(mFrameGraphSamples.Num(), FMath::Max(2, MaxFrameGraphSampleCount));
	const int32 firstSampleIndex = mFrameGraphSamples.Num() - sampleCount;
	const float xStep = graphWidth / static_cast<float>(sampleCount - 1);

	auto makeGraphPoint = [&](int32 sampleOffset, float frameMs)
	{
		const float normalizedFrameMs = FMath::Clamp(frameMs / maxFrameMs, 0.0f, 1.0f);
		return FVector2D(
			graphOrigin.X + xStep * static_cast<float>(sampleOffset),
			graphOrigin.Y + graphHeight * (1.0f - normalizedFrameMs));
	};

	TArray<FVector2D> borderPoints;
	borderPoints.Reserve(5);
	borderPoints.Add(graphOrigin);
	borderPoints.Add(FVector2D(graphOrigin.X + graphWidth, graphOrigin.Y));
	borderPoints.Add(FVector2D(graphOrigin.X + graphWidth, graphOrigin.Y + graphHeight));
	borderPoints.Add(FVector2D(graphOrigin.X, graphOrigin.Y + graphHeight));
	borderPoints.Add(graphOrigin);
	FSlateDrawElement::MakeLines(
		outDrawElements,
		layerId + 1,
		allottedGeometry.ToPaintGeometry(),
		borderPoints,
		ESlateDrawEffect::None,
		FrameGraphBorderColor,
		true,
		1.0f);

	const float targetFrameMs = 1000.0f / FMath::Max(1.0f, TargetGameFrameRate);
	const float targetY = makeGraphPoint(0, targetFrameMs).Y;
	TArray<FVector2D> targetLinePoints;
	targetLinePoints.Reserve(2);
	targetLinePoints.Add(FVector2D(graphOrigin.X, targetY));
	targetLinePoints.Add(FVector2D(graphOrigin.X + graphWidth, targetY));
	FSlateDrawElement::MakeLines(
		outDrawElements,
		layerId + 2,
		allottedGeometry.ToPaintGeometry(),
		targetLinePoints,
		ESlateDrawEffect::None,
		FrameGraphTargetLineColor,
		true,
		1.0f);

	TArray<FVector2D> frameLinePoints;
	frameLinePoints.Reserve(sampleCount);
	for (int32 sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
	{
		const FVdjmRecorderDebugFrameSample& sample = mFrameGraphSamples[firstSampleIndex + sampleIndex];
		frameLinePoints.Add(makeGraphPoint(sampleIndex, sample.GameDeltaSeconds * 1000.0f));

		if (sample.bGameFrameSpike || sample.bRecorderFrameGap)
		{
			const float spikeX = graphOrigin.X + xStep * static_cast<float>(sampleIndex);
			TArray<FVector2D> spikeLinePoints;
			spikeLinePoints.Reserve(2);
			spikeLinePoints.Add(FVector2D(spikeX, graphOrigin.Y));
			spikeLinePoints.Add(FVector2D(spikeX, graphOrigin.Y + graphHeight));
			FSlateDrawElement::MakeLines(
				outDrawElements,
				layerId + 3,
				allottedGeometry.ToPaintGeometry(),
				spikeLinePoints,
				ESlateDrawEffect::None,
				FrameGraphSpikeColor,
				true,
				1.5f);
		}
	}

	FSlateDrawElement::MakeLines(
		outDrawElements,
		layerId + 4,
		allottedGeometry.ToPaintGeometry(),
		frameLinePoints,
		ESlateDrawEffect::None,
		FrameGraphLineColor,
		true,
		2.0f);

	return layerId + 4;
}
