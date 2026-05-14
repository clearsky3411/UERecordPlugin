#include "VdjmReflection/VdjmReflectionUiTypes.h"

#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"

namespace
{
FString GetBoolValueString(bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}

FString GetIntPointValueString(const FIntPoint& value)
{
	return FString::Printf(TEXT("X=%d Y=%d"), value.X, value.Y);
}

FIntPoint ParseIntPointValueString(const FString& valueString)
{
	FIntPoint parsedValue = FIntPoint::ZeroValue;
	if (parsedValue.InitFromString(valueString))
	{
		return parsedValue;
	}

	FString leftValueString;
	FString rightValueString;
	if (valueString.Split(TEXT(","), &leftValueString, &rightValueString))
	{
		parsedValue.X = FCString::Atoi(*leftValueString.TrimStartAndEnd());
		parsedValue.Y = FCString::Atoi(*rightValueString.TrimStartAndEnd());
	}

	return parsedValue;
}

bool AreReflectionUiAnyValuesSame(const FVdjmReflectionUiAnyValue& leftValue, const FVdjmReflectionUiAnyValue& rightValue)
{
	return leftValue.ValueType == rightValue.ValueType
		&& leftValue.ValueIndex == rightValue.ValueIndex
		&& leftValue.TrackingId == rightValue.TrackingId
		&& leftValue.IntValue == rightValue.IntValue
		&& FMath::IsNearlyEqual(leftValue.FloatValue, rightValue.FloatValue)
		&& leftValue.bBoolValue == rightValue.bBoolValue
		&& leftValue.StringValue == rightValue.StringValue
		&& leftValue.NameValue == rightValue.NameValue
		&& leftValue.IntPointValue == rightValue.IntPointValue
		&& leftValue.ValueAsString == rightValue.ValueAsString;
}

bool IsNumericReflectionUiValueType(EVdjmReflectionUiValueType valueType)
{
	return valueType == EVdjmReflectionUiValueType::EFloat
		|| valueType == EVdjmReflectionUiValueType::EInt;
}

float GetReflectionValueAsFloat(const FVdjmReflectionUiAnyValue& value)
{
	return value.ValueType == EVdjmReflectionUiValueType::EInt
		? static_cast<float>(value.IntValue)
		: value.FloatValue;
}
}

void FVdjmReflectionUiAnyValue::Reset()
{
	ValueType = EVdjmReflectionUiValueType::EUnsupported;
	ValueIndex = INDEX_NONE;
	TrackingId = INDEX_NONE;
	IntValue = 0;
	FloatValue = 0.0f;
	bBoolValue = false;
	StringValue.Reset();
	NameValue = NAME_None;
	IntPointValue = FIntPoint::ZeroValue;
	ValueAsString.Reset();
}

FName UVdjmReflectionUiItemObject::GetOptionKey() const
{
	return OptionKey;
}

FString UVdjmReflectionUiItemObject::GetValueTypeName() const
{
	const UEnum* enumObject = StaticEnum<EVdjmReflectionUiValueType>();
	return enumObject != nullptr
		? enumObject->GetNameStringByValue(static_cast<int64>(ValueType))
		: FString::FromInt(static_cast<int32>(ValueType));
}

FText UVdjmReflectionUiItemObject::GetUiDisplayNameText() const
{
	return DisplayName;
}

FString UVdjmReflectionUiItemObject::GetUiDisplayNameString() const
{
	return DisplayName.ToString();
}

FString UVdjmReflectionUiItemObject::GetDebugSummary() const
{
	return FString::Printf(
		TEXT("OptionKey=%s DisplayName=%s ValueType=%s UiType=%s Default=%s Min=%s Max=%s Advanced=%s"),
		*OptionKey.ToString(),
		*DisplayName.ToString(),
		*GetValueTypeName(),
		*UiType.ToString(),
		*ValueAsString,
		bHasMinValue ? *FString::SanitizeFloat(MinValue) : TEXT("None"),
		bHasMaxValue ? *FString::SanitizeFloat(MaxValue) : TEXT("None"),
		bAdvanced ? TEXT("true") : TEXT("false"));
}

