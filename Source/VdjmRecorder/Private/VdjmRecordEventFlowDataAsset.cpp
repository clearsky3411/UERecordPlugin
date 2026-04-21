#include "VdjmRecordEventFlowDataAsset.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmRecordEventNode.h"

namespace
{
	TSharedPtr<FJsonObject> BuildEventJsonObject(const UVdjmRecordEventBase* Event)
	{
		if (Event == nullptr)
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject> EventObject = MakeShared<FJsonObject>();
		EventObject->SetStringField(TEXT("class"), Event->GetClass()->GetPathName());
		EventObject->SetStringField(TEXT("tag"), Event->EventTag.ToString());

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
			EventObject->SetStringField(TEXT("target_class"), TargetClassPath);
			EventObject->SetStringField(TEXT("target_tag"), SelectorNode->TargetTag.ToString());
			EventObject->SetBoolField(TEXT("abort_if_not_found"), SelectorNode->bAbortIfNotFound);
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
