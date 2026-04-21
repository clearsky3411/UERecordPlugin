#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecorderStateObserver.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordResource;

UENUM(BlueprintType)
enum class EVdjmRecorderObservedState : uint8
{
	ENew UMETA(DisplayName = "New"),
	EReady UMETA(DisplayName = "Ready"),
	ERunning UMETA(DisplayName = "Running"),
	EWaiting UMETA(DisplayName = "Waiting"),
	ETerminated UMETA(DisplayName = "Terminated"),
	EError UMETA(DisplayName = "Error")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FVdjmRecorderObservedStateChangedEvent,
	EVdjmRecorderObservedState,
	PreviousState,
	EVdjmRecorderObservedState,
	CurrentState,
	EVdjmRecordBridgeInitStep,
	LastInitStep,
	double,
	TransitionSeconds);

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecorderStateObserver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|StateObserver", meta = (WorldContext = "WorldContextObject"))
	static UVdjmRecorderStateObserver* CreateObserver(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|StateObserver")
	bool InitializeObserver(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|StateObserver")
	bool BindBridge(AVdjmRecordBridgeActor* InBridgeActor);

	UFUNCTION(BlueprintCallable, Category = "Recorder|StateObserver")
	void UnbindBridge();

	UFUNCTION(BlueprintPure, Category = "Recorder|StateObserver")
	EVdjmRecorderObservedState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Recorder|StateObserver")
	EVdjmRecordBridgeInitStep GetLastInitStep() const { return LastInitStep; }

	UFUNCTION(BlueprintPure, Category = "Recorder|StateObserver")
	double GetLastTransitionSeconds() const { return LastTransitionSeconds; }

	UPROPERTY(BlueprintAssignable, Category = "Recorder|StateObserver")
	FVdjmRecorderObservedStateChangedEvent OnObservedStateChanged;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

protected:
	virtual UWorld* GetWorld() const override;

private:
	UFUNCTION()
	void HandleInitComplete(AVdjmRecordBridgeActor* InBridgeActor);
	UFUNCTION()
	void HandleInitErrorEnd(AVdjmRecordBridgeActor* InBridgeActor);
	UFUNCTION()
	void HandleRecordStarted(UVdjmRecordResource* InRecordResource);
	UFUNCTION()
	void HandleRecordStopped(UVdjmRecordResource* InRecordResource);
	UFUNCTION()
	void HandleChainInitChanged(AVdjmRecordBridgeActor* InBridgeActor, EVdjmRecordBridgeInitStep PrevStep, EVdjmRecordBridgeInitStep CurrentStep);

	void TransitionTo(EVdjmRecorderObservedState NewState);
	void ApplyStateByBridgeSnapshot();

	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<AVdjmRecordBridgeActor> WeakBridgeActor;

	EVdjmRecorderObservedState CurrentState = EVdjmRecorderObservedState::ENew;
	EVdjmRecordBridgeInitStep LastInitStep = EVdjmRecordBridgeInitStep::EInitializeStart;
	double LastTransitionSeconds = 0.0;
};
