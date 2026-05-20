#pragma once

#include "CoreMinimal.h"
#include "VdjmVcardWidgetBase.h"
#include "VdjmVcardBottomSheetWidget.generated.h"

class UButton;
class UNamedSlot;
class UWidget;

UENUM(BlueprintType)
enum class EVcardBottomSheetMotionState : uint8
{
	EIdle UMETA(DisplayName = "Idle"),
	EPressTracking UMETA(DisplayName = "Press Tracking"),
	EDragging UMETA(DisplayName = "Dragging"),
	EAnimating UMETA(DisplayName = "Animating")
};

UENUM(BlueprintType)
enum class EVcardBottomSheetDragRangeBasis : uint8
{
	EViewportHeight UMETA(DisplayName = "Viewport Height"),
	ESheetPanelHeight UMETA(DisplayName = "Sheet Panel Height"),
	ECustomPixels UMETA(DisplayName = "Custom Pixels")
};

enum class EVcardBottomSheetPointerSource : uint8
{
	ENone,
	ETouch,
	EMouse
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVcardBottomSheetOpenRatioChanged, float, PreviousRatio, float, NewRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVcardBottomSheetMotionStateChanged, EVcardBottomSheetMotionState, NewState);

/**
 * Draggable bottom sheet container for CreatorLobby.ToolContents.
 *
 * Responsibility:
 * - Own open-ratio, tap-toggle, drag tracking, snap, and visual translation.
 * - Expose its visual top position for later layout/editor helpers.
 *
 * Must not:
 * - Know preset/upload/background/motion content rules.
 * - Own the content widget's data. Content is attached through the fixed Content named slot.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardBottomSheetWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|BottomSheet")
	void SetOpenRatio(float openRatio);
	UFUNCTION(BlueprintCallable, Category = "Vcard|BottomSheet")
	void SetOpenRatioImmediate(float openRatio);
	UFUNCTION(BlueprintCallable, Category = "Vcard|BottomSheet")
	void AnimateToOpenRatio(float openRatio);
	UFUNCTION(BlueprintCallable, Category = "Vcard|BottomSheet")
	void ToggleOpenRatio();
	UFUNCTION(BlueprintCallable, Category = "Vcard|BottomSheet")
	void OpenSheet();
	UFUNCTION(BlueprintCallable, Category = "Vcard|BottomSheet")
	void CloseSheet();
	UFUNCTION(BlueprintCallable, Category = "Vcard|BottomSheet")
	void SnapToNearestRatio();

	UFUNCTION(BlueprintPure, Category = "Vcard|BottomSheet")
	float GetOpenRatio() const { return mOpenRatio; }
	UFUNCTION(BlueprintPure, Category = "Vcard|BottomSheet")
	float GetSheetTopScreenY() const;
	UFUNCTION(BlueprintPure, Category = "Vcard|BottomSheet")
	float GetSheetTopNormalized() const;
	UFUNCTION(BlueprintPure, Category = "Vcard|BottomSheet")
	float GetDragRangePixels() const { return GetEffectiveDragRangePixels(); }
	UFUNCTION(BlueprintPure, Category = "Vcard|BottomSheet")
	FName GetContentSlotName() const { return ContentSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|BottomSheet")
	EVcardBottomSheetMotionState GetMotionState() const { return mMotionState; }

	UPROPERTY(BlueprintAssignable, Category = "Vcard|BottomSheet")
	FVcardBottomSheetOpenRatioChanged OnOpenRatioChanged;

	UPROPERTY(BlueprintAssignable, Category = "Vcard|BottomSheet")
	FVcardBottomSheetMotionStateChanged OnMotionStateChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|BottomSheet")
	void BP_OnOpenRatioChanged(float previousRatio, float newRatio);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|BottomSheet")
	void BP_OnMotionStateChanged(EVcardBottomSheetMotionState newState);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|BottomSheet")
	void BP_OnDragStarted(float openRatio);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|BottomSheet")
	void BP_OnDragFinished(float openRatio);

	UFUNCTION()
	void HandleDragHandlePressed();
	UFUNCTION()
	void HandleDragHandleReleased();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|BottomSheet")
	TObjectPtr<UWidget> SheetPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|BottomSheet")
	TObjectPtr<UButton> DragHandle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|BottomSheet")
	TObjectPtr<UNamedSlot> Content;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet")
	FName ContentSlotName = TEXT("Content");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Ratio", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Initial open amount. 1 is fully open, 0 is moved down by the selected drag range."))
	float InitialOpenRatio = 1.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Ratio", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Lowest allowed open amount."))
	float MinOpenRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Ratio", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Highest allowed open amount."))
	float MaxOpenRatio = 1.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Ratio", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Target ratio when the sheet is collapsed."))
	float CollapsedRatio = 0.18f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Ratio", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Target ratio when the sheet is expanded."))
	float ExpandedRatio = 1.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Ratio", meta = (ToolTip = "Release targets. Drag release snaps to the nearest ratio."))
	TArray<float> SnapRatios = { 0.18f, 1.0f };

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Input", meta = (ClampMin = "0"))
	float DragStartThresholdPixels = 10.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Input", meta = (ClampMin = "0.001"))
	float TrackingIntervalSeconds = 0.016f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Motion", meta = (ClampMin = "0"))
	float AnimationInterpSpeed = 18.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Motion", meta = (ClampMin = "0"))
	float AnimationCompleteTolerance = 0.002f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Motion")
	EVcardBottomSheetDragRangeBasis DragRangeBasis = EVcardBottomSheetDragRangeBasis::EViewportHeight;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|BottomSheet|Motion", meta = (ClampMin = "0", ToolTip = "Used only when DragRangeBasis is Custom Pixels."))
	float DragRangePixelsOverride = 0.0f;

private:
	void BindDragHandle();
	void UnbindDragHandle();
	void StartMotionTimer();
	void StopMotionTimerIfIdle();
	void StopMotionTimer();
	void HandleMotionTimer();

	bool SamplePointerScreenPosition(FVector2D& outScreenPosition) const;
	bool SamplePressedPointerScreenPosition(FVector2D& outScreenPosition, EVcardBottomSheetPointerSource& outPointerSource) const;
	bool SampleTrackedPressedPointerScreenPosition(FVector2D& outScreenPosition) const;
	void UpdatePointerDeltas(const FVector2D& screenPosition);
	bool ShouldStartDrag() const;
	void BeginSheetDrag();
	void EndSheetDrag(bool bWasCancelled);
	void FinishPointerInteraction(bool bWasCancelled);

	float CalculateOpenRatioFromDrag() const;
	float CalculateNearestSnapRatio(float openRatio) const;
	float CalculateAnimatedOpenRatio(float deltaSeconds) const;
	float CalculateSheetTranslationY(float openRatio) const;
	float GetViewportHeightPixels() const;
	float GetEffectiveDragRangePixels() const;
	float GetClampedOpenRatio(float openRatio) const;
	void ApplyOpenRatio(float openRatio, bool bBroadcastChange);
	void ApplySheetTransform(float openRatio);
	void SetMotionState(EVcardBottomSheetMotionState newState);

	FTimerHandle mMotionTimerHandle;
	FVector2D mPressStartScreenPosition = FVector2D::ZeroVector;
	FVector2D mLastPointerScreenPosition = FVector2D::ZeroVector;
	FVector2D mCurrentPointerScreenPosition = FVector2D::ZeroVector;
	FVector2D mFramePointerDelta = FVector2D::ZeroVector;
	FVector2D mTotalPointerDelta = FVector2D::ZeroVector;
	float mOpenRatio = 1.0f;
	float mPressStartOpenRatio = 1.0f;
	float mTargetOpenRatio = 1.0f;
	float mLastMotionTimeSeconds = 0.0f;
	bool mbDragHandleBound = false;
	bool mbHasAnimationTarget = false;
	EVcardBottomSheetPointerSource mPointerSource = EVcardBottomSheetPointerSource::ENone;
	EVcardBottomSheetMotionState mMotionState = EVcardBottomSheetMotionState::EIdle;
};
