#include "VdjmWidgetMediaCarouselWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "VdjmRecordMediaPreview.h"

namespace
{
	FVector2D NormalizeOrFallback(FVector2D value, FVector2D fallback)
	{
		if (value.IsNearlyZero())
		{
			return fallback;
		}

		value.Normalize();
		return value;
	}

	int32 WrapIndex(int32 index, int32 count)
	{
		if (count <= 0)
		{
			return INDEX_NONE;
		}

		return (index % count + count) % count;
	}

	EVdjmWidgetMediaCarouselLayoutState ResolveLayoutState(int32 sourceCount, int32 visibleCount)
	{
		if (sourceCount <= 0)
		{
			return EVdjmWidgetMediaCarouselLayoutState::EEmpty;
		}

		if (sourceCount == 1)
		{
			return EVdjmWidgetMediaCarouselLayoutState::ESingle;
		}

		if (sourceCount == 2)
		{
			return EVdjmWidgetMediaCarouselLayoutState::EPair;
		}

		return sourceCount > visibleCount
			? EVdjmWidgetMediaCarouselLayoutState::EOverflow
			: EVdjmWidgetMediaCarouselLayoutState::EWindow;
	}
}

void UVdjmWidgetMediaCarouselSource::SetPreviewManager(AVdjmRecordMediaPreviewManagerActor* previewManager)
{
	mPreviewManager = previewManager;
}

bool UVdjmWidgetMediaCarouselSource::RefreshSourceSnapshot(
	TArray<FVdjmWidgetMediaCardSource>& outSources,
	FString& outErrorReason) const
{
	outSources.Reset();
	outErrorReason.Reset();

	AVdjmRecordMediaPreviewManagerActor* previewManager = mPreviewManager.Get();
	if (previewManager == nullptr)
	{
		outErrorReason = TEXT("Preview manager is not assigned.");
		return false;
	}

	const TArray<FVdjmRecordMediaRegistryEntry> registryEntries = previewManager->GetPreviewRegistryEntries();
	outSources.Reserve(registryEntries.Num());
	for (int32 sourceIndex = 0; sourceIndex < registryEntries.Num(); ++sourceIndex)
	{
		FVdjmWidgetMediaCardSource cardSource;
		cardSource.SourceRegistryIndex = sourceIndex;
		cardSource.RegistryEntry = registryEntries[sourceIndex];
		cardSource.bValid = true;
		outSources.Add(cardSource);
	}

	return true;
}

bool UVdjmWidgetMediaCarouselCardPool::EnsureCards(
	UUserWidget* ownerWidget,
	UPanelWidget* hostPanel,
	TSubclassOf<UVdjmWidgetMediaCardWidget> cardWidgetClass,
	int32 desiredCardCount,
	TArray<TObjectPtr<UVdjmWidgetMediaCardWidget>>& cards,
	FString& outErrorReason) const
{
	outErrorReason.Reset();
	if (ownerWidget == nullptr)
	{
		outErrorReason = TEXT("Owner widget is invalid.");
		return false;
	}

	if (hostPanel == nullptr)
	{
		outErrorReason = TEXT("CardHostPanel is invalid.");
		return false;
	}

	if (cardWidgetClass == nullptr)
	{
		outErrorReason = TEXT("CardWidgetClass is invalid.");
		return false;
	}

	const int32 safeDesiredCardCount = FMath::Max(0, desiredCardCount);
	while (cards.Num() < safeDesiredCardCount)
	{
		UVdjmWidgetMediaCardWidget* cardWidget = CreateWidget<UVdjmWidgetMediaCardWidget>(
			ownerWidget,
			cardWidgetClass);
		if (cardWidget == nullptr)
		{
			outErrorReason = TEXT("Failed to create media card widget.");
			return false;
		}

		hostPanel->AddChild(cardWidget);
		cards.Add(cardWidget);
	}

	for (int32 cardIndex = 0; cardIndex < cards.Num(); ++cardIndex)
	{
		if (cards[cardIndex] != nullptr)
		{
			cards[cardIndex]->SetVisibility(
				cardIndex < safeDesiredCardCount
					? ESlateVisibility::Visible
					: ESlateVisibility::Collapsed);
		}
	}

	return true;
}