bool UVdjmReflectionUiValueWidgetBase::InitializeReflectionValueWidget(UVdjmReflectionUiItemObject* itemObject, UObject* linkedObject, int32 trackingId)
{
	mInitialized = true;
	mInitializationValid = false;
	mInitializationError.Reset();
	mReflectionUiItem = itemObject;
	mLinkedObject = linkedObject;
	mTrackingId = trackingId != INDEX_NONE
		? trackingId
		: (mReflectionUiItem != nullptr ? mReflectionUiItem->SortOrder : INDEX_NONE);

	if (not ValidateReflectionValueWidgetItem(mReflectionUiItem, mInitializationError))
	{
		mCurrentValue.Reset();
		ClearReflectionValueHistory();
		return false;
	}

	if (mReflectionUiItem != nullptr)
	{
		SetReflectionValueFromString(mReflectionUiItem->ValueType, mReflectionUiItem->ValueAsString, mTrackingId, false);
	}
	else
	{
		mCurrentValue.Reset();
	}
	ClearReflectionValueHistory();
	mInitializationValid = true;

	NativeOnReflectionValueWidgetInitialized(mReflectionUiItem);
	BP_OnReflectionValueWidgetInitialized(mReflectionUiItem);
	BP_OnReflectionUiItemApplied(mReflectionUiItem);
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::ApplyReflectionUiItem(UVdjmReflectionUiItemObject* itemObject)
{
	return InitializeReflectionValueWidget(itemObject, GetLinkedObject(), INDEX_NONE);
}

UVdjmReflectionUiItemObject* UVdjmReflectionUiValueWidgetBase::GetReflectionUiItem() const
{
	return mReflectionUiItem;
}

FVdjmReflectionUiAnyValue UVdjmReflectionUiValueWidgetBase::GetCurrentReflectionValue() const
{
	return mCurrentValue;
}

FString UVdjmReflectionUiValueWidgetBase::GetCurrentReflectionValueString() const
{
	return mCurrentValue.ValueAsString;
}

bool UVdjmReflectionUiValueWidgetBase::ValidateReflectionValueWidgetItem(UVdjmReflectionUiItemObject* itemObject, FString& outErrorReason) const
{
	outErrorReason.Reset();
	if (itemObject == nullptr)
	{
		outErrorReason = TEXT("Reflection UI item is null.");
		return false;
	}

	if (itemObject->ValueType == EVdjmReflectionUiValueType::EUnsupported)
	{
		outErrorReason = FString::Printf(TEXT("Unsupported reflection UI value type. Item=%s"), *itemObject->GetDebugSummary());
		return false;
	}

	if (not mSupportedValueTypes.IsEmpty() && not mSupportedValueTypes.Contains(itemObject->ValueType))
	{
		outErrorReason = FString::Printf(
			TEXT("Value widget does not support this value type. ValueType=%s"),
			*itemObject->GetValueTypeName());
		return false;
	}

	return NativeValidateReflectionValueWidgetItem(itemObject, outErrorReason);
}

bool UVdjmReflectionUiValueWidgetBase::IsReflectionValueWidgetInitialized() const
{
	return mInitialized;
}

bool UVdjmReflectionUiValueWidgetBase::IsReflectionValueWidgetInitializationValid() const
{
	return mInitializationValid;
}

FString UVdjmReflectionUiValueWidgetBase::GetReflectionValueWidgetInitializationError() const
{
	return mInitializationError;
}

bool UVdjmReflectionUiValueWidgetBase::GetNumericWidgetState(FVdjmReflectionUiNumericWidgetState& outState, FString& outErrorReason) const
{
	outState = FVdjmReflectionUiNumericWidgetState();
	outErrorReason.Reset();
	if (mReflectionUiItem == nullptr)
	{
		outErrorReason = TEXT("Reflection UI item is null.");
		return false;
	}

	if (not IsNumericReflectionUiValueType(mReflectionUiItem->ValueType))
	{
		outErrorReason = FString::Printf(TEXT("Value type is not numeric. ValueType=%s"), *mReflectionUiItem->GetValueTypeName());
		return false;
	}

	const float currentValue = GetReflectionValueAsFloat(mCurrentValue);
	outState.bHasMinValue = mReflectionUiItem->bHasMinValue;
	outState.bHasMaxValue = mReflectionUiItem->bHasMaxValue;
	outState.MinValue = mReflectionUiItem->bHasMinValue ? mReflectionUiItem->MinValue : 0.0f;
	outState.MaxValue = mReflectionUiItem->bHasMaxValue ? mReflectionUiItem->MaxValue : FMath::Max(1.0f, currentValue);
	outState.bIsInteger = mReflectionUiItem->ValueType == EVdjmReflectionUiValueType::EInt;

	if (mReflectionUiItem->bHasMinValue && mReflectionUiItem->bHasMaxValue && mReflectionUiItem->MaxValue < mReflectionUiItem->MinValue)
	{
		outErrorReason = FString::Printf(
			TEXT("Invalid numeric range. Min=%s Max=%s"),
			*FString::SanitizeFloat(mReflectionUiItem->MinValue),
			*FString::SanitizeFloat(mReflectionUiItem->MaxValue));
		return false;
	}

	if (outState.MaxValue < outState.MinValue)
	{
		outState.MaxValue = outState.MinValue;
	}

	outState.CurrentValue = currentValue;
	if (mReflectionUiItem->bHasMinValue || mReflectionUiItem->bHasMaxValue)
	{
		outState.CurrentValue = FMath::Clamp(outState.CurrentValue, outState.MinValue, outState.MaxValue);
	}

	return true;
}

bool UVdjmReflectionUiValueWidgetBase::SubmitNumericWidgetValue(float value, bool bBroadcast)
{
	if (mReflectionUiItem == nullptr || not IsNumericReflectionUiValueType(mReflectionUiItem->ValueType))
	{
		return false;
	}

	FVdjmReflectionUiNumericWidgetState state;
	FString errorReason;
	const bool bHasState = GetNumericWidgetState(state, errorReason);
	const float clampedValue = bHasState && (state.bHasMinValue || state.bHasMaxValue)
		? FMath::Clamp(value, state.MinValue, state.MaxValue)
		: value;

	if (mReflectionUiItem->ValueType == EVdjmReflectionUiValueType::EInt)
	{
		SetReflectionIntValue(FMath::RoundToInt(clampedValue), mTrackingId, bBroadcast);
		return true;
	}

	SetReflectionFloatValue(clampedValue, mTrackingId, bBroadcast);
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::GetTextWidgetState(FVdjmReflectionUiTextWidgetState& outState, FString& outErrorReason) const
{
	outState = FVdjmReflectionUiTextWidgetState();
	outErrorReason.Reset();
	if (mReflectionUiItem == nullptr)
	{
		outErrorReason = TEXT("Reflection UI item is null.");
		return false;
	}

	if (mReflectionUiItem->ValueType != EVdjmReflectionUiValueType::EString
		&& mReflectionUiItem->ValueType != EVdjmReflectionUiValueType::EEnum)
	{
		outErrorReason = FString::Printf(TEXT("Value type is not text-like. ValueType=%s"), *mReflectionUiItem->GetValueTypeName());
		return false;
	}

	outState.DisplayText = mReflectionUiItem->DisplayName;
	outState.CurrentString = mCurrentValue.ValueAsString;
	outState.Options = mReflectionUiItem->EnumOptions;
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::SubmitTextWidgetValue(const FString& value, bool bBroadcast)
{
	if (mReflectionUiItem == nullptr)
	{
		return false;
	}

	if (mReflectionUiItem->ValueType == EVdjmReflectionUiValueType::EString)
	{
		SetReflectionStringValue(value, mTrackingId, bBroadcast);
		return true;
	}

	if (mReflectionUiItem->ValueType == EVdjmReflectionUiValueType::EEnum)
	{
		FVdjmReflectionUiAnyValue nextValue;
		nextValue.ValueType = EVdjmReflectionUiValueType::EEnum;
		nextValue.ValueIndex = mTrackingId;
		nextValue.TrackingId = mTrackingId;
		nextValue.StringValue = value;
		nextValue.NameValue = FName(*value);
		nextValue.ValueAsString = value;
		SetReflectionValue(nextValue, bBroadcast);
		return true;
	}

	return false;
}

bool UVdjmReflectionUiValueWidgetBase::GetBoolWidgetState(FVdjmReflectionUiBoolWidgetState& outState, FString& outErrorReason) const
{
	outState = FVdjmReflectionUiBoolWidgetState();
	outErrorReason.Reset();
	if (mReflectionUiItem == nullptr)
	{
		outErrorReason = TEXT("Reflection UI item is null.");
		return false;
	}

	if (mReflectionUiItem->ValueType != EVdjmReflectionUiValueType::EBool)
	{
		outErrorReason = FString::Printf(TEXT("Value type is not bool. ValueType=%s"), *mReflectionUiItem->GetValueTypeName());
		return false;
	}

	outState.bCurrentValue = mCurrentValue.bBoolValue;
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::SubmitBoolWidgetValue(bool bValue, bool bBroadcast)
{
	if (mReflectionUiItem == nullptr || mReflectionUiItem->ValueType != EVdjmReflectionUiValueType::EBool)
	{
		return false;
	}

	SetReflectionBoolValue(bValue, mTrackingId, bBroadcast);
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::GetIntPointWidgetState(FVdjmReflectionUiIntPointWidgetState& outState, FString& outErrorReason) const
{
	outState = FVdjmReflectionUiIntPointWidgetState();
	outErrorReason.Reset();
	if (mReflectionUiItem == nullptr)
	{
		outErrorReason = TEXT("Reflection UI item is null.");
		return false;
	}

	if (mReflectionUiItem->ValueType != EVdjmReflectionUiValueType::EIntPoint)
	{
		outErrorReason = FString::Printf(TEXT("Value type is not int point. ValueType=%s"), *mReflectionUiItem->GetValueTypeName());
		return false;
	}

	outState.CurrentValue = mCurrentValue.IntPointValue;
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::SubmitIntPointWidgetValue(FIntPoint value, bool bBroadcast)
{
	if (mReflectionUiItem == nullptr || mReflectionUiItem->ValueType != EVdjmReflectionUiValueType::EIntPoint)
	{
		return false;
	}

	SetReflectionIntPointValue(value, mTrackingId, bBroadcast);
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::BindNumericSlider(USlider* sliderWidget, bool bBindValueChanged, bool bApplyCurrentState)
{
	if (mBoundNumericSlider != nullptr)
	{
		mBoundNumericSlider->OnValueChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundNumericSliderValueChanged);
	}

	mBoundNumericSlider = sliderWidget;
	if (mBoundNumericSlider == nullptr)
	{
		return false;
	}

	FString errorReason;
	FVdjmReflectionUiNumericWidgetState state;
	if (mReflectionUiItem != nullptr && not GetNumericWidgetState(state, errorReason))
	{
		mBoundNumericSlider = nullptr;
		return false;
	}

	if (bBindValueChanged)
	{
		mBoundNumericSlider->OnValueChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundNumericSliderValueChanged);
		mBoundNumericSlider->OnValueChanged.AddDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundNumericSliderValueChanged);
	}

	if (bApplyCurrentState)
	{
		ApplyCurrentValueToBoundWidgets();
	}
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::BindBoolCheckBox(UCheckBox* checkBoxWidget, bool bBindValueChanged, bool bApplyCurrentState)
{
	if (mBoundBoolCheckBox != nullptr)
	{
		mBoundBoolCheckBox->OnCheckStateChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundBoolCheckBoxValueChanged);
	}

	mBoundBoolCheckBox = checkBoxWidget;
	if (mBoundBoolCheckBox == nullptr)
	{
		return false;
	}

	FString errorReason;
	FVdjmReflectionUiBoolWidgetState state;
	if (mReflectionUiItem != nullptr && not GetBoolWidgetState(state, errorReason))
	{
		mBoundBoolCheckBox = nullptr;
		return false;
	}

	if (bBindValueChanged)
	{
		mBoundBoolCheckBox->OnCheckStateChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundBoolCheckBoxValueChanged);
		mBoundBoolCheckBox->OnCheckStateChanged.AddDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundBoolCheckBoxValueChanged);
	}

	if (bApplyCurrentState)
	{
		ApplyCurrentValueToBoundWidgets();
	}
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::BindTextBox(UEditableTextBox* textBoxWidget, bool bBindValueChanged, bool bApplyCurrentState)
{
	if (mBoundTextBox != nullptr)
	{
		mBoundTextBox->OnTextChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundTextBoxTextChanged);
	}

	mBoundTextBox = textBoxWidget;
	if (mBoundTextBox == nullptr)
	{
		return false;
	}

	FString errorReason;
	FVdjmReflectionUiTextWidgetState state;
	if (mReflectionUiItem != nullptr && not GetTextWidgetState(state, errorReason))
	{
		mBoundTextBox = nullptr;
		return false;
	}

	if (bBindValueChanged)
	{
		mBoundTextBox->OnTextChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundTextBoxTextChanged);
		mBoundTextBox->OnTextChanged.AddDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundTextBoxTextChanged);
	}

	if (bApplyCurrentState)
	{
		ApplyCurrentValueToBoundWidgets();
	}
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::BindComboBoxString(UComboBoxString* comboBoxWidget, bool bBindValueChanged, bool bApplyCurrentState)
{
	if (mBoundComboBoxString != nullptr)
	{
		mBoundComboBoxString->OnSelectionChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundComboBoxSelectionChanged);
	}

	mBoundComboBoxString = comboBoxWidget;
	if (mBoundComboBoxString == nullptr)
	{
		return false;
	}

	FString errorReason;
	FVdjmReflectionUiTextWidgetState state;
	if (mReflectionUiItem != nullptr && not GetTextWidgetState(state, errorReason))
	{
		mBoundComboBoxString = nullptr;
		return false;
	}

	if (bBindValueChanged)
	{
		mBoundComboBoxString->OnSelectionChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundComboBoxSelectionChanged);
		mBoundComboBoxString->OnSelectionChanged.AddDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundComboBoxSelectionChanged);
	}

	if (bApplyCurrentState)
	{
		ApplyCurrentValueToBoundWidgets();
	}
	return true;
}

