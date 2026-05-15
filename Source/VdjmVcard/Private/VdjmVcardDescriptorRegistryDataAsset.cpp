#include "VdjmVcardDescriptorRegistryDataAsset.h"

bool UVcardDescriptorRegistryDataAsset::FindDescriptorById(FName descriptorId, UVcardDescriptorBase*& outDescriptor) const
{
	outDescriptor = nullptr;

	if (descriptorId.IsNone())
	{
		return false;
	}

	if (const TObjectPtr<UVcardDescriptorBase>* mappedDescriptor = DescriptorMap.Find(descriptorId))
	{
		outDescriptor = mappedDescriptor->Get();
		if (IsValid(outDescriptor))
		{
			return true;
		}
	}

	for (UVcardDescriptorBase* descriptor : Descriptors)
	{
		if (IsValid(descriptor) && descriptor->GetDescriptorId() == descriptorId)
		{
			outDescriptor = descriptor;
			return true;
		}
	}

	return false;
}

TArray<UVcardDescriptorBase*> UVcardDescriptorRegistryDataAsset::GetDescriptorList() const
{
	TArray<UVcardDescriptorBase*> descriptorList;
	descriptorList.Reserve(DescriptorMap.Num() + Descriptors.Num());

	TSet<UVcardDescriptorBase*> seenDescriptors;

	for (const TPair<FName, TObjectPtr<UVcardDescriptorBase>>& descriptorPair : DescriptorMap)
	{
		UVcardDescriptorBase* descriptor = descriptorPair.Value.Get();
		if (IsValid(descriptor))
		{
			descriptorList.Add(descriptor);
			seenDescriptors.Add(descriptor);
		}
	}

	for (UVcardDescriptorBase* descriptor : Descriptors)
	{
		if (IsValid(descriptor) && !seenDescriptors.Contains(descriptor))
		{
			descriptorList.Add(descriptor);
		}
	}

	return descriptorList;
}
