#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VdjmVcardTileViewWidgets.h"
#include "VdjmVcardPresetCatalogDataAsset.generated.h"

class UMaterialInterface;
class UTexture2D;

UENUM(BlueprintType)
enum class EVcardPresetAssetKind : uint8
{
	ENone UMETA(DisplayName = "None"),
	ETexture UMETA(DisplayName = "Texture"),
	EImage UMETA(DisplayName = "Image"),
	EMaterial UMETA(DisplayName = "Material"),
	ESound UMETA(DisplayName = "Sound"),
	EAnimation UMETA(DisplayName = "Animation"),
	EMotion UMETA(DisplayName = "Motion"),
	EWidget UMETA(DisplayName = "Widget"),
	EDataAsset UMETA(DisplayName = "Data Asset"),
	EExternalFile UMETA(DisplayName = "External File"),
	ECustom UMETA(DisplayName = "Custom")
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardPresetAssetRef
{
	GENERATED_BODY()

public:
	FSoftObjectPath GetResolvedAssetPath() const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	FName SlotKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	EVcardPresetAssetKind AssetKind = EVcardPresetAssetKind::ENone;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	TSoftObjectPtr<UObject> AssetObject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	FSoftObjectPath AssetPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	FString ExternalPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	bool bRequired = false;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardPresetItemData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	FName GroupKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	EVcardPresetAssetKind PrimaryKind = EVcardPresetAssetKind::ENone;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	FName PrimaryAssetSlotKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset|Visual")
	TSoftObjectPtr<UTexture2D> ThumbnailTexture;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	TArray<FVcardPresetAssetRef> Assets;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset|Action")
	FName ActionDescriptorKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset|Action")
	FName SelectSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset|Action")
	FName HoverSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset|Visual")
	TObjectPtr<UMaterialInterface> HoverBorderMaterial = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset|Visual")
	TObjectPtr<UMaterialInterface> SelectedBorderMaterial = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset|Visual")
	FLinearColor NormalTint = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset|Visual")
	FLinearColor HoverTint = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset|Visual")
	FLinearColor SelectedTint = FLinearColor(1.0f, 0.18f, 0.52f, 1.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	TArray<FName> Tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Preset")
	bool bEnabled = true;
};

/**
 * Runtime payload copied from a preset catalog item.
 *
 * Responsibility:
 * - Keep the immutable DataAsset item values available from a runtime tile item.
 * - Let click handlers resolve typed asset refs by slot/kind.
 *
 * Must not:
 * - Mutate the source DataAsset.
 * - Apply the selected asset to world actors by itself.
 */
UCLASS(BlueprintType)
class VDJMVCARD_API UVcardPresetItemRuntimeData : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Preset")
	void InitializeFromPresetItemData(const FVcardPresetItemData& presetItemData);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Preset")
	bool FindAssetBySlot(FName slotKey, FVcardPresetAssetRef& outAssetRef) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Preset")
	bool FindFirstAssetByKind(EVcardPresetAssetKind assetKind, FVcardPresetAssetRef& outAssetRef) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Preset")
	bool GetPrimaryAssetRef(FVcardPresetAssetRef& outAssetRef) const;

	UFUNCTION(BlueprintPure, Category = "Vcard|Preset")
	FVcardPresetItemData GetPresetItemData() const { return PresetItemData; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Preset")
	FName GetItemId() const { return PresetItemData.ItemId; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Preset")
	FText GetDisplayName() const { return PresetItemData.DisplayName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Preset")
	FName GetGroupKey() const { return PresetItemData.GroupKey; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Preset")
	EVcardPresetAssetKind GetPrimaryKind() const { return PresetItemData.PrimaryKind; }

protected:
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Vcard|Preset")
	FVcardPresetItemData PresetItemData;
};

/**
 * Editable catalog for V-card selectable presets.
 *
 * Responsibility:
 * - Store background, motion, sound, animation, and future preset rows in one catalog.
 * - Convert catalog rows into runtime UVcardTileItemDataState objects for TileView.
 *
 * Must not:
 * - Store user runtime history.
 * - Own thumbnail loading; AVcardTileImageLoadBroker handles that after item creation.
 */
UCLASS(BlueprintType)
class VDJMVCARD_API UVcardPresetCatalogDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Vcard|Preset")
	TArray<FVcardPresetItemData> GetPresetItems() const { return PresetItems; }
	UFUNCTION(BlueprintCallable, Category = "Vcard|Preset")
	TArray<FName> GetGroupKeys() const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Preset")
	bool FindPresetItemById(FName itemId, FVcardPresetItemData& outPresetItemData) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Preset")
	void GetPresetItemsByGroup(FName groupKey, TArray<FVcardPresetItemData>& outPresetItems) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Preset")
	bool CreateTileItemDataStates(UObject* outer, TArray<UVcardTileItemDataState*>& outItemDataStates, FString& outErrorReason) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|Preset")
	bool CreateTileItemDataStatesByGroup(FName groupKey, UObject* outer, TArray<UVcardTileItemDataState*>& outItemDataStates, FString& outErrorReason) const;

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Preset")
	TArray<FVcardPresetItemData> PresetItems;

private:
	UVcardTileItemDataState* CreateTileItemDataState(UObject* outer, const FVcardPresetItemData& presetItemData) const;
	TSoftObjectPtr<UTexture2D> ResolveTileSourceTexture(const FVcardPresetItemData& presetItemData) const;
	bool ResolveLocalImagePath(const FVcardPresetItemData& presetItemData, FString& outLocalImagePath) const;
	FSoftObjectPath ResolvePrimaryAssetPath(const FVcardPresetItemData& presetItemData) const;
};
