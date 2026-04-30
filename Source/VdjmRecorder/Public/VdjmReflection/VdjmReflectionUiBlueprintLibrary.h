#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "VdjmReflection/VdjmReflectionUiTypes.h"
#include "VdjmReflectionUiBlueprintLibrary.generated.h"

/**
 * @brief FProperty/UPROPERTY metadata를 UMG가 사용할 수 있는 UI item 목록으로 변환하는 공용 BP 헬퍼다.
 */
UCLASS()
class VDJMRECORDER_API UVdjmReflectionUiBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi", meta = (WorldContext = "worldContextObject"))
	static TArray<UVdjmReflectionUiItemObject*> BuildReflectionUiItemsFromStruct(
		UObject* worldContextObject,
		UScriptStruct* structType);

	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi", meta = (WorldContext = "worldContextObject"))
	static TArray<UVdjmReflectionUiItemObject*> BuildReflectionUiItemsFromJsonString(
		UObject* worldContextObject,
		const FString& schemaJsonString,
		FString& outErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi")
	static FString ExportReflectionUiMapToJson(UScriptStruct* structType, bool bPrettyPrint = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Option|Ui", meta = (WorldContext = "worldContextObject"))
	static TArray<UVdjmReflectionUiItemObject*> BuildRecorderOptionUiItems(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Option|Ui", meta = (WorldContext = "worldContextObject"))
	static bool SubmitRecorderOptionUiValue(
		UObject* worldContextObject,
		FName optionKey,
		const FString& valueString,
		FString& outErrorReason,
		bool bProcessPendingAfterSubmit = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Option|Ui", meta = (WorldContext = "worldContextObject"))
	static bool SubmitRecorderOptionUiItemValue(
		UObject* worldContextObject,
		UVdjmReflectionUiItemObject* itemObject,
		const FString& valueString,
		FString& outErrorReason,
		bool bProcessPendingAfterSubmit = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Option|Ui", meta = (WorldContext = "worldContextObject"))
	static bool SubmitRecorderOptionUiBoolValue(
		UObject* worldContextObject,
		FName optionKey,
		bool bValue,
		FString& outErrorReason,
		bool bProcessPendingAfterSubmit = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Option|Ui", meta = (WorldContext = "worldContextObject"))
	static bool SubmitRecorderOptionUiFloatValue(
		UObject* worldContextObject,
		FName optionKey,
		float value,
		FString& outErrorReason,
		bool bProcessPendingAfterSubmit = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Option|Ui", meta = (WorldContext = "worldContextObject"))
	static bool SubmitRecorderOptionUiIntValue(
		UObject* worldContextObject,
		FName optionKey,
		int32 value,
		FString& outErrorReason,
		bool bProcessPendingAfterSubmit = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Option|Ui", meta = (WorldContext = "worldContextObject"))
	static bool SubmitRecorderOptionUiIntPointValue(
		UObject* worldContextObject,
		FName optionKey,
		FIntPoint value,
		FString& outErrorReason,
		bool bProcessPendingAfterSubmit = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Option|Ui")
	static FString ExportRecorderOptionUiMapToJson(bool bPrettyPrint = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Option|Ui")
	static FString ExportRecorderOptionUiSchemaJson(bool bPrettyPrint = true);
};
