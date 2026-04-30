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

	bool CloneEventArray(
		const TArray<TObjectPtr<UVdjmRecordEventBase>>& sourceEvents,
		UObject* outer,
		TArray<TObjectPtr<UVdjmRecordEventBase>>& outEvents,
		FString& outError,
		const FString& contextLabel)
	{
		outEvents.Reset();
		outEvents.Reserve(sourceEvents.Num());

		for (int32 eventIndex = 0; eventIndex < sourceEvents.Num(); ++eventIndex)
		{
			const UVdjmRecordEventBase* sourceEventNode = sourceEvents[eventIndex];
			if (sourceEventNode == nullptr)
			{
				outEvents.Add(nullptr);
				continue;
			}

			TSharedPtr<FJsonObject> eventJsonObject;
			if (not VdjmRecordEventJson::SerializeEventNodeToJsonObject(sourceEventNode, eventJsonObject, outError))
			{
				outError = FString::Printf(TEXT("%s index %d serialize failed: %s"), *contextLabel, eventIndex, *outError);
				return false;
			}

			UVdjmRecordEventBase* newEventNode = nullptr;
			if (not VdjmRecordEventJson::DeserializeEventNodeFromJsonObject(eventJsonObject, outer, newEventNode, outError))
			{
				outError = FString::Printf(TEXT("%s index %d clone failed: %s"), *contextLabel, eventIndex, *outError);
				return false;
			}

			outEvents.Add(newEventNode);
		}

		return true;
	}

	bool DeserializeEventArrayFromJsonValues(
		const TArray<TSharedPtr<FJsonValue>>& eventValues,
		UObject* outer,
		TArray<TObjectPtr<UVdjmRecordEventBase>>& outEvents,
		FString& outError,
		const FString& contextLabel)
	{
		outEvents.Reset();
		outEvents.Reserve(eventValues.Num());

		for (int32 eventIndex = 0; eventIndex < eventValues.Num(); ++eventIndex)
		{
			const TSharedPtr<FJsonValue>& eventValue = eventValues[eventIndex];
			if (not eventValue.IsValid() || eventValue->Type != EJson::Object)
			{
				outError = FString::Printf(TEXT("%s index %d has invalid event type."), *contextLabel, eventIndex);
				return false;
			}

			UVdjmRecordEventBase* newNode = nullptr;
			if (not VdjmRecordEventJson::DeserializeEventNodeFromJsonObject(eventValue->AsObject(), outer, newNode, outError))
			{
				outError = FString::Printf(TEXT("%s index %d parse failed: %s"), *contextLabel, eventIndex, *outError);
				return false;
			}

			outEvents.Add(newNode);
		}

		return true;
	}

	bool DeserializeRequiredEventArrayField(
		const TSharedPtr<FJsonObject>& rootObject,
		const TCHAR* fieldName,
		UObject* outer,
		TArray<TObjectPtr<UVdjmRecordEventBase>>& outEvents,
		FString& outError,
		const FString& contextLabel)
	{
		const TArray<TSharedPtr<FJsonValue>>* eventValues = nullptr;
		if (not rootObject->TryGetArrayField(fieldName, eventValues) || eventValues == nullptr)
		{
			outError = FString::Printf(TEXT("Missing %s array."), fieldName);
			return false;
		}

		return DeserializeEventArrayFromJsonValues(*eventValues, outer, outEvents, outError, contextLabel);
	}

	bool ValidateUniqueSubgraphTags(const TArray<FVdjmRecordEventSubgraph>& subgraphs, FString& outError)
	{
		TSet<FName> seenTags;
		for (const FVdjmRecordEventSubgraph& subgraph : subgraphs)
		{
			if (subgraph.SubgraphTag.IsNone())
			{
				continue;
			}

			if (seenTags.Contains(subgraph.SubgraphTag))
			{
				outError = FString::Printf(TEXT("Duplicate subgraph tag: %s"), *subgraph.SubgraphTag.ToString());
				return false;
			}

			seenTags.Add(subgraph.SubgraphTag);
		}

		return true;
	}

	bool DeserializeSubgraphsFromJsonObject(
		const TSharedPtr<FJsonObject>& rootObject,
		UObject* outer,
		TArray<FVdjmRecordEventSubgraph>& outSubgraphs,
		FString& outError)
	{
		outSubgraphs.Reset();

		const TArray<TSharedPtr<FJsonValue>>* subgraphValues = nullptr;
		if (not rootObject->TryGetArrayField(TEXT("subgraphs"), subgraphValues) || subgraphValues == nullptr)
		{
			return true;
		}

		outSubgraphs.Reserve(subgraphValues->Num());
		for (int32 subgraphIndex = 0; subgraphIndex < subgraphValues->Num(); ++subgraphIndex)
		{
			const TSharedPtr<FJsonValue>& subgraphValue = (*subgraphValues)[subgraphIndex];
			if (not subgraphValue.IsValid() || subgraphValue->Type != EJson::Object)
			{
				outError = FString::Printf(TEXT("Invalid subgraph type at index %d."), subgraphIndex);
				return false;
			}

			const TSharedPtr<FJsonObject> subgraphObject = subgraphValue->AsObject();
			FString tagString;
			if (not subgraphObject->TryGetStringField(TEXT("tag"), tagString))
			{
				subgraphObject->TryGetStringField(TEXT("SubgraphTag"), tagString);
			}

			FVdjmRecordEventSubgraph newSubgraph;
			newSubgraph.SubgraphTag = tagString.IsEmpty() ? NAME_None : FName(*tagString);

			const FString contextLabel = FString::Printf(TEXT("Subgraph '%s' event"), *newSubgraph.SubgraphTag.ToString());
			if (not DeserializeRequiredEventArrayField(
				subgraphObject,
				TEXT("events"),
				outer,
				newSubgraph.Events,
				outError,
				contextLabel))
			{
				outError = FString::Printf(TEXT("Subgraph index %d parse failed: %s"), subgraphIndex, *outError);
				return false;
			}

			outSubgraphs.Add(MoveTemp(newSubgraph));
		}

		return ValidateUniqueSubgraphTags(outSubgraphs, outError);
	}

	void SerializeEventArrayToJsonValues(
		const TArray<TObjectPtr<UVdjmRecordEventBase>>& events,
		TArray<TSharedPtr<FJsonValue>>& outEventValues,
		const FString& contextLabel)
	{
		outEventValues.Reset();
		outEventValues.Reserve(events.Num());

		for (const UVdjmRecordEventBase* eventNode : events)
		{
			TSharedPtr<FJsonObject> eventObject;
			FString serializeError;
			if (VdjmRecordEventJson::SerializeEventNodeToJsonObject(eventNode, eventObject, serializeError) && eventObject.IsValid())
			{
				outEventValues.Add(MakeShared<FJsonValueObject>(eventObject));
			}
			else if (not serializeError.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("%s - Failed to serialize event node: %s"), *contextLabel, *serializeError);
			}
		}
	}

	void SerializeSubgraphsToJsonValues(
		const TArray<FVdjmRecordEventSubgraph>& subgraphs,
		TArray<TSharedPtr<FJsonValue>>& outSubgraphValues)
	{
		outSubgraphValues.Reset();
		outSubgraphValues.Reserve(subgraphs.Num());

		for (const FVdjmRecordEventSubgraph& subgraph : subgraphs)
		{
			const TSharedPtr<FJsonObject> subgraphObject = MakeShared<FJsonObject>();
			subgraphObject->SetStringField(TEXT("tag"), subgraph.SubgraphTag.ToString());

			TArray<TSharedPtr<FJsonValue>> eventValues;
			const FString contextLabel = FString::Printf(TEXT("Export subgraph '%s'"), *subgraph.SubgraphTag.ToString());
			SerializeEventArrayToJsonValues(subgraph.Events, eventValues, contextLabel);
			subgraphObject->SetArrayField(TEXT("events"), eventValues);

			outSubgraphValues.Add(MakeShared<FJsonValueObject>(subgraphObject));
		}
	}
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

