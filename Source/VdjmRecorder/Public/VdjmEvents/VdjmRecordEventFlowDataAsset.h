#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VdjmRecordEventFlowDataAsset.generated.h"

class UVdjmRecordEventBase;
struct FVdjmRecordEventFlowFragment;

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordEventSubgraph
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowAsset")
	FName SubgraphTag = NAME_None;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Recorder|EventFlowAsset")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Events;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventFlowDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UVdjmRecordEventFlowDataAsset();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	static UVdjmRecordEventFlowDataAsset* TryGetEventFlowDataAsset(const FSoftObjectPath& AssetPath);

	static UVdjmRecordEventFlowDataAsset* CreateTransientFlowDataAsset(UObject* Outer);
	static UVdjmRecordEventFlowDataAsset* CreateTransientFlowDataAssetFromJsonString(UObject* Outer, const FString& InJsonString, FString& OutError);
	static UVdjmRecordEventFlowDataAsset* CreateTransientFlowDataAssetFromFragment(UObject* Outer, const FVdjmRecordEventFlowFragment& InFragment, FString& OutError);

	bool InitializeEmpty();
	bool InitializeFromJsonString(const FString& InJsonString, FString& OutError);
	bool ImportFlowFromFragment(const FVdjmRecordEventFlowFragment& InFragment, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool ImportFlowFromJsonString(const FString& InJsonString, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool ValidateFlowJson(const FString& InJsonString, FString& OutError) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	int32 FindEventIndexByTag(FName InTag) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	int32 FindSubgraphIndexByTag(FName subgraphTag) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	FString ExportFlowToJsonString(bool bPrettyPrint = true) const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Recorder|EventFlow")
	void BuildExportedJson();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Recorder|EventFlow")
	void ClearExportedJson();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Recorder|EventFlow|Guide")
	void ResetAuthoringGuide();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Recorder|EventFlow|Summary")
	void RefreshEventSummary();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Recorder|EventFlow|Summary")
	void ClearEventSummary();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowAsset|Guide", meta = (MultiLine = "true", DisplayName = "Flow Authoring Guide"))
	FString FlowAuthoringGuide;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlowAsset|Guide", meta = (MultiLine = "true", DisplayName = "Author Notes"))
	FString FlowAuthorNotes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlowAsset|Summary", meta = (DisplayName = "Summary Filter", ToolTip = "비워두면 전체 이벤트를 보여주고, 값을 넣으면 class/tag/key/property 문자열에 포함된 이벤트만 요약합니다."))
	FString EventSummaryFilter;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Recorder|EventFlowAsset|Summary", meta = (MultiLine = "true", DisplayName = "Current Event Summary"))
	FString EventSummary;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Recorder|EventFlowAsset")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Events;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlowAsset", meta = (TitleProperty = "SubgraphTag"))
	TArray<FVdjmRecordEventSubgraph> Subgraphs;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Recorder|EventFlowAsset|Debug", meta = (MultiLine = "true"))
	FString ExportedFlowJson;
};
