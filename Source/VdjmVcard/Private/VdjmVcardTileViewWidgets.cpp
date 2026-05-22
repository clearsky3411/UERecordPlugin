#include "VdjmVcardTileViewWidgets.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TileView.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "VdjmVcard.h"

void UVcardTileItemDataState::SetAssetTextureSource(TSoftObjectPtr<UTexture2D> sourceTexture, TSoftObjectPtr<UTexture2D> thumbnailTexture)
{
	ImageSourceType = EVcardTileImageSourceType::EAssetTexture;
	SourceTexture = sourceTexture;
	ThumbnailTexture = thumbnailTexture;
	LocalSourceImagePath.Reset();
	ClearLoadedImages();
}

void UVcardTileItemDataState::SetLocalImageFileSource(const FString& localSourceImagePath)
{
	ImageSourceType = EVcardTileImageSourceType::ELocalImageFile;
	LocalSourceImagePath = localSourceImagePath;
	SourceTexture.Reset();
	ThumbnailTexture.Reset();
	ClearLoadedImages();
}

void UVcardTileItemDataState::SetImageLoadState(
	EVcardTileImageLoadRequestType requestType,
	EVcardTileImageLoadState loadState,
	const FString& errorReason)
{
	LastLoadError = errorReason;

	if (requestType == EVcardTileImageLoadRequestType::EThumbnailOnly || requestType == EVcardTileImageLoadRequestType::EThumbnailAndSource)
	{
		ThumbnailLoadState = loadState;
	}

	if (requestType == EVcardTileImageLoadRequestType::ESourceOnly || requestType == EVcardTileImageLoadRequestType::EThumbnailAndSource)
	{
		SourceLoadState = loadState;
	}

	BroadcastImageLoadStateChanged(requestType, loadState);
}

void UVcardTileItemDataState::SetLoadedThumbnail(UTexture2D* loadedThumbnail)
{
	LoadedThumbnail = loadedThumbnail;
	LastLoadError.Reset();
	ThumbnailLoadState = IsValid(loadedThumbnail) ? EVcardTileImageLoadState::ELoaded : EVcardTileImageLoadState::EUnloaded;
	BroadcastImageLoadStateChanged(EVcardTileImageLoadRequestType::EThumbnailOnly, ThumbnailLoadState);
}

void UVcardTileItemDataState::SetLoadedSourceImage(UTexture2D* loadedSourceImage)
{
	LoadedSourceImage = loadedSourceImage;
	LastLoadError.Reset();
	SourceLoadState = IsValid(loadedSourceImage) ? EVcardTileImageLoadState::ELoaded : EVcardTileImageLoadState::EUnloaded;
	BroadcastImageLoadStateChanged(EVcardTileImageLoadRequestType::ESourceOnly, SourceLoadState);
}

void UVcardTileItemDataState::ClearLoadedImages()
{
	LoadedThumbnail = nullptr;
	LoadedSourceImage = nullptr;
	LastLoadError.Reset();
	ThumbnailLoadState = EVcardTileImageLoadState::EUnloaded;
	SourceLoadState = EVcardTileImageLoadState::EUnloaded;
	BroadcastImageLoadStateChanged(EVcardTileImageLoadRequestType::EThumbnailAndSource, EVcardTileImageLoadState::EUnloaded);
}

void UVcardTileItemDataState::SetRuntimeHovered(bool bIsHovered)
{
	mbRuntimeHovered = bIsHovered;
}

void UVcardTileItemDataState::SetRuntimeSelected(bool bIsSelected)
{
	mbRuntimeSelected = bIsSelected;
}

void UVcardTileItemDataState::BroadcastImageLoadStateChanged(EVcardTileImageLoadRequestType requestType, EVcardTileImageLoadState loadState)
{
	OnImageLoadStateChanged.Broadcast(this, requestType, loadState);
	BP_OnImageLoadStateChanged(requestType, loadState);
}

