#include "VdjmVcardWidgetBase.h"

#include "VdjmVcardDescriptorRegistryDataAsset.h"

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
