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
