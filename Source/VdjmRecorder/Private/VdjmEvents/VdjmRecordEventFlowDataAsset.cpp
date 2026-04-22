#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmEvents/VdjmRecordEventJsonHelper.h"
#include "VdjmEvents/VdjmRecordEventNode.h"

namespace
{
	constexpr int32 FlowSchemaVersion = 1;
}

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventFlowDataAsset::TryGetEventFlowDataAsset(const FSoftObjectPath& AssetPath)
{
	if (!AssetPath.IsValid())
	{
		return nullptr;
	}

	UObject* LoadedObject = AssetPath.TryLoad();
	return Cast<UVdjmRecordEventFlowDataAsset>(LoadedObject);
}

bool UVdjmRecordEventFlowDataAsset::ImportFlowFromJsonString(const FString& InJsonString, FString& OutError)
{
	OutError.Reset();

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Failed to parse flow json.");
		return false;
	}

	double SchemaVersion = 0.0;
	if (!RootObject->TryGetNumberField(TEXT("schema_version"), SchemaVersion) || static_cast<int32>(SchemaVersion) != FlowSchemaVersion)
	{
		OutError = FString::Printf(TEXT("Unsupported schema_version. expected=%d"), FlowSchemaVersion);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* EventValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("events"), EventValues) || EventValues == nullptr)
	{
		OutError = TEXT("Missing events array.");
		return false;
	}

	TArray<TObjectPtr<UVdjmRecordEventBase>> NewEvents;
	NewEvents.Reserve(EventValues->Num());

	for (int32 EventIndex = 0; EventIndex < EventValues->Num(); ++EventIndex)
	{
		const TSharedPtr<FJsonValue>& EventValue = (*EventValues)[EventIndex];
		if (!EventValue.IsValid() || EventValue->Type != EJson::Object)
		{
			OutError = FString::Printf(TEXT("Invalid event type at index %d."), EventIndex);
			return false;
		}

		UVdjmRecordEventBase* NewNode = nullptr;
		if (!VdjmRecordEventJson::DeserializeEventNodeFromJsonObject(EventValue->AsObject(), this, NewNode, OutError))
		{
			OutError = FString::Printf(TEXT("Event index %d parse failed: %s"), EventIndex, *OutError);
			return false;
		}

		NewEvents.Add(NewNode);
	}

	Events = MoveTemp(NewEvents);
	MarkPackageDirty();
	return true;
}

bool UVdjmRecordEventFlowDataAsset::ValidateFlowJson(const FString& InJsonString, FString& OutError) const
{
	OutError.Reset();

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Failed to parse flow json.");
		return false;
	}

	double SchemaVersion = 0.0;
	if (!RootObject->TryGetNumberField(TEXT("schema_version"), SchemaVersion) || static_cast<int32>(SchemaVersion) != FlowSchemaVersion)
	{
		OutError = FString::Printf(TEXT("Unsupported schema_version. expected=%d"), FlowSchemaVersion);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* EventValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("events"), EventValues) || EventValues == nullptr)
	{
		OutError = TEXT("Missing events array.");
		return false;
	}

	for (int32 EventIndex = 0; EventIndex < EventValues->Num(); ++EventIndex)
	{
		const TSharedPtr<FJsonValue>& EventValue = (*EventValues)[EventIndex];
		if (!EventValue.IsValid() || EventValue->Type != EJson::Object)
		{
			OutError = FString::Printf(TEXT("Invalid event type at index %d."), EventIndex);
			return false;
		}

		UVdjmRecordEventBase* DummyNode = nullptr;
		if (!VdjmRecordEventJson::DeserializeEventNodeFromJsonObject(EventValue->AsObject(), GetTransientPackage(), DummyNode, OutError))
		{
			OutError = FString::Printf(TEXT("Event index %d validation failed: %s"), EventIndex, *OutError);
			return false;
		}
	}

	return true;
}

int32 UVdjmRecordEventFlowDataAsset::FindEventIndexByTag(FName InTag) const
{
	if (InTag.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		const UVdjmRecordEventBase* Event = Events[Index];
		if (Event != nullptr && Event->EventTag == InTag)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FString UVdjmRecordEventFlowDataAsset::ExportFlowToJsonString(bool bPrettyPrint) const
{
	const TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("asset_name"), GetPathName());
	RootObject->SetNumberField(TEXT("schema_version"), FlowSchemaVersion);

	TArray<TSharedPtr<FJsonValue>> EventValues;
	for (const UVdjmRecordEventBase* Event : Events)
	{
		TSharedPtr<FJsonObject> EventObject;
		FString SerializeError;
		if (VdjmRecordEventJson::SerializeEventNodeToJsonObject(Event, EventObject, SerializeError) && EventObject.IsValid())
		{
			EventValues.Add(MakeShared<FJsonValueObject>(EventObject));
		}
		else if (!SerializeError.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("ExportFlowToJsonString - Failed to serialize event node: %s"), *SerializeError);
		}
	}
	RootObject->SetArrayField(TEXT("events"), EventValues);

	FString OutJson;
	if (bPrettyPrint)
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	}
	else
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	}

	return OutJson;
}
