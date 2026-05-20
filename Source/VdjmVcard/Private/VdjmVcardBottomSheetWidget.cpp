#include "VdjmVcardBottomSheetWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/NamedSlot.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "VdjmVcard.h"

namespace
{
	constexpr int32 VcardBottomSheetInitialApplyMaxRetries = 12;

	const TCHAR* GetVcardBottomSheetDragRangeBasisText(EVcardBottomSheetDragRangeBasis dragRangeBasis)
	{
		switch (dragRangeBasis)
		{
		case EVcardBottomSheetDragRangeBasis::EViewportHeight:
			return TEXT("ViewportHeight");
		case EVcardBottomSheetDragRangeBasis::ESheetPanelHeight:
			return TEXT("SheetPanelHeight");
		case EVcardBottomSheetDragRangeBasis::ECustomPixels:
			return TEXT("CustomPixels");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetVcardBottomSheetMotionStateText(EVcardBottomSheetMotionState motionState)
	{
		switch (motionState)
		{
		case EVcardBottomSheetMotionState::EIdle:
			return TEXT("Idle");
		case EVcardBottomSheetMotionState::EPressTracking:
			return TEXT("PressTracking");
		case EVcardBottomSheetMotionState::EDragging:
			return TEXT("Dragging");
		case EVcardBottomSheetMotionState::EAnimating:
			return TEXT("Animating");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetVcardBottomSheetPointerSourceText(EVcardBottomSheetPointerSource pointerSource)
	{
		switch (pointerSource)
		{
		case EVcardBottomSheetPointerSource::ETouch:
			return TEXT("Touch");
		case EVcardBottomSheetPointerSource::EMouse:
			return TEXT("Mouse");
		case EVcardBottomSheetPointerSource::ENone:
		default:
			return TEXT("None");
		}
	}

	const TCHAR* GetVcardBottomSheetMoveDirectionText(EVcardBottomSheetMoveDirection moveDirection)
	{
		switch (moveDirection)
		{
		case EVcardBottomSheetMoveDirection::EOpening:
			return TEXT("Opening");
		case EVcardBottomSheetMoveDirection::EClosing:
			return TEXT("Closing");
		case EVcardBottomSheetMoveDirection::ENone:
		default:
			return TEXT("None");
		}
	}

	bool IsVcardBottomSheetMouseButtonPressed(APlayerController* playerController)
	{
		const bool bPlayerControllerPressed = playerController != nullptr && playerController->IsInputKeyDown(EKeys::LeftMouseButton);
		const bool bSlatePressed = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
		return bPlayerControllerPressed || bSlatePressed;
	}

	bool SampleVcardBottomSheetMousePosition(APlayerController* playerController, FVector2D& outScreenPosition)
	{
		if (FSlateApplication::IsInitialized())
		{
			outScreenPosition = FSlateApplication::Get().GetCursorPos();
			return true;
		}

		float pointerX = 0.0f;
		float pointerY = 0.0f;
		if (playerController != nullptr && playerController->GetMousePosition(pointerX, pointerY))
		{
			outScreenPosition = FVector2D(pointerX, pointerY);
			return true;
		}

		return false;
	}
}

void UVcardBottomSheetWidget::SetOpenRatio(float openRatio)
{
	AnimateToOpenRatio(openRatio);
}

void UVcardBottomSheetWidget::SetOpenRatioImmediate(float openRatio)
{
	mbHasAnimationTarget = false;
	const float previousRatio = mOpenRatio;
	ApplyOpenRatio(GetClampedOpenRatio(openRatio), true);
	SetLastMoveDirection(CalculateOpenRatioMoveDirection(previousRatio, mOpenRatio), mMotionState);
	RefreshBoundaryDirection(mMotionState);
	if (mMotionState == EVcardBottomSheetMotionState::EAnimating)
	{
		SetMotionState(EVcardBottomSheetMotionState::EIdle);
	}
	StopMotionTimerIfIdle();
}

void UVcardBottomSheetWidget::AnimateToOpenRatio(float openRatio)
{
	AnimateToOpenRatioWithDirection(openRatio, CalculateOpenRatioMoveDirection(mOpenRatio, GetClampedOpenRatio(openRatio)));
}

void UVcardBottomSheetWidget::ToggleOpenRatio()
{
	const float targetRatio = ResolveDirectedToggleTargetRatio();
	AnimateToOpenRatioWithDirection(targetRatio, CalculateOpenRatioMoveDirection(mOpenRatio, targetRatio));
}

void UVcardBottomSheetWidget::OpenSheet()
{
	AnimateToOpenRatioWithDirection(ExpandedRatio, EVcardBottomSheetMoveDirection::EOpening);
}

void UVcardBottomSheetWidget::CloseSheet()
{
	AnimateToOpenRatioWithDirection(CollapsedRatio, EVcardBottomSheetMoveDirection::EClosing);
}

void UVcardBottomSheetWidget::SnapToNearestRatio()
{
	const float targetRatio = CalculateNearestSnapRatio(mOpenRatio);
	EVcardBottomSheetMoveDirection moveDirection = mLastMoveDirection;
	if (moveDirection == EVcardBottomSheetMoveDirection::ENone)
	{
		moveDirection = CalculateOpenRatioMoveDirection(mOpenRatio, targetRatio);
	}

	AnimateToOpenRatioWithDirection(targetRatio, moveDirection);
}

void UVcardBottomSheetWidget::ApplyInitialOpenRatio()
{
	mInitialApplyRetryCount = 0;
	TryApplyInitialOpenRatio(true);
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
	ApplyInitialOpenRatio();

	UE_LOG(
		LogVdjmVcard,
		Display,
		TEXT("VcardBottomSheet Construct Widget=%s SheetPanel=%s DragHandle=%s Content=%s InitialRaw=%.3f AppliedOpen=%.3f Collapsed=%.3f Expanded=%.3f RangeBasis=%s RangePixels=%.1f TopY=%.1f TopNorm=%.3f Direction=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SheetPanel.Get()),
		*GetNameSafe(DragHandle.Get()),
		*GetNameSafe(Content.Get()),
		InitialOpenRatio,
		mOpenRatio,
		CollapsedRatio,
		ExpandedRatio,
		GetVcardBottomSheetDragRangeBasisText(DragRangeBasis),
		GetEffectiveDragRangePixels(),
		GetSheetTopScreenY(),
		GetSheetTopNormalized(),
		GetVcardBottomSheetMoveDirectionText(mLastMoveDirection));
}

void UVcardBottomSheetWidget::NativeDestruct()
{
	StopMotionTimer();
	CancelInitialOpenRatioRetry();
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
	mbButtonReleaseHinted = false;
	mPointerSource = pointerSource;
	mPressStartScreenPosition = pointerPosition;
	mLastPointerScreenPosition = pointerPosition;
	mCurrentPointerScreenPosition = pointerPosition;
	mFramePointerDelta = FVector2D::ZeroVector;
	mTotalPointerDelta = FVector2D::ZeroVector;
	mPressStartOpenRatio = mOpenRatio;
	SetMotionState(EVcardBottomSheetMotionState::EPressTracking);
	StartMotionTimer();

	UE_LOG(
		LogVdjmVcard,
		Display,
		TEXT("VcardBottomSheet PressCandidate Widget=%s Source=%s Pos=%s Open=%.3f RangePixels=%.1f Threshold=%.1f"),
		*GetNameSafe(this),
		GetVcardBottomSheetPointerSourceText(mPointerSource),
		*pointerPosition.ToString(),
		mOpenRatio,
		GetEffectiveDragRangePixels(),
		DragStartThresholdPixels);
}

void UVcardBottomSheetWidget::HandleDragHandleReleased()
{
	FVector2D pointerPosition;
	if (SamplePointerScreenPosition(pointerPosition))
	{
		mCurrentPointerScreenPosition = pointerPosition;
	}

	UE_LOG(
		LogVdjmVcard,
		Display,
		TEXT("VcardBottomSheet ButtonReleased Widget=%s State=%s Pos=%s TotalDelta=%s Distance=%.1f Open=%.3f"),
		*GetNameSafe(this),
		GetVcardBottomSheetMotionStateText(mMotionState),
		*pointerPosition.ToString(),
		*mTotalPointerDelta.ToString(),
		mTotalPointerDelta.Size(),
		mOpenRatio);
	SetPointerReleasedHint();
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
		const bool bPointerPressed = IsTrackedPointerPressed();
		const bool bHasPointerPosition = SamplePointerScreenPosition(pointerPosition);
		if (bHasPointerPosition)
		{
			UpdatePointerDeltas(pointerPosition);
		}

		if (!bPointerPressed)
		{
			UE_LOG(
				LogVdjmVcard,
				Display,
				TEXT("VcardBottomSheet ReleaseDetectedByTimer Widget=%s State=%s Source=%s ReleaseHint=%s LastPos=%s TotalDelta=%s Distance=%.1f Open=%.3f"),
				*GetNameSafe(this),
				GetVcardBottomSheetMotionStateText(mMotionState),
				GetVcardBottomSheetPointerSourceText(mPointerSource),
				mbButtonReleaseHinted ? TEXT("true") : TEXT("false"),
				*mLastPointerScreenPosition.ToString(),
				*mTotalPointerDelta.ToString(),
				mTotalPointerDelta.Size(),
				mOpenRatio);
			if (mMotionState == EVcardBottomSheetMotionState::EPressTracking && ShouldStartDrag())
			{
				BeginSheetDrag();
				ApplyOpenRatio(CalculateOpenRatioFromDrag(), true);
			}
			FinishPointerInteraction(false);
		}
		else if (bHasPointerPosition)
		{
			if (mMotionState == EVcardBottomSheetMotionState::EPressTracking && ShouldStartDrag())
			{
				BeginSheetDrag();
			}

			if (mMotionState == EVcardBottomSheetMotionState::EDragging)
			{
				SetLastMoveDirection(CalculatePointerMoveDirection(), mMotionState);
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
			const EVcardBottomSheetMotionState previousState = mMotionState;
			const EVcardBottomSheetMoveDirection settledDirection = mLastMoveDirection;
			BroadcastSettled(settledDirection, previousState, mOpenRatio);
			RefreshBoundaryDirection(previousState);
			SetMotionState(EVcardBottomSheetMotionState::EIdle);
		}
	}

	StopMotionTimerIfIdle();
}

void UVcardBottomSheetWidget::TryApplyInitialOpenRatio(bool bAllowRetry)
{
	mbHasAnimationTarget = false;
	mOpenRatio = GetClampedOpenRatio(InitialOpenRatio);
	mTargetOpenRatio = mOpenRatio;
	ApplySheetTransform(mOpenRatio);
	RefreshBoundaryDirection(mMotionState);

	const float dragRangePixels = GetEffectiveDragRangePixels();
	const float translationY = CalculateSheetTranslationY(mOpenRatio);
	if (dragRangePixels <= KINDA_SMALL_NUMBER)
	{
		if (bAllowRetry && mInitialApplyRetryCount < VcardBottomSheetInitialApplyMaxRetries)
		{
			mbInitialApplyPending = true;
			ScheduleInitialOpenRatioRetry();
			UE_LOG(
				LogVdjmVcard,
				Display,
				TEXT("VcardBottomSheet InitialApplyDeferred Widget=%s Retry=%d/%d InitialRaw=%.3f AppliedOpen=%.3f TranslationY=%.1f RangePixels=%.1f Direction=%s"),
				*GetNameSafe(this),
				mInitialApplyRetryCount,
				VcardBottomSheetInitialApplyMaxRetries,
				InitialOpenRatio,
				mOpenRatio,
				translationY,
				dragRangePixels,
				GetVcardBottomSheetMoveDirectionText(mLastMoveDirection));
			return;
		}

		mbInitialApplyPending = false;
		UE_LOG(
			LogVdjmVcard,
			Warning,
			TEXT("VcardBottomSheet InitialApplyFailed Widget=%s Retry=%d/%d InitialRaw=%.3f AppliedOpen=%.3f TranslationY=%.1f RangePixels=%.1f Direction=%s"),
			*GetNameSafe(this),
			mInitialApplyRetryCount,
			VcardBottomSheetInitialApplyMaxRetries,
			InitialOpenRatio,
			mOpenRatio,
			translationY,
			dragRangePixels,
			GetVcardBottomSheetMoveDirectionText(mLastMoveDirection));
		return;
	}

	mbInitialApplyPending = false;
	CancelInitialOpenRatioRetry();
	UE_LOG(
		LogVdjmVcard,
		Display,
		TEXT("VcardBottomSheet InitialApplied Widget=%s Retry=%d InitialRaw=%.3f AppliedOpen=%.3f TranslationY=%.1f RangePixels=%.1f Direction=%s"),
		*GetNameSafe(this),
		mInitialApplyRetryCount,
		InitialOpenRatio,
		mOpenRatio,
		translationY,
		dragRangePixels,
		GetVcardBottomSheetMoveDirectionText(mLastMoveDirection));
}

void UVcardBottomSheetWidget::ScheduleInitialOpenRatioRetry()
{
	UWorld* world = GetWorld();
	if (world == nullptr || world->GetTimerManager().IsTimerActive(mInitialApplyRetryTimerHandle))
	{
		return;
	}

	world->GetTimerManager().SetTimer(
		mInitialApplyRetryTimerHandle,
		[this]()
		{
			++mInitialApplyRetryCount;
			TryApplyInitialOpenRatio(true);
		},
		FMath::Max(TrackingIntervalSeconds, 0.001f),
		false);
}

void UVcardBottomSheetWidget::CancelInitialOpenRatioRetry()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(mInitialApplyRetryTimerHandle);
	}
}

void UVcardBottomSheetWidget::AnimateToOpenRatioWithDirection(float openRatio, EVcardBottomSheetMoveDirection moveDirection)
{
	mTargetOpenRatio = GetClampedOpenRatio(openRatio);
	if (moveDirection == EVcardBottomSheetMoveDirection::ENone)
	{
		moveDirection = CalculateOpenRatioMoveDirection(mOpenRatio, mTargetOpenRatio);
	}

	SetLastMoveDirection(moveDirection, mMotionState);
	mbHasAnimationTarget = true;
	SetMotionState(EVcardBottomSheetMotionState::EAnimating);
	StartMotionTimer();
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
	if (mPointerSource == EVcardBottomSheetPointerSource::ETouch)
	{
		playerController->GetInputTouchState(ETouchIndex::Touch1, pointerX, pointerY, bTouchPressed);
		if (bTouchPressed)
		{
			outScreenPosition = FVector2D(pointerX, pointerY);
			return true;
		}

		return false;
	}

	if (mPointerSource == EVcardBottomSheetPointerSource::EMouse)
	{
		return SampleVcardBottomSheetMousePosition(playerController, outScreenPosition);
	}

	playerController->GetInputTouchState(ETouchIndex::Touch1, pointerX, pointerY, bTouchPressed);
	if (bTouchPressed)
	{
		outScreenPosition = FVector2D(pointerX, pointerY);
		return true;
	}

	return SampleVcardBottomSheetMousePosition(playerController, outScreenPosition);
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

	if (IsVcardBottomSheetMouseButtonPressed(playerController) && SampleVcardBottomSheetMousePosition(playerController, outScreenPosition))
	{
		outPointerSource = EVcardBottomSheetPointerSource::EMouse;
		return true;
	}

	return false;
}

bool UVcardBottomSheetWidget::IsTrackedPointerPressed() const
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
		return bTouchPressed;

	case EVcardBottomSheetPointerSource::EMouse:
		return IsVcardBottomSheetMouseButtonPressed(playerController);

	default:
	{
		EVcardBottomSheetPointerSource pointerSource = EVcardBottomSheetPointerSource::ENone;
		FVector2D pointerPosition;
		return SamplePressedPointerScreenPosition(pointerPosition, pointerSource);
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
	UE_LOG(
		LogVdjmVcard,
		Display,
		TEXT("VcardBottomSheet DragBegin Widget=%s Source=%s TotalDelta=%s Distance=%.1f Threshold=%.1f PressOpen=%.3f"),
		*GetNameSafe(this),
		GetVcardBottomSheetPointerSourceText(mPointerSource),
		*mTotalPointerDelta.ToString(),
		mTotalPointerDelta.Size(),
		DragStartThresholdPixels,
		mPressStartOpenRatio);
	SetMotionState(EVcardBottomSheetMotionState::EDragging);
	SetLastMoveDirection(CalculatePointerMoveDirection(), EVcardBottomSheetMotionState::EPressTracking);
	BP_OnDragStarted(mOpenRatio);
}

void UVcardBottomSheetWidget::EndSheetDrag(bool bWasCancelled)
{
	UE_LOG(
		LogVdjmVcard,
		Display,
		TEXT("VcardBottomSheet DragFinished Widget=%s Cancelled=%s TotalDelta=%s Distance=%.1f Open=%.3f SnapTarget=%.3f"),
		*GetNameSafe(this),
		bWasCancelled ? TEXT("true") : TEXT("false"),
		*mTotalPointerDelta.ToString(),
		mTotalPointerDelta.Size(),
		mOpenRatio,
		CalculateNearestSnapRatio(mOpenRatio));
	BP_OnDragFinished(mOpenRatio);
}

void UVcardBottomSheetWidget::FinishPointerInteraction(bool bWasCancelled)
{
	mPointerSource = EVcardBottomSheetPointerSource::ENone;
	mbButtonReleaseHinted = false;

	if (mMotionState == EVcardBottomSheetMotionState::EDragging)
	{
		UE_LOG(
			LogVdjmVcard,
			Display,
			TEXT("VcardBottomSheet ReleaseAsDrag Widget=%s Cancelled=%s Open=%.3f"),
			*GetNameSafe(this),
			bWasCancelled ? TEXT("true") : TEXT("false"),
			mOpenRatio);
		EndSheetDrag(bWasCancelled);
		SnapToNearestRatio();
		return;
	}

	if (mMotionState == EVcardBottomSheetMotionState::EPressTracking)
	{
		SetMotionState(EVcardBottomSheetMotionState::EIdle);
		if (!bWasCancelled)
		{
			UE_LOG(
				LogVdjmVcard,
				Display,
				TEXT("VcardBottomSheet ReleaseAsTap Widget=%s TotalDelta=%s Distance=%.1f Threshold=%.1f Open=%.3f"),
				*GetNameSafe(this),
				*mTotalPointerDelta.ToString(),
				mTotalPointerDelta.Size(),
				DragStartThresholdPixels,
				mOpenRatio);
			ToggleOpenRatio();
		}
		return;
	}

	StopMotionTimerIfIdle();
}

void UVcardBottomSheetWidget::SetPointerReleasedHint()
{
	mbButtonReleaseHinted = true;
}

float UVcardBottomSheetWidget::ResolveDirectedToggleTargetRatio() const
{
	const float minRatio = GetClampedOpenRatio(MinOpenRatio);
	const float maxRatio = GetClampedOpenRatio(MaxOpenRatio);
	const float collapsedRatio = GetClampedOpenRatio(CollapsedRatio);
	const float expandedRatio = GetClampedOpenRatio(ExpandedRatio);
	const float boundaryTolerance = FMath::Max(AnimationCompleteTolerance, 0.002f);
	if (mOpenRatio <= minRatio + boundaryTolerance || mOpenRatio <= collapsedRatio + boundaryTolerance)
	{
		return expandedRatio;
	}

	if (mOpenRatio >= maxRatio - boundaryTolerance || mOpenRatio >= expandedRatio - boundaryTolerance)
	{
		return collapsedRatio;
	}

	if (mLastMoveDirection == EVcardBottomSheetMoveDirection::EClosing)
	{
		return collapsedRatio;
	}

	if (mLastMoveDirection == EVcardBottomSheetMoveDirection::EOpening)
	{
		return expandedRatio;
	}

	const float midpointRatio = (collapsedRatio + expandedRatio) * 0.5f;
	return mOpenRatio >= midpointRatio ? collapsedRatio : expandedRatio;
}

float UVcardBottomSheetWidget::CalculateOpenRatioFromDrag() const
{
	const float dragRangePixels = GetEffectiveDragRangePixels();
	if (dragRangePixels <= KINDA_SMALL_NUMBER)
	{
		return mPressStartOpenRatio;
	}

	const float ratioDelta = -mTotalPointerDelta.Y / dragRangePixels;
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
	return (1.0f - FMath::Clamp(GetClampedOpenRatio(openRatio), 0.0f, 1.0f)) * GetEffectiveDragRangePixels();
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

EVcardBottomSheetMoveDirection UVcardBottomSheetWidget::CalculatePointerMoveDirection() const
{
	if (mTotalPointerDelta.Y > KINDA_SMALL_NUMBER)
	{
		return EVcardBottomSheetMoveDirection::EClosing;
	}

	if (mTotalPointerDelta.Y < -KINDA_SMALL_NUMBER)
	{
		return EVcardBottomSheetMoveDirection::EOpening;
	}

	return EVcardBottomSheetMoveDirection::ENone;
}

EVcardBottomSheetMoveDirection UVcardBottomSheetWidget::CalculateOpenRatioMoveDirection(float previousRatio, float newRatio) const
{
	if (newRatio > previousRatio + KINDA_SMALL_NUMBER)
	{
		return EVcardBottomSheetMoveDirection::EOpening;
	}

	if (newRatio < previousRatio - KINDA_SMALL_NUMBER)
	{
		return EVcardBottomSheetMoveDirection::EClosing;
	}

	return EVcardBottomSheetMoveDirection::ENone;
}

void UVcardBottomSheetWidget::ApplyOpenRatio(float openRatio, bool bBroadcastChange)
{
	const float previousRatio = mOpenRatio;
	mOpenRatio = GetClampedOpenRatio(openRatio);
	ApplySheetTransform(mOpenRatio);

	if (bBroadcastChange && !FMath::IsNearlyEqual(previousRatio, mOpenRatio))
	{
		UE_LOG(
			LogVdjmVcard,
			Display,
			TEXT("VcardBottomSheet RatioChanged Widget=%s Previous=%.3f New=%.3f TranslationY=%.1f RangePixels=%.1f"),
			*GetNameSafe(this),
			previousRatio,
			mOpenRatio,
			CalculateSheetTranslationY(mOpenRatio),
			GetEffectiveDragRangePixels());
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

	UE_LOG(
		LogVdjmVcard,
		Display,
		TEXT("VcardBottomSheet StateChanged Widget=%s Previous=%s New=%s Open=%.3f"),
		*GetNameSafe(this),
		GetVcardBottomSheetMotionStateText(mMotionState),
		GetVcardBottomSheetMotionStateText(newState),
		mOpenRatio);
	mMotionState = newState;
	OnMotionStateChanged.Broadcast(newState);
	BP_OnMotionStateChanged(newState);
}

void UVcardBottomSheetWidget::SetLastMoveDirection(EVcardBottomSheetMoveDirection newDirection, EVcardBottomSheetMotionState previousState)
{
	if (newDirection == EVcardBottomSheetMoveDirection::ENone || mLastMoveDirection == newDirection)
	{
		return;
	}

	const EVcardBottomSheetMoveDirection previousDirection = mLastMoveDirection;
	UE_LOG(
		LogVdjmVcard,
		Display,
		TEXT("VcardBottomSheet DirectionChanged Widget=%s Previous=%s New=%s PreviousState=%s Open=%.3f"),
		*GetNameSafe(this),
		GetVcardBottomSheetMoveDirectionText(previousDirection),
		GetVcardBottomSheetMoveDirectionText(newDirection),
		GetVcardBottomSheetMotionStateText(previousState),
		mOpenRatio);

	mLastMoveDirection = newDirection;
	OnMoveDirectionChanged.Broadcast(previousDirection, newDirection, previousState);
	BP_OnMoveDirectionChanged(previousDirection, newDirection, previousState);
}

void UVcardBottomSheetWidget::RefreshBoundaryDirection(EVcardBottomSheetMotionState previousState)
{
	const float minRatio = GetClampedOpenRatio(MinOpenRatio);
	const float maxRatio = GetClampedOpenRatio(MaxOpenRatio);
	const float collapsedRatio = GetClampedOpenRatio(CollapsedRatio);
	const float expandedRatio = GetClampedOpenRatio(ExpandedRatio);
	const float boundaryTolerance = FMath::Max(AnimationCompleteTolerance, 0.002f);
	if (mOpenRatio <= minRatio + boundaryTolerance || mOpenRatio <= collapsedRatio + boundaryTolerance)
	{
		SetLastMoveDirection(EVcardBottomSheetMoveDirection::EOpening, previousState);
		return;
	}

	if (mOpenRatio >= maxRatio - boundaryTolerance || mOpenRatio >= expandedRatio - boundaryTolerance)
	{
		SetLastMoveDirection(EVcardBottomSheetMoveDirection::EClosing, previousState);
	}
}

void UVcardBottomSheetWidget::BroadcastSettled(EVcardBottomSheetMoveDirection direction, EVcardBottomSheetMotionState previousState, float finalRatio)
{
	UE_LOG(
		LogVdjmVcard,
		Display,
		TEXT("VcardBottomSheet Settled Widget=%s Direction=%s PreviousState=%s FinalOpen=%.3f TranslationY=%.1f"),
		*GetNameSafe(this),
		GetVcardBottomSheetMoveDirectionText(direction),
		GetVcardBottomSheetMotionStateText(previousState),
		finalRatio,
		CalculateSheetTranslationY(finalRatio));
	OnSettled.Broadcast(direction, previousState, finalRatio);
	BP_OnSettled(direction, previousState, finalRatio);
}
