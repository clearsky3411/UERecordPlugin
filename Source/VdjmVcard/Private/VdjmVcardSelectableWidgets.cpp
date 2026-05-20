#include "VdjmVcardSelectableWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "VdjmVcard.h"

void UVcardSelectableGroupWidget::SetSelectableItems(const TArray<FVcardSelectableItemDescriptor>& itemDescriptors)
{
	ItemDescriptors = itemDescriptors;

	FString errorReason;
	RefreshSelectableItems(errorReason);
	BP_OnSelectableItemsChanged(ItemDescriptors);
}

void UVcardSelectableGroupWidget::AddSelectableItem(const FVcardSelectableItemDescriptor& itemDescriptor)
{
	ItemDescriptors.Add(itemDescriptor);

	FString errorReason;
	RefreshSelectableItems(errorReason);
	BP_OnSelectableItemsChanged(ItemDescriptors);
}

void UVcardSelectableGroupWidget::ClearSelectableItems()
{
	for (TWeakObjectPtr<UVcardSelectableItemWidget>& itemWidget : mItemWidgets)
	{
		if (itemWidget.IsValid())
		{
			itemWidget->SetSelectableGroup(nullptr);
			itemWidget->ClearInteractionState();
		}
	}

	ItemDescriptors.Reset();
	mItemWidgets.Reset();

	if (IsValid(ItemHostPanel))
	{
		ItemHostPanel->ClearChildren();
	}

	BP_OnSelectableItemsChanged(ItemDescriptors);
	BP_OnSelectableChildrenRebuilt(GetSelectableChildren());
}

bool UVcardSelectableGroupWidget::RefreshSelectableItems(FString& outErrorReason)
{
	outErrorReason.Reset();

	if (!IsValid(ItemHostPanel))
	{
		outErrorReason = TEXT("ItemHostPanel is not bound.");
		return false;
	}

	ItemHostPanel->ClearChildren();
	mItemWidgets.Reset();

	for (const FVcardSelectableItemDescriptor& itemDescriptor : ItemDescriptors)
	{
		UVcardSelectableItemWidget* itemWidget = nullptr;
		if (!CreateSelectableChild(itemDescriptor, itemWidget, outErrorReason))
		{
			return false;
		}

		ItemHostPanel->AddChild(itemWidget);
		RegisterSelectableChild(itemWidget);
	}

	BP_OnSelectableChildrenRebuilt(GetSelectableChildren());
	return true;
}

bool UVcardSelectableGroupWidget::RegisterSelectableChild(UVcardSelectableItemWidget* itemWidget)
{
	if (!IsValid(itemWidget))
	{
		return false;
	}

	if (!ContainsSelectableChild(itemWidget))
	{
		mItemWidgets.Add(itemWidget);
	}

	itemWidget->SetSelectableGroup(this);
	return true;
}

bool UVcardSelectableGroupWidget::UnregisterSelectableChild(UVcardSelectableItemWidget* itemWidget)
{
	if (!IsValid(itemWidget))
	{
		return false;
	}

	for (int32 itemIndex = mItemWidgets.Num() - 1; itemIndex >= 0; --itemIndex)
	{
		if (!mItemWidgets[itemIndex].IsValid() || mItemWidgets[itemIndex].Get() == itemWidget)
		{
			mItemWidgets.RemoveAt(itemIndex);
		}
	}

	if (itemWidget->GetSelectableGroup() == this)
	{
		itemWidget->SetSelectableGroup(nullptr);
	}

	return true;
}

