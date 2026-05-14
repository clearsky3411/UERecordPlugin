#include "VdjmReflection/VdjmReflectionUiBlueprintLibrary.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmRecorderController.h"

namespace
{
	const FName ActionPropertyName(TEXT("Action"));
	const FName ValuePropertyName(TEXT("Value"));

	bool HasPropertyMetadata(const FProperty* property, const TCHAR* metadataKey)
	{
#if WITH_METADATA
		return property != nullptr && property->HasMetaData(metadataKey);
#else
		return false;
#endif
	}

	FString GetPropertyMetadata(const FProperty* property, const TCHAR* metadataKey)
	{
#if WITH_METADATA
		return property != nullptr ? property->GetMetaData(metadataKey) : FString();
#else
		return FString();
#endif
	}

	bool HasEnumMetadata(const UEnum* enumObject, const TCHAR* metadataKey, int32 index)
	{
#if WITH_METADATA
		return enumObject != nullptr && enumObject->HasMetaData(metadataKey, index);
#else
		return false;
#endif
	}

	FString GetPropertyDisplayName(const FProperty* property)
	{
		if (property == nullptr)
		{
			return FString();
		}

		const FString displayName = GetPropertyMetadata(property, TEXT("DisplayName"));
		return displayName.IsEmpty() ? property->GetName() : displayName;
	}

	FName GetPropertyCategory(const FProperty* property)
	{
		if (property == nullptr)
		{
			return NAME_None;
		}

		const FString category = GetPropertyMetadata(property, TEXT("Category"));
		return category.IsEmpty() ? NAME_None : FName(*category);
	}

	bool TryReadFloatMetadata(const FProperty* property, const TCHAR* metadataKey, float& outValue)
	{
		outValue = 0.0f;
		if (property == nullptr || not HasPropertyMetadata(property, metadataKey))
		{
			return false;
		}

		const FString metadataValue = GetPropertyMetadata(property, metadataKey);
		if (metadataValue.IsEmpty())
		{
			return false;
		}

		outValue = FCString::Atof(*metadataValue);
		return true;
	}

	int32 ReadIntMetadata(const FProperty* property, const TCHAR* metadataKey, int32 defaultValue)
	{
		if (property == nullptr || not HasPropertyMetadata(property, metadataKey))
		{
			return defaultValue;
		}

		return FCString::Atoi(*GetPropertyMetadata(property, metadataKey));
	}

	bool ReadBoolMetadata(const FProperty* property, const TCHAR* metadataKey)
	{
		if (property == nullptr || not HasPropertyMetadata(property, metadataKey))
		{
			return false;
		}

		const FString metadataValue = GetPropertyMetadata(property, metadataKey);
		return metadataValue.IsEmpty() ||
			metadataValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
			metadataValue.Equals(TEXT("1"), ESearchCase::IgnoreCase);
	}

	EVdjmReflectionUiValueType ResolveValueType(const FProperty* valueProperty)
	{
		if (valueProperty == nullptr)
		{
			return EVdjmReflectionUiValueType::EUnsupported;
		}

		if (CastField<FIntProperty>(valueProperty) != nullptr)
		{
			return EVdjmReflectionUiValueType::EInt;
		}

		if (CastField<FFloatProperty>(valueProperty) != nullptr ||
			CastField<FDoubleProperty>(valueProperty) != nullptr)
		{
			return EVdjmReflectionUiValueType::EFloat;
		}

		if (CastField<FBoolProperty>(valueProperty) != nullptr)
		{
			return EVdjmReflectionUiValueType::EBool;
		}

		if (CastField<FStrProperty>(valueProperty) != nullptr ||
			CastField<FNameProperty>(valueProperty) != nullptr ||
			CastField<FTextProperty>(valueProperty) != nullptr)
		{
			return EVdjmReflectionUiValueType::EString;
		}

		if (CastField<FEnumProperty>(valueProperty) != nullptr)
		{
			return EVdjmReflectionUiValueType::EEnum;
		}

		if (const FByteProperty* byteProperty = CastField<FByteProperty>(valueProperty))
		{
			return byteProperty->Enum != nullptr
				? EVdjmReflectionUiValueType::EEnum
				: EVdjmReflectionUiValueType::EInt;
		}

		if (const FStructProperty* structProperty = CastField<FStructProperty>(valueProperty))
		{
			if (structProperty->Struct == TBaseStructure<FIntPoint>::Get())
			{
				return EVdjmReflectionUiValueType::EIntPoint;
			}
		}

		return EVdjmReflectionUiValueType::EUnsupported;
	}

	UEnum* ResolveValueEnum(const FProperty* valueProperty)
	{
		if (const FEnumProperty* enumProperty = CastField<FEnumProperty>(valueProperty))
		{
			return enumProperty->GetEnum();
		}

		if (const FByteProperty* byteProperty = CastField<FByteProperty>(valueProperty))
		{
			return byteProperty->Enum;
		}

		return nullptr;
	}

	void FillEnumOptions(const FProperty* valueProperty, TArray<FString>& outEnumOptions)
	{
		outEnumOptions.Reset();

		const UEnum* enumObject = ResolveValueEnum(valueProperty);
		if (enumObject == nullptr)
		{
			return;
		}

		for (int32 index = 0; index < enumObject->NumEnums(); ++index)
		{
			if (HasEnumMetadata(enumObject, TEXT("Hidden"), index))
			{
				continue;
			}

			const FString enumName = enumObject->GetNameStringByIndex(index);
			if (enumName.EndsWith(TEXT("_MAX")))
			{
				continue;
			}

			outEnumOptions.Add(enumName);
		}
	}

