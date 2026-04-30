#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "UObject/Object.h"
#include "VdjmReflectionUiTypes.generated.h"

class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UPanelWidget;
class USlider;
class UVdjmReflectionUiValueWidgetBase;

/**
 * @brief Reflection UI를 구성하는 기본 흐름.
 *
 * 1. Reflection builder가 UObject/UStruct의 UPROPERTY를 읽고 UVdjmReflectionUiItemObject 배열을 만든다.
 * 2. UMG ListView는 UVdjmReflectionUiItemObject를 row object로 사용한다.
 * 3. UVdjmReflectionUiListEntryWidgetBase는 ListView row 껍데기다. DisplayName을 표시하고, BP_OnReflectionUiItemSet에서 값 위젯을 갈아끼우면 된다.
 * 4. 실제 slider, checkbox, text input, int point editor BP는 UVdjmReflectionUiValueWidgetBase를 상속한다.
 * 5. row BP가 value widget을 만든 뒤 SetReflectionValueWidget을 호출하면, row slot에 추가되고 value widget init까지 처리된다.
 * 6. value widget BP는 실제 UI 값이 바뀔 때 SetReflectionFloatValue 같은 타입별 setter를 호출한다.
 * 7. 상위 UI는 OnReflectionValueChanged를 받아 controller request, preview update, validation 등을 처리한다.
 */

/**
 * @brief Reflection으로 발견한 property 값을 어떤 종류의 UI 값으로 다룰지 나타낸다.
 */
UENUM(BlueprintType)
enum class EVdjmReflectionUiValueType : uint8
{
	EUnsupported UMETA(DisplayName = "Unsupported"),
	EInt UMETA(DisplayName = "Int"),
	EFloat UMETA(DisplayName = "Float"),
	EBool UMETA(DisplayName = "Bool"),
	EString UMETA(DisplayName = "String"),
	EEnum UMETA(DisplayName = "Enum"),
	EIntPoint UMETA(DisplayName = "Int Point")
};

