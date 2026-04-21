#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VdjmRecordEventFlowDataAsset.generated.h"

UENUM(BlueprintType)
enum class EVdjmRecordEventFlowNodeType : uint8
{
	EAction UMETA(DisplayName = "Action"),
	ESequence UMETA(DisplayName = "Sequence"),
	ESelector UMETA(DisplayName = "Selector")
};

UENUM(BlueprintType)
enum class EVdjmRecordEventFlowResult : uint8
{
	ESuccess UMETA(DisplayName = "Success"),
	EFailure UMETA(DisplayName = "Failure"),
	ERunning UMETA(DisplayName = "Running"),
	EAbort UMETA(DisplayName = "Abort"),
	ESelectIndex UMETA(DisplayName = "SelectIndex"),
	EJumpLabel UMETA(DisplayName = "JumpLabel")
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordEventFlowNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	FName NodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	EVdjmRecordEventFlowNodeType NodeType = EVdjmRecordEventFlowNodeType::EAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	TSoftClassPtr<UObject> EventClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	TMap<FString, FString> EventParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	TArray<FVdjmRecordEventFlowNode> Children;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	int32 SelectIndexOnResult = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	FName JumpLabelOnResult = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	EVdjmRecordEventFlowResult ResultPolicy = EVdjmRecordEventFlowResult::ESuccess;
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordEventFlowExportPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	FString SchemaVersion = TEXT("1.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	TArray<FVdjmRecordEventFlowNode> RootNodes;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventFlowDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	FVdjmRecordEventFlowExportPayload EventFlow;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	FString ExportFlowToJson(bool bPrettyPrint = true) const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool SaveFlowJsonToFile(const FString& InFilePath, bool bPrettyPrint, FString& OutErrorReason) const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow")
	bool ImportFlowFromJson(const FString& InJsonString, FString& OutErrorReason);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	bool HasValidRootNodes() const;

	static UVdjmRecordEventFlowDataAsset* TryGetRecordEventFlowDataAsset(
		const FSoftObjectPath& InAssetPath = FSoftObjectPath(
			TEXT("/Script/VdjmRecorder.VdjmRecordEventFlowDataAsset'/Game/Temp/Bp_VdjmRecordEventFlowDataAsset.Bp_VdjmRecordEventFlowDataAsset'")));
};
