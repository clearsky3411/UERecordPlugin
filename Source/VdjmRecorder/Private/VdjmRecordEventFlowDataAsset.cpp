#include "VdjmRecordEventFlowDataAsset.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmRecordBridgeActor.h"
#include "VdjmRecordEventNode.h"

namespace
{
	constexpr int32 FlowSchemaVersion = 1;

	bool ParseEventNode(
		const TSharedPtr<FJsonObject>& EventObject,
		UObject* Outer,
		UVdjmRecordEventBase*& OutEvent,
		FString& OutError)
	{
		OutEvent = nullptr;
		if (!EventObject.IsValid())
		{
			OutError = TEXT("Event object is invalid.");
			return false;
		}

		FString ClassPath;
		if (!EventObject->TryGetStringField(TEXT("class"), ClassPath) || ClassPath.IsEmpty())
		{
			OutError = TEXT("Missing 'class' in event object.");
			return false;
		}

		UClass* EventClass = FindObject<UClass>(nullptr, *ClassPath);
		if (EventClass == nullptr)
		{
			EventClass = LoadObject<UClass>(nullptr, *ClassPath);
		}
		if (EventClass == nullptr || !EventClass->IsChildOf(UVdjmRecordEventBase::StaticClass()))
		{
			OutError = FString::Printf(TEXT("Invalid event class: %s"), *ClassPath);
			return false;
		}

		UVdjmRecordEventBase* CreatedEvent = NewObject<UVdjmRecordEventBase>(Outer, EventClass);
		if (CreatedEvent == nullptr)
		{
			OutError = FString::Printf(TEXT("Failed to create event node: %s"), *ClassPath);
			return false;
		}

		FString TagString;
		if (EventObject->TryGetStringField(TEXT("tag"), TagString))
		{
			CreatedEvent->EventTag = FName(*TagString);
		}

		const TSharedPtr<FJsonObject>* ConfigObjectPtr = nullptr;
		if (EventObject->TryGetObjectField(TEXT("config"), ConfigObjectPtr))
		{
			const TSharedPtr<FJsonObject>& ConfigObject = *ConfigObjectPtr;
			if (UVdjmRecordEventSelectorNode* SelectorNode = Cast<UVdjmRecordEventSelectorNode>(CreatedEvent))
			{
				FString TargetClassPath;
				if (ConfigObject->TryGetStringField(TEXT("target_class"), TargetClassPath) && !TargetClassPath.IsEmpty())
				{
					UClass* TargetClass = FindObject<UClass>(nullptr, *TargetClassPath);
					if (TargetClass == nullptr)
					{
						TargetClass = LoadObject<UClass>(nullptr, *TargetClassPath);
					}
					if (TargetClass != nullptr && TargetClass->IsChildOf(UVdjmRecordEventBase::StaticClass()))
					{
						SelectorNode->TargetClass = TargetClass;
					}
				}

				FString TargetTag;
				if (ConfigObject->TryGetStringField(TEXT("target_tag"), TargetTag))
				{
					SelectorNode->TargetTag = FName(*TargetTag);
				}

				ConfigObject->TryGetBoolField(TEXT("abort_if_not_found"), SelectorNode->bAbortIfNotFound);
			}
			else if (UVdjmRecordEventSpawnBridgeActorNode* SpawnNode = Cast<UVdjmRecordEventSpawnBridgeActorNode>(CreatedEvent))
			{
				ConfigObject->TryGetBoolField(TEXT("reuse_existing_bridge_actor"), SpawnNode->bReuseExistingBridgeActor);

				FString SpawnClassPath;
				if (ConfigObject->TryGetStringField(TEXT("bridge_actor_class"), SpawnClassPath) && !SpawnClassPath.IsEmpty())
				{
					UClass* SpawnClass = FindObject<UClass>(nullptr, *SpawnClassPath);
					if (SpawnClass == nullptr)
					{
						SpawnClass = LoadObject<UClass>(nullptr, *SpawnClassPath);
					}
					if (SpawnClass != nullptr && SpawnClass->IsChildOf(AVdjmRecordBridgeActor::StaticClass()))
					{
						SpawnNode->BridgeActorClass = SpawnClass;
					}
				}
			}
			else if (UVdjmRecordEventSetEnvDataAssetPathNode* SetPathNode = Cast<UVdjmRecordEventSetEnvDataAssetPathNode>(CreatedEvent))
			{
				FString EnvPath;
				if (ConfigObject->TryGetStringField(TEXT("env_data_asset_path"), EnvPath))
				{
					SetPathNode->EnvDataAssetPath = FSoftObjectPath(EnvPath);
				}
				ConfigObject->TryGetBoolField(TEXT("require_load_success"), SetPathNode->bRequireLoadSuccess);
			}
		}

		if (UVdjmRecordEventSequenceNode* SequenceNode = Cast<UVdjmRecordEventSequenceNode>(CreatedEvent))
		{
			const TArray<TSharedPtr<FJsonValue>>* ChildrenValues = nullptr;
			if (EventObject->TryGetArrayField(TEXT("children"), ChildrenValues) && ChildrenValues != nullptr)
			{
				for (int32 ChildIndex = 0; ChildIndex < ChildrenValues->Num(); ++ChildIndex)
				{
					const TSharedPtr<FJsonValue>& ChildValue = (*ChildrenValues)[ChildIndex];
					if (!ChildValue.IsValid() || ChildValue->Type != EJson::Object)
					{
						OutError = FString::Printf(TEXT("Invalid child node at index %d."), ChildIndex);
						return false;
					}

					UVdjmRecordEventBase* ChildNode = nullptr;
					if (!ParseEventNode(ChildValue->AsObject(), SequenceNode, ChildNode, OutError))
					{
						return false;
					}
					SequenceNode->Children.Add(ChildNode);
				}
			}
		}

		OutEvent = CreatedEvent;
		return true;
	}

