#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmEvents/VdjmRecordEventFlowFragment.h"
#include "VdjmEvents/VdjmRecordEventJsonHelper.h"
#include "VdjmEvents/VdjmRecordEventNode.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr int32 FlowSchemaVersion = 1;

	FString MakeDefaultFlowAuthoringGuide()
	{
		return FString::Printf(
			TEXT("Vdjm Record EventFlow Quick Guide%s%s")
			TEXT("Session: 독립 실행 영역입니다. Signal, RuntimeSlot, pause/stop/fail 요청은 Session 단위로 분리됩니다.%s")
			TEXT("Flow: Session 안에서 실행되는 이벤트 배열의 런타임 진행 상태입니다. Sequence 같은 composite는 같은 Session 안에 child flow 상태를 만들 수 있습니다.%s")
			TEXT("EventTag: 이벤트 노드 자체를 식별하는 작성/디버그용 태그입니다. 객체 저장이나 signal 대기에는 직접 쓰이지 않습니다.%s")
			TEXT("RuntimeSlotKey: 현재 Session 안에서 임시 객체를 저장/조회하는 이름입니다. 위젯, Controller, Actor 같은 런타임 참조를 넘길 때 씁니다.%s")
			TEXT("ContextKey: WorldContextSubsystem에 등록되는 전역 조회 키입니다. flow 밖의 UI/Actor/Blueprint가 객체를 찾을 때 씁니다.%s")
			TEXT("SignalTag: Wait/Emit 노드가 주고받는 신호 이름입니다. 같은 Session 또는 SignalRoute 규칙에 따라 전달됩니다.%s")
			TEXT("Controller: UVdjmRecordEventEnsureRecorderControllerNode 또는 SubmitRecorderOptionRequestNode를 통해 생성/조회하는 것이 안전합니다.%s")
			TEXT("RefreshEventSummary 버튼을 누르면 현재 Root events와 Subgraphs의 노드 목록/주요 tag/key 상태를 다시 요약합니다."),
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			LINE_TERMINATOR);
	}

	FString ExportPropertyText(const UObject* eventNode, FName propertyName)
	{
		if (eventNode == nullptr || propertyName.IsNone())
		{
			return FString();
		}

		const FProperty* property = eventNode->GetClass()->FindPropertyByName(propertyName);
		if (property == nullptr)
		{
			return FString();
		}

		FString valueText;
		property->ExportText_InContainer(
			0,
			valueText,
			eventNode,
			nullptr,
			const_cast<UObject*>(eventNode),
			PPF_None);
		valueText.TrimStartAndEndInline();

		if (valueText.IsEmpty() || valueText == TEXT("None") || valueText == TEXT("()"))
		{
			return FString();
		}

		return FString::Printf(TEXT("%s=%s"), *propertyName.ToString(), *valueText);
	}

	void AppendPropertyTextIfSet(const UObject* eventNode, FName propertyName, TArray<FString>& outParts)
	{
		const FString propertyText = ExportPropertyText(eventNode, propertyName);
		if (not propertyText.IsEmpty())
		{
			outParts.Add(propertyText);
		}
	}

	FString MakeEventSummaryLine(
		const UVdjmRecordEventBase* eventNode,
		const FString& ownerLabel,
		int32 eventIndex,
		int32 depth)
	{
		if (eventNode == nullptr)
		{
			return FString::Printf(TEXT("%s[%s #%d] <null>"), *FString::ChrN(depth * 2, TCHAR(' ')), *ownerLabel, eventIndex);
		}

		TArray<FString> parts;
		parts.Add(FString::Printf(TEXT("Class=%s"), *eventNode->GetClass()->GetName()));
		AppendPropertyTextIfSet(eventNode, GET_MEMBER_NAME_CHECKED(UVdjmRecordEventBase, EventTag), parts);

		static const FName RuntimeSlotKeyName(TEXT("RuntimeSlotKey"));
		static const FName ContextKeyName(TEXT("ContextKey"));
		static const FName SignalTagName(TEXT("SignalTag"));
		static const FName EmitSignalTagName(TEXT("EmitSignalTag"));
		static const FName StartSignalTagName(TEXT("StartSignalTag"));
		static const FName WidgetClassName(TEXT("WidgetClass"));
		static const FName ObjectClassName(TEXT("ObjectClass"));
		static const FName ActorClassName(TEXT("ActorClass"));
		static const FName StartPolicyName(TEXT("StartPolicy"));
		static const FName ConditionModeName(TEXT("ConditionMode"));

		AppendPropertyTextIfSet(eventNode, RuntimeSlotKeyName, parts);
		AppendPropertyTextIfSet(eventNode, ContextKeyName, parts);
		AppendPropertyTextIfSet(eventNode, SignalTagName, parts);
		AppendPropertyTextIfSet(eventNode, EmitSignalTagName, parts);
		AppendPropertyTextIfSet(eventNode, StartSignalTagName, parts);
		AppendPropertyTextIfSet(eventNode, WidgetClassName, parts);
		AppendPropertyTextIfSet(eventNode, ObjectClassName, parts);
		AppendPropertyTextIfSet(eventNode, ActorClassName, parts);
		AppendPropertyTextIfSet(eventNode, StartPolicyName, parts);
		AppendPropertyTextIfSet(eventNode, ConditionModeName, parts);

		return FString::Printf(
			TEXT("%s[%s #%d] %s"),
			*FString::ChrN(depth * 2, TCHAR(' ')),
			*ownerLabel,
			eventIndex,
			*FString::Join(parts, TEXT(" | ")));
	}

	bool MatchesSummaryFilter(const FString& line, const FString& filter)
	{
		return filter.TrimStartAndEnd().IsEmpty() || line.Contains(filter, ESearchCase::IgnoreCase);
	}

	void AppendEventSummaryLines(
		const TArray<TObjectPtr<UVdjmRecordEventBase>>& events,
		const FString& ownerLabel,
		const FString& filter,
		int32 depth,
		TArray<FString>& outLines)
	{
		for (int32 eventIndex = 0; eventIndex < events.Num(); ++eventIndex)
		{
			const UVdjmRecordEventBase* eventNode = events[eventIndex];
			const FString line = MakeEventSummaryLine(eventNode, ownerLabel, eventIndex, depth);
			if (MatchesSummaryFilter(line, filter))
			{
				outLines.Add(line);
			}

			const UVdjmRecordEventSequenceNode* sequenceNode = Cast<UVdjmRecordEventSequenceNode>(eventNode);
			if (sequenceNode != nullptr)
			{
				const FString childOwnerLabel = FString::Printf(TEXT("%s #%d Children"), *ownerLabel, eventIndex);
				AppendEventSummaryLines(sequenceNode->Children, childOwnerLabel, filter, depth + 1, outLines);
			}
		}
	}

	void AppendEventSummarySection(
		const FString& sectionLabel,
		const TArray<TObjectPtr<UVdjmRecordEventBase>>& events,
		const FString& filter,
		TArray<FString>& outLines)
	{
		const int32 sectionStartIndex = outLines.Num();
		AppendEventSummaryLines(events, sectionLabel, filter, 0, outLines);
		if (outLines.Num() > sectionStartIndex)
		{
			outLines.Insert(FString::Printf(TEXT("[%s] EventCount=%d"), *sectionLabel, events.Num()), sectionStartIndex);
		}
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

UVdjmRecordEventFlowDataAsset::UVdjmRecordEventFlowDataAsset()
{
	FlowAuthoringGuide = MakeDefaultFlowAuthoringGuide();
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

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventFlowDataAsset::CreateTransientFlowDataAsset(UObject* Outer)
{
	UObject* DataAssetOuter = Outer ? Outer : GetTransientPackage();
	UVdjmRecordEventFlowDataAsset* NewDataAsset = NewObject<UVdjmRecordEventFlowDataAsset>(
		DataAssetOuter,
		NAME_None,
		RF_Transient);
	if (NewDataAsset != nullptr)
	{
		NewDataAsset->InitializeEmpty();
	}

	return NewDataAsset;
}

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventFlowDataAsset::CreateTransientFlowDataAssetFromJsonString(
	UObject* Outer,
	const FString& InJsonString,
	FString& OutError)
{
	OutError.Reset();

	UVdjmRecordEventFlowDataAsset* NewDataAsset = CreateTransientFlowDataAsset(Outer);
	if (NewDataAsset == nullptr)
	{
		OutError = TEXT("Failed to allocate flow data asset.");
		return nullptr;
	}

	if (!NewDataAsset->InitializeFromJsonString(InJsonString, OutError))
	{
		return nullptr;
	}

	return NewDataAsset;
}

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventFlowDataAsset::CreateTransientFlowDataAssetFromFragment(
	UObject* Outer,
	const FVdjmRecordEventFlowFragment& InFragment,
	FString& OutError)
{
	OutError.Reset();

	UVdjmRecordEventFlowDataAsset* NewDataAsset = CreateTransientFlowDataAsset(Outer);
	if (NewDataAsset == nullptr)
	{
		OutError = TEXT("Failed to allocate flow data asset.");
		return nullptr;
	}

	if (!NewDataAsset->ImportFlowFromFragment(InFragment, OutError))
	{
		return nullptr;
	}

	return NewDataAsset;
}

bool UVdjmRecordEventFlowDataAsset::InitializeEmpty()
{
	Events.Reset();
	Subgraphs.Reset();
	ExportedFlowJson.Reset();
	EventSummary.Reset();
	if (FlowAuthoringGuide.IsEmpty())
	{
		FlowAuthoringGuide = MakeDefaultFlowAuthoringGuide();
	}
	return true;
}

bool UVdjmRecordEventFlowDataAsset::InitializeFromJsonString(const FString& InJsonString, FString& OutError)
{
	Events.Reset();
	Subgraphs.Reset();
	ExportedFlowJson.Reset();
	EventSummary.Reset();
	if (FlowAuthoringGuide.IsEmpty())
	{
		FlowAuthoringGuide = MakeDefaultFlowAuthoringGuide();
	}
	return ImportFlowFromJsonString(InJsonString, OutError);
}

bool UVdjmRecordEventFlowDataAsset::ImportFlowFromFragment(const FVdjmRecordEventFlowFragment& InFragment, FString& OutError)
{
	OutError.Reset();

	TSharedPtr<FJsonObject> RootJsonObject;
	if (!InFragment.WriteJsonObject(RootJsonObject, OutError) || !RootJsonObject.IsValid())
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Failed to serialize flow fragment.");
		}
		return false;
	}

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("Failed to write flow fragment json string.");
		return false;
	}

	return ImportFlowFromJsonString(JsonString, OutError);
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

	TArray<TObjectPtr<UVdjmRecordEventBase>> NewEvents;
	if (not DeserializeRequiredEventArrayField(RootObject, TEXT("events"), this, NewEvents, OutError, TEXT("Root event")))
	{
		return false;
	}

	TArray<FVdjmRecordEventSubgraph> NewSubgraphs;
	if (not DeserializeSubgraphsFromJsonObject(RootObject, this, NewSubgraphs, OutError))
	{
		return false;
	}

	Events = MoveTemp(NewEvents);
	Subgraphs = MoveTemp(NewSubgraphs);
	ExportedFlowJson.Reset();
	EventSummary.Reset();
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

	TArray<TObjectPtr<UVdjmRecordEventBase>> dummyEvents;
	if (not DeserializeRequiredEventArrayField(RootObject, TEXT("events"), GetTransientPackage(), dummyEvents, OutError, TEXT("Root event")))
	{
		return false;
	}

	TArray<FVdjmRecordEventSubgraph> dummySubgraphs;
	if (not DeserializeSubgraphsFromJsonObject(RootObject, GetTransientPackage(), dummySubgraphs, OutError))
	{
		return false;
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

int32 UVdjmRecordEventFlowDataAsset::FindSubgraphIndexByTag(FName subgraphTag) const
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

FString UVdjmRecordEventFlowDataAsset::ExportFlowToJsonString(bool bPrettyPrint) const
{
	const TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("asset_name"), GetPathName());
	RootObject->SetNumberField(TEXT("schema_version"), FlowSchemaVersion);

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
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	}

	return OutJson;
}