bool UVcardSelectableGroupWidget::HandleSelectableChildClicked(UVcardSelectableItemWidget* itemWidget)
{
	if (!ContainsSelectableChild(itemWidget) || !itemWidget->IsEnabledState())
	{
		return false;
	}

	ApplyClickFeedback(itemWidget);

	const FName itemId = itemWidget->GetItemId();
	OnSelectableItemClicked.Broadcast(itemWidget, itemId);
	BP_OnSelectableItemClicked(itemWidget, itemId);

	const FVcardSelectableItemDescriptor itemDescriptor = itemWidget->GetItemDescriptor();
	const bool bRouteHandled = RouteSelectableItemClicked(itemWidget, itemId, itemDescriptor);
	if (bRouteHandled)
	{
		UE_LOG(LogVdjmVcard, Display, TEXT("VcardSelectableGroup click routed. Group=%s Item=%s ActionDescriptor=%s Signal=%s"),
			*GetNameSafe(this),
			*itemId.ToString(),
			*itemDescriptor.ActionDescriptorKey.ToString(),
			*itemDescriptor.ClickSignalTag.ToString());
		return true;
	}

	if (bEmitSignalWhenClicked && !itemDescriptor.ClickSignalTag.IsNone())
	{
		RequestSignalForChild(itemWidget, itemDescriptor.ClickSignalTag);
	}

	return true;
}

bool UVcardSelectableGroupWidget::RouteSelectableItemClicked_Implementation(
	UVcardSelectableItemWidget* itemWidget,
	FName itemId,
	const FVcardSelectableItemDescriptor& itemDescriptor)
{
	return false;
}

bool UVcardSelectableGroupWidget::HandleSelectableChildHoverChanged(UVcardSelectableItemWidget* itemWidget, bool bIsHovered)
{
	if (!ContainsSelectableChild(itemWidget) || !itemWidget->IsEnabledState())
	{
		return false;
	}

	itemWidget->SetHovered(bIsHovered);

	const FName itemId = itemWidget->GetItemId();
	OnSelectableItemHoverChanged.Broadcast(itemWidget, itemId, bIsHovered);
	BP_OnSelectableItemHoverChanged(itemWidget, itemId, bIsHovered);

	const FVcardSelectableItemDescriptor itemDescriptor = itemWidget->GetItemDescriptor();
	if (bIsHovered && bEmitSignalWhenHovered && !itemDescriptor.HoverSignalTag.IsNone())
	{
		RequestSignalForChild(itemWidget, itemDescriptor.HoverSignalTag);
	}

	return true;
}

bool UVcardSelectableGroupWidget::RequestSignalForChild(UVcardSelectableItemWidget* itemWidget, FName signalTag)
{
	if (!ContainsSelectableChild(itemWidget) || signalTag.IsNone())
	{
		return false;
	}

	OnSelectableSignalRequested.Broadcast(signalTag, itemWidget);
	BP_OnSelectableSignalRequested(signalTag, itemWidget);
	return true;
}

TArray<UVcardSelectableItemWidget*> UVcardSelectableGroupWidget::GetSelectableChildren() const
{
	TArray<UVcardSelectableItemWidget*> itemWidgets;
	itemWidgets.Reserve(mItemWidgets.Num());

	for (const TWeakObjectPtr<UVcardSelectableItemWidget>& itemWidget : mItemWidgets)
	{
		if (itemWidget.IsValid())
		{
			itemWidgets.Add(itemWidget.Get());
		}
	}

	return itemWidgets;
}

TArray<FVcardSelectableItemState> UVcardSelectableGroupWidget::GetSelectableChildStates() const
{
	TArray<FVcardSelectableItemState> itemStates;
	itemStates.Reserve(mItemWidgets.Num());

	for (const TWeakObjectPtr<UVcardSelectableItemWidget>& itemWidget : mItemWidgets)
	{
		if (itemWidget.IsValid())
		{
			itemStates.Add(itemWidget->GetSelectableItemState());
		}
	}

	return itemStates;
}

UVcardSelectableItemWidget* UVcardSelectableGroupWidget::FindSelectableChildById(FName itemId) const
{
	for (const TWeakObjectPtr<UVcardSelectableItemWidget>& itemWidget : mItemWidgets)
	{
		if (itemWidget.IsValid() && itemWidget->GetItemId() == itemId)
		{
			return itemWidget.Get();
		}
	}

	return nullptr;
}

void UVcardSelectableGroupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bRebuildOnConstruct)
	{
		FString errorReason;
		RefreshSelectableItems(errorReason);
	}
}

