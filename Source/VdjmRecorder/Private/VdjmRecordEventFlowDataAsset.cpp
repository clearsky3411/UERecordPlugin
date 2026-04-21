#include "VdjmRecordEventFlowDataAsset.h"

#include "JsonObjectConverter.h"
#include "VdjmRecordTypes.h"
#include "Misc/FileHelper.h"

namespace
{
	template <typename PrintPolicy>
	FString SerializeJsonObject(const TSharedRef<FJsonObject>& InJsonObject)
	{
		FString OutJsonString;
		TSharedRef<TJsonWriter<TCHAR, PrintPolicy>> Writer = TJsonWriterFactory<TCHAR, PrintPolicy>::Create(&OutJsonString);
		FJsonSerializer::Serialize(InJsonObject, Writer);
		return OutJsonString;
	}
}

FString UVdjmRecordEventFlowDataAsset::ExportFlowToJson(bool bPrettyPrint) const
{
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	if (!FJsonObjectConverter::UStructToJsonObject(FVdjmRecordEventFlowExportPayload::StaticStruct(), &EventFlow, JsonObject, 0, 0))
	{
		return FString();
	}

	return bPrettyPrint
		? SerializeJsonObject<TPrettyJsonPrintPolicy<TCHAR>>(JsonObject)
		: SerializeJsonObject<TCondensedJsonPrintPolicy<TCHAR>>(JsonObject);
}

bool UVdjmRecordEventFlowDataAsset::SaveFlowJsonToFile(const FString& InFilePath, bool bPrettyPrint, FString& OutErrorReason) const
{
	OutErrorReason.Reset();
	if (InFilePath.TrimStartAndEnd().IsEmpty())
	{
		OutErrorReason = TEXT("File path is empty.");
		return false;
	}

	const FString JsonString = ExportFlowToJson(bPrettyPrint);
	if (JsonString.IsEmpty())
	{
		OutErrorReason = TEXT("Failed to export event flow to JSON string.");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonString, *InFilePath))
	{
		OutErrorReason = FString::Printf(TEXT("Failed to save JSON file. Path=%s"), *InFilePath);
		return false;
	}

	return true;
}

bool UVdjmRecordEventFlowDataAsset::ImportFlowFromJson(const FString& InJsonString, FString& OutErrorReason)
{
	OutErrorReason.Reset();
	if (InJsonString.TrimStartAndEnd().IsEmpty())
	{
		OutErrorReason = TEXT("Input JSON string is empty.");
		return false;
	}

	FVdjmRecordEventFlowExportPayload ParsedPayload;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(
		InJsonString,
		&ParsedPayload,
		0,
		0))
	{
		OutErrorReason = TEXT("Failed to parse JSON into FVdjmRecordEventFlowExportPayload.");
		return false;
	}

	EventFlow = MoveTemp(ParsedPayload);
	return true;
}

bool UVdjmRecordEventFlowDataAsset::HasValidRootNodes() const
{
	return EventFlow.RootNodes.Num() > 0;
}

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventFlowDataAsset::TryGetRecordEventFlowDataAsset(const FSoftObjectPath& InAssetPath)
{
	if (!InAssetPath.IsAsset() || !InAssetPath.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("TryGetRecordEventFlowDataAsset - Invalid soft object path: %s"), *InAssetPath.ToString());
		return nullptr;
	}

	UObject* ResolvedObject = InAssetPath.ResolveObject();
	if (ResolvedObject == nullptr)
	{
		ResolvedObject = InAssetPath.TryLoad();
	}

	if (ResolvedObject == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("TryGetRecordEventFlowDataAsset - Failed to load object: %s"), *InAssetPath.ToString());
		return nullptr;
	}

	UVdjmRecordEventFlowDataAsset* Result = Cast<UVdjmRecordEventFlowDataAsset>(ResolvedObject);
	if (Result == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("TryGetRecordEventFlowDataAsset - Loaded object is not UVdjmRecordEventFlowDataAsset: %s"), *InAssetPath.ToString());
	}

	return Result;
}
