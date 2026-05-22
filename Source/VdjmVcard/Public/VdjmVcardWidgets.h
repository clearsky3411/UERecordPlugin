#pragma once

#include "CoreMinimal.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardWidgetBase.h"
#include "VdjmVcardWidgets.generated.h"

class UPanelWidget;
class UVcardDescriptorBase;
class UVcardDescriptorRegistryDataAsset;

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
 * Creator lobby screen shell.
 *
 * Responsibility:
 * - Provide stable slot names for creator top controls, tool buttons, and tool content area.
 *
 * Must not:
 * - Hard-code concrete background/motion/text editor panels.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardCreatorLobbyWidget : public UVcardScreenWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Vcard|CreatorLobby")
	FName GetTopSlotName() const { return TopSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|CreatorLobby")
	FName GetToolsSlotName() const { return ToolsSlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|CreatorLobby")
	FName GetToolContentsSlotName() const { return ToolContentsSlotName; }

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|CreatorLobby")
	FName TopSlotName = TEXT("Top");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|CreatorLobby")
	FName ToolsSlotName = TEXT("Tools");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|CreatorLobby")
	FName ToolContentsSlotName = TEXT("ToolContents");
};

/**
 * Tool option content shell.
 *
 * Responsibility:
 * - Provide optional tool option-name region and main tool-content region.
 *
 * Must not:
 * - Assume every tool uses option names. Tools can bypass this shell and fill CreatorLobby.ToolContents directly.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardToolOptContentWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Vcard|ToolOptContent")
	FName GetToolOptNameArraySlotName() const { return ToolOptNameArraySlotName; }
	UFUNCTION(BlueprintPure, Category = "Vcard|ToolOptContent")
	FName GetToolOptContentMainSlotName() const { return ToolOptContentMainSlotName; }

protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|ToolOptContent")
	FName ToolOptNameArraySlotName = TEXT("toolOptNameArray");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|ToolOptContent")
	FName ToolOptContentMainSlotName = TEXT("toolOptContentMain");
};

/**
 * Group widget that arranges widgets generated by descriptors.
 *
 * Responsibility:
 * - Run a descriptor in create-only mode and attach the generated widgets to its own panel.
 * - Let Blueprint reorder/filter generated widgets before final panel attachment.
 *
 * Must not:
 * - Decide domain behavior such as background or motion selection.
 * - Store persistent editor state.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API UVcardDescriptorGroupWidget : public UVcardWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vcard|DescriptorGroup")
	bool ApplyFactoryDescriptor(UVcardDescriptorBase* descriptor, UObject* payloadData, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|DescriptorGroup")
	bool ApplyFactoryDescriptorFromRegistry(UVcardDescriptorRegistryDataAsset* descriptorRegistryDataAsset, FName descriptorKey, UObject* payloadData, FVcardDescriptorApplyResult& outResult);
	UFUNCTION(BlueprintCallable, Category = "Vcard|DescriptorGroup")
	bool AttachGeneratedWidgets(const TArray<UUserWidget*>& generatedWidgets, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|DescriptorGroup")
	bool ClearGeneratedWidgets(FString& outErrorReason);

	UFUNCTION(BlueprintPure, Category = "Vcard|DescriptorGroup")
	TArray<UUserWidget*> GetGeneratedWidgets() const;
	UFUNCTION(BlueprintPure, Category = "Vcard|DescriptorGroup")
	FName GetGeneratedWidgetPanelName() const { return GeneratedWidgetPanelName; }

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Vcard|DescriptorGroup")
	void ResolveGeneratedWidgets(const TArray<UUserWidget*>& sourceWidgets, UObject* payloadData, TArray<UUserWidget*>& outResolvedWidgets);
	UFUNCTION(BlueprintImplementableEvent, Category = "Vcard|DescriptorGroup")
	void BP_OnGeneratedWidgetsChanged(const TArray<UUserWidget*>& generatedWidgets);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vcard|DescriptorGroup")
	TObjectPtr<UPanelWidget> GeneratedWidgetPanel;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|DescriptorGroup")
	FName GeneratedWidgetPanelName = TEXT("GeneratedWidgetPanel");

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|DescriptorGroup")
	bool bClearPanelBeforeApply = true;

private:
	bool ApplyFactoryDescriptorInternal(UVcardDescriptorBase* descriptor, UVcardDescriptorRegistryDataAsset* descriptorRegistryDataAsset, UObject* payloadData, FVcardDescriptorApplyResult& outResult);
	bool FindGeneratedWidgetPanel(UPanelWidget*& outPanelWidget, FString& outErrorReason) const;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UUserWidget>> mGeneratedWidgets;
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
UCLASS(BlueprintType, Blueprintable, meta = (DeprecatedNode, DeprecationMessage = "Use UVcardCreatorLobbyWidget. This class keeps old Background WBP parents loadable during migration."))
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
