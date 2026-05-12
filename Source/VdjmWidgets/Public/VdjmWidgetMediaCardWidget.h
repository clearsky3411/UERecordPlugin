#pragma once

#include "Blueprint/UserWidget.h"
#include "VdjmWidgetMediaCarouselTypes.h"
#include "VdjmWidgetMediaCardWidget.generated.h"

class UImage;
class UMediaPlayer;
class UMediaTexture;
class UWidget;
class UVdjmRecordMediaPreviewPlayer;

/**
 * A single media card owned by a carousel.
 *
 * Responsibility:
 * - Own its visual/media state transitions.
 * - Expose BP hooks for active preview, visible thumbnail, hidden release, and empty placeholder.
 *
 * Must not:
 * - Refresh registry or manifest lists.
 * - Decide carousel active index, visible window, or swipe policy.
 * - Create or destroy sibling cards.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMWIDGETS_API UVdjmWidgetMediaCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	bool SetCardSource(const FVdjmWidgetMediaCardSource& cardSource);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void ClearCardSource();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	bool SetCardState(EVdjmWidgetMediaCardState newState);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void EnterEmpty();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void EnterWaiting();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void EnterVisible();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void EnterActive();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void EnterHidden();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void EnterError(const FString& errorReason);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void StartActivePreviewLoop();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void StopPreview(bool bReleaseMediaResources);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	void ReleaseMediaResources();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	bool PrepareActivePreviewVisuals();
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	bool StartManagedActivePreview(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	bool ApplyActiveMediaTextureBrush();
	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Card")
	bool HasValidCardSource() const;
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	bool ValidateCardSource(FString& outErrorReason) const;
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	bool GetVisiblePreviewSource(FString& outSource, EVdjmWidgetMediaSourceKind& outSourceKind, FString& outErrorReason) const;
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card")
	bool GetActivePreviewSource(FString& outSource, EVdjmWidgetMediaSourceKind& outSourceKind, FString& outErrorReason) const;
	UFUNCTION(BlueprintCallable, Category = "VdjmWidgets|Media|Card|Debug")
	void DumpDebugCardState(const FString& reason) const;

	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Card")
	FVdjmWidgetMediaCardSource GetCardSource() const { return mCardSource; }
	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Card")
	EVdjmWidgetMediaCardState GetCardState() const { return mCardState; }
	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Card")
	FString GetLastErrorReason() const { return mLastErrorReason; }
	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Card")
	UMediaPlayer* GetManagedMediaPlayer() const { return mMediaPlayer; }
	UFUNCTION(BlueprintPure, Category = "VdjmWidgets|Media|Card")
	UMediaTexture* GetManagedMediaTexture() const { return mMediaTexture; }

	UPROPERTY(BlueprintAssignable, Category = "VdjmWidgets|Media|Card")
	FVdjmWidgetMediaCardStateDelegate OnCardStateChanged;

protected:
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_OnCardSourceChanged(const FVdjmWidgetMediaCardSource& cardSource);
	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_OnCardStateChanged(EVdjmWidgetMediaCardState previousState, EVdjmWidgetMediaCardState newState);
	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_ShowEmptyCard();
	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_ShowWaitingCard();
	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_ShowVisibleCard();
	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_ShowActiveCard();
	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_ShowHiddenCard();
	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_ShowErrorCard(const FString& errorReason);
	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_StartActivePreviewLoop();
	UFUNCTION(BlueprintImplementableEvent, Category = "VdjmWidgets|Media|Card")
	void BP_StopPreview(bool bReleaseMediaResources);

	void EnsureMediaObjects();
	UImage* ResolveActivePreviewImage() const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "VdjmWidgets|Media|Card")
	TObjectPtr<UWidget> EmptyLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "VdjmWidgets|Media|Card")
	TObjectPtr<UWidget> WaitingLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "VdjmWidgets|Media|Card")
	TObjectPtr<UWidget> VisibleLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "VdjmWidgets|Media|Card")
	TObjectPtr<UWidget> ActiveLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "VdjmWidgets|Media|Card")
	TObjectPtr<UWidget> ErrorLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "VdjmWidgets|Media|Card")
	TObjectPtr<UImage> Image_ActivePreview;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Card")
	bool bAutoManagePreviewMedia = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Card|Debug")
	bool bDebugTraceState = true;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> mMediaPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> mMediaTexture;

	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordMediaPreviewPlayer> mPreviewPlayer;

	UPROPERTY(Transient)
	FVdjmWidgetMediaCardSource mCardSource;

	FString mLastErrorReason;
	EVdjmWidgetMediaCardState mCardState = EVdjmWidgetMediaCardState::EEmpty;
};