/**
 * @brief Reflection UI row가 공통으로 주고받는 값 컨테이너다.
 *
 * slider, checkbox, text input처럼 외형은 서로 달라도 상위 UI에는 이 구조체 하나로 값을 전달한다.
 * 모든 필드가 항상 유효한 것은 아니며, ValueType에 맞는 필드를 읽는 방식으로 사용한다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmReflectionUiAnyValue
{
	GENERATED_BODY()

public:
	/** 값을 기본 상태로 되돌린다. ListView entry release나 null item 적용 때 사용한다. */
	void Reset();

	/** 현재 값이 int, float, bool, string, enum, int point 중 무엇인지 나타낸다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	EVdjmReflectionUiValueType ValueType = EVdjmReflectionUiValueType::EUnsupported;

	/** 같은 위젯 안에서 여러 값을 다룰 때 쓰는 값 번호다. 기본적으로 TrackingId로 채워진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	int32 ValueIndex = INDEX_NONE;

	/** row/value widget을 추적하기 위한 런타임 번호다. 기본 구현은 item의 SortOrder를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	int32 TrackingId = INDEX_NONE;

	/** ValueType이 EInt일 때 주로 읽는 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	int32 IntValue = 0;

	/** ValueType이 EFloat일 때 주로 읽는 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	float FloatValue = 0.0f;

	/** ValueType이 EBool일 때 주로 읽는 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	bool bBoolValue = false;

	/** ValueType이 EString 또는 EEnum일 때 주로 읽는 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	FString StringValue;

	/** string 값을 name key처럼 써야 할 때 보조로 읽는 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	FName NameValue = NAME_None;

	/** ValueType이 EIntPoint일 때 주로 읽는 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	FIntPoint IntPointValue = FIntPoint::ZeroValue;

	/** controller request나 debug 출력에 바로 쓰기 쉬운 문자열 형태 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Value")
	FString ValueAsString;
};

/**
 * @brief Slider/SpinBox 계열 BP 위젯에 넘기기 쉬운 범위/현재값 패킷이다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmReflectionUiNumericWidgetState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Numeric")
	float MinValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Numeric")
	float MaxValue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Numeric")
	float CurrentValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Numeric")
	bool bHasMinValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Numeric")
	bool bHasMaxValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Numeric")
	bool bIsInteger = false;
};

/**
 * @brief TextBox/ComboBox 계열 BP 위젯에 넘기기 쉬운 문자열 상태 패킷이다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmReflectionUiTextWidgetState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Text")
	FText DisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Text")
	FString CurrentString;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Text")
	TArray<FString> Options;
};

/**
 * @brief CheckBox/Toggle 계열 BP 위젯에 넘기기 쉬운 bool 상태 패킷이다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmReflectionUiBoolWidgetState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|Bool")
	bool bCurrentValue = false;
};

/**
 * @brief IntPoint/Resolution 계열 BP 위젯에 넘기기 쉬운 X/Y 상태 패킷이다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmReflectionUiIntPointWidgetState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|IntPoint")
	FIntPoint CurrentValue = FIntPoint::ZeroValue;
};

/** 실제 값 입력 위젯이 값 변경을 상위 UI에 알릴 때 사용하는 공통 delegate다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FVdjmReflectionUiValueChangedSignature,
	UVdjmReflectionUiValueWidgetBase*, sourceWidget,
	int32, valueIndex,
	FVdjmReflectionUiAnyValue, value);

/**
 * @brief UPROPERTY metadata와 FProperty 타입에서 추출한 런타임 UI descriptor다.
 *
 * 이 객체는 특정 recorder 기능에 묶이지 않는다. UMG ListView의 item으로 넘기기 위해 UObject로 제공한다.
 * 이 객체는 "값 입력 위젯" 자체가 아니라, 어떤 값을 어떤 이름과 타입으로 보여줄지 알려주는 데이터다.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMRECORDER_API UVdjmReflectionUiItemObject : public UObject
{
	GENERATED_BODY()

public:
	/** controller request 등에 사용할 property key를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi")
	FName GetOptionKey() const;

	/** debug 표시용 값 타입 이름을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi")
	FString GetValueTypeName() const;

	/** UI label에 표시할 FText 이름을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi")
	FText GetUiDisplayNameText() const;

	/** UI label에 표시할 이름을 FString으로 반환한다. Print/String 비교용이다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi")
	FString GetUiDisplayNameString() const;

	/** 현재 descriptor 상태를 한 줄 문자열로 반환한다. BP debug용이다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi")
	FString GetDebugSummary() const;

	/** 실제 옵션/property를 식별하는 key다. controller request의 대상 이름으로 사용한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	FName OptionKey = NAME_None;

	/** 중첩 struct property를 표현할 때 쓰는 경로 문자열이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	FString PropertyPath;

	/** row label에 표시할 이름이다. 추후 string table로 교체되더라도 FText 기준으로 유지한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	FText DisplayName;

	/** UI에서 그룹을 나누고 싶을 때 쓰는 category 값이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	FName Category = NAME_None;

	/** 이 property가 어떤 값 위젯으로 편집되어야 하는지 나타내는 기본 타입이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	EVdjmReflectionUiValueType ValueType = EVdjmReflectionUiValueType::EUnsupported;

	/** request struct 내부에서 값을 담는 property 이름이다. 보통 Value를 사용한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	FString ValuePropertyName = TEXT("Value");

	/** 기본값 또는 현재값을 문자열로 저장한 값이다. value widget 초기화에 사용한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	FString ValueAsString;

	/** slider/spinbox 같은 범위형 UI의 최소값이다. bHasMinValue가 true일 때만 사용한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	float MinValue = 0.0f;

	/** slider/spinbox 같은 범위형 UI의 최대값이다. bHasMaxValue가 true일 때만 사용한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	float MaxValue = 0.0f;

	/** MinValue가 의미 있는지 나타낸다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	bool bHasMinValue = false;

	/** MaxValue가 의미 있는지 나타낸다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	bool bHasMaxValue = false;

	/** ValueType보다 더 구체적인 BP 위젯 선택용 힌트다. 예: Slider, ComboBox, TextBox. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	FName UiType = NAME_None;

	/** UI 정렬과 기본 TrackingId로 사용한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	int32 SortOrder = 0;

	/** 고급 옵션으로 접어둘지 결정하는 힌트다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	bool bAdvanced = false;

	/** true면 기본 UI 생성 대상에서 제외할 수 있다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	bool bHidden = false;

	/** enum/choice UI가 표시할 후보 문자열이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vdjm|ReflectionUi")
	TArray<FString> EnumOptions;
};

/**
 * @brief Slider, checkbox, text input 같은 실제 값 입력 위젯들이 상속하는 공통 base다.
 *
 * ListView row는 이 위젯을 담는 껍데기 역할만 하고, 값 변경/검증/연결 UObject 추적은 이 base가 맡는다.
 *
 * BP 사용법:
 * - BP_OptionSlider, BP_OptionCheckBox, BP_OptionText 같은 위젯의 부모 클래스로 사용한다.
 * - row BP가 이 위젯을 생성한 뒤 보통은 ListEntry의 SetReflectionValueWidget(valueWidget)를 호출한다.
 * - 직접 초기화해야 한다면 InitializeReflectionValueWidget(itemObject)를 호출한다.
 * - InitializeReflectionValueWidget 이후 BP_OnReflectionValueWidgetInitialized에서 slider 범위, checkbox 기본값, text 기본값을 세팅한다.
 * - 사용자가 값을 바꾸면 SetReflectionFloatValue, SetReflectionBoolValue, SetReflectionStringValue 같은 setter를 호출한다.
 * - 상위 UI는 OnReflectionValueChanged를 받아 controller request로 변환한다.
 * - UndoReflectionValue/RedoReflectionValue는 이 value widget 내부 값 변경만 되돌린다. 실제 slider 표시값도 바꾸려면 BP가 OnReflectionValueChanged를 받아 UI 표시를 동기화해야 한다.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class VDJMRECORDER_API UVdjmReflectionUiValueWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** item descriptor, linked object, tracking id를 한 번에 적용하는 명시적 init 함수다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	bool InitializeReflectionValueWidget(UVdjmReflectionUiItemObject* itemObject, UObject* linkedObject = nullptr, int32 trackingId = -1);

	/** item descriptor를 이 값 위젯에 적용한다. 기존 BP 호환용이며 내부적으로 InitializeReflectionValueWidget을 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	bool ApplyReflectionUiItem(UVdjmReflectionUiItemObject* itemObject);

	/** 현재 이 value widget이 편집 중인 descriptor를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget")
	UVdjmReflectionUiItemObject* GetReflectionUiItem() const;

	/** 현재 저장된 공통 값 구조체를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget")
	FVdjmReflectionUiAnyValue GetCurrentReflectionValue() const;

	/** 현재 값을 request/debug에 쓰기 쉬운 문자열로 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget")
	FString GetCurrentReflectionValueString() const;

	/** 이 value widget이 itemObject를 받아도 되는지 확인한다. false면 초기화 실패로 처리할 수 있다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|Validation")
	bool ValidateReflectionValueWidgetItem(UVdjmReflectionUiItemObject* itemObject, FString& outErrorReason) const;

	/** InitializeReflectionValueWidget이 호출된 적 있는지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|Validation")
	bool IsReflectionValueWidgetInitialized() const;

	/** 마지막 InitializeReflectionValueWidget이 성공했는지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|Validation")
	bool IsReflectionValueWidgetInitializationValid() const;

	/** 마지막 초기화 실패 이유를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|Validation")
	FString GetReflectionValueWidgetInitializationError() const;

	/** 현재 item이 slider/spinbox에 들어갈 수 있으면 범위와 현재값을 채워준다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|Numeric")
	bool GetNumericWidgetState(FVdjmReflectionUiNumericWidgetState& outState, FString& outErrorReason) const;

	/** slider/spinbox 변경값을 현재 item 타입에 맞게 submit한다. int item이면 반올림해서 int로 보낸다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|Numeric")
	bool SubmitNumericWidgetValue(float value, bool bBroadcast = true);

	/** 현재 item이 text/combo에 들어갈 수 있으면 문자열 상태를 채워준다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|Text")
	bool GetTextWidgetState(FVdjmReflectionUiTextWidgetState& outState, FString& outErrorReason) const;

	/** text/combo 변경값을 현재 item 타입에 맞게 submit한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|Text")
	bool SubmitTextWidgetValue(const FString& value, bool bBroadcast = true);

	/** 현재 item이 checkbox/toggle에 들어갈 수 있으면 bool 상태를 채워준다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|Bool")
	bool GetBoolWidgetState(FVdjmReflectionUiBoolWidgetState& outState, FString& outErrorReason) const;

	/** checkbox/toggle 변경값을 submit한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|Bool")
	bool SubmitBoolWidgetValue(bool bValue, bool bBroadcast = true);

	/** 현재 item이 intpoint/resolution editor에 들어갈 수 있으면 X/Y 상태를 채워준다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|IntPoint")
	bool GetIntPointWidgetState(FVdjmReflectionUiIntPointWidgetState& outState, FString& outErrorReason) const;

	/** intpoint/resolution editor 변경값을 submit한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|IntPoint")
	bool SubmitIntPointWidgetValue(FIntPoint value, bool bBroadcast = true);

	/** BP가 만든 Slider를 넘기면 min/max/current 적용과 OnValueChanged bind까지 처리한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	bool BindNumericSlider(USlider* sliderWidget, bool bBindValueChanged = true, bool bApplyCurrentState = true);

	/** BP가 만든 CheckBox를 넘기면 current 적용과 OnCheckStateChanged bind까지 처리한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	bool BindBoolCheckBox(UCheckBox* checkBoxWidget, bool bBindValueChanged = true, bool bApplyCurrentState = true);

	/** BP가 만든 EditableTextBox를 넘기면 current 적용과 OnTextChanged bind까지 처리한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	bool BindTextBox(UEditableTextBox* textBoxWidget, bool bBindValueChanged = true, bool bApplyCurrentState = true);

	/** BP가 만든 ComboBoxString을 넘기면 option/current 적용과 OnSelectionChanged bind까지 처리한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	bool BindComboBoxString(UComboBoxString* comboBoxWidget, bool bBindValueChanged = true, bool bApplyCurrentState = true);

	/** bind된 실제 UMG 입력 위젯들의 delegate를 해제하고 참조를 비운다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	void UnbindValueInputWidgets();

	/** 현재 mCurrentValue를 bind된 실제 UMG 입력 위젯들에 다시 반영한다. undo/redo 동기화에도 사용한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	bool ApplyCurrentValueToBoundWidgets();

	/** 이미 만들어진 공통 값 구조체를 통째로 적용한다. 특수 위젯에서 직접 값을 구성할 때 사용한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void SetReflectionValue(const FVdjmReflectionUiAnyValue& value, bool bBroadcast = true);

	/** text input, enum combo 같은 문자열형 위젯에서 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void SetReflectionStringValue(const FString& value, int32 valueIndex = -1, bool bBroadcast = true);

	/** int spinbox, int slider 같은 정수형 위젯에서 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void SetReflectionIntValue(int32 value, int32 valueIndex = -1, bool bBroadcast = true);

	/** float slider, float spinbox 같은 실수형 위젯에서 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void SetReflectionFloatValue(float value, int32 valueIndex = -1, bool bBroadcast = true);

	/** checkbox, toggle 같은 bool 위젯에서 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void SetReflectionBoolValue(bool bValue, int32 valueIndex = -1, bool bBroadcast = true);

	/** 해상도처럼 X/Y를 같이 편집하는 위젯에서 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void SetReflectionIntPointValue(FIntPoint value, int32 valueIndex = -1, bool bBroadcast = true);

	/** descriptor의 ValueAsString을 타입에 맞게 공통 값 구조체로 변환할 때 사용한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void SetReflectionValueFromString(EVdjmReflectionUiValueType valueType, const FString& valueString, int32 valueIndex = -1, bool bBroadcast = true);

	/** 현재 저장된 값을 다시 방송한다. 외부에서 수동 submit 버튼을 만들 때 사용할 수 있다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void BroadcastReflectionValueChanged();

	/** 이전 값이 있으면 현재 값을 undo하고 변경 delegate를 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	bool UndoReflectionValue(bool bBroadcast = true);

	/** undo로 되돌린 값이 있으면 다시 redo하고 변경 delegate를 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	bool RedoReflectionValue(bool bBroadcast = true);

	/** undo 가능한 값이 있는지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	bool CanUndoReflectionValue() const;

	/** redo 가능한 값이 있는지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	bool CanRedoReflectionValue() const;

	/** 현재 undo stack 개수를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	int32 GetUndoValueCount() const;

	/** 현재 redo stack 개수를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	int32 GetRedoValueCount() const;

	/** 초기화나 외부 apply 이후 값 변경 기록을 비운다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	void ClearReflectionValueHistory();

	/** undo stack 최대 개수를 지정한다. 0 이하이면 기록을 쌓지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	void SetMaxUndoValueCount(int32 maxUndoValueCount);

	/** undo stack 최대 개수를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	int32 GetMaxUndoValueCount() const;

	/** 이 value widget이 편집하거나 감시하는 UObject를 약하게 연결한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void SetLinkedObject(UObject* linkedObject);

	/** 연결된 UObject를 반환한다. 이미 GC되었으면 nullptr일 수 있다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget")
	UObject* GetLinkedObject() const;

	/** 연결된 UObject가 아직 유효한지 확인한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget")
	bool IsLinkedObjectValid() const;

	/** 이 value widget을 추적하기 위한 번호를 직접 지정한다. 기본값은 item SortOrder다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ValueWidget")
	void SetTrackingId(int32 trackingId);

	/** 현재 추적 번호를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ValueWidget")
	int32 GetTrackingId() const;

	/** 값 변경 시 호출된다. 상위 UI는 여기서 controller request나 preview 갱신을 처리한다. */
	UPROPERTY(BlueprintAssignable, Category = "Vdjm|ReflectionUi|ValueWidget")
	FVdjmReflectionUiValueChangedSignature OnReflectionValueChanged;

