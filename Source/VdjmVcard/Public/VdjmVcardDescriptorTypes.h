#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "VdjmVcardDescriptorTypes.generated.h"

class UVcardDescriptorBase;
class UVcardDescriptorRegistryDataAsset;

UENUM(BlueprintType)
enum class EVcardDescriptorOpenPolicy : uint8
{
	EReplace,
	EAdd,
	EKeepIfSame,
	EHide,
	ECacheSwap
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardDescriptorApplyRequest
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "NamedSlot 또는 PanelWidget을 가진 대상 위젯입니다. 이 위젯의 내부 슬롯들이 descriptor에 의해 채워집니다."))
	TObjectPtr<UUserWidget> NamedSlotHostWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "ECacheSwap에서 캐시 생명주기를 묶을 owner 위젯입니다. 비워두면 NamedSlotHostWidget을 owner로 사용합니다. owner가 사라지면 해당 캐시도 폐기됩니다."))
	TObjectPtr<UUserWidget> CacheOwnerWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "descriptor 안의 attachment가 TargetSlotName을 비웠을 때만 사용하는 fallback 슬롯 이름입니다. 기본 사용에서는 attachment의 TargetSlotName을 명시하세요."))
	FName FallbackTargetSlotName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "현재 실행 중인 descriptor key입니다. ECacheSwap에서 CacheEntryKey가 비었을 때 재사용 key 후보로 사용합니다."))
	FName DescriptorKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "attachment의 PayloadDescriptorKey를 실제 UObject payload로 해석할 때 사용할 descriptor registry입니다. 일반 사용에서는 descriptor applier가 자동으로 채웁니다."))
	TObjectPtr<UVcardDescriptorRegistryDataAsset> DescriptorRegistryDataAsset;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "ECacheSwap에서 논리 슬롯을 식별하는 key입니다. 비워두면 TargetSlotName을 사용합니다. 같은 owner 아래 다른 host widget 사이에서 같은 캐시를 공유하려면 명시하세요."))
	FName CacheSlotKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "ECacheSwap에서 이 attachment가 만든 위젯을 식별하는 key입니다. 비워두면 DescriptorKey, DebugName, WidgetClass 순서로 fallback합니다."))
	FName CacheEntryKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "생성해서 TargetSlotName에 붙일 위젯 클래스입니다."))
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "이 attachment 전용 payload입니다. 비어 있으면 descriptor 호출 시 전달된 payload를 사용합니다."))
	TObjectPtr<UObject> PayloadData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "PayloadData가 비어 있을 때 registry에서 이 key의 descriptor를 찾아 생성된 위젯에 payload로 전달합니다. 같은 위젯 클래스를 서로 다른 데이터 구성으로 재사용할 때 사용합니다."))
	FName PayloadDescriptorKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FSoftObjectPath PayloadPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "기존 슬롯 내용을 교체할지, 추가할지, 같은 클래스면 유지할지 정합니다."))
	EVcardDescriptorOpenPolicy OpenPolicy = EVcardDescriptorOpenPolicy::EReplace;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "ECacheSwap에서 위젯이 비활성화될 때 media/resource 해제를 요청할지 정합니다. 인터페이스를 구현한 위젯만 이 값을 사용합니다."))
	bool bReleaseResourcesOnCacheDeactivate = true;

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
