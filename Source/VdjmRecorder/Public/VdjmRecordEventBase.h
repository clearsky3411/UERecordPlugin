#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmRecordEventBase.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordEventManager;

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMRECORDER_API UVdjmRecordEventBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Recorder|EventNode")
	bool ExecuteEvent(UVdjmRecordEventManager* EventManager, FString& OutErrorReason);
	virtual bool ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, FString& OutErrorReason);
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class VDJMRECORDER_API UVdjmRecordEventSpawnBridgeActor : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventNode")
	TSoftClassPtr<AVdjmRecordBridgeActor> BridgeActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventNode")
	bool bReuseExistingBridgeIfFound = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventNode")
	bool bBindSpawnedBridgeToManager = true;

	virtual bool ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, FString& OutErrorReason) override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class VDJMRECORDER_API UVdjmRecordEventSetEnvDataAssetPath : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventNode")
	FSoftObjectPath RecordEnvDataAssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventNode")
	bool bResetToDefaultWhenPathIsInvalid = false;

	virtual bool ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, FString& OutErrorReason) override;
};
