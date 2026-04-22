#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmRecordEventNode.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordEventManager;

UENUM(BlueprintType)
enum class EVdjmRecordEventResultType : uint8
{
	ESuccess,
	EFailure,
	ERunning,
	EAbort,
	ESelectIndex,
	EJumpToLabel,
};

USTRUCT(BlueprintType)
struct FVdjmRecordEventResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	EVdjmRecordEventResultType ResultType = EVdjmRecordEventResultType::ESuccess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow", meta = (ClampMin = "0"))
	int32 SelectedIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	FName JumpLabel = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow", meta = (ClampMin = "0.0"))
	float WaitSeconds = 0.0f;
};

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMRECORDER_API UVdjmRecordEventBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Recorder|EventFlow")
	FVdjmRecordEventResult ExecuteEvent(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor);
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Recorder|EventFlow")
	void ResetRuntimeState();
	virtual void ResetRuntimeState_Implementation();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	static FVdjmRecordEventResult MakeResult(EVdjmRecordEventResultType InResultType, int32 InSelectedIndex, FName InJumpLabel, float InWaitSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow")
	FName EventTag = NAME_None;
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMRECORDER_API UVdjmRecordEventSequenceNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Recorder|EventFlow")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Children;

private:
	UPROPERTY(Transient)
	int32 RuntimeChildIndex = 0;
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMRECORDER_API UVdjmRecordEventSelectorNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow")
	TSubclassOf<UVdjmRecordEventBase> TargetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow")
	FName TargetTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow")
	bool bAbortIfNotFound = false;
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMRECORDER_API UVdjmRecordEventSpawnBridgeActorNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow")
	bool bReuseExistingBridgeActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow")
	TSubclassOf<AVdjmRecordBridgeActor> BridgeActorClass;
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMRECORDER_API UVdjmRecordEventSetEnvDataAssetPathNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow")
	FSoftObjectPath EnvDataAssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow")
	bool bRequireLoadSuccess = false;
};