void UVcardSelectableGroupWidget::NativeDestruct()
{
	for (TWeakObjectPtr<UVcardSelectableItemWidget>& itemWidget : mItemWidgets)
	{
		if (itemWidget.IsValid() && itemWidget->GetSelectableGroup() == this)
		{
			itemWidget->SetSelectableGroup(nullptr);
		}
	}

	Super::NativeDestruct();
}

bool UVcardSelectableGroupWidget::CreateSelectableChild(
	const FVcardSelectableItemDescriptor& itemDescriptor,
	UVcardSelectableItemWidget*& outItemWidget,
	FString& outErrorReason)
{
	outItemWidget = nullptr;
	outErrorReason.Reset();

	TSubclassOf<UVcardSelectableItemWidget> itemWidgetClass = itemDescriptor.ItemWidgetClass;
	if (!*itemWidgetClass)
	{
		itemWidgetClass = DefaultItemWidgetClass;
	}

	if (!*itemWidgetClass)
	{
		outErrorReason = TEXT("Selectable item widget class is not assigned.");
		return false;
	}

	if (APlayerController* owningPlayer = GetOwningPlayer())
	{
		outItemWidget = CreateWidget<UVcardSelectableItemWidget>(owningPlayer, itemWidgetClass);
	}

	if (!IsValid(outItemWidget))
	{
		UWorld* world = GetWorld();
		if (world != nullptr)
		{
			outItemWidget = CreateWidget<UVcardSelectableItemWidget>(world, itemWidgetClass);
		}
	}

	if (!IsValid(outItemWidget))
	{
		outErrorReason = FString::Printf(TEXT("Failed to create selectable item widget '%s'."), *GetNameSafe(*itemWidgetClass));
		return false;
	}

	outItemWidget->ApplyVcardDescriptorContext(GetDescriptorRegistry(), GetVcardContextObject());
	outItemWidget->SetItemDescriptor(itemDescriptor);
	return true;
}

void UVcardSelectableGroupWidget::ApplyClickFeedback(UVcardSelectableItemWidget* clickedItemWidget)
{
	if (!IsValid(clickedItemWidget))
	{
		return;
	}

	if (bExclusiveClick)
	{
		for (const TWeakObjectPtr<UVcardSelectableItemWidget>& itemWidget : mItemWidgets)
		{
			if (itemWidget.IsValid())
			{
				const bool bClicked = itemWidget.Get() == clickedItemWidget;
				const FVcardSelectableItemDescriptor itemDescriptor = itemWidget->GetItemDescriptor();
				itemWidget->SetClicked(bClicked, bClicked && itemDescriptor.bLockColorOnClick);
			}
		}

		return;
	}

	const bool bNextClicked = bToggleClickedWhenNotExclusive ? !clickedItemWidget->IsClicked() : true;
	const FVcardSelectableItemDescriptor clickedDescriptor = clickedItemWidget->GetItemDescriptor();
	clickedItemWidget->SetClicked(bNextClicked, bNextClicked && clickedDescriptor.bLockColorOnClick);
}

bool UVcardSelectableGroupWidget::ContainsSelectableChild(UVcardSelectableItemWidget* itemWidget) const
{
	if (!IsValid(itemWidget))
	{
		return false;
	}

	for (const TWeakObjectPtr<UVcardSelectableItemWidget>& candidate : mItemWidgets)
	{
		if (candidate.Get() == itemWidget)
		{
			return true;
		}
	}

	return false;
}

void UVcardSelectableItemWidget::SetSelectableGroup(UVcardSelectableGroupWidget* groupWidget)
{
	mGroupWidget = groupWidget;
}

void UVcardSelectableItemWidget::SetItemDescriptor(const FVcardSelectableItemDescriptor& itemDescriptor)
{
	mItemDescriptor = itemDescriptor;
	mState.ItemId = itemDescriptor.ItemId;
	mState.bEnabled = itemDescriptor.bEnabled;
	mState.bHovered = false;
	mState.bClicked = false;
	mState.bColorLocked = false;

	if (IsValid(Text_Label))
	{
		Text_Label->SetText(itemDescriptor.DisplayText);
	}

	if (IsValid(Button_Item))
	{
		Button_Item->SetIsEnabled(itemDescriptor.bEnabled);
	}

	BP_OnItemDescriptorChanged(itemDescriptor);
	RefreshVisualColor();
}

