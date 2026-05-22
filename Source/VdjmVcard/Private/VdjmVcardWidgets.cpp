#include "VdjmVcardWidgets.h"

#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"
#include "VdjmVcardDescriptorApplier.h"
#include "VdjmVcardDescriptorBase.h"
#include "VdjmVcardDescriptorRegistryDataAsset.h"

bool UVcardRootWidget::OpenScreenDescriptor(UVcardDescriptorBase* descriptor, UObject* previousStateObject, UObject* savedStateObject, FVcardDescriptorApplyResult& outResult)
{
	if (!IsValid(descriptor))
	{
		outResult = FVcardDescriptorApplyResult();
		outResult.ErrorReason = TEXT("Screen descriptor is invalid.");
		return false;
	}

	FVcardDescriptorApplyRequest request = BuildApplyRequest(previousStateObject, savedStateObject);
	return descriptor->ApplyToWidget(request, outResult);
}

bool UVcardRootWidget::OpenModalDescriptor(UVcardDescriptorBase* descriptor, UObject* previousStateObject, UObject* savedStateObject, FVcardDescriptorApplyResult& outResult)
{
	if (!IsValid(descriptor))
	{
		outResult = FVcardDescriptorApplyResult();
		outResult.ErrorReason = TEXT("Modal descriptor is invalid.");
		return false;
	}

	FVcardDescriptorApplyRequest request = BuildApplyRequest(previousStateObject, savedStateObject);
	return descriptor->ApplyToWidget(request, outResult);
}

bool UVcardRootWidget::CloseModal(FString& outErrorReason)
{
	outErrorReason.Reset();

	UNamedSlot* namedSlot = nullptr;
	if (UVcardDescriptorApplier::FindNamedSlot(this, ModalSlotName, namedSlot))
	{
		namedSlot->SetContent(nullptr);
		return true;
	}

	UPanelWidget* panelWidget = nullptr;
	if (UVcardDescriptorApplier::FindPanelWidget(this, ModalSlotName, panelWidget))
	{
		panelWidget->ClearChildren();
		return true;
	}

	outErrorReason = FString::Printf(TEXT("Modal slot '%s' was not found."), *ModalSlotName.ToString());
	return false;
}

FVcardDescriptorApplyRequest UVcardRootWidget::BuildApplyRequest(UObject* previousStateObject, UObject* savedStateObject) const
{
	FVcardDescriptorApplyRequest request;
	request.NamedSlotHostWidget = const_cast<UVcardRootWidget*>(this);
	request.PayloadData = IsValid(savedStateObject) ? savedStateObject : previousStateObject;
	request.bAllowCreate = true;
	return request;
}

bool UVcardScreenWidgetBase::ApplyCompositionDescriptor(UVcardDescriptorBase* descriptor, FVcardDescriptorApplyResult& outResult)
{
	if (!IsValid(descriptor))
	{
		outResult = FVcardDescriptorApplyResult();
		outResult.ErrorReason = TEXT("Composition descriptor is invalid.");
		return false;
	}

	FVcardDescriptorApplyRequest request;
	request.NamedSlotHostWidget = this;
	request.bAllowCreate = true;
	return descriptor->ApplyToWidget(request, outResult);
}

void UVcardScreenWidgetBase::NotifyScreenOpened(FName screenId)
{
	mCurrentScreenId = screenId;
	BP_OnScreenOpened(screenId);
}

void UVcardScreenWidgetBase::NotifyScreenClosed(FName screenId)
{
	if (mCurrentScreenId == screenId)
	{
		mCurrentScreenId = NAME_None;
	}

	BP_OnScreenClosed(screenId);
}

bool UVcardBackgroundWidget::ApplyToolDescriptor(UVcardDescriptorBase* descriptor, FVcardDescriptorApplyResult& outResult)
{
	return ApplyCompositionDescriptor(descriptor, outResult);
}