EVdjmWidgetMediaCarouselLayoutState UVdjmWidgetMediaCarouselLayoutPolicy::BuildLayout(
	const TArray<FVdjmWidgetMediaCardSource>& sources,
	int32 activeSourceIndex,
	const FVdjmWidgetMediaCarouselLayoutOptions& layoutOptions,
	FVector2D viewSize,
	TArray<FVdjmWidgetMediaCarouselSlot>& outSlots) const
{
	outSlots.Reset();

	const int32 sourceCount = sources.Num();
	const int32 visibleCount = FMath::Clamp(layoutOptions.VisibleCardCount, 1, 32);
	const EVdjmWidgetMediaCarouselLayoutState layoutState = ResolveLayoutState(sourceCount, visibleCount);
	const int32 slotCount = sourceCount > 0
		? FMath::Min(sourceCount, visibleCount)
		: 1;
	const int32 activeSlotIndex = FMath::Clamp(slotCount / 2, 0, slotCount - 1);
	const int32 resolvedActiveSourceIndex = sourceCount > 0
		? FMath::Clamp(activeSourceIndex, 0, sourceCount - 1)
		: INDEX_NONE;
	const FVector2D direction = NormalizeOrFallback(layoutOptions.ProgressDirection, FVector2D(1.0f, 0.0f));
	const float axisExtent = FMath::Max(
		1.0f,
		FMath::Abs(direction.X) * FMath::Max(1.0f, viewSize.X) +
		FMath::Abs(direction.Y) * FMath::Max(1.0f, viewSize.Y));
	const float spacing = axisExtent * FMath::Clamp(layoutOptions.NormalizedSpacing, 0.0f, 1.0f);

	outSlots.Reserve(slotCount);
	for (int32 slotIndex = 0; slotIndex < slotCount; ++slotIndex)
	{
		const int32 sourceIndexCandidate = resolvedActiveSourceIndex - activeSlotIndex + slotIndex;
		const int32 sourceIndex = layoutOptions.bLoop
			? WrapIndex(sourceIndexCandidate, sourceCount)
			: sourceIndexCandidate;
		const bool bHasSource = sources.IsValidIndex(sourceIndex);
		const float slotOffset = static_cast<float>(slotIndex - activeSlotIndex);
		const float distance = FMath::Clamp(FMath::Abs(slotOffset), 0.0f, 4.0f);
		const float normalizedDistance = FMath::Clamp(distance / FMath::Max(1.0f, static_cast<float>(activeSlotIndex + 1)), 0.0f, 1.0f);

		FVdjmWidgetMediaCarouselSlot layoutSlot;
		layoutSlot.SlotIndex = slotIndex;
		layoutSlot.SourceIndex = bHasSource ? sourceIndex : INDEX_NONE;
		layoutSlot.SlotOffset = slotOffset;
		layoutSlot.TargetCardState = bHasSource
			? (FMath::IsNearlyZero(slotOffset) ? EVdjmWidgetMediaCardState::EActive : EVdjmWidgetMediaCardState::EVisible)
			: EVdjmWidgetMediaCardState::EEmpty;
		layoutSlot.RenderTranslation = direction * spacing * slotOffset;
		const float scale = FMath::Lerp(layoutOptions.ActiveScale, layoutOptions.FarScale, normalizedDistance);
		layoutSlot.RenderScale = FVector2D(scale, scale);
		layoutSlot.RenderOpacity = FMath::Lerp(layoutOptions.ActiveOpacity, layoutOptions.FarOpacity, normalizedDistance);
		layoutSlot.ZOrder = 100 - FMath::RoundToInt(distance * 10.0f);
		outSlots.Add(layoutSlot);
	}

	return layoutState;
}

EVdjmWidgetMediaCardState UVdjmWidgetMediaCarouselStatePolicy::ResolveCardState(
	const FVdjmWidgetMediaCarouselSlot& layoutSlot) const
{
	return layoutSlot.TargetCardState;
}