UVdjmRecordEventFlowRuntime* UVdjmRecordEventFlowRuntime::CreateEmptyFlowRuntime(UObject* Outer)
{
	UObject* RuntimeOuter = Outer ? Outer : GetTransientPackage();
	UVdjmRecordEventFlowRuntime* NewFlowRuntime = NewObject<UVdjmRecordEventFlowRuntime>(RuntimeOuter);
	if (NewFlowRuntime != nullptr)
	{
		NewFlowRuntime->InitializeEmpty();
	}

	return NewFlowRuntime;
}

bool UVdjmRecordEventFlowRuntime::InitializeEmpty()
{
	SourceFlowAsset = nullptr;
	Events.Reset();
	Subgraphs.Reset();
	CompiledManifest.Reset();
	return true;
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

	TArray<TObjectPtr<UVdjmRecordEventBase>> newEvents;
	if (not CloneEventArray(InSourceFlowAsset->Events, this, newEvents, OutError, TEXT("Root event")))
	{
		return false;
	}

	TArray<FVdjmRecordEventSubgraph> newSubgraphs;
	newSubgraphs.Reserve(InSourceFlowAsset->Subgraphs.Num());
	for (int32 subgraphIndex = 0; subgraphIndex < InSourceFlowAsset->Subgraphs.Num(); ++subgraphIndex)
	{
		const FVdjmRecordEventSubgraph& sourceSubgraph = InSourceFlowAsset->Subgraphs[subgraphIndex];

		FVdjmRecordEventSubgraph newSubgraph;
		newSubgraph.SubgraphTag = sourceSubgraph.SubgraphTag;

		const FString contextLabel = FString::Printf(TEXT("Subgraph '%s' event"), *sourceSubgraph.SubgraphTag.ToString());
		if (not CloneEventArray(sourceSubgraph.Events, this, newSubgraph.Events, OutError, contextLabel))
		{
			OutError = FString::Printf(TEXT("Subgraph index %d clone failed: %s"), subgraphIndex, *OutError);
			return false;
		}

		newSubgraphs.Add(MoveTemp(newSubgraph));
	}

	if (not ValidateUniqueSubgraphTags(newSubgraphs, OutError))
	{
		return false;
	}

	Events = MoveTemp(newEvents);
	Subgraphs = MoveTemp(newSubgraphs);
	SourceFlowAsset = const_cast<UVdjmRecordEventFlowDataAsset*>(InSourceFlowAsset);
	return CompileManifest(OutError);
}