void UVdjmReflectionUiValueWidgetBase::UnbindValueInputWidgets()
{
	if (mBoundNumericSlider != nullptr)
	{
		mBoundNumericSlider->OnValueChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundNumericSliderValueChanged);
	}
	if (mBoundBoolCheckBox != nullptr)
	{
		mBoundBoolCheckBox->OnCheckStateChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundBoolCheckBoxValueChanged);
	}
	if (mBoundTextBox != nullptr)
	{
		mBoundTextBox->OnTextChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundTextBoxTextChanged);
	}
	if (mBoundComboBoxString != nullptr)
	{
		mBoundComboBoxString->OnSelectionChanged.RemoveDynamic(this, &UVdjmReflectionUiValueWidgetBase::HandleBoundComboBoxSelectionChanged);
	}

	mBoundNumericSlider = nullptr;
	mBoundBoolCheckBox = nullptr;
	mBoundTextBox = nullptr;
	mBoundComboBoxString = nullptr;
}

bool UVdjmReflectionUiValueWidgetBase::ApplyCurrentValueToBoundWidgets()
{
	bool bAppliedAnyWidget = false;
	mApplyingBoundWidgetValue = true;

	if (mBoundNumericSlider != nullptr)
	{
		FString errorReason;
		FVdjmReflectionUiNumericWidgetState state;
		if (GetNumericWidgetState(state, errorReason))
		{
			mBoundNumericSlider->SetMinValue(state.MinValue);
			mBoundNumericSlider->SetMaxValue(state.MaxValue);
			mBoundNumericSlider->SetValue(state.CurrentValue);
			bAppliedAnyWidget = true;
		}
	}

	if (mBoundBoolCheckBox != nullptr)
	{
		FString errorReason;
		FVdjmReflectionUiBoolWidgetState state;
		if (GetBoolWidgetState(state, errorReason))
		{
			mBoundBoolCheckBox->SetIsChecked(state.bCurrentValue);
			bAppliedAnyWidget = true;
		}
	}

	if (mBoundTextBox != nullptr)
	{
		FString errorReason;
		FVdjmReflectionUiTextWidgetState state;
		if (GetTextWidgetState(state, errorReason))
		{
			mBoundTextBox->SetText(FText::FromString(state.CurrentString));
			bAppliedAnyWidget = true;
		}
	}

	if (mBoundComboBoxString != nullptr)
	{
		FString errorReason;
		FVdjmReflectionUiTextWidgetState state;
		if (GetTextWidgetState(state, errorReason))
		{
			mBoundComboBoxString->ClearOptions();
			for (const FString& option : state.Options)
			{
				mBoundComboBoxString->AddOption(option);
			}
			if (not state.CurrentString.IsEmpty() && mBoundComboBoxString->FindOptionIndex(state.CurrentString) == INDEX_NONE)
			{
				mBoundComboBoxString->AddOption(state.CurrentString);
			}
			mBoundComboBoxString->SetSelectedOption(state.CurrentString);
			bAppliedAnyWidget = true;
		}
	}

	mApplyingBoundWidgetValue = false;
	return bAppliedAnyWidget;
}

