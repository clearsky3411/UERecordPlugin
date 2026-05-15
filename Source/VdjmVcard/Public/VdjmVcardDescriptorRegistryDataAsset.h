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
 * - Keep editable V-card descriptors together.
 * - Provide map lookup for runtime calls and array fallback for editor-friendly authoring.
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

	UFUNCTION(BlueprintPure, Category = "Vcard|Registry")
	TArray<UVcardDescriptorBase*> GetDescriptorList() const;

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Vcard|Registry")
	TArray<TObjectPtr<UVcardDescriptorBase>> Descriptors;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Vcard|Registry")
	TMap<FName, TObjectPtr<UVcardDescriptorBase>> DescriptorMap;
};
