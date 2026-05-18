#include "VdjmVcardDescriptorRegistryDataAsset.h"

bool UVcardDescriptorRegistryDataAsset::FindDescriptorByKey(FName descriptorKey, UVcardDescriptorBase*& outDescriptor) const
{
	outDescriptor = nullptr;

	if (descriptorKey.IsNone())
	{
		return false;
	}

	if (const TObjectPtr<UVcardDescriptorBase>* mappedDescriptor = DescriptorByKey.Find(descriptorKey))
	{
		outDescriptor = mappedDescriptor->Get();
		if (IsValid(outDescriptor))
		{
			return true;
		}
	}

	return false;
}

TArray<UVcardDescriptorBase*> UVcardDescriptorRegistryDataAsset::GetDescriptorList() const
{
	TArray<UVcardDescriptorBase*> descriptorList;
	descriptorList.Reserve(DescriptorByKey.Num());

	for (const TPair<FName, TObjectPtr<UVcardDescriptorBase>>& descriptorPair : DescriptorByKey)
	{
		UVcardDescriptorBase* descriptor = descriptorPair.Value.Get();
		if (IsValid(descriptor))
		{
			descriptorList.Add(descriptor);
		}
	}

	return descriptorList;
}
