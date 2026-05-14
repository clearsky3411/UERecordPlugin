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

struct FVdjmAssetRegistryEditorRootRow
{
	int32 SourceIndex = INDEX_NONE;
	FString Kind;
	FString Key;
	FString Root;
	FString RelativePath;
	FString DefaultPath;
	FString WinPath;
	FString AndroidPath;
	FString IosPath;
	FString bScanText;
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
	FReply HandlePickScanFolderClicked();
	FReply HandleScanFolderClicked();
	FReply HandleScanFolderRegisterClicked();
	FReply HandleSaveClicked();
	FReply HandleOpenJsonClicked();
	FReply HandleCopySummaryClicked();
	FReply HandleRemoveSelectedClicked();
	FReply HandleAddRootClicked();
	FReply HandleAddExternalRootClicked();
	FReply HandleAddDefinedRootClicked();
	FReply HandleRemoveRootClicked();

	TSharedRef<ITableRow> GenerateAssetRow(
		TSharedPtr<FVdjmAssetRegistryEditorAssetRow> item,
		const TSharedRef<STableViewBase>& ownerTable);
	TSharedRef<ITableRow> GenerateRootRow(
		TSharedPtr<FVdjmAssetRegistryEditorRootRow> item,
		const TSharedRef<STableViewBase>& ownerTable);
	TSharedRef<ITableRow> GenerateMessageRow(
		TSharedPtr<FVdjmAssetRegistryMessage> item,
		const TSharedRef<STableViewBase>& ownerTable);
	void HandleAssetSelectionChanged(TSharedPtr<FVdjmAssetRegistryEditorAssetRow> item, ESelectInfo::Type selectInfo);
	void HandleRootSelectionChanged(TSharedPtr<FVdjmAssetRegistryEditorRootRow> item, ESelectInfo::Type selectInfo);

	void RefreshAll();
	void RefreshAssetRows();
	void RefreshRootRows();
	void RefreshMessageRows();
	void RefreshSummary();
	void SetMessages(const TArray<FVdjmAssetRegistryMessage>& messages);
	void SetStatus(const FString& statusText);
	void ApplySelectedField(const FString& fieldName, const FString& value);
	void ApplySelectedRootField(const FString& fieldName, const FString& value);

	bool PassesFilter(const FVdjmAssetRegistryAssetEntry& asset) const;
	bool TryApplyFolderToScanRequest(const FString& folderPath);
	bool TryResolveRootPath(const FString& rootKey, FString& outFullPath) const;
	FVdjmAssetRegistryScanRequest MakeCurrentScanRequest(bool bRegisterDiscoveredAssets) const;
	FString GetSelectedAssetField(const FString& fieldName) const;
	FString GetSelectedTagsText() const;
	FString GetSelectedMetaPurposeText() const;
	FString GetSelectedRootField(const FString& fieldName) const;
	FString NormalizeVirtualPathInput(const FString& value) const;
	FText GetStatusText() const;
	FText GetSummaryText() const;
	FText GetScanRootKeyText() const;
	FText GetScanRelativePathText() const;
	FText GetSelectedTypeText() const;
	FText GetSelectedVirtualPathText() const;
	FText GetSelectedImportanceText() const;
	FText GetSelectedClassText() const;
	FText GetSelectedTagsTextValue() const;
	FText GetSelectedPurposeText() const;
	FText GetSelectedRootKindText() const;
	FText GetSelectedRootKeyText() const;
	FText GetSelectedRootBaseText() const;
	FText GetSelectedRootRelativePathText() const;
	FText GetSelectedRootDefaultPathText() const;
	FText GetSelectedRootWinPathText() const;
	FText GetSelectedRootAndroidPathText() const;
	FText GetSelectedRootIosPathText() const;
	FText GetSelectedRootScanText() const;

	void HandleSearchTextChanged(const FText& text);
	void HandleTypeFilterChanged(const FText& text);
	void HandleVirtualFilterChanged(const FText& text);
	void HandleImportanceFilterChanged(const FText& text);
	void HandleScanRootKeyCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleScanRelativePathCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedTypeCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedVirtualPathCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedImportanceCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedClassCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedTagsCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedPurposeCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedRootKeyCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedRootBaseCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedRootRelativePathCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedRootDefaultPathCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedRootWinPathCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedRootAndroidPathCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedRootIosPathCommitted(const FText& text, ETextCommit::Type commitType);
	void HandleSelectedRootScanCommitted(const FText& text, ETextCommit::Type commitType);

	FVdjmAssetRegistryDocument Registry;
	FVdjmAssetRegistryScanResult LastScanResult;
	TArray<TSharedPtr<FVdjmAssetRegistryEditorAssetRow>> AssetRows;
	TArray<TSharedPtr<FVdjmAssetRegistryEditorRootRow>> RootRows;
	TArray<TSharedPtr<FVdjmAssetRegistryMessage>> MessageRows;
	TSharedPtr<class SListView<TSharedPtr<FVdjmAssetRegistryEditorAssetRow>>> AssetListView;
	TSharedPtr<class SListView<TSharedPtr<FVdjmAssetRegistryEditorRootRow>>> RootListView;
	TSharedPtr<class SListView<TSharedPtr<FVdjmAssetRegistryMessage>>> MessageListView;
	TSharedPtr<FVdjmAssetRegistryEditorAssetRow> SelectedRow;
	TSharedPtr<FVdjmAssetRegistryEditorRootRow> SelectedRootRow;
	FString ScanRootKey;
	FString ScanRelativePath;
	FString SearchText;
	FString TypeFilter;
	FString VirtualFilter;
	FString ImportanceFilter;
	FString StatusText;
	FString SummaryText;
};
