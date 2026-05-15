#include "VdjmVcardTileViewWidgets.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TileView.h"
#include "Materials/MaterialInterface.h"
#include "VdjmVcard.h"

void UVcardTileItemDescriptor::SetRuntimeHovered(bool bIsHovered)
{
	mbRuntimeHovered = bIsHovered;
}

void UVcardTileItemDescriptor::SetRuntimeSelected(bool bIsSelected)
{
	mbRuntimeSelected = bIsSelected;
}

void UVcardTileViewWidget::SetTileItems(const TArray<UVcardTileItemDescriptor*>& itemDescriptors)
{
	mItemDescriptors.Reset();
	mItemDescriptors.Reserve(itemDescriptors.Num());

	for (UVcardTileItemDescriptor* itemDescriptor : itemDescriptors)
	{
		if (IsValid(itemDescriptor))
		{
			itemDescriptor->SetRuntimeHovered(false);
			itemDescriptor->SetRuntimeSelected(false);
			mItemDescriptors.Add(itemDescriptor);
		}
	}

	if (!mSelectedItem.IsValid() || !ContainsTileItem(mSelectedItem.Get()))
	{
		mSelectedItem.Reset();
	}

	FString errorReason;
	RefreshTileView(errorReason);
	BP_OnTileItemsChanged(GetTileItems());
}

void UVcardTileViewWidget::AddTileItem(UVcardTileItemDescriptor* itemDescriptor)
{
	if (!IsValid(itemDescriptor) || ContainsTileItem(itemDescriptor))
	{
		return;
	}

	itemDescriptor->SetRuntimeHovered(false);
	itemDescriptor->SetRuntimeSelected(false);
	mItemDescriptors.Add(itemDescriptor);

	FString errorReason;
	RefreshTileView(errorReason);
	BP_OnTileItemsChanged(GetTileItems());
}

void UVcardTileViewWidget::ClearTileItems()
{
	for (UVcardTileItemDescriptor* itemDescriptor : mItemDescriptors)
	{
		if (IsValid(itemDescriptor))
		{
			itemDescriptor->SetRuntimeHovered(false);
			itemDescriptor->SetRuntimeSelected(false);
		}
	}

	mItemDescriptors.Reset();
	mSelectedItem.Reset();

	if (IsValid(TileView))
	{
		TileView->ClearListItems();
	}

	BP_OnTileItemsChanged(GetTileItems());
}

bool UVcardTileViewWidget::RefreshTileView(FString& outErrorReason)
{
	outErrorReason.Reset();

	if (!IsValid(TileView))
	{
		outErrorReason = TEXT("TileView is not bound.");
		return false;
	}

	TArray<UObject*> listItems;
	listItems.Reserve(mItemDescriptors.Num());
	for (UVcardTileItemDescriptor* itemDescriptor : mItemDescriptors)
	{
		if (IsValid(itemDescriptor))
		{
			listItems.Add(itemDescriptor);
		}
	}

	TileView->SetListItems(listItems);

	if (mSelectedItem.IsValid() && ContainsTileItem(mSelectedItem.Get()))
	{
		TileView->SetSelectedItem(mSelectedItem.Get());
		UpdateRuntimeSelection(mSelectedItem.Get());
	}
	else
	{
		UpdateRuntimeSelection(nullptr);
	}

	return true;
}

bool UVcardTileViewWidget::SelectTileItem(UVcardTileItemDescriptor* itemDescriptor, bool bEmitSignalRequest)
{
	if (!IsValid(itemDescriptor) || !ContainsTileItem(itemDescriptor))
	{
		return false;
	}

	mSelectedItem = itemDescriptor;
	UpdateRuntimeSelection(itemDescriptor);

	if (IsValid(TileView))
	{
		TileView->SetSelectedItem(itemDescriptor);
	}

	OnTileItemSelected.Broadcast(itemDescriptor, itemDescriptor->GetItemId());
	BP_OnTileItemSelected(itemDescriptor, itemDescriptor->GetItemId());

	if (bEmitSignalRequest && bEmitSignalWhenTileClicked && !itemDescriptor->GetSelectSignalTag().IsNone())
	{
		RequestSignalForItem(itemDescriptor, itemDescriptor->GetSelectSignalTag());
	}

	return true;
}

