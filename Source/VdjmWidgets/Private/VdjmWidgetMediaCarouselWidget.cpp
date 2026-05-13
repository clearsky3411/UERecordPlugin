#include "VdjmWidgetMediaCarouselWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "Components/OverlaySlot.h"
#include "VdjmRecordMediaPreview.h"
#include "VdjmWidgets.h"

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

	int32 FindSourceIndexByRecordId(
		const TArray<FVdjmWidgetMediaCardSource>& sources,
		const FString& recordId)
	{
		if (recordId.IsEmpty())
		{
			return INDEX_NONE;
		}

		for (int32 sourceIndex = 0; sourceIndex < sources.Num(); ++sourceIndex)
		{
			if (sources[sourceIndex].RegistryEntry.RecordId == recordId)
			{
				return sourceIndex;
			}
		}

		return INDEX_NONE;
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

	template <typename EnumType>
	FString GetEnumValueString(EnumType value)
	{
		const UEnum* enumObject = StaticEnum<EnumType>();
		if (enumObject == nullptr)
		{
			return FString::FromInt(static_cast<int32>(value));
		}

		return enumObject->GetValueAsString(value);
	}

	const TCHAR* GetVisibilityString(ESlateVisibility visibility)
	{
		switch (visibility)
		{
		case ESlateVisibility::Visible:
			return TEXT("Visible");
		case ESlateVisibility::Collapsed:
			return TEXT("Collapsed");
		case ESlateVisibility::Hidden:
			return TEXT("Hidden");
		case ESlateVisibility::HitTestInvisible:
			return TEXT("HitTestInvisible");
		case ESlateVisibility::SelfHitTestInvisible:
			return TEXT("SelfHitTestInvisible");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetBoolString(bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	constexpr int32 ActivePreviewMaxStartAttempts = 5;
	constexpr float ActivePreviewInitialDelaySeconds = 0.10f;
	constexpr float ActivePreviewRetryDelaySeconds = 0.35f;
	constexpr float ActivePreviewVerifyDelaySeconds = 0.45f;
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

		if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(hostPanel->AddChild(cardWidget)))
		{
			overlaySlot->SetHorizontalAlignment(HAlign_Fill);
			overlaySlot->SetVerticalAlignment(VAlign_Fill);
			overlaySlot->SetPadding(FMargin(0.0f));
		}
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
	const int32 slotCount = visibleCount;
	const int32 activeSlotIndex = layoutOptions.ActiveSlotIndex != INDEX_NONE
		? FMath::Clamp(layoutOptions.ActiveSlotIndex, 0, slotCount - 1)
		: FMath::Clamp(slotCount / 2, 0, slotCount - 1);
	const int32 resolvedActiveSourceIndex = sourceCount > 0
		? FMath::Clamp(activeSourceIndex, 0, sourceCount - 1)
		: INDEX_NONE;
	const bool bCanLoop = layoutOptions.bLoop && sourceCount >= visibleCount && sourceCount > 1;
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
		const int32 sourceIndex = bCanLoop
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
			? (slotIndex == activeSlotIndex ? EVdjmWidgetMediaCardState::EWaiting : EVdjmWidgetMediaCardState::EVisible)
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
	if (bDebugTraceLayout)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel Refresh Begin Widget=%s Host=%s CardClass=%s Active=%d SourceCount=%d CardCount=%d ViewSize=%s Visibility=%s"),
			*GetNameSafe(this),
			*GetNameSafe(CardHostPanel),
			*GetNameSafe(CardWidgetClass.Get()),
			mActiveSourceIndex,
			mSourceSnapshot.Num(),
			mCards.Num(),
			*GetCarouselViewSize().ToString(),
			GetVisibilityString(GetVisibility()));
	}

	if (not EnsureControllers())
	{
		outErrorReason = TEXT("Failed to ensure carousel controllers.");
		if (bDebugTraceLayout)
		{
			UE_LOG(LogVdjmWidgets, Warning, TEXT("VdjmCarousel Refresh Failed Reason=%s"), *outErrorReason);
		}
		OnCarouselRefreshFinished.Broadcast(false, outErrorReason);
		return false;
	}

	if (mSource->GetPreviewManager() == nullptr)
	{
		mSource->SetPreviewManager(AVdjmRecordMediaPreviewManagerActor::FindMediaPreviewManagerActor(this));
		if (bDebugTraceLayout)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel Refresh ResolveManager Manager=%s"),
				*GetNameSafe(mSource->GetPreviewManager()));
		}
	}

	AVdjmRecordMediaPreviewManagerActor* previewManager = mSource->GetPreviewManager();
	if (bRefreshPreviewStoreOnRefresh && previewManager != nullptr)
	{
		FString refreshStoreErrorReason;
		if (not previewManager->RefreshPreviewStoreFromDisk(refreshStoreErrorReason))
		{
			outErrorReason = refreshStoreErrorReason.IsEmpty()
				? TEXT("Failed to refresh preview store from disk.")
				: refreshStoreErrorReason;
			if (bKeepPreviousSnapshotOnRefreshFailure && mSourceSnapshot.Num() > 0)
			{
				FString rebuildErrorReason;
				RebuildVisibleCards(rebuildErrorReason);
				if (bDebugTraceLayout)
				{
					UE_LOG(
						LogVdjmWidgets,
						Warning,
						TEXT("VdjmCarousel RefreshStore Failed KeepPrevious=true Reason=%s RebuildReason=%s"),
						*outErrorReason,
						*rebuildErrorReason);
				}
			}
			else if (bDebugTraceLayout)
			{
				UE_LOG(
					LogVdjmWidgets,
					Warning,
					TEXT("VdjmCarousel RefreshStore Failed KeepPrevious=false Reason=%s"),
					*outErrorReason);
			}
			OnCarouselRefreshFinished.Broadcast(false, outErrorReason);
			return false;
		}

		if (bDebugTraceLayout)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel RefreshStore Success Manager=%s RegistryCount=%d"),
				*GetNameSafe(previewManager),
				previewManager->GetPreviewRegistryEntries().Num());
		}
	}

	TArray<FVdjmWidgetMediaCardSource> sources;
	if (not mSource->RefreshSourceSnapshot(sources, outErrorReason))
	{
		if (bKeepPreviousSnapshotOnRefreshFailure && mSourceSnapshot.Num() > 0)
		{
			FString rebuildErrorReason;
			RebuildVisibleCards(rebuildErrorReason);
			if (bDebugTraceLayout)
			{
				UE_LOG(
					LogVdjmWidgets,
					Warning,
					TEXT("VdjmCarousel RefreshSnapshot Failed KeepPrevious=true Reason=%s RebuildReason=%s"),
					*outErrorReason,
					*rebuildErrorReason);
			}
		}
		if (bDebugTraceLayout)
		{
			UE_LOG(LogVdjmWidgets, Warning, TEXT("VdjmCarousel Refresh Failed Reason=%s"), *outErrorReason);
		}
		OnCarouselRefreshFinished.Broadcast(false, outErrorReason);
		return false;
	}

	if (bDebugTraceLayout)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel Refresh Snapshot SourceCount=%d Manager=%s"),
			sources.Num(),
			*GetNameSafe(mSource->GetPreviewManager()));
	}

	const bool bResult = ApplySourceSnapshot(sources, outErrorReason);
	if (bDebugTraceLayout)
	{
		if (bResult)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel Refresh Complete Success=true Reason=%s SourceCount=%d CardCount=%d Active=%d Layout=%s"),
				*outErrorReason,
				mSourceSnapshot.Num(),
				mCards.Num(),
				mActiveSourceIndex,
				*GetEnumValueString(mLayoutState));
		}
		else
		{
			UE_LOG(
				LogVdjmWidgets,
				Warning,
				TEXT("VdjmCarousel Refresh Complete Success=false Reason=%s SourceCount=%d CardCount=%d Active=%d Layout=%s"),
				*outErrorReason,
				mSourceSnapshot.Num(),
				mCards.Num(),
				mActiveSourceIndex,
				*GetEnumValueString(mLayoutState));
		}
	}
	OnCarouselRefreshFinished.Broadcast(bResult, outErrorReason);
	return bResult;
}

