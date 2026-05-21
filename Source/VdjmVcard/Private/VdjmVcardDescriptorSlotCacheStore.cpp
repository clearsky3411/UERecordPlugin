#include "VdjmVcardDescriptorSlotCacheStore.h"

#include "Blueprint/UserWidget.h"
#include "Components/NamedSlot.h"
#include "VdjmVcard.h"
#include "VdjmVcardCacheSwapLifecycle.h"
#include "VdjmVcardDescriptorApplier.h"

UVcardDescriptorSlotCacheStore* UVcardDescriptorSlotCacheStore::Get(const UObject* worldContextObject)
{
	if (worldContextObject == nullptr)
	{
		return nullptr;
	}

	const UWorld* world = worldContextObject->GetWorld();
	if (world == nullptr)
	{
		return nullptr;
	}

	UGameInstance* gameInstance = world->GetGameInstance();
	if (gameInstance == nullptr)
	{
		return nullptr;
	}

	return gameInstance->GetSubsystem<UVcardDescriptorSlotCacheStore>();
}

FName UVcardDescriptorSlotCacheStore::ResolveCacheSlotKey(const FVcardWidgetAttachDescriptor& attachmentDescriptor)
{
	return !attachmentDescriptor.CacheSlotKey.IsNone()
		? attachmentDescriptor.CacheSlotKey
		: attachmentDescriptor.TargetSlotName;
}

FName UVcardDescriptorSlotCacheStore::ResolveCacheEntryKey(
	const FVcardDescriptorApplyRequest& request,
	const FVcardWidgetAttachDescriptor& attachmentDescriptor)
{
	if (!attachmentDescriptor.CacheEntryKey.IsNone())
	{
		return attachmentDescriptor.CacheEntryKey;
	}

	if (!request.DescriptorKey.IsNone())
	{
		return request.DescriptorKey;
	}

	if (!attachmentDescriptor.DebugName.IsNone())
	{
		return attachmentDescriptor.DebugName;
	}

	if (*attachmentDescriptor.WidgetClass)
	{
		return attachmentDescriptor.WidgetClass->GetFName();
	}

	return NAME_None;
}

void UVcardDescriptorSlotCacheStore::ClearOwnerCache(UUserWidget* cacheOwnerWidget)
{
	if (!IsValid(cacheOwnerWidget))
	{
		return;
	}

	for (int32 recordIndex = mSlotRecords.Num() - 1; recordIndex >= 0; --recordIndex)
	{
		if (mSlotRecords[recordIndex].OwnerWidget.Get() == cacheOwnerWidget)
		{
			mSlotRecords.RemoveAtSwap(recordIndex);
		}
	}
}

bool UVcardDescriptorSlotCacheStore::HasCachedWidget(UUserWidget* cacheOwnerWidget, FName cacheSlotKey, FName cacheEntryKey) const
{
	UUserWidget* cachedWidget = nullptr;
	return FindCachedWidget(cacheOwnerWidget, cacheSlotKey, cacheEntryKey, cachedWidget);
}

bool UVcardDescriptorSlotCacheStore::FindCachedWidget(
	UUserWidget* cacheOwnerWidget,
	FName cacheSlotKey,
	FName cacheEntryKey,
	UUserWidget*& outCachedWidget) const
{
	outCachedWidget = nullptr;

	if (!IsValid(cacheOwnerWidget) || cacheSlotKey.IsNone() || cacheEntryKey.IsNone())
	{
		return false;
	}

	const FVcardDescriptorSlotCacheRecord* record = FindRecord(cacheOwnerWidget, cacheSlotKey);
	if (record == nullptr)
	{
		return false;
	}

	const FVcardDescriptorCachedWidgetEntry* entry = record->CachedWidgets.Find(cacheEntryKey);
	if (entry == nullptr || !IsValid(entry->CachedWidget))
	{
		return false;
	}

	outCachedWidget = entry->CachedWidget;
	return true;
}

