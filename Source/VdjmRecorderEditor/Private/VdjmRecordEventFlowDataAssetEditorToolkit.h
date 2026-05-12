#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/GCObject.h"

class IDetailsView;
class SMultiLineEditableTextBox;
class UVdjmRecordEventFlowDataAsset;

class FVdjmRecordEventFlowDataAssetEditorToolkit
	: public FAssetEditorToolkit
	, public FGCObject
{
public:
	void InitEditor(
		const EToolkitMode::Type mode,
		const TSharedPtr<IToolkitHost>& initToolkitHost,
		UVdjmRecordEventFlowDataAsset* flowDataAsset);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& tabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& tabManager) override;

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	virtual void AddReferencedObjects(FReferenceCollector& collector) override;
	virtual FString GetReferencerName() const override;

private:
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& args);
	TSharedRef<SDockTab> SpawnJsonTab(const FSpawnTabArgs& args);
	TSharedRef<SDockTab> SpawnSummaryTab(const FSpawnTabArgs& args);
	TSharedRef<SDockTab> SpawnGuideTab(const FSpawnTabArgs& args);

	void RefreshAllViews();
	void RefreshJsonText();
	void RefreshSummaryText();
	void RefreshGuideText();
	void SetStatusMessage(const FString& statusMessage);

	FReply OnRefreshJsonClicked();
	FReply OnCopyJsonClicked();
	FReply OnValidateJsonClicked();
	FReply OnImportJsonClicked();
	FReply OnRefreshSummaryClicked();
	FReply OnResetGuideClicked();

	FText GetStatusText() const;
	FString GetCurrentJsonText() const;

	TObjectPtr<UVdjmRecordEventFlowDataAsset> EditingAsset = nullptr;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SMultiLineEditableTextBox> JsonTextBox;
	TSharedPtr<SMultiLineEditableTextBox> SummaryTextBox;
	TSharedPtr<SMultiLineEditableTextBox> GuideTextBox;
	FString LastStatusMessage;
};