bool UVdjmWidgetMediaCarouselWidget::ApplySourceSnapshot(
	const TArray<FVdjmWidgetMediaCardSource>& sources,
	FString& outErrorReason)
{
	outErrorReason.Reset();
	if (bDebugTraceLayout)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel ApplySourceSnapshot Begin IncomingSources=%d PreviousSources=%d Active=%d"),
			sources.Num(),
			mSourceSnapshot.Num(),
			mActiveSourceIndex);
	}

	if (not EnsureControllers())
	{
		outErrorReason = TEXT("Failed to ensure carousel controllers.");
		return false;
	}

	const int32 previousSourceIndex = mActiveSourceIndex;
	const FString previousRecordId = mSourceSnapshot.IsValidIndex(mActiveSourceIndex)
		? mSourceSnapshot[mActiveSourceIndex].RegistryEntry.RecordId
		: FString();

	mSourceSnapshot = sources;
	mActiveSourceIndex = ResolveActiveSourceIndexAfterRefresh(
		mSourceSnapshot,
		previousSourceIndex,
		previousRecordId);
	if (previousSourceIndex != mActiveSourceIndex)
	{
		OnActiveSourceChanged.Broadcast(previousSourceIndex, mActiveSourceIndex);
	}

	if (bDebugTraceLayout)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel ApplySourceSnapshot Resolved SourceCount=%d PreviousActive=%d Active=%d PreviousRecordId=%s Policy=%s"),
			mSourceSnapshot.Num(),
			previousSourceIndex,
			mActiveSourceIndex,
			previousRecordId.IsEmpty() ? TEXT("None") : *previousRecordId,
			*GetEnumValueString(ActiveAfterRefreshPolicy));
	}

	return RebuildVisibleCards(outErrorReason);
}

bool UVdjmWidgetMediaCarouselWidget::SetActiveSourceIndex(
	int32 activeSourceIndex,
	bool bRefreshLayout)
{
	return ApplyResolvedActiveSourceIndex(activeSourceIndex, bRefreshLayout);
}

bool UVdjmWidgetMediaCarouselWidget::SlideNext()
{
	return ApplyResolvedActiveSourceIndex(mActiveSourceIndex + 1, true);
}

bool UVdjmWidgetMediaCarouselWidget::SlidePrevious()
{
	return ApplyResolvedActiveSourceIndex(mActiveSourceIndex - 1, true);
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

bool UVdjmWidgetMediaCarouselWidget::StartActiveCardPreview(FString& outErrorReason)
{
	outErrorReason.Reset();

	UVdjmWidgetMediaCardWidget* activeCard = FindActiveCard();
	if (activeCard == nullptr)
	{
		outErrorReason = TEXT("Active card is not assigned yet.");
		return false;
	}

	if (not activeCard->HasValidCardSource())
	{
		outErrorReason = TEXT("Active card source is invalid.");
		return false;
	}

	if (not IsPreviewGeometryReady(this) || not IsPreviewGeometryReady(activeCard))
	{
		outErrorReason = TEXT("Active preview geometry is not ready.");
		return false;
	}

	return activeCard->TryStartActivePreviewLoop(outErrorReason);
}

void UVdjmWidgetMediaCarouselWidget::StopAllCardPreviews(bool bReleaseMediaResources)
{
	CancelActivePreviewStart();
	for (UVdjmWidgetMediaCardWidget* cardWidget : mCards)
	{
		if (cardWidget != nullptr)
		{
			cardWidget->StopPreview(bReleaseMediaResources);
		}
	}
}

FVdjmWidgetMediaCarouselInputPayload UVdjmWidgetMediaCarouselWidget::GetLastInputPayload() const
{
	return mInputController != nullptr
		? mInputController->GetLastInputPayload()
		: FVdjmWidgetMediaCarouselInputPayload();
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
	if (bDebugTraceLayout || bDebugTraceInput)
	{
		DumpDebugCarouselState(TEXT("Construct"));
	}
}

void UVdjmWidgetMediaCarouselWidget::NativeDestruct()
{
	StopAllCardPreviews(true);
	Super::NativeDestruct();
}

void UVdjmWidgetMediaCarouselWidget::RequestActivePreviewStart(FName reason)
{
	CancelActivePreviewStart();
	mActivePreviewStartAttemptCount = 0;
	mActivePreviewStartReason = reason;

	if (not mSourceSnapshot.IsValidIndex(mActiveSourceIndex))
	{
		if (bDebugTraceLayout)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel ActivePreview RequestIgnored Reason=%s Active=%d SourceCount=%d"),
				*reason.ToString(),
				mActiveSourceIndex,
				mSourceSnapshot.Num());
		}
		return;
	}

	if (bDebugTraceLayout)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel ActivePreview Request Reason=%s Active=%d SourceCount=%d"),
			*reason.ToString(),
			mActiveSourceIndex,
			mSourceSnapshot.Num());
	}
	ScheduleActivePreviewStart(ActivePreviewInitialDelaySeconds);
}

