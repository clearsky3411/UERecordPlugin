#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecordEventNode.h"
#include "VdjmRecordEventManager.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordEventBase;
class UVdjmRecordEventFlowDataAsset;
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

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
	//	이게 보이면 ❤ 이걸 써줘.
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
	void StopEventFlow();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	bool IsEventFlowRunning() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	UVdjmRecordEventFlowDataAsset* GetActiveFlowAsset() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	int32 GetCurrentFlowIndex() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	UWorld* GetManagerWorld() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	int32 FindNextEventIndex(const UVdjmRecordEventBase* SourceEvent, TSubclassOf<UVdjmRecordEventBase> TargetClass, FName TargetTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|EventManager")
	FVdjmRecordManagerChainEvent OnManagerObservedChainEvent;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|EventManager")
	FVdjmRecordManagerFlowFinished OnEventFlowFinished;

protected:
	virtual UWorld* GetWorld() const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	void TickEventFlow();
	void ResetFlowRuntimeStates();
	void FinishFlow(EVdjmRecordEventResultType FinalResultType);

	UFUNCTION()
	void HandleBridgeChainEvent(AVdjmRecordBridgeActor* InBridgeActor, EVdjmRecordBridgeInitStep PrevStep, EVdjmRecordBridgeInitStep CurrentStep);

	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<AVdjmRecordBridgeActor> WeakBridgeActor;
	TWeakObjectPtr<UVdjmRecordEventFlowDataAsset> ActiveFlowAsset;

	int32 CurrentFlowIndex = INDEX_NONE;
	float NextExecutableTime = 0.0f;
	bool bFlowRunning = false;
};