bool UVcardBackgroundWidget::ApplyBottomDescriptor(UVcardDescriptorBase* descriptor, FVcardDescriptorApplyResult& outResult)
{
	return ApplyCompositionDescriptor(descriptor, outResult);
}

bool UVcardDescriptorGroupWidget::ApplyFactoryDescriptor(UVcardDescriptorBase* descriptor, UObject* payloadData, FVcardDescriptorApplyResult& outResult)
{
	return ApplyFactoryDescriptorInternal(descriptor, GetDescriptorRegistry(), payloadData, outResult);
}

bool UVcardDescriptorGroupWidget::ApplyFactoryDescriptorFromRegistry(
	UVcardDescriptorRegistryDataAsset* descriptorRegistryDataAsset,
	FName descriptorKey,
	UObject* payloadData,
	FVcardDescriptorApplyResult& outResult)
{
	outResult = FVcardDescriptorApplyResult();
	outResult.DescriptorKey = descriptorKey;

	if (!IsValid(descriptorRegistryDataAsset))
	{
		outResult.ErrorReason = TEXT("Descriptor registry data asset is invalid.");
		return false;
	}

	if (descriptorKey.IsNone())
	{
		outResult.ErrorReason = TEXT("Descriptor key is None.");
		return false;
	}

	UVcardDescriptorBase* descriptor = nullptr;
	if (!descriptorRegistryDataAsset->FindDescriptorByKey(descriptorKey, descriptor) || !IsValid(descriptor))
	{
		outResult.ErrorReason = FString::Printf(TEXT("Descriptor key '%s' was not found."), *descriptorKey.ToString());
		return false;
	}

	return ApplyFactoryDescriptorInternal(descriptor, descriptorRegistryDataAsset, payloadData, outResult);
}

bool UVcardDescriptorGroupWidget::ApplyFactoryDescriptorInternal(
	UVcardDescriptorBase* descriptor,
	UVcardDescriptorRegistryDataAsset* descriptorRegistryDataAsset,
	UObject* payloadData,
	FVcardDescriptorApplyResult& outResult)
{
	outResult = FVcardDescriptorApplyResult();

	if (!IsValid(descriptor))
	{
		outResult.ErrorReason = TEXT("Factory descriptor is invalid.");
		return false;
	}

	FVcardDescriptorApplyRequest request;
	request.NamedSlotHostWidget = this;
	request.CacheOwnerWidget = this;
	request.DescriptorRegistryDataAsset = descriptorRegistryDataAsset;
	request.PayloadData = payloadData;
	request.AttachmentMode = EVcardDescriptorAttachmentMode::ECreateOnly;
	request.bAllowCreate = true;

	FVcardDescriptorApplyResult factoryResult;
	const bool bGenerated = descriptor->ApplyToWidget(request, factoryResult);
	outResult = factoryResult;
	if (!bGenerated)
	{
		return false;
	}

	TArray<UUserWidget*> sourceWidgets;
	sourceWidgets.Reserve(factoryResult.CreatedWidgets.Num());
	for (UUserWidget* createdWidget : factoryResult.CreatedWidgets)
	{
		if (IsValid(createdWidget))
		{
			sourceWidgets.Add(createdWidget);
		}
	}

	TArray<UUserWidget*> resolvedWidgets;
	ResolveGeneratedWidgets(sourceWidgets, payloadData, resolvedWidgets);

	FString errorReason;
	if (!AttachGeneratedWidgets(resolvedWidgets, errorReason))
	{
		outResult.bSuccess = false;
		outResult.ErrorReason = errorReason;
		return false;
	}

	outResult.CreatedWidgets.Reset();
	for (UUserWidget* resolvedWidget : resolvedWidgets)
	{
		if (IsValid(resolvedWidget))
		{
			outResult.CreatedWidgets.Add(resolvedWidget);
		}
	}

	outResult.bSuccess = true;
	return true;
}

