#include "VdjmVcardDescriptorApplier.h"

#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"
#include "VdjmVcard.h"
#include "VdjmVcardDescriptorBase.h"
#include "VdjmVcardDescriptorReceiver.h"
#include "VdjmVcardDescriptorRegistryDataAsset.h"
#include "VdjmVcardDescriptorSlotCacheStore.h"

bool UVcardDescriptorApplier::GenerateWidgetsIntoNamedSlotsFromVcardDescriptorDataAsset(
	UUserWidget* namedSlotHostWidget,
	UVcardDescriptorRegistryDataAsset* descriptorRegistryDataAsset,
	FName descriptorKey,
	UObject* payloadData,
	TArray<UUserWidget*>& outCreatedWidgets,
	FString& outErrorReason)
{
	outCreatedWidgets.Reset();
	outErrorReason.Reset();

	if (!IsValid(namedSlotHostWidget))
	{
		outErrorReason = TEXT("Named slot host widget is invalid.");
		return false;
	}

	if (!IsValid(descriptorRegistryDataAsset))
	{
		outErrorReason = TEXT("Descriptor registry data asset is invalid.");
		return false;
	}

	if (descriptorKey.IsNone())
	{
		outErrorReason = TEXT("Descriptor key is None.");
		return false;
	}

	UVcardDescriptorBase* descriptor = nullptr;
	if (!descriptorRegistryDataAsset->FindDescriptorByKey(descriptorKey, descriptor) || !IsValid(descriptor))
	{
		outErrorReason = FString::Printf(TEXT("Descriptor key '%s' was not found."), *descriptorKey.ToString());
		return false;
	}

	FVcardDescriptorApplyRequest request;
	request.NamedSlotHostWidget = namedSlotHostWidget;
	request.CacheOwnerWidget = namedSlotHostWidget;
	request.DescriptorKey = descriptorKey;
	request.PayloadData = payloadData;
	request.bAllowCreate = true;

	FVcardDescriptorApplyResult result;
	const bool bGenerated = descriptor->ApplyToWidget(request, result);
	for (UUserWidget* createdWidget : result.CreatedWidgets)
	{
		if (IsValid(createdWidget))
		{
			outCreatedWidgets.Add(createdWidget);
		}
	}

	outErrorReason = result.ErrorReason;
	if (!bGenerated && outErrorReason.IsEmpty())
	{
		outErrorReason = FString::Printf(TEXT("Descriptor key '%s' failed without error reason."), *descriptorKey.ToString());
	}

	return bGenerated;
}

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

	if (openPolicy == EVcardDescriptorOpenPolicy::ECacheSwap)
	{
		outErrorReason = TEXT("ECacheSwap must be applied through ApplyWidgetAttachment.");
		return false;
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

	if (openPolicy == EVcardDescriptorOpenPolicy::ECacheSwap)
	{
		outErrorReason = TEXT("ECacheSwap supports NamedSlot targets only.");
		return false;
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

	if (!IsValid(request.NamedSlotHostWidget))
	{
		outErrorReason = TEXT("Apply request has no named slot host widget.");
		return false;
	}

	FVcardWidgetAttachDescriptor normalizedAttachment = attachmentDescriptor;
	if (normalizedAttachment.TargetSlotName.IsNone())
	{
		normalizedAttachment.TargetSlotName = request.FallbackTargetSlotName;
	}

	if (normalizedAttachment.TargetSlotName.IsNone())
	{
		outErrorReason = TEXT("Attachment has no target slot name.");
		return false;
	}

	if (!request.bAllowCreate)
	{
		outErrorReason = TEXT("Apply request does not allow widget creation.");
		return false;
	}

	if (normalizedAttachment.OpenPolicy == EVcardDescriptorOpenPolicy::ECacheSwap)
	{
		UVcardDescriptorSlotCacheStore* cacheStore = UVcardDescriptorSlotCacheStore::Get(request.NamedSlotHostWidget);
		if (cacheStore == nullptr)
		{
			outErrorReason = TEXT("Cache swap store is unavailable.");
			return false;
		}

		UUserWidget* cacheOwnerWidget = IsValid(request.CacheOwnerWidget)
			? request.CacheOwnerWidget.Get()
			: request.NamedSlotHostWidget.Get();
		const FName cacheSlotKey = UVcardDescriptorSlotCacheStore::ResolveCacheSlotKey(normalizedAttachment);
		const FName cacheEntryKey = UVcardDescriptorSlotCacheStore::ResolveCacheEntryKey(request, normalizedAttachment);
		UUserWidget* cachedWidget = nullptr;
		if (cacheStore->FindCachedWidget(cacheOwnerWidget, cacheSlotKey, cacheEntryKey, cachedWidget) && IsValid(cachedWidget))
		{
			outCreatedWidget = cachedWidget;
		}
		else if (!CreateUserWidgetForHost(request.NamedSlotHostWidget, normalizedAttachment.WidgetClass, outCreatedWidget, outErrorReason))
		{
			return false;
		}

		if (!cacheStore->ApplyCacheSwap(request, normalizedAttachment, outCreatedWidget, outCreatedWidget, outErrorReason))
		{
			return false;
		}

		if (normalizedAttachment.bAutoApplyPayload && outCreatedWidget->GetClass()->ImplementsInterface(UVcardDescriptorReceiver::StaticClass()))
		{
			UObject* payloadData = IsValid(normalizedAttachment.PayloadData) ? normalizedAttachment.PayloadData.Get() : request.PayloadData.Get();
			IVcardDescriptorReceiver::Execute_ApplyVcardWidgetAttachment(outCreatedWidget, normalizedAttachment, payloadData);
		}

		UE_LOG(LogVdjmVcard, Verbose, TEXT("Vcard cache attachment applied Host=%s Target=%s Widget=%s"),
			*GetNameSafe(request.NamedSlotHostWidget),
			*normalizedAttachment.TargetSlotName.ToString(),
			*GetNameSafe(outCreatedWidget));

		return true;
	}

	if (!CreateUserWidgetForHost(request.NamedSlotHostWidget, normalizedAttachment.WidgetClass, outCreatedWidget, outErrorReason))
	{
		return false;
	}

	bool bAttached = AttachWidgetToNamedSlot(request.NamedSlotHostWidget, normalizedAttachment.TargetSlotName, outCreatedWidget, normalizedAttachment.OpenPolicy, outErrorReason);
	if (!bAttached)
	{
		FString panelErrorReason;
		bAttached = AttachWidgetToPanel(request.NamedSlotHostWidget, normalizedAttachment.TargetSlotName, outCreatedWidget, normalizedAttachment.OpenPolicy, panelErrorReason);
		if (!bAttached)
		{
			outErrorReason = FString::Printf(TEXT("%s / %s"), *outErrorReason, *panelErrorReason);
			return false;
		}
	}

	if (normalizedAttachment.bAutoApplyPayload && outCreatedWidget->GetClass()->ImplementsInterface(UVcardDescriptorReceiver::StaticClass()))
	{
		UObject* payloadData = IsValid(normalizedAttachment.PayloadData) ? normalizedAttachment.PayloadData.Get() : request.PayloadData.Get();
		IVcardDescriptorReceiver::Execute_ApplyVcardWidgetAttachment(outCreatedWidget, normalizedAttachment, payloadData);
	}

	UE_LOG(LogVdjmVcard, Verbose, TEXT("Vcard attachment applied Host=%s Target=%s Widget=%s"),
		*GetNameSafe(request.NamedSlotHostWidget),
		*normalizedAttachment.TargetSlotName.ToString(),
		*GetNameSafe(outCreatedWidget));

	return true;
}
