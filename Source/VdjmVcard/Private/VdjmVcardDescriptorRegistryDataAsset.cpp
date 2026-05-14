#include "VdjmVcardDescriptorRegistryDataAsset.h"

bool UVcardDescriptorRegistryDataAsset::FindDescriptorById(FName descriptorId, UVcardDescriptorBase*& outDescriptor) const
{
	outDescriptor = nullptr;

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
	descriptorList.Reserve(Descriptors.Num());

	for (UVcardDescriptorBase* descriptor : Descriptors)
	{
		if (IsValid(descriptor))
		{
			descriptorList.Add(descriptor);
		}
	}

	return descriptorList;
}

bool UVcardDescriptorRegistryDataAsset::FindScreenDescriptor(FName screenId, FVcardScreenDescriptor& outScreenDescriptor) const
{
	for (const FVcardScreenDescriptor& screenDescriptor : ScreenDescriptors)
	{
		if (screenDescriptor.ScreenId == screenId)
		{
			outScreenDescriptor = screenDescriptor;
			return true;
		}
	}

	return false;
}

bool UVcardDescriptorRegistryDataAsset::FindBottomSheetDescriptor(FName descriptorId, FVcardBottomSheetDescriptor& outBottomSheetDescriptor) const
{
	for (const FVcardBottomSheetDescriptor& bottomSheetDescriptor : BottomSheetDescriptors)
	{
		if (bottomSheetDescriptor.DescriptorId == descriptorId)
		{
			outBottomSheetDescriptor = bottomSheetDescriptor;
			return true;
		}
	}

	return false;
}

bool UVcardDescriptorRegistryDataAsset::FindToolDescriptor(FName toolId, FVcardToolDescriptor& outToolDescriptor) const
{
	for (const FVcardToolDescriptor& toolDescriptor : ToolDescriptors)
	{
		if (toolDescriptor.ToolId == toolId)
		{
			outToolDescriptor = toolDescriptor;
			return true;
		}
	}

	return false;
}

bool UVcardDescriptorRegistryDataAsset::FindBackgroundItem(FName itemId, FVcardBackgroundItemData& outBackgroundItem) const
{
	for (const FVcardBackgroundItemData& backgroundItem : BackgroundItems)
	{
		if (backgroundItem.ItemId == itemId)
		{
			outBackgroundItem = backgroundItem;
			return true;
		}
	}

	return false;
}

bool UVcardDescriptorRegistryDataAsset::FindMotionItem(FName itemId, FVcardMotionItemData& outMotionItem) const
{
	for (const FVcardMotionItemData& motionItem : MotionItems)
	{
		if (motionItem.ItemId == itemId)
		{
			outMotionItem = motionItem;
			return true;
		}
	}

	return false;
}

bool UVcardDescriptorRegistryDataAsset::FindTextField(FName fieldId, FVcardTextFieldDescriptor& outTextField) const
{
	for (const FVcardTextFieldDescriptor& textField : TextFields)
	{
		if (textField.FieldId == fieldId)
		{
			outTextField = textField;
			return true;
		}
	}

	return false;
}
