#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecorderController.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordArtifact;
class UVdjmRecordMetadataStore;
class UVdjmRecordEnvDataAsset;
class UVdjmRecordEventManager;
class UVdjmRecorderStateObserver;

UENUM(BlueprintType)
enum class EVdjmRecorderOptionValueAction : uint8
{
	EIgnore UMETA(DisplayName = "Ignore"),
	ESet UMETA(DisplayName = "Set"),
	EClear UMETA(DisplayName = "Clear")
};

UENUM(BlueprintType)
enum class EVdjmRecorderOptionSubmitPolicy : uint8
{
	EProcessIfSafe UMETA(DisplayName = "Process If Safe"),
	EQueueOnly UMETA(DisplayName = "Queue Only")
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderOptionInt32Message
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	EVdjmRecorderOptionValueAction Action = EVdjmRecorderOptionValueAction::EIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option", meta = (ClampMin = "0"))
	int32 Value = 0;

	bool HasAction() const
	{
		return Action != EVdjmRecorderOptionValueAction::EIgnore;
	}

	void MergeFrom(const FVdjmRecorderOptionInt32Message& Other)
	{
		if (Other.Action == EVdjmRecorderOptionValueAction::EIgnore)
		{
			return;
		}

		Action = Other.Action;
		Value = Other.Value;
	}

	void Reset()
	{
		Action = EVdjmRecorderOptionValueAction::EIgnore;
		Value = 0;
	}
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderOptionFloatMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	EVdjmRecorderOptionValueAction Action = EVdjmRecorderOptionValueAction::EIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option", meta = (ClampMin = "0.0"))
	float Value = 0.0f;

	bool HasAction() const
	{
		return Action != EVdjmRecorderOptionValueAction::EIgnore;
	}

	void MergeFrom(const FVdjmRecorderOptionFloatMessage& Other)
	{
		if (Other.Action == EVdjmRecorderOptionValueAction::EIgnore)
		{
			return;
		}

		Action = Other.Action;
		Value = Other.Value;
	}

	void Reset()
	{
		Action = EVdjmRecorderOptionValueAction::EIgnore;
		Value = 0.0f;
	}
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderOptionBoolMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	EVdjmRecorderOptionValueAction Action = EVdjmRecorderOptionValueAction::EIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	bool Value = false;

	bool HasAction() const
	{
		return Action != EVdjmRecorderOptionValueAction::EIgnore;
	}

	void MergeFrom(const FVdjmRecorderOptionBoolMessage& Other)
	{
		if (Other.Action == EVdjmRecorderOptionValueAction::EIgnore)
		{
			return;
		}

		Action = Other.Action;
		Value = Other.Value;
	}

	void Reset()
	{
		Action = EVdjmRecorderOptionValueAction::EIgnore;
		Value = false;
	}
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderOptionIntPointMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	EVdjmRecorderOptionValueAction Action = EVdjmRecorderOptionValueAction::EIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	FIntPoint Value = FIntPoint::ZeroValue;

	bool HasAction() const
	{
		return Action != EVdjmRecorderOptionValueAction::EIgnore;
	}

	void MergeFrom(const FVdjmRecorderOptionIntPointMessage& Other)
	{
		if (Other.Action == EVdjmRecorderOptionValueAction::EIgnore)
		{
			return;
		}

		Action = Other.Action;
		Value = Other.Value;
	}

	void Reset()
	{
		Action = EVdjmRecorderOptionValueAction::EIgnore;
		Value = FIntPoint::ZeroValue;
	}
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderOptionStringMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	EVdjmRecorderOptionValueAction Action = EVdjmRecorderOptionValueAction::EIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	FString Value;

	bool HasAction() const
	{
		return Action != EVdjmRecorderOptionValueAction::EIgnore;
	}

	void MergeFrom(const FVdjmRecorderOptionStringMessage& Other)
	{
		if (Other.Action == EVdjmRecorderOptionValueAction::EIgnore)
		{
			return;
		}

		Action = Other.Action;
		Value = Other.Value;
	}

	void Reset()
	{
		Action = EVdjmRecorderOptionValueAction::EIgnore;
		Value.Reset();
	}
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderOptionQualityTierMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	EVdjmRecorderOptionValueAction Action = EVdjmRecorderOptionValueAction::EIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	EVdjmRecordQualityTiers Value = EVdjmRecordQualityTiers::EUndefined;

	bool HasAction() const
	{
		return Action != EVdjmRecorderOptionValueAction::EIgnore;
	}

