#include "VdjmEvents/VdjmRecordEventJsonHelper.h"

#include "JsonObjectConverter.h"
#include "UObject/UnrealType.h"
#include "VdjmEvents/VdjmRecordEventNode.h"

namespace
{
	constexpr int64 JsonPropertySkipFlags =
		CPF_Transient |
		CPF_DuplicateTransient |
		CPF_NonPIEDuplicateTransient |
		CPF_Deprecated |
		CPF_SkipSerialization;

	bool ShouldSerializeProperty(const FProperty* Property)
	{
		return Property != nullptr && !Property->HasAnyPropertyFlags(static_cast<EPropertyFlags>(JsonPropertySkipFlags));
	}

	bool TryGetEventNodeObjectProperty(const FProperty* Property, const FObjectPropertyBase*& OutObjectProperty)
	{
		OutObjectProperty = CastField<FObjectPropertyBase>(Property);
		return OutObjectProperty != nullptr &&
			OutObjectProperty->PropertyClass != nullptr &&
			OutObjectProperty->PropertyClass->IsChildOf(UVdjmRecordEventBase::StaticClass());
	}

	bool TryGetEventNodeArrayProperty(
		const FProperty* Property,
		const FArrayProperty*& OutArrayProperty,
		const FObjectPropertyBase*& OutInnerObjectProperty)
	{
		OutArrayProperty = CastField<FArrayProperty>(Property);
		if (OutArrayProperty == nullptr)
		{
			OutInnerObjectProperty = nullptr;
			return false;
		}

		OutInnerObjectProperty = CastField<FObjectPropertyBase>(OutArrayProperty->Inner);
		return OutInnerObjectProperty != nullptr &&
			OutInnerObjectProperty->PropertyClass != nullptr &&
			OutInnerObjectProperty->PropertyClass->IsChildOf(UVdjmRecordEventBase::StaticClass());
	}

	bool SerializeEventNodeInternal(
		const UVdjmRecordEventBase* EventNode,
		TSharedPtr<FJsonObject>& OutJsonObject,
		FString& OutError);

	bool DeserializeEventNodeInternal(
		const TSharedPtr<FJsonObject>& EventJsonObject,
		UObject* Outer,
		UVdjmRecordEventBase*& OutEventNode,
		FString& OutError);

	bool SerializePropertyToJson(
		const UVdjmRecordEventBase* EventNode,
		const FProperty* Property,
		const void* ValuePtr,
		const TSharedPtr<FJsonObject>& PropertiesObject,
		FString& OutError)
	{
		if (!ShouldSerializeProperty(Property))
		{
			return true;
		}

		const FString FieldName = Property->GetName();

		const FObjectPropertyBase* ObjectProperty = nullptr;
		if (TryGetEventNodeObjectProperty(Property, ObjectProperty))
		{
			UObject* ChildObject = ObjectProperty->GetObjectPropertyValue(ValuePtr);
			if (const UVdjmRecordEventBase* ChildEventNode = Cast<UVdjmRecordEventBase>(ChildObject))
			{
				TSharedPtr<FJsonObject> ChildEventJson;
				if (!SerializeEventNodeInternal(ChildEventNode, ChildEventJson, OutError))
				{
					OutError = FString::Printf(TEXT("Failed to serialize child object property '%s': %s"), *FieldName, *OutError);
					return false;
				}
				PropertiesObject->SetObjectField(FieldName, ChildEventJson);
			}
			else
			{
				PropertiesObject->SetField(FieldName, MakeShared<FJsonValueNull>());
			}

			return true;
		}

		const FArrayProperty* ArrayProperty = nullptr;
		const FObjectPropertyBase* InnerObjectProperty = nullptr;
		if (TryGetEventNodeArrayProperty(Property, ArrayProperty, InnerObjectProperty))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
			TArray<TSharedPtr<FJsonValue>> JsonChildren;
			JsonChildren.Reserve(ArrayHelper.Num());

			for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
			{
				const void* ElementValuePtr = ArrayHelper.GetRawPtr(Index);
				UObject* ElementObject = InnerObjectProperty->GetObjectPropertyValue(ElementValuePtr);
				const UVdjmRecordEventBase* ElementEventNode = Cast<UVdjmRecordEventBase>(ElementObject);
				if (ElementEventNode == nullptr)
				{
					JsonChildren.Add(MakeShared<FJsonValueNull>());
					continue;
				}

				TSharedPtr<FJsonObject> ElementJsonObject;
				if (!SerializeEventNodeInternal(ElementEventNode, ElementJsonObject, OutError))
				{
					OutError = FString::Printf(TEXT("Failed to serialize element %d of '%s': %s"), Index, *FieldName, *OutError);
					return false;
				}

				JsonChildren.Add(MakeShared<FJsonValueObject>(ElementJsonObject));
			}

			PropertiesObject->SetArrayField(FieldName, JsonChildren);
			return true;
		}

