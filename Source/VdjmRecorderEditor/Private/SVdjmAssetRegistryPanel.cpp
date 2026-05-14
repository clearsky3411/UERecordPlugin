#include "SVdjmAssetRegistryPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "VdjmAssetRegistryBlueprintLibrary.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SVdjmAssetRegistryPanel"

namespace
{
	FString StripVirtualToken(const FString& value)
	{
		if (value.StartsWith(TEXT("#{")) && value.EndsWith(TEXT("}")))
		{
			return value.Mid(2, value.Len() - 3);
		}
		return value;
	}

	FString SeverityToString(const EVdjmAssetRegistryMessageSeverity severity)
	{
		switch (severity)
		{
		case EVdjmAssetRegistryMessageSeverity::Error:
			return TEXT("Error");
		case EVdjmAssetRegistryMessageSeverity::Warning:
			return TEXT("Warning");
		default:
			return TEXT("Info");
		}
	}

	FSlateColor SeverityToColor(const EVdjmAssetRegistryMessageSeverity severity)
	{
		switch (severity)
		{
		case EVdjmAssetRegistryMessageSeverity::Error:
			return FSlateColor(FLinearColor(0.95f, 0.25f, 0.18f, 1.0f));
		case EVdjmAssetRegistryMessageSeverity::Warning:
			return FSlateColor(FLinearColor(1.0f, 0.72f, 0.18f, 1.0f));
		default:
			return FSlateColor(FLinearColor(0.55f, 0.75f, 1.0f, 1.0f));
		}
	}

	class SVdjmAssetRegistryAssetTableRow : public SMultiColumnTableRow<TSharedPtr<FVdjmAssetRegistryEditorAssetRow>>
	{
	public:
		SLATE_BEGIN_ARGS(SVdjmAssetRegistryAssetTableRow) {}
			SLATE_ARGUMENT(TSharedPtr<FVdjmAssetRegistryEditorAssetRow>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& inArgs, const TSharedRef<STableViewBase>& ownerTable)
		{
			Item = inArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FVdjmAssetRegistryEditorAssetRow>>::Construct(
				FSuperRowType::FArguments().Padding(1.0f),
				ownerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& columnName) override
		{
			FString text;
			if (Item.IsValid())
			{
				if (columnName == TEXT("AssetKey"))
				{
					text = Item->AssetKey;
				}
				else if (columnName == TEXT("Type"))
				{
					text = Item->Type;
				}
				else if (columnName == TEXT("Root"))
				{
					text = Item->Root;
				}
				else if (columnName == TEXT("RelativePath"))
				{
					text = Item->RelativePath;
				}
				else if (columnName == TEXT("VirtualPath"))
				{
					text = StripVirtualToken(Item->VirtualPath);
				}
				else if (columnName == TEXT("Importance"))
				{
					text = Item->Importance;
				}
				else if (columnName == TEXT("Class"))
				{
					text = Item->Class;
				}
			}

			return SNew(STextBlock)
				.Text(FText::FromString(text))
				.ToolTipText(FText::FromString(text));
		}

	private:
		TSharedPtr<FVdjmAssetRegistryEditorAssetRow> Item;
	};