	FString GetDefaultValueAsString(UScriptStruct* ownerStruct, const FStructProperty* messageProperty, const FProperty* valueProperty)
	{
		if (ownerStruct == nullptr || messageProperty == nullptr || valueProperty == nullptr)
		{
			return FString();
		}

		TArray<uint8> structMemory;
		structMemory.SetNumZeroed(ownerStruct->GetStructureSize());
		ownerStruct->InitializeStruct(structMemory.GetData());

		FString valueString;
		const void* messageValuePtr = messageProperty->ContainerPtrToValuePtr<void>(structMemory.GetData());
		const void* valuePtr = valueProperty->ContainerPtrToValuePtr<void>(messageValuePtr);
		valueProperty->ExportTextItem_Direct(valueString, valuePtr, nullptr, nullptr, PPF_None);

		ownerStruct->DestroyStruct(structMemory.GetData());
		return valueString;
	}

	UVdjmReflectionUiItemObject* BuildItemFromProperty(
		UObject* outer,
		UScriptStruct* ownerStruct,
		const FStructProperty* messageProperty)
	{
		if (outer == nullptr || ownerStruct == nullptr || messageProperty == nullptr || messageProperty->Struct == nullptr)
		{
			return nullptr;
		}

		FProperty* actionProperty = messageProperty->Struct->FindPropertyByName(ActionPropertyName);
		FProperty* valueProperty = messageProperty->Struct->FindPropertyByName(ValuePropertyName);
		if (actionProperty == nullptr || valueProperty == nullptr)
		{
			return nullptr;
		}

		if (ReadBoolMetadata(messageProperty, TEXT("ReflectionUiHidden")))
		{
			return nullptr;
		}

		UVdjmReflectionUiItemObject* itemObject = NewObject<UVdjmReflectionUiItemObject>(outer);
		if (itemObject == nullptr)
		{
			return nullptr;
		}

		itemObject->OptionKey = messageProperty->GetFName();
		itemObject->PropertyPath = messageProperty->GetName();
		itemObject->DisplayName = FText::FromString(GetPropertyDisplayName(messageProperty));
		itemObject->Category = GetPropertyCategory(messageProperty);
		itemObject->ValueType = ResolveValueType(valueProperty);
		itemObject->ValuePropertyName = valueProperty->GetName();
		itemObject->UiType = HasPropertyMetadata(messageProperty, TEXT("ReflectionUiType"))
			? FName(*GetPropertyMetadata(messageProperty, TEXT("ReflectionUiType")))
			: NAME_None;
		itemObject->SortOrder = ReadIntMetadata(messageProperty, TEXT("ReflectionUiSortOrder"), 0);
		itemObject->bAdvanced = ReadBoolMetadata(messageProperty, TEXT("ReflectionUiAdvanced"));
		itemObject->bHidden = false;
		itemObject->bHasMinValue = TryReadFloatMetadata(messageProperty, TEXT("ClampMin"), itemObject->MinValue);
		itemObject->bHasMaxValue = TryReadFloatMetadata(messageProperty, TEXT("ClampMax"), itemObject->MaxValue);
		itemObject->ValueAsString = GetDefaultValueAsString(ownerStruct, messageProperty, valueProperty);
		FillEnumOptions(valueProperty, itemObject->EnumOptions);
		return itemObject;
	}

	TSharedPtr<FJsonObject> BuildJsonObjectFromItem(const UVdjmReflectionUiItemObject* itemObject)
	{
		TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		if (itemObject == nullptr)
		{
			return jsonObject;
		}

		jsonObject->SetStringField(TEXT("option_key"), itemObject->OptionKey.ToString());
		jsonObject->SetStringField(TEXT("property_path"), itemObject->PropertyPath);
		jsonObject->SetStringField(TEXT("display_name"), itemObject->DisplayName.ToString());
		jsonObject->SetStringField(TEXT("category"), itemObject->Category.ToString());
		jsonObject->SetStringField(TEXT("value_type"), itemObject->GetValueTypeName());
		jsonObject->SetStringField(TEXT("value_property_name"), itemObject->ValuePropertyName);
		jsonObject->SetStringField(TEXT("default_value"), itemObject->ValueAsString);
		jsonObject->SetStringField(TEXT("ui_type"), itemObject->UiType.ToString());
		jsonObject->SetNumberField(TEXT("sort_order"), itemObject->SortOrder);
		jsonObject->SetBoolField(TEXT("advanced"), itemObject->bAdvanced);
		jsonObject->SetBoolField(TEXT("hidden"), itemObject->bHidden);

		if (itemObject->bHasMinValue)
		{
			jsonObject->SetNumberField(TEXT("min"), itemObject->MinValue);
		}

		if (itemObject->bHasMaxValue)
		{
			jsonObject->SetNumberField(TEXT("max"), itemObject->MaxValue);
		}

		TArray<TSharedPtr<FJsonValue>> enumValues;
		enumValues.Reserve(itemObject->EnumOptions.Num());
		for (const FString& enumOption : itemObject->EnumOptions)
		{
			enumValues.Add(MakeShared<FJsonValueString>(enumOption));
		}
		jsonObject->SetArrayField(TEXT("enum_options"), enumValues);
		return jsonObject;
	}

