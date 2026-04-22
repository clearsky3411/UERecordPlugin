#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecorderController.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordEnvDataAsset;
class UVdjmRecordEventManager;
class UVdjmRecorderStateObserver;

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderOptionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	bool bOverrideQualityTier = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	EVdjmRecordQualityTiers QualityTier = EVdjmRecordQualityTiers::EDefault;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	bool bOverrideFileName = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|Option")
	FString FileName;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecorderController : public UObject
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
	bool StartRecording();

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller")
	void StopRecording();

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller")
	AVdjmRecordBridgeActor* GetBridgeActor() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller")
	UVdjmRecordEnvDataAsset* GetResolvedDataAsset() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller")
	UVdjmRecorderStateObserver* GetStateObserver() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller")
	UVdjmRecordEventManager* GetEventManager() const;

protected:
	virtual UWorld* GetWorld() const override;

private:
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
};