void UVdjmWidgetMediaCarouselWidget::ScheduleActivePreviewStart(float delaySeconds)
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(mActivePreviewStartTimerHandle);
		world->GetTimerManager().SetTimer(
			mActivePreviewStartTimerHandle,
			this,
			&UVdjmWidgetMediaCarouselWidget::HandleActivePreviewStartTimer,
			FMath::Max(0.01f, delaySeconds),
			false);
		return;
	}

	HandleActivePreviewStartTimer();
}

void UVdjmWidgetMediaCarouselWidget::ScheduleActivePreviewVerify()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(mActivePreviewVerifyTimerHandle);
		world->GetTimerManager().SetTimer(
			mActivePreviewVerifyTimerHandle,
			this,
			&UVdjmWidgetMediaCarouselWidget::HandleActivePreviewVerifyTimer,
			ActivePreviewVerifyDelaySeconds,
			false);
		return;
	}

	HandleActivePreviewVerifyTimer();
}

void UVdjmWidgetMediaCarouselWidget::CancelActivePreviewStart()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(mActivePreviewStartTimerHandle);
		world->GetTimerManager().ClearTimer(mActivePreviewVerifyTimerHandle);
	}
}

void UVdjmWidgetMediaCarouselWidget::HandleActivePreviewStartTimer()
{
	++mActivePreviewStartAttemptCount;

	FString errorReason;
	const bool bStarted = StartActiveCardPreview(errorReason);
	if (bDebugTraceLayout)
	{
		if (bStarted)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel ActivePreview StartAttempt Reason=%s Attempt=%d/%d Active=%d Result=true Error=%s"),
				*mActivePreviewStartReason.ToString(),
				mActivePreviewStartAttemptCount,
				ActivePreviewMaxStartAttempts,
				mActiveSourceIndex,
				errorReason.IsEmpty() ? TEXT("None") : *errorReason);
		}
		else
		{
			UE_LOG(
				LogVdjmWidgets,
				Warning,
				TEXT("VdjmCarousel ActivePreview StartAttempt Reason=%s Attempt=%d/%d Active=%d Result=false Error=%s"),
				*mActivePreviewStartReason.ToString(),
				mActivePreviewStartAttemptCount,
				ActivePreviewMaxStartAttempts,
				mActiveSourceIndex,
				errorReason.IsEmpty() ? TEXT("None") : *errorReason);
		}
	}

	if (bStarted)
	{
		ScheduleActivePreviewVerify();
		return;
	}

	if (CanRetryActivePreviewStart())
	{
		ScheduleActivePreviewStart(ActivePreviewRetryDelaySeconds);
	}
}

void UVdjmWidgetMediaCarouselWidget::HandleActivePreviewVerifyTimer()
{
	UVdjmWidgetMediaCardWidget* activeCard = FindActiveCard();
	if (activeCard == nullptr)
	{
		if (CanRetryActivePreviewStart())
		{
			ScheduleActivePreviewStart(ActivePreviewRetryDelaySeconds);
		}
		return;
	}

	const bool bAutoManaged = activeCard->IsAutoManagingPreviewMedia();
	const bool bPreviewOpened = activeCard->IsManagedPreviewOpened();
	const bool bPlaybackHealthy = activeCard->IsManagedPreviewPlaybackHealthy();
	const bool bPlaybackPending = activeCard->IsManagedPreviewPlaybackPending();
	const bool bPlaybackStalled = activeCard->IsManagedPreviewPlaybackStalled();
	if (not bAutoManaged || bPlaybackHealthy)
	{
		if (activeCard->GetCardState() != EVdjmWidgetMediaCardState::EActive)
		{
			activeCard->SetCardState(EVdjmWidgetMediaCardState::EActive);
		}

		if (bDebugTraceLayout)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel ActivePreview Ready Reason=%s Attempt=%d Active=%d Card=%s Opened=%s Healthy=%s AutoManaged=%s"),
				*mActivePreviewStartReason.ToString(),
				mActivePreviewStartAttemptCount,
				mActiveSourceIndex,
				*GetNameSafe(activeCard),
				bPreviewOpened ? TEXT("true") : TEXT("false"),
				bPlaybackHealthy ? TEXT("true") : TEXT("false"),
				bAutoManaged ? TEXT("true") : TEXT("false"));
		}
		return;
	}

	if (bPlaybackPending)
	{
		if (bDebugTraceLayout)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel ActivePreview Pending Reason=%s Attempt=%d/%d Active=%d Card=%s Opened=%s Stalled=%s"),
				*mActivePreviewStartReason.ToString(),
				mActivePreviewStartAttemptCount,
				ActivePreviewMaxStartAttempts,
				mActiveSourceIndex,
				*GetNameSafe(activeCard),
				bPreviewOpened ? TEXT("true") : TEXT("false"),
				bPlaybackStalled ? TEXT("true") : TEXT("false"));
		}
		ScheduleActivePreviewVerify();
		return;
	}

	const FString lastErrorReason = activeCard->GetManagedPreviewLastErrorReason();
	if (bDebugTraceLayout)
	{
		UE_LOG(
			LogVdjmWidgets,
			Warning,
			TEXT("VdjmCarousel ActivePreview VerifyFailed Reason=%s Attempt=%d/%d Active=%d Card=%s PreviewActive=%s Opened=%s Healthy=%s Stalled=%s Error=%s"),
			*mActivePreviewStartReason.ToString(),
			mActivePreviewStartAttemptCount,
			ActivePreviewMaxStartAttempts,
			mActiveSourceIndex,
			*GetNameSafe(activeCard),
			activeCard->IsManagedPreviewActive() ? TEXT("true") : TEXT("false"),
			bPreviewOpened ? TEXT("true") : TEXT("false"),
			bPlaybackHealthy ? TEXT("true") : TEXT("false"),
			bPlaybackStalled ? TEXT("true") : TEXT("false"),
			lastErrorReason.IsEmpty() ? TEXT("None") : *lastErrorReason);
	}

	if (CanRetryActivePreviewStart())
	{
		activeCard->StopPreview(true);
		if (activeCard->GetCardState() != EVdjmWidgetMediaCardState::EWaiting)
		{
			activeCard->SetCardState(EVdjmWidgetMediaCardState::EWaiting);
		}
		ScheduleActivePreviewStart(ActivePreviewRetryDelaySeconds);
	}
}