void UVdjmWidgetMediaCarouselInputController::ResetInput()
{
	mLastInputPayload = FVdjmWidgetMediaCarouselInputPayload();
}

void UVdjmWidgetMediaCarouselInputController::StoreInputPayload(
	const FVdjmWidgetMediaCarouselInputPayload& inputPayload)
{
	mLastInputPayload = inputPayload;
}

void UVdjmWidgetMediaCarouselMotionController::LockMotion(FName reason)
{
	mbMotionLocked = true;
	mMotionLockReason = reason;
}

void UVdjmWidgetMediaCarouselMotionController::UnlockMotion(FName reason)
{
	if (mMotionLockReason.IsNone() || mMotionLockReason == reason)
	{
		mbMotionLocked = false;
		mMotionLockReason = NAME_None;
	}
}

bool UVdjmWidgetMediaCarouselWidget::RefreshCarousel(FString& outErrorReason)
{
	outErrorReason.Reset();
	if (not EnsureControllers())
	{
		outErrorReason = TEXT("Failed to ensure carousel controllers.");
		OnCarouselRefreshFinished.Broadcast(false, outErrorReason);
		return false;
	}

	TArray<FVdjmWidgetMediaCardSource> sources;
	if (not mSource->RefreshSourceSnapshot(sources, outErrorReason))
	{
		OnCarouselRefreshFinished.Broadcast(false, outErrorReason);
		return false;
	}

	const bool bResult = ApplySourceSnapshot(sources, outErrorReason);
	OnCarouselRefreshFinished.Broadcast(bResult, outErrorReason);
	return bResult;
}

bool UVdjmWidgetMediaCarouselWidget::ApplySourceSnapshot(
	const TArray<FVdjmWidgetMediaCardSource>& sources,
	FString& outErrorReason)
{
	outErrorReason.Reset();
	if (not EnsureControllers())
	{
		outErrorReason = TEXT("Failed to ensure carousel controllers.");
		return false;
	}

	mSourceSnapshot = sources;
	if (mSourceSnapshot.Num() <= 0)
	{
		mActiveSourceIndex = INDEX_NONE;
	}
	else if (not mSourceSnapshot.IsValidIndex(mActiveSourceIndex))
	{
		mActiveSourceIndex = 0;
	}

	return RebuildVisibleCards(outErrorReason);
}

bool UVdjmWidgetMediaCarouselWidget::SetActiveSourceIndex(
	int32 activeSourceIndex,
	bool bRefreshLayout)
{
	if (not mSourceSnapshot.IsValidIndex(activeSourceIndex))
	{
		return false;
	}

	mActiveSourceIndex = activeSourceIndex;
	if (bRefreshLayout)
	{
		FString errorReason;
		return RebuildVisibleCards(errorReason);
	}

	return true;
}

void UVdjmWidgetMediaCarouselWidget::SetPreviewManager(
	AVdjmRecordMediaPreviewManagerActor* previewManager)
{
	EnsureControllers();
	if (mSource != nullptr)
	{
		mSource->SetPreviewManager(previewManager);
	}
}

TArray<UVdjmWidgetMediaCardWidget*> UVdjmWidgetMediaCarouselWidget::GetCards() const
{
	TArray<UVdjmWidgetMediaCardWidget*> cards;
	cards.Reserve(mCards.Num());
	for (UVdjmWidgetMediaCardWidget* card : mCards)
	{
		cards.Add(card);
	}
	return cards;
}

void UVdjmWidgetMediaCarouselWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureControllers();
}