void UVdjmRecordEventFlowDataAsset::BuildExportedJson()
{
	ExportedFlowJson = ExportFlowToJsonString(true);
}

void UVdjmRecordEventFlowDataAsset::ClearExportedJson()
{
	ExportedFlowJson.Reset();
}

void UVdjmRecordEventFlowDataAsset::ResetAuthoringGuide()
{
	FlowAuthoringGuide = MakeDefaultFlowAuthoringGuide();
	MarkPackageDirty();
}

void UVdjmRecordEventFlowDataAsset::RefreshEventSummary()
{
	TArray<FString> summaryLines;
	const FString filter = EventSummaryFilter.TrimStartAndEnd();

	summaryLines.Add(FString::Printf(TEXT("Asset: %s"), *GetPathName()));
	summaryLines.Add(FString::Printf(TEXT("RootEvents=%d | Subgraphs=%d | Filter=%s"),
		Events.Num(),
		Subgraphs.Num(),
		filter.IsEmpty() ? TEXT("<empty>") : *filter));

	AppendEventSummarySection(TEXT("Root"), Events, filter, summaryLines);

	for (const FVdjmRecordEventSubgraph& subgraph : Subgraphs)
	{
		const FString sectionLabel = subgraph.SubgraphTag.IsNone()
			? TEXT("Subgraph:<None>")
			: FString::Printf(TEXT("Subgraph:%s"), *subgraph.SubgraphTag.ToString());
		AppendEventSummarySection(sectionLabel, subgraph.Events, filter, summaryLines);
	}

	if (summaryLines.Num() <= 2)
	{
		summaryLines.Add(TEXT("No events matched the current summary filter."));
	}

	EventSummary = FString::Join(summaryLines, LINE_TERMINATOR);
}

void UVdjmRecordEventFlowDataAsset::ClearEventSummary()
{
	EventSummary.Reset();
}
