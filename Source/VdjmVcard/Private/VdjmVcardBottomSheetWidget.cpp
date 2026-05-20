#include "VdjmVcardBottomSheetWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/NamedSlot.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

namespace
{
	bool IsVcardBottomSheetMouseButtonPressed(APlayerController* playerController)
	{
		const bool bPlayerControllerPressed = playerController != nullptr && playerController->IsInputKeyDown(EKeys::LeftMouseButton);
		const bool bSlatePressed = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
		return bPlayerControllerPressed || bSlatePressed;
	}
}

void UVcardBottomSheetWidget::SetOpenRatio(float openRatio)
{
	AnimateToOpenRatio(openRatio);
}

void UVcardBottomSheetWidget::SetOpenRatioImmediate(float openRatio)
{
	mbHasAnimationTarget = false;
	ApplyOpenRatio(GetClampedOpenRatio(openRatio), true);
	if (mMotionState == EVcardBottomSheetMotionState::EAnimating)
	{
		SetMotionState(EVcardBottomSheetMotionState::EIdle);
	}
	StopMotionTimerIfIdle();
}

void UVcardBottomSheetWidget::AnimateToOpenRatio(float openRatio)
{
	mTargetOpenRatio = GetClampedOpenRatio(openRatio);
	mbHasAnimationTarget = true;
	SetMotionState(EVcardBottomSheetMotionState::EAnimating);
	StartMotionTimer();
}

void UVcardBottomSheetWidget::ToggleOpenRatio()
{
	const float midpointRatio = (CollapsedRatio + ExpandedRatio) * 0.5f;
	const float targetRatio = mOpenRatio >= midpointRatio ? CollapsedRatio : ExpandedRatio;
	AnimateToOpenRatio(targetRatio);
}

void UVcardBottomSheetWidget::OpenSheet()
{
	AnimateToOpenRatio(ExpandedRatio);
}

void UVcardBottomSheetWidget::CloseSheet()
{
	AnimateToOpenRatio(CollapsedRatio);
}

void UVcardBottomSheetWidget::SnapToNearestRatio()
{
	AnimateToOpenRatio(CalculateNearestSnapRatio(mOpenRatio));
}

float UVcardBottomSheetWidget::GetSheetTopScreenY() const
{
	const UWidget* targetWidget = IsValid(SheetPanel) ? SheetPanel.Get() : static_cast<const UWidget*>(this);
	if (!IsValid(targetWidget))
	{
		return 0.0f;
	}

	const FGeometry geometry = targetWidget->GetCachedGeometry();
	return geometry.LocalToAbsolute(FVector2D::ZeroVector).Y;
}

float UVcardBottomSheetWidget::GetSheetTopNormalized() const
{
	const FVector2D viewportSize = UWidgetLayoutLibrary::GetViewportSize(const_cast<UVcardBottomSheetWidget*>(this));
	if (viewportSize.Y <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(GetSheetTopScreenY() / viewportSize.Y, 0.0f, 1.0f);
}

void UVcardBottomSheetWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindDragHandle();
	mOpenRatio = GetClampedOpenRatio(InitialOpenRatio);
	mTargetOpenRatio = mOpenRatio;
	ApplySheetTransform(mOpenRatio);
}

void UVcardBottomSheetWidget::NativeDestruct()
{
	StopMotionTimer();
	UnbindDragHandle();

	Super::NativeDestruct();
}

void UVcardBottomSheetWidget::HandleDragHandlePressed()
{
	FVector2D pointerPosition;
	EVcardBottomSheetPointerSource pointerSource = EVcardBottomSheetPointerSource::ENone;
	if (!SamplePressedPointerScreenPosition(pointerPosition, pointerSource) && !SamplePointerScreenPosition(pointerPosition))
	{
		pointerPosition = mLastPointerScreenPosition;
	}

	mbHasAnimationTarget = false;
	mPointerSource = pointerSource;
	mPressStartScreenPosition = pointerPosition;
	mLastPointerScreenPosition = pointerPosition;
	mCurrentPointerScreenPosition = pointerPosition;
	mFramePointerDelta = FVector2D::ZeroVector;
	mTotalPointerDelta = FVector2D::ZeroVector;
	mPressStartOpenRatio = mOpenRatio;
	SetMotionState(EVcardBottomSheetMotionState::EPressTracking);
	StartMotionTimer();
}

void UVcardBottomSheetWidget::HandleDragHandleReleased()
{
	FVector2D pointerPosition;
	if (SamplePointerScreenPosition(pointerPosition))
	{
		UpdatePointerDeltas(pointerPosition);
	}

	FinishPointerInteraction(false);
}

