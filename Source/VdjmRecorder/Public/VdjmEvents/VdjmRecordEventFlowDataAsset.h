#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VdjmRecordEventFlowDataAsset.generated.h"

class UVdjmRecordEventBase;
struct FVdjmRecordEventFlowFragment;

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventFlowDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
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
	FString ExportFlowToJsonString(bool bPrettyPrint = true) const;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Recorder|EventFlowAsset")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Events;
};