void UVdjmReflectionUiValueWidgetBase::SetReflectionValue(const FVdjmReflectionUiAnyValue& value, bool bBroadcast)
{
	if (bBroadcast
		&& not mApplyingReflectionValueHistory
		&& mCurrentValue.ValueType != EVdjmReflectionUiValueType::EUnsupported
		&& not AreReflectionUiAnyValuesSame(mCurrentValue, value))
	{
		PushUndoReflectionValue(mCurrentValue);
		mRedoValues.Reset();
	}

	mCurrentValue = value;
	mCurrentValue.TrackingId = mTrackingId;
	if (mCurrentValue.ValueIndex == INDEX_NONE)
	{
		mCurrentValue.ValueIndex = mTrackingId;
	}
	ApplyCurrentValueToBoundWidgets();

	if (bBroadcast)
	{
		BroadcastReflectionValueChanged();
	}
}

void UVdjmReflectionUiValueWidgetBase::SetReflectionStringValue(const FString& value, int32 valueIndex, bool bBroadcast)
{
	FVdjmReflectionUiAnyValue nextValue;
	nextValue.ValueType = EVdjmReflectionUiValueType::EString;
	nextValue.ValueIndex = valueIndex;
	nextValue.TrackingId = mTrackingId;
	nextValue.StringValue = value;
	nextValue.NameValue = FName(*value);
	nextValue.ValueAsString = value;
	SetReflectionValue(nextValue, bBroadcast);
}

