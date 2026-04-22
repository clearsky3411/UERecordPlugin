#include "VdjmEvents/VdjmRecordEventFlowRuntime.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"
#include "VdjmEvents/VdjmRecordEventJsonHelper.h"
#include "VdjmEvents/VdjmRecordEventNode.h"

namespace
{
	constexpr int32 FlowSchemaVersion = 1;
}

UVdjmRecordEventFlowRuntime* UVdjmRecordEventFlowRuntime::CreateFlowRuntimeFromAsset(
	UObject* Outer,
	const UVdjmRecordEventFlowDataAsset* SourceFlowAsset,
	FString& OutError)
{
	OutError.Reset();

	UObject* RuntimeOuter = Outer ? Outer : GetTransientPackage();
	UVdjmRecordEventFlowRuntime* NewFlowRuntime = NewObject<UVdjmRecordEventFlowRuntime>(RuntimeOuter);
	if (NewFlowRuntime == nullptr)
	{
		OutError = TEXT("Failed to allocate flow runtime.");
		return nullptr;
	}

	if (!NewFlowRuntime->InitializeFromAsset(SourceFlowAsset, OutError))
	{
		return nullptr;
	}

	return NewFlowRuntime;
}

UVdjmRecordEventFlowRuntime* UVdjmRecordEventFlowRuntime::CreateFlowRuntimeFromJsonString(
	UObject* Outer,
	const FString& InJsonString,
	FString& OutError)
{
	OutError.Reset();

	UObject* RuntimeOuter = Outer ? Outer : GetTransientPackage();
	UVdjmRecordEventFlowRuntime* NewFlowRuntime = NewObject<UVdjmRecordEventFlowRuntime>(RuntimeOuter);
	if (NewFlowRuntime == nullptr)
	{
		OutError = TEXT("Failed to allocate flow runtime.");
		return nullptr;
	}

	if (!NewFlowRuntime->InitializeFromJsonString(InJsonString, OutError))
	{
		return nullptr;
	}

	return NewFlowRuntime;
}

bool UVdjmRecordEventFlowRuntime::InitializeFromAsset(const UVdjmRecordEventFlowDataAsset* InSourceFlowAsset, FString& OutError)
{
	OutError.Reset();
	SourceFlowAsset = nullptr;

	if (InSourceFlowAsset == nullptr)
	{
		OutError = TEXT("Source flow asset is null.");
		return false;
	}

	TArray<TObjectPtr<UVdjmRecordEventBase>> NewEvents;
	NewEvents.Reserve(InSourceFlowAsset->Events.Num());

	for (int32 EventIndex = 0; EventIndex < InSourceFlowAsset->Events.Num(); ++EventIndex)
	{
		const UVdjmRecordEventBase* SourceEventNode = InSourceFlowAsset->Events[EventIndex];
		if (SourceEventNode == nullptr)
		{
			NewEvents.Add(nullptr);
			continue;
		}

		TSharedPtr<FJsonObject> EventJsonObject;
		if (!VdjmRecordEventJson::SerializeEventNodeToJsonObject(SourceEventNode, EventJsonObject, OutError))
		{
			OutError = FString::Printf(TEXT("Event index %d serialize failed: %s"), EventIndex, *OutError);
			return false;
		}

		UVdjmRecordEventBase* NewEventNode = nullptr;
		if (!VdjmRecordEventJson::DeserializeEventNodeFromJsonObject(EventJsonObject, this, NewEventNode, OutError))
		{
			OutError = FString::Printf(TEXT("Event index %d clone failed: %s"), EventIndex, *OutError);
			return false;
		}

		NewEvents.Add(NewEventNode);
	}

	Events = MoveTemp(NewEvents);
	SourceFlowAsset = const_cast<UVdjmRecordEventFlowDataAsset*>(InSourceFlowAsset);
	return true;
}

bool UVdjmRecordEventFlowRuntime::InitializeFromJsonString(const FString& InJsonString, FString& OutError)
{
	SourceFlowAsset = nullptr;
	return ImportFlowFromJsonString(InJsonString, OutError);
}

bool UVdjmRecordEventFlowRuntime::ImportFlowFromJsonString(const FString& InJsonString, FString& OutError)
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
	return true;
}

FString UVdjmRecordEventFlowRuntime::ExportFlowToJsonString(bool bPrettyPrint) const
{
	const TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("schema_version"), FlowSchemaVersion);

	if (SourceFlowAsset.IsValid())
	{
		RootObject->SetStringField(TEXT("asset_name"), SourceFlowAsset->GetPathName());
	}

	TArray<TSharedPtr<FJsonValue>> EventValues;
	for (const UVdjmRecordEventBase* EventNode : Events)
	{
		TSharedPtr<FJsonObject> EventObject;
		FString SerializeError;
		if (VdjmRecordEventJson::SerializeEventNodeToJsonObject(EventNode, EventObject, SerializeError) && EventObject.IsValid())
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
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	}

	return OutJson;
}

int32 UVdjmRecordEventFlowRuntime::FindEventIndexByTag(FName InTag) const
{
	if (InTag.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		const UVdjmRecordEventBase* EventNode = Events[Index];
		if (EventNode != nullptr && EventNode->EventTag == InTag)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

void UVdjmRecordEventFlowRuntime::ResetRuntimeStates()
{
	for (UVdjmRecordEventBase* EventNode : Events)
	{
		if (EventNode != nullptr)
		{
			EventNode->ResetRuntimeState();
		}
	}
}

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventFlowRuntime::GetSourceFlowAsset() const
{
	return SourceFlowAsset.Get();
}
