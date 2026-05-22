#include "SVdjmVcardPresetCatalogPanel.h"

#include "Animation/AnimationAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "SVdjmVcardPresetCatalogPanel"

namespace
{
	FString PresetKindToString(EVcardPresetAssetKind assetKind)
	{
		switch (assetKind)
		{
		case EVcardPresetAssetKind::ETexture:
			return TEXT("Texture");
		case EVcardPresetAssetKind::EImage:
			return TEXT("Image");
		case EVcardPresetAssetKind::EMaterial:
			return TEXT("Material");
		case EVcardPresetAssetKind::ESound:
			return TEXT("Sound");
		case EVcardPresetAssetKind::EAnimation:
			return TEXT("Animation");
		case EVcardPresetAssetKind::EMotion:
			return TEXT("Motion");
		case EVcardPresetAssetKind::EWidget:
			return TEXT("Widget");
		case EVcardPresetAssetKind::EDataAsset:
			return TEXT("DataAsset");
		case EVcardPresetAssetKind::EExternalFile:
			return TEXT("ExternalFile");
		case EVcardPresetAssetKind::ECustom:
			return TEXT("Custom");
		case EVcardPresetAssetKind::ENone:
		default:
			return TEXT("None");
		}
	}

	FText LabelText(const TCHAR* text)
	{
		return FText::FromString(text);
	}

	FName MakeSanitizedItemId(const FString& chunkKey, const FString& assetName)
	{
		FString itemId = chunkKey.IsEmpty()
			? assetName
			: FString::Printf(TEXT("%s.%s"), *chunkKey, *assetName);
		itemId.ReplaceInline(TEXT(" "), TEXT("_"));
		return FName(*itemId);
	}

	class SVdjmVcardPresetCatalogScanTableRow : public SMultiColumnTableRow<TSharedPtr<FVdjmVcardPresetCatalogScanRow>>
	{
	public:
		SLATE_BEGIN_ARGS(SVdjmVcardPresetCatalogScanTableRow) {}
			SLATE_ARGUMENT(TSharedPtr<FVdjmVcardPresetCatalogScanRow>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& inArgs, const TSharedRef<STableViewBase>& ownerTable)
		{
			Item = inArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FVdjmVcardPresetCatalogScanRow>>::Construct(
				FSuperRowType::FArguments(),
				ownerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& columnName) override
		{
			FString text;
			if (Item.IsValid())
			{
				if (columnName == TEXT("Name"))
				{
					text = Item->AssetName;
				}
				else if (columnName == TEXT("Class"))
				{
					text = Item->AssetClass;
				}
				else if (columnName == TEXT("Path"))
				{
					text = Item->ObjectPath;
				}
			}

			return SNew(STextBlock).Text(FText::FromString(text));
		}

	private:
		TSharedPtr<FVdjmVcardPresetCatalogScanRow> Item;
	};
}

void SVdjmVcardPresetCatalogPanel::Construct(const FArguments& inArgs)
{
	RefreshKindOptions();
	RefreshSummary();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBorder)
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock).Text(LabelText(TEXT("Catalog")))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SVdjmVcardPresetCatalogPanel::GetCatalogPathText)
						.OnTextCommitted(this, &SVdjmVcardPresetCatalogPanel::HandleCatalogPathCommitted)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LabelText(TEXT("Use Selected")))
						.OnClicked(this, &SVdjmVcardPresetCatalogPanel::HandleUseSelectedCatalogClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LabelText(TEXT("Load")))
						.OnClicked(this, &SVdjmVcardPresetCatalogPanel::HandleLoadCatalogClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LabelText(TEXT("Save Asset")))
						.OnClicked(this, &SVdjmVcardPresetCatalogPanel::HandleSaveCatalogClicked)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock).Text(LabelText(TEXT("Folder")))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SEditableTextBox)
						.Text(FText::FromString(ScanFolderPath))
						.OnTextCommitted(this, &SVdjmVcardPresetCatalogPanel::HandleScanFolderCommitted)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LabelText(TEXT("Scan Folder")))
						.OnClicked(this, &SVdjmVcardPresetCatalogPanel::HandleScanFolderClicked)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SEditableTextBox)
						.HintText(LabelText(TEXT("ChunkKey")))
						.Text(FText::FromString(ChunkKeyText))
						.OnTextCommitted(this, &SVdjmVcardPresetCatalogPanel::HandleChunkKeyCommitted)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SEditableTextBox)
						.HintText(LabelText(TEXT("GroupKey")))
						.Text(FText::FromString(GroupKeyText))
						.OnTextCommitted(this, &SVdjmVcardPresetCatalogPanel::HandleGroupKeyCommitted)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.8f)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SEditableTextBox)
						.HintText(LabelText(TEXT("SlotKey")))
						.Text(FText::FromString(SlotKeyText))
						.OnTextCommitted(this, &SVdjmVcardPresetCatalogPanel::HandleSlotKeyCommitted)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SAssignNew(KindComboBox, SComboBox<TSharedPtr<EVcardPresetAssetKind>>)
						.OptionsSource(&KindOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<EVcardPresetAssetKind> item)
						{
							return SNew(STextBlock)
								.Text(item.IsValid() ? FText::FromString(PresetKindToString(*item)) : LabelText(TEXT("None")));
						})
						.OnSelectionChanged(this, &SVdjmVcardPresetCatalogPanel::HandleKindSelectionChanged)
						[
							SNew(STextBlock).Text(this, &SVdjmVcardPresetCatalogPanel::GetSelectedKindText)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LabelText(TEXT("Replace Chunk")))
						.OnClicked(this, &SVdjmVcardPresetCatalogPanel::HandleReplaceChunkClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LabelText(TEXT("Append")))
						.OnClicked(this, &SVdjmVcardPresetCatalogPanel::HandleAppendChunkClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LabelText(TEXT("Clear Chunk")))
						.OnClicked(this, &SVdjmVcardPresetCatalogPanel::HandleClearChunkClicked)
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f)
		[
			SAssignNew(ScanListView, SListView<TSharedPtr<FVdjmVcardPresetCatalogScanRow>>)
			.ListItemsSource(&ScanRows)
			.OnGenerateRow(this, &SVdjmVcardPresetCatalogPanel::GenerateScanRow)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(TEXT("Name")).DefaultLabel(LabelText(TEXT("Name"))).FillWidth(0.25f)
				+ SHeaderRow::Column(TEXT("Class")).DefaultLabel(LabelText(TEXT("Class"))).FillWidth(0.20f)
				+ SHeaderRow::Column(TEXT("Path")).DefaultLabel(LabelText(TEXT("Object Path"))).FillWidth(0.55f)
			)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock).Text(this, &SVdjmVcardPresetCatalogPanel::GetSummaryText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Text(this, &SVdjmVcardPresetCatalogPanel::GetStatusText)
			]
		]
	];
}

