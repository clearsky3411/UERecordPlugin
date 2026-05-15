#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "VdjmVcardDescriptorReceiver.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardWidgetBase.generated.h"

class UVcardDescriptorRegistryDataAsset;
class UVcardDescriptorBase;
class AVcardUiRegistryActor;

/**
 * Minimal base for V-card UMG widgets.
 *
 * Responsibility:
 * - Keep runtime context passed by descriptors.
 * - Expose Blueprint hooks when descriptor payload is applied.
 *
 * Must not:
 * - Force a concrete design system.
 * - Own screen-level undo/redo history.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardWidgetBase : public UUserWidget, public IVcardDescriptorReceiver
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Widget")
	void ApplyVcardDescriptorContext(UVcardDescriptorRegistryDataAsset* descriptorRegistry, UObject* contextObject);
	virtual void ApplyVcardWidgetAttachment_Implementation(const FVcardWidgetAttachDescriptor& attachmentDescriptor, UObject* payloadData) override;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Widget")
	bool EnsureDescriptorRegistry(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Widget")
	UVcardDescriptorRegistryDataAsset* LoadDefaultDescriptorRegistry();
	UFUNCTION(BlueprintCallable, Category = "Vcard|Widget")
	bool ApplyDescriptorById(FName descriptorId, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Widget")
	bool ApplyDescriptorToNamedSlot(FName slotName, FName descriptorId, FVcardDescriptorApplyResult& outResult);

	UFUNCTION(BlueprintPure, Category = "Vcard|Widget")
	UVcardDescriptorRegistryDataAsset* GetDescriptorRegistry() const { return mDescriptorRegistry.Get(); }
	UFUNCTION(BlueprintPure, Category = "Vcard|Widget")
	UObject* GetVcardContextObject() const { return mContextObject.Get(); }
	UFUNCTION(BlueprintPure, Category = "Vcard|Widget")
	FVcardWidgetAttachDescriptor GetLastAttachmentDescriptor() const { return mLastAttachmentDescriptor; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Widget")
	UObject* GetLastPayloadData() const { return mLastPayloadData.Get(); }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Widget")
	void BP_OnVcardDescriptorContextApplied(UVcardDescriptorRegistryDataAsset* descriptorRegistry, UObject* contextObject);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Widget")
	void BP_OnVcardAttachmentApplied(const FVcardWidgetAttachDescriptor& attachmentDescriptor, UObject* payloadData);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Widget")
	TSoftObjectPtr<UVcardDescriptorRegistryDataAsset> DefaultDescriptorRegistryAsset;

private:
	AVcardUiRegistryActor* FindWorldRegistryActor() const;
	bool ApplyDescriptorInternal(FName invocationSlotName, FName descriptorId, FVcardDescriptorApplyResult& outResult);

private:
	UPROPERTY(Transient)
	TObjectPtr<UVcardDescriptorRegistryDataAsset> mDescriptorRegistry;

	UPROPERTY(Transient)
	TObjectPtr<UObject> mContextObject;

	UPROPERTY(Transient)
	FVcardWidgetAttachDescriptor mLastAttachmentDescriptor;

	UPROPERTY(Transient)
	TObjectPtr<UObject> mLastPayloadData;
};
