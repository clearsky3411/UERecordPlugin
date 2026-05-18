#include "VdjmVcardWidgetBase.h"

#include "EngineUtils.h"
#include "VdjmVcardDescriptorApplier.h"
#include "VdjmVcardDescriptorBase.h"
#include "VdjmVcardDescriptorRegistryDataAsset.h"
#include "VdjmVcardUiRegistryActor.h"

void UVcardWidgetBase::ApplyVcardDescriptorContext(UVcardDescriptorRegistryDataAsset* descriptorRegistry, UObject* contextObject)
{
	mDescriptorRegistry = descriptorRegistry;
	mContextObject = contextObject;

	BP_OnVcardDescriptorContextApplied(descriptorRegistry, contextObject);
}

void UVcardWidgetBase::ApplyVcardWidgetAttachment_Implementation(const FVcardWidgetAttachDescriptor& attachmentDescriptor, UObject* payloadData)
{
	mLastAttachmentDescriptor = attachmentDescriptor;
	mLastPayloadData = payloadData;

	BP_OnVcardAttachmentApplied(attachmentDescriptor, payloadData);
}

bool UVcardWidgetBase::EnsureDescriptorRegistry(FString& outErrorReason)
{
	outErrorReason.Reset();

	if (IsValid(mDescriptorRegistry))
	{
		return true;
	}

	if (AVcardUiRegistryActor* registryActor = FindWorldRegistryActor())
	{
		if (UVcardDescriptorRegistryDataAsset* descriptorRegistry = registryActor->GetDescriptorRegistry())
		{
			ApplyVcardDescriptorContext(descriptorRegistry, registryActor);
			return true;
		}
	}

	if (UVcardDescriptorRegistryDataAsset* descriptorRegistry = LoadDefaultDescriptorRegistry())
	{
		ApplyVcardDescriptorContext(descriptorRegistry, IsValid(mContextObject) ? mContextObject.Get() : this);
		return true;
	}

	outErrorReason = TEXT("Descriptor registry is not assigned and no world Vcard UI registry actor was found.");
	return false;
}

UVcardDescriptorRegistryDataAsset* UVcardWidgetBase::LoadDefaultDescriptorRegistry()
{
	if (IsValid(mDescriptorRegistry))
	{
		return mDescriptorRegistry.Get();
	}

	UVcardDescriptorRegistryDataAsset* loadedRegistry = DefaultDescriptorRegistryAsset.LoadSynchronous();
	if (IsValid(loadedRegistry))
	{
		mDescriptorRegistry = loadedRegistry;
	}

	return loadedRegistry;
}

bool UVcardWidgetBase::ApplyDescriptorByKey(FName descriptorKey, FVcardDescriptorApplyResult& outResult)
{
	return ApplyDescriptorInternal(NAME_None, descriptorKey, outResult);
}

bool UVcardWidgetBase::ApplyDescriptorToNamedSlot(FName slotName, FName descriptorKey, FVcardDescriptorApplyResult& outResult)
{
	return ApplyDescriptorInternal(slotName, descriptorKey, outResult);
}

AVcardUiRegistryActor* UVcardWidgetBase::FindWorldRegistryActor() const
{
	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AVcardUiRegistryActor> actorIt(world); actorIt; ++actorIt)
	{
		if (IsValid(*actorIt))
		{
			return *actorIt;
		}
	}

	return nullptr;
}

bool UVcardWidgetBase::ApplyDescriptorInternal(FName fallbackTargetSlotName, FName descriptorKey, FVcardDescriptorApplyResult& outResult)
{
	outResult = FVcardDescriptorApplyResult();
	outResult.DescriptorKey = descriptorKey;

	FString errorReason;
	if (!EnsureDescriptorRegistry(errorReason))
	{
		outResult.ErrorReason = errorReason;
		return false;
	}

	if (descriptorKey.IsNone())
	{
		outResult.ErrorReason = TEXT("Descriptor key is None.");
		return false;
	}

	UObject* payloadData = IsValid(mContextObject) ? mContextObject.Get() : this;
	if (fallbackTargetSlotName.IsNone())
	{
		TArray<UUserWidget*> createdWidgets;
		FString applyErrorReason;
		const bool bGenerated = UVcardDescriptorApplier::GenerateWidgetsIntoNamedSlotsFromVcardDescriptorDataAsset(
			this,
			mDescriptorRegistry.Get(),
			descriptorKey,
			payloadData,
			createdWidgets,
			applyErrorReason);
		outResult.bSuccess = bGenerated;
		outResult.ErrorReason = applyErrorReason;

		for (UUserWidget* createdWidget : createdWidgets)
		{
			if (IsValid(createdWidget))
			{
				outResult.CreatedWidgets.Add(createdWidget);
			}
		}

		return bGenerated;
	}

	UVcardDescriptorBase* descriptor = nullptr;
	if (!mDescriptorRegistry->FindDescriptorByKey(descriptorKey, descriptor) || !IsValid(descriptor))
	{
		outResult.ErrorReason = FString::Printf(TEXT("Descriptor key '%s' was not found."), *descriptorKey.ToString());
		return false;
	}

	FVcardDescriptorApplyRequest request;
	request.NamedSlotHostWidget = this;
	request.FallbackTargetSlotName = fallbackTargetSlotName;
	request.PayloadData = payloadData;
	request.bAllowCreate = true;
	return descriptor->ApplyToWidget(request, outResult);
}