FReply SVdjmVcardPresetCatalogPanel::HandleUseSelectedCatalogClicked()
{
	FContentBrowserModule& contentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FAssetData> selectedAssets;
	contentBrowserModule.Get().GetSelectedAssets(selectedAssets);

	for (const FAssetData& assetData : selectedAssets)
	{
		if (UVcardPresetCatalogDataAsset* selectedCatalog = Cast<UVcardPresetCatalogDataAsset>(assetData.GetAsset()))
		{
			CatalogDataAsset = selectedCatalog;
			CatalogObjectPath = assetData.GetSoftObjectPath().ToString();
			RefreshSummary();
			SetStatus(FString::Printf(TEXT("Loaded selected catalog: %s"), *CatalogObjectPath));
			return FReply::Handled();
		}
	}

	SetStatus(TEXT("No selected UVcardPresetCatalogDataAsset found in Content Browser."));
	return FReply::Handled();
}

FReply SVdjmVcardPresetCatalogPanel::HandleLoadCatalogClicked()
{
	if (!LoadCatalogFromPath(CatalogObjectPath))
	{
		SetStatus(FString::Printf(TEXT("Failed to load catalog: %s"), *CatalogObjectPath));
	}

	return FReply::Handled();
}

FReply SVdjmVcardPresetCatalogPanel::HandleScanFolderClicked()
{
	ScanFolder();
	return FReply::Handled();
}

FReply SVdjmVcardPresetCatalogPanel::HandleReplaceChunkClicked()
{
	ApplyCatalogMutation(true);
	return FReply::Handled();
}

FReply SVdjmVcardPresetCatalogPanel::HandleAppendChunkClicked()
{
	ApplyCatalogMutation(false);
	return FReply::Handled();
}