bool UVcardDescriptorGroupWidget::AttachGeneratedWidgets(const TArray<UUserWidget*>& generatedWidgets, FString& outErrorReason)
{
	outErrorReason.Reset();

	UPanelWidget* panelWidget = nullptr;
	if (!FindGeneratedWidgetPanel(panelWidget, outErrorReason))
	{
		return false;
	}

	if (bClearPanelBeforeApply)
	{
		panelWidget->ClearChildren();
		mGeneratedWidgets.Reset();
	}

	for (UUserWidget* generatedWidget : generatedWidgets)
	{
		if (!IsValid(generatedWidget))
		{
			continue;
		}

		generatedWidget->RemoveFromParent();
		if (!panelWidget->AddChild(generatedWidget))
		{
			outErrorReason = FString::Printf(TEXT("Generated widget panel rejected widget '%s'."), *GetNameSafe(generatedWidget));
			return false;
		}

		mGeneratedWidgets.Add(generatedWidget);
	}

	panelWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BP_OnGeneratedWidgetsChanged(GetGeneratedWidgets());
	return true;
}

bool UVcardDescriptorGroupWidget::ClearGeneratedWidgets(FString& outErrorReason)
{
	outErrorReason.Reset();

	UPanelWidget* panelWidget = nullptr;
	if (!FindGeneratedWidgetPanel(panelWidget, outErrorReason))
	{
		return false;
	}

	panelWidget->ClearChildren();
	mGeneratedWidgets.Reset();
	BP_OnGeneratedWidgetsChanged(GetGeneratedWidgets());
	return true;
}

TArray<UUserWidget*> UVcardDescriptorGroupWidget::GetGeneratedWidgets() const
{
	TArray<UUserWidget*> generatedWidgets;
	generatedWidgets.Reserve(mGeneratedWidgets.Num());

	for (const TWeakObjectPtr<UUserWidget>& generatedWidget : mGeneratedWidgets)
	{
		if (generatedWidget.IsValid())
		{
			generatedWidgets.Add(generatedWidget.Get());
		}
	}

	return generatedWidgets;
}

void UVcardDescriptorGroupWidget::ResolveGeneratedWidgets_Implementation(
	const TArray<UUserWidget*>& sourceWidgets,
	UObject* payloadData,
	TArray<UUserWidget*>& outResolvedWidgets)
{
	outResolvedWidgets = sourceWidgets;
}

bool UVcardDescriptorGroupWidget::FindGeneratedWidgetPanel(UPanelWidget*& outPanelWidget, FString& outErrorReason) const
{
	outPanelWidget = nullptr;
	outErrorReason.Reset();

	if (IsValid(GeneratedWidgetPanel))
	{
		outPanelWidget = GeneratedWidgetPanel;
		return true;
	}

	if (GeneratedWidgetPanelName.IsNone())
	{
		outErrorReason = TEXT("Generated widget panel name is None.");
		return false;
	}

	UWidget* foundWidget = nullptr;
	if (!UVcardDescriptorApplier::FindWidgetByName(const_cast<UVcardDescriptorGroupWidget*>(this), GeneratedWidgetPanelName, foundWidget))
	{
		outErrorReason = FString::Printf(TEXT("Generated widget panel '%s' was not found."), *GeneratedWidgetPanelName.ToString());
		return false;
	}

	outPanelWidget = Cast<UPanelWidget>(foundWidget);
	if (!IsValid(outPanelWidget))
	{
		outErrorReason = FString::Printf(TEXT("Generated widget target '%s' is not a panel widget."), *GeneratedWidgetPanelName.ToString());
		return false;
	}

	return true;
}

void UVcardTopBarWidget::SetTopBarText(FText titleText, FText primaryText)
{
	mTitleText = titleText;
	mPrimaryText = primaryText;

	BP_OnTopBarTextChanged(titleText, primaryText);
}

