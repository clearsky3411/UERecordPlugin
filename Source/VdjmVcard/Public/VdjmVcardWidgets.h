#pragma once

#include "CoreMinimal.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardWidgetBase.h"
#include "VdjmVcardWidgets.generated.h"

class UVcardDescriptorBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVcardToolIdDelegate, FName, ToolId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVcardOptionItemIdDelegate, FName, ItemId);

/**
 * Root widget for V-card screens.
 *
 * Responsibility:
 * - Own top-level screen and modal slots.
 * - Apply descriptors to those slots when a flow asks for a screen/modal transition.
 *
 * Must not:
 * - Know concrete background or motion item rules.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardRootWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Root")
	bool OpenScreenDescriptor(UVcardDescriptorBase* descriptor, UObject* previousStateObject, UObject* savedStateObject, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Root")
	bool OpenModalDescriptor(UVcardDescriptorBase* descriptor, UObject* previousStateObject, UObject* savedStateObject, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Root")
	bool CloseModal(FString& outErrorReason);

	UFUNCTION(BlueprintPure, Category = "Vcard|Root")
	FName GetStageSlotName() const { return StageSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Root")
	FName GetModalSlotName() const { return ModalSlotName; }

protected:
	FVcardDescriptorApplyRequest BuildApplyRequest(UObject* previousStateObject, UObject* savedStateObject) const;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Root")
	FName StageSlotName = TEXT("Stage");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Root")
	FName ModalSlotName = TEXT("Modal");
};

/**
 * Base class for V-card screens.
 *
 * Responsibility:
 * - Receive screen open/close notifications and apply composition descriptors.
 *
 * Must not:
 * - Force a fixed screen layout beyond named slot conventions.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardScreenWidgetBase : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Screen")
	bool ApplyCompositionDescriptor(UVcardDescriptorBase* descriptor, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Screen")
	void NotifyScreenOpened(FName screenId);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Screen")
	void NotifyScreenClosed(FName screenId);

	UFUNCTION(BlueprintPure, Category = "Vcard|Screen")
	FName GetCurrentScreenId() const { return mCurrentScreenId; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Screen")
	void BP_OnScreenOpened(FName screenId);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Screen")
	void BP_OnScreenClosed(FName screenId);

private:
	UPROPERTY(Transient)
	FName mCurrentScreenId = NAME_None;
};

/**
 * Preview lobby screen shell.
 *
 * Responsibility:
 * - Provide stable slot names for top, carousel, and bottom lobby controls.
 *
 * Must not:
 * - Own carousel internals.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardPreviewLobbyWidget : public UVcardScreenWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Vcard|PreviewLobby")
	FName GetTopSlotName() const { return TopSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|PreviewLobby")
	FName GetCarouselSlotName() const { return CarouselSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|PreviewLobby")
	FName GetBottomSlotName() const { return BottomSlotName; }

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|PreviewLobby")
	FName TopSlotName = TEXT("Top");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|PreviewLobby")
	FName CarouselSlotName = TEXT("Carousel");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|PreviewLobby")
	FName BottomSlotName = TEXT("Bottom");
};

/**
 * Background editor screen shell.
 *
 * Responsibility:
 * - Provide named regions for top controls, left tools, stage, and bottom sheet.
 *
 * Must not:
 * - Hard-code specific preset/upload panels.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardBackgroundWidget : public UVcardScreenWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Background")
	bool ApplyToolDescriptor(UVcardDescriptorBase* descriptor, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Background")
	bool ApplyBottomDescriptor(UVcardDescriptorBase* descriptor, FVcardDescriptorApplyResult& outResult);

	UFUNCTION(BlueprintPure, Category = "Vcard|Background")
	FName GetTopSlotName() const { return TopSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Background")
	FName GetLeftSlotName() const { return LeftSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Background")
	FName GetStageSlotName() const { return StageSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Background")
	FName GetBottomSlotName() const { return BottomSlotName; }

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Background")
	FName TopSlotName = TEXT("Top");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Background")
	FName LeftSlotName = TEXT("Left");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Background")
	FName StageSlotName = TEXT("Stage");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Background")
	FName BottomSlotName = TEXT("Bottom");
};

/**
 * Draggable bottom sheet shell.
 *
 * Responsibility:
 * - Store open ratio/snap policy and attach header/content widgets from descriptors.
 *
 * Must not:
 * - Implement concrete background preset logic.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardBackgroundBottomWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Bottom")
	bool ApplyBottomSheetDescriptor(const FVcardBottomSheetDescriptor& bottomSheetDescriptor, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Bottom")
	void SetOpenRatio(float openRatio);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Bottom")
	void ToggleOpenRatio();

	UFUNCTION(BlueprintPure, Category = "Vcard|Bottom")
	float GetOpenRatio() const { return mOpenRatio; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Bottom")
	FName GetHeaderSlotName() const { return HeaderSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Bottom")
	FName GetContentSlotName() const { return ContentSlotName; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Bottom")
	void BP_OnOpenRatioChanged(float previousRatio, float newRatio);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Bottom")
	void BP_OnBottomSheetDescriptorApplied(const FVcardBottomSheetDescriptor& bottomSheetDescriptor);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Bottom")
	FName HeaderSlotName = TEXT("Header");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Bottom")
	FName ContentSlotName = TEXT("Content");

private:
	UPROPERTY(Transient)
	FVcardBottomSheetDescriptor mCurrentBottomSheetDescriptor;

	UPROPERTY(Transient)
	float mOpenRatio = 1.0f;
};

/**
 * Shared V-card top bar shell.
 *
 * Responsibility:
 * - Keep title and command signal ids for back/primary actions.
 *
 * Must not:
 * - Perform navigation by itself.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardTopBarWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|TopBar")
	void SetTopBarText(FText titleText, FText primaryText);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TopBar")
	void SetTopBarSignals(FName backSignalTag, FName primarySignalTag);

	UFUNCTION(BlueprintPure, Category = "Vcard|TopBar")
	FText GetTitleText() const { return mTitleText; }
	UFUNCTION(BlueprintPure, Category = "Vcard|TopBar")
	FText GetPrimaryText() const { return mPrimaryText; }
	UFUNCTION(BlueprintPure, Category = "Vcard|TopBar")
	FName GetBackSignalTag() const { return mBackSignalTag; }
	UFUNCTION(BlueprintPure, Category = "Vcard|TopBar")
	FName GetPrimarySignalTag() const { return mPrimarySignalTag; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|TopBar")
	void BP_OnTopBarTextChanged(const FText& titleText, const FText& primaryText);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|TopBar")
	void BP_OnTopBarSignalsChanged(FName backSignalTag, FName primarySignalTag);

private:
	UPROPERTY(Transient)
	FText mTitleText;

	UPROPERTY(Transient)
	FText mPrimaryText;

	UPROPERTY(Transient)
	FName mBackSignalTag = NAME_None;

	UPROPERTY(Transient)
	FName mPrimarySignalTag = NAME_None;
};

/**
 * Modal layer shell.
 *
 * Responsibility:
 * - Attach and clear modal content in a named slot.
 *
 * Must not:
 * - Decide when modal flows begin or end.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardModalLayerWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Modal")
	bool OpenModalContent(const FVcardWidgetAttachDescriptor& attachmentDescriptor, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Modal")
	bool CloseModalContent(FString& outErrorReason);

	UFUNCTION(BlueprintPure, Category = "Vcard|Modal")
	FName GetContentSlotName() const { return ContentSlotName; }

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|Modal")
	FName ContentSlotName = TEXT("Content");
};

UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardToolRailWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Tool")
	void SetTools(const TArray<FVcardToolDescriptor>& toolDescriptors);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Tool")
	bool SelectTool(FName toolId);

	UFUNCTION(BlueprintPure, Category = "Vcard|Tool")
	TArray<FVcardToolDescriptor> GetTools() const { return mToolDescriptors; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Tool")
	FName GetSelectedToolId() const { return mSelectedToolId; }

	UPROPERTY(BlueprintAssignable, Category = "Vcard|Tool")
	FVcardToolIdDelegate OnToolSelected;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Tool")
	void BP_OnToolsChanged(const TArray<FVcardToolDescriptor>& toolDescriptors);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Tool")
	void BP_OnToolSelected(FName toolId);

private:
	UPROPERTY(Transient)
	TArray<FVcardToolDescriptor> mToolDescriptors;

	UPROPERTY(Transient)
	FName mSelectedToolId = NAME_None;
};

UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardOptionGridWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Option")
	void SetOptionItems(const TArray<FVcardOptionItemData>& optionItems);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Option")
	bool SelectOptionItem(FName itemId);

	UFUNCTION(BlueprintPure, Category = "Vcard|Option")
	TArray<FVcardOptionItemData> GetOptionItems() const { return mOptionItems; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Option")
	FName GetSelectedItemId() const { return mSelectedItemId; }

	UPROPERTY(BlueprintAssignable, Category = "Vcard|Option")
	FVcardOptionItemIdDelegate OnOptionItemSelected;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Option")
	void BP_OnOptionItemsChanged(const TArray<FVcardOptionItemData>& optionItems);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Option")
	void BP_OnOptionItemSelected(FName itemId);

private:
	UPROPERTY(Transient)
	TArray<FVcardOptionItemData> mOptionItems;

	UPROPERTY(Transient)
	FName mSelectedItemId = NAME_None;
};

UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardOptionItemWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|Option")
	void SetOptionItemData(const FVcardOptionItemData& optionItemData, bool bSelected);
	UFUNCTION(BlueprintCallable, Category = "Vcard|Option")
	void SetSelected(bool bSelected);

	UFUNCTION(BlueprintPure, Category = "Vcard|Option")
	FVcardOptionItemData GetOptionItemData() const { return mOptionItemData; }
	UFUNCTION(BlueprintPure, Category = "Vcard|Option")
	bool IsSelected() const { return mbSelected; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Option")
	void BP_OnOptionItemDataChanged(const FVcardOptionItemData& optionItemData);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|Option")
	void BP_OnSelectedChanged(bool bSelected);

private:
	UPROPERTY(Transient)
	FVcardOptionItemData mOptionItemData;

	UPROPERTY(Transient)
	bool mbSelected = false;
};
