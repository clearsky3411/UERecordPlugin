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
	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	int32 FindEventIndexByTag(FName InTag) const;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Recorder|EventFlow")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Events;
};
