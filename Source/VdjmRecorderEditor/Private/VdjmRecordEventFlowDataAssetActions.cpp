#include "VdjmRecordEventFlowDataAssetActions.h"

#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"
#include "VdjmRecordEventFlowDataAssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "FVdjmRecordEventFlowDataAssetActions"

FText FVdjmRecordEventFlowDataAssetActions::GetName() const
{
	return LOCTEXT("AssetName", "Vdjm Record Event Flow");
}

FColor FVdjmRecordEventFlowDataAssetActions::GetTypeColor() const
{
	return FColor(48, 164, 196);
}

UClass* FVdjmRecordEventFlowDataAssetActions::GetSupportedClass() const
{
	return UVdjmRecordEventFlowDataAsset::StaticClass();
}

uint32 FVdjmRecordEventFlowDataAssetActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FVdjmRecordEventFlowDataAssetActions::OpenAssetEditor(
	const TArray<UObject*>& inObjects,
	TSharedPtr<IToolkitHost> editWithinLevelEditor)
{
	for (UObject* object : inObjects)
	{
		UVdjmRecordEventFlowDataAsset* flowDataAsset = Cast<UVdjmRecordEventFlowDataAsset>(object);
		if (flowDataAsset == nullptr)
		{
			continue;
		}

		const TSharedRef<FVdjmRecordEventFlowDataAssetEditorToolkit> editorToolkit =
			MakeShared<FVdjmRecordEventFlowDataAssetEditorToolkit>();
		editorToolkit->InitEditor(EToolkitMode::Standalone, editWithinLevelEditor, flowDataAsset);
	}
}

#undef LOCTEXT_NAMESPACE
