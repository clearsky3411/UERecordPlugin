#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "VdjmVcardDescriptorTypes.generated.h"

class UVcardDescriptorBase;

UENUM(BlueprintType)
enum class EVcardDescriptorOpenPolicy : uint8
{
	EReplace,
	EAdd,
	EKeepIfSame,
	EHide
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardDescriptorApplyRequest
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "NamedSlot 또는 PanelWidget을 가진 대상 위젯입니다. 이 위젯의 내부 슬롯들이 descriptor에 의해 채워집니다."))
	TObjectPtr<UUserWidget> NamedSlotHostWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "descriptor 안의 attachment가 TargetSlotName을 비웠을 때만 사용하는 fallback 슬롯 이름입니다. 기본 사용에서는 attachment의 TargetSlotName을 명시하세요."))
	FName FallbackTargetSlotName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "생성된 위젯에 전달할 런타임 payload입니다. attachment의 PayloadData가 비어 있으면 이 값을 전달합니다."))
	TObjectPtr<UObject> PayloadData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "false면 descriptor가 위젯을 생성하지 않습니다. 일반적으로 true로 둡니다."))
	bool bAllowCreate = true;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardDescriptorApplyResult
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FName DescriptorKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	TArray<TObjectPtr<UUserWidget>> CreatedWidgets;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FString ErrorReason;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardWidgetAttachDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "기능에는 영향을 주지 않는 디버그용 이름입니다. 비워도 됩니다."))
	FName DebugName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "생성한 위젯을 붙일 NamedSlot 또는 PanelWidget 이름입니다."))
	FName TargetSlotName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "생성해서 TargetSlotName에 붙일 위젯 클래스입니다."))
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "이 attachment 전용 payload입니다. 비어 있으면 descriptor 호출 시 전달된 payload를 사용합니다."))
	TObjectPtr<UObject> PayloadData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FSoftObjectPath PayloadPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "기존 슬롯 내용을 교체할지, 추가할지, 같은 클래스면 유지할지 정합니다."))
	EVcardDescriptorOpenPolicy OpenPolicy = EVcardDescriptorOpenPolicy::EReplace;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "생성된 위젯이 VcardDescriptorReceiver를 구현하면 attachment 정보와 payload를 전달합니다."))
	bool bAutoApplyPayload = true;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardOptionItemData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	TSoftObjectPtr<UObject> Thumbnail;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	TSoftObjectPtr<UObject> PayloadObject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	FSoftObjectPath PayloadPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	TArray<FName> Tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	bool bRuntimeGenerated = false;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardToolDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	FName ToolId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	TSubclassOf<UUserWidget> ButtonWidgetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	TSoftObjectPtr<UObject> DefaultIcon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	TSoftObjectPtr<UObject> ActiveIcon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	FName CommandSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	FName TargetDescriptorKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	bool bLocksOtherTools = false;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardBottomSheetDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	FName DescriptorKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	TSubclassOf<UUserWidget> SheetWidgetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	TArray<FVcardWidgetAttachDescriptor> HeaderAttachments;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	FVcardWidgetAttachDescriptor ContentAttachment;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	float InitialOpenRatio = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	float MinOpenRatio = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	float MaxOpenRatio = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	TArray<float> SnapRatios;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	bool bAllowDrag = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	bool bAllowTapToggle = true;
};
