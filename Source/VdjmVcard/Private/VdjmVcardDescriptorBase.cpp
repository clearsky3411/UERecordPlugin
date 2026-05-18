#include "VdjmVcardDescriptorBase.h"

#include "VdjmVcard.h"
#include "VdjmVcardDescriptorApplier.h"

bool UVcardDescriptorBase::ApplyToWidget(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult)
{
	outResult = FVcardDescriptorApplyResult();

	FString reason;
	if (!CanApplyToWidget(request, reason))
	{
		outResult.ErrorReason = reason;
		return false;
	}

	const bool bApplied = ApplyToWidgetInternal(request, outResult);
	outResult.bSuccess = bApplied;
	return bApplied;
}

bool UVcardDescriptorBase::CanApplyToWidget(const FVcardDescriptorApplyRequest& request, FString& outReason) const
{
	outReason.Reset();

	if (!IsValid(request.NamedSlotHostWidget))
	{
		outReason = TEXT("Named slot host widget is invalid.");
		return false;
	}

	return true;
}

bool UVcardDescriptorBase::GenerateWidgetsIntoNamedSlots(
	UUserWidget* namedSlotHostWidget,
	UObject* payloadData,
	TArray<UUserWidget*>& outCreatedWidgets,
	FString& outErrorReason)
{
	outCreatedWidgets.Reset();
	outErrorReason.Reset();

	FVcardDescriptorApplyRequest request;
	request.NamedSlotHostWidget = namedSlotHostWidget;
	request.PayloadData = payloadData;
	request.bAllowCreate = true;

	FVcardDescriptorApplyResult result;
	const bool bGenerated = ApplyToWidget(request, result);
	for (UUserWidget* createdWidget : result.CreatedWidgets)
	{
		if (IsValid(createdWidget))
		{
			outCreatedWidgets.Add(createdWidget);
		}
	}

	outErrorReason = result.ErrorReason;
	return bGenerated;
}

bool UVcardDescriptorBase::ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult)
{
	UE_LOG(LogVdjmVcard, Verbose, TEXT("Vcard descriptor no-op DebugName=%s Host=%s FallbackSlot=%s"),
		*DebugName.ToString(),
		*GetNameSafe(request.NamedSlotHostWidget),
		*request.FallbackTargetSlotName.ToString());
	return true;
}

UVcardSingleSlotWidgetDescriptor::UVcardSingleSlotWidgetDescriptor()
{
	DebugName = TEXT("vcard-single-slot");
}

bool UVcardSingleSlotWidgetDescriptor::ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult)
{
	UUserWidget* createdWidget = nullptr;
	FString errorReason;
	const bool bApplied = UVcardDescriptorApplier::ApplyWidgetAttachment(request, SlotAttachment, createdWidget, errorReason);
	if (bApplied)
	{
		outResult.CreatedWidgets.Add(createdWidget);
		return true;
	}

	UE_LOG(LogVdjmVcard, Warning, TEXT("Vcard single-slot descriptor failed Descriptor=%s Target=%s WidgetClass=%s Reason=%s"),
		*DebugName.ToString(),
		*SlotAttachment.TargetSlotName.ToString(),
		*GetNameSafe(*SlotAttachment.WidgetClass),
		*errorReason);

	outResult.ErrorReason = errorReason;
	return false;
}

bool UVcardWidgetCompositionDescriptor::ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult)
{
	if (Attachments.Num() == 0)
	{
		UE_LOG(LogVdjmVcard, Verbose, TEXT("Vcard composition descriptor no-op DebugName=%s Host=%s FallbackSlot=%s"),
			*DebugName.ToString(),
			*GetNameSafe(request.NamedSlotHostWidget),
			*request.FallbackTargetSlotName.ToString());
		return true;
	}

	bool bAnySuccess = false;

	for (const FVcardWidgetAttachDescriptor& attachmentDescriptor : Attachments)
	{
		UUserWidget* createdWidget = nullptr;
		FString errorReason;
		const bool bApplied = UVcardDescriptorApplier::ApplyWidgetAttachment(request, attachmentDescriptor, createdWidget, errorReason);
		if (bApplied)
		{
			bAnySuccess = true;
			outResult.CreatedWidgets.Add(createdWidget);
			continue;
		}

		UE_LOG(LogVdjmVcard, Warning, TEXT("Vcard descriptor attachment failed Descriptor=%s Attachment=%s Target=%s WidgetClass=%s Reason=%s"),
			*DebugName.ToString(),
			*attachmentDescriptor.DebugName.ToString(),
			*attachmentDescriptor.TargetSlotName.ToString(),
			*GetNameSafe(*attachmentDescriptor.WidgetClass),
			*errorReason);

		outResult.ErrorReason = errorReason;
		if (bStopOnFirstFailure)
		{
			return false;
		}
	}

	return bAnySuccess;
}