bool UVcardDescriptorSlotCacheStore::ApplyCacheSwap(
	const FVcardDescriptorApplyRequest& request,
	const FVcardWidgetAttachDescriptor& attachmentDescriptor,
	UUserWidget* contentWidget,
	UUserWidget*& outAppliedWidget,
	FString& outErrorReason)
{
	outAppliedWidget = nullptr;
	outErrorReason.Reset();
	PurgeInvalidRecords();

	if (!IsValid(request.NamedSlotHostWidget))
	{
		outErrorReason = TEXT("Cache swap request has no named slot host widget.");
		return false;
	}

	if (!IsValid(contentWidget))
	{
		outErrorReason = TEXT("Cache swap content widget is invalid.");
		return false;
	}

	if (attachmentDescriptor.TargetSlotName.IsNone())
	{
		outErrorReason = TEXT("Cache swap target slot name is None.");
		return false;
	}

	UUserWidget* cacheOwnerWidget = IsValid(request.CacheOwnerWidget)
		? request.CacheOwnerWidget.Get()
		: request.NamedSlotHostWidget.Get();
	if (!IsValid(cacheOwnerWidget))
	{
		outErrorReason = TEXT("Cache swap owner widget is invalid.");
		return false;
	}

	const FName cacheSlotKey = ResolveCacheSlotKey(attachmentDescriptor);
	const FName cacheEntryKey = ResolveCacheEntryKey(request, attachmentDescriptor);
	if (cacheSlotKey.IsNone() || cacheEntryKey.IsNone())
	{
		outErrorReason = FString::Printf(
			TEXT("Cache swap key is invalid. CacheSlotKey=%s CacheEntryKey=%s"),
			*cacheSlotKey.ToString(),
			*cacheEntryKey.ToString());
		return false;
	}

	UNamedSlot* namedSlot = nullptr;
	if (!UVcardDescriptorApplier::FindNamedSlot(request.NamedSlotHostWidget, attachmentDescriptor.TargetSlotName, namedSlot) || !IsValid(namedSlot))
	{
		outErrorReason = FString::Printf(
			TEXT("Cache swap requires NamedSlot '%s' on host '%s'."),
			*attachmentDescriptor.TargetSlotName.ToString(),
			*GetNameSafe(request.NamedSlotHostWidget));
		return false;
	}

	FVcardDescriptorSlotCacheRecord* record = FindOrAddRecord(cacheOwnerWidget, cacheSlotKey);
	if (record == nullptr)
	{
		outErrorReason = TEXT("Failed to create cache swap record.");
		return false;
	}

	UWidget* currentContent = namedSlot->GetContent();
	UUserWidget* currentUserWidget = Cast<UUserWidget>(currentContent);
	UUserWidget* previousActiveWidget = IsValid(record->ActiveWidget) ? record->ActiveWidget.Get() : currentUserWidget;
	const bool bAlreadyActive =
		currentContent == contentWidget &&
		previousActiveWidget == contentWidget &&
		record->ActiveCacheEntryKey == cacheEntryKey;

	if (bAlreadyActive)
	{
		record->LastHostWidget = request.NamedSlotHostWidget;
		record->LastTargetSlotName = attachmentDescriptor.TargetSlotName;
		StoreWidgetEntry(*record, cacheEntryKey, contentWidget, request, attachmentDescriptor);
		namedSlot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		outAppliedWidget = contentWidget;
		UE_LOG(
			LogVdjmVcard,
			Log,
			TEXT("Vcard cache swap already active Owner=%s Host=%s Slot=%s CacheSlot=%s Entry=%s Widget=%s"),
			*GetNameSafe(cacheOwnerWidget),
			*GetNameSafe(request.NamedSlotHostWidget),
			*attachmentDescriptor.TargetSlotName.ToString(),
			*cacheSlotKey.ToString(),
			*cacheEntryKey.ToString(),
			*GetNameSafe(contentWidget));
		return true;
	}

	const bool bSwitchingWidget = previousActiveWidget != nullptr && previousActiveWidget != contentWidget;

	if (bSwitchingWidget)
	{
		NotifyDeactivated(
			previousActiveWidget,
			request.NamedSlotHostWidget,
			record->LastTargetSlotName.IsNone() ? attachmentDescriptor.TargetSlotName : record->LastTargetSlotName,
			record->ActiveCacheEntryKey,
			attachmentDescriptor.bReleaseResourcesOnCacheDeactivate);

		if (!record->ActiveCacheEntryKey.IsNone())
		{
			FVcardDescriptorCachedWidgetEntry& previousEntry = record->CachedWidgets.FindOrAdd(record->ActiveCacheEntryKey);
			previousEntry.CacheEntryKey = record->ActiveCacheEntryKey;
			previousEntry.CachedWidget = previousActiveWidget;
		}

		previousActiveWidget->RemoveFromParent();
	}
	else if (currentContent != nullptr && currentContent != contentWidget)
	{
		currentContent->RemoveFromParent();
	}

	contentWidget->RemoveFromParent();
	namedSlot->SetContent(contentWidget);
	namedSlot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	record->LastHostWidget = request.NamedSlotHostWidget;
	record->LastTargetSlotName = attachmentDescriptor.TargetSlotName;
	record->ActiveCacheEntryKey = cacheEntryKey;
	record->ActiveWidget = contentWidget;
	StoreWidgetEntry(*record, cacheEntryKey, contentWidget, request, attachmentDescriptor);

	NotifyActivated(contentWidget, request.NamedSlotHostWidget, attachmentDescriptor.TargetSlotName, cacheEntryKey);

	outAppliedWidget = contentWidget;
	UE_LOG(
		LogVdjmVcard,
		Verbose,
		TEXT("Vcard cache swap applied Owner=%s Host=%s Slot=%s CacheSlot=%s Entry=%s Widget=%s"),
		*GetNameSafe(cacheOwnerWidget),
		*GetNameSafe(request.NamedSlotHostWidget),
		*attachmentDescriptor.TargetSlotName.ToString(),
		*cacheSlotKey.ToString(),
		*cacheEntryKey.ToString(),
		*GetNameSafe(contentWidget));
	return true;
}