bool UVdjmWidgetMediaCarouselWidget::EnsureControllers()
{
	if (mSource == nullptr)
	{
		mSource = NewObject<UVdjmWidgetMediaCarouselSource>(
			this,
			SourceClass != nullptr ? SourceClass.Get() : UVdjmWidgetMediaCarouselSource::StaticClass());
	}

	if (mCardPool == nullptr)
	{
		mCardPool = NewObject<UVdjmWidgetMediaCarouselCardPool>(
			this,
			CardPoolClass != nullptr ? CardPoolClass.Get() : UVdjmWidgetMediaCarouselCardPool::StaticClass());
	}

	if (mLayoutPolicy == nullptr)
	{
		mLayoutPolicy = NewObject<UVdjmWidgetMediaCarouselLayoutPolicy>(
			this,
			LayoutPolicyClass != nullptr ? LayoutPolicyClass.Get() : UVdjmWidgetMediaCarouselLayoutPolicy::StaticClass());
	}

	if (mStatePolicy == nullptr)
	{
		mStatePolicy = NewObject<UVdjmWidgetMediaCarouselStatePolicy>(
			this,
			StatePolicyClass != nullptr ? StatePolicyClass.Get() : UVdjmWidgetMediaCarouselStatePolicy::StaticClass());
	}

	if (mInputController == nullptr)
	{
		mInputController = NewObject<UVdjmWidgetMediaCarouselInputController>(
			this,
			InputControllerClass != nullptr ? InputControllerClass.Get() : UVdjmWidgetMediaCarouselInputController::StaticClass());
	}

	if (mMotionController == nullptr)
	{
		mMotionController = NewObject<UVdjmWidgetMediaCarouselMotionController>(
			this,
			MotionControllerClass != nullptr ? MotionControllerClass.Get() : UVdjmWidgetMediaCarouselMotionController::StaticClass());
	}

	return mSource != nullptr &&
		mCardPool != nullptr &&
		mLayoutPolicy != nullptr &&
		mStatePolicy != nullptr &&
		mInputController != nullptr &&
		mMotionController != nullptr;
}

bool UVdjmWidgetMediaCarouselWidget::RebuildVisibleCards(FString& outErrorReason)
{
	outErrorReason.Reset();
	TArray<FVdjmWidgetMediaCarouselSlot> layoutSlots;
	mLayoutState = mLayoutPolicy->BuildLayout(
		mSourceSnapshot,
		mActiveSourceIndex,
		LayoutOptions,
		GetCarouselViewSize(),
		layoutSlots);

	if (not mCardPool->EnsureCards(
		this,
		CardHostPanel,
		CardWidgetClass,
		layoutSlots.Num(),
		mCards,
		outErrorReason))
	{
		return false;
	}

	return ApplyLayoutSlots(layoutSlots, outErrorReason);
}

bool UVdjmWidgetMediaCarouselWidget::ApplyLayoutSlots(
	const TArray<FVdjmWidgetMediaCarouselSlot>& layoutSlots,
	FString& outErrorReason)
{
	outErrorReason.Reset();
	for (int32 slotIndex = 0; slotIndex < layoutSlots.Num(); ++slotIndex)
	{
		if (not mCards.IsValidIndex(slotIndex) || mCards[slotIndex] == nullptr)
		{
			outErrorReason = FString::Printf(TEXT("Card widget is invalid. Slot=%d"), slotIndex);
			return false;
		}

		UVdjmWidgetMediaCardWidget* cardWidget = mCards[slotIndex];
		const FVdjmWidgetMediaCarouselSlot& layoutSlot = layoutSlots[slotIndex];
		cardWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		cardWidget->SetRenderTranslation(layoutSlot.RenderTranslation);
		cardWidget->SetRenderScale(layoutSlot.RenderScale);
		cardWidget->SetRenderOpacity(layoutSlot.RenderOpacity);

		if (mSourceSnapshot.IsValidIndex(layoutSlot.SourceIndex))
		{
			cardWidget->SetCardSource(mSourceSnapshot[layoutSlot.SourceIndex]);
		}
		else
		{
			cardWidget->ClearCardSource();
		}

		cardWidget->SetCardState(mStatePolicy->ResolveCardState(layoutSlot));
	}

	return true;
}

FVector2D UVdjmWidgetMediaCarouselWidget::GetCarouselViewSize() const
{
	const FVector2D localSize = GetCachedGeometry().GetLocalSize();
	if (localSize.X > 1.0f && localSize.Y > 1.0f)
	{
		return localSize;
	}

	return FVector2D(1080.0f, 1920.0f);
}