UVcardCompositeDescriptor::UVcardCompositeDescriptor()
{
	DebugName = TEXT("vcard-composite");
}

TArray<UVcardDescriptorBase*> UVcardCompositeDescriptor::GetChildDescriptorList() const
{
	TArray<UVcardDescriptorBase*> childDescriptorList;
	childDescriptorList.Reserve(ChildDescriptors.Num());

	for (UVcardDescriptorBase* childDescriptor : ChildDescriptors)
	{
		if (IsValid(childDescriptor))
		{
			childDescriptorList.Add(childDescriptor);
		}
	}

	return childDescriptorList;
}

bool UVcardCompositeDescriptor::ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult)
{
	if (ChildDescriptors.Num() == 0)
	{
		UE_LOG(LogVdjmVcard, Verbose, TEXT("Vcard composite descriptor no-op DebugName=%s Host=%s FallbackSlot=%s"),
			*DebugName.ToString(),
			*GetNameSafe(request.NamedSlotHostWidget),
			*request.FallbackTargetSlotName.ToString());
		return true;
	}

	bool bAnySuccess = false;

	for (int32 childIndex = 0; childIndex < ChildDescriptors.Num(); ++childIndex)
	{
		UVcardDescriptorBase* childDescriptor = ChildDescriptors[childIndex];
		if (!IsValid(childDescriptor) || childDescriptor == this)
		{
			outResult.ErrorReason = FString::Printf(TEXT("Composite child descriptor is invalid. Descriptor=%s Index=%d"),
				*DebugName.ToString(),
				childIndex);
			UE_LOG(LogVdjmVcard, Warning, TEXT("Vcard composite child skipped Descriptor=%s Index=%d Reason=%s"),
				*DebugName.ToString(),
				childIndex,
				*outResult.ErrorReason);

			if (bStopOnFirstFailure)
			{
				return false;
			}

			continue;
		}

		FVcardDescriptorApplyResult childResult;
		const bool bApplied = childDescriptor->ApplyToWidget(request, childResult);
		if (bApplied)
		{
			bAnySuccess = true;
			for (UUserWidget* createdWidget : childResult.CreatedWidgets)
			{
				if (IsValid(createdWidget))
				{
					outResult.CreatedWidgets.Add(createdWidget);
				}
			}
			continue;
		}

		outResult.ErrorReason = childResult.ErrorReason.IsEmpty()
			? FString::Printf(TEXT("Composite child descriptor failed. Descriptor=%s Child=%s Index=%d"),
				*DebugName.ToString(),
				*GetNameSafe(childDescriptor),
				childIndex)
			: childResult.ErrorReason;
		UE_LOG(LogVdjmVcard, Warning, TEXT("Vcard composite child failed Descriptor=%s Child=%s Index=%d Reason=%s"),
			*DebugName.ToString(),
			*GetNameSafe(childDescriptor),
			childIndex,
			*outResult.ErrorReason);

		if (bStopOnFirstFailure)
		{
			return false;
		}
	}

	return bAnySuccess;
}

UVcardRootDescriptor::UVcardRootDescriptor()
{
	DebugName = TEXT("vcard-root");
}

UVcardPreviewLobbyDescriptor::UVcardPreviewLobbyDescriptor()
{
	DebugName = TEXT("vcard-preview-lobby");
}

UVcardCreatorLobbyDescriptor::UVcardCreatorLobbyDescriptor()
{
	DebugName = TEXT("vcard-creator-lobby");
}

UVcardToolOptContentDescriptor::UVcardToolOptContentDescriptor()
{
	DebugName = TEXT("vcard-tool-opt-content");
}

UVcardStageLobbyDescriptor::UVcardStageLobbyDescriptor()
{
	DebugName = TEXT("vcard-stage-lobby");
}

UVcardLobbyDescriptor::UVcardLobbyDescriptor()
{
	DebugName = TEXT("vcard-lobby");
}

UVcardStageDescriptor::UVcardStageDescriptor()
{
	DebugName = TEXT("vcard-stage");
}