bool UVdjmWidgetMediaCarouselWidget::CanRetryActivePreviewStart() const
{
	return mActivePreviewStartAttemptCount < ActivePreviewMaxStartAttempts &&
		mSourceSnapshot.IsValidIndex(mActiveSourceIndex);
}

bool UVdjmWidgetMediaCarouselWidget::IsPreviewGeometryReady(const UWidget* widget) const
{
	if (widget == nullptr)
	{
		return false;
	}

	const FVector2D localSize = widget->GetCachedGeometry().GetLocalSize();
	return localSize.X > 1.0f && localSize.Y > 1.0f;
}

UVdjmWidgetMediaCardWidget* UVdjmWidgetMediaCarouselWidget::FindActiveCard() const
{
	for (UVdjmWidgetMediaCardWidget* cardWidget : mCards)
	{
		if (cardWidget == nullptr)
		{
			continue;
		}

		if (cardWidget->GetCardSource().SourceRegistryIndex == mActiveSourceIndex)
		{
			return cardWidget;
		}
	}

	return nullptr;
}

FReply UVdjmWidgetMediaCarouselWidget::NativeOnMouseButtonDown(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	BeginDebugPointerTrace(inGeometry, inMouseEvent, TEXT("Mouse"));
	return BuildDebugInputReply();
}

FReply UVdjmWidgetMediaCarouselWidget::NativeOnMouseButtonUp(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	EndDebugPointerTrace(inGeometry, inMouseEvent, TEXT("Mouse"));
	return BuildDebugInputReply();
}

FReply UVdjmWidgetMediaCarouselWidget::NativeOnMouseMove(
	const FGeometry& inGeometry,
	const FPointerEvent& inMouseEvent)
{
	UpdateDebugPointerTrace(inGeometry, inMouseEvent, TEXT("Mouse"));
	return mbDebugPointerTracking ? BuildDebugInputReply() : Super::NativeOnMouseMove(inGeometry, inMouseEvent);
}

FReply UVdjmWidgetMediaCarouselWidget::NativeOnTouchStarted(
	const FGeometry& inGeometry,
	const FPointerEvent& inGestureEvent)
{
	BeginDebugPointerTrace(inGeometry, inGestureEvent, TEXT("Touch"));
	return BuildDebugInputReply();
}

FReply UVdjmWidgetMediaCarouselWidget::NativeOnTouchMoved(
	const FGeometry& inGeometry,
	const FPointerEvent& inGestureEvent)
{
	UpdateDebugPointerTrace(inGeometry, inGestureEvent, TEXT("Touch"));
	return mbDebugPointerTracking ? BuildDebugInputReply() : Super::NativeOnTouchMoved(inGeometry, inGestureEvent);
}

FReply UVdjmWidgetMediaCarouselWidget::NativeOnTouchEnded(
	const FGeometry& inGeometry,
	const FPointerEvent& inGestureEvent)
{
	EndDebugPointerTrace(inGeometry, inGestureEvent, TEXT("Touch"));
	return BuildDebugInputReply();
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

	if (bDebugTraceLayout)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel RebuildVisibleCards Layout=%s SourceCount=%d LayoutSlots=%d Host=%s HostChildren=%d CardClass=%s ViewSize=%s"),
			*GetEnumValueString(mLayoutState),
			mSourceSnapshot.Num(),
			layoutSlots.Num(),
			*GetNameSafe(CardHostPanel),
			CardHostPanel != nullptr ? CardHostPanel->GetChildrenCount() : -1,
			*GetNameSafe(CardWidgetClass.Get()),
			*GetCarouselViewSize().ToString());
	}

	if (not mCardPool->EnsureCards(
		this,
		CardHostPanel,
		CardWidgetClass,
		layoutSlots.Num(),
		mCards,
		outErrorReason))
	{
		if (bDebugTraceLayout)
		{
			UE_LOG(LogVdjmWidgets, Warning, TEXT("VdjmCarousel RebuildVisibleCards Failed Reason=%s"), *outErrorReason);
		}
		return false;
	}

	const bool bApplied = ApplyLayoutSlots(layoutSlots, outErrorReason);
	if (bApplied)
	{
		RequestActivePreviewStart(TEXT("RebuildVisibleCards"));
	}
	return bApplied;
}