	EVdjmReflectionUiValueType ParseReflectionValueTypeName(const FString& valueTypeName)
	{
		const FString normalizedValue = valueTypeName.TrimStartAndEnd();
		if (normalizedValue.Equals(TEXT("Int"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("EInt"), ESearchCase::IgnoreCase))
		{
			return EVdjmReflectionUiValueType::EInt;
		}
		if (normalizedValue.Equals(TEXT("Float"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("EFloat"), ESearchCase::IgnoreCase))
		{
			return EVdjmReflectionUiValueType::EFloat;
		}
		if (normalizedValue.Equals(TEXT("Bool"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("EBool"), ESearchCase::IgnoreCase))
		{
			return EVdjmReflectionUiValueType::EBool;
		}
		if (normalizedValue.Equals(TEXT("String"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("EString"), ESearchCase::IgnoreCase))
		{
			return EVdjmReflectionUiValueType::EString;
		}
		if (normalizedValue.Equals(TEXT("Enum"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("EEnum"), ESearchCase::IgnoreCase))
		{
			return EVdjmReflectionUiValueType::EEnum;
		}
		if (normalizedValue.Equals(TEXT("IntPoint"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("EIntPoint"), ESearchCase::IgnoreCase))
		{
			return EVdjmReflectionUiValueType::EIntPoint;
		}

		return EVdjmReflectionUiValueType::EUnsupported;
	}

	UVdjmReflectionUiItemObject* BuildItemFromJsonObject(
		UObject* outer,
		const TSharedPtr<FJsonObject>& itemObjectJson,
		FString& outErrorReason)
	{
		outErrorReason.Reset();
		if (outer == nullptr || not itemObjectJson.IsValid())
		{
			outErrorReason = TEXT("Reflection UI schema item is invalid.");
			return nullptr;
		}

		FString optionKeyString;
		if (not itemObjectJson->TryGetStringField(TEXT("option_key"), optionKeyString) || optionKeyString.TrimStartAndEnd().IsEmpty())
		{
			outErrorReason = TEXT("Reflection UI schema item has no option_key.");
			return nullptr;
		}

		UVdjmReflectionUiItemObject* itemObject = NewObject<UVdjmReflectionUiItemObject>(outer);
		if (itemObject == nullptr)
		{
			outErrorReason = FString::Printf(TEXT("Failed to create Reflection UI item. OptionKey=%s"), *optionKeyString);
			return nullptr;
		}

		FString displayName;
		FString category;
		FString valueTypeName;
		FString uiTypeName;
		FString valueAsString;
		FString valuePropertyName;
		double minValue = 0.0;
		double maxValue = 0.0;
		double sortOrder = 0.0;
		bool bAdvanced = false;
		bool bHidden = false;

		itemObject->OptionKey = FName(*optionKeyString.TrimStartAndEnd());
		itemObjectJson->TryGetStringField(TEXT("property_path"), itemObject->PropertyPath);
		if (itemObject->PropertyPath.IsEmpty())
		{
			itemObject->PropertyPath = optionKeyString.TrimStartAndEnd();
		}

		itemObjectJson->TryGetStringField(TEXT("display_name"), displayName);
		itemObject->DisplayName = FText::FromString(displayName.IsEmpty() ? optionKeyString : displayName);
		itemObjectJson->TryGetStringField(TEXT("category"), category);
		itemObject->Category = category.IsEmpty() ? NAME_None : FName(*category);
		itemObjectJson->TryGetStringField(TEXT("value_type"), valueTypeName);
		itemObject->ValueType = ParseReflectionValueTypeName(valueTypeName);
		itemObjectJson->TryGetStringField(TEXT("value_property_name"), valuePropertyName);
		itemObject->ValuePropertyName = valuePropertyName.IsEmpty() ? TEXT("Value") : valuePropertyName;
		itemObjectJson->TryGetStringField(TEXT("default_value"), valueAsString);
		itemObject->ValueAsString = valueAsString;
		itemObjectJson->TryGetStringField(TEXT("ui_type"), uiTypeName);
		itemObject->UiType = uiTypeName.IsEmpty() ? NAME_None : FName(*uiTypeName);
		itemObject->SortOrder = itemObjectJson->TryGetNumberField(TEXT("sort_order"), sortOrder)
			? FMath::RoundToInt(sortOrder)
			: 0;
		itemObjectJson->TryGetBoolField(TEXT("advanced"), bAdvanced);
		itemObjectJson->TryGetBoolField(TEXT("hidden"), bHidden);
		itemObject->bAdvanced = bAdvanced;
		itemObject->bHidden = bHidden;
		itemObject->bHasMinValue = itemObjectJson->TryGetNumberField(TEXT("min"), minValue);
		itemObject->bHasMaxValue = itemObjectJson->TryGetNumberField(TEXT("max"), maxValue);
		itemObject->MinValue = static_cast<float>(minValue);
		itemObject->MaxValue = static_cast<float>(maxValue);

		const TArray<TSharedPtr<FJsonValue>>* enumOptions = nullptr;
		if (itemObjectJson->TryGetArrayField(TEXT("enum_options"), enumOptions) && enumOptions != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& enumOption : *enumOptions)
			{
				FString enumOptionString;
				if (enumOption.IsValid() && enumOption->TryGetString(enumOptionString) && not enumOptionString.IsEmpty())
				{
					itemObject->EnumOptions.Add(enumOptionString);
				}
			}
		}

		if (itemObject->ValueType == EVdjmReflectionUiValueType::EUnsupported)
		{
			outErrorReason = FString::Printf(
				TEXT("Reflection UI schema item has unsupported value_type. OptionKey=%s ValueType=%s"),
				*optionKeyString,
				*valueTypeName);
			return nullptr;
		}

		return itemObject;
	}

	FString GetRecorderOptionRuntimeUiSchemaJson()
	{
		/*
		 * Runtime schema guide:
		 * - option_key must match FVdjmRecorderOptionRequest property names.
		 * - value_type controls UVdjmReflectionUiValueWidgetBase state helpers.
		 * - ui_type is a BP row/widget selection hint only.
		 * - min/max/default_value are runtime data and must not depend on UPROPERTY metadata.
		 * - When FVdjmRecorderOptionRequest changes, update this table in the same work unit.
		 */
		return TEXT(R"JSON(
{
  "schema_version": 1,
  "schema_name": "RecorderOptionUiSchema",
  "docs": [
    "Runtime schema only. Do not rely on UPROPERTY metadata in packaged builds.",
    "option_key must match FVdjmRecorderOptionRequest field names.",
    "Update this schema whenever recorder option request fields change."
  ],
  "items": [
    {
      "option_key": "QualityTier",
      "property_path": "QualityTier",
      "display_name": "Quality Tier",
      "category": "Recorder|Option",
      "value_type": "Enum",
      "value_property_name": "Value",
      "default_value": "EDefault",
      "ui_type": "ComboBox",
      "sort_order": 10,
      "advanced": false,
      "hidden": false,
      "enum_options": ["EDefault", "EUltra", "EHigh", "EMediumHigh", "EMedium", "EMdeiumLow", "ELow", "ELowest", "ECustom"]
    },
    {
      "option_key": "FileName",
      "property_path": "FileName",
      "display_name": "File Name",
      "category": "Recorder|Option",
      "value_type": "String",
      "value_property_name": "Value",
      "default_value": "",
      "ui_type": "TextBox",
      "sort_order": 20,
      "advanced": false,
      "hidden": false
    },
    {
      "option_key": "FrameRate",
      "property_path": "FrameRate",
      "display_name": "Frame Rate",
      "category": "Recorder|Option",
      "value_type": "Int",
      "value_property_name": "Value",
      "default_value": "30",
      "ui_type": "Slider",
      "sort_order": 30,
      "advanced": false,
      "hidden": false,
      "min": 24,
      "max": 60
    },
    {
      "option_key": "Bitrate",
      "property_path": "Bitrate",
      "display_name": "Bitrate",
      "category": "Recorder|Option",
      "value_type": "Int",
      "value_property_name": "Value",
      "default_value": "8000000",
      "ui_type": "Numeric",
      "sort_order": 40,
      "advanced": false,
      "hidden": false,
      "min": 500000,
      "max": 50000000
    },
    {
      "option_key": "Resolution",
      "property_path": "Resolution",
      "display_name": "Resolution",
      "category": "Recorder|Option|Video",
      "value_type": "IntPoint",
      "value_property_name": "Value",
      "default_value": "1920,1080",
      "ui_type": "IntPoint",
      "sort_order": 50,
      "advanced": false,
      "hidden": false,
      "min": 0
    },
    {
      "option_key": "ResolutionFitToDisplay",
      "property_path": "ResolutionFitToDisplay",
      "display_name": "Fit Resolution To Display",
      "category": "Recorder|Option|Video",
      "value_type": "Bool",
      "value_property_name": "Value",
      "default_value": "false",
      "ui_type": "CheckBox",
      "sort_order": 60,
      "advanced": false,
      "hidden": false
    },
    {
      "option_key": "KeyframeInterval",
      "property_path": "KeyframeInterval",
      "display_name": "Keyframe Interval",
      "category": "Recorder|Option|Video",
      "value_type": "Int",
      "value_property_name": "Value",
      "default_value": "1",
      "ui_type": "Numeric",
      "sort_order": 70,
      "advanced": true,
      "hidden": false,
      "min": 0,
      "max": 10
    },
    {
      "option_key": "MaxRecordDurationSeconds",
      "property_path": "MaxRecordDurationSeconds",
      "display_name": "Max Record Duration Seconds",
      "category": "Recorder|Option|Runtime",
      "value_type": "Float",
      "value_property_name": "Value",
      "default_value": "30.0",
      "ui_type": "Numeric",
      "sort_order": 80,
      "advanced": false,
      "hidden": false,
      "min": 1
    },
    {
      "option_key": "OutputFilePath",
      "property_path": "OutputFilePath",
      "display_name": "Output File Path",
      "category": "Recorder|Option|Output",
      "value_type": "String",
      "value_property_name": "Value",
      "default_value": "",
      "ui_type": "TextBox",
      "sort_order": 90,
      "advanced": true,
      "hidden": false
    },
    {
      "option_key": "SessionId",
      "property_path": "SessionId",
      "display_name": "Session Id",
      "category": "Recorder|Option|Output",
      "value_type": "String",
      "value_property_name": "Value",
      "default_value": "",
      "ui_type": "TextBox",
      "sort_order": 100,
      "advanced": true,
      "hidden": false
    },
    {
      "option_key": "OverwriteExists",
      "property_path": "OverwriteExists",
      "display_name": "Overwrite Existing File",
      "category": "Recorder|Option|Output",
      "value_type": "Bool",
      "value_property_name": "Value",
      "default_value": "true",
      "ui_type": "CheckBox",
      "sort_order": 110,
      "advanced": true,
      "hidden": false
    }
  ]
}
)JSON");
	}

	bool ParseBoolString(const FString& valueString, bool& outValue)
	{
		const FString normalizedValue = valueString.TrimStartAndEnd();
		if (normalizedValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("1"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("yes"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("on"), ESearchCase::IgnoreCase))
		{
			outValue = true;
			return true;
		}

		if (normalizedValue.Equals(TEXT("false"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("0"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("no"), ESearchCase::IgnoreCase) ||
			normalizedValue.Equals(TEXT("off"), ESearchCase::IgnoreCase))
		{
			outValue = false;
			return true;
		}

		return false;
	}

	bool ParseIntPointString(const FString& valueString, FIntPoint& outValue)
	{
		FString leftString;
		FString rightString;
		const FString normalizedValue = valueString.TrimStartAndEnd();
		if (normalizedValue.Split(TEXT(","), &leftString, &rightString) ||
			normalizedValue.Split(TEXT("x"), &leftString, &rightString) ||
			normalizedValue.Split(TEXT("X"), &leftString, &rightString))
		{
			outValue.X = FCString::Atoi(*leftString.TrimStartAndEnd());
			outValue.Y = FCString::Atoi(*rightString.TrimStartAndEnd());
			return true;
		}

		return false;
	}

	bool SetEnumPropertyFromString(FEnumProperty* enumProperty, void* valuePtr, const FString& valueString)
	{
		if (enumProperty == nullptr || valuePtr == nullptr)
		{
			return false;
		}

		UEnum* enumObject = enumProperty->GetEnum();
		if (enumObject == nullptr)
		{
			return false;
		}

		const FString normalizedValue = valueString.TrimStartAndEnd();
		int64 enumValue = enumObject->GetValueByNameString(normalizedValue);
		if (enumValue == INDEX_NONE)
		{
			for (int32 index = 0; index < enumObject->NumEnums(); ++index)
			{
				if (enumObject->GetNameStringByIndex(index).Equals(normalizedValue, ESearchCase::IgnoreCase) ||
					enumObject->GetDisplayNameTextByIndex(index).ToString().Equals(normalizedValue, ESearchCase::IgnoreCase))
				{
					enumValue = enumObject->GetValueByIndex(index);
					break;
				}
			}
		}

		if (enumValue == INDEX_NONE)
		{
			return false;
		}

		enumProperty->GetUnderlyingProperty()->SetIntPropertyValue(valuePtr, enumValue);
		return true;
	}

	bool SetByteEnumPropertyFromString(FByteProperty* byteProperty, void* valuePtr, const FString& valueString)
	{
		if (byteProperty == nullptr || valuePtr == nullptr || byteProperty->Enum == nullptr)
		{
			return false;
		}

		const FString normalizedValue = valueString.TrimStartAndEnd();
		int64 enumValue = byteProperty->Enum->GetValueByNameString(normalizedValue);
		if (enumValue == INDEX_NONE)
		{
			for (int32 index = 0; index < byteProperty->Enum->NumEnums(); ++index)
			{
				if (byteProperty->Enum->GetNameStringByIndex(index).Equals(normalizedValue, ESearchCase::IgnoreCase) ||
					byteProperty->Enum->GetDisplayNameTextByIndex(index).ToString().Equals(normalizedValue, ESearchCase::IgnoreCase))
				{
					enumValue = byteProperty->Enum->GetValueByIndex(index);
					break;
				}
			}
		}

		if (enumValue == INDEX_NONE)
		{
			return false;
		}

		byteProperty->SetIntPropertyValue(valuePtr, enumValue);
		return true;
	}

	bool SetValuePropertyFromString(FProperty* valueProperty, void* valuePtr, const FString& valueString, FString& outErrorReason)
	{
		if (valueProperty == nullptr || valuePtr == nullptr)
		{
			outErrorReason = TEXT("Value property is not available.");
			return false;
		}

		if (FIntProperty* intProperty = CastField<FIntProperty>(valueProperty))
		{
			intProperty->SetPropertyValue(valuePtr, FCString::Atoi(*valueString));
			return true;
		}

		if (FFloatProperty* floatProperty = CastField<FFloatProperty>(valueProperty))
		{
			floatProperty->SetPropertyValue(valuePtr, FCString::Atof(*valueString));
			return true;
		}

		if (FDoubleProperty* doubleProperty = CastField<FDoubleProperty>(valueProperty))
		{
			doubleProperty->SetPropertyValue(valuePtr, FCString::Atod(*valueString));
			return true;
		}

		if (FBoolProperty* boolProperty = CastField<FBoolProperty>(valueProperty))
		{
			bool bParsedValue = false;
			if (not ParseBoolString(valueString, bParsedValue))
			{
				outErrorReason = FString::Printf(TEXT("Failed to parse bool value '%s'."), *valueString);
				return false;
			}

			boolProperty->SetPropertyValue(valuePtr, bParsedValue);
			return true;
		}

		if (FStrProperty* stringProperty = CastField<FStrProperty>(valueProperty))
		{
			stringProperty->SetPropertyValue(valuePtr, valueString);
			return true;
		}

		if (FNameProperty* nameProperty = CastField<FNameProperty>(valueProperty))
		{
			nameProperty->SetPropertyValue(valuePtr, FName(*valueString));
			return true;
		}

		if (FTextProperty* textProperty = CastField<FTextProperty>(valueProperty))
		{
			textProperty->SetPropertyValue(valuePtr, FText::FromString(valueString));
			return true;
		}

		if (FEnumProperty* enumProperty = CastField<FEnumProperty>(valueProperty))
		{
			if (not SetEnumPropertyFromString(enumProperty, valuePtr, valueString))
			{
				outErrorReason = FString::Printf(TEXT("Failed to parse enum value '%s'."), *valueString);
				return false;
			}
			return true;
		}

		if (FByteProperty* byteProperty = CastField<FByteProperty>(valueProperty))
		{
			if (byteProperty->Enum != nullptr)
			{
				if (not SetByteEnumPropertyFromString(byteProperty, valuePtr, valueString))
				{
					outErrorReason = FString::Printf(TEXT("Failed to parse enum value '%s'."), *valueString);
					return false;
				}
				return true;
			}

			byteProperty->SetIntPropertyValue(valuePtr, static_cast<uint64>(static_cast<uint8>(FCString::Atoi(*valueString))));
			return true;
		}

		if (const FStructProperty* structProperty = CastField<FStructProperty>(valueProperty))
		{
			if (structProperty->Struct == TBaseStructure<FIntPoint>::Get())
			{
				FIntPoint intPointValue = FIntPoint::ZeroValue;
				if (not ParseIntPointString(valueString, intPointValue))
				{
					outErrorReason = FString::Printf(TEXT("Failed to parse int point value '%s'. Use 'X,Y' or 'XxY'."), *valueString);
					return false;
				}

				*static_cast<FIntPoint*>(valuePtr) = intPointValue;
				return true;
			}
		}

		outErrorReason = FString::Printf(TEXT("Unsupported value property '%s'."), *valueProperty->GetName());
		return false;
	}

	bool BuildRecorderOptionRequestFromString(FName optionKey, const FString& valueString, FVdjmRecorderOptionRequest& outRequest, FString& outErrorReason)
	{
		outRequest.Reset();
		outErrorReason.Reset();

		if (optionKey.IsNone())
		{
			outErrorReason = TEXT("Option key is None.");
			return false;
		}

		UScriptStruct* requestStruct = FVdjmRecorderOptionRequest::StaticStruct();
		FStructProperty* messageProperty = CastField<FStructProperty>(requestStruct->FindPropertyByName(optionKey));
		if (messageProperty == nullptr || messageProperty->Struct == nullptr)
		{
			outErrorReason = FString::Printf(TEXT("Recorder option key '%s' was not found."), *optionKey.ToString());
			return false;
		}

		FProperty* actionProperty = messageProperty->Struct->FindPropertyByName(ActionPropertyName);
		FProperty* valueProperty = messageProperty->Struct->FindPropertyByName(ValuePropertyName);
		if (actionProperty == nullptr || valueProperty == nullptr)
		{
			outErrorReason = FString::Printf(TEXT("Recorder option key '%s' is not an Action/Value message."), *optionKey.ToString());
			return false;
		}

		void* messagePtr = messageProperty->ContainerPtrToValuePtr<void>(&outRequest);
		void* actionPtr = actionProperty->ContainerPtrToValuePtr<void>(messagePtr);
		void* valuePtr = valueProperty->ContainerPtrToValuePtr<void>(messagePtr);

		if (FEnumProperty* enumActionProperty = CastField<FEnumProperty>(actionProperty))
		{
			enumActionProperty->GetUnderlyingProperty()->SetIntPropertyValue(
				actionPtr,
				static_cast<int64>(EVdjmRecorderOptionValueAction::ESet));
		}
		else if (FByteProperty* byteActionProperty = CastField<FByteProperty>(actionProperty))
		{
			byteActionProperty->SetIntPropertyValue(actionPtr, static_cast<int64>(EVdjmRecorderOptionValueAction::ESet));
		}
		else
		{
			outErrorReason = FString::Printf(TEXT("Recorder option key '%s' has unsupported Action property."), *optionKey.ToString());
			return false;
		}

		return SetValuePropertyFromString(valueProperty, valuePtr, valueString, outErrorReason);
	}

	bool SubmitRecorderOptionRequestFromString(
		UObject* worldContextObject,
		FName optionKey,
		const FString& valueString,
		FString& outErrorReason,
		bool bProcessPendingAfterSubmit)
	{
		outErrorReason.Reset();

		FVdjmRecorderOptionRequest optionRequest;
		if (not BuildRecorderOptionRequestFromString(optionKey, valueString, optionRequest, outErrorReason))
		{
			return false;
		}

		UVdjmRecorderController* recorderController = UVdjmRecorderController::FindOrCreateRecorderController(worldContextObject);
		if (recorderController == nullptr)
		{
			outErrorReason = TEXT("Recorder controller is not available.");
			return false;
		}

		if (not recorderController->SubmitOptionRequest(optionRequest, outErrorReason))
		{
			return false;
		}

		if (bProcessPendingAfterSubmit)
		{
			FString processErrorReason;
			const bool bProcessResult = recorderController->ProcessPendingOptionRequests(processErrorReason);
			if (not bProcessResult && not recorderController->HasPendingOptionRequest())
			{
				outErrorReason = processErrorReason.IsEmpty()
					? TEXT("Failed to process recorder option request.")
					: processErrorReason;
				return false;
			}
		}

		return true;
	}
}

TArray<UVdjmReflectionUiItemObject*> UVdjmReflectionUiBlueprintLibrary::BuildReflectionUiItemsFromStruct(
	UObject* worldContextObject,
	UScriptStruct* structType)
{
	TArray<UVdjmReflectionUiItemObject*> itemObjects;
	if (structType == nullptr)
	{
		return itemObjects;
	}

	UObject* outerObject = worldContextObject != nullptr ? worldContextObject : GetTransientPackage();
	for (TFieldIterator<FProperty> propertyIterator(structType); propertyIterator; ++propertyIterator)
	{
		const FStructProperty* messageProperty = CastField<FStructProperty>(*propertyIterator);
		if (messageProperty == nullptr)
		{
			continue;
		}

		if (UVdjmReflectionUiItemObject* itemObject = BuildItemFromProperty(outerObject, structType, messageProperty))
		{
			itemObjects.Add(itemObject);
		}
	}

	itemObjects.Sort([](const UVdjmReflectionUiItemObject& left, const UVdjmReflectionUiItemObject& right)
	{
		if (left.SortOrder != right.SortOrder)
		{
			return left.SortOrder < right.SortOrder;
		}

		return left.OptionKey.LexicalLess(right.OptionKey);
	});

	return itemObjects;
}

TArray<UVdjmReflectionUiItemObject*> UVdjmReflectionUiBlueprintLibrary::BuildReflectionUiItemsFromJsonString(
	UObject* worldContextObject,
	const FString& schemaJsonString,
	FString& outErrorReason)
{
	TArray<UVdjmReflectionUiItemObject*> itemObjects;
	outErrorReason.Reset();

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(schemaJsonString);
	if (not FJsonSerializer::Deserialize(reader, rootObject) || not rootObject.IsValid())
	{
		outErrorReason = TEXT("Failed to parse reflection UI schema JSON.");
		return itemObjects;
	}

	const TArray<TSharedPtr<FJsonValue>>* itemValues = nullptr;
	if (not rootObject->TryGetArrayField(TEXT("items"), itemValues) || itemValues == nullptr)
	{
		outErrorReason = TEXT("Reflection UI schema has no items array.");
		return itemObjects;
	}

	UObject* outerObject = worldContextObject != nullptr ? worldContextObject : GetTransientPackage();
	for (const TSharedPtr<FJsonValue>& itemValue : *itemValues)
	{
		if (not itemValue.IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject> itemObjectJson = itemValue->AsObject();
		FString itemErrorReason;
		UVdjmReflectionUiItemObject* itemObject = BuildItemFromJsonObject(outerObject, itemObjectJson, itemErrorReason);
		if (itemObject == nullptr)
		{
			if (outErrorReason.IsEmpty())
			{
				outErrorReason = itemErrorReason;
			}
			continue;
		}

		if (not itemObject->bHidden)
		{
			itemObjects.Add(itemObject);
		}
	}

	itemObjects.Sort([](const UVdjmReflectionUiItemObject& left, const UVdjmReflectionUiItemObject& right)
	{
		if (left.SortOrder != right.SortOrder)
		{
			return left.SortOrder < right.SortOrder;
		}

		return left.OptionKey.LexicalLess(right.OptionKey);
	});

	if (itemObjects.IsEmpty() && outErrorReason.IsEmpty())
	{
		outErrorReason = TEXT("Reflection UI schema produced no visible items.");
	}

	return itemObjects;
}

FString UVdjmReflectionUiBlueprintLibrary::ExportReflectionUiMapToJson(UScriptStruct* structType, bool bPrettyPrint)
{
	TArray<UVdjmReflectionUiItemObject*> itemObjects = BuildReflectionUiItemsFromStruct(GetTransientPackage(), structType);

	const TSharedPtr<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("struct"), structType != nullptr ? structType->GetPathName() : TEXT("None"));

	TArray<TSharedPtr<FJsonValue>> itemValues;
	itemValues.Reserve(itemObjects.Num());
	for (const UVdjmReflectionUiItemObject* itemObject : itemObjects)
	{
		itemValues.Add(MakeShared<FJsonValueObject>(BuildJsonObjectFromItem(itemObject)));
	}
	rootObject->SetArrayField(TEXT("items"), itemValues);

	FString jsonString;
	if (bPrettyPrint)
	{
		const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&jsonString);
		FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer);
	}
	else
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&jsonString);
		FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer);
	}

	return jsonString;
}

TArray<UVdjmReflectionUiItemObject*> UVdjmReflectionUiBlueprintLibrary::BuildRecorderOptionUiItems(UObject* worldContextObject)
{
	FString errorReason;
	TArray<UVdjmReflectionUiItemObject*> itemObjects = BuildReflectionUiItemsFromJsonString(
		worldContextObject,
		GetRecorderOptionRuntimeUiSchemaJson(),
		errorReason);
	if (itemObjects.IsEmpty() && not errorReason.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UVdjmReflectionUiBlueprintLibrary::BuildRecorderOptionUiItems - Failed to build runtime schema items. Reason=%s"),
			*errorReason);
	}

	return itemObjects;
}

bool UVdjmReflectionUiBlueprintLibrary::SubmitRecorderOptionUiValue(
	UObject* worldContextObject,
	FName optionKey,
	const FString& valueString,
	FString& outErrorReason,
	bool bProcessPendingAfterSubmit)
{
	return SubmitRecorderOptionRequestFromString(
		worldContextObject,
		optionKey,
		valueString,
		outErrorReason,
		bProcessPendingAfterSubmit);
}

bool UVdjmReflectionUiBlueprintLibrary::SubmitRecorderOptionUiItemValue(
	UObject* worldContextObject,
	UVdjmReflectionUiItemObject* itemObject,
	const FString& valueString,
	FString& outErrorReason,
	bool bProcessPendingAfterSubmit)
{
	if (itemObject == nullptr)
	{
		outErrorReason = TEXT("Reflection UI item is not available.");
		return false;
	}

	return SubmitRecorderOptionUiValue(
		worldContextObject,
		itemObject->OptionKey,
		valueString,
		outErrorReason,
		bProcessPendingAfterSubmit);
}

bool UVdjmReflectionUiBlueprintLibrary::SubmitRecorderOptionUiBoolValue(
	UObject* worldContextObject,
	FName optionKey,
	bool bValue,
	FString& outErrorReason,
	bool bProcessPendingAfterSubmit)
{
	return SubmitRecorderOptionUiValue(
		worldContextObject,
		optionKey,
		bValue ? TEXT("true") : TEXT("false"),
		outErrorReason,
		bProcessPendingAfterSubmit);
}

bool UVdjmReflectionUiBlueprintLibrary::SubmitRecorderOptionUiFloatValue(
	UObject* worldContextObject,
	FName optionKey,
	float value,
	FString& outErrorReason,
	bool bProcessPendingAfterSubmit)
{
	return SubmitRecorderOptionUiValue(
		worldContextObject,
		optionKey,
		FString::SanitizeFloat(value),
		outErrorReason,
		bProcessPendingAfterSubmit);
}

bool UVdjmReflectionUiBlueprintLibrary::SubmitRecorderOptionUiIntValue(
	UObject* worldContextObject,
	FName optionKey,
	int32 value,
	FString& outErrorReason,
	bool bProcessPendingAfterSubmit)
{
	return SubmitRecorderOptionUiValue(
		worldContextObject,
		optionKey,
		FString::FromInt(value),
		outErrorReason,
		bProcessPendingAfterSubmit);
}

bool UVdjmReflectionUiBlueprintLibrary::SubmitRecorderOptionUiIntPointValue(
	UObject* worldContextObject,
	FName optionKey,
	FIntPoint value,
	FString& outErrorReason,
	bool bProcessPendingAfterSubmit)
{
	return SubmitRecorderOptionUiValue(
		worldContextObject,
		optionKey,
		FString::Printf(TEXT("%d,%d"), value.X, value.Y),
		outErrorReason,
		bProcessPendingAfterSubmit);
}

FString UVdjmReflectionUiBlueprintLibrary::ExportRecorderOptionUiMapToJson(bool bPrettyPrint)
{
	FString errorReason;
	TArray<UVdjmReflectionUiItemObject*> itemObjects = BuildReflectionUiItemsFromJsonString(
		GetTransientPackage(),
		GetRecorderOptionRuntimeUiSchemaJson(),
		errorReason);

	const TSharedPtr<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetStringField(TEXT("schema_source"), TEXT("runtime_json"));
	rootObject->SetStringField(TEXT("struct"), FVdjmRecorderOptionRequest::StaticStruct()->GetPathName());
	rootObject->SetStringField(TEXT("error_reason"), errorReason);

	TArray<TSharedPtr<FJsonValue>> itemValues;
	itemValues.Reserve(itemObjects.Num());
	for (const UVdjmReflectionUiItemObject* itemObject : itemObjects)
	{
		itemValues.Add(MakeShared<FJsonValueObject>(BuildJsonObjectFromItem(itemObject)));
	}
	rootObject->SetArrayField(TEXT("items"), itemValues);

	FString jsonString;
	if (bPrettyPrint)
	{
		const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&jsonString);
		FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer);
	}
	else
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&jsonString);
		FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer);
	}

	return jsonString;
}

FString UVdjmReflectionUiBlueprintLibrary::ExportRecorderOptionUiSchemaJson(bool bPrettyPrint)
{
	const FString schemaJsonString = GetRecorderOptionRuntimeUiSchemaJson();
	if (not bPrettyPrint)
	{
		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(schemaJsonString);
		if (FJsonSerializer::Deserialize(reader, rootObject) && rootObject.IsValid())
		{
			FString condensedJsonString;
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> writer =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&condensedJsonString);
			FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer);
			return condensedJsonString;
		}
	}

	return schemaJsonString;
}
