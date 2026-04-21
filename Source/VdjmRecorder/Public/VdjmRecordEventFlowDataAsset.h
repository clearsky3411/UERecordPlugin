#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VdjmRecordEventFlowDataAsset.generated.h"

class UVdjmRecordEventBase;

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventFlowDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	static UVdjmRecordEventFlowDataAsset* TryGetEventFlowDataAsset(const FSoftObjectPath& AssetPath);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	int32 FindEventIndexByTag(FName InTag) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	FString ExportFlowToJsonString(bool bPrettyPrint = true) const;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Recorder|EventFlow")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Events;
};