bool UVdjmWidgetMediaCarouselWidget::ApplyLayoutSlots(
	const TArray<FVdjmWidgetMediaCarouselSlot>& layoutSlots,
	FString& outErrorReason)
{
	outErrorReason.Reset();
	mLayoutSlots = layoutSlots;
	ApplySlotTransforms(mLayoutSlots);

	for (int32 slotIndex = 0; slotIndex < layoutSlots.Num(); ++slotIndex)
	{
		if (not mCards.IsValidIndex(slotIndex) || mCards[slotIndex] == nullptr)
		{
			outErrorReason = FString::Printf(TEXT("Card widget is invalid. Slot=%d"), slotIndex);
			return false;
		}

		UVdjmWidgetMediaCardWidget* cardWidget = mCards[slotIndex];
		const FVdjmWidgetMediaCarouselSlot& layoutSlot = layoutSlots[slotIndex];

		if (mSourceSnapshot.IsValidIndex(layoutSlot.SourceIndex))
		{
			cardWidget->SetCardSource(mSourceSnapshot[layoutSlot.SourceIndex]);
		}
		else
		{
			cardWidget->ClearCardSource();
		}

		cardWidget->SetCardState(mStatePolicy->ResolveCardState(layoutSlot));

		if (bDebugTraceLayout)
		{
			const FGeometry cardGeometry = cardWidget->GetCachedGeometry();
			const FVector2D cardLocalSize = cardGeometry.GetLocalSize();
			const FVector2D cardAbsTopLeft = cardGeometry.LocalToAbsolute(FVector2D::ZeroVector);
			const FVector2D cardAbsCenter = cardGeometry.LocalToAbsolute(cardLocalSize * 0.5f);
			const FWidgetTransform renderTransform = cardWidget->GetRenderTransform();
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel CardLayout Slot=%d Source=%d State=%s Widget=%s Visibility=%s RenderTranslation=%s RenderScale=%s Opacity=%.3f LocalSize=%s AbsTopLeft=%s AbsCenter=%s Z=%d TransientSlotOffset=%.3f"),
				layoutSlot.SlotIndex,
				layoutSlot.SourceIndex,
				*GetEnumValueString(mStatePolicy->ResolveCardState(layoutSlot)),
				*GetNameSafe(cardWidget),
				GetVisibilityString(cardWidget->GetVisibility()),
				*renderTransform.Translation.ToString(),
				*renderTransform.Scale.ToString(),
				layoutSlot.RenderOpacity,
				*cardLocalSize.ToString(),
				*cardAbsTopLeft.ToString(),
				*cardAbsCenter.ToString(),
				layoutSlot.ZOrder,
				mTransientSlotOffset);
		}
	}

	if (bDebugTraceLayout)
	{
		DumpDebugCarouselState(TEXT("ApplyLayoutSlots"));
	}
	ApplyHostChildOrder(mLayoutSlots);
	return true;
}

void UVdjmWidgetMediaCarouselWidget::ApplySlotTransforms(
	const TArray<FVdjmWidgetMediaCarouselSlot>& layoutSlots)
{
	const FVector2D direction = GetLayoutDirection();
	const float spacing = GetLayoutSpacing();
	const FVector2D transientOffset = direction * spacing * mTransientSlotOffset;

	for (int32 slotIndex = 0; slotIndex < layoutSlots.Num(); ++slotIndex)
	{
		if (not mCards.IsValidIndex(slotIndex) || mCards[slotIndex] == nullptr)
		{
			continue;
		}

		UVdjmWidgetMediaCardWidget* cardWidget = mCards[slotIndex];
		const FVdjmWidgetMediaCarouselSlot& layoutSlot = layoutSlots[slotIndex];
		cardWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		cardWidget->SetRenderTranslation(layoutSlot.RenderTranslation + transientOffset);
		cardWidget->SetRenderScale(layoutSlot.RenderScale);
		cardWidget->SetRenderOpacity(layoutSlot.RenderOpacity);

		if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(cardWidget->Slot))
		{
			overlaySlot->SetHorizontalAlignment(HAlign_Fill);
			overlaySlot->SetVerticalAlignment(VAlign_Fill);
			overlaySlot->SetPadding(FMargin(0.0f));
		}
	}
}

void UVdjmWidgetMediaCarouselWidget::ApplyHostChildOrder(
	const TArray<FVdjmWidgetMediaCarouselSlot>& layoutSlots)
{
	if (CardHostPanel == nullptr || layoutSlots.Num() != mCards.Num())
	{
		return;
	}

	TArray<int32> slotOrder;
	slotOrder.Reserve(layoutSlots.Num());
	for (int32 slotIndex = 0; slotIndex < layoutSlots.Num(); ++slotIndex)
	{
		slotOrder.Add(slotIndex);
	}

	slotOrder.Sort([&layoutSlots](int32 leftSlotIndex, int32 rightSlotIndex)
	{
		return layoutSlots[leftSlotIndex].ZOrder < layoutSlots[rightSlotIndex].ZOrder;
	});

	for (int32 slotIndex : slotOrder)
	{
		if (not mCards.IsValidIndex(slotIndex) || mCards[slotIndex] == nullptr)
		{
			continue;
		}

		UVdjmWidgetMediaCardWidget* cardWidget = mCards[slotIndex];
		CardHostPanel->RemoveChild(cardWidget);
		if (UOverlaySlot* overlaySlot = Cast<UOverlaySlot>(CardHostPanel->AddChild(cardWidget)))
		{
			overlaySlot->SetHorizontalAlignment(HAlign_Fill);
			overlaySlot->SetVerticalAlignment(VAlign_Fill);
			overlaySlot->SetPadding(FMargin(0.0f));
		}
	}
	ApplySlotTransforms(layoutSlots);
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

FVector2D UVdjmWidgetMediaCarouselWidget::GetLayoutDirection() const
{
	return NormalizeOrFallback(LayoutOptions.ProgressDirection, FVector2D(1.0f, 0.0f));
}

float UVdjmWidgetMediaCarouselWidget::GetLayoutSpacing() const
{
	const FVector2D direction = GetLayoutDirection();
	const FVector2D viewSize = GetCarouselViewSize();
	const float axisExtent = FMath::Max(
		1.0f,
		FMath::Abs(direction.X) * FMath::Max(1.0f, viewSize.X) +
		FMath::Abs(direction.Y) * FMath::Max(1.0f, viewSize.Y));
	return axisExtent * FMath::Clamp(LayoutOptions.NormalizedSpacing, 0.0f, 1.0f);
}

int32 UVdjmWidgetMediaCarouselWidget::ResolveActiveSourceIndex(int32 activeSourceIndex) const
{
	const int32 sourceCount = mSourceSnapshot.Num();
	if (sourceCount <= 0)
	{
		return INDEX_NONE;
	}

	if (LayoutOptions.bLoop)
	{
		return WrapIndex(activeSourceIndex, sourceCount);
	}

	return mSourceSnapshot.IsValidIndex(activeSourceIndex) ? activeSourceIndex : INDEX_NONE;
}

int32 UVdjmWidgetMediaCarouselWidget::ResolveActiveSourceIndexAfterRefresh(
	const TArray<FVdjmWidgetMediaCardSource>& sources,
	int32 previousSourceIndex,
	const FString& previousRecordId) const
{
	if (sources.Num() <= 0)
	{
		return INDEX_NONE;
	}

	switch (ActiveAfterRefreshPolicy)
	{
	case EVdjmWidgetMediaCarouselActiveAfterRefreshPolicy::EKeepRecordId:
	{
		const int32 recordSourceIndex = FindSourceIndexByRecordId(sources, previousRecordId);
		if (sources.IsValidIndex(recordSourceIndex))
		{
			return recordSourceIndex;
		}
		return FMath::Clamp(previousSourceIndex, 0, sources.Num() - 1);
	}
	case EVdjmWidgetMediaCarouselActiveAfterRefreshPolicy::EKeepIndex:
		return FMath::Clamp(previousSourceIndex, 0, sources.Num() - 1);
	case EVdjmWidgetMediaCarouselActiveAfterRefreshPolicy::ELatest:
		return sources.Num() - 1;
	case EVdjmWidgetMediaCarouselActiveAfterRefreshPolicy::EFirst:
	default:
		return 0;
	}
}

bool UVdjmWidgetMediaCarouselWidget::ApplyResolvedActiveSourceIndex(
	int32 activeSourceIndex,
	bool bRefreshLayout)
{
	const int32 resolvedActiveSourceIndex = ResolveActiveSourceIndex(activeSourceIndex);
	if (resolvedActiveSourceIndex == INDEX_NONE)
	{
		if (bDebugTraceLayout)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel ActiveChange Blocked Requested=%d Current=%d SourceCount=%d Loop=%s"),
				activeSourceIndex,
				mActiveSourceIndex,
				mSourceSnapshot.Num(),
				LayoutOptions.bLoop ? TEXT("true") : TEXT("false"));
		}
		return false;
	}

	const int32 previousSourceIndex = mActiveSourceIndex;
	mActiveSourceIndex = resolvedActiveSourceIndex;
	mTransientSlotOffset = 0.0f;
	if (previousSourceIndex != mActiveSourceIndex)
	{
		OnActiveSourceChanged.Broadcast(previousSourceIndex, mActiveSourceIndex);
	}

	if (bDebugTraceLayout)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel ActiveChanged Previous=%d Current=%d RefreshLayout=%s"),
			previousSourceIndex,
			mActiveSourceIndex,
			bRefreshLayout ? TEXT("true") : TEXT("false"));
	}

	if (bRefreshLayout)
	{
		FString errorReason;
		return RebuildVisibleCards(errorReason);
	}

	RequestActivePreviewStart(TEXT("ActiveChangedNoLayout"));
	return true;
}