void UVdjmReflectionUiValueWidgetBase::SetReflectionIntValue(int32 value, int32 valueIndex, bool bBroadcast)
{
	FVdjmReflectionUiAnyValue nextValue;
	nextValue.ValueType = EVdjmReflectionUiValueType::EInt;
	nextValue.ValueIndex = valueIndex;
	nextValue.TrackingId = mTrackingId;
	nextValue.IntValue = value;
	nextValue.FloatValue = static_cast<float>(value);
	nextValue.StringValue = FString::FromInt(value);
	nextValue.ValueAsString = nextValue.StringValue;
	SetReflectionValue(nextValue, bBroadcast);
}

void UVdjmReflectionUiValueWidgetBase::SetReflectionFloatValue(float value, int32 valueIndex, bool bBroadcast)
{
	FVdjmReflectionUiAnyValue nextValue;
	nextValue.ValueType = EVdjmReflectionUiValueType::EFloat;
	nextValue.ValueIndex = valueIndex;
	nextValue.TrackingId = mTrackingId;
	nextValue.FloatValue = value;
	nextValue.IntValue = FMath::RoundToInt(value);
	nextValue.StringValue = FString::SanitizeFloat(value);
	nextValue.ValueAsString = nextValue.StringValue;
	SetReflectionValue(nextValue, bBroadcast);
}

