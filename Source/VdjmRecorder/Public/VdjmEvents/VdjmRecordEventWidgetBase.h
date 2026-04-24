#pragma once

#include "Blueprint/UserWidget.h"
#include "VdjmRecordEventWidgetBase.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordEventFlowDataAsset;
class UVdjmRecordEventManager;
class UVdjmRecorderWorldContextSubsystem;

UCLASS(Abstract, BlueprintType, Blueprintable)
class VDJMRECORDER_API UVdjmRecordEventWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Widget")
	void ApplyEventContext(UVdjmRecordEventManager* InEventManager, AVdjmRecordBridgeActor* InBridgeActor);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Widget")
	UVdjmRecordEventManager* GetLinkedEventManager() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Widget")
	AVdjmRecordBridgeActor* GetLinkedBridgeActor() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Widget")
	UVdjmRecorderWorldContextSubsystem* GetWorldContextSubsystem() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Widget")
	bool EmitLinkedFlowSignal(FName signalTag) const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Widget")
	bool RequestPauseLinkedEventFlow() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Widget")
	bool RequestResumeLinkedEventFlow() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Widget")
	bool StopLinkedEventFlow() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Widget")
	bool RequestStopLinkedEventFlow() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Widget")
	bool RequestAbortLinkedEventFlow() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Widget")
	bool RequestFailLinkedEventFlow() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Widget")
	bool StartLinkedEventFlow(UVdjmRecordEventFlowDataAsset* flowDataAsset, bool bResetRuntimeStates = true) const;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Recorder|EventFlow|Widget")
	void HandleEventContextApplied(UVdjmRecordEventManager* InEventManager, AVdjmRecordBridgeActor* InBridgeActor);
	virtual void HandleEventContextApplied_Implementation(UVdjmRecordEventManager* InEventManager, AVdjmRecordBridgeActor* InBridgeActor);

	TWeakObjectPtr<UVdjmRecordEventManager> LinkedEventManager;
	TWeakObjectPtr<AVdjmRecordBridgeActor> LinkedBridgeActor;
};