	void MergeFrom(const FVdjmRecorderOptionQualityTierMessage& Other)
	{
		if (Other.Action == EVdjmRecorderOptionValueAction::EIgnore)
		{
			return;
		}

		Action = Other.Action;
		Value = Other.Value;
	}

	void Reset()
	{
		Action = EVdjmRecorderOptionValueAction::EIgnore;
		Value = EVdjmRecordQualityTiers::EUndefined;
	}
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderOptionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	EVdjmRecorderOptionSubmitPolicy SubmitPolicy = EVdjmRecorderOptionSubmitPolicy::EProcessIfSafe;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option",
		meta = (DisplayName = "Quality Tier", ReflectionUiType = "ComboBox", ReflectionUiSortOrder = "10"))
	FVdjmRecorderOptionQualityTierMessage QualityTier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option",
		meta = (DisplayName = "File Name", ReflectionUiType = "TextBox", ReflectionUiSortOrder = "20"))
	FVdjmRecorderOptionStringMessage FileName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option",
		meta = (DisplayName = "Frame Rate", ReflectionUiType = "Slider", ClampMin = "24", ClampMax = "60", ReflectionUiSortOrder = "30"))
	FVdjmRecorderOptionInt32Message FrameRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option",
		meta = (DisplayName = "Bitrate", ReflectionUiType = "Numeric", ClampMin = "500000", ClampMax = "50000000", ReflectionUiSortOrder = "40"))
	FVdjmRecorderOptionInt32Message Bitrate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option|Video",
		meta = (DisplayName = "Resolution", ReflectionUiType = "IntPoint", ClampMin = "0", ReflectionUiSortOrder = "50"))
	FVdjmRecorderOptionIntPointMessage Resolution;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option|Video",
		meta = (DisplayName = "Fit Resolution To Display", ReflectionUiType = "CheckBox", ReflectionUiSortOrder = "60"))
	FVdjmRecorderOptionBoolMessage ResolutionFitToDisplay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option|Video",
		meta = (DisplayName = "Keyframe Interval", ReflectionUiType = "Numeric", ClampMin = "0", ClampMax = "10", ReflectionUiSortOrder = "70", ReflectionUiAdvanced))
	FVdjmRecorderOptionInt32Message KeyframeInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option|Runtime",
		meta = (DisplayName = "Max Record Duration Seconds", ReflectionUiType = "Numeric", ClampMin = "1", ReflectionUiSortOrder = "80"))
	FVdjmRecorderOptionFloatMessage MaxRecordDurationSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option|Output",
		meta = (DisplayName = "Output File Path", ReflectionUiType = "TextBox", ReflectionUiSortOrder = "90", ReflectionUiAdvanced))
	FVdjmRecorderOptionStringMessage OutputFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option|Output",
		meta = (DisplayName = "Session Id", ReflectionUiType = "TextBox", ReflectionUiSortOrder = "100", ReflectionUiAdvanced))
	FVdjmRecorderOptionStringMessage SessionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option|Output",
		meta = (DisplayName = "Overwrite Existing File", ReflectionUiType = "CheckBox", ReflectionUiSortOrder = "110", ReflectionUiAdvanced))
	FVdjmRecorderOptionBoolMessage OverwriteExists;

	bool HasAnyMessage() const
	{
		return QualityTier.HasAction()
			|| FileName.HasAction()
			|| FrameRate.HasAction()
			|| Bitrate.HasAction()
			|| Resolution.HasAction()
			|| ResolutionFitToDisplay.HasAction()
			|| KeyframeInterval.HasAction()
			|| MaxRecordDurationSeconds.HasAction()
			|| OutputFilePath.HasAction()
			|| SessionId.HasAction()
			|| OverwriteExists.HasAction();
	}

	void MergeFrom(const FVdjmRecorderOptionRequest& Other)
	{
		SubmitPolicy = Other.SubmitPolicy;
		QualityTier.MergeFrom(Other.QualityTier);
		FileName.MergeFrom(Other.FileName);
		FrameRate.MergeFrom(Other.FrameRate);
		Bitrate.MergeFrom(Other.Bitrate);
		Resolution.MergeFrom(Other.Resolution);
		ResolutionFitToDisplay.MergeFrom(Other.ResolutionFitToDisplay);
		KeyframeInterval.MergeFrom(Other.KeyframeInterval);
		MaxRecordDurationSeconds.MergeFrom(Other.MaxRecordDurationSeconds);
		OutputFilePath.MergeFrom(Other.OutputFilePath);
		SessionId.MergeFrom(Other.SessionId);
		OverwriteExists.MergeFrom(Other.OverwriteExists);
	}

	void Reset()
	{
		SubmitPolicy = EVdjmRecorderOptionSubmitPolicy::EProcessIfSafe;
		QualityTier.Reset();
		FileName.Reset();
		FrameRate.Reset();
		Bitrate.Reset();
		Resolution.Reset();
		ResolutionFitToDisplay.Reset();
		KeyframeInterval.Reset();
		MaxRecordDurationSeconds.Reset();
		OutputFilePath.Reset();
		SessionId.Reset();
		OverwriteExists.Reset();
	}
};