void UVdjmReflectionUiValueWidgetBase::SetReflectionBoolValue(bool bValue, int32 valueIndex, bool bBroadcast)
{
	FVdjmReflectionUiAnyValue nextValue;
	nextValue.ValueType = EVdjmReflectionUiValueType::EBool;
	nextValue.ValueIndex = valueIndex;
	nextValue.TrackingId = mTrackingId;
	nextValue.bBoolValue = bValue;
	nextValue.IntValue = bValue ? 1 : 0;
	nextValue.StringValue = GetBoolValueString(bValue);
	nextValue.ValueAsString = nextValue.StringValue;
	SetReflectionValue(nextValue, bBroadcast);
}

void UVdjmReflectionUiValueWidgetBase::SetReflectionIntPointValue(FIntPoint value, int32 valueIndex, bool bBroadcast)
{
	FVdjmReflectionUiAnyValue nextValue;
	nextValue.ValueType = EVdjmReflectionUiValueType::EIntPoint;
	nextValue.ValueIndex = valueIndex;
	nextValue.TrackingId = mTrackingId;
	nextValue.IntPointValue = value;
	nextValue.StringValue = GetIntPointValueString(value);
	nextValue.ValueAsString = nextValue.StringValue;
	SetReflectionValue(nextValue, bBroadcast);
}

void UVdjmReflectionUiValueWidgetBase::SetReflectionValueFromString(EVdjmReflectionUiValueType valueType, const FString& valueString, int32 valueIndex, bool bBroadcast)
{
	switch (valueType)
	{
	case EVdjmReflectionUiValueType::EInt:
		SetReflectionIntValue(FCString::Atoi(*valueString), valueIndex, bBroadcast);
		return;
	case EVdjmReflectionUiValueType::EFloat:
		SetReflectionFloatValue(FCString::Atof(*valueString), valueIndex, bBroadcast);
		return;
	case EVdjmReflectionUiValueType::EBool:
		SetReflectionBoolValue(valueString.ToBool(), valueIndex, bBroadcast);
		return;
	case EVdjmReflectionUiValueType::EEnum:
	{
		FVdjmReflectionUiAnyValue nextValue;
		nextValue.ValueType = EVdjmReflectionUiValueType::EEnum;
		nextValue.ValueIndex = valueIndex;
		nextValue.TrackingId = mTrackingId;
		nextValue.StringValue = valueString;
		nextValue.NameValue = FName(*valueString);
		nextValue.ValueAsString = valueString;
		SetReflectionValue(nextValue, bBroadcast);
		return;
	}
	case EVdjmReflectionUiValueType::EIntPoint:
		SetReflectionIntPointValue(ParseIntPointValueString(valueString), valueIndex, bBroadcast);
		return;
	case EVdjmReflectionUiValueType::EString:
	default:
		SetReflectionStringValue(valueString, valueIndex, bBroadcast);
		return;
	}
}

