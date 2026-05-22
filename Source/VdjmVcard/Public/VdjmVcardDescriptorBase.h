#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardDescriptorBase.generated.h"

/**
 * Base object for descriptor-driven V-card UI composition.
 *
 * Responsibility:
 * - Carry the rules for generating widgets into a host widget's named slots or panels.
 * - Provide the default descriptor behavior used by Blueprint helpers.
 *
 * Must not:
 * - Contain final visual layout; widgets own their actual NamedSlot positions.
 * - Own long-lived screen state or undo/redo stacks.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardDescriptorBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	bool ApplyToWidget(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	virtual bool CanApplyToWidget(const FVcardDescriptorApplyRequest& request, FString& outReason) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	virtual bool GenerateWidgetsIntoNamedSlots(
		UUserWidget* namedSlotHostWidget,
		UObject* payloadData,
		TArray<UUserWidget*>& outCreatedWidgets,
		FString& outErrorReason);

	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	FText GetDisplayName() const { return DisplayName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	FName GetDebugName() const { return DebugName; }

protected:
	virtual bool ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "기능에는 영향을 주지 않는 디버그용 이름입니다. DataAsset map key가 실제 lookup 기준입니다."))
	FName DebugName = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "true면 하나의 attachment가 실패했을 때 즉시 중단합니다."))
	bool bStopOnFirstFailure = true;
};

/**
 * Descriptor for the common one-slot case.
 *
 * Responsibility:
 * - Create one widget and attach it to one named slot or panel on the host widget.
 *
 * Must not:
 * - Own the created widget after attach; UMG slot/panel ownership keeps it alive.
 * - Compose multiple regions. Use UVcardCompositeDescriptor for that.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardSingleSlotWidgetDescriptor : public UVcardDescriptorBase
{
	GENERATED_BODY()

public:
	UVcardSingleSlotWidgetDescriptor();

	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	FVcardWidgetAttachDescriptor GetSlotAttachment() const { return SlotAttachment; }

protected:
	virtual bool ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "TargetSlotName에 WidgetClass를 생성해서 붙이는 단일 슬롯 설정입니다."))
	FVcardWidgetAttachDescriptor SlotAttachment;
};

/**
 * Descriptor that creates child widgets and attaches them to named slots/panels.
 *
 * Responsibility:
 * - Apply a list of attachment descriptors in order.
 *
 * Must not:
 * - Guess target widget names. Slot names must be explicit descriptor data.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardWidgetCompositionDescriptor : public UVcardDescriptorBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	const TArray<FVcardWidgetAttachDescriptor>& GetAttachments() const { return Attachments; }

protected:
	virtual bool ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor", meta = (ToolTip = "이 descriptor가 host widget 안에 생성해서 붙일 위젯 목록입니다."))
	TArray<FVcardWidgetAttachDescriptor> Attachments;
};

/**
 * Descriptor that runs child descriptors in order.
 *
 * Responsibility:
 * - Group small descriptors into one reusable descriptor entry.
 *
 * Must not:
 * - Store runtime screen state. It only forwards the same apply request to its children.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardCompositeDescriptor : public UVcardDescriptorBase
{
	GENERATED_BODY()

public:
	UVcardCompositeDescriptor();

	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	TArray<UVcardDescriptorBase*> GetChildDescriptorList() const;

protected:
	virtual bool ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Vcard|Descriptor", meta = (ToolTip = "순서대로 실행할 하위 descriptor입니다. 보통 SingleSlot descriptor들을 묶을 때 사용합니다."))
	TArray<TObjectPtr<UVcardDescriptorBase>> ChildDescriptors;
};

/**
 * Descriptor that creates widgets from child descriptors without attaching them.
 *
 * Responsibility:
 * - Run child descriptors with ECreateOnly so a receiver widget can arrange the generated widgets.
 *
 * Must not:
 * - Attach generated widgets by itself. A group/panel widget owns the final arrangement rule.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardWidgetFactoryGroupDescriptor : public UVcardDescriptorBase
{
	GENERATED_BODY()

public:
	UVcardWidgetFactoryGroupDescriptor();

	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	TArray<UVcardDescriptorBase*> GetFactoryDescriptorList() const;

protected:
	virtual bool ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Vcard|Descriptor", meta = (ToolTip = "순서대로 실행할 생성용 descriptor입니다. 이 descriptor 안에서는 child가 ECreateOnly로 실행되어 생성 위젯만 반환합니다."))
	TArray<TObjectPtr<UVcardDescriptorBase>> FactoryDescriptors;
};

/**
 * Root descriptor for bootstrapping the V-card root widget.
 *
 * Responsibility:
 * - Hold optional root-level attachments such as initial stage/modal layers.
 *
 * Must not:
 * - Create the root widget; AVcardUiRegistryActor owns that bootstrap step.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardRootDescriptor : public UVcardWidgetCompositionDescriptor
{
	GENERATED_BODY()

public:
	UVcardRootDescriptor();
};

/**
 * Preview lobby descriptor for composing PreviewLobby.Top/Carousel/Bottom.
 *
 * Responsibility:
 * - Group descriptor fragments used by the preview lobby shell.
 *
 * Must not:
 * - Create or own the root Stage itself.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardPreviewLobbyDescriptor : public UVcardCompositeDescriptor
{
	GENERATED_BODY()

public:
	UVcardPreviewLobbyDescriptor();
};

/**
 * Creator lobby descriptor for composing CreatorLobby.Top/Tools/ToolContents.
 *
 * Responsibility:
 * - Group descriptor fragments used by the creator lobby shell.
 *
 * Must not:
 * - Own the selected tool state.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardCreatorLobbyDescriptor : public UVcardCompositeDescriptor
{
	GENERATED_BODY()

public:
	UVcardCreatorLobbyDescriptor();
};

/**
 * Tool option content descriptor for composing toolOptNameArray/toolOptContentMain.
 *
 * Responsibility:
 * - Group descriptor fragments used by option-driven tool content panels.
 *
 * Must not:
 * - Force tools that do not need option names to use this shell.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardToolOptContentDescriptor : public UVcardCompositeDescriptor
{
	GENERATED_BODY()

public:
	UVcardToolOptContentDescriptor();
};

/**
 * Stage-lobby descriptor for lobby content placed into a root/stage slot.
 *
 * Responsibility:
 * - Compose lobby widgets into the host slot that invoked it.
 *
 * Must not:
 * - Know concrete background/motion domain item structures.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardStageLobbyDescriptor : public UVcardWidgetCompositionDescriptor
{
	GENERATED_BODY()

public:
	UVcardStageLobbyDescriptor();
};

/**
 * Lobby descriptor for composing the V-card lobby stage.
 *
 * Responsibility:
 * - Describe which widgets enter the lobby's named slots.
 *
 * Must not:
 * - Own lobby runtime state such as selected card or preview playback.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardLobbyDescriptor : public UVcardWidgetCompositionDescriptor
{
	GENERATED_BODY()

public:
	UVcardLobbyDescriptor();
};

/**
 * Stage descriptor for a common V-card stage layout.
 *
 * Responsibility:
 * - Compose widgets into a stage host's named slots.
 *
 * Must not:
 * - Own concrete domain choices such as background or motion item data.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardStageDescriptor : public UVcardWidgetCompositionDescriptor
{
	GENERATED_BODY()

public:
	UVcardStageDescriptor();
};