	class SVdjmAssetRegistryMessageTableRow : public SMultiColumnTableRow<TSharedPtr<FVdjmAssetRegistryMessage>>
	{
	public:
		SLATE_BEGIN_ARGS(SVdjmAssetRegistryMessageTableRow) {}
			SLATE_ARGUMENT(TSharedPtr<FVdjmAssetRegistryMessage>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& inArgs, const TSharedRef<STableViewBase>& ownerTable)
		{
			Item = inArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FVdjmAssetRegistryMessage>>::Construct(
				FSuperRowType::FArguments().Padding(1.0f),
				ownerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& columnName) override
		{
			FString text;
			FSlateColor color = FSlateColor::UseForeground();
			if (Item.IsValid())
			{
				if (columnName == TEXT("Severity"))
				{
					text = SeverityToString(Item->Severity);
					color = SeverityToColor(Item->Severity);
				}
				else if (columnName == TEXT("Code"))
				{
					text = Item->Code;
				}
				else if (columnName == TEXT("Path"))
				{
					text = Item->Path;
				}
				else if (columnName == TEXT("Message"))
				{
					text = Item->Message;
				}
			}

			return SNew(STextBlock)
				.ColorAndOpacity(color)
				.Text(FText::FromString(text))
				.ToolTipText(FText::FromString(text));
		}

	private:
		TSharedPtr<FVdjmAssetRegistryMessage> Item;
	};
}

void SVdjmAssetRegistryPanel::Construct(const FArguments& inArgs)
{
	StatusText = TEXT("Ready");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Load", "Load"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandleLoadClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Validate", "Validate"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandleValidateClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Scan", "Scan"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandleScanClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ScanRegister", "Scan + Register"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandleScanRegisterClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Save", "Save"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandleSaveClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenJson", "Open JSON"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandleOpenJsonClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CopySummary", "Copy Summary"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandleCopySummaryClicked)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(8.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SVdjmAssetRegistryPanel::GetStatusText)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.35f).Padding(2.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("SearchHint", "Search asset key/path"))
				.OnTextChanged(this, &SVdjmAssetRegistryPanel::HandleSearchTextChanged)
			]
			+ SHorizontalBox::Slot().FillWidth(0.2f).Padding(2.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("TypeFilterHint", "Type filter"))
				.OnTextChanged(this, &SVdjmAssetRegistryPanel::HandleTypeFilterChanged)
			]
			+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(2.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("VirtualFilterHint", "Virtual filter"))
				.OnTextChanged(this, &SVdjmAssetRegistryPanel::HandleVirtualFilterChanged)
			]
			+ SHorizontalBox::Slot().FillWidth(0.2f).Padding(2.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("ImportanceFilterHint", "Importance filter"))
				.OnTextChanged(this, &SVdjmAssetRegistryPanel::HandleImportanceFilterChanged)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(6.0f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot()
			.Value(0.66f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(AssetListView, SListView<TSharedPtr<FVdjmAssetRegistryEditorAssetRow>>)
					.ListItemsSource(&AssetRows)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SVdjmAssetRegistryPanel::GenerateAssetRow)
					.OnSelectionChanged(this, &SVdjmAssetRegistryPanel::HandleAssetSelectionChanged)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(TEXT("AssetKey")).DefaultLabel(LOCTEXT("AssetKey", "Asset Key")).FillWidth(0.23f)
						+ SHeaderRow::Column(TEXT("Type")).DefaultLabel(LOCTEXT("Type", "Type")).FillWidth(0.1f)
						+ SHeaderRow::Column(TEXT("Root")).DefaultLabel(LOCTEXT("Root", "Root")).FillWidth(0.1f)
						+ SHeaderRow::Column(TEXT("RelativePath")).DefaultLabel(LOCTEXT("RelativePath", "Relative Path")).FillWidth(0.22f)
						+ SHeaderRow::Column(TEXT("VirtualPath")).DefaultLabel(LOCTEXT("VirtualPath", "Virtual")).FillWidth(0.16f)
						+ SHeaderRow::Column(TEXT("Importance")).DefaultLabel(LOCTEXT("Importance", "Importance")).FillWidth(0.09f)
						+ SHeaderRow::Column(TEXT("Class")).DefaultLabel(LOCTEXT("Class", "Class")).FillWidth(0.1f)
					)
				]
			]
			+ SSplitter::Slot()
			.Value(0.34f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SelectedAsset", "Selected Asset"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("SelectedTypeHint", "type"))
					.Text(this, &SVdjmAssetRegistryPanel::GetSelectedTypeText)
					.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedTypeCommitted)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("SelectedVirtualHint", "virtual path, e.g. recorder-preview-carousel"))
					.Text(this, &SVdjmAssetRegistryPanel::GetSelectedVirtualPathText)
					.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedVirtualPathCommitted)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("SelectedImportanceHint", "required / recommended / optional"))
					.Text(this, &SVdjmAssetRegistryPanel::GetSelectedImportanceText)
					.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedImportanceCommitted)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("SelectedClassHint", "class path"))
					.Text(this, &SVdjmAssetRegistryPanel::GetSelectedClassText)
					.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedClassCommitted)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("SelectedTagsHint", "tags, comma separated"))
					.Text(this, &SVdjmAssetRegistryPanel::GetSelectedTagsTextValue)
					.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedTagsCommitted)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("SelectedPurposeHint", "meta purpose"))
					.Text(this, &SVdjmAssetRegistryPanel::GetSelectedPurposeText)
					.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedPurposeCommitted)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RemoveSelected", "Remove Entry"))
					.OnClicked(this, &SVdjmAssetRegistryPanel::HandleRemoveSelectedClicked)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
				[
					SNew(SSeparator)
				]
				+ SVerticalBox::Slot()
				.FillHeight(0.45f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(this, &SVdjmAssetRegistryPanel::GetSummaryText)
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(0.55f)
				[
					SAssignNew(MessageListView, SListView<TSharedPtr<FVdjmAssetRegistryMessage>>)
					.ListItemsSource(&MessageRows)
					.OnGenerateRow(this, &SVdjmAssetRegistryPanel::GenerateMessageRow)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(TEXT("Severity")).DefaultLabel(LOCTEXT("MsgSeverity", "Severity")).FillWidth(0.14f)
						+ SHeaderRow::Column(TEXT("Code")).DefaultLabel(LOCTEXT("MsgCode", "Code")).FillWidth(0.22f)
						+ SHeaderRow::Column(TEXT("Path")).DefaultLabel(LOCTEXT("MsgPath", "Path")).FillWidth(0.24f)
						+ SHeaderRow::Column(TEXT("Message")).DefaultLabel(LOCTEXT("MsgMessage", "Message")).FillWidth(0.4f)
					)
				]
			]
		]
	];

	HandleLoadClicked();
}

