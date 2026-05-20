#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "VdjmVcardWidgetBase.h"
#include "VdjmVcardSelectableWidgets.generated.h"

class UBorder;
class UButton;
class UImage;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UVcardSelectableGroupWidget;
class UVcardSelectableItemWidget;

UENUM(BlueprintType)
enum class EVcardSelectableItemVisualState : uint8
{
	ENormal UMETA(DisplayName = "Normal"),
	EHovered UMETA(DisplayName = "Hovered"),
	EClicked UMETA(DisplayName = "Clicked"),
	EDisabled UMETA(DisplayName = "Disabled")
};

/**
 * Data used by a selectable item widget.
 *
 * Responsibility:
 * - Describe one UI item that can be clicked/hovered inside a parent group.
 * - Carry visual colors and optional payload/action ids.
 *
 * Must not:
 * - Store runtime parent widget references.
 * - Store authoritative runtime selection state; the child widget owns that.
 */
USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardSelectableItemDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable")
	FText DisplayText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable")
	TSubclassOf<UVcardSelectableItemWidget> ItemWidgetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable")
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable")
	TObjectPtr<UObject> PayloadData = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable")
	FSoftObjectPath PayloadPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable")
	FName ActionDescriptorKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable|Signal")
	FName ClickSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable|Signal")
	FName HoverSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable|Visual")
	FLinearColor NormalColor = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable|Visual")
	FLinearColor HoverColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable|Visual")
	FLinearColor ClickColor = FLinearColor(1.0f, 0.18f, 0.52f, 1.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable|Visual")
	FLinearColor DeActiveColor = FLinearColor(0.55f, 0.58f, 0.68f, 1.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable|Visual")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Selectable|Visual")
	bool bLockColorOnClick = true;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardSelectableItemState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vcard|Selectable")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vcard|Selectable")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vcard|Selectable")
	bool bHovered = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vcard|Selectable")
	bool bClicked = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vcard|Selectable")
	bool bColorLocked = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vcard|Selectable")
	EVcardSelectableItemVisualState VisualState = EVcardSelectableItemVisualState::ENormal;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Vcard|Selectable")
	FLinearColor CurrentColor = FLinearColor::White;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVcardSelectableItemDelegate, UVcardSelectableItemWidget*, ItemWidget, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVcardSelectableItemHoverDelegate, UVcardSelectableItemWidget*, ItemWidget, FName, ItemId, bool, bIsHovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVcardSelectableSignalRequestDelegate, FName, SignalTag, UVcardSelectableItemWidget*, ItemWidget);

