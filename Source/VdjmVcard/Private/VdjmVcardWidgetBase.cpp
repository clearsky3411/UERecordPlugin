#include "VdjmVcardWidgetBase.h"

#include "EngineUtils.h"
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

bool UVcardWidgetBase::ApplyDescriptorById(FName descriptorId, FVcardDescriptorApplyResult& outResult)
{
	return ApplyDescriptorInternal(NAME_None, descriptorId, outResult);
}

bool UVcardWidgetBase::ApplyDescriptorToNamedSlot(FName slotName, FName descriptorId, FVcardDescriptorApplyResult& outResult)
{
	return ApplyDescriptorInternal(slotName, descriptorId, outResult);
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

bool UVcardWidgetBase::ApplyDescriptorInternal(FName invocationSlotName, FName descriptorId, FVcardDescriptorApplyResult& outResult)
{
	outResult = FVcardDescriptorApplyResult();
	outResult.DescriptorId = descriptorId;

	FString errorReason;
	if (!EnsureDescriptorRegistry(errorReason))
	{
		outResult.ErrorReason = errorReason;
		return false;
	}

	if (descriptorId.IsNone())
	{
		outResult.ErrorReason = TEXT("DescriptorId is None.");
		return false;
	}

	UVcardDescriptorBase* descriptor = nullptr;
	if (!mDescriptorRegistry->FindDescriptorById(descriptorId, descriptor) || !IsValid(descriptor))
	{
		outResult.ErrorReason = FString::Printf(TEXT("Descriptor '%s' was not found."), *descriptorId.ToString());
		return false;
	}

	FVcardDescriptorApplyRequest request;
	request.HostWidget = this;
	request.ContextObject = IsValid(mContextObject) ? mContextObject.Get() : this;
	request.InvocationSlotName = invocationSlotName;
	request.bAllowCreate = true;
	return descriptor->ApplyToWidget(request, outResult);
}
