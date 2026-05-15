#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardUiRegistryActor.generated.h"

class UVcardDescriptorBase;
class UVcardDescriptorRegistryDataAsset;
class UVcardRootWidget;
class UVcardWidgetBase;

/**
 * World-level owner for V-card UI descriptor configuration.
 *
 * Responsibility:
 * - Own the descriptor registry DataAsset for the current V-card UI session.
 * - Create the root widget and invoke the root descriptor.
 *
 * Must not:
 * - Own concrete widget layout details.
 * - Replace the descriptor registry DataAsset as the editable source of UI composition rules.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API AVcardUiRegistryActor : public AInfo
{
	GENERATED_BODY()

public:
	AVcardUiRegistryActor();

	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool CreateRootWidget(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool ApplyRootDescriptor(FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool ApplyRegistryToWidget(UVcardWidgetBase* widget, UObject* contextObject);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Registry")
	bool FindDescriptorById(FName descriptorId, UVcardDescriptorBase*& outDescriptor) const;

	UFUNCTION(BlueprintPure, Category = "Vcard|Registry")
	UVcardDescriptorRegistryDataAsset* GetDescriptorRegistry() const { return DescriptorRegistry; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Registry")
	UVcardRootWidget* GetRootWidget() const { return mRootWidget.Get(); }
	UFUNCTION(BlueprintPure, Category = "Vcard|Registry")
	FName GetRootDescriptorId() const { return RootDescriptorId; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry")
	TObjectPtr<UVcardDescriptorRegistryDataAsset> DescriptorRegistry;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry|Root")
	TSubclassOf<UVcardRootWidget> RootWidgetClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry|Root")
	FName RootDescriptorId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry|Root")
	int32 PlayerIndex = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry|Root")
	int32 RootZOrder = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry|Root")
	bool bRequireOwningPlayer = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry|Root")
	bool bCreateRootOnBeginPlay = true;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Registry|Root")
	bool bApplyRootDescriptorOnCreate = true;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVcardRootWidget> mRootWidget;
};
