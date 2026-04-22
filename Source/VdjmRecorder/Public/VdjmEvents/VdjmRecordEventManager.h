#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecordEventNode.h"
#include "VdjmRecorderSessionTypes.h"
#include "VdjmRecordEventManager.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordEventBase;
class UVdjmRecordEventFlowDataAsset;
class UVdjmRecordEventFlowRuntime;
class UVdjmRecordResource;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FVdjmRecordManagerChainEvent,
	AVdjmRecordBridgeActor*,
	BridgeActor,
	EVdjmRecordBridgeInitStep,
	PreviousStep,
	EVdjmRecordBridgeInitStep,
	CurrentStep);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FVdjmRecordManagerFlowFinished,
	UVdjmRecordEventManager*,
	EventManager,
	UVdjmRecordEventFlowDataAsset*,
	FlowAsset,
	EVdjmRecordEventResultType,
	FinalResult);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FVdjmRecordManagerSessionStateChanged,
	UVdjmRecordEventManager*,
	EventManager,
	EVdjmRecorderSessionState,
	PreviousState,
	EVdjmRecorderSessionState,
	CurrentState,
	double,
	TransitionSeconds);

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager", meta = (WorldContext = "WorldContextObject"))
	static UVdjmRecordEventManager* CreateEventManager(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool InitializeManager(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool BindBridge(AVdjmRecordBridgeActor* InBridgeActor);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	AVdjmRecordBridgeActor* GetBoundBridge() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartRecordingByManager();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	void StopRecordingByManager();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartEventFlow(UVdjmRecordEventFlowDataAsset* InFlowAsset, bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartEventFlowRuntime(UVdjmRecordEventFlowRuntime* InFlowRuntime, bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartEventFlowFromJsonString(const FString& InJsonString, FString& OutError, bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	void StopEventFlow();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	bool IsEventFlowRunning() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	UVdjmRecordEventFlowDataAsset* GetActiveFlowAsset() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	UVdjmRecordEventFlowRuntime* GetActiveFlowRuntime() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	int32 GetCurrentFlowIndex() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	UWorld* GetManagerWorld() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	EVdjmRecorderSessionState GetSessionState() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	double GetLastSessionTransitionSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	FString GetLastSessionError() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	int32 FindNextEventIndex(const UVdjmRecordEventBase* SourceEvent, TSubclassOf<UVdjmRecordEventBase> TargetClass, FName TargetTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|EventManager")
	FVdjmRecordManagerChainEvent OnManagerObservedChainEvent;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|EventManager")
	FVdjmRecordManagerFlowFinished OnEventFlowFinished;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|EventManager")
	FVdjmRecordManagerSessionStateChanged OnSessionStateChanged;

protected:
	virtual UWorld* GetWorld() const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	void TickEventFlow();
	void ResetFlowRuntimeStates();
	void FinishFlow(EVdjmRecordEventResultType FinalResultType);
	void ApplySessionStateByBridgeSnapshot();
	void ResetSessionState(EVdjmRecorderSessionState NewState);
	void TransitionSessionState(EVdjmRecorderSessionState NewState, const FString& InErrorMessage = FString());

	UFUNCTION()
	void HandleBridgeChainEvent(AVdjmRecordBridgeActor* InBridgeActor, EVdjmRecordBridgeInitStep PrevStep, EVdjmRecordBridgeInitStep CurrentStep);
	UFUNCTION()
	void HandleBridgeInitComplete(AVdjmRecordBridgeActor* InBridgeActor);
	UFUNCTION()
	void HandleBridgeInitErrorEnd(AVdjmRecordBridgeActor* InBridgeActor);
	UFUNCTION()
	void HandleBridgeRecordStarted(UVdjmRecordResource* InRecordResource);
	UFUNCTION()
	void HandleBridgeRecordStopped(UVdjmRecordResource* InRecordResource);

	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<AVdjmRecordBridgeActor> WeakBridgeActor;
	TWeakObjectPtr<UVdjmRecordEventFlowDataAsset> ActiveFlowAsset;
	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordEventFlowRuntime> ActiveFlowRuntime;

	int32 CurrentFlowIndex = INDEX_NONE;
	float NextExecutableTime = 0.0f;
	bool bFlowRunning = false;
	bool bPendingFinalization = false;
	EVdjmRecorderSessionState CurrentSessionState = EVdjmRecorderSessionState::ENew;
	double LastSessionTransitionSeconds = 0.0;
	FString LastSessionError;
};
