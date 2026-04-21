#include "VdjmRecordEventFlowDataAsset.h"

#include "VdjmRecordEventNode.h"

int32 UVdjmRecordEventFlowDataAsset::FindEventIndexByTag(FName InTag) const
{
	if (InTag.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		const UVdjmRecordEventBase* Event = Events[Index];
		if (Event != nullptr && Event->EventTag == InTag)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}