struct FVdjmRecorderOptionHistoryEntry
{
public:
	FVdjmRecorderOptionRequest ForwardRequest;
	FVdjmRecorderOptionRequest ReverseRequest;
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderControllerStatusSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bHasBridgeActor = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bHasEventManager = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bHasMetadataStore = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bIsRecording = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bIsFinalizingRecording = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bHasPendingOptionRequest = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bIsPostProcessingMedia = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	int32 ActiveMediaPublishJobCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bHasLatestArtifact = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bIsLatestArtifactValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	bool bHasLatestMetadata = false;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	EVdjmRecordMediaPublishStatus LatestMediaPublishStatus = EVdjmRecordMediaPublishStatus::ENotStarted;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	FString LatestOutputFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	FString LatestMetadataFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	FString LatestPublishedContentUri;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|Controller|Status")
	FString StatusText;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecorderController : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller", meta = (WorldContext = "WorldContextObject"))
	static UVdjmRecorderController* CreateRecorderController(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecorderController* FindRecorderController(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecorderController* FindOrCreateRecorderController(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller")
	bool InitializeController();

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller")
	bool ApplyOptionRequest(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller")
	bool SubmitOptionRequest(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller")
	bool ProcessPendingOptionRequests(FString& OutErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller")
	bool StartRecording();

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller")
	void StopRecording();
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Status")
	FVdjmRecorderControllerStatusSnapshot GetControllerStatusSnapshot() const;
	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller|Status")
	bool ValidateControllerState(FString& outStatusText) const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Status")
	bool IsRecording() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Status")
	bool IsFinalizingRecording() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Status")
	bool IsControllerBusy() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Status")
	bool IsPostProcessingMedia() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Status")
	int32 GetActiveMediaPublishJobCount() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Artifact")
	UVdjmRecordArtifact* GetLatestArtifact() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Metadata")
	UVdjmRecordMetadataStore* GetMetadataStore() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Media")
	EVdjmRecordMediaPublishStatus GetLatestMediaPublishStatus() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Media")
	FString GetLatestPublishedContentUri() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|Controller|Media")
	FString GetLatestMediaPublishErrorReason() const;
	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller|Artifact")
	void ClearLatestArtifact();

	void SetLatestArtifact(UVdjmRecordArtifact* artifact);

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller")
	AVdjmRecordBridgeActor* GetBridgeActor() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller")
	UVdjmRecordEnvDataAsset* GetResolvedDataAsset() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller")
	UVdjmRecorderStateObserver* GetStateObserver() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller")
	UVdjmRecordEventManager* GetEventManager() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller")
	bool HasPendingOptionRequest() const;

protected:
	virtual UWorld* GetWorld() const override;

private:
	bool ApplyOptionRequestToBridge(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason);
	void ResetOptionHistory();
	void StageAppliedRequestForUndo(const FVdjmRecorderOptionRequest& AppliedRequest);
	void ClearPendingOptionRequest();
	bool EnsureEventManager();
	bool EnsureBridge();
	void EnsureStateObserver();
	bool EnsureMetadataStore();
	bool ValidateRequest(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason) const;

	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<AVdjmRecordBridgeActor> WeakBridgeActor;
	TWeakObjectPtr<UVdjmRecordEnvDataAsset> WeakDataAsset;
	UPROPERTY()
	TObjectPtr<UVdjmRecordEventManager> EventManager;
	UPROPERTY()
	TObjectPtr<UVdjmRecorderStateObserver> StateObserver;
	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordArtifact> LatestArtifact;
	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordMetadataStore> MetadataStore;
	FVdjmRecorderOptionRequest PendingOptionRequest;
	TArray<FVdjmRecorderOptionHistoryEntry> UndoHistory;
	TArray<FVdjmRecorderOptionHistoryEntry> RedoHistory;
	int32 MaxUndoHistoryDepth = 16;
	bool bHasPendingOptionRequest = false;
};