FVdjmWidgetMediaCarouselInputPayload UVdjmWidgetMediaCarouselWidget::BuildInputPayload(
	const FGeometry& inGeometry,
	const FVector2D& screenPosition,
	const FVector2D& delta,
	EVdjmWidgetMediaCarouselInputAction action) const
{
	FVdjmWidgetMediaCarouselInputPayload inputPayload;
	inputPayload.Action = action;
	inputPayload.ScreenPosition = screenPosition;
	inputPayload.Delta = delta;

	const int32 slotIndex = FindNearestLayoutSlotFromScreenPosition(inGeometry, screenPosition);
	inputPayload.CardIndex = slotIndex;
	if (mLayoutSlots.IsValidIndex(slotIndex))
	{
		const FVdjmWidgetMediaCarouselSlot& layoutSlot = mLayoutSlots[slotIndex];
		inputPayload.SourceIndex = layoutSlot.SourceIndex;
		inputPayload.CardState = layoutSlot.TargetCardState;
		inputPayload.bHasSource = mSourceSnapshot.IsValidIndex(layoutSlot.SourceIndex);
	}

	return inputPayload;
}

int32 UVdjmWidgetMediaCarouselWidget::FindNearestLayoutSlotFromScreenPosition(
	const FGeometry& inGeometry,
	const FVector2D& screenPosition) const
{
	if (mLayoutSlots.Num() <= 0)
	{
		return INDEX_NONE;
	}

	FVector2D localSize = inGeometry.GetLocalSize();
	if (localSize.X <= 1.0f || localSize.Y <= 1.0f)
	{
		localSize = GetCarouselViewSize();
	}

	const FVector2D localCenter = localSize * 0.5f;
	const FVector2D transientOffset = GetLayoutDirection() * GetLayoutSpacing() * mTransientSlotOffset;
	int32 bestSlotIndex = INDEX_NONE;
	float bestDistanceSquared = TNumericLimits<float>::Max();

	for (int32 slotIndex = 0; slotIndex < mLayoutSlots.Num(); ++slotIndex)
	{
		const FVector2D slotCenter = inGeometry.LocalToAbsolute(
			localCenter + mLayoutSlots[slotIndex].RenderTranslation + transientOffset);
		const float distanceSquared = FVector2D::DistSquared(screenPosition, slotCenter);
		if (distanceSquared < bestDistanceSquared)
		{
			bestDistanceSquared = distanceSquared;
			bestSlotIndex = slotIndex;
		}
	}

	return bestSlotIndex;
}