FReply SVdjmVcardPresetCatalogPanel::HandleClearChunkClicked()
{
	UVcardPresetCatalogDataAsset* catalog = CatalogDataAsset.Get();
	if (!IsValid(catalog))
	{
		SetStatus(TEXT("Catalog is not loaded."));
		return FReply::Handled();
	}

	catalog->Modify();
	if (catalog->ClearChunk(GetChunkKey()))
	{
		catalog->MarkPackageDirty();
		RefreshSummary();
		SetStatus(FString::Printf(TEXT("Cleared chunk: %s"), *GetChunkKey().ToString()));
	}
	else
	{
		SetStatus(FString::Printf(TEXT("Chunk not found: %s"), *GetChunkKey().ToString()));
	}

	return FReply::Handled();
}

FReply SVdjmVcardPresetCatalogPanel::HandleSaveCatalogClicked()
{
	UVcardPresetCatalogDataAsset* catalog = CatalogDataAsset.Get();
	if (!IsValid(catalog))
	{
		SetStatus(TEXT("Catalog is not loaded."));
		return FReply::Handled();
	}

	TArray<UPackage*> packagesToSave;
	packagesToSave.Add(catalog->GetOutermost());
	const bool bSaved = FEditorFileUtils::PromptForCheckoutAndSave(packagesToSave, false, false) == FEditorFileUtils::EPromptReturnCode::PR_Success;
	SetStatus(bSaved ? TEXT("Catalog package saved.") : TEXT("Catalog package save was cancelled or failed."));
	return FReply::Handled();
}

TSharedRef<ITableRow> SVdjmVcardPresetCatalogPanel::GenerateScanRow(
	TSharedPtr<FVdjmVcardPresetCatalogScanRow> item,
	const TSharedRef<STableViewBase>& ownerTable)
{
	return SNew(SVdjmVcardPresetCatalogScanTableRow, ownerTable)
		.Item(item);
}

void SVdjmVcardPresetCatalogPanel::RefreshScanRows()
{
	if (ScanListView.IsValid())
	{
		ScanListView->RequestListRefresh();
	}
}

void SVdjmVcardPresetCatalogPanel::RefreshSummary()
{
	const UVcardPresetCatalogDataAsset* catalog = CatalogDataAsset.Get();
	if (!IsValid(catalog))
	{
		SummaryText = FString::Printf(TEXT("Catalog: [not loaded] | ScanRows: %d"), ScanRows.Num());
		return;
	}

	SummaryText = FString::Printf(
		TEXT("Catalog: %s | TotalItems: %d | Chunks: %d | ScanRows: %d"),
		*GetNameSafe(catalog),
		catalog->GetPresetItems().Num(),
		catalog->GetPresetChunks().Num(),
		ScanRows.Num());
}

void SVdjmVcardPresetCatalogPanel::SetStatus(const FString& statusText)
{
	StatusText = statusText;
	RefreshSummary();
}

bool SVdjmVcardPresetCatalogPanel::LoadCatalogFromPath(const FString& catalogObjectPath)
{
	if (catalogObjectPath.IsEmpty())
	{
		return false;
	}

	UObject* loadedObject = StaticLoadObject(UVcardPresetCatalogDataAsset::StaticClass(), nullptr, *catalogObjectPath);
	UVcardPresetCatalogDataAsset* loadedCatalog = Cast<UVcardPresetCatalogDataAsset>(loadedObject);
	if (!IsValid(loadedCatalog))
	{
		return false;
	}

	CatalogDataAsset = loadedCatalog;
	CatalogObjectPath = catalogObjectPath;
	RefreshSummary();
	SetStatus(FString::Printf(TEXT("Loaded catalog: %s"), *catalogObjectPath));
	return true;
}

bool SVdjmVcardPresetCatalogPanel::ScanFolder()
{
	ScanRows.Reset();

	if (!ScanFolderPath.StartsWith(TEXT("/Game")))
	{
		SetStatus(TEXT("Scan folder must start with /Game."));
		RefreshScanRows();
		return false;
	}

	FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter filter;
	filter.PackagePaths.Add(FName(*ScanFolderPath));
	filter.bRecursivePaths = true;

	TArray<FAssetData> assetDatas;
	assetRegistryModule.Get().GetAssets(filter, assetDatas);
	for (const FAssetData& assetData : assetDatas)
	{
		if (!IsAssetAcceptedForSelectedKind(assetData))
		{
			continue;
		}

		TSharedPtr<FVdjmVcardPresetCatalogScanRow> row = MakeShared<FVdjmVcardPresetCatalogScanRow>();
		row->AssetData = assetData;
		row->AssetName = assetData.AssetName.ToString();
		row->AssetClass = assetData.AssetClassPath.GetAssetName().ToString();
		row->ObjectPath = assetData.GetSoftObjectPath().ToString();
		ScanRows.Add(row);
	}

	RefreshScanRows();
	SetStatus(FString::Printf(TEXT("Scanned %d assets from %s."), ScanRows.Num(), *ScanFolderPath));
	return ScanRows.Num() > 0;
}

