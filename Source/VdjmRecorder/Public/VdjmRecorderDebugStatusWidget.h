#pragma once

#include "Blueprint/UserWidget.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecorderController.h"
#include "VdjmRecorderDebugStatusWidget.generated.h"

class AVdjmRecordBridgeActor;
class UTextBlock;
class UVdjmRecordArtifact;
class UVdjmRecorderController;

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderDebugFrameSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus|FrameGraph")
	float SampleTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus|FrameGraph")
	float GameDeltaSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus|FrameGraph")
	float GameFrameRate = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus|FrameGraph")
	float RecorderFrameRate = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus|FrameGraph")
	int32 RecordedFrameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus|FrameGraph")
	int32 RecordedFrameDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus|FrameGraph")
	bool bGameFrameSpike = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus|FrameGraph")
	bool bRecorderFrameGap = false;
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderDebugStatusSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bHasController = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bHasBridgeActor = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bIsRecording = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bIsFinalizingRecording = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bIsPostProcessingMedia = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bIsControllerBusy = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bHasLatestArtifact = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bIsLatestArtifactValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bHasLatestMetadata = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	EVdjmRecordBridgeInitStep CurrentInitStep = EVdjmRecordBridgeInitStep::EInitializeStart;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FString CurrentInitStepName;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	int32 RecordedFrameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	float EstimatedFrameRate = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	float LastGameFrameMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	float LastGameFrameRate = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	float AverageGameFrameRate = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	float MaxGameFrameMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	int32 GameFrameSpikeCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	int32 RecorderFrameGapCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bHasRecentGameFrameSpike = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	bool bHasRecentRecorderFrameGap = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	int32 ActiveMediaPublishJobCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	EVdjmRecordMediaPublishStatus LatestMediaPublishStatus = EVdjmRecordMediaPublishStatus::ENotStarted;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FString LatestOutputFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FString LatestMetadataFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FString LatestPublishedContentUri;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FString LatestMediaPublishErrorReason;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FString LatestArtifactValidationError;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FString ControllerStatusText;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FString StatusLine;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FText StatusText;
};

UCLASS(Abstract, BlueprintType, Blueprintable)
class VDJMRECORDER_API UVdjmRecorderDebugStatusWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|DebugStatus")
	FVdjmRecorderDebugStatusSnapshot RefreshRecorderDebugStatus();

	UFUNCTION(BlueprintPure, Category = "Recorder|DebugStatus")
	FVdjmRecorderDebugStatusSnapshot GetLastRecorderDebugStatusSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|DebugStatus")
	FText GetLastStatusText() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|DebugStatus")
	FString GetLastStatusLine() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|DebugStatus")
	void SetAutoRefreshEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Recorder|DebugStatus")
	void SetRefreshIntervalSeconds(float refreshIntervalSeconds);

	UFUNCTION(BlueprintPure, Category = "Recorder|DebugStatus")
	bool IsAutoRefreshEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|DebugStatus")
	float GetRefreshIntervalSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|DebugStatus")
	UVdjmRecorderController* GetDebugRecorderController() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|DebugStatus")
	AVdjmRecordBridgeActor* GetDebugBridgeActor() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|DebugStatus|FrameGraph")
	TArray<FVdjmRecorderDebugFrameSample> GetFrameGraphSamples() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|DebugStatus|FrameGraph")
	void ResetFrameGraphSamples();

	UFUNCTION(BlueprintCallable, Category = "Recorder|DebugStatus|FrameGraph")
	void SetFrameGraphEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Recorder|DebugStatus|FrameGraph")
	bool IsFrameGraphEnabled() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& myGeometry, float inDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& args,
		const FGeometry& allottedGeometry,
		const FSlateRect& myCullingRect,
		FSlateWindowElementList& outDrawElements,
		int32 layerId,
		const FWidgetStyle& inWidgetStyle,
		bool bParentEnabled) const override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Recorder|DebugStatus", meta = (DisplayName = "On Recorder Debug Status Updated"))
	void BP_OnRecorderDebugStatusUpdated(const FVdjmRecorderDebugStatusSnapshot& statusSnapshot);

	void RestartAutoRefreshTimer();
	void StopAutoRefreshTimer();
	void ApplyStatusTextToBoundWidget(const FVdjmRecorderDebugStatusSnapshot& statusSnapshot);

	UFUNCTION()
	void HandleAutoRefreshTimer();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Recorder|DebugStatus")
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus")
	bool bAutoRefresh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus", meta = (ClampMin = "0.05"))
	float RefreshIntervalSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus")
	bool bCreateControllerIfMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus")
	bool bWriteStatusToTextBlock = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph")
	bool bCollectFrameGraphSamples = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph")
	bool bDrawFrameGraph = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph", meta = (ClampMin = "16", ClampMax = "600"))
	int32 MaxFrameGraphSampleCount = 180;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph", meta = (ClampMin = "1.0"))
	float TargetGameFrameRate = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph", meta = (ClampMin = "1.0"))
	float SpikeThresholdFrameMs = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph", meta = (ClampMin = "0.01"))
	float RecorderFrameGapThresholdSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph", meta = (ClampMin = "16.0"))
	float FrameGraphHeight = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph", meta = (ClampMin = "0.0"))
	float FrameGraphPadding = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph", meta = (ClampMin = "1.0"))
	float FrameGraphMaxFrameMs = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph")
	FLinearColor FrameGraphLineColor = FLinearColor(0.1f, 0.85f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph")
	FLinearColor FrameGraphTargetLineColor = FLinearColor(0.15f, 0.45f, 1.0f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph")
	FLinearColor FrameGraphSpikeColor = FLinearColor(1.0f, 0.12f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|DebugStatus|FrameGraph")
	FLinearColor FrameGraphBorderColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.25f);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Recorder|DebugStatus")
	FVdjmRecorderDebugStatusSnapshot mLastStatusSnapshot;

private:
	FVdjmRecorderDebugStatusSnapshot BuildDebugStatusSnapshot();
	FString BuildDebugStatusLine(const FVdjmRecorderDebugStatusSnapshot& statusSnapshot) const;
	FText BuildDebugStatusText(const FVdjmRecorderDebugStatusSnapshot& statusSnapshot) const;
	float CalculateEstimatedFrameRate(int32 recordedFrameCount, bool bIsRecording);
	void AddFrameGraphSample(float inDeltaTime);
	void ApplyFrameGraphStats(FVdjmRecorderDebugStatusSnapshot& statusSnapshot) const;
	int32 PaintFrameGraph(const FGeometry& allottedGeometry, FSlateWindowElementList& outDrawElements, int32 layerId) const;

	mutable TWeakObjectPtr<UVdjmRecorderController> mCachedController;
	mutable TWeakObjectPtr<AVdjmRecordBridgeActor> mCachedBridgeActor;
	FTimerHandle mRefreshTimerHandle;
	double mLastFrameSampleTimeSeconds = 0.0;
	int32 mLastFrameSampleCount = 0;
	float mLastEstimatedFrameRate = 0.0f;
	TArray<FVdjmRecorderDebugFrameSample> mFrameGraphSamples;
	double mFrameGraphStartTimeSeconds = 0.0;
	double mLastRecorderFrameAdvanceTimeSeconds = 0.0;
	int32 mLastGraphRecordedFrameCount = 0;
	int32 mGameFrameSpikeCount = 0;
	int32 mRecorderFrameGapCount = 0;
	bool mbLastRecorderFrameGap = false;
};