void UVcardSelectableItemWidget::SetColor(FLinearColor targetColor)
{
	mState.CurrentColor = targetColor;

	if (IsValid(Text_Label))
	{
		Text_Label->SetColorAndOpacity(targetColor);
	}

	if (IsValid(Image_Icon))
	{
		Image_Icon->SetColorAndOpacity(targetColor);
	}

	if (IsValid(Image_Tint))
	{
		Image_Tint->SetColorAndOpacity(targetColor);
	}

	if (IsValid(Border_Tint))
	{
		Border_Tint->SetBrushColor(targetColor);
	}

	BP_OnColorApplied(targetColor);
}

void UVcardSelectableItemWidget::SetEnabledState(bool bEnabled)
{
	mState.bEnabled = bEnabled;

	if (IsValid(Button_Item))
	{
		Button_Item->SetIsEnabled(bEnabled);
	}

	if (!bEnabled)
	{
		mState.bHovered = false;
	}

	RefreshVisualColor();
}

void UVcardSelectableItemWidget::SetHovered(bool bIsHovered)
{
	if (!mState.bEnabled)
	{
		return;
	}

	mState.bHovered = bIsHovered;
	RefreshVisualColor();
}

void UVcardSelectableItemWidget::SetClicked(bool bIsClicked, bool bLockColor)
{
	if (!mState.bEnabled && bIsClicked)
	{
		return;
	}

	mState.bClicked = bIsClicked;
	mState.bColorLocked = bIsClicked && bLockColor;
	RefreshVisualColor();
}

void UVcardSelectableItemWidget::SetColorLocked(bool bLocked)
{
	mState.bColorLocked = bLocked;
	RefreshVisualColor();
}

void UVcardSelectableItemWidget::ClearInteractionState()
{
	mState.bHovered = false;
	mState.bClicked = false;
	mState.bColorLocked = false;
	RefreshVisualColor();
}

void UVcardSelectableItemWidget::RefreshVisualColor()
{
	const EVcardSelectableItemVisualState visualState = ResolveVisualState();
	mState.VisualState = visualState;

	SetColor(ResolveColorForState(visualState));
	ApplyLayerVisibility(visualState);
	BroadcastStateChanged();
}

void UVcardSelectableItemWidget::NotifyClicked()
{
	UVcardSelectableGroupWidget* groupWidget = mGroupWidget.Get();
	if (!IsValid(groupWidget))
	{
		groupWidget = FindOwnerSelectableGroup();
	}

	if (IsValid(groupWidget))
	{
		groupWidget->HandleSelectableChildClicked(this);
	}
}

void UVcardSelectableItemWidget::NotifyHoverChanged(bool bIsHovered)
{
	UVcardSelectableGroupWidget* groupWidget = mGroupWidget.Get();
	if (!IsValid(groupWidget))
	{
		groupWidget = FindOwnerSelectableGroup();
	}

	if (IsValid(groupWidget))
	{
		groupWidget->HandleSelectableChildHoverChanged(this, bIsHovered);
		return;
	}

	SetHovered(bIsHovered);
}

FVcardSelectableItemState UVcardSelectableItemWidget::GetSelectableItemState() const
{
	return mState;
}

UVcardSelectableGroupWidget* UVcardSelectableItemWidget::FindOwnerSelectableGroup() const
{
	UWidget* parentWidget = GetParent();
	while (parentWidget != nullptr)
	{
		if (UVcardSelectableGroupWidget* groupWidget = Cast<UVcardSelectableGroupWidget>(parentWidget))
		{
			return groupWidget;
		}

		parentWidget = parentWidget->GetParent();
	}

	return nullptr;
}

void UVcardSelectableItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindButtonEvents();
	RefreshVisualColor();
}

void UVcardSelectableItemWidget::NativeDestruct()
{
	UnbindButtonEvents();

	Super::NativeDestruct();
}