protected:
	/** InitializeReflectionValueWidget 이후 C++ 파생 클래스가 범위/표시/검증 상태를 초기화할 수 있는 hook이다. */
	virtual void NativeOnReflectionValueWidgetInitialized(UVdjmReflectionUiItemObject* itemObject);

	/** 파생 클래스가 추가 검증을 넣고 싶을 때 override한다. */
	virtual bool NativeValidateReflectionValueWidgetItem(UVdjmReflectionUiItemObject* itemObject, FString& outErrorReason) const;

	/** InitializeReflectionValueWidget 이후 BP에서 외형 초기화, 범위 적용, 기본값 표시를 할 수 있는 hook이다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vdjm|ReflectionUi|ValueWidget", meta = (DisplayName = "On Reflection Value Widget Initialized"))
	void BP_OnReflectionValueWidgetInitialized(UVdjmReflectionUiItemObject* itemObject);

	/** ApplyReflectionUiItem 이후 BP에서 외형 초기화, 범위 적용, 기본값 표시를 할 수 있는 hook이다. 기존 BP 호환용이다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vdjm|ReflectionUi|ValueWidget", meta = (DisplayName = "On Reflection UI Item Applied"))
	void BP_OnReflectionUiItemApplied(UVdjmReflectionUiItemObject* itemObject);

	UFUNCTION()
	void HandleBoundNumericSliderValueChanged(float value);

	UFUNCTION()
	void HandleBoundBoolCheckBoxValueChanged(bool bValue);

	UFUNCTION()
	void HandleBoundTextBoxTextChanged(const FText& text);

	UFUNCTION()
	void HandleBoundComboBoxSelectionChanged(FString selectedItem, ESelectInfo::Type selectionType);

	void PushUndoReflectionValue(const FVdjmReflectionUiAnyValue& value);
	void TrimUndoReflectionValueHistory();

	/** 현재 적용된 item descriptor다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget")
	TObjectPtr<UVdjmReflectionUiItemObject> mReflectionUiItem;

	/** 현재 이 value widget이 들고 있는 값이다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget")
	FVdjmReflectionUiAnyValue mCurrentValue;

	/** 상태 감시용 약한 UObject 참조다. BP에는 함수로만 노출한다. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> mLinkedObject;

	/** 이 value widget의 추적 번호다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget")
	int32 mTrackingId = INDEX_NONE;

	/** 비어 있으면 EUnsupported가 아닌 모든 타입을 허용한다. Slider BP라면 EFloat/EInt만 넣는 식으로 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|ValueWidget|Validation")
	TArray<EVdjmReflectionUiValueType> mSupportedValueTypes;

	/** InitializeReflectionValueWidget이 호출된 적 있는지 나타낸다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget|Validation")
	bool mInitialized = false;

	/** 마지막 InitializeReflectionValueWidget 검증이 성공했는지 나타낸다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget|Validation")
	bool mInitializationValid = false;

	/** 마지막 InitializeReflectionValueWidget 실패 이유다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget|Validation")
	FString mInitializationError;

	/** BP subclass가 넘긴 실제 numeric slider다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	TObjectPtr<USlider> mBoundNumericSlider;

	/** BP subclass가 넘긴 실제 bool checkbox다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	TObjectPtr<UCheckBox> mBoundBoolCheckBox;

	/** BP subclass가 넘긴 실제 text box다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	TObjectPtr<UEditableTextBox> mBoundTextBox;

	/** BP subclass가 넘긴 실제 combo box다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget|Bind")
	TObjectPtr<UComboBoxString> mBoundComboBoxString;

	/** undo 시 되돌아갈 이전 값들이다. 사용자 변경으로 broadcast되는 값만 기록한다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	TArray<FVdjmReflectionUiAnyValue> mUndoValues;

	/** redo 시 다시 적용할 값들이다. 새 값이 들어오면 비운다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	TArray<FVdjmReflectionUiAnyValue> mRedoValues;

	/** undo 기록 최대 개수다. 기본값은 가벼운 UI 변경을 고려해 32개로 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vdjm|ReflectionUi|ValueWidget|History")
	int32 mMaxUndoValueCount = 32;

	/** undo/redo 적용 중 다시 history가 쌓이지 않게 막는 내부 플래그다. */
	UPROPERTY(Transient)
	bool mApplyingReflectionValueHistory = false;

	/** 실제 UMG 위젯에 값을 반영하는 중 다시 OnChanged가 들어와도 submit하지 않게 막는다. */
	UPROPERTY(Transient)
	bool mApplyingBoundWidgetValue = false;
};

