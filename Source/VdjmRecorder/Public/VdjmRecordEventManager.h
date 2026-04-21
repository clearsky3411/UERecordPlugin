#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecordEventManager.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordResource;

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