FReply SVdjmAssetRegistryPanel::HandleLoadClicked()
{
	TArray<FVdjmAssetRegistryMessage> messages;
	if (UVdjmAssetRegistryBlueprintLibrary::LoadDefaultRegistry(Registry, messages))
	{
		SetStatus(TEXT("Loaded registry."));
	}
	else
	{
		SetStatus(TEXT("Load failed."));
	}
	SetMessages(messages);
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleValidateClicked()
{
	TArray<FVdjmAssetRegistryMessage> messages;
	if (UVdjmAssetRegistryBlueprintLibrary::ValidateRegistry(Registry, messages))
	{
		SetStatus(TEXT("Validation completed without errors."));
	}
	else
	{
		SetStatus(TEXT("Validation found errors."));
	}
	SetMessages(messages);
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleScanClicked()
{
	TArray<FVdjmAssetRegistryMessage> messages;
	UVdjmAssetRegistryBlueprintLibrary::ScanDefaultRegistry(false, false, Registry, LastScanResult, messages);
	SetStatus(FString::Printf(
		TEXT("Scanned %d files. Missing registered assets: %d."),
		LastScanResult.ScannedFileCount,
		LastScanResult.MissingRegisteredAssetCount));
	SetMessages(messages);
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleScanRegisterClicked()
{
	TArray<FVdjmAssetRegistryMessage> messages;
	UVdjmAssetRegistryBlueprintLibrary::ScanDefaultRegistry(true, false, Registry, LastScanResult, messages);
	SetStatus(FString::Printf(
		TEXT("Scanned %d files. Added %d, updated %d. Save to persist."),
		LastScanResult.ScannedFileCount,
		LastScanResult.AddedAssetCount,
		LastScanResult.UpdatedAssetCount));
	SetMessages(messages);
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleSaveClicked()
{
	TArray<FVdjmAssetRegistryMessage> messages;
	if (UVdjmAssetRegistryBlueprintLibrary::SaveDefaultRegistry(Registry, messages))
	{
		SetStatus(TEXT("Saved registry JSON."));
	}
	else
	{
		SetStatus(TEXT("Save failed."));
	}
	SetMessages(messages);
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleOpenJsonClicked()
{
	FString registryPath;
	UVdjmAssetRegistryBlueprintLibrary::GetDefaultRegistryFilePath(registryPath);
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*registryPath);
	SetStatus(FString::Printf(TEXT("Opened %s"), *registryPath));
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleCopySummaryClicked()
{
	RefreshSummary();
	FPlatformApplicationMisc::ClipboardCopy(*SummaryText);
	SetStatus(TEXT("Summary copied to clipboard."));
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleRemoveSelectedClicked()
{
	if (!SelectedRow.IsValid() || !Registry.Assets.IsValidIndex(SelectedRow->SourceIndex))
	{
		SetStatus(TEXT("No selected asset entry."));
		return FReply::Handled();
	}

	const FString assetKey = SelectedRow->AssetKey;
	Registry.Assets.RemoveAt(SelectedRow->SourceIndex);
	SelectedRow.Reset();
	SetStatus(FString::Printf(TEXT("Removed entry %s. Save to persist."), *assetKey));
	RefreshAll();
	return FReply::Handled();
}

TSharedRef<ITableRow> SVdjmAssetRegistryPanel::GenerateAssetRow(
	TSharedPtr<FVdjmAssetRegistryEditorAssetRow> item,
	const TSharedRef<STableViewBase>& ownerTable)
{
	return SNew(SVdjmAssetRegistryAssetTableRow, ownerTable)
		.Item(item);
}

TSharedRef<ITableRow> SVdjmAssetRegistryPanel::GenerateMessageRow(
	TSharedPtr<FVdjmAssetRegistryMessage> item,
	const TSharedRef<STableViewBase>& ownerTable)
{
	return SNew(SVdjmAssetRegistryMessageTableRow, ownerTable)
		.Item(item);
}

void SVdjmAssetRegistryPanel::HandleAssetSelectionChanged(TSharedPtr<FVdjmAssetRegistryEditorAssetRow> item, ESelectInfo::Type selectInfo)
{
	SelectedRow = item;
}

void SVdjmAssetRegistryPanel::RefreshAll()
{
	RefreshAssetRows();
	RefreshSummary();
	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}
	if (MessageListView.IsValid())
	{
		MessageListView->RequestListRefresh();
	}
}

void SVdjmAssetRegistryPanel::RefreshAssetRows()
{
	AssetRows.Reset();
	for (int32 assetIndex = 0; assetIndex < Registry.Assets.Num(); ++assetIndex)
	{
		const FVdjmAssetRegistryAssetEntry& asset = Registry.Assets[assetIndex];
		if (!PassesFilter(asset))
		{
			continue;
		}

		TSharedPtr<FVdjmAssetRegistryEditorAssetRow> row = MakeShared<FVdjmAssetRegistryEditorAssetRow>();
		row->SourceIndex = assetIndex;
		row->AssetKey = UVdjmAssetRegistryBlueprintLibrary::MakeAssetKey(asset.Type, asset.Root, asset.RelativePath);
		row->Type = asset.Type;
		row->Root = asset.Root;
		row->RelativePath = asset.RelativePath;
		row->VirtualPath = asset.VirtualPath;
		row->Importance = asset.Importance;
		row->Class = asset.Class;
		AssetRows.Add(row);
	}
}

void SVdjmAssetRegistryPanel::RefreshMessageRows()
{
	if (MessageListView.IsValid())
	{
		MessageListView->RequestListRefresh();
	}
}

void SVdjmAssetRegistryPanel::RefreshSummary()
{
	UVdjmAssetRegistryBlueprintLibrary::GetRegistrySummary(Registry, SummaryText);
}

void SVdjmAssetRegistryPanel::SetMessages(const TArray<FVdjmAssetRegistryMessage>& messages)
{
	MessageRows.Reset();
	for (const FVdjmAssetRegistryMessage& message : messages)
	{
		MessageRows.Add(MakeShared<FVdjmAssetRegistryMessage>(message));
	}
	RefreshMessageRows();
}

void SVdjmAssetRegistryPanel::SetStatus(const FString& statusText)
{
	StatusText = statusText;
}

void SVdjmAssetRegistryPanel::ApplySelectedField(const FString& fieldName, const FString& value)
{
	if (!SelectedRow.IsValid() || !Registry.Assets.IsValidIndex(SelectedRow->SourceIndex))
	{
		SetStatus(TEXT("No selected asset entry."));
		return;
	}

	FVdjmAssetRegistryAssetEntry& asset = Registry.Assets[SelectedRow->SourceIndex];
	if (fieldName == TEXT("type"))
	{
		asset.Type = value;
		asset.AssetKey = UVdjmAssetRegistryBlueprintLibrary::MakeAssetKey(asset.Type, asset.Root, asset.RelativePath);
	}
	else if (fieldName == TEXT("virtual_path"))
	{
		asset.VirtualPath = NormalizeVirtualPathInput(value);
	}
	else if (fieldName == TEXT("importance"))
	{
		asset.Importance = value.IsEmpty() ? TEXT("optional") : value;
	}
	else if (fieldName == TEXT("class"))
	{
		asset.Class = value;
	}
	else if (fieldName == TEXT("tags"))
	{
		asset.Tags.Reset();
		TArray<FString> parts;
		value.ParseIntoArray(parts, TEXT(","), true);
		for (FString part : parts)
		{
			part.TrimStartAndEndInline();
			if (!part.IsEmpty())
			{
				asset.Tags.Add(part);
			}
		}
	}
	else if (fieldName == TEXT("purpose"))
	{
		if (value.IsEmpty())
		{
			asset.Meta.Remove(TEXT("purpose"));
		}
		else
		{
			asset.Meta.Add(TEXT("purpose"), value);
		}
	}

	SetStatus(FString::Printf(TEXT("Updated %s. Save to persist."), *fieldName));
	RefreshAll();
}

bool SVdjmAssetRegistryPanel::PassesFilter(const FVdjmAssetRegistryAssetEntry& asset) const
{
	const FString virtualPath = StripVirtualToken(asset.VirtualPath);
	const FString assetKey = UVdjmAssetRegistryBlueprintLibrary::MakeAssetKey(asset.Type, asset.Root, asset.RelativePath);

	if (!SearchText.IsEmpty()
		&& !assetKey.Contains(SearchText)
		&& !asset.RelativePath.Contains(SearchText)
		&& !asset.Class.Contains(SearchText))
	{
		return false;
	}
	if (!TypeFilter.IsEmpty() && !asset.Type.Contains(TypeFilter))
	{
		return false;
	}
	if (!VirtualFilter.IsEmpty() && !virtualPath.Contains(VirtualFilter))
	{
		return false;
	}
	if (!ImportanceFilter.IsEmpty() && !asset.Importance.Contains(ImportanceFilter))
	{
		return false;
	}
	return true;
}

FString SVdjmAssetRegistryPanel::GetSelectedAssetField(const FString& fieldName) const
{
	if (!SelectedRow.IsValid() || !Registry.Assets.IsValidIndex(SelectedRow->SourceIndex))
	{
		return FString();
	}

	const FVdjmAssetRegistryAssetEntry& asset = Registry.Assets[SelectedRow->SourceIndex];
	if (fieldName == TEXT("type"))
	{
		return asset.Type;
	}
	if (fieldName == TEXT("virtual_path"))
	{
		return StripVirtualToken(asset.VirtualPath);
	}
	if (fieldName == TEXT("importance"))
	{
		return asset.Importance;
	}
	if (fieldName == TEXT("class"))
	{
		return asset.Class;
	}
	return FString();
}

FString SVdjmAssetRegistryPanel::GetSelectedTagsText() const
{
	if (!SelectedRow.IsValid() || !Registry.Assets.IsValidIndex(SelectedRow->SourceIndex))
	{
		return FString();
	}
	return FString::Join(Registry.Assets[SelectedRow->SourceIndex].Tags, TEXT(", "));
}

FString SVdjmAssetRegistryPanel::GetSelectedMetaPurposeText() const
{
	if (!SelectedRow.IsValid() || !Registry.Assets.IsValidIndex(SelectedRow->SourceIndex))
	{
		return FString();
	}
	const FString* purpose = Registry.Assets[SelectedRow->SourceIndex].Meta.Find(TEXT("purpose"));
	return purpose != nullptr ? *purpose : FString();
}

FString SVdjmAssetRegistryPanel::NormalizeVirtualPathInput(const FString& value) const
{
	FString trimmedValue = value;
	trimmedValue.TrimStartAndEndInline();
	if (trimmedValue.IsEmpty() || (trimmedValue.StartsWith(TEXT("#{")) && trimmedValue.EndsWith(TEXT("}"))))
	{
		return trimmedValue;
	}
	return FString::Printf(TEXT("#{%s}"), *trimmedValue);
}

FText SVdjmAssetRegistryPanel::GetStatusText() const
{
	return FText::FromString(StatusText);
}

FText SVdjmAssetRegistryPanel::GetSummaryText() const
{
	return FText::FromString(SummaryText);
}

FText SVdjmAssetRegistryPanel::GetSelectedTypeText() const
{
	return FText::FromString(GetSelectedAssetField(TEXT("type")));
}

FText SVdjmAssetRegistryPanel::GetSelectedVirtualPathText() const
{
	return FText::FromString(GetSelectedAssetField(TEXT("virtual_path")));
}

FText SVdjmAssetRegistryPanel::GetSelectedImportanceText() const
{
	return FText::FromString(GetSelectedAssetField(TEXT("importance")));
}

FText SVdjmAssetRegistryPanel::GetSelectedClassText() const
{
	return FText::FromString(GetSelectedAssetField(TEXT("class")));
}

FText SVdjmAssetRegistryPanel::GetSelectedTagsTextValue() const
{
	return FText::FromString(GetSelectedTagsText());
}

FText SVdjmAssetRegistryPanel::GetSelectedPurposeText() const
{
	return FText::FromString(GetSelectedMetaPurposeText());
}

void SVdjmAssetRegistryPanel::HandleSearchTextChanged(const FText& text)
{
	SearchText = text.ToString();
	RefreshAll();
}

void SVdjmAssetRegistryPanel::HandleTypeFilterChanged(const FText& text)
{
	TypeFilter = text.ToString();
	RefreshAll();
}

void SVdjmAssetRegistryPanel::HandleVirtualFilterChanged(const FText& text)
{
	VirtualFilter = text.ToString();
	RefreshAll();
}

void SVdjmAssetRegistryPanel::HandleImportanceFilterChanged(const FText& text)
{
	ImportanceFilter = text.ToString();
	RefreshAll();
}

void SVdjmAssetRegistryPanel::HandleSelectedTypeCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedField(TEXT("type"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedVirtualPathCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedField(TEXT("virtual_path"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedImportanceCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedField(TEXT("importance"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedClassCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedField(TEXT("class"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedTagsCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedField(TEXT("tags"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedPurposeCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedField(TEXT("purpose"), text.ToString());
}

#undef LOCTEXT_NAMESPACE
