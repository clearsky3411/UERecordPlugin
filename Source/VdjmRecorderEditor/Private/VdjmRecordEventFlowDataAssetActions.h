#pragma once

#include "AssetTypeActions_Base.h"

class FVdjmRecordEventFlowDataAssetActions : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual void OpenAssetEditor(
		const TArray<UObject*>& inObjects,
		TSharedPtr<IToolkitHost> editWithinLevelEditor) override;
};
