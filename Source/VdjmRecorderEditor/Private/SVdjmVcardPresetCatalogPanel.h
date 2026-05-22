#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "VdjmVcardPresetCatalogDataAsset.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/SListView.h"

class SEditableTextBox;
class STableViewBase;

struct FVdjmVcardPresetCatalogScanRow
{
	FAssetData AssetData;
	FString AssetName;
	FString AssetClass;
	FString ObjectPath;
};

class SVdjmVcardPresetCatalogPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVdjmVcardPresetCatalogPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& inArgs);

private:
	FReply HandleUseSelectedCatalogClicked();
	FReply HandleLoadCatalogClicked();
	FReply HandleScanFolderClicked();
	FReply HandleReplaceChunkClicked();
	FReply HandleAppendChunkClicked();
	FReply HandleClearChunkClicked();
	FReply HandleSaveCatalogClicked();

	TSharedRef<ITableRow> GenerateScanRow(
		TSharedPtr<FVdjmVcardPresetCatalogScanRow> item,
		const TSharedRef<STableViewBase>& ownerTable);

	void RefreshScanRows();
	void RefreshSummary();
	void SetStatus(const FString& statusText);
	bool LoadCatalogFromPath(const FString& catalogObjectPath);
	bool ScanFolder();
	bool BuildPresetItemsFromScanRows(TArray<FVcardPresetItemData>& outPresetItems, FString& outErrorReason) const;
	bool IsAssetAcceptedForSelectedKind(const FAssetData& assetData) const;
	void ApplyCatalogMutation(bool bReplace);
	void RefreshKindOptions();

	FName GetChunkKey() const;
	FName GetGroupKey() const;
	FName GetSlotKey() const;
	EVcardPresetAssetKind GetSelectedKind() const;
	FText GetCatalogPathText() const;
	FText GetStatusText() const;
	FText GetSummaryText() const;
	FText GetSelectedKindText() const;

	void HandleCatalogPathCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleScanFolderCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleChunkKeyCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleGroupKeyCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSlotKeyCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleKindSelectionChanged(TSharedPtr<EVcardPresetAssetKind> item, ESelectInfo::Type selectInfo);

	TWeakObjectPtr<UVcardPresetCatalogDataAsset> CatalogDataAsset;
	TArray<TSharedPtr<FVdjmVcardPresetCatalogScanRow>> ScanRows;
	TArray<TSharedPtr<EVcardPresetAssetKind>> KindOptions;
	TSharedPtr<SListView<TSharedPtr<FVdjmVcardPresetCatalogScanRow>>> ScanListView;
	TSharedPtr<SComboBox<TSharedPtr<EVcardPresetAssetKind>>> KindComboBox;
	FString CatalogObjectPath;
	FString ScanFolderPath = TEXT("/Game");
	FString ChunkKeyText = TEXT("background_001");
	FString GroupKeyText = TEXT("background.preset");
	FString SlotKeyText = TEXT("source");
	EVcardPresetAssetKind SelectedKind = EVcardPresetAssetKind::ETexture;
	FString StatusText;
	FString SummaryText;
};