/**
 * Parent group for selectable item widgets.
 *
 * Responsibility:
 * - Build/register selectable children from descriptors.
 * - Receive child click/hover triggers and forward visual feedback.
 * - Broadcast signal requests for flow binding.
 *
 * Must not:
 * - Own the authoritative clicked/hovered state; child widgets do.
 * - Decide final visual layout beyond the optional host panel.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardSelectableGroupWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Selectable")
	void SetSelectableItems(const TArray<FVcardSelectableItemDescriptor>& itemDescriptors);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Selectable")
	void AddSelectableItem(const FVcardSelectableItemDescriptor& itemDescriptor);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Selectable")
	void ClearSelectableItems();
	UFUNCTION(BlueprintCallable, Category = "Vcard|Selectable")
	bool RefreshSelectableItems(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Selectable")
	bool RegisterSelectableChild(UVcardSelectableItemWidget* itemWidget);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Selectable")
	bool UnregisterSelectableChild(UVcardSelectableItemWidget* itemWidget);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Selectable")
	bool HandleSelectableChildClicked(UVcardSelectableItemWidget* itemWidget);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Selectable")
	bool HandleSelectableChildHoverChanged(UVcardSelectableItemWidget* itemWidget, bool bIsHovered);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Selectable")
	bool RequestSignalForChild(UVcardSelectableItemWidget* itemWidget, FName signalTag);

	UFUNCTION(BlueprintPure, Category = "Vcard|Selectable")
	TArray<FVcardSelectableItemDescriptor> GetSelectableItemDescriptors() const { return ItemDescriptors; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Selectable")
	TArray<UVcardSelectableItemWidget*> GetSelectableChildren() const;
	UFUNCTION(BlueprintPure, Category = "Vcard|Selectable")
	TArray<FVcardSelectableItemState> GetSelectableChildStates() const;
	UFUNCTION(BlueprintPure, Category = "Vcard|Selectable")
	UVcardSelectableItemWidget* FindSelectableChildById(FName itemId) const;

	UPROPERTY(BlueprintAssignable, Category = "Vcard|Selectable")
	FVcardSelectableItemDelegate OnSelectableItemClicked;

	UPROPERTY(BlueprintAssignable, Category = "Vcard|Selectable")
	FVcardSelectableItemHoverDelegate OnSelectableItemHoverChanged;

	UPROPERTY(BlueprintAssignable, Category = "Vcard|Selectable")
	FVcardSelectableSignalRequestDelegate OnSelectableSignalRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Selectable")
	void BP_OnSelectableItemsChanged(const TArray<FVcardSelectableItemDescriptor>& itemDescriptors);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Selectable")
	void BP_OnSelectableChildrenRebuilt(const TArray<UVcardSelectableItemWidget*>& itemWidgets);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Selectable")
	void BP_OnSelectableItemClicked(UVcardSelectableItemWidget* itemWidget, FName itemId);
	UFUNCTION(BlueprintNativeEvent, Category = "Vcard|Selectable")
	bool RouteSelectableItemClicked(UVcardSelectableItemWidget* itemWidget, FName itemId, const FVcardSelectableItemDescriptor& itemDescriptor);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Selectable")
	void BP_OnSelectableItemHoverChanged(UVcardSelectableItemWidget* itemWidget, FName itemId, bool bIsHovered);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Selectable")
	void BP_OnSelectableSignalRequested(FName signalTag, UVcardSelectableItemWidget* itemWidget);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|Selectable")
	TObjectPtr<UPanelWidget> ItemHostPanel;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Selectable")
	TArray<FVcardSelectableItemDescriptor> ItemDescriptors;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Selectable")
	TSubclassOf<UVcardSelectableItemWidget> DefaultItemWidgetClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Selectable")
	bool bExclusiveClick = true;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Selectable")
	bool bToggleClickedWhenNotExclusive = true;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Selectable")
	bool bEmitSignalWhenClicked = true;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Selectable")
	bool bEmitSignalWhenHovered = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Selectable")
	bool bRebuildOnConstruct = true;

private:
	bool CreateSelectableChild(const FVcardSelectableItemDescriptor& itemDescriptor, UVcardSelectableItemWidget*& outItemWidget, FString& outErrorReason);
	void ApplyClickFeedback(UVcardSelectableItemWidget* clickedItemWidget);
	bool ContainsSelectableChild(UVcardSelectableItemWidget* itemWidget) const;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UVcardSelectableItemWidget>> mItemWidgets;
};

/**
 * Child item for selectable groups.
 *
 * Responsibility:
 * - Own clicked/hovered/enabled/color-lock runtime state.
 * - Apply color to optional text/image/border targets.
 * - Notify the parent group when user input occurs.
 *
 * Must not:
 * - Decide sibling states.
 * - Store descriptor registry or flow manager ownership.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardSelectableItemWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void SetSelectableGroup(UVcardSelectableGroupWidget* groupWidget);
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void SetItemDescriptor(const FVcardSelectableItemDescriptor& itemDescriptor);
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void SetColor(FLinearColor targetColor);
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void SetEnabledState(bool bEnabled);
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void SetHovered(bool bIsHovered);
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void SetClicked(bool bIsClicked, bool bLockColor);
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void SetColorLocked(bool bLocked);
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void ClearInteractionState();
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void RefreshVisualColor();
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void NotifyClicked();
	UFUNCTION(BlueprintCallable, Category = "Vcard|SelectableItem")
	void NotifyHoverChanged(bool bIsHovered);

	UFUNCTION(BlueprintPure, Category = "Vcard|SelectableItem")
	FVcardSelectableItemDescriptor GetItemDescriptor() const { return mItemDescriptor; }
	UFUNCTION(BlueprintPure, Category = "Vcard|SelectableItem")
	FVcardSelectableItemState GetSelectableItemState() const;
	UFUNCTION(BlueprintPure, Category = "Vcard|SelectableItem")
	FName GetItemId() const { return mItemDescriptor.ItemId; }
	UFUNCTION(BlueprintPure, Category = "Vcard|SelectableItem")
	bool IsEnabledState() const { return mState.bEnabled; }
	UFUNCTION(BlueprintPure, Category = "Vcard|SelectableItem")
	bool IsItemHovered() const { return mState.bHovered; }
	UFUNCTION(BlueprintPure, Category = "Vcard|SelectableItem")
	bool IsClicked() const { return mState.bClicked; }
	UFUNCTION(BlueprintPure, Category = "Vcard|SelectableItem")
	bool IsColorLocked() const { return mState.bColorLocked; }
	UFUNCTION(BlueprintPure, Category = "Vcard|SelectableItem")
	UVcardSelectableGroupWidget* GetSelectableGroup() const { return mGroupWidget.Get(); }
	UFUNCTION(BlueprintPure, Category = "Vcard|SelectableItem")
	UVcardSelectableGroupWidget* FindOwnerSelectableGroup() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& inMouseEvent) override;

	UFUNCTION()
	void HandleButtonClicked();
	UFUNCTION()
	void HandleButtonHovered();
	UFUNCTION()
	void HandleButtonUnhovered();

	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|SelectableItem")
	void BP_OnItemDescriptorChanged(const FVcardSelectableItemDescriptor& itemDescriptor);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|SelectableItem")
	void BP_OnSelectableStateChanged(const FVcardSelectableItemState& itemState);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|SelectableItem")
	void BP_OnColorApplied(FLinearColor targetColor);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|SelectableItem")
	TObjectPtr<UButton> Button_Item;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|SelectableItem")
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|SelectableItem")
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|SelectableItem")
	TObjectPtr<UImage> Image_Tint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|SelectableItem")
	TObjectPtr<UBorder> Border_Tint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|SelectableItem")
	TObjectPtr<UWidget> Widget_HoverLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|SelectableItem")
	TObjectPtr<UWidget> Widget_ClickLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|SelectableItem")
	TObjectPtr<UWidget> Widget_DisabledLayer;

private:
	void BindButtonEvents();
	void UnbindButtonEvents();
	EVcardSelectableItemVisualState ResolveVisualState() const;
	FLinearColor ResolveColorForState(EVcardSelectableItemVisualState visualState) const;
	void ApplyLayerVisibility(EVcardSelectableItemVisualState visualState) const;
	void SetOptionalLayerVisible(UWidget* layerWidget, bool bVisible) const;
	void BroadcastStateChanged();

	UPROPERTY(Transient)
	TWeakObjectPtr<UVcardSelectableGroupWidget> mGroupWidget;

	UPROPERTY(Transient)
	FVcardSelectableItemDescriptor mItemDescriptor;

	UPROPERTY(Transient)
	FVcardSelectableItemState mState;
};