void UVcardBottomSheetWidget::BindDragHandle()
{
	if (mbDragHandleBound || !IsValid(DragHandle))
	{
		return;
	}

	DragHandle->OnPressed.AddDynamic(this, &UVcardBottomSheetWidget::HandleDragHandlePressed);
	DragHandle->OnReleased.AddDynamic(this, &UVcardBottomSheetWidget::HandleDragHandleReleased);
	mbDragHandleBound = true;
}

void UVcardBottomSheetWidget::UnbindDragHandle()
{
	if (!mbDragHandleBound || !IsValid(DragHandle))
	{
		mbDragHandleBound = false;
		return;
	}

	DragHandle->OnPressed.RemoveDynamic(this, &UVcardBottomSheetWidget::HandleDragHandlePressed);
	DragHandle->OnReleased.RemoveDynamic(this, &UVcardBottomSheetWidget::HandleDragHandleReleased);
	mbDragHandleBound = false;
}

void UVcardBottomSheetWidget::StartMotionTimer()
{
	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return;
	}

	mLastMotionTimeSeconds = world->GetTimeSeconds();
	if (!world->GetTimerManager().IsTimerActive(mMotionTimerHandle))
	{
		world->GetTimerManager().SetTimer(
			mMotionTimerHandle,
			this,
			&UVcardBottomSheetWidget::HandleMotionTimer,
			FMath::Max(TrackingIntervalSeconds, 0.001f),
			true);
	}
}

void UVcardBottomSheetWidget::StopMotionTimerIfIdle()
{
	if (mMotionState == EVcardBottomSheetMotionState::EIdle && !mbHasAnimationTarget)
	{
		StopMotionTimer();
	}
}

void UVcardBottomSheetWidget::StopMotionTimer()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(mMotionTimerHandle);
	}
}

void UVcardBottomSheetWidget::HandleMotionTimer()
{
	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return;
	}

	const float currentTimeSeconds = world->GetTimeSeconds();
	const float deltaSeconds = mLastMotionTimeSeconds > 0.0f
		? FMath::Max(currentTimeSeconds - mLastMotionTimeSeconds, 0.0f)
		: FMath::Max(TrackingIntervalSeconds, 0.001f);
	mLastMotionTimeSeconds = currentTimeSeconds;

	if (mMotionState == EVcardBottomSheetMotionState::EPressTracking ||
		mMotionState == EVcardBottomSheetMotionState::EDragging)
	{
		FVector2D pointerPosition;
		if (!SampleTrackedPressedPointerScreenPosition(pointerPosition))
		{
			FinishPointerInteraction(false);
		}
		else
		{
			UpdatePointerDeltas(pointerPosition);
			if (mMotionState == EVcardBottomSheetMotionState::EPressTracking && ShouldStartDrag())
			{
				BeginSheetDrag();
			}

			if (mMotionState == EVcardBottomSheetMotionState::EDragging)
			{
				ApplyOpenRatio(CalculateOpenRatioFromDrag(), true);
			}
		}
	}

	if (mMotionState == EVcardBottomSheetMotionState::EAnimating && mbHasAnimationTarget)
	{
		const float nextOpenRatio = CalculateAnimatedOpenRatio(deltaSeconds);
		ApplyOpenRatio(nextOpenRatio, true);
		if (FMath::IsNearlyEqual(mOpenRatio, mTargetOpenRatio, AnimationCompleteTolerance))
		{
			ApplyOpenRatio(mTargetOpenRatio, true);
			mbHasAnimationTarget = false;
			SetMotionState(EVcardBottomSheetMotionState::EIdle);
		}
	}

	StopMotionTimerIfIdle();
}

bool UVcardBottomSheetWidget::SamplePointerScreenPosition(FVector2D& outScreenPosition) const
{
	APlayerController* playerController = GetOwningPlayer();
	if (playerController == nullptr)
	{
		return false;
	}

	float pointerX = 0.0f;
	float pointerY = 0.0f;
	bool bTouchPressed = false;
	playerController->GetInputTouchState(ETouchIndex::Touch1, pointerX, pointerY, bTouchPressed);
	if (bTouchPressed)
	{
		outScreenPosition = FVector2D(pointerX, pointerY);
		return true;
	}

	if (playerController->GetMousePosition(pointerX, pointerY))
	{
		outScreenPosition = FVector2D(pointerX, pointerY);
		return true;
	}

	return false;
}

