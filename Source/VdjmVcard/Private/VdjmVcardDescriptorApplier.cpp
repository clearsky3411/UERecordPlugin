#include "VdjmVcardDescriptorApplier.h"

#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"
#include "VdjmVcard.h"
#include "VdjmVcardDescriptorReceiver.h"

bool UVcardDescriptorApplier::FindWidgetByName(UUserWidget* hostWidget, FName widgetName, UWidget*& outWidget)
{
	outWidget = nullptr;

	if (!IsValid(hostWidget))
	{
		return false;
	}

	if (widgetName.IsNone())
	{
		return false;
	}

	if (!hostWidget->WidgetTree)
	{
		return false;
	}

	outWidget = hostWidget->WidgetTree->FindWidget(widgetName);
	return IsValid(outWidget);
}

bool UVcardDescriptorApplier::FindNamedSlot(UUserWidget* hostWidget, FName slotName, UNamedSlot*& outNamedSlot)
{
	outNamedSlot = nullptr;

	UWidget* foundWidget = nullptr;
	if (!FindWidgetByName(hostWidget, slotName, foundWidget))
	{
		return false;
	}

	outNamedSlot = Cast<UNamedSlot>(foundWidget);
	return IsValid(outNamedSlot);
}

bool UVcardDescriptorApplier::FindPanelWidget(UUserWidget* hostWidget, FName panelName, UPanelWidget*& outPanelWidget)
{
	outPanelWidget = nullptr;

	UWidget* foundWidget = nullptr;
	if (!FindWidgetByName(hostWidget, panelName, foundWidget))
	{
		return false;
	}

	outPanelWidget = Cast<UPanelWidget>(foundWidget);
	return IsValid(outPanelWidget);
}

bool UVcardDescriptorApplier::CreateUserWidgetForHost(UUserWidget* hostWidget, TSubclassOf<UUserWidget> widgetClass, UUserWidget*& outCreatedWidget, FString& outErrorReason)
{
	outCreatedWidget = nullptr;
	outErrorReason.Reset();

	if (!IsValid(hostWidget))
	{
		outErrorReason = TEXT("Host widget is invalid.");
		return false;
	}

	if (!*widgetClass)
	{
		outErrorReason = TEXT("Widget class is not assigned.");
		return false;
	}

	if (APlayerController* owningPlayer = hostWidget->GetOwningPlayer())
	{
		outCreatedWidget = CreateWidget<UUserWidget>(owningPlayer, widgetClass);
	}
	else if (UWorld* world = hostWidget->GetWorld())
	{
		outCreatedWidget = CreateWidget<UUserWidget>(world, widgetClass);
	}

	if (!IsValid(outCreatedWidget))
	{
		outErrorReason = FString::Printf(TEXT("Failed to create widget class '%s'."), *GetNameSafe(*widgetClass));
		return false;
	}

	return true;
}

bool UVcardDescriptorApplier::AttachWidgetToNamedSlot(UUserWidget* hostWidget, FName slotName, UWidget* contentWidget, EVcardDescriptorOpenPolicy openPolicy, FString& outErrorReason)
{
	outErrorReason.Reset();

	if (!IsValid(contentWidget))
	{
		outErrorReason = TEXT("Content widget is invalid.");
		return false;
	}

	UNamedSlot* namedSlot = nullptr;
	if (!FindNamedSlot(hostWidget, slotName, namedSlot))
	{
		outErrorReason = FString::Printf(TEXT("NamedSlot '%s' was not found on host '%s'."), *slotName.ToString(), *GetNameSafe(hostWidget));
		return false;
	}

	if (openPolicy == EVcardDescriptorOpenPolicy::EHide)
	{
		namedSlot->SetVisibility(ESlateVisibility::Collapsed);
		return true;
	}

	if (openPolicy == EVcardDescriptorOpenPolicy::EKeepIfSame)
	{
		if (UWidget* currentContent = namedSlot->GetContent())
		{
			if (currentContent->GetClass() == contentWidget->GetClass())
			{
				return true;
			}
		}
	}

	contentWidget->RemoveFromParent();
	namedSlot->SetContent(contentWidget);
	namedSlot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return true;
}

