#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmEvents/VdjmRecorderSessionTypes.h"
#include "VdjmRecorderStateObserver.generated.h"

class UVdjmRecordEventManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FVdjmRecorderObservedStateChangedEvent,
	EVdjmRecorderSessionState,
	PreviousState,
	EVdjmRecorderSessionState,
	CurrentState,
	double,
	TransitionSeconds);

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecorderStateObserver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|StateObserver", meta = (WorldContext = "WorldContextObject"))
	static UVdjmRecorderStateObserver* CreateObserver(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|StateObserver")
	bool InitializeObserver(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|StateObserver")
	bool BindEventManager(UVdjmRecordEventManager* InEventManager);

	UFUNCTION(BlueprintCallable, Category = "Recorder|StateObserver")
	void UnbindEventManager();

	UFUNCTION(BlueprintPure, Category = "Recorder|StateObserver")
	EVdjmRecorderSessionState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Recorder|StateObserver")
	double GetLastTransitionSeconds() const { return LastTransitionSeconds; }

	UFUNCTION(BlueprintPure, Category = "Recorder|StateObserver")
	FString GetLastError() const { return LastError; }

	UPROPERTY(BlueprintAssignable, Category = "Recorder|StateObserver")
	FVdjmRecorderObservedStateChangedEvent OnObservedStateChanged;

private:
	UFUNCTION()
	void HandleManagerSessionStateChanged(
		UVdjmRecordEventManager* InEventManager,
		EVdjmRecorderSessionState PreviousState,
		EVdjmRecorderSessionState NewState,
		double TransitionSeconds);

	TWeakObjectPtr<UVdjmRecordEventManager> WeakEventManager;

	EVdjmRecorderSessionState CurrentState = EVdjmRecorderSessionState::ENew;
	double LastTransitionSeconds = 0.0;
	FString LastError;
};
