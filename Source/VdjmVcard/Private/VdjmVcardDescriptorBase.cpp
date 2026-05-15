#include "VdjmVcardDescriptorBase.h"

#include "VdjmVcard.h"
#include "VdjmVcardDescriptorApplier.h"

bool UVcardDescriptorBase::ApplyToWidget(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult)
{
	outResult = FVcardDescriptorApplyResult();
	outResult.DescriptorId = DescriptorId;

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

	if (DescriptorId.IsNone())
	{
		outReason = TEXT("DescriptorId is not assigned.");
		return false;
	}

	if (!IsValid(request.HostWidget))
	{
		outReason = TEXT("Host widget is invalid.");
		return false;
	}

	return true;
}

bool UVcardDescriptorBase::ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult)
{
	outResult.ErrorReason = FString::Printf(TEXT("Descriptor '%s' has no native apply implementation."), *DescriptorId.ToString());
	UE_LOG(LogVdjmVcard, Warning, TEXT("Vcard descriptor apply skipped Descriptor=%s Host=%s Reason=%s"),
		*DescriptorId.ToString(),
		*GetNameSafe(request.HostWidget),
		*outResult.ErrorReason);
	return false;
}

bool UVcardWidgetCompositionDescriptor::ApplyToWidgetInternal(const FVcardDescriptorApplyRequest& request, FVcardDescriptorApplyResult& outResult)
{
	if (Attachments.Num() == 0)
	{
		UE_LOG(LogVdjmVcard, Verbose, TEXT("Vcard composition descriptor no-op Descriptor=%s Host=%s InvocationSlot=%s"),
			*DescriptorId.ToString(),
			*GetNameSafe(request.HostWidget),
			*request.InvocationSlotName.ToString());
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

		UE_LOG(LogVdjmVcard, Warning, TEXT("Vcard composition attachment failed Descriptor=%s Attach=%s Target=%s Reason=%s"),
			*DescriptorId.ToString(),
			*attachmentDescriptor.AttachId.ToString(),
			*attachmentDescriptor.TargetSlotName.ToString(),
			*errorReason);

		outResult.ErrorReason = errorReason;
		if (bStopOnFirstFailure)
		{
			return false;
		}
	}

	return bAnySuccess;
}

UVcardRootDescriptor::UVcardRootDescriptor()
{
	DescriptorId = TEXT("vcard-root");
}

UVcardStageLobbyDescriptor::UVcardStageLobbyDescriptor()
{
	DescriptorId = TEXT("vcard-stage-lobby");
}

UVcardLobbyDescriptor::UVcardLobbyDescriptor()
{
	DescriptorId = TEXT("vcard-lobby");
}

UVcardStageDescriptor::UVcardStageDescriptor()
{
	DescriptorId = TEXT("vcard-stage");
}