void UVdjmReflectionUiValueWidgetBase::BroadcastReflectionValueChanged()
{
	OnReflectionValueChanged.Broadcast(this, mCurrentValue.ValueIndex, mCurrentValue);
}

bool UVdjmReflectionUiValueWidgetBase::UndoReflectionValue(bool bBroadcast)
{
	if (not CanUndoReflectionValue())
	{
		return false;
	}

	const FVdjmReflectionUiAnyValue previousValue = mUndoValues.Last();
	mUndoValues.Pop(EAllowShrinking::No);
	mRedoValues.Add(mCurrentValue);

	mApplyingReflectionValueHistory = true;
	SetReflectionValue(previousValue, bBroadcast);
	mApplyingReflectionValueHistory = false;
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::RedoReflectionValue(bool bBroadcast)
{
	if (not CanRedoReflectionValue())
	{
		return false;
	}

	const FVdjmReflectionUiAnyValue nextValue = mRedoValues.Last();
	mRedoValues.Pop(EAllowShrinking::No);
	PushUndoReflectionValue(mCurrentValue);

	mApplyingReflectionValueHistory = true;
	SetReflectionValue(nextValue, bBroadcast);
	mApplyingReflectionValueHistory = false;
	return true;
}

bool UVdjmReflectionUiValueWidgetBase::CanUndoReflectionValue() const
{
	return not mUndoValues.IsEmpty();
}

bool UVdjmReflectionUiValueWidgetBase::CanRedoReflectionValue() const
{
	return not mRedoValues.IsEmpty();
}

int32 UVdjmReflectionUiValueWidgetBase::GetUndoValueCount() const
{
	return mUndoValues.Num();
}

int32 UVdjmReflectionUiValueWidgetBase::GetRedoValueCount() const
{
	return mRedoValues.Num();
}

void UVdjmReflectionUiValueWidgetBase::ClearReflectionValueHistory()
{
	mUndoValues.Reset();
	mRedoValues.Reset();
}

void UVdjmReflectionUiValueWidgetBase::SetMaxUndoValueCount(int32 maxUndoValueCount)
{
	mMaxUndoValueCount = FMath::Max(0, maxUndoValueCount);
	TrimUndoReflectionValueHistory();
	if (mMaxUndoValueCount <= 0)
	{
		mRedoValues.Reset();
	}
}

int32 UVdjmReflectionUiValueWidgetBase::GetMaxUndoValueCount() const
{
	return mMaxUndoValueCount;
}

void UVdjmReflectionUiValueWidgetBase::SetLinkedObject(UObject* linkedObject)
{
	mLinkedObject = linkedObject;
}

UObject* UVdjmReflectionUiValueWidgetBase::GetLinkedObject() const
{
	return mLinkedObject.Get();
}

bool UVdjmReflectionUiValueWidgetBase::IsLinkedObjectValid() const
{
	return mLinkedObject.IsValid();
}

void UVdjmReflectionUiValueWidgetBase::SetTrackingId(int32 trackingId)
{
	mTrackingId = trackingId;
	mCurrentValue.TrackingId = mTrackingId;
	if (mCurrentValue.ValueIndex == INDEX_NONE)
	{
		mCurrentValue.ValueIndex = mTrackingId;
	}
}

int32 UVdjmReflectionUiValueWidgetBase::GetTrackingId() const
{
	return mTrackingId;
}

void UVdjmReflectionUiValueWidgetBase::NativeOnReflectionValueWidgetInitialized(UVdjmReflectionUiItemObject* itemObject)
{
}

bool UVdjmReflectionUiValueWidgetBase::NativeValidateReflectionValueWidgetItem(UVdjmReflectionUiItemObject* itemObject, FString& outErrorReason) const
{
	return true;
}

void UVdjmReflectionUiValueWidgetBase::HandleBoundNumericSliderValueChanged(float value)
{
	if (mApplyingBoundWidgetValue)
	{
		return;
	}
	SubmitNumericWidgetValue(value, true);
}

void UVdjmReflectionUiValueWidgetBase::HandleBoundBoolCheckBoxValueChanged(bool bValue)
{
	if (mApplyingBoundWidgetValue)
	{
		return;
	}
	SubmitBoolWidgetValue(bValue, true);
}

void UVdjmReflectionUiValueWidgetBase::HandleBoundTextBoxTextChanged(const FText& text)
{
	if (mApplyingBoundWidgetValue)
	{
		return;
	}
	SubmitTextWidgetValue(text.ToString(), true);
}

void UVdjmReflectionUiValueWidgetBase::HandleBoundComboBoxSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType)
{
	if (mApplyingBoundWidgetValue)
	{
		return;
	}
	SubmitTextWidgetValue(selectedItem, true);
}

