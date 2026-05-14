#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardDescriptorApplier.generated.h"

class UNamedSlot;
class UPanelWidget;
class UWidget;

/**
 * Common named-slot/panel composition helper for V-card descriptors.
 *
 * Responsibility:
 * - Find widgets inside a host widget by name.
 * - Create child widgets and attach them to NamedSlot or PanelWidget targets.
 *
 * Must not:
 * - Decide which screen should be opened.
 * - Own undo/redo history.
 * - Trigger flow signals by itself.
 */
UCLASS()
class VDJMVCARD_API UVcardDescriptorApplier : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	static bool FindWidgetByName(UUserWidget* hostWidget, FName widgetName, UWidget*& outWidget);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	static bool FindNamedSlot(UUserWidget* hostWidget, FName slotName, UNamedSlot*& outNamedSlot);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	static bool FindPanelWidget(UUserWidget* hostWidget, FName panelName, UPanelWidget*& outPanelWidget);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	static bool CreateUserWidgetForHost(UUserWidget* hostWidget, TSubclassOf<UUserWidget> widgetClass, UUserWidget*& outCreatedWidget, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	static bool AttachWidgetToNamedSlot(UUserWidget* hostWidget, FName slotName, UWidget* contentWidget, EVcardDescriptorOpenPolicy openPolicy, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	static bool AttachWidgetToPanel(UUserWidget* hostWidget, FName panelName, UWidget* contentWidget, EVcardDescriptorOpenPolicy openPolicy, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor")
	static bool ApplyWidgetAttachment(const FVcardDescriptorApplyRequest& request, const FVcardWidgetAttachDescriptor& attachmentDescriptor, UUserWidget*& outCreatedWidget, FString& outErrorReason);
};
