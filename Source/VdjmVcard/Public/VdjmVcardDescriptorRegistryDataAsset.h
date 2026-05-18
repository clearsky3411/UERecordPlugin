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
 * - Keep editable V-card descriptors behind explicit descriptor keys.
 * - Provide one lookup path for widgets that ask a descriptor to fill their named slots.
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
	bool FindDescriptorByKey(FName descriptorKey, UVcardDescriptorBase*& outDescriptor) const;

	UFUNCTION(BlueprintPure, Category = "Vcard|Registry")
	TArray<UVcardDescriptorBase*> GetDescriptorList() const;

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Vcard|Registry", meta = (ToolTip = "Key가 descriptor lookup 기준입니다. Widget은 이 key를 helper에 넘겨 자신의 NamedSlot/PanelWidget을 채웁니다."))
	TMap<FName, TObjectPtr<UVcardDescriptorBase>> DescriptorByKey;
};
