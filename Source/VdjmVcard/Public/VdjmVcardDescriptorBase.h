#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardDescriptorBase.generated.h"

/**
 * Base object for descriptor-driven V-card UI composition.
 *
 * Responsibility:
 * - Carry the intent for a UI placement/setup operation.
 * - Provide one entry point that can choose default, previous runtime, or saved state inputs.
 *
 * Must not:
 * - Contain final visual layout; widgets own their actual NamedSlot positions.
 * - Own long-lived screen state or undo/redo stacks.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardDescriptorBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	bool ApplyToWidget(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	virtual bool CanApplyToWidget(const FVcardDescriptorApplyRequest& request, FString& outReason) const;

	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	FName GetDescriptorId() const { return DescriptorId; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	FText GetDisplayName() const { return DisplayName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	EVcardDescriptorApplyTiming GetApplyTiming() const { return ApplyTiming; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Descriptor")
	EVcardDescriptorRestorePolicy GetRestorePolicy() const { return RestorePolicy; }

protected:
	virtual bool ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor")
	FName DescriptorId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor")
	EVcardDescriptorApplyTiming ApplyTiming = EVcardDescriptorApplyTiming::EImmediate;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor")
	EVcardDescriptorRestorePolicy RestorePolicy = EVcardDescriptorRestorePolicy::EPreferPreviousThenDefault;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor")
	FName DebugTag = NAME_None;
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

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor")
	TArray<FVcardWidgetAttachDescriptor> Attachments;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Descriptor")
	bool bStopOnFirstFailure = true;
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