bool UVcardTileViewWidget::SelectTileItemById(FName itemId, bool bEmitSignalRequest)
{
	for (UVcardTileItemDescriptor* itemDescriptor : mItemDescriptors)
	{
		if (IsValid(itemDescriptor) && itemDescriptor->GetItemId() == itemId)
		{
			return SelectTileItem(itemDescriptor, bEmitSignalRequest);
		}
	}

	return false;
}

bool UVcardTileViewWidget::RequestSignalForItem(UVcardTileItemDescriptor* itemDescriptor, FName signalTag)
{
	if (!IsValid(itemDescriptor) || signalTag.IsNone())
	{
		return false;
	}

	OnTileSignalRequested.Broadcast(signalTag, itemDescriptor);
	BP_OnTileSignalRequested(signalTag, itemDescriptor);
	return true;
}

TArray<UVcardTileItemDescriptor*> UVcardTileViewWidget::GetTileItems() const
{
	TArray<UVcardTileItemDescriptor*> itemDescriptors;
	itemDescriptors.Reserve(mItemDescriptors.Num());

	for (UVcardTileItemDescriptor* itemDescriptor : mItemDescriptors)
	{
		if (IsValid(itemDescriptor))
		{
			itemDescriptors.Add(itemDescriptor);
		}
	}

	return itemDescriptors;
}

void UVcardTileViewWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindTileViewEvents();

	FString errorReason;
	RefreshTileView(errorReason);
}

void UVcardTileViewWidget::NativeDestruct()
{
	UnbindTileViewEvents();

	Super::NativeDestruct();
}

void UVcardTileViewWidget::BindTileViewEvents()
{
	if (mbTileViewEventsBound || !IsValid(TileView))
	{
		return;
	}

	TileView->OnItemClicked().AddUObject(this, &UVcardTileViewWidget::HandleTileItemClicked);
	TileView->OnItemSelectionChanged().AddUObject(this, &UVcardTileViewWidget::HandleTileItemSelectionChanged);
	TileView->OnItemIsHoveredChanged().AddUObject(this, &UVcardTileViewWidget::HandleTileItemHoveredChanged);
	mbTileViewEventsBound = true;
}

void UVcardTileViewWidget::UnbindTileViewEvents()
{
	if (!mbTileViewEventsBound || !IsValid(TileView))
	{
		mbTileViewEventsBound = false;
		return;
	}

	TileView->OnItemClicked().RemoveAll(this);
	TileView->OnItemSelectionChanged().RemoveAll(this);
	TileView->OnItemIsHoveredChanged().RemoveAll(this);
	mbTileViewEventsBound = false;
}

void UVcardTileViewWidget::HandleTileItemClicked(UObject* itemObject)
{
	UVcardTileItemDescriptor* itemDescriptor = Cast<UVcardTileItemDescriptor>(itemObject);
	SelectTileItem(itemDescriptor, true);
}

void UVcardTileViewWidget::HandleTileItemSelectionChanged(UObject* itemObject)
{
	UVcardTileItemDescriptor* itemDescriptor = Cast<UVcardTileItemDescriptor>(itemObject);
	if (IsValid(itemDescriptor) && ContainsTileItem(itemDescriptor))
	{
		mSelectedItem = itemDescriptor;
		UpdateRuntimeSelection(itemDescriptor);
	}
	else
	{
		mSelectedItem.Reset();
		UpdateRuntimeSelection(nullptr);
	}
}

void UVcardTileViewWidget::HandleTileItemHoveredChanged(UObject* itemObject, bool bIsHovered)
{
	UVcardTileItemDescriptor* itemDescriptor = Cast<UVcardTileItemDescriptor>(itemObject);
	if (!IsValid(itemDescriptor))
	{
		return;
	}

	itemDescriptor->SetRuntimeHovered(bIsHovered);
	OnTileItemHoveredChanged.Broadcast(itemDescriptor, itemDescriptor->GetItemId(), bIsHovered);
	BP_OnTileItemHoveredChanged(itemDescriptor, itemDescriptor->GetItemId(), bIsHovered);

	if (bIsHovered && bEmitSignalWhenTileHovered && !itemDescriptor->GetHoverSignalTag().IsNone())
	{
		RequestSignalForItem(itemDescriptor, itemDescriptor->GetHoverSignalTag());
	}
}

void UVcardTileViewWidget::UpdateRuntimeSelection(UVcardTileItemDescriptor* selectedItem)
{
	for (UVcardTileItemDescriptor* itemDescriptor : mItemDescriptors)
	{
		if (IsValid(itemDescriptor))
		{
			itemDescriptor->SetRuntimeSelected(itemDescriptor == selectedItem);
		}
	}
}