bool UVdjmWidgetMediaCarouselWidget::HandleTapInput(
	const FGeometry& inGeometry,
	const FVector2D& screenPosition,
	const FVector2D& totalDelta,
	double elapsedSeconds)
{
	if (mInputController == nullptr)
	{
		return false;
	}

	FVdjmWidgetMediaCarouselInputPayload inputPayload = BuildInputPayload(
		inGeometry,
		screenPosition,
		totalDelta,
		EVdjmWidgetMediaCarouselInputAction::EClicked);
	mInputController->StoreInputPayload(inputPayload);

	if (not mLayoutSlots.IsValidIndex(inputPayload.CardIndex))
	{
		if (bDebugTraceInput)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel TapIgnored Reason=NoSlot Screen=%s Elapsed=%.3f"),
				*screenPosition.ToString(),
				elapsedSeconds);
		}
		return false;
	}

	if (not inputPayload.bHasSource)
	{
		OnEmptyCardTapped.Broadcast(inputPayload);
		if (bDebugTraceInput)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel TapEmpty Slot=%d State=%s Screen=%s Delta=%s Elapsed=%.3f"),
				inputPayload.CardIndex,
				*GetEnumValueString(inputPayload.CardState),
				*screenPosition.ToString(),
				*totalDelta.ToString(),
				elapsedSeconds);
		}
		return true;
	}

	const int32 activeSourceIndexBeforeTap = mActiveSourceIndex;
	OnCardTapped.Broadcast(inputPayload);
	const bool bActivated = bActivateCardOnTap && inputPayload.SourceIndex != mActiveSourceIndex
		? ApplyResolvedActiveSourceIndex(inputPayload.SourceIndex, true)
		: inputPayload.SourceIndex == mActiveSourceIndex;
	if (bActivated && inputPayload.SourceIndex == activeSourceIndexBeforeTap)
	{
		RequestActivePreviewStart(TEXT("TapActiveCard"));
	}
	if (bDebugTraceInput)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel TapCard Slot=%d Source=%d State=%s ActiveBefore=%d Activated=%s Screen=%s Delta=%s Elapsed=%.3f"),
			inputPayload.CardIndex,
			inputPayload.SourceIndex,
			*GetEnumValueString(inputPayload.CardState),
			activeSourceIndexBeforeTap,
			bActivated ? TEXT("true") : TEXT("false"),
			*screenPosition.ToString(),
			*totalDelta.ToString(),
			elapsedSeconds);
	}
	return bActivated;
}

void UVdjmWidgetMediaCarouselWidget::DumpDebugCarouselState(const FString& reason) const
{
	const FGeometry carouselGeometry = GetCachedGeometry();
	const FVector2D carouselLocalSize = carouselGeometry.GetLocalSize();
	const FVector2D carouselAbsTopLeft = carouselGeometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D carouselAbsCenter = carouselGeometry.LocalToAbsolute(carouselLocalSize * 0.5f);
	const int32 hostChildren = CardHostPanel != nullptr ? CardHostPanel->GetChildrenCount() : -1;

	UE_LOG(
		LogVdjmWidgets,
		Log,
		TEXT("VdjmCarousel State Reason=%s Widget=%s Visibility=%s Host=%s HostChildren=%d CardClass=%s SourceCount=%d CardCount=%d Active=%d Layout=%s TransientSlotOffset=%.3f LocalSize=%s AbsTopLeft=%s AbsCenter=%s"),
		*reason,
		*GetNameSafe(this),
		GetVisibilityString(GetVisibility()),
		*GetNameSafe(CardHostPanel),
		hostChildren,
		*GetNameSafe(CardWidgetClass.Get()),
		mSourceSnapshot.Num(),
		mCards.Num(),
		mActiveSourceIndex,
		*GetEnumValueString(mLayoutState),
		mTransientSlotOffset,
		*carouselLocalSize.ToString(),
		*carouselAbsTopLeft.ToString(),
		*carouselAbsCenter.ToString());

	for (int32 cardIndex = 0; cardIndex < mCards.Num(); ++cardIndex)
	{
		const UVdjmWidgetMediaCardWidget* cardWidget = mCards[cardIndex];
		if (cardWidget == nullptr)
		{
			UE_LOG(LogVdjmWidgets, Log, TEXT("VdjmCarousel StateCard Index=%d Widget=None"), cardIndex);
			continue;
		}

		const FGeometry cardGeometry = cardWidget->GetCachedGeometry();
		const FVector2D cardLocalSize = cardGeometry.GetLocalSize();
		const FVector2D cardAbsTopLeft = cardGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D cardAbsCenter = cardGeometry.LocalToAbsolute(cardLocalSize * 0.5f);
		const FWidgetTransform renderTransform = cardWidget->GetRenderTransform();
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel StateCard Index=%d Widget=%s State=%s Visibility=%s RenderTranslation=%s RenderScale=%s RenderOpacity=%.3f LocalSize=%s AbsTopLeft=%s AbsCenter=%s"),
			cardIndex,
			*GetNameSafe(cardWidget),
			*GetEnumValueString(cardWidget->GetCardState()),
			GetVisibilityString(cardWidget->GetVisibility()),
			*renderTransform.Translation.ToString(),
			*renderTransform.Scale.ToString(),
			cardWidget->GetRenderOpacity(),
			*cardLocalSize.ToString(),
			*cardAbsTopLeft.ToString(),
			*cardAbsCenter.ToString());
	}
}

void UVdjmWidgetMediaCarouselWidget::BeginDebugPointerTrace(
	const FGeometry& inGeometry,
	const FPointerEvent& pointerEvent,
	const TCHAR* inputType)
{
	const double nowSeconds = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
	const FVector2D screenPosition = pointerEvent.GetScreenSpacePosition();
	mbDebugPointerTracking = true;
	mDebugPointerStartScreenPosition = screenPosition;
	mDebugPointerLastScreenPosition = screenPosition;
	mDebugPointerStartSeconds = nowSeconds;
	mDebugPointerLastMoveLogSeconds = nowSeconds;
	mDebugPointerMoveCount = 0;

	if (mInputController != nullptr)
	{
		mInputController->StoreInputPayload(BuildInputPayload(
			inGeometry,
			screenPosition,
			FVector2D::ZeroVector,
			EVdjmWidgetMediaCarouselInputAction::EPressed));
	}

	if (bDebugTraceInput)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel Input Begin Type=%s Pointer=%d Screen=%s Local=%s GeometryLocalSize=%s AbsTopLeft=%s Consume=%s"),
			inputType,
			pointerEvent.GetPointerIndex(),
			*screenPosition.ToString(),
			*inGeometry.AbsoluteToLocal(screenPosition).ToString(),
			*inGeometry.GetLocalSize().ToString(),
			*inGeometry.LocalToAbsolute(FVector2D::ZeroVector).ToString(),
			GetBoolString(bDebugConsumeInput));
	}
}

