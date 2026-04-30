#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "VdjmRecorderCore.h"
#include "VdjmRecordMediaPreview.generated.h"

class UButton;
class UImage;
class UMediaPlayer;
class UMediaTexture;
class UOverlay;
class UVdjmRecordMediaPreviewWidget;
class AVdjmRecordMediaPreviewManagerActor;

UENUM(BlueprintType)
enum class EVdjmRecordMediaPreviewSlotState : uint8
{
	EEmpty UMETA(DisplayName = "Empty"),
	EHidden UMETA(DisplayName = "Hidden"),
	EStatic UMETA(DisplayName = "Static"),
	EPrepared UMETA(DisplayName = "Prepared"),
	EPreviewing UMETA(DisplayName = "Previewing"),
	EStopped UMETA(DisplayName = "Stopped"),
	EReleased UMETA(DisplayName = "Released"),
	EDeleted UMETA(DisplayName = "Deleted"),
	EFailed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EVdjmRecordMediaPreviewInputEvent : uint8
{
	ERegistered UMETA(DisplayName = "Registered"),
	EUnregistered UMETA(DisplayName = "Unregistered"),
	EHovered UMETA(DisplayName = "Hovered"),
	EUnhovered UMETA(DisplayName = "Unhovered"),
	EPressed UMETA(DisplayName = "Pressed"),
	EReleased UMETA(DisplayName = "Released"),
	EClicked UMETA(DisplayName = "Clicked"),
	EPreviewStarted UMETA(DisplayName = "Preview Started"),
	EPreviewStopped UMETA(DisplayName = "Preview Stopped"),
	EPreviewFailed UMETA(DisplayName = "Preview Failed"),
	EManifestChanged UMETA(DisplayName = "Manifest Changed"),
	EStateChanged UMETA(DisplayName = "State Changed")
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordMediaPreviewSlot : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	void ClearSlot();
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	void SetRegistryEntry(const FVdjmRecordMediaRegistryEntry& registryEntry, int32 sourceRegistryIndex);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	void SetManifest(UVdjmRecordMediaManifest* manifest);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	void SetSlotState(EVdjmRecordMediaPreviewSlotState newState);

	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Slot")
	bool HasRegistryEntry() const { return SourceRegistryIndex != INDEX_NONE || not RegistryEntry.RecordId.IsEmpty(); }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Slot")
	UVdjmRecordMediaManifest* GetManifest() const { return Manifest; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Slot")
	FString GetRecordId() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Slot")
	FString GetOutputFilePath() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Slot")
	FString GetMetadataFilePath() const;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview|Slot")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview|Slot")
	int32 SourceRegistryIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview|Slot")
	FVdjmRecordMediaRegistryEntry RegistryEntry;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Recorder|MediaPreview|Slot")
	TObjectPtr<UVdjmRecordMediaManifest> Manifest;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview|Slot")
	EVdjmRecordMediaPreviewSlotState SlotState = EVdjmRecordMediaPreviewSlotState::EEmpty;
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordMediaPreviewNotifyPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	int32 PreviewIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	int32 PreviousPreviewIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	int32 SourceRegistryIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	EVdjmRecordMediaPreviewInputEvent InputEventType = EVdjmRecordMediaPreviewInputEvent::EStateChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	EVdjmRecordMediaPreviewSlotState PreviousState = EVdjmRecordMediaPreviewSlotState::EEmpty;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	EVdjmRecordMediaPreviewSlotState CurrentState = EVdjmRecordMediaPreviewSlotState::EEmpty;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	FName PreviewPolicyTag = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	FString RecordId;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	FString MetadataFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	FString OutputFilePath;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	FString PreviewSource;

	UPROPERTY(BlueprintReadOnly, Category = "Recorder|MediaPreview")
	FVdjmRecordMediaRegistryEntry RegistryEntry;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Recorder|MediaPreview")
	TObjectPtr<UVdjmRecordMediaManifest> Manifest = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Recorder|MediaPreview")
	TObjectPtr<UVdjmRecordMediaPreviewSlot> PreviewSlot = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Recorder|MediaPreview")
	TObjectPtr<UVdjmRecordMediaPreviewWidget> PreviewWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Recorder|MediaPreview")
	TObjectPtr<UVdjmRecordMediaPreviewWidget> PreviousPreviewWidget = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVdjmRecordMediaPreviewNotifyDelegate, const FVdjmRecordMediaPreviewNotifyPayload&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVdjmRecordMediaPreviewSimpleDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVdjmRecordMediaPreviewStoreRefreshDelegate, bool, bSuccess, const FString&, ErrorReason);

UCLASS(BlueprintType, Blueprintable)
class VDJMRECORDER_API AVdjmRecordMediaPreviewManagerActor : public AActor
{
	GENERATED_BODY()

public:
	AVdjmRecordMediaPreviewManagerActor();

	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview", meta = (WorldContext = "worldContextObject"))
	static AVdjmRecordMediaPreviewManagerActor* FindMediaPreviewManagerActor(UObject* worldContextObject);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview", meta = (WorldContext = "worldContextObject"))
	static AVdjmRecordMediaPreviewManagerActor* FindOrSpawnMediaPreviewManagerActor(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Store")
	bool RefreshPreviewStoreFromDisk(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Carousel")
	bool ApplyCarouselWindow(int32 centerSourceIndex, int32 slotCount, int32 activeSlotIndex);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Carousel")
	bool SetCenterSourceIndex(int32 centerSourceIndex);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Carousel")
	bool SlideNext();
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Carousel")
	bool SlidePrevious();
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	bool RegisterPreviewWidget(UVdjmRecordMediaPreviewWidget* previewWidget);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	bool UnregisterPreviewWidget(UVdjmRecordMediaPreviewWidget* previewWidget);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	bool StartPreviewSlot(int32 slotIndex);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	bool StopPreviewSlot(int32 slotIndex, bool bCloseMedia);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	bool HideSlot(int32 slotIndex);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	bool ReleaseSlot(int32 slotIndex);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	bool DeleteSlot(int32 slotIndex);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Slot")
	void StopAllPreviews(bool bCloseMedia);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Notify")
	void NotifyPreviewWidgetInput(UVdjmRecordMediaPreviewWidget* previewWidget, EVdjmRecordMediaPreviewInputEvent inputEventType);

	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Store")
	TArray<FVdjmRecordMediaRegistryEntry> GetPreviewRegistryEntries() const { return mRegistryEntries; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Slot")
	TArray<UVdjmRecordMediaPreviewSlot*> GetPreviewSlots() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Slot")
	UVdjmRecordMediaPreviewSlot* GetPreviewSlot(int32 slotIndex) const;
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Carousel")
	int32 GetCenterSourceIndex() const { return mCenterSourceIndex; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Carousel")
	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|State")
	FString BuildPreviewStateJson() const;
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|State")
	bool RestorePreviewStateJson(const FString& previewStateJson, FString& outErrorReason);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|MediaPreview|Event")
	FVdjmRecordMediaPreviewNotifyDelegate OnPreviewNotify;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|MediaPreview|Event")
	FVdjmRecordMediaPreviewNotifyDelegate OnPreviewStarted;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|MediaPreview|Event")
	FVdjmRecordMediaPreviewNotifyDelegate OnPreviewStopped;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|MediaPreview|Event")
	FVdjmRecordMediaPreviewNotifyDelegate OnPreviewFailed;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|MediaPreview|Event")
	FVdjmRecordMediaPreviewStoreRefreshDelegate OnPreviewStoreRefreshFinished;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|MediaPreview|Event")
	FVdjmRecordMediaPreviewSimpleDelegate OnPreviewStoreRefreshStarted;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|MediaPreview|Event")
	FVdjmRecordMediaPreviewSimpleDelegate OnPreviewRegistryChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview|Config")
	bool bAutoRefreshStoreOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview|Config")
	bool bAutoStartCenterPreview = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview|Config", meta = (ClampMin = "1", ClampMax = "32"))
	int32 SlotCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview|Config", meta = (ClampMin = "0", ClampMax = "31"))
	int32 ActiveSlotIndex = 2;

protected:
	UFUNCTION()
	void HandleWidgetHovered();
	UFUNCTION()
	void HandleWidgetUnhovered();
	UFUNCTION()
	void HandleWidgetPressed();
	UFUNCTION()
	void HandleWidgetReleased();
	UFUNCTION()
	void HandleWidgetClicked();

private:
	bool RegisterSelfAsPrimitive();
	void EnsureSlotCount(int32 desiredSlotCount);
	UVdjmRecordMediaPreviewSlot* FindSlotByWidget(const UVdjmRecordMediaPreviewWidget* previewWidget) const;
	UVdjmRecordMediaPreviewWidget* FindWidgetBySlotIndex(int32 slotIndex) const;
	int32 FindWidgetIndex(UVdjmRecordMediaPreviewWidget* previewWidget) const;
	FVdjmRecordMediaPreviewNotifyPayload BuildPayload(
		UVdjmRecordMediaPreviewWidget* previewWidget,
		UVdjmRecordMediaPreviewSlot* previewSlot,
		EVdjmRecordMediaPreviewInputEvent inputEventType,
		EVdjmRecordMediaPreviewSlotState previousState) const;
	void BroadcastPayload(const FVdjmRecordMediaPreviewNotifyPayload& payload);
	void SetSlotStateAndBroadcast(
		UVdjmRecordMediaPreviewWidget* previewWidget,
		UVdjmRecordMediaPreviewSlot* previewSlot,
		EVdjmRecordMediaPreviewSlotState newState,
		EVdjmRecordMediaPreviewInputEvent inputEventType);
	bool AssignEntryToSlot(int32 slotIndex, int32 sourceRegistryIndex);
	void CompactWidgetRefs();

	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordMetadataStore> mMetadataStore;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVdjmRecordMediaPreviewSlot>> mPreviewSlots;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UVdjmRecordMediaPreviewWidget>> mPreviewWidgets;

	UPROPERTY(Transient)
	TArray<FVdjmRecordMediaRegistryEntry> mRegistryEntries;

	int32 mCenterSourceIndex = INDEX_NONE;
	int32 mPreviewingSlotIndex = INDEX_NONE;
	int32 mLastPreviewIndex = INDEX_NONE;
};

UCLASS(BlueprintType, Blueprintable)
class VDJMRECORDER_API UVdjmRecordMediaPreviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	bool SetRegistryEntry(const FVdjmRecordMediaRegistryEntry& registryEntry, int32 sourceRegistryIndex);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	bool SetManifest(UVdjmRecordMediaManifest* manifest);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	bool BindPreviewSlot(UVdjmRecordMediaPreviewSlot* previewSlot);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	bool StartPreview(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	void StopPreview(bool bCloseMedia);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	void SetHiddenByPreviewManager(bool bHidden);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	bool PreparePreviewVisuals();
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview")
	void ReleasePreviewVisuals(bool bCloseMedia);

	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	int32 GetPreviewIndex() const { return PreviewIndex; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	FName GetPreviewPolicyTag() const { return PreviewPolicyTag; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	UVdjmRecordMediaPreviewSlot* GetPreviewSlot() const { return mPreviewSlot; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	UVdjmRecordMediaManifest* GetPreviewManifest() const;
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	UMediaPlayer* GetPreviewMediaPlayer() const { return mMediaPlayer; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	UMediaTexture* GetPreviewMediaTexture() const { return mMediaTexture; }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	AVdjmRecordMediaPreviewManagerActor* GetPreviewManager() const { return mPreviewManager.Get(); }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview")
	bool ShouldStartPreviewOnClick() const { return bStartPreviewOnClick; }

	void SetPreviewIndex(int32 previewIndex) { PreviewIndex = previewIndex; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleInputHovered();
	UFUNCTION()
	void HandleInputUnhovered();
	UFUNCTION()
	void HandleInputPressed();
	UFUNCTION()
	void HandleInputReleased();
	UFUNCTION()
	void HandleInputClicked();

	void EnsureInternalWidgets();
	void EnsureMediaObjects();
	bool EnsurePreviewManager();
	void RegisterToPreviewManager();
	void UnregisterFromPreviewManager();
	void ApplyMediaTextureBrush();
	void ApplyInputButtonStyle();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Recorder|MediaPreview")
	TObjectPtr<UOverlay> PreviewOverlay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Recorder|MediaPreview")
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Recorder|MediaPreview")
	TObjectPtr<UButton> InputButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview")
	bool bAutoRegisterToPreviewManager = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview")
	bool bAutoCreateInternalWidgets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview")
	bool bStartPreviewOnClick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview")
	FName PreviewPolicyTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview")
	int32 DesiredPreviewIndex = INDEX_NONE;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> mMediaPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> mMediaTexture;

	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordMediaPreviewPlayer> mPreviewPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordMediaPreviewSlot> mPreviewSlot;

	TWeakObjectPtr<AVdjmRecordMediaPreviewManagerActor> mPreviewManager;
	int32 PreviewIndex = INDEX_NONE;
};

UCLASS(BlueprintType, Blueprintable)
class VDJMRECORDER_API UVdjmRecordMediaPreviewCarouselWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Carousel")
	bool RefreshCarousel(FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Carousel")
	bool SetCenterSourceIndex(int32 centerSourceIndex);
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Carousel")
	bool SlideNext();
	UFUNCTION(BlueprintCallable, Category = "Recorder|MediaPreview|Carousel")
	bool SlidePrevious();

	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Carousel")
	AVdjmRecordMediaPreviewManagerActor* GetPreviewManager() const { return mPreviewManager.Get(); }
	UFUNCTION(BlueprintPure, Category = "Recorder|MediaPreview|Carousel")
	int32 GetCenterSourceIndex() const;

protected:
	virtual void NativeConstruct() override;

	bool EnsurePreviewManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview|Carousel")
	bool bRefreshStoreOnConstruct = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview|Carousel", meta = (ClampMin = "1", ClampMax = "32"))
	int32 SlotCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview|Carousel", meta = (ClampMin = "0", ClampMax = "31"))
	int32 ActiveSlotIndex = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|MediaPreview|Carousel")
	int32 InitialCenterSourceIndex = INDEX_NONE;

private:
	TWeakObjectPtr<AVdjmRecordMediaPreviewManagerActor> mPreviewManager;
};