bool UVcardBottomSheetWidget::SamplePressedPointerScreenPosition(FVector2D& outScreenPosition, EVcardBottomSheetPointerSource& outPointerSource) const
{
	outPointerSource = EVcardBottomSheetPointerSource::ENone;

	APlayerController* playerController = GetOwningPlayer();
	if (playerController == nullptr)
	{
		return false;
	}

	float pointerX = 0.0f;
	float pointerY = 0.0f;
	bool bTouchPressed = false;
	playerController->GetInputTouchState(ETouchIndex::Touch1, pointerX, pointerY, bTouchPressed);
	if (bTouchPressed)
	{
		outScreenPosition = FVector2D(pointerX, pointerY);
		outPointerSource = EVcardBottomSheetPointerSource::ETouch;
		return true;
	}

	if (IsVcardBottomSheetMouseButtonPressed(playerController) && playerController->GetMousePosition(pointerX, pointerY))
	{
		outScreenPosition = FVector2D(pointerX, pointerY);
		outPointerSource = EVcardBottomSheetPointerSource::EMouse;
		return true;
	}

	return false;
}

bool UVcardBottomSheetWidget::SampleTrackedPressedPointerScreenPosition(FVector2D& outScreenPosition) const
{
	APlayerController* playerController = GetOwningPlayer();
	if (playerController == nullptr)
	{
		return false;
	}

	float pointerX = 0.0f;
	float pointerY = 0.0f;
	bool bTouchPressed = false;
	switch (mPointerSource)
	{
	case EVcardBottomSheetPointerSource::ETouch:
		playerController->GetInputTouchState(ETouchIndex::Touch1, pointerX, pointerY, bTouchPressed);
		if (bTouchPressed)
		{
			outScreenPosition = FVector2D(pointerX, pointerY);
			return true;
		}
		return false;

	case EVcardBottomSheetPointerSource::EMouse:
		if (IsVcardBottomSheetMouseButtonPressed(playerController) && playerController->GetMousePosition(pointerX, pointerY))
		{
			outScreenPosition = FVector2D(pointerX, pointerY);
			return true;
		}
		return false;

	default:
	{
		EVcardBottomSheetPointerSource pointerSource = EVcardBottomSheetPointerSource::ENone;
		return SamplePressedPointerScreenPosition(outScreenPosition, pointerSource);
	}
	}
}

void UVcardBottomSheetWidget::UpdatePointerDeltas(const FVector2D& screenPosition)
{
	mCurrentPointerScreenPosition = screenPosition;
	mFramePointerDelta = mCurrentPointerScreenPosition - mLastPointerScreenPosition;
	mTotalPointerDelta = mCurrentPointerScreenPosition - mPressStartScreenPosition;
	mLastPointerScreenPosition = mCurrentPointerScreenPosition;
}

bool UVcardBottomSheetWidget::ShouldStartDrag() const
{
	return FMath::Abs(mTotalPointerDelta.Y) >= DragStartThresholdPixels;
}

void UVcardBottomSheetWidget::BeginSheetDrag()
{
	SetMotionState(EVcardBottomSheetMotionState::EDragging);
	BP_OnDragStarted(mOpenRatio);
}

void UVcardBottomSheetWidget::EndSheetDrag(bool bWasCancelled)
{
	(void)bWasCancelled;
	BP_OnDragFinished(mOpenRatio);
}

void UVcardBottomSheetWidget::FinishPointerInteraction(bool bWasCancelled)
{
	mPointerSource = EVcardBottomSheetPointerSource::ENone;

	if (mMotionState == EVcardBottomSheetMotionState::EDragging)
	{
		EndSheetDrag(bWasCancelled);
		SnapToNearestRatio();
		return;
	}

	if (mMotionState == EVcardBottomSheetMotionState::EPressTracking)
	{
		SetMotionState(EVcardBottomSheetMotionState::EIdle);
		if (!bWasCancelled)
		{
			ToggleOpenRatio();
		}
		return;
	}

	StopMotionTimerIfIdle();
}

float UVcardBottomSheetWidget::CalculateOpenRatioFromDrag() const
{
	const float dragRangePixels = GetEffectiveDragRangePixels();
	if (dragRangePixels <= KINDA_SMALL_NUMBER)
	{
		return mPressStartOpenRatio;
	}

	float minRatio = MinOpenRatio;
	float maxRatio = MaxOpenRatio;
	if (minRatio > maxRatio)
	{
		Swap(minRatio, maxRatio);
	}

	const float ratioRange = FMath::Max(maxRatio - minRatio, KINDA_SMALL_NUMBER);
	const float ratioDelta = (-mTotalPointerDelta.Y / dragRangePixels) * ratioRange;
	return GetClampedOpenRatio(mPressStartOpenRatio + ratioDelta);
}