bool UVcardDescriptorApplier::AttachWidgetToPanel(UUserWidget* hostWidget, FName panelName, UWidget* contentWidget, EVcardDescriptorOpenPolicy openPolicy, FString& outErrorReason)
{
	outErrorReason.Reset();

	if (!IsValid(contentWidget))
	{
		outErrorReason = TEXT("Content widget is invalid.");
		return false;
	}

	UPanelWidget* panelWidget = nullptr;
	if (!FindPanelWidget(hostWidget, panelName, panelWidget))
	{
		outErrorReason = FString::Printf(TEXT("Panel '%s' was not found on host '%s'."), *panelName.ToString(), *GetNameSafe(hostWidget));
		return false;
	}

	if (openPolicy == EVcardDescriptorOpenPolicy::EHide)
	{
		panelWidget->SetVisibility(ESlateVisibility::Collapsed);
		return true;
	}

	if (openPolicy == EVcardDescriptorOpenPolicy::EReplace)
	{
		panelWidget->ClearChildren();
	}
	else if (openPolicy == EVcardDescriptorOpenPolicy::EKeepIfSame)
	{
		for (int32 childIndex = 0; childIndex < panelWidget->GetChildrenCount(); ++childIndex)
		{
			UWidget* childWidget = panelWidget->GetChildAt(childIndex);
			if (IsValid(childWidget) && childWidget->GetClass() == contentWidget->GetClass())
			{
				return true;
			}
		}
	}

	contentWidget->RemoveFromParent();
	UPanelSlot* panelSlot = panelWidget->AddChild(contentWidget);
	if (!panelSlot)
	{
		outErrorReason = FString::Printf(TEXT("Panel '%s' rejected content '%s'."), *panelName.ToString(), *GetNameSafe(contentWidget));
		return false;
	}

	panelWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return true;
}

bool UVcardDescriptorApplier::ApplyWidgetAttachment(const FVcardDescriptorApplyRequest& request, const FVcardWidgetAttachDescriptor& attachmentDescriptor, UUserWidget*& outCreatedWidget, FString& outErrorReason)
{
	outCreatedWidget = nullptr;
	outErrorReason.Reset();

	if (!IsValid(request.HostWidget))
	{
		outErrorReason = TEXT("Apply request has no host widget.");
		return false;
	}

	if (attachmentDescriptor.TargetSlotName.IsNone())
	{
		outErrorReason = TEXT("Attachment has no target slot name.");
		return false;
	}

	if (!request.bAllowCreate)
	{
		outErrorReason = TEXT("Apply request does not allow widget creation.");
		return false;
	}

	if (!CreateUserWidgetForHost(request.HostWidget, attachmentDescriptor.WidgetClass, outCreatedWidget, outErrorReason))
	{
		return false;
	}

	bool bAttached = AttachWidgetToNamedSlot(request.HostWidget, attachmentDescriptor.TargetSlotName, outCreatedWidget, attachmentDescriptor.OpenPolicy, outErrorReason);
	if (!bAttached)
	{
		FString panelErrorReason;
		bAttached = AttachWidgetToPanel(request.HostWidget, attachmentDescriptor.TargetSlotName, outCreatedWidget, attachmentDescriptor.OpenPolicy, panelErrorReason);
		if (!bAttached)
		{
			outErrorReason = FString::Printf(TEXT("%s / %s"), *outErrorReason, *panelErrorReason);
			return false;
		}
	}

	if (attachmentDescriptor.bAutoApplyPayload && outCreatedWidget->GetClass()->ImplementsInterface(UVcardDescriptorReceiver::StaticClass()))
	{
		IVcardDescriptorReceiver::Execute_ApplyVcardWidgetAttachment(outCreatedWidget, attachmentDescriptor, attachmentDescriptor.PayloadData);
	}

	UE_LOG(LogVdjmVcard, Verbose, TEXT("Vcard attachment applied Host=%s Target=%s Widget=%s"),
		*GetNameSafe(request.HostWidget),
		*attachmentDescriptor.TargetSlotName.ToString(),
		*GetNameSafe(outCreatedWidget));

	return true;
}