bool UVcardTileViewWidget::ContainsTileItem(UVcardTileItemDescriptor* itemDescriptor) const
{
	if (!IsValid(itemDescriptor))
	{
		return false;
	}

	for (UVcardTileItemDescriptor* candidate : mItemDescriptors)
	{
		if (candidate == itemDescriptor)
		{
			return true;
		}
	}

	return false;
}

void UVcardTileEntryWidget::SetTileItemDescriptor(UVcardTileItemDescriptor* itemDescriptor)
{
	mItemDescriptor = itemDescriptor;
	mbEntryHovered = IsValid(itemDescriptor) && itemDescriptor->IsRuntimeHovered();
	mbEntrySelected = IsValid(itemDescriptor) && itemDescriptor->IsRuntimeSelected();

	BP_OnTileItemDescriptorChanged(itemDescriptor);
	RefreshVisualState();
}

void UVcardTileEntryWidget::SetEntryHovered(bool bIsHovered)
{
	mbEntryHovered = bIsHovered;

	if (UVcardTileItemDescriptor* itemDescriptor = mItemDescriptor.Get())
	{
		itemDescriptor->SetRuntimeHovered(bIsHovered);
	}

	RefreshVisualState();
}

void UVcardTileEntryWidget::RefreshVisualState()
{
	UVcardTileItemDescriptor* itemDescriptor = mItemDescriptor.Get();
	const bool bSelected = mbEntrySelected || (IsValid(itemDescriptor) && itemDescriptor->IsRuntimeSelected());
	const bool bHovered = mbEntryHovered || (IsValid(itemDescriptor) && itemDescriptor->IsRuntimeHovered());
	const bool bShowHover = bHovered && (!bSelected || !bHideHoverLayerWhenSelected);

	SetOptionalLayerVisible(HoverBorderLayer, bShowHover);
	SetOptionalLayerVisible(SelectedBorderLayer, bSelected);

	FLinearColor targetTint = FLinearColor::White;
	if (IsValid(itemDescriptor))
	{
		targetTint = bSelected ? itemDescriptor->SelectedTint : (bHovered ? itemDescriptor->HoverTint : itemDescriptor->NormalTint);

		if (IsValid(Image_HoverBorderMaterial) && IsValid(itemDescriptor->HoverBorderMaterial))
		{
			Image_HoverBorderMaterial->SetBrushFromMaterial(itemDescriptor->HoverBorderMaterial);
		}

		if (IsValid(Image_SelectedBorderMaterial) && IsValid(itemDescriptor->SelectedBorderMaterial))
		{
			Image_SelectedBorderMaterial->SetBrushFromMaterial(itemDescriptor->SelectedBorderMaterial);
		}
	}

	if (IsValid(Image_Tint))
	{
		Image_Tint->SetColorAndOpacity(targetTint);
	}

	if (IsValid(Border_Tint))
	{
		Border_Tint->SetBrushColor(targetTint);
	}

	BP_OnTileVisualStateChanged(itemDescriptor, bHovered, bSelected);
}

void UVcardTileEntryWidget::NativeOnListItemObjectSet(UObject* listItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(listItemObject);
	SetTileItemDescriptor(Cast<UVcardTileItemDescriptor>(listItemObject));
}

void UVcardTileEntryWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);

	mbEntrySelected = bIsSelected;
	if (UVcardTileItemDescriptor* itemDescriptor = mItemDescriptor.Get())
	{
		itemDescriptor->SetRuntimeSelected(bIsSelected);
	}

	RefreshVisualState();
}

void UVcardTileEntryWidget::NativeOnEntryReleased()
{
	IUserObjectListEntry::NativeOnEntryReleased();

	SetEntryHovered(false);
	mbEntrySelected = false;
	mItemDescriptor.Reset();
	RefreshVisualState();
}

void UVcardTileEntryWidget::NativeOnMouseEnter(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent)
{
	Super::NativeOnMouseEnter(inGeometry, inMouseEvent);
	SetEntryHovered(true);
}

void UVcardTileEntryWidget::NativeOnMouseLeave(const FPointerEvent& inMouseEvent)
{
	Super::NativeOnMouseLeave(inMouseEvent);
	SetEntryHovered(false);
}

void UVcardTileEntryWidget::SetOptionalLayerVisible(UWidget* layerWidget, bool bVisible) const
{
	if (!IsValid(layerWidget))
	{
		return;
	}

	layerWidget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