		TSharedPtr<FJsonValue> JsonValue = FJsonObjectConverter::UPropertyToJsonValue(
			const_cast<FProperty*>(Property),
			ValuePtr,
			0,
			JsonPropertySkipFlags,
			nullptr);
		if (!JsonValue.IsValid())
		{
			OutError = FString::Printf(
				TEXT("Unsupported property for json serialization. Class='%s' Property='%s'"),
				*EventNode->GetClass()->GetPathName(),
				*FieldName);
			return false;
		}

		PropertiesObject->SetField(FieldName, JsonValue);
		return true;
	}

	bool DeserializePropertyFromJson(
		UVdjmRecordEventBase* EventNode,
		const FProperty* Property,
		void* ValuePtr,
		const TSharedPtr<FJsonObject>& PropertiesObject,
		FString& OutError)
	{
		if (!ShouldSerializeProperty(Property))
		{
			return true;
		}

		const FString FieldName = Property->GetName();
		const TSharedPtr<FJsonValue>* JsonValuePtr = PropertiesObject->Values.Find(FieldName);
		if (JsonValuePtr == nullptr || !JsonValuePtr->IsValid())
		{
			return true;
		}

		const TSharedPtr<FJsonValue>& JsonValue = *JsonValuePtr;

		const FObjectPropertyBase* ObjectProperty = nullptr;
		if (TryGetEventNodeObjectProperty(Property, ObjectProperty))
		{
			if (JsonValue->Type == EJson::Null)
			{
				ObjectProperty->SetObjectPropertyValue(ValuePtr, nullptr);
				return true;
			}

			if (JsonValue->Type != EJson::Object)
			{
				OutError = FString::Printf(TEXT("Property '%s' must be an object node."), *FieldName);
				return false;
			}

			UVdjmRecordEventBase* ChildEventNode = nullptr;
			if (!DeserializeEventNodeInternal(JsonValue->AsObject(), EventNode, ChildEventNode, OutError))
			{
				OutError = FString::Printf(TEXT("Failed to deserialize child object property '%s': %s"), *FieldName, *OutError);
				return false;
			}

			ObjectProperty->SetObjectPropertyValue(ValuePtr, ChildEventNode);
			return true;
		}

		const FArrayProperty* ArrayProperty = nullptr;
		const FObjectPropertyBase* InnerObjectProperty = nullptr;
		if (TryGetEventNodeArrayProperty(Property, ArrayProperty, InnerObjectProperty))
		{
			if (JsonValue->Type != EJson::Array)
			{
				OutError = FString::Printf(TEXT("Property '%s' must be an array of event nodes."), *FieldName);
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>>& JsonArray = JsonValue->AsArray();
			FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
			ArrayHelper.Resize(JsonArray.Num());

			for (int32 Index = 0; Index < JsonArray.Num(); ++Index)
			{
				void* ElementValuePtr = ArrayHelper.GetRawPtr(Index);
				const TSharedPtr<FJsonValue>& ElementValue = JsonArray[Index];
				if (!ElementValue.IsValid() || ElementValue->Type == EJson::Null)
				{
					InnerObjectProperty->SetObjectPropertyValue(ElementValuePtr, nullptr);
					continue;
				}

				if (ElementValue->Type != EJson::Object)
				{
					OutError = FString::Printf(TEXT("Property '%s' index %d must be an object node."), *FieldName, Index);
					return false;
				}

				UVdjmRecordEventBase* ChildEventNode = nullptr;
				if (!DeserializeEventNodeInternal(ElementValue->AsObject(), EventNode, ChildEventNode, OutError))
				{
					OutError = FString::Printf(TEXT("Failed to deserialize '%s' index %d: %s"), *FieldName, Index, *OutError);
					return false;
				}

				InnerObjectProperty->SetObjectPropertyValue(ElementValuePtr, ChildEventNode);
			}

			return true;
		}

		FText FailureReason;
		const bool bConverted = FJsonObjectConverter::JsonValueToUProperty(
			JsonValue,
			const_cast<FProperty*>(Property),
			ValuePtr,
			0,
			JsonPropertySkipFlags,
			false,
			&FailureReason,
			nullptr);
		if (!bConverted)
		{
			OutError = FString::Printf(
				TEXT("Failed to convert property '%s'. Reason: %s"),
				*FieldName,
				*FailureReason.ToString());
			return false;
		}

		return true;
	}

	void CopyLegacyFieldIfExists(
		const TSharedPtr<FJsonObject>& SourceObject,
		const TCHAR* SourceKey,
		const TSharedPtr<FJsonObject>& DestinationObject,
		const TCHAR* DestinationKey)
	{
		if (!SourceObject.IsValid() || !DestinationObject.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue>* JsonValuePtr = SourceObject->Values.Find(SourceKey);
		if (JsonValuePtr != nullptr && JsonValuePtr->IsValid())
		{
			DestinationObject->SetField(DestinationKey, *JsonValuePtr);
		}
	}

	TSharedPtr<FJsonObject> ConvertLegacyEventToModernSchema(const TSharedPtr<FJsonObject>& LegacyEventObject)
	{
		if (!LegacyEventObject.IsValid())
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject> ModernEventObject = MakeShared<FJsonObject>();

		FString ClassPath;
		if (LegacyEventObject->TryGetStringField(TEXT("class"), ClassPath))
		{
			ModernEventObject->SetStringField(TEXT("class"), ClassPath);
		}

		const TSharedPtr<FJsonObject> PropertiesObject = MakeShared<FJsonObject>();
		ModernEventObject->SetObjectField(TEXT("properties"), PropertiesObject);

		FString TagString;
		if (LegacyEventObject->TryGetStringField(TEXT("tag"), TagString))
		{
			PropertiesObject->SetStringField(TEXT("EventTag"), TagString);
		}

		const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
		if (LegacyEventObject->TryGetArrayField(TEXT("children"), ChildrenArray) && ChildrenArray != nullptr)
		{
			PropertiesObject->SetArrayField(TEXT("Children"), *ChildrenArray);
		}

		const TSharedPtr<FJsonObject>* LegacyConfigObjectPtr = nullptr;
		if (LegacyEventObject->TryGetObjectField(TEXT("config"), LegacyConfigObjectPtr) && LegacyConfigObjectPtr != nullptr)
		{
			const TSharedPtr<FJsonObject>& LegacyConfigObject = *LegacyConfigObjectPtr;

			CopyLegacyFieldIfExists(LegacyConfigObject, TEXT("target_class"), PropertiesObject, TEXT("TargetClass"));
			CopyLegacyFieldIfExists(LegacyConfigObject, TEXT("target_tag"), PropertiesObject, TEXT("TargetTag"));
			CopyLegacyFieldIfExists(LegacyConfigObject, TEXT("abort_if_not_found"), PropertiesObject, TEXT("bAbortIfNotFound"));
			CopyLegacyFieldIfExists(LegacyConfigObject, TEXT("reuse_existing_bridge_actor"), PropertiesObject, TEXT("bReuseExistingBridgeActor"));
			CopyLegacyFieldIfExists(LegacyConfigObject, TEXT("bridge_actor_class"), PropertiesObject, TEXT("BridgeActorClass"));
			CopyLegacyFieldIfExists(LegacyConfigObject, TEXT("env_data_asset_path"), PropertiesObject, TEXT("EnvDataAssetPath"));
			CopyLegacyFieldIfExists(LegacyConfigObject, TEXT("require_load_success"), PropertiesObject, TEXT("bRequireLoadSuccess"));
		}

		return ModernEventObject;
	}

	bool SerializeEventNodeInternal(
		const UVdjmRecordEventBase* EventNode,
		TSharedPtr<FJsonObject>& OutJsonObject,
		FString& OutError)
	{
		OutJsonObject = nullptr;
		if (EventNode == nullptr)
		{
			OutError = TEXT("Event node is null.");
			return false;
		}

		OutJsonObject = MakeShared<FJsonObject>();
		OutJsonObject->SetStringField(TEXT("class"), EventNode->GetClass()->GetPathName());

		const TSharedPtr<FJsonObject> PropertiesObject = MakeShared<FJsonObject>();
		OutJsonObject->SetObjectField(TEXT("properties"), PropertiesObject);

		for (TFieldIterator<FProperty> PropertyIt(EventNode->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(EventNode);
			if (!SerializePropertyToJson(EventNode, Property, ValuePtr, PropertiesObject, OutError))
			{
				return false;
			}
		}

		return true;
	}

	bool DeserializeEventNodeInternal(
		const TSharedPtr<FJsonObject>& EventJsonObject,
		UObject* Outer,
		UVdjmRecordEventBase*& OutEventNode,
		FString& OutError)
	{
		OutEventNode = nullptr;

		if (!EventJsonObject.IsValid())
		{
			OutError = TEXT("Event object is invalid.");
			return false;
		}

		FString ClassPath;
		if (!EventJsonObject->TryGetStringField(TEXT("class"), ClassPath) || ClassPath.IsEmpty())
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

		if (Outer == nullptr)
		{
			OutError = TEXT("Event node outer is null.");
			return false;
		}

		UVdjmRecordEventBase* NewEventNode = NewObject<UVdjmRecordEventBase>(Outer, EventClass);
		if (NewEventNode == nullptr)
		{
			OutError = FString::Printf(TEXT("Failed to instantiate event class: %s"), *ClassPath);
			return false;
		}

		const TSharedPtr<FJsonObject>* PropertiesObjectPtr = nullptr;
		if (!EventJsonObject->TryGetObjectField(TEXT("properties"), PropertiesObjectPtr) || PropertiesObjectPtr == nullptr)
		{
			const TSharedPtr<FJsonObject> ConvertedObject = ConvertLegacyEventToModernSchema(EventJsonObject);
			if (!ConvertedObject.IsValid())
			{
				OutError = TEXT("Failed to convert legacy event schema.");
				return false;
			}

			if (!ConvertedObject->TryGetObjectField(TEXT("properties"), PropertiesObjectPtr) || PropertiesObjectPtr == nullptr)
			{
				OutError = TEXT("Missing 'properties' in converted legacy event schema.");
				return false;
			}
		}

		const TSharedPtr<FJsonObject>& PropertiesObject = *PropertiesObjectPtr;
		for (TFieldIterator<FProperty> PropertyIt(EventClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(NewEventNode);
			if (!DeserializePropertyFromJson(NewEventNode, Property, ValuePtr, PropertiesObject, OutError))
			{
				OutError = FString::Printf(TEXT("Property deserialize failed '%s': %s"), *Property->GetName(), *OutError);
				return false;
			}
		}

		OutEventNode = NewEventNode;
		return true;
	}
}

namespace VdjmRecordEventJson
{
	bool SerializeEventNodeToJsonObject(
		const UVdjmRecordEventBase* EventNode,
		TSharedPtr<FJsonObject>& OutJsonObject,
		FString& OutError)
	{
		OutError.Reset();
		return SerializeEventNodeInternal(EventNode, OutJsonObject, OutError);
	}

	bool DeserializeEventNodeFromJsonObject(
		const TSharedPtr<FJsonObject>& EventJsonObject,
		UObject* Outer,
		UVdjmRecordEventBase*& OutEventNode,
		FString& OutError)
	{
		OutError.Reset();
		return DeserializeEventNodeInternal(EventJsonObject, Outer, OutEventNode, OutError);
	}
}

