#pragma once

#include "CoreMinimal.h"
#include "VdjmEvents/VdjmRecordEventNode.h"
#include "VdjmVcardDescriptorEventNodes.generated.h"

class UUserWidget;
class UVcardDescriptorRegistryDataAsset;

/**
 * Applies a V-card descriptor to a widget found from the flow runtime/context keys.
 *
 * Responsibility:
 * - Resolve an existing UUserWidget host from RuntimeSlotKey/ContextKey.
 * - Ask DescriptorRegistryDataAsset[DescriptorKey] to fill the host widget's named slots/panels.
 *
 * Must not:
 * - Require V-card widgets to inherit from a custom base class.
 * - Own long-lived UI state; the generated widgets are owned by their UMG slots/panels.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "Flow에서 Vcard descriptor를 특정 UUserWidget의 NamedSlot/PanelWidget에 적용합니다."))
class VDJMVCARD_API UVcardEventApplyDescriptorNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Lookup", meta = (ToolTip = "현재 flow session 안에서만 쓰는 로컬 위젯 키입니다. LookupPolicy가 runtime slot을 볼 때 사용합니다."))
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Lookup", meta = (ToolTip = "WorldContextSubsystem에 등록된 글로벌 위젯 키입니다. LookupPolicy가 context를 볼 때 사용합니다. None이면 RuntimeSlotKey를 context key처럼 사용할 수 있습니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Lookup", meta = (ToolTip = "대상 host widget을 RuntimeSlotKey와 ContextKey 중 어디에서 찾을지 정합니다."))
	EVdjmRecordEventWidgetLookupPolicy LookupPolicy = EVdjmRecordEventWidgetLookupPolicy::ERuntimeSlotThenContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Descriptor", meta = (ToolTip = "필수입니다. DescriptorKey를 찾을 중앙 Vcard descriptor registry data asset입니다."))
	TObjectPtr<UVcardDescriptorRegistryDataAsset> DescriptorRegistryDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Descriptor", meta = (ToolTip = "필수입니다. DescriptorRegistryDataAsset의 map에서 실행할 descriptor key입니다."))
	FName DescriptorKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Descriptor", meta = (ToolTip = "선택입니다. descriptor 내부 attachment의 TargetSlotName이 None일 때만 대신 사용할 슬롯 이름입니다."))
	FName FallbackTargetSlotName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Descriptor", meta = (ToolTip = "선택입니다. 생성된 위젯에 전달할 런타임 데이터입니다. attachment payload가 비어 있으면 이 값을 전달합니다."))
	TObjectPtr<UObject> PayloadData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Behavior", meta = (ToolTip = "context에서 위젯을 찾았고 RuntimeSlotKey가 있으면 현재 flow session에도 같은 위젯을 캐시합니다."))
	bool bStoreContextResultInRuntimeSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Behavior", meta = (ToolTip = "true면 host/registry/descriptor가 없거나 적용에 실패해도 flow는 성공으로 진행합니다."))
	bool bSucceedIfMissing = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Debug", meta = (ToolTip = "디버그용입니다. true면 적용 결과를 LogVdjmVcard에 남깁니다."))
	bool bLogResult = true;
};

/**
 * Clears or hides one named slot/panel on a V-card host widget.
 *
 * Responsibility:
 * - Resolve an existing UUserWidget host from RuntimeSlotKey/ContextKey.
 * - Clear or collapse a named slot/panel by name.
 *
 * Must not:
 * - Decide which descriptor should replace the cleared area. Use UVcardEventApplyDescriptorNode next.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "Flow에서 특정 UUserWidget의 NamedSlot 또는 PanelWidget 내용을 비우거나 숨깁니다."))
class VDJMVCARD_API UVcardEventClearDescriptorSlotNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Lookup", meta = (ToolTip = "현재 flow session 안에서만 쓰는 로컬 위젯 키입니다. LookupPolicy가 runtime slot을 볼 때 사용합니다."))
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Lookup", meta = (ToolTip = "WorldContextSubsystem에 등록된 글로벌 위젯 키입니다. LookupPolicy가 context를 볼 때 사용합니다. None이면 RuntimeSlotKey를 context key처럼 사용할 수 있습니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Lookup", meta = (ToolTip = "대상 host widget을 RuntimeSlotKey와 ContextKey 중 어디에서 찾을지 정합니다."))
	EVdjmRecordEventWidgetLookupPolicy LookupPolicy = EVdjmRecordEventWidgetLookupPolicy::ERuntimeSlotThenContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Descriptor", meta = (ToolTip = "필수입니다. 비우거나 숨길 NamedSlot 또는 PanelWidget 이름입니다."))
	FName TargetSlotName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Behavior", meta = (ToolTip = "true면 내용을 제거하지 않고 대상 slot/panel visibility만 Collapsed로 둡니다. false면 slot content 또는 panel children을 제거합니다."))
	bool bHideInsteadOfRemove = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Behavior", meta = (ToolTip = "true면 host나 TargetSlotName을 찾지 못해도 flow는 성공으로 진행합니다."))
	bool bSucceedIfMissing = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|Debug", meta = (ToolTip = "디버그용입니다. true면 clear/hide 결과를 LogVdjmVcard에 남깁니다."))
	bool bLogResult = true;
};