bool UVdjmRecordEventFlowRuntime::InitializeFromJsonString(const FString& InJsonString, FString& OutError)
{
	SourceFlowAsset = nullptr;
	return ImportFlowFromJsonString(InJsonString, OutError);
}

bool UVdjmRecordEventFlowRuntime::ImportFlowFromJsonString(const FString& InJsonString, FString& OutError)
{
	OutError.Reset();
	SourceFlowAsset = nullptr;

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

	TArray<TObjectPtr<UVdjmRecordEventBase>> NewEvents;
	if (not DeserializeRequiredEventArrayField(RootObject, TEXT("events"), this, NewEvents, OutError, TEXT("Root event")))
	{
		return false;
	}

	TArray<FVdjmRecordEventSubgraph> newSubgraphs;
	if (not DeserializeSubgraphsFromJsonObject(RootObject, this, newSubgraphs, OutError))
	{
		return false;
	}

	Events = MoveTemp(NewEvents);
	Subgraphs = MoveTemp(newSubgraphs);
	return CompileManifest(OutError);
}

bool UVdjmRecordEventFlowRuntime::AppendFlowFragment(const FVdjmRecordEventFlowFragment& InFragment, FString& OutError)
{
	TArray<TObjectPtr<UVdjmRecordEventBase>> NewEvents;
	if (!BuildEventNodesFromFragment(InFragment, NewEvents, OutError))
	{
		return false;
	}

	SourceFlowAsset = nullptr;
	Events.Append(MoveTemp(NewEvents));
	return CompileManifest(OutError);
}

bool UVdjmRecordEventFlowRuntime::InsertFlowFragment(int32 InsertIndex, const FVdjmRecordEventFlowFragment& InFragment, FString& OutError)
{
	if (InsertIndex < 0 || InsertIndex > Events.Num())
	{
		OutError = TEXT("Insert index is out of range.");
		return false;
	}

	TArray<TObjectPtr<UVdjmRecordEventBase>> NewEvents;
	if (!BuildEventNodesFromFragment(InFragment, NewEvents, OutError))
	{
		return false;
	}

	if (NewEvents.Num() == 0)
	{
		return true;
	}

	SourceFlowAsset = nullptr;
	for (int32 OffsetIndex = 0; OffsetIndex < NewEvents.Num(); ++OffsetIndex)
	{
		Events.Insert(NewEvents[OffsetIndex], InsertIndex + OffsetIndex);
	}
	return CompileManifest(OutError);
}

