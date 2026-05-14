#pragma once

#include "CoreMinimal.h"
#include "VdjmAssetRegistryTypes.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class STableViewBase;

struct FVdjmAssetRegistryEditorAssetRow
{
	int32 SourceIndex = INDEX_NONE;
	FString AssetKey;
	FString Type;
	FString Root;
	FString RelativePath;
	FString VirtualPath;
	FString Importance;
	FString Class;
};

class SVdjmAssetRegistryPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVdjmAssetRegistryPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& inArgs);

private:
	FReply HandleLoadClicked();
	FReply HandleValidateClicked();
	FReply HandleScanClicked();
	FReply HandleScanRegisterClicked();
	FReply HandleSaveClicked();
	FReply HandleOpenJsonClicked();
	FReply HandleCopySummaryClicked();
	FReply HandleRemoveSelectedClicked();

	TSharedRef<ITableRow> GenerateAssetRow(
		TSharedPtr<FVdjmAssetRegistryEditorAssetRow> item,
		const TSharedRef<STableViewBase>& ownerTable);
	TSharedRef<ITableRow> GenerateMessageRow(
		TSharedPtr<FVdjmAssetRegistryMessage> item,
		const TSharedRef<STableViewBase>& ownerTable);
	void HandleAssetSelectionChanged(TSharedPtr<FVdjmAssetRegistryEditorAssetRow> item, ESelectInfo::Type selectInfo);

	void RefreshAll();
	void RefreshAssetRows();
	void RefreshMessageRows();
	void RefreshSummary();
	void SetMessages(const TArray<FVdjmAssetRegistryMessage>& messages);
	void SetStatus(const FString& statusText);
	void ApplySelectedField(const FString& fieldName, const FString& value);

	bool PassesFilter(const FVdjmAssetRegistryAssetEntry& asset) const;
	FString GetSelectedAssetField(const FString& fieldName) const;
	FString GetSelectedTagsText() const;
	FString GetSelectedMetaPurposeText() const;
	FString NormalizeVirtualPathInput(const FString& value) const;
	FText GetStatusText() const;
	FText GetSummaryText() const;
	FText GetSelectedTypeText() const;
	FText GetSelectedVirtualPathText() const;
	FText GetSelectedImportanceText() const;
	FText GetSelectedClassText() const;
	FText GetSelectedTagsTextValue() const;
	FText GetSelectedPurposeText() const;

	void HandleSearchTextChanged(const FText& text);
	void HandleTypeFilterChanged(const FText& text);
	void HandleVirtualFilterChanged(const FText& text);
	void HandleImportanceFilterChanged(const FText& text);
	void HandleSelectedTypeCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedVirtualPathCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedImportanceCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedClassCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedTagsCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedPurposeCommitted(const FText& text, ETextCommit::Type commitType);

	FVdjmAssetRegistryDocument Registry;
	FVdjmAssetRegistryScanResult LastScanResult;
	TArray<TSharedPtr<FVdjmAssetRegistryEditorAssetRow>> AssetRows;
	TArray<TSharedPtr<FVdjmAssetRegistryMessage>> MessageRows;
	TSharedPtr<class SListView<TSharedPtr<FVdjmAssetRegistryEditorAssetRow>>> AssetListView;
	TSharedPtr<class SListView<TSharedPtr<FVdjmAssetRegistryMessage>>> MessageListView;
	TSharedPtr<FVdjmAssetRegistryEditorAssetRow> SelectedRow;
	FString SearchText;
	FString TypeFilter;
	FString VirtualFilter;
	FString ImportanceFilter;
	FString StatusText;
	FString SummaryText;
};
