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
	if (Attachments.Num() == 0)
	{
		UE_LOG(LogVdjmVcard, Verbose, TEXT("Vcard descriptor no-op DebugName=%s Host=%s FallbackSlot=%s"),
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

bool UVcardWidgetCompositionDescriptor::ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult)
{
	return Super::ApplyToWidgetInternal(request, outResult);
}

UVcardRootDescriptor::UVcardRootDescriptor()
{
	DebugName = TEXT("vcard-root");
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
