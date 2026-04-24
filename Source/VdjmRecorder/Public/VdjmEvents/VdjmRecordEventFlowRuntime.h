#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmEvents/VdjmRecordEventFlowFragment.h"
#include "VdjmRecordEventFlowRuntime.generated.h"

class UVdjmRecordEventBase;
class UVdjmRecordEventFlowDataAsset;

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventFlowRuntime : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "Outer"))
	static UVdjmRecordEventFlowRuntime* CreateEmptyFlowRuntime(UObject* Outer);

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
	bool InitializeEmpty();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool InitializeFromAsset(const UVdjmRecordEventFlowDataAsset* SourceFlowAsset, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool InitializeFromJsonString(const FString& InJsonString, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool ImportFlowFromJsonString(const FString& InJsonString, FString& OutError);

	bool AppendFlowFragment(const FVdjmRecordEventFlowFragment& InFragment, FString& OutError);

	bool InsertFlowFragment(int32 InsertIndex, const FVdjmRecordEventFlowFragment& InFragment, FString& OutError);

	bool ReplaceEventByTagFromFragment(FName InTag, const FVdjmRecordEventNodeFragment& InFragment, FString& OutError);

	bool RemoveEventAt(int32 EventIndex);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	FString ExportFlowToJsonString(bool bPrettyPrint = true) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	int32 FindEventIndexByTag(FName InTag) const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	void ResetRuntimeStates();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	UVdjmRecordEventFlowDataAsset* GetSourceFlowAsset() const;

	UPROPERTY(Transient, Instanced, BlueprintReadOnly, Category = "Recorder|EventFlowRuntime")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Events;

private:
	bool BuildEventNodeFromFragment(const FVdjmRecordEventNodeFragment& InFragment, UVdjmRecordEventBase*& OutEventNode, FString& OutError);
	bool BuildEventNodesFromFragment(const FVdjmRecordEventFlowFragment& InFragment, TArray<TObjectPtr<UVdjmRecordEventBase>>& OutEvents, FString& OutError);

	TWeakObjectPtr<UVdjmRecordEventFlowDataAsset> SourceFlowAsset;
};
