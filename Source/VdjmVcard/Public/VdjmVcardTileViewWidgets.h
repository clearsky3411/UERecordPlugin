#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "VdjmVcardWidgetBase.h"
#include "VdjmVcardTileViewWidgets.generated.h"

class UBorder;
class UImage;
class UMaterialInterface;
class UTileView;
class UWidget;
class UVcardDescriptorBase;
class UVcardTileItemDescriptor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVcardTileItemDelegate, UVcardTileItemDescriptor*, ItemDescriptor, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVcardTileItemHoverDelegate, UVcardTileItemDescriptor*, ItemDescriptor, FName, ItemId, bool, bIsHovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVcardTileSignalRequestDelegate, FName, SignalTag, UVcardTileItemDescriptor*, ItemDescriptor);

/**
 * TileView item descriptor.
 *
 * Responsibility:
 * - Represent one selectable tile item as UObject data for UTileView.
 * - Carry payload/action ids and visual hints used by the entry widget.
 *
 * Must not:
 * - Emit flow signals directly.
 * - Own entry widgets; UTileView virtualizes and reuses entries.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMVCARD_API UVcardTileItemDescriptor : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Tile")
	void SetRuntimeHovered(bool bIsHovered);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Tile")
	void SetRuntimeSelected(bool bIsSelected);

	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	FName GetItemId() const { return ItemId; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	FText GetDisplayName() const { return DisplayName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	UObject* GetPayloadData() const { return PayloadData; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	FSoftObjectPath GetPayloadPath() const { return PayloadPath; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	FName GetActionDescriptorId() const { return ActionDescriptorId; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	UVcardDescriptorBase* GetActionDescriptor() const { return ActionDescriptor; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	FName GetSelectSignalTag() const { return SelectSignalTag; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	FName GetHoverSignalTag() const { return HoverSignalTag; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	bool IsRuntimeHovered() const { return mbRuntimeHovered; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tile")
	bool IsRuntimeSelected() const { return mbRuntimeSelected; }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile")
	TSoftObjectPtr<UObject> Thumbnail;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile")
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile")
	TObjectPtr<UObject> PayloadData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile")
	FSoftObjectPath PayloadPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile")
	FName ActionDescriptorId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile")
	TObjectPtr<UVcardDescriptorBase> ActionDescriptor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile|Signal")
	FName SelectSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile|Signal")
	FName HoverSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile|Visual")
	TObjectPtr<UMaterialInterface> HoverBorderMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile|Visual")
	TObjectPtr<UMaterialInterface> SelectedBorderMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile|Visual")
	FLinearColor NormalTint = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile|Visual")
	FLinearColor HoverTint = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tile|Visual")
	FLinearColor SelectedTint = FLinearColor(1.0f, 0.18f, 0.52f, 1.0f);

private:
	UPROPERTY(Transient)
	bool mbRuntimeHovered = false;

	UPROPERTY(Transient)
	bool mbRuntimeSelected = false;
};

/**
 * TileView owner for V-card selectable item descriptors.
 *
 * Responsibility:
 * - Own the item descriptor list and UTileView selection state.
 * - Broadcast selection/hover/signal requests for external flow binding.
 *
 * Must not:
 * - Call the flow manager directly.
 * - Decide how an entry is visually designed.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardTileViewWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileView")
	void SetTileItems(const TArray<UVcardTileItemDescriptor*>& itemDescriptors);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileView")
	void AddTileItem(UVcardTileItemDescriptor* itemDescriptor);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileView")
	void ClearTileItems();
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileView")
	bool RefreshTileView(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileView")
	bool SelectTileItem(UVcardTileItemDescriptor* itemDescriptor, bool bEmitSignalRequest = true);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileView")
	bool SelectTileItemById(FName itemId, bool bEmitSignalRequest = true);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileView")
	bool RequestSignalForItem(UVcardTileItemDescriptor* itemDescriptor, FName signalTag);

	UFUNCTION(BlueprintPure, Category = "Vcard|TileView")
	TArray<UVcardTileItemDescriptor*> GetTileItems() const;
	UFUNCTION(BlueprintPure, Category = "Vcard|TileView")
	UVcardTileItemDescriptor* GetSelectedTileItem() const { return mSelectedItem.Get(); }
	UFUNCTION(BlueprintPure, Category = "Vcard|TileView")
	UTileView* GetTileView() const { return TileView; }

	UPROPERTY(BlueprintAssignable, Category = "Vcard|TileView")
	FVcardTileItemDelegate OnTileItemSelected;

	UPROPERTY(BlueprintAssignable, Category = "Vcard|TileView")
	FVcardTileItemHoverDelegate OnTileItemHoveredChanged;

	UPROPERTY(BlueprintAssignable, Category = "Vcard|TileView")
	FVcardTileSignalRequestDelegate OnTileSignalRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|TileView")
	void BP_OnTileItemsChanged(const TArray<UVcardTileItemDescriptor*>& itemDescriptors);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|TileView")
	void BP_OnTileItemSelected(UVcardTileItemDescriptor* itemDescriptor, FName itemId);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|TileView")
	void BP_OnTileItemHoveredChanged(UVcardTileItemDescriptor* itemDescriptor, FName itemId, bool bIsHovered);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|TileView")
	void BP_OnTileSignalRequested(FName signalTag, UVcardTileItemDescriptor* itemDescriptor);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|TileView")
	TObjectPtr<UTileView> TileView;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|TileView")
	bool bEmitSignalWhenTileClicked = true;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|TileView")
	bool bEmitSignalWhenTileHovered = false;

private:
	void BindTileViewEvents();
	void UnbindTileViewEvents();
	void HandleTileItemClicked(UObject* itemObject);
	void HandleTileItemSelectionChanged(UObject* itemObject);
	void HandleTileItemHoveredChanged(UObject* itemObject, bool bIsHovered);
	void UpdateRuntimeSelection(UVcardTileItemDescriptor* selectedItem);
	bool ContainsTileItem(UVcardTileItemDescriptor* itemDescriptor) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVcardTileItemDescriptor>> mItemDescriptors;

	UPROPERTY(Transient)
	TWeakObjectPtr<UVcardTileItemDescriptor> mSelectedItem;

	bool mbTileViewEventsBound = false;
};

/**
 * TileView entry widget for UVcardTileItemDescriptor.
 *
 * Responsibility:
 * - Reflect hover and selection state in optional layers/materials/tints.
 *
 * Must not:
 * - Own selection policy.
 * - Emit flow signals or mutate the TileView list.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardTileEntryWidget : public UVcardWidgetBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileEntry")
	void SetTileItemDescriptor(UVcardTileItemDescriptor* itemDescriptor);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileEntry")
	void SetEntryHovered(bool bIsHovered);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileEntry")
	void RefreshVisualState();

	UFUNCTION(BlueprintPure, Category = "Vcard|TileEntry")
	UVcardTileItemDescriptor* GetTileItemDescriptor() const { return mItemDescriptor.Get(); }
	UFUNCTION(BlueprintPure, Category = "Vcard|TileEntry")
	bool IsEntryHovered() const { return mbEntryHovered; }
	UFUNCTION(BlueprintPure, Category = "Vcard|TileEntry")
	bool IsEntrySelected() const { return mbEntrySelected; }

protected:
	virtual void NativeOnListItemObjectSet(UObject* listItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;
	virtual void NativeOnMouseEnter(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& inMouseEvent) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|TileEntry")
	void BP_OnTileItemDescriptorChanged(UVcardTileItemDescriptor* itemDescriptor);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|TileEntry")
	void BP_OnTileVisualStateChanged(UVcardTileItemDescriptor* itemDescriptor, bool bIsHovered, bool bIsSelected);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|TileEntry")
	TObjectPtr<UWidget> HoverBorderLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|TileEntry")
	TObjectPtr<UWidget> SelectedBorderLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|TileEntry")
	TObjectPtr<UImage> Image_HoverBorderMaterial;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|TileEntry")
	TObjectPtr<UImage> Image_SelectedBorderMaterial;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|TileEntry")
	TObjectPtr<UImage> Image_Tint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|TileEntry")
	TObjectPtr<UBorder> Border_Tint;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|TileEntry")
	bool bHideHoverLayerWhenSelected = false;

private:
	void SetOptionalLayerVisible(UWidget* layerWidget, bool bVisible) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<UVcardTileItemDescriptor> mItemDescriptor;

	bool mbEntryHovered = false;
	bool mbEntrySelected = false;
};
