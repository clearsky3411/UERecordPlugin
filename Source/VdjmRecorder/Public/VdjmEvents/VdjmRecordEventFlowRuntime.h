#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"
#include "VdjmEvents/VdjmRecordEventFlowFragment.h"
#include "VdjmRecordEventFlowRuntime.generated.h"

class UVdjmRecordEventBase;
class UVdjmRecordEventFlowDataAsset;

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordEventNodeManifestEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	int32 EventIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	FName EventTag = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	FString EventClassName;
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordEventSignalManifestEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	FName SignalTag = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	int32 EventIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	FName EventTag = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	FString EventClassName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	bool bWaiter = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	bool bEmitter = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	bool bBridgeStartSignal = false;
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordEventFlowManifest
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	TArray<FVdjmRecordEventNodeManifestEntry> EventEntries;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowManifest")
	TArray<FVdjmRecordEventSignalManifestEntry> SignalEntries;

	void Reset()
	{
		EventEntries.Reset();
		SignalEntries.Reset();
	}

	void AddEventNode(int32 eventIndex, FName eventTag, const FString& eventClassName)
	{
		FVdjmRecordEventNodeManifestEntry& entry = EventEntries.AddDefaulted_GetRef();
		entry.EventIndex = eventIndex;
		entry.EventTag = eventTag;
		entry.EventClassName = eventClassName;
	}

	void AddSignalWaiter(
		FName signalTag,
		int32 eventIndex,
		FName eventTag,
		const FString& eventClassName,
		bool bBridgeStartSignal = false)
	{
		if (signalTag.IsNone())
		{
			return;
		}

		FVdjmRecordEventSignalManifestEntry& entry = SignalEntries.AddDefaulted_GetRef();
		entry.SignalTag = signalTag;
		entry.EventIndex = eventIndex;
		entry.EventTag = eventTag;
		entry.EventClassName = eventClassName;
		entry.bWaiter = true;
		entry.bBridgeStartSignal = bBridgeStartSignal;
	}

	void AddSignalEmitter(FName signalTag, int32 eventIndex, FName eventTag, const FString& eventClassName)
	{
		if (signalTag.IsNone())
		{
			return;
		}

		FVdjmRecordEventSignalManifestEntry& entry = SignalEntries.AddDefaulted_GetRef();
		entry.SignalTag = signalTag;
		entry.EventIndex = eventIndex;
		entry.EventTag = eventTag;
		entry.EventClassName = eventClassName;
		entry.bEmitter = true;
	}

	bool HasSignalWaiterAtOrAfterIndex(FName signalTag, int32 eventIndex) const
	{
		if (signalTag.IsNone())
		{
			return false;
		}

		for (const FVdjmRecordEventSignalManifestEntry& entry : SignalEntries)
		{
			if (entry.bWaiter &&
				entry.SignalTag == signalTag &&
				(eventIndex == INDEX_NONE || entry.EventIndex >= eventIndex))
			{
				return true;
			}
		}

		return false;
	}
};

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

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	int32 FindSubgraphIndexByTag(FName subgraphTag) const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	void ResetRuntimeStates();

	bool CompileManifest(FString& outError);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	UVdjmRecordEventFlowDataAsset* GetSourceFlowAsset() const;

	UPROPERTY(Transient, Instanced, BlueprintReadOnly, Category = "Recorder|EventFlowRuntime")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Events;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Recorder|EventFlowRuntime")
	TArray<FVdjmRecordEventSubgraph> Subgraphs;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Recorder|EventFlowRuntime")
	FVdjmRecordEventFlowManifest CompiledManifest;

private:
	bool BuildEventNodeFromFragment(const FVdjmRecordEventNodeFragment& InFragment, UVdjmRecordEventBase*& OutEventNode, FString& OutError);
	bool BuildEventNodesFromFragment(const FVdjmRecordEventFlowFragment& InFragment, TArray<TObjectPtr<UVdjmRecordEventBase>>& OutEvents, FString& OutError);

	TWeakObjectPtr<UVdjmRecordEventFlowDataAsset> SourceFlowAsset;
};