void UVcardTileViewWidget::SetTileItems(const TArray<UVcardTileItemDataState*>& itemDataStates)
{
	mItemDataStates.Reset();
	mItemDataStates.Reserve(itemDataStates.Num());

	for (UVcardTileItemDataState* itemDataState : itemDataStates)
	{
		if (IsValid(itemDataState))
		{
			itemDataState->SetRuntimeHovered(false);
			itemDataState->SetRuntimeSelected(false);
			mItemDataStates.Add(itemDataState);
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

void UVcardTileViewWidget::AddTileItem(UVcardTileItemDataState* itemDataState)
{
	if (!IsValid(itemDataState) || ContainsTileItem(itemDataState))
	{
		return;
	}

	itemDataState->SetRuntimeHovered(false);
	itemDataState->SetRuntimeSelected(false);
	mItemDataStates.Add(itemDataState);

	FString errorReason;
	RefreshTileView(errorReason);
	BP_OnTileItemsChanged(GetTileItems());
}

void UVcardTileViewWidget::ClearTileItems()
{
	for (UVcardTileItemDataState* itemDataState : mItemDataStates)
	{
		if (IsValid(itemDataState))
		{
			itemDataState->SetRuntimeHovered(false);
			itemDataState->SetRuntimeSelected(false);
		}
	}

	mItemDataStates.Reset();
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
	listItems.Reserve(mItemDataStates.Num());
	for (UVcardTileItemDataState* itemDataState : mItemDataStates)
	{
		if (IsValid(itemDataState))
		{
			listItems.Add(itemDataState);
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

bool UVcardTileViewWidget::SelectTileItem(UVcardTileItemDataState* itemDataState, bool bEmitSignalRequest)
{
	if (!IsValid(itemDataState) || !ContainsTileItem(itemDataState))
	{
		return false;
	}

	mSelectedItem = itemDataState;
	UpdateRuntimeSelection(itemDataState);

	if (IsValid(TileView))
	{
		TileView->SetSelectedItem(itemDataState);
	}

	OnTileItemSelected.Broadcast(itemDataState, itemDataState->GetItemId());
	BP_OnTileItemSelected(itemDataState, itemDataState->GetItemId());

	if (bEmitSignalRequest && bEmitSignalWhenTileClicked && !itemDataState->GetSelectSignalTag().IsNone())
	{
		RequestSignalForItem(itemDataState, itemDataState->GetSelectSignalTag());
	}

	return true;
}

bool UVcardTileViewWidget::SelectTileItemById(FName itemId, bool bEmitSignalRequest)
{
	for (UVcardTileItemDataState* itemDataState : mItemDataStates)
	{
		if (IsValid(itemDataState) && itemDataState->GetItemId() == itemId)
		{
			return SelectTileItem(itemDataState, bEmitSignalRequest);
		}
	}

	return false;
}

bool UVcardTileViewWidget::RequestSignalForItem(UVcardTileItemDataState* itemDataState, FName signalTag)
{
	if (!IsValid(itemDataState) || signalTag.IsNone())
	{
		return false;
	}

	OnTileSignalRequested.Broadcast(signalTag, itemDataState);
	BP_OnTileSignalRequested(signalTag, itemDataState);
	return true;
}

TArray<UVcardTileItemDataState*> UVcardTileViewWidget::GetTileItems() const
{
	TArray<UVcardTileItemDataState*> itemDataStates;
	itemDataStates.Reserve(mItemDataStates.Num());

	for (UVcardTileItemDataState* itemDataState : mItemDataStates)
	{
		if (IsValid(itemDataState))
		{
			itemDataStates.Add(itemDataState);
		}
	}

	return itemDataStates;
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
	UVcardTileItemDataState* itemDataState = Cast<UVcardTileItemDataState>(itemObject);
	SelectTileItem(itemDataState, true);
}

void UVcardTileViewWidget::HandleTileItemSelectionChanged(UObject* itemObject)
{
	UVcardTileItemDataState* itemDataState = Cast<UVcardTileItemDataState>(itemObject);
	if (IsValid(itemDataState) && ContainsTileItem(itemDataState))
	{
		mSelectedItem = itemDataState;
		UpdateRuntimeSelection(itemDataState);
	}
	else
	{
		mSelectedItem.Reset();
		UpdateRuntimeSelection(nullptr);
	}
}

void UVcardTileViewWidget::HandleTileItemHoveredChanged(UObject* itemObject, bool bIsHovered)
{
	UVcardTileItemDataState* itemDataState = Cast<UVcardTileItemDataState>(itemObject);
	if (!IsValid(itemDataState))
	{
		return;
	}

	itemDataState->SetRuntimeHovered(bIsHovered);
	OnTileItemHoveredChanged.Broadcast(itemDataState, itemDataState->GetItemId(), bIsHovered);
	BP_OnTileItemHoveredChanged(itemDataState, itemDataState->GetItemId(), bIsHovered);

	if (bIsHovered && bEmitSignalWhenTileHovered && !itemDataState->GetHoverSignalTag().IsNone())
	{
		RequestSignalForItem(itemDataState, itemDataState->GetHoverSignalTag());
	}
}

void UVcardTileViewWidget::UpdateRuntimeSelection(UVcardTileItemDataState* selectedItem)
{
	for (UVcardTileItemDataState* itemDataState : mItemDataStates)
	{
		if (IsValid(itemDataState))
		{
			itemDataState->SetRuntimeSelected(itemDataState == selectedItem);
		}
	}
}

bool UVcardTileViewWidget::ContainsTileItem(UVcardTileItemDataState* itemDataState) const
{
	if (!IsValid(itemDataState))
	{
		return false;
	}

	for (UVcardTileItemDataState* candidate : mItemDataStates)
	{
		if (candidate == itemDataState)
		{
			return true;
		}
	}

	return false;
}

void UVcardTileEntryWidget::SetTileItemDataState(UVcardTileItemDataState* itemDataState)
{
	UnbindTileItemState();
	BindTileItemState(itemDataState);
	mbEntryHovered = IsValid(itemDataState) && itemDataState->IsRuntimeHovered();
	mbEntrySelected = IsValid(itemDataState) && itemDataState->IsRuntimeSelected();

	BP_OnTileItemDataStateChanged(itemDataState);
	RefreshVisualState();
}

void UVcardTileEntryWidget::SetEntryHovered(bool bIsHovered)
{
	mbEntryHovered = bIsHovered;

	if (UVcardTileItemDataState* itemDataState = mItemDataState.Get())
	{
		itemDataState->SetRuntimeHovered(bIsHovered);
	}

	RefreshVisualState();
}

void UVcardTileEntryWidget::RefreshVisualState()
{
	UVcardTileItemDataState* itemDataState = mItemDataState.Get();
	const bool bSelected = mbEntrySelected || (IsValid(itemDataState) && itemDataState->IsRuntimeSelected());
	const bool bHovered = mbEntryHovered || (IsValid(itemDataState) && itemDataState->IsRuntimeHovered());
	const bool bShowHover = bHovered && (!bSelected || !bHideHoverLayerWhenSelected);

	SetOptionalLayerVisible(HoverBorderLayer, bShowHover);
	SetOptionalLayerVisible(SelectedBorderLayer, bSelected);

	FLinearColor targetTint = FLinearColor::White;
	if (IsValid(itemDataState))
	{
		targetTint = bSelected ? itemDataState->SelectedTint : (bHovered ? itemDataState->HoverTint : itemDataState->NormalTint);

		if (IsValid(Image_HoverBorderMaterial) && IsValid(itemDataState->HoverBorderMaterial))
		{
			Image_HoverBorderMaterial->SetBrushFromMaterial(itemDataState->HoverBorderMaterial);
		}

		if (IsValid(Image_SelectedBorderMaterial) && IsValid(itemDataState->SelectedBorderMaterial))
		{
			Image_SelectedBorderMaterial->SetBrushFromMaterial(itemDataState->SelectedBorderMaterial);
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

	BP_OnTileVisualStateChanged(itemDataState, bHovered, bSelected);
}

void UVcardTileEntryWidget::NativeOnListItemObjectSet(UObject* listItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(listItemObject);
	SetTileItemDataState(Cast<UVcardTileItemDataState>(listItemObject));
}

void UVcardTileEntryWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);

	mbEntrySelected = bIsSelected;
	if (UVcardTileItemDataState* itemDataState = mItemDataState.Get())
	{
		itemDataState->SetRuntimeSelected(bIsSelected);
	}

	RefreshVisualState();
}

void UVcardTileEntryWidget::NativeOnEntryReleased()
{
	IUserObjectListEntry::NativeOnEntryReleased();

	SetEntryHovered(false);
	mbEntrySelected = false;
	UnbindTileItemState();
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

void UVcardTileEntryWidget::HandleTileItemImageLoadStateChanged(
	UVcardTileItemDataState* itemDataState,
	EVcardTileImageLoadRequestType requestType,
	EVcardTileImageLoadState loadState)
{
	if (itemDataState != mItemDataState.Get())
	{
		return;
	}

	BP_OnTileImageLoadStateChanged(itemDataState, requestType, loadState);
}

void UVcardTileEntryWidget::SetOptionalLayerVisible(UWidget* layerWidget, bool bVisible) const
{
	if (!IsValid(layerWidget))
	{
		return;
	}

	layerWidget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UVcardTileEntryWidget::BindTileItemState(UVcardTileItemDataState* itemDataState)
{
	mItemDataState = itemDataState;
	if (IsValid(itemDataState))
	{
		itemDataState->OnImageLoadStateChanged.AddDynamic(this, &UVcardTileEntryWidget::HandleTileItemImageLoadStateChanged);
	}
}

void UVcardTileEntryWidget::UnbindTileItemState()
{
	if (UVcardTileItemDataState* itemDataState = mItemDataState.Get())
	{
		itemDataState->OnImageLoadStateChanged.RemoveAll(this);
	}

	mItemDataState.Reset();
}