bool UVdjmRecordEventFlowRuntime::ReplaceEventByTagFromFragment(FName InTag, const FVdjmRecordEventNodeFragment& InFragment, FString& OutError)
{
	OutError.Reset();

	const int32 EventIndex = FindEventIndexByTag(InTag);
	if (EventIndex == INDEX_NONE)
	{
		OutError = TEXT("Target event tag was not found.");
		return false;
	}

	UVdjmRecordEventBase* NewEventNode = nullptr;
	if (!BuildEventNodeFromFragment(InFragment, NewEventNode, OutError))
	{
		return false;
	}

	SourceFlowAsset = nullptr;
	Events[EventIndex] = NewEventNode;
	return CompileManifest(OutError);
}

bool UVdjmRecordEventFlowRuntime::RemoveEventAt(int32 EventIndex)
{
	if (!Events.IsValidIndex(EventIndex))
	{
		return false;
	}

	SourceFlowAsset = nullptr;
	Events.RemoveAt(EventIndex);

	FString compileError;
	return CompileManifest(compileError);
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
	SerializeEventArrayToJsonValues(Events, EventValues, TEXT("Export root flow"));
	RootObject->SetArrayField(TEXT("events"), EventValues);

	TArray<TSharedPtr<FJsonValue>> SubgraphValues;
	SerializeSubgraphsToJsonValues(Subgraphs, SubgraphValues);
	RootObject->SetArrayField(TEXT("subgraphs"), SubgraphValues);

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

int32 UVdjmRecordEventFlowRuntime::FindSubgraphIndexByTag(FName subgraphTag) const
{
	if (subgraphTag.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 index = 0; index < Subgraphs.Num(); ++index)
	{
		if (Subgraphs[index].SubgraphTag == subgraphTag)
		{
			return index;
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

	for (FVdjmRecordEventSubgraph& subgraph : Subgraphs)
	{
		for (UVdjmRecordEventBase* eventNode : subgraph.Events)
		{
			if (eventNode != nullptr)
			{
				eventNode->ResetRuntimeState();
			}
		}
	}
}

bool UVdjmRecordEventFlowRuntime::CompileManifest(FString& outError)
{
	outError.Reset();
	CompiledManifest.Reset();

	for (int32 eventIndex = 0; eventIndex < Events.Num(); ++eventIndex)
	{
		const UVdjmRecordEventBase* eventNode = Events[eventIndex];
		if (eventNode == nullptr)
		{
			continue;
		}

		eventNode->CollectFlowManifest(CompiledManifest, eventIndex);
	}

	return true;
}

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventFlowRuntime::GetSourceFlowAsset() const
{
	return SourceFlowAsset.Get();
}

bool UVdjmRecordEventFlowRuntime::BuildEventNodeFromFragment(const FVdjmRecordEventNodeFragment& InFragment, UVdjmRecordEventBase*& OutEventNode, FString& OutError)
{
	OutError.Reset();
	OutEventNode = nullptr;

	TSharedPtr<FJsonObject> EventJsonObject;
	if (!InFragment.WriteEventJsonObject(EventJsonObject, OutError))
	{
		return false;
	}

	return VdjmRecordEventJson::DeserializeEventNodeFromJsonObject(EventJsonObject, this, OutEventNode, OutError);
}

bool UVdjmRecordEventFlowRuntime::BuildEventNodesFromFragment(
	const FVdjmRecordEventFlowFragment& InFragment,
	TArray<TObjectPtr<UVdjmRecordEventBase>>& OutEvents,
	FString& OutError)
{
	OutError.Reset();
	OutEvents.Reset();

	for (int32 EventIndex = 0; EventIndex < InFragment.Events.Num(); ++EventIndex)
	{
		UVdjmRecordEventBase* NewEventNode = nullptr;
		if (!BuildEventNodeFromFragment(InFragment.Events[EventIndex], NewEventNode, OutError))
		{
			OutError = FString::Printf(TEXT("Fragment event index %d build failed: %s"), EventIndex, *OutError);
			OutEvents.Reset();
			return false;
		}

		OutEvents.Add(NewEventNode);
	}

	return true;
}