void UVdjmReflectionUiValueWidgetBase::PushUndoReflectionValue(const FVdjmReflectionUiAnyValue& value)
{
	if (mMaxUndoValueCount <= 0)
	{
		return;
	}

	if (not mUndoValues.IsEmpty() && AreReflectionUiAnyValuesSame(mUndoValues.Last(), value))
	{
		return;
	}

	mUndoValues.Add(value);
	TrimUndoReflectionValueHistory();
}

void UVdjmReflectionUiValueWidgetBase::TrimUndoReflectionValueHistory()
{
	while (mUndoValues.Num() > mMaxUndoValueCount)
	{
		mUndoValues.RemoveAt(0, 1, EAllowShrinking::No);
	}
}

UVdjmReflectionUiItemObject* UVdjmReflectionUiListEntryWidgetBase::GetReflectionUiItem() const
{
	return mReflectionUiItem;
}

FText UVdjmReflectionUiListEntryWidgetBase::GetReflectionDisplayText() const
{
	return mDisplayText;
}

void UVdjmReflectionUiListEntryWidgetBase::SetReflectionDisplayText(FText displayText)
{
	mDisplayText = displayText;
	if (DisplayTextBlock != nullptr)
	{
		DisplayTextBlock->SetText(mDisplayText);
	}
}

bool UVdjmReflectionUiListEntryWidgetBase::SetReflectionValueWidget(UVdjmReflectionUiValueWidgetBase* valueWidget, UObject* linkedObject)
{
	ClearReflectionValueWidget();
	if (valueWidget == nullptr)
	{
		return false;
	}

	mReflectionValueWidget = valueWidget;
	if (ValueWidgetSlot != nullptr)
	{
		ValueWidgetSlot->AddChild(valueWidget);
	}

	return InitializeReflectionValueWidget(linkedObject);
}

bool UVdjmReflectionUiListEntryWidgetBase::InitializeReflectionValueWidget(UObject* linkedObject)
{
	if (mReflectionValueWidget == nullptr)
	{
		return false;
	}

	const bool initializationResult = mReflectionValueWidget->InitializeReflectionValueWidget(
		mReflectionUiItem,
		linkedObject,
		mReflectionUiItem != nullptr ? mReflectionUiItem->SortOrder : INDEX_NONE);
	if (not initializationResult)
	{
		ClearReflectionValueWidget();
		return false;
	}

	return true;
}

void UVdjmReflectionUiListEntryWidgetBase::ClearReflectionValueWidget()
{
	if (ValueWidgetSlot != nullptr)
	{
		ValueWidgetSlot->ClearChildren();
	}
	mReflectionValueWidget = nullptr;
}

UVdjmReflectionUiValueWidgetBase* UVdjmReflectionUiListEntryWidgetBase::GetReflectionValueWidget() const
{
	return mReflectionValueWidget;
}

void UVdjmReflectionUiListEntryWidgetBase::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	mReflectionUiItem = Cast<UVdjmReflectionUiItemObject>(ListItemObject);
	if (mReflectionUiItem != nullptr)
	{
		SetReflectionDisplayText(mReflectionUiItem->DisplayName);
	}
	else
	{
		SetReflectionDisplayText(FText::GetEmpty());
	}

	InitializeReflectionValueWidget();
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	BP_OnReflectionUiItemSet(mReflectionUiItem);
}

void UVdjmReflectionUiListEntryWidgetBase::NativeOnEntryReleased()
{
	mReflectionUiItem = nullptr;
	ClearReflectionValueWidget();
	SetReflectionDisplayText(FText::GetEmpty());
	IUserObjectListEntry::NativeOnEntryReleased();
	BP_OnReflectionUiItemReleased();
}