float UVcardBottomSheetWidget::CalculateNearestSnapRatio(float openRatio) const
{
	if (SnapRatios.IsEmpty())
	{
		const float midpointRatio = (CollapsedRatio + ExpandedRatio) * 0.5f;
		return openRatio >= midpointRatio ? ExpandedRatio : CollapsedRatio;
	}

	float nearestRatio = GetClampedOpenRatio(SnapRatios[0]);
	float nearestDistance = FMath::Abs(openRatio - nearestRatio);
	for (int32 ratioIndex = 1; ratioIndex < SnapRatios.Num(); ++ratioIndex)
	{
		const float candidateRatio = GetClampedOpenRatio(SnapRatios[ratioIndex]);
		const float candidateDistance = FMath::Abs(openRatio - candidateRatio);
		if (candidateDistance < nearestDistance)
		{
			nearestRatio = candidateRatio;
			nearestDistance = candidateDistance;
		}
	}

	return nearestRatio;
}

float UVcardBottomSheetWidget::CalculateAnimatedOpenRatio(float deltaSeconds) const
{
	return FMath::FInterpTo(mOpenRatio, mTargetOpenRatio, deltaSeconds, AnimationInterpSpeed);
}

float UVcardBottomSheetWidget::CalculateSheetTranslationY(float openRatio) const
{
	float minRatio = MinOpenRatio;
	float maxRatio = MaxOpenRatio;
	if (minRatio > maxRatio)
	{
		Swap(minRatio, maxRatio);
	}

	const float ratioRange = FMath::Max(maxRatio - minRatio, KINDA_SMALL_NUMBER);
	const float normalizedOpenRatio = FMath::Clamp((GetClampedOpenRatio(openRatio) - minRatio) / ratioRange, 0.0f, 1.0f);
	return (1.0f - normalizedOpenRatio) * GetEffectiveDragRangePixels();
}

float UVcardBottomSheetWidget::GetViewportHeightPixels() const
{
	const FVector2D viewportSize = UWidgetLayoutLibrary::GetViewportSize(const_cast<UVcardBottomSheetWidget*>(this));
	return FMath::Max(viewportSize.Y, 0.0f);
}

float UVcardBottomSheetWidget::GetEffectiveDragRangePixels() const
{
	if (DragRangeBasis == EVcardBottomSheetDragRangeBasis::ECustomPixels)
	{
		return DragRangePixelsOverride > 0.0f ? DragRangePixelsOverride : GetViewportHeightPixels();
	}

	if (DragRangeBasis == EVcardBottomSheetDragRangeBasis::EViewportHeight)
	{
		return GetViewportHeightPixels();
	}

	const UWidget* targetWidget = IsValid(SheetPanel) ? SheetPanel.Get() : static_cast<const UWidget*>(this);
	if (!IsValid(targetWidget))
	{
		return GetViewportHeightPixels();
	}
	return FMath::Max(targetWidget->GetCachedGeometry().GetLocalSize().Y, 0.0f);
}

float UVcardBottomSheetWidget::GetClampedOpenRatio(float openRatio) const
{
	float minRatio = MinOpenRatio;
	float maxRatio = MaxOpenRatio;
	if (minRatio > maxRatio)
	{
		Swap(minRatio, maxRatio);
	}

	return FMath::Clamp(openRatio, minRatio, maxRatio);
}

void UVcardBottomSheetWidget::ApplyOpenRatio(float openRatio, bool bBroadcastChange)
{
	const float previousRatio = mOpenRatio;
	mOpenRatio = GetClampedOpenRatio(openRatio);
	ApplySheetTransform(mOpenRatio);

	if (bBroadcastChange && !FMath::IsNearlyEqual(previousRatio, mOpenRatio))
	{
		OnOpenRatioChanged.Broadcast(previousRatio, mOpenRatio);
		BP_OnOpenRatioChanged(previousRatio, mOpenRatio);
	}
}

void UVcardBottomSheetWidget::ApplySheetTransform(float openRatio)
{
	if (IsValid(SheetPanel))
	{
		SheetPanel->SetRenderTranslation(FVector2D(0.0f, CalculateSheetTranslationY(openRatio)));
	}
}

void UVcardBottomSheetWidget::SetMotionState(EVcardBottomSheetMotionState newState)
{
	if (mMotionState == newState)
	{
		return;
	}

	mMotionState = newState;
	OnMotionStateChanged.Broadcast(newState);
	BP_OnMotionStateChanged(newState);
}
