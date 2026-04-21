#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmRecordEventFlowRuntime.generated.h"

class UVdjmRecordEventBase;
class UVdjmRecordEventFlowDataAsset;

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventFlowRuntime : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "Outer"))
	static UVdjmRecordEventFlowRuntime* CreateFlowRuntimeFromAsset(
		UObject* Outer,
		const UVdjmRecordEventFlowDataAsset* SourceFlowAsset,
		FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "Outer"))
	static UVdjmRecordEventFlowRuntime* CreateFlowRuntimeFromJsonString(
		UObject* Outer,
		const FString& InJsonString,
		FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool InitializeFromAsset(const UVdjmRecordEventFlowDataAsset* SourceFlowAsset, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool InitializeFromJsonString(const FString& InJsonString, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool ImportFlowFromJsonString(const FString& InJsonString, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	FString ExportFlowToJsonString(bool bPrettyPrint = true) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	int32 FindEventIndexByTag(FName InTag) const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	void ResetRuntimeStates();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	UVdjmRecordEventFlowDataAsset* GetSourceFlowAsset() const;

	UPROPERTY(Transient, Instanced, BlueprintReadOnly, Category = "Recorder|EventFlow")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Events;

private:
	TWeakObjectPtr<UVdjmRecordEventFlowDataAsset> SourceFlowAsset;
};
