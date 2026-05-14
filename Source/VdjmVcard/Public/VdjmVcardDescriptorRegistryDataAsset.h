#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VdjmVcardDescriptorBase.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardDescriptorRegistryDataAsset.generated.h"

/**
 * Central V-card descriptor registry.
 *
 * Responsibility:
 * - Keep known screen, bottom sheet, tool, preset, text, and composition descriptors together.
 *
 * Must not:
 * - Replace project-level asset registry validation.
 * - Spawn widgets by itself; callers choose the host widget and call descriptors.
 */
UCLASS(BlueprintType)
class VDJMVCARD_API UVcardDescriptorRegistryDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool FindDescriptorById(FName descriptorId, UVcardDescriptorBase*& outDescriptor) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool FindScreenDescriptor(FName screenId, FVcardScreenDescriptor& outScreenDescriptor) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool FindBottomSheetDescriptor(FName descriptorId, FVcardBottomSheetDescriptor& outBottomSheetDescriptor) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool FindToolDescriptor(FName toolId, FVcardToolDescriptor& outToolDescriptor) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool FindBackgroundItem(FName itemId, FVcardBackgroundItemData& outBackgroundItem) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool FindMotionItem(FName itemId, FVcardMotionItemData& outMotionItem) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool FindTextField(FName fieldId, FVcardTextFieldDescriptor& outTextField) const;

	UFUNCTION(BlueprintPure, Category = "Vcard|Registry")
	TArray<UVcardDescriptorBase*> GetDescriptorList() const;

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Vcard|Registry")
	TArray<TObjectPtr<UVcardDescriptorBase>> Descriptors;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry")
	TArray<FVcardScreenDescriptor> ScreenDescriptors;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry")
	TArray<FVcardBottomSheetDescriptor> BottomSheetDescriptors;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry")
	TArray<FVcardToolDescriptor> ToolDescriptors;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry")
	TArray<FVcardBackgroundItemData> BackgroundItems;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry")
	TArray<FVcardMotionItemData> MotionItems;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry")
	TArray<FVcardTextFieldDescriptor> TextFields;
};