void UVdjmWidgetMediaCarouselWidget::UpdateDebugPointerTrace(
	const FGeometry& inGeometry,
	const FPointerEvent& pointerEvent,
	const TCHAR* inputType)
{
	if (not mbDebugPointerTracking)
	{
		return;
	}

	const double nowSeconds = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
	const FVector2D screenPosition = pointerEvent.GetScreenSpacePosition();
	const FVector2D frameDelta = screenPosition - mDebugPointerLastScreenPosition;
	const FVector2D totalDelta = screenPosition - mDebugPointerStartScreenPosition;
	const FVector2D direction = GetLayoutDirection();
	const float spacing = GetLayoutSpacing();
	if (spacing > 1.0f)
	{
		mTransientSlotOffset = FMath::Clamp(FVector2D::DotProduct(totalDelta, direction) / spacing, -1.25f, 1.25f);
		ApplySlotTransforms(mLayoutSlots);
	}

	if (mInputController != nullptr)
	{
		mInputController->StoreInputPayload(BuildInputPayload(
			inGeometry,
			screenPosition,
			totalDelta,
			EVdjmWidgetMediaCarouselInputAction::EMoved));
	}

	if (not bDebugTraceInput)
	{
		mDebugPointerLastScreenPosition = screenPosition;
		return;
	}

	if (nowSeconds - mDebugPointerLastMoveLogSeconds < DebugInputMoveLogIntervalSeconds)
	{
		return;
	}

	mDebugPointerLastScreenPosition = screenPosition;
	mDebugPointerLastMoveLogSeconds = nowSeconds;
	++mDebugPointerMoveCount;

	UE_LOG(
		LogVdjmWidgets,
		Log,
		TEXT("VdjmCarousel Input Move Type=%s Pointer=%d MoveCount=%d Screen=%s Local=%s FrameDelta=%s TotalDelta=%s Distance=%.1f AxisDelta=%.1f SlotOffset=%.3f Elapsed=%.3f"),
		inputType,
		pointerEvent.GetPointerIndex(),
		mDebugPointerMoveCount,
		*screenPosition.ToString(),
		*inGeometry.AbsoluteToLocal(screenPosition).ToString(),
		*frameDelta.ToString(),
		*totalDelta.ToString(),
		totalDelta.Size(),
		FVector2D::DotProduct(totalDelta, direction),
		mTransientSlotOffset,
		nowSeconds - mDebugPointerStartSeconds);
}

void UVdjmWidgetMediaCarouselWidget::EndDebugPointerTrace(
	const FGeometry& inGeometry,
	const FPointerEvent& pointerEvent,
	const TCHAR* inputType)
{
	if (not mbDebugPointerTracking)
	{
		return;
	}

	const double nowSeconds = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
	const FVector2D screenPosition = pointerEvent.GetScreenSpacePosition();
	const FVector2D totalDelta = screenPosition - mDebugPointerStartScreenPosition;
	const double elapsedSeconds = nowSeconds - mDebugPointerStartSeconds;
	const float axisDelta = FVector2D::DotProduct(totalDelta, GetLayoutDirection());
	const bool bSwipe = FMath::Abs(axisDelta) >= DebugSwipeDistanceThreshold && elapsedSeconds >= DebugSwipeMinDurationSeconds;
	if (mInputController != nullptr)
	{
		mInputController->StoreInputPayload(BuildInputPayload(
			inGeometry,
			screenPosition,
			totalDelta,
			bSwipe ? EVdjmWidgetMediaCarouselInputAction::ESwipe : EVdjmWidgetMediaCarouselInputAction::EReleased));
	}

	const bool bSwipeCommitted = bSwipe && CommitDebugSwipe(totalDelta, elapsedSeconds);
	if (not bSwipeCommitted)
	{
		mTransientSlotOffset = 0.0f;
		ApplySlotTransforms(mLayoutSlots);
	}
	const bool bTapHandled = not bSwipe && HandleTapInput(inGeometry, screenPosition, totalDelta, elapsedSeconds);
	mbDebugPointerTracking = false;

	if (bDebugTraceInput)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel Input End Type=%s Pointer=%d Screen=%s Local=%s TotalDelta=%s Distance=%.1f AxisDelta=%.1f Elapsed=%.3f MoveCount=%d Result=%s Committed=%s TapHandled=%s Threshold=%.1f MinDuration=%.3f"),
			inputType,
			pointerEvent.GetPointerIndex(),
			*screenPosition.ToString(),
			*inGeometry.AbsoluteToLocal(screenPosition).ToString(),
			*totalDelta.ToString(),
			totalDelta.Size(),
			axisDelta,
			elapsedSeconds,
			mDebugPointerMoveCount,
			bSwipe ? TEXT("Swipe") : TEXT("Tap"),
			bSwipeCommitted ? TEXT("true") : TEXT("false"),
			bTapHandled ? TEXT("true") : TEXT("false"),
			DebugSwipeDistanceThreshold,
			DebugSwipeMinDurationSeconds);

		DumpDebugCarouselState(bSwipe ? TEXT("InputEndSwipe") : TEXT("InputEndTap"));
	}
}

bool UVdjmWidgetMediaCarouselWidget::CommitDebugSwipe(
	const FVector2D& totalDelta,
	double elapsedSeconds)
{
	const float axisDelta = FVector2D::DotProduct(totalDelta, GetLayoutDirection());
	const int32 sourceDelta = axisDelta < 0.0f ? 1 : -1;
	const int32 requestedSourceIndex = mActiveSourceIndex + sourceDelta;

	if (mSourceSnapshot.Num() <= 1)
	{
		if (bDebugTraceInput)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCarousel SwipeIgnored Reason=NotEnoughSources SourceCount=%d AxisDelta=%.1f Elapsed=%.3f"),
				mSourceSnapshot.Num(),
				axisDelta,
				elapsedSeconds);
		}
		return false;
	}

	const bool bResult = ApplyResolvedActiveSourceIndex(requestedSourceIndex, true);
	if (bDebugTraceInput)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCarousel SwipeCommit AxisDelta=%.1f SourceDelta=%d Requested=%d Result=%s Active=%d"),
			axisDelta,
			sourceDelta,
			requestedSourceIndex,
			bResult ? TEXT("true") : TEXT("false"),
			mActiveSourceIndex);
	}
	return bResult;
}

FReply UVdjmWidgetMediaCarouselWidget::BuildDebugInputReply() const
{
	return bDebugConsumeInput ? FReply::Handled() : FReply::Unhandled();
}
