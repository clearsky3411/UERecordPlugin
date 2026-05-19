#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VdjmVcardCacheSwapLifecycle.generated.h"

UINTERFACE(BlueprintType)
class VDJMVCARD_API UVcardCacheSwapLifecycle : public UInterface
{
	GENERATED_BODY()
};

class VDJMVCARD_API IVcardCacheSwapLifecycle
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vcard|CacheSwap")
	void OnCacheSwapActivated(UUserWidget* targetHostWidget, FName targetSlotName, FName cacheEntryKey);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vcard|CacheSwap")
	void OnCacheSwapDeactivated(UUserWidget* previousHostWidget, FName targetSlotName, FName cacheEntryKey, bool bReleaseResources);
};
