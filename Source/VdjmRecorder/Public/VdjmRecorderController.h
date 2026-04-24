#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecorderController.generated.h"

class AVdjmRecordBridgeActor;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	FVdjmRecorderOptionQualityTierMessage QualityTier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	FVdjmRecorderOptionStringMessage FileName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	FVdjmRecorderOptionInt32Message FrameRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	FVdjmRecorderOptionInt32Message Bitrate;

	bool HasAnyMessage() const
	{
		return QualityTier.HasAction()
			|| FileName.HasAction()
			|| FrameRate.HasAction()
			|| Bitrate.HasAction();
	}

	void MergeFrom(const FVdjmRecorderOptionRequest& Other)
	{
		SubmitPolicy = Other.SubmitPolicy;
		QualityTier.MergeFrom(Other.QualityTier);
		FileName.MergeFrom(Other.FileName);
		FrameRate.MergeFrom(Other.FrameRate);
		Bitrate.MergeFrom(Other.Bitrate);
	}

	void Reset()
	{
		SubmitPolicy = EVdjmRecorderOptionSubmitPolicy::EProcessIfSafe;
		QualityTier.Reset();
		FileName.Reset();
		FrameRate.Reset();
		Bitrate.Reset();
	}
};

struct FVdjmRecorderOptionHistoryEntry
{
public:
	FVdjmRecorderOptionRequest ForwardRequest;
	FVdjmRecorderOptionRequest ReverseRequest;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecorderController : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller", meta = (WorldContext = "WorldContextObject"))
	static UVdjmRecorderController* CreateRecorderController(UObject* WorldContextObject);

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
	bool ValidateRequest(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason) const;

	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<AVdjmRecordBridgeActor> WeakBridgeActor;
	TWeakObjectPtr<UVdjmRecordEnvDataAsset> WeakDataAsset;
	UPROPERTY()
	TObjectPtr<UVdjmRecordEventManager> EventManager;
	UPROPERTY()
	TObjectPtr<UVdjmRecorderStateObserver> StateObserver;
	FVdjmRecorderOptionRequest PendingOptionRequest;
	TArray<FVdjmRecorderOptionHistoryEntry> UndoHistory;
	TArray<FVdjmRecorderOptionHistoryEntry> RedoHistory;
	int32 MaxUndoHistoryDepth = 16;
	bool bHasPendingOptionRequest = false;
};