void UVcardDescriptorSlotCacheStore::PurgeInvalidRecords()
{
	for (int32 recordIndex = mSlotRecords.Num() - 1; recordIndex >= 0; --recordIndex)
	{
		if (!mSlotRecords[recordIndex].OwnerWidget.IsValid())
		{
			mSlotRecords.RemoveAtSwap(recordIndex);
		}
	}
}

FVcardDescriptorSlotCacheRecord* UVcardDescriptorSlotCacheStore::FindOrAddRecord(UUserWidget* cacheOwnerWidget, FName cacheSlotKey)
{
	if (!IsValid(cacheOwnerWidget) || cacheSlotKey.IsNone())
	{
		return nullptr;
	}

	for (FVcardDescriptorSlotCacheRecord& record : mSlotRecords)
	{
		if (record.OwnerWidget.Get() == cacheOwnerWidget && record.CacheSlotKey == cacheSlotKey)
		{
			return &record;
		}
	}

	FVcardDescriptorSlotCacheRecord& newRecord = mSlotRecords.AddDefaulted_GetRef();
	newRecord.OwnerWidget = cacheOwnerWidget;
	newRecord.CacheSlotKey = cacheSlotKey;
	return &newRecord;
}

const FVcardDescriptorSlotCacheRecord* UVcardDescriptorSlotCacheStore::FindRecord(UUserWidget* cacheOwnerWidget, FName cacheSlotKey) const
{
	if (!IsValid(cacheOwnerWidget) || cacheSlotKey.IsNone())
	{
		return nullptr;
	}

	for (const FVcardDescriptorSlotCacheRecord& record : mSlotRecords)
	{
		if (record.OwnerWidget.Get() == cacheOwnerWidget && record.CacheSlotKey == cacheSlotKey)
		{
			return &record;
		}
	}

	return nullptr;
}

void UVcardDescriptorSlotCacheStore::StoreWidgetEntry(
	FVcardDescriptorSlotCacheRecord& record,
	FName cacheEntryKey,
	UUserWidget* widget,
	const FVcardDescriptorApplyRequest& request,
	const FVcardWidgetAttachDescriptor& attachmentDescriptor)
{
	if (cacheEntryKey.IsNone() || !IsValid(widget))
	{
		return;
	}

	FVcardDescriptorCachedWidgetEntry& entry = record.CachedWidgets.FindOrAdd(cacheEntryKey);
	entry.CacheEntryKey = cacheEntryKey;
	entry.DescriptorKey = request.DescriptorKey;
	entry.TargetSlotName = attachmentDescriptor.TargetSlotName;
	entry.WidgetClass = attachmentDescriptor.WidgetClass;
	entry.CachedWidget = widget;
	entry.PayloadData = IsValid(attachmentDescriptor.PayloadData) ? attachmentDescriptor.PayloadData.Get() : request.PayloadData.Get();
	entry.PayloadPath = attachmentDescriptor.PayloadPath;
}

void UVcardDescriptorSlotCacheStore::NotifyActivated(UUserWidget* widget, UUserWidget* hostWidget, FName targetSlotName, FName cacheEntryKey) const
{
	if (IsValid(widget) && widget->GetClass()->ImplementsInterface(UVcardCacheSwapLifecycle::StaticClass()))
	{
		IVcardCacheSwapLifecycle::Execute_OnCacheSwapActivated(widget, hostWidget, targetSlotName, cacheEntryKey);
	}
}

void UVcardDescriptorSlotCacheStore::NotifyDeactivated(
	UUserWidget* widget,
	UUserWidget* hostWidget,
	FName targetSlotName,
	FName cacheEntryKey,
	bool bReleaseResources) const
{
	if (IsValid(widget) && widget->GetClass()->ImplementsInterface(UVcardCacheSwapLifecycle::StaticClass()))
	{
		IVcardCacheSwapLifecycle::Execute_OnCacheSwapDeactivated(widget, hostWidget, targetSlotName, cacheEntryKey, bReleaseResources);
	}
}