/**
 * @brief Reflection UI item을 받는 ListView entry widget용 C++ base다.
 *
 * BP에서는 이 클래스를 부모로 삼아 row widget을 만들고, ListView의 Entry Widget Class로 지정하면 된다.
 *
 * 이 클래스는 "값을 편집하는 위젯"이 아니다. row의 label과 value widget을 담는 껍데기다.
 *
 * BP 사용법:
 * - row BP의 부모 클래스로 사용한다.
 * - label을 자동으로 표시하려면 TextBlock 이름을 DisplayTextBlock으로 만든다.
 * - value widget을 자동으로 담고 싶으면 PanelWidget 이름을 ValueWidgetSlot으로 만든다. Overlay, CanvasPanel, HorizontalBox 같은 Panel 계열이면 된다.
 * - BP_OnReflectionUiItemSet에서 itemObject의 ValueType 또는 UiType을 보고 slider/checkbox/text value widget BP를 생성한다.
 * - 생성한 value widget은 UVdjmReflectionUiValueWidgetBase를 상속해야 하고, 생성 직후 SetReflectionValueWidget(valueWidget)를 호출한다.
 * - value widget의 OnReflectionValueChanged에 상위 UI 또는 row BP가 바인드해서 값을 전달한다.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class VDJMRECORDER_API UVdjmReflectionUiListEntryWidgetBase : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** ListView가 현재 row에 넘긴 item descriptor를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ListEntry")
	UVdjmReflectionUiItemObject* GetReflectionUiItem() const;

	/** row label에 표시 중인 텍스트를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ListEntry")
	FText GetReflectionDisplayText() const;

	/** row label 텍스트를 설정한다. DisplayTextBlock이 있으면 즉시 반영된다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ListEntry")
	void SetReflectionDisplayText(FText displayText);

	/** ValueWidgetSlot에 value widget을 넣고 현재 item descriptor로 초기화한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ListEntry")
	bool SetReflectionValueWidget(UVdjmReflectionUiValueWidgetBase* valueWidget, UObject* linkedObject = nullptr);

	/** 현재 row의 value widget을 item descriptor로 다시 초기화한다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ListEntry")
	bool InitializeReflectionValueWidget(UObject* linkedObject = nullptr);

	/** ValueWidgetSlot과 현재 value widget 참조를 비운다. */
	UFUNCTION(BlueprintCallable, Category = "Vdjm|ReflectionUi|ListEntry")
	void ClearReflectionValueWidget();

	/** 현재 row에 연결된 value widget을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Vdjm|ReflectionUi|ListEntry")
	UVdjmReflectionUiValueWidgetBase* GetReflectionValueWidget() const;

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;

	/** ListView item이 row에 들어왔을 때 BP에서 value widget switching을 처리하는 hook이다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vdjm|ReflectionUi|ListEntry", meta = (DisplayName = "On Reflection UI Item Set"))
	void BP_OnReflectionUiItemSet(UVdjmReflectionUiItemObject* itemObject);

	/** ListView row가 release될 때 BP에서 생성한 value widget 정리 등을 처리하는 hook이다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vdjm|ReflectionUi|ListEntry", meta = (DisplayName = "On Reflection UI Item Released"))
	void BP_OnReflectionUiItemReleased();

	/** BP row 안에 같은 이름의 TextBlock을 만들면 DisplayName이 자동으로 들어간다. 없어도 된다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vdjm|ReflectionUi|ListEntry")
	TObjectPtr<UTextBlock> DisplayTextBlock;

	/** BP row 안에 같은 이름의 PanelWidget을 만들면 value widget이 여기에 자동으로 들어간다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vdjm|ReflectionUi|ListEntry")
	TObjectPtr<UPanelWidget> ValueWidgetSlot;

	/** ListView가 넘긴 item descriptor 캐시다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ListEntry")
	TObjectPtr<UVdjmReflectionUiItemObject> mReflectionUiItem;

	/** 현재 row slot에 연결된 value widget이다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ListEntry")
	TObjectPtr<UVdjmReflectionUiValueWidgetBase> mReflectionValueWidget;

	/** 현재 row label 텍스트다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Vdjm|ReflectionUi|ListEntry")
	FText mDisplayText;
};