bool SVdjmVcardPresetCatalogPanel::BuildPresetItemsFromScanRows(
	TArray<FVcardPresetItemData>& outPresetItems,
	FString& outErrorReason) const
{
	outPresetItems.Reset();
	outErrorReason.Reset();

	if (ScanRows.Num() <= 0)
	{
		outErrorReason = TEXT("No scanned assets to register.");
		return false;
	}

	const FName chunkKey = GetChunkKey();
	const FName groupKey = GetGroupKey();
	const FName slotKey = GetSlotKey();
	const EVcardPresetAssetKind selectedKind = GetSelectedKind();

	for (const TSharedPtr<FVdjmVcardPresetCatalogScanRow>& row : ScanRows)
	{
		if (!row.IsValid())
		{
			continue;
		}

		const FSoftObjectPath assetPath = row->AssetData.GetSoftObjectPath();
		FVcardPresetAssetRef assetRef;
		assetRef.SlotKey = slotKey;
		assetRef.AssetKind = selectedKind;
		assetRef.AssetObject = TSoftObjectPtr<UObject>(assetPath);
		assetRef.AssetPath = assetPath;
		assetRef.bRequired = true;

		FVcardPresetItemData presetItemData;
		presetItemData.ItemId = MakeSanitizedItemId(chunkKey.ToString(), row->AssetName);
		presetItemData.DisplayName = FText::FromString(row->AssetName);
		presetItemData.GroupKey = groupKey;
		presetItemData.PrimaryKind = selectedKind;
		presetItemData.PrimaryAssetSlotKey = slotKey;
		presetItemData.SelectSignalTag = NAME_None;
		presetItemData.Assets.Add(assetRef);

		if (selectedKind == EVcardPresetAssetKind::ETexture || selectedKind == EVcardPresetAssetKind::EImage)
		{
			presetItemData.ThumbnailTexture = TSoftObjectPtr<UTexture2D>(assetPath);
		}

		outPresetItems.Add(presetItemData);
	}

	return outPresetItems.Num() > 0;
}

bool SVdjmVcardPresetCatalogPanel::IsAssetAcceptedForSelectedKind(const FAssetData& assetData) const
{
	UClass* assetClass = assetData.GetClass();
	if (assetClass == nullptr)
	{
		return false;
	}

	switch (SelectedKind)
	{
	case EVcardPresetAssetKind::ETexture:
	case EVcardPresetAssetKind::EImage:
		return assetClass->IsChildOf(UTexture2D::StaticClass());
	case EVcardPresetAssetKind::EMaterial:
		return assetClass->IsChildOf(UMaterialInterface::StaticClass());
	case EVcardPresetAssetKind::ESound:
		return assetClass->IsChildOf(USoundBase::StaticClass());
	case EVcardPresetAssetKind::EAnimation:
	case EVcardPresetAssetKind::EMotion:
		return assetClass->IsChildOf(UAnimationAsset::StaticClass()) ||
			assetClass->GetName().Contains(TEXT("Sequence")) ||
			assetClass->GetName().Contains(TEXT("Animation"));
	case EVcardPresetAssetKind::EDataAsset:
		return assetClass->IsChildOf(UDataAsset::StaticClass());
	case EVcardPresetAssetKind::EWidget:
		return assetClass->GetName().Contains(TEXT("Widget"));
	case EVcardPresetAssetKind::ECustom:
		return true;
	case EVcardPresetAssetKind::EExternalFile:
	case EVcardPresetAssetKind::ENone:
	default:
		return false;
	}
}