void UVcardSelectableItemWidget::NativeOnMouseEnter(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent)
{
	Super::NativeOnMouseEnter(inGeometry, inMouseEvent);
	NotifyHoverChanged(true);
}

void UVcardSelectableItemWidget::NativeOnMouseLeave(const FPointerEvent& inMouseEvent)
{
	Super::NativeOnMouseLeave(inMouseEvent);
	NotifyHoverChanged(false);
}

void UVcardSelectableItemWidget::HandleButtonClicked()
{
	NotifyClicked();
}

void UVcardSelectableItemWidget::HandleButtonHovered()
{
	NotifyHoverChanged(true);
}

void UVcardSelectableItemWidget::HandleButtonUnhovered()
{
	NotifyHoverChanged(false);
}

void UVcardSelectableItemWidget::BindButtonEvents()
{
	if (!IsValid(Button_Item))
	{
		return;
	}

	Button_Item->OnClicked.RemoveDynamic(this, &UVcardSelectableItemWidget::HandleButtonClicked);
	Button_Item->OnHovered.RemoveDynamic(this, &UVcardSelectableItemWidget::HandleButtonHovered);
	Button_Item->OnUnhovered.RemoveDynamic(this, &UVcardSelectableItemWidget::HandleButtonUnhovered);

	Button_Item->OnClicked.AddDynamic(this, &UVcardSelectableItemWidget::HandleButtonClicked);
	Button_Item->OnHovered.AddDynamic(this, &UVcardSelectableItemWidget::HandleButtonHovered);
	Button_Item->OnUnhovered.AddDynamic(this, &UVcardSelectableItemWidget::HandleButtonUnhovered);
}

void UVcardSelectableItemWidget::UnbindButtonEvents()
{
	if (!IsValid(Button_Item))
	{
		return;
	}

	Button_Item->OnClicked.RemoveDynamic(this, &UVcardSelectableItemWidget::HandleButtonClicked);
	Button_Item->OnHovered.RemoveDynamic(this, &UVcardSelectableItemWidget::HandleButtonHovered);
	Button_Item->OnUnhovered.RemoveDynamic(this, &UVcardSelectableItemWidget::HandleButtonUnhovered);
}

EVcardSelectableItemVisualState UVcardSelectableItemWidget::ResolveVisualState() const
{
	if (!mState.bEnabled)
	{
		return EVcardSelectableItemVisualState::EDisabled;
	}

	if (mState.bClicked || mState.bColorLocked)
	{
		return EVcardSelectableItemVisualState::EClicked;
	}

	if (mState.bHovered)
	{
		return EVcardSelectableItemVisualState::EHovered;
	}

	return EVcardSelectableItemVisualState::ENormal;
}

FLinearColor UVcardSelectableItemWidget::ResolveColorForState(EVcardSelectableItemVisualState visualState) const
{
	switch (visualState)
	{
	case EVcardSelectableItemVisualState::EDisabled:
		return mItemDescriptor.DeActiveColor;
	case EVcardSelectableItemVisualState::EClicked:
		return mItemDescriptor.ClickColor;
	case EVcardSelectableItemVisualState::EHovered:
		return mItemDescriptor.HoverColor;
	case EVcardSelectableItemVisualState::ENormal:
	default:
		return mItemDescriptor.NormalColor;
	}
}

void UVcardSelectableItemWidget::ApplyLayerVisibility(EVcardSelectableItemVisualState visualState) const
{
	SetOptionalLayerVisible(Widget_DisabledLayer, visualState == EVcardSelectableItemVisualState::EDisabled);
	SetOptionalLayerVisible(Widget_ClickLayer, visualState == EVcardSelectableItemVisualState::EClicked);
	SetOptionalLayerVisible(Widget_HoverLayer, visualState == EVcardSelectableItemVisualState::EHovered);
}

void UVcardSelectableItemWidget::SetOptionalLayerVisible(UWidget* layerWidget, bool bVisible) const
{
	if (!IsValid(layerWidget))
	{
		return;
	}

	layerWidget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UVcardSelectableItemWidget::BroadcastStateChanged()
{
	BP_OnSelectableStateChanged(mState);
}
