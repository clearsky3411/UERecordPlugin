#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardDescriptorSlotCacheStore.generated.h"

class UNamedSlot;

USTRUCT()
struct FVcardDescriptorCachedWidgetEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	FName CacheEntryKey = NAME_None;

	UPROPERTY(Transient)
	FName DescriptorKey = NAME_None;

	UPROPERTY(Transient)
	FName TargetSlotName = NAME_None;

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CachedWidget;

	UPROPERTY(Transient)
	TObjectPtr<UObject> PayloadData;

	UPROPERTY(Transient)
	FSoftObjectPath PayloadPath;
};

USTRUCT()
struct FVcardDescriptorSlotCacheRecord
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TWeakObjectPtr<UUserWidget> OwnerWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<UUserWidget> LastHostWidget;

	UPROPERTY(Transient)
	FName CacheSlotKey = NAME_None;

	UPROPERTY(Transient)
	FName LastTargetSlotName = NAME_None;

	UPROPERTY(Transient)
	FName ActiveCacheEntryKey = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ActiveWidget;

	UPROPERTY(Transient)
	TMap<FName, FVcardDescriptorCachedWidgetEntry> CachedWidgets;
};

/**
 * Runtime cache for descriptor slot swaps.
 *
 * Responsibility:
 * - Keep inactive UUserWidget instances alive while their cache owner widget is alive.
 * - Reattach cached widgets to the requested NamedSlot when ECacheSwap is applied.
 * - Notify optional cache lifecycle interfaces when widgets are deactivated/activated.
 *
 * Must not:
 * - Store static descriptor definitions. Data assets own those.
 * - Keep caches after the owner widget has been destroyed.
 * - Add hidden wrapper widgets under the designer's NamedSlot.
 */
UCLASS()
class VDJMVCARD_API UVcardDescriptorSlotCacheStore : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UVcardDescriptorSlotCacheStore* Get(const UObject* worldContextObject);
	static FName ResolveCacheSlotKey(const FVcardWidgetAttachDescriptor& attachmentDescriptor);
	static FName ResolveCacheEntryKey(const FVcardDescriptorApplyRequest& request, const FVcardWidgetAttachDescriptor& attachmentDescriptor);

	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor|Cache")
	void ClearOwnerCache(UUserWidget* cacheOwnerWidget);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Descriptor|Cache")
	bool HasCachedWidget(UUserWidget* cacheOwnerWidget, FName cacheSlotKey, FName cacheEntryKey) const;

	bool FindCachedWidget(UUserWidget* cacheOwnerWidget, FName cacheSlotKey, FName cacheEntryKey, UUserWidget*& outCachedWidget) const;
	bool ApplyCacheSwap(
		const FVcardDescriptorApplyRequest& request,
		const FVcardWidgetAttachDescriptor& attachmentDescriptor,
		UUserWidget* contentWidget,
		UUserWidget*& outAppliedWidget,
		FString& outErrorReason);

private:
	void PurgeInvalidRecords();
	FVcardDescriptorSlotCacheRecord* FindOrAddRecord(UUserWidget* cacheOwnerWidget, FName cacheSlotKey);
	const FVcardDescriptorSlotCacheRecord* FindRecord(UUserWidget* cacheOwnerWidget, FName cacheSlotKey) const;
	void StoreWidgetEntry(
		FVcardDescriptorSlotCacheRecord& record,
		FName cacheEntryKey,
		UUserWidget* widget,
		const FVcardDescriptorApplyRequest& request,
		const FVcardWidgetAttachDescriptor& attachmentDescriptor);
	void NotifyActivated(UUserWidget* widget, UUserWidget* hostWidget, FName targetSlotName, FName cacheEntryKey) const;
	void NotifyDeactivated(UUserWidget* widget, UUserWidget* hostWidget, FName targetSlotName, FName cacheEntryKey, bool bReleaseResources) const;

	UPROPERTY(Transient)
	TArray<FVcardDescriptorSlotCacheRecord> mSlotRecords;
};
