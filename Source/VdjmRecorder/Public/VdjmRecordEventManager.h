#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecordEventManager.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordResource;
class UVdjmRecordEventBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FVdjmRecordManagerChainEvent,
	AVdjmRecordBridgeActor*,
	BridgeActor,
	EVdjmRecordBridgeInitStep,
	PreviousStep,
	EVdjmRecordBridgeInitStep,
	CurrentStep);

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventManager : public UObject
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
	bool ExecuteEventNode(UVdjmRecordEventBase* EventNode, FString& OutErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool ExecuteManagedEventNodes(FString& OutErrorReason, bool bStopOnFirstFailure = true);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	UWorld* GetManagerWorld() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Recorder|EventManager")
	TArray<TObjectPtr<UVdjmRecordEventBase>> ManagedEventNodes;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|EventManager")
	FVdjmRecordManagerChainEvent OnManagerObservedChainEvent;

protected:
	virtual UWorld* GetWorld() const override;

private:
	UFUNCTION()
	void HandleBridgeChainEvent(AVdjmRecordBridgeActor* InBridgeActor, EVdjmRecordBridgeInitStep PrevStep, EVdjmRecordBridgeInitStep CurrentStep);

	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<AVdjmRecordBridgeActor> WeakBridgeActor;
};