void SVdjmVcardPresetCatalogPanel::ApplyCatalogMutation(bool bReplace)
{
	UVcardPresetCatalogDataAsset* catalog = CatalogDataAsset.Get();
	if (!IsValid(catalog))
	{
		SetStatus(TEXT("Catalog is not loaded."));
		return;
	}

	TArray<FVcardPresetItemData> presetItems;
	FString errorReason;
	if (!BuildPresetItemsFromScanRows(presetItems, errorReason))
	{
		SetStatus(errorReason);
		return;
	}

	catalog->Modify();
	if (bReplace)
	{
		catalog->ReplaceChunkItems(
			GetChunkKey(),
			FText::FromName(GetChunkKey()),
			GetGroupKey(),
			GetSelectedKind(),
			GetSlotKey(),
			presetItems);
	}
	else
	{
		catalog->AppendChunkItems(
			GetChunkKey(),
			FText::FromName(GetChunkKey()),
			GetGroupKey(),
			GetSelectedKind(),
			GetSlotKey(),
			presetItems);
	}

	catalog->MarkPackageDirty();
	RefreshSummary();
	SetStatus(FString::Printf(
		TEXT("%s %d items into chunk '%s'."),
		bReplace ? TEXT("Replaced") : TEXT("Appended"),
		presetItems.Num(),
		*GetChunkKey().ToString()));
}

void SVdjmVcardPresetCatalogPanel::RefreshKindOptions()
{
	KindOptions.Reset();
	KindOptions.Add(MakeShared<EVcardPresetAssetKind>(EVcardPresetAssetKind::ETexture));
	KindOptions.Add(MakeShared<EVcardPresetAssetKind>(EVcardPresetAssetKind::EImage));
	KindOptions.Add(MakeShared<EVcardPresetAssetKind>(EVcardPresetAssetKind::EMaterial));
	KindOptions.Add(MakeShared<EVcardPresetAssetKind>(EVcardPresetAssetKind::ESound));
	KindOptions.Add(MakeShared<EVcardPresetAssetKind>(EVcardPresetAssetKind::EAnimation));
	KindOptions.Add(MakeShared<EVcardPresetAssetKind>(EVcardPresetAssetKind::EMotion));
	KindOptions.Add(MakeShared<EVcardPresetAssetKind>(EVcardPresetAssetKind::EWidget));
	KindOptions.Add(MakeShared<EVcardPresetAssetKind>(EVcardPresetAssetKind::EDataAsset));
	KindOptions.Add(MakeShared<EVcardPresetAssetKind>(EVcardPresetAssetKind::ECustom));
}

FName SVdjmVcardPresetCatalogPanel::GetChunkKey() const
{
	return FName(*ChunkKeyText);
}

FName SVdjmVcardPresetCatalogPanel::GetGroupKey() const
{
	return FName(*GroupKeyText);
}

FName SVdjmVcardPresetCatalogPanel::GetSlotKey() const
{
	return FName(*SlotKeyText);
}

EVcardPresetAssetKind SVdjmVcardPresetCatalogPanel::GetSelectedKind() const
{
	return SelectedKind;
}

FText SVdjmVcardPresetCatalogPanel::GetCatalogPathText() const
{
	return FText::FromString(CatalogObjectPath);
}

FText SVdjmVcardPresetCatalogPanel::GetStatusText() const
{
	return FText::FromString(StatusText);
}

FText SVdjmVcardPresetCatalogPanel::GetSummaryText() const
{
	return FText::FromString(SummaryText);
}

FText SVdjmVcardPresetCatalogPanel::GetSelectedKindText() const
{
	return FText::FromString(PresetKindToString(SelectedKind));
}

void SVdjmVcardPresetCatalogPanel::HandleCatalogPathCommitted(const FText& text, ETextCommit::Type commitType)
{
	CatalogObjectPath = text.ToString();
}

void SVdjmVcardPresetCatalogPanel::HandleScanFolderCommitted(const FText& text, ETextCommit::Type commitType)
{
	ScanFolderPath = text.ToString();
}

void SVdjmVcardPresetCatalogPanel::HandleChunkKeyCommitted(const FText& text, ETextCommit::Type commitType)
{
	ChunkKeyText = text.ToString();
}

void SVdjmVcardPresetCatalogPanel::HandleGroupKeyCommitted(const FText& text, ETextCommit::Type commitType)
{
	GroupKeyText = text.ToString();
}

void SVdjmVcardPresetCatalogPanel::HandleSlotKeyCommitted(const FText& text, ETextCommit::Type commitType)
{
	SlotKeyText = text.ToString();
}

void SVdjmVcardPresetCatalogPanel::HandleKindSelectionChanged(TSharedPtr<EVcardPresetAssetKind> item, ESelectInfo::Type selectInfo)
{
	if (item.IsValid())
	{
		SelectedKind = *item;
	}
}

#undef LOCTEXT_NAMESPACE
