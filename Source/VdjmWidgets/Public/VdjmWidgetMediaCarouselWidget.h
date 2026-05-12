#pragma once

#include "Blueprint/UserWidget.h"
#include "VdjmWidgetMediaCardWidget.h"
#include "VdjmWidgetMediaCarouselWidget.generated.h"

class AVdjmRecordMediaPreviewManagerActor;
class UPanelWidget;

/**
 * Reads media sources for the carousel.
 *
 * Responsibility:
 * - Convert an already prepared registry/manifest source into a card source snapshot.
 *
 * Must not:
 * - Start preview manager init/loading flows.
 * - Create cards, arrange widgets, or play media.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMWIDGETS_API UVdjmWidgetMediaCarouselSource : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel|Source")
	void SetPreviewManager(AVdjmRecordMediaPreviewManagerActor* previewManager);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel|Source")
	bool RefreshSourceSnapshot(TArray<FVdjmWidgetMediaCardSource>& outSources, FString& outErrorReason) const;

	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Carousel|Source")
	AVdjmRecordMediaPreviewManagerActor* GetPreviewManager() const { return mPreviewManager.Get(); }

private:
	TWeakObjectPtr<AVdjmRecordMediaPreviewManagerActor> mPreviewManager;
};

/**
 * Owns card widget creation and reuse for one carousel.
 *
 * Responsibility:
 * - Ensure the requested number of card widgets exists under the host panel.
 *
 * Must not:
 * - Decide active source, card state, layout transform, or media playback.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMWIDGETS_API UVdjmWidgetMediaCarouselCardPool : public UObject
{
	GENERATED_BODY()

public:
	bool EnsureCards(
		UUserWidget* ownerWidget,
		UPanelWidget* hostPanel,
		TSubclassOf<UVdjmWidgetMediaCardWidget> cardWidgetClass,
		int32 desiredCardCount,
		TArray<TObjectPtr<UVdjmWidgetMediaCardWidget>>& cards,
		FString& outErrorReason) const;
};

/**
 * Pure layout calculator for carousel slots.
 *
 * Responsibility:
 * - Convert source count, active index, viewport size, and layout options into slot transforms.
 *
 * Must not:
 * - Create widgets, bind sources, start media, or handle input.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMWIDGETS_API UVdjmWidgetMediaCarouselLayoutPolicy : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel|Layout")
	EVdjmWidgetMediaCarouselLayoutState BuildLayout(
		const TArray<FVdjmWidgetMediaCardSource>& sources,
		int32 activeSourceIndex,
		const FVdjmWidgetMediaCarouselLayoutOptions& layoutOptions,
		FVector2D viewSize,
		TArray<FVdjmWidgetMediaCarouselSlot>& outSlots) const;
};

/**
 * Converts layout slots into target card states.
 *
 * Responsibility:
 * - Decide whether a slot should be Active, Visible, Hidden, or Empty.
 *
 * Must not:
 * - Move widgets, create cards, refresh registry, or open media directly.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMWIDGETS_API UVdjmWidgetMediaCarouselStatePolicy : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel|State")
	EVdjmWidgetMediaCardState ResolveCardState(const FVdjmWidgetMediaCarouselSlot& layoutSlot) const;
};

/**
 * Central input interpreter for the carousel.
 *
 * Responsibility:
 * - Hold raw pointer/card actions until the carousel decides what to do.
 *
 * Must not:
 * - Refresh registry, create cards, or play media.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMWIDGETS_API UVdjmWidgetMediaCarouselInputController : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel|Input")
	void ResetInput();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel|Input")
	void StoreInputPayload(const FVdjmWidgetMediaCarouselInputPayload& inputPayload);

	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Carousel|Input")
	FVdjmWidgetMediaCarouselInputPayload GetLastInputPayload() const { return mLastInputPayload; }

private:
	FVdjmWidgetMediaCarouselInputPayload mLastInputPayload;
};

/**
 * Calculates carousel motion values for swipe/snap.
 *
 * Responsibility:
 * - Own motion lock and future velocity/offset state.
 *
 * Must not:
 * - Decide card media state or source refresh.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class VDJMWIDGETS_API UVdjmWidgetMediaCarouselMotionController : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel|Motion")
	void LockMotion(FName reason);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel|Motion")
	void UnlockMotion(FName reason);

	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Carousel|Motion")
	bool IsMotionLocked() const { return mbMotionLocked; }
	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Carousel|Motion")
	FName GetMotionLockReason() const { return mMotionLockReason; }

private:
	FName mMotionLockReason = NAME_None;
	bool mbMotionLocked = false;
};

/**
 * New carousel manager for VdjmWidgets media cards.
 *
 * Responsibility:
 * - Own source snapshot refresh, card pool, layout, state assignment, input, and motion controllers.
 * - Tell each card what state it should enter.
 *
 * Must not:
 * - Start preview manager init/loading.
 * - Mutate manifest registry schema.
 * - Directly manipulate a card media player; call card public APIs instead.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMWIDGETS_API UVdjmWidgetMediaCarouselWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel")
	bool RefreshCarousel(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel")
	bool ApplySourceSnapshot(const TArray<FVdjmWidgetMediaCardSource>& sources, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel")
	bool SetActiveSourceIndex(int32 activeSourceIndex, bool bRefreshLayout);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel")
	bool SlideNext();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel")
	bool SlidePrevious();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel")
	void SetPreviewManager(AVdjmRecordMediaPreviewManagerActor* previewManager);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Carousel|Debug")
	void DumpDebugCarouselState(const FString& reason) const;

	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Carousel")
	int32 GetActiveSourceIndex() const { return mActiveSourceIndex; }
	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Carousel")
	EVdjmWidgetMediaCarouselLayoutState GetLayoutState() const { return mLayoutState; }
	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Carousel")
	TArray<FVdjmWidgetMediaCardSource> GetSourceSnapshot() const { return mSourceSnapshot; }
	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Carousel")
	TArray<UVdjmWidgetMediaCardWidget*> GetCards() const;

	UPROPERTY(BlueprintAssignable, Category = "VdjmWidgets|Media|Carousel")
	FVdjmWidgetMediaCarouselRefreshDelegate OnCarouselRefreshFinished;

	UPROPERTY(BlueprintAssignable, Category = "VdjmWidgets|Media|Carousel")
	FVdjmWidgetMediaCarouselActiveSourceChangedDelegate OnActiveSourceChanged;

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& inGeometry, const FPointerEvent& inMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& inGeometry, const FPointerEvent& inGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& inGeometry, const FPointerEvent& inGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& inGeometry, const FPointerEvent& inGestureEvent) override;

	bool EnsureControllers();
	bool RebuildVisibleCards(FString& outErrorReason);
	bool ApplyLayoutSlots(const TArray<FVdjmWidgetMediaCarouselSlot>& layoutSlots, FString& outErrorReason);
	void ApplySlotTransforms(const TArray<FVdjmWidgetMediaCarouselSlot>& layoutSlots);
	void ApplyHostChildOrder(const TArray<FVdjmWidgetMediaCarouselSlot>& layoutSlots);
	FVector2D GetCarouselViewSize() const;
	FVector2D GetLayoutDirection() const;
	float GetLayoutSpacing() const;
	int32 ResolveActiveSourceIndex(int32 activeSourceIndex) const;
	bool ApplyResolvedActiveSourceIndex(int32 activeSourceIndex, bool bRefreshLayout);
	void BeginDebugPointerTrace(const FGeometry& inGeometry, const FPointerEvent& pointerEvent, const TCHAR* inputType);
	void UpdateDebugPointerTrace(const FGeometry& inGeometry, const FPointerEvent& pointerEvent, const TCHAR* inputType);
	void EndDebugPointerTrace(const FGeometry& inGeometry, const FPointerEvent& pointerEvent, const TCHAR* inputType);
	bool CommitDebugSwipe(const FVector2D& totalDelta, double elapsedSeconds);
	FReply BuildDebugInputReply() const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "VdjmWidgets|Media|Carousel")
	TObjectPtr<UPanelWidget> CardHostPanel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	TSubclassOf<UVdjmWidgetMediaCardWidget> CardWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	TSubclassOf<UVdjmWidgetMediaCarouselSource> SourceClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	TSubclassOf<UVdjmWidgetMediaCarouselCardPool> CardPoolClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	TSubclassOf<UVdjmWidgetMediaCarouselLayoutPolicy> LayoutPolicyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	TSubclassOf<UVdjmWidgetMediaCarouselStatePolicy> StatePolicyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	TSubclassOf<UVdjmWidgetMediaCarouselInputController> InputControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	TSubclassOf<UVdjmWidgetMediaCarouselMotionController> MotionControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	FVdjmWidgetMediaCarouselLayoutOptions LayoutOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel|Debug")
	bool bDebugTraceInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel|Debug")
	bool bDebugTraceLayout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel|Debug")
	bool bDebugConsumeInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel|Debug", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DebugInputMoveLogIntervalSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel|Debug", meta = (ClampMin = "0.0"))
	float DebugSwipeDistanceThreshold = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel|Debug", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DebugSwipeMinDurationSeconds = 0.03f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVdjmWidgetMediaCarouselSource> mSource;

	UPROPERTY(Transient)
	TObjectPtr<UVdjmWidgetMediaCarouselCardPool> mCardPool;

	UPROPERTY(Transient)
	TObjectPtr<UVdjmWidgetMediaCarouselLayoutPolicy> mLayoutPolicy;

	UPROPERTY(Transient)
	TObjectPtr<UVdjmWidgetMediaCarouselStatePolicy> mStatePolicy;

	UPROPERTY(Transient)
	TObjectPtr<UVdjmWidgetMediaCarouselInputController> mInputController;

	UPROPERTY(Transient)
	TObjectPtr<UVdjmWidgetMediaCarouselMotionController> mMotionController;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVdjmWidgetMediaCardWidget>> mCards;

	UPROPERTY(Transient)
	TArray<FVdjmWidgetMediaCardSource> mSourceSnapshot;

	UPROPERTY(Transient)
	TArray<FVdjmWidgetMediaCarouselSlot> mLayoutSlots;

	FVector2D mDebugPointerStartScreenPosition = FVector2D::ZeroVector;
	FVector2D mDebugPointerLastScreenPosition = FVector2D::ZeroVector;
	double mDebugPointerStartSeconds = 0.0;
	double mDebugPointerLastMoveLogSeconds = 0.0;
	float mTransientSlotOffset = 0.0f;
	EVdjmWidgetMediaCarouselLayoutState mLayoutState = EVdjmWidgetMediaCarouselLayoutState::EEmpty;
	int32 mActiveSourceIndex = INDEX_NONE;
	int32 mDebugPointerMoveCount = 0;
	bool mbDebugPointerTracking = false;
};