void UVcardTopBarWidget::SetTopBarSignals(FName backSignalTag, FName primarySignalTag)
{
	mBackSignalTag = backSignalTag;
	mPrimarySignalTag = primarySignalTag;

	BP_OnTopBarSignalsChanged(backSignalTag, primarySignalTag);
}

bool UVcardModalLayerWidget::OpenModalContent(const FVcardWidgetAttachDescriptor& attachmentDescriptor, FVcardDescriptorApplyResult& outResult)
{
	outResult = FVcardDescriptorApplyResult();
	outResult.DescriptorKey = attachmentDescriptor.DebugName;

	FVcardWidgetAttachDescriptor normalizedAttachment = attachmentDescriptor;
	if (normalizedAttachment.TargetSlotName.IsNone())
	{
		normalizedAttachment.TargetSlotName = ContentSlotName;
	}

	FVcardDescriptorApplyRequest request;
	request.NamedSlotHostWidget = this;
	request.bAllowCreate = true;

	UUserWidget* createdWidget = nullptr;
	FString errorReason;
	const bool bApplied = UVcardDescriptorApplier::ApplyWidgetAttachment(request, normalizedAttachment, createdWidget, errorReason);
	outResult.bSuccess = bApplied;
	if (bApplied)
	{
		outResult.CreatedWidgets.Add(createdWidget);
		return true;
	}

	outResult.ErrorReason = errorReason;
	return false;
}

bool UVcardModalLayerWidget::CloseModalContent(FString& outErrorReason)
{
	outErrorReason.Reset();

	UNamedSlot* namedSlot = nullptr;
	if (UVcardDescriptorApplier::FindNamedSlot(this, ContentSlotName, namedSlot))
	{
		namedSlot->SetContent(nullptr);
		return true;
	}

	UPanelWidget* panelWidget = nullptr;
	if (UVcardDescriptorApplier::FindPanelWidget(this, ContentSlotName, panelWidget))
	{
		panelWidget->ClearChildren();
		return true;
	}

	outErrorReason = FString::Printf(TEXT("Modal content slot '%s' was not found."), *ContentSlotName.ToString());
	return false;
}

void UVcardToolRailWidget::SetTools(const TArray<FVcardToolDescriptor>& toolDescriptors)
{
	mToolDescriptors = toolDescriptors;
	BP_OnToolsChanged(toolDescriptors);
}

bool UVcardToolRailWidget::SelectTool(FName toolId)
{
	for (const FVcardToolDescriptor& toolDescriptor : mToolDescriptors)
	{
		if (toolDescriptor.ToolId == toolId)
		{
			mSelectedToolId = toolId;
			BP_OnToolSelected(toolId);
			OnToolSelected.Broadcast(toolId);
			return true;
		}
	}

	return false;
}

void UVcardOptionGridWidget::SetOptionItems(const TArray<FVcardOptionItemData>& optionItems)
{
	mOptionItems = optionItems;
	BP_OnOptionItemsChanged(optionItems);
}

bool UVcardOptionGridWidget::SelectOptionItem(FName itemId)
{
	for (const FVcardOptionItemData& optionItem : mOptionItems)
	{
		if (optionItem.ItemId == itemId)
		{
			mSelectedItemId = itemId;
			BP_OnOptionItemSelected(itemId);
			OnOptionItemSelected.Broadcast(itemId);
			return true;
		}
	}

	return false;
}

void UVcardOptionItemWidget::SetOptionItemData(const FVcardOptionItemData& optionItemData, bool bSelected)
{
	mOptionItemData = optionItemData;
	mbSelected = bSelected;

	BP_OnOptionItemDataChanged(optionItemData);
	BP_OnSelectedChanged(bSelected);
}

void UVcardOptionItemWidget::SetSelected(bool bSelected)
{
	if (mbSelected == bSelected)
	{
		return;
	}

	mbSelected = bSelected;
	BP_OnSelectedChanged(bSelected);
}
