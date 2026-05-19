#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VdjmRuntimeErrorCacheSubsystem.generated.h"

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRuntimeErrorLogPayload
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	bool bHasCachedError = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	FGuid ErrorId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	FDateTime TimestampUtc;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	FName SourceTag = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	FName OperationTag = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	FString Message;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	FString Details;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	FString SourceObjectName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	FString SourceObjectClassName;
};

/**
 * Stores the latest runtime error payload for UI that decides how to present it.
 *
 * This is intentionally not a UE_LOG collector, not a history list, and not a modal dispatcher.
 * Error producers push one payload here, then emit whatever flow signal they need themselves.
 */
UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRuntimeErrorCacheSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UVdjmRuntimeErrorCacheSubsystem* Get(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|RuntimeError")
	bool PushRuntimeErrorLog(
		FName sourceTag,
		FName operationTag,
		const FString& message,
		const FString& details,
		UObject* sourceObject);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|RuntimeError")
	FVdjmRuntimeErrorLogPayload CachedRuntimeErrorLog;
};

UCLASS()
class VDJMRECORDER_API UVdjmRuntimeErrorCacheBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|RuntimeError", meta = (WorldContext = "worldContextObject", AdvancedDisplay = "details,sourceObject"))
	static bool PushRuntimeErrorLog(
		UObject* worldContextObject,
		FName sourceTag,
		FName operationTag,
		const FString& message,
		const FString& details,
		UObject* sourceObject);
};