	TSharedPtr<FJsonObject> BuildEventJsonObject(const UVdjmRecordEventBase* Event)
	{
		if (Event == nullptr)
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject> EventObject = MakeShared<FJsonObject>();
		EventObject->SetStringField(TEXT("class"), Event->GetClass()->GetPathName());
		EventObject->SetStringField(TEXT("tag"), Event->EventTag.ToString());
		EventObject->SetNumberField(TEXT("schema_version"), FlowSchemaVersion);
		const TSharedPtr<FJsonObject> ConfigObject = MakeShared<FJsonObject>();
		EventObject->SetObjectField(TEXT("config"), ConfigObject);

		if (const UVdjmRecordEventSequenceNode* SequenceNode = Cast<UVdjmRecordEventSequenceNode>(Event))
		{
			TArray<TSharedPtr<FJsonValue>> ChildrenValues;
			for (const UVdjmRecordEventBase* Child : SequenceNode->Children)
			{
				if (const TSharedPtr<FJsonObject> ChildObject = BuildEventJsonObject(Child))
				{
					ChildrenValues.Add(MakeShared<FJsonValueObject>(ChildObject));
				}
			}

			EventObject->SetArrayField(TEXT("children"), ChildrenValues);
		}
		else if (const UVdjmRecordEventSelectorNode* SelectorNode = Cast<UVdjmRecordEventSelectorNode>(Event))
		{
			const FString TargetClassPath = SelectorNode->TargetClass ? SelectorNode->TargetClass->GetPathName() : FString();
			ConfigObject->SetStringField(TEXT("target_class"), TargetClassPath);
			ConfigObject->SetStringField(TEXT("target_tag"), SelectorNode->TargetTag.ToString());
			ConfigObject->SetBoolField(TEXT("abort_if_not_found"), SelectorNode->bAbortIfNotFound);
		}
		else if (const UVdjmRecordEventSpawnBridgeActorNode* SpawnNode = Cast<UVdjmRecordEventSpawnBridgeActorNode>(Event))
		{
			const FString SpawnClassPath = SpawnNode->BridgeActorClass ? SpawnNode->BridgeActorClass->GetPathName() : FString();
			ConfigObject->SetBoolField(TEXT("reuse_existing_bridge_actor"), SpawnNode->bReuseExistingBridgeActor);
			ConfigObject->SetStringField(TEXT("bridge_actor_class"), SpawnClassPath);
		}
		else if (const UVdjmRecordEventSetEnvDataAssetPathNode* SetPathNode = Cast<UVdjmRecordEventSetEnvDataAssetPathNode>(Event))
		{
			ConfigObject->SetStringField(TEXT("env_data_asset_path"), SetPathNode->EnvDataAssetPath.ToString());
			ConfigObject->SetBoolField(TEXT("require_load_success"), SetPathNode->bRequireLoadSuccess);
		}

		return EventObject;
	}
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
		if (!ParseEventNode(EventValue->AsObject(), this, NewNode, OutError))
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
		if (!ParseEventNode(EventValue->AsObject(), GetTransientPackage(), DummyNode, OutError))
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
		if (const TSharedPtr<FJsonObject> EventObject = BuildEventJsonObject(Event))
		{
			EventValues.Add(MakeShared<FJsonValueObject>(EventObject));
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
