#include "SVdjmAssetRegistryPanel.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "VdjmAssetRegistryBlueprintLibrary.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
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

	bool ParseBoolText(const FString& value)
	{
		const FString normalizedValue = value.TrimStartAndEnd().ToLower();
		return normalizedValue == TEXT("true")
			|| normalizedValue == TEXT("1")
			|| normalizedValue == TEXT("yes")
			|| normalizedValue == TEXT("y");
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

	class SVdjmAssetRegistryRootTableRow : public SMultiColumnTableRow<TSharedPtr<FVdjmAssetRegistryEditorRootRow>>
	{
	public:
		SLATE_BEGIN_ARGS(SVdjmAssetRegistryRootTableRow) {}
			SLATE_ARGUMENT(TSharedPtr<FVdjmAssetRegistryEditorRootRow>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& inArgs, const TSharedRef<STableViewBase>& ownerTable)
		{
			Item = inArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FVdjmAssetRegistryEditorRootRow>>::Construct(
				FSuperRowType::FArguments().Padding(1.0f),
				ownerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& columnName) override
		{
			FString text;
			if (Item.IsValid())
			{
				if (columnName == TEXT("Kind"))
				{
					text = Item->Kind;
				}
				else if (columnName == TEXT("Key"))
				{
					text = Item->Key;
				}
				else if (columnName == TEXT("Path"))
				{
					text = Item->Kind == TEXT("defined")
						? FString::Printf(TEXT("%s/%s"), *Item->Root, *Item->RelativePath)
						: (Item->WinPath.IsEmpty() ? Item->DefaultPath : Item->WinPath);
				}
				else if (columnName == TEXT("Scan"))
				{
					text = Item->bScanText;
				}
			}

			return SNew(STextBlock)
				.Text(FText::FromString(text))
				.ToolTipText(FText::FromString(text));
		}

	private:
		TSharedPtr<FVdjmAssetRegistryEditorRootRow> Item;
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
				.Text(LOCTEXT("PickScanFolder", "Pick Folder"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandlePickScanFolderClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ScanFolder", "Scan Folder"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandleScanFolderClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ScanFolderRegister", "Folder + Register"))
				.OnClicked(this, &SVdjmAssetRegistryPanel::HandleScanFolderRegisterClicked)
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
		.AutoHeight()
		.Padding(6.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ScanScopeLabel", "Scan Scope"))
			]
			+ SHorizontalBox::Slot().FillWidth(0.28f).Padding(2.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("ScanRootKeyHint", "root key"))
				.Text(this, &SVdjmAssetRegistryPanel::GetScanRootKeyText)
				.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleScanRootKeyCommitted)
			]
			+ SHorizontalBox::Slot().FillWidth(0.72f).Padding(2.0f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("ScanRelativePathHint", "relative folder under root"))
				.Text(this, &SVdjmAssetRegistryPanel::GetScanRelativePathText)
				.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleScanRelativePathCommitted)
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
				.FillHeight(0.34f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RootSettings", "Root Settings"))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("AddRoot", "Add Root"))
							.OnClicked(this, &SVdjmAssetRegistryPanel::HandleAddRootClicked)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("AddExternalRoot", "Add External"))
							.OnClicked(this, &SVdjmAssetRegistryPanel::HandleAddExternalRootClicked)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("AddDefinedRoot", "Add Defined"))
							.OnClicked(this, &SVdjmAssetRegistryPanel::HandleAddDefinedRootClicked)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("RemoveRoot", "Remove"))
							.OnClicked(this, &SVdjmAssetRegistryPanel::HandleRemoveRootClicked)
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(0.45f)
					[
						SAssignNew(RootListView, SListView<TSharedPtr<FVdjmAssetRegistryEditorRootRow>>)
						.ListItemsSource(&RootRows)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SVdjmAssetRegistryPanel::GenerateRootRow)
						.OnSelectionChanged(this, &SVdjmAssetRegistryPanel::HandleRootSelectionChanged)
						.HeaderRow
						(
							SNew(SHeaderRow)
							+ SHeaderRow::Column(TEXT("Kind")).DefaultLabel(LOCTEXT("RootKind", "Kind")).FillWidth(0.18f)
							+ SHeaderRow::Column(TEXT("Key")).DefaultLabel(LOCTEXT("RootKey", "Key")).FillWidth(0.22f)
							+ SHeaderRow::Column(TEXT("Path")).DefaultLabel(LOCTEXT("RootPath", "Path")).FillWidth(0.46f)
							+ SHeaderRow::Column(TEXT("Scan")).DefaultLabel(LOCTEXT("RootScan", "Scan")).FillWidth(0.14f)
						)
					]
					+ SVerticalBox::Slot()
					.FillHeight(0.55f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RootKindHint", "kind"))
								.IsReadOnly(true)
								.Text(this, &SVdjmAssetRegistryPanel::GetSelectedRootKindText)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RootKeyHint", "key"))
								.Text(this, &SVdjmAssetRegistryPanel::GetSelectedRootKeyText)
								.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedRootKeyCommitted)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RootBaseHint", "defined base root"))
								.Text(this, &SVdjmAssetRegistryPanel::GetSelectedRootBaseText)
								.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedRootBaseCommitted)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RootRelativeHint", "defined relative path"))
								.Text(this, &SVdjmAssetRegistryPanel::GetSelectedRootRelativePathText)
								.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedRootRelativePathCommitted)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RootDefaultPathHint", "default path"))
								.Text(this, &SVdjmAssetRegistryPanel::GetSelectedRootDefaultPathText)
								.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedRootDefaultPathCommitted)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RootWinPathHint", "win path"))
								.Text(this, &SVdjmAssetRegistryPanel::GetSelectedRootWinPathText)
								.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedRootWinPathCommitted)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RootAndroidPathHint", "android path"))
								.Text(this, &SVdjmAssetRegistryPanel::GetSelectedRootAndroidPathText)
								.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedRootAndroidPathCommitted)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RootIosPathHint", "ios path"))
								.Text(this, &SVdjmAssetRegistryPanel::GetSelectedRootIosPathText)
								.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedRootIosPathCommitted)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RootScanHint", "scan true/false"))
								.Text(this, &SVdjmAssetRegistryPanel::GetSelectedRootScanText)
								.OnTextCommitted(this, &SVdjmAssetRegistryPanel::HandleSelectedRootScanCommitted)
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(0.24f)
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
				.FillHeight(0.42f)
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
	FVdjmAssetRegistryScanRequest scanRequest;
	scanRequest.bUseEnabledRoots = true;
	UVdjmAssetRegistryBlueprintLibrary::ScanRegistryWithRequest(scanRequest, Registry, LastScanResult, messages);
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
	FVdjmAssetRegistryScanRequest scanRequest;
	scanRequest.bRegisterDiscoveredAssets = true;
	scanRequest.bUseEnabledRoots = true;
	UVdjmAssetRegistryBlueprintLibrary::ScanRegistryWithRequest(scanRequest, Registry, LastScanResult, messages);
	SetStatus(FString::Printf(
		TEXT("Scanned %d files. Added %d, updated %d. Save to persist."),
		LastScanResult.ScannedFileCount,
		LastScanResult.AddedAssetCount,
		LastScanResult.UpdatedAssetCount));
	SetMessages(messages);
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandlePickScanFolderClicked()
{
	IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
	if (desktopPlatform == nullptr)
	{
		SetStatus(TEXT("Desktop platform module is not available."));
		return FReply::Handled();
	}

	FString folderPath;
	const void* parentWindowHandle = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
		: nullptr;
	if (!desktopPlatform->OpenDirectoryDialog(parentWindowHandle, TEXT("Select Vdjm registry scan folder"), FString(), folderPath))
	{
		return FReply::Handled();
	}

	if (TryApplyFolderToScanRequest(folderPath))
	{
		SetStatus(FString::Printf(TEXT("Scan scope set to %s:%s."), *ScanRootKey, *ScanRelativePath));
	}
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleScanFolderClicked()
{
	TArray<FVdjmAssetRegistryMessage> messages;
	UVdjmAssetRegistryBlueprintLibrary::ScanRegistryWithRequest(
		MakeCurrentScanRequest(false),
		Registry,
		LastScanResult,
		messages);
	SetStatus(FString::Printf(
		TEXT("Scanned folder %s:%s. Missing scoped assets: %d."),
		*ScanRootKey,
		*ScanRelativePath,
		LastScanResult.MissingRegisteredAssetCount));
	SetMessages(messages);
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleScanFolderRegisterClicked()
{
	TArray<FVdjmAssetRegistryMessage> messages;
	UVdjmAssetRegistryBlueprintLibrary::ScanRegistryWithRequest(
		MakeCurrentScanRequest(true),
		Registry,
		LastScanResult,
		messages);
	SetStatus(FString::Printf(
		TEXT("Scanned folder %s:%s. Added %d, updated %d. Save to persist."),
		*ScanRootKey,
		*ScanRelativePath,
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

FReply SVdjmAssetRegistryPanel::HandleAddRootClicked()
{
	FVdjmAssetRegistryPathRoot newRoot;
	newRoot.Key = FString::Printf(TEXT("root_%d"), Registry.Roots.Num() + 1);
	newRoot.DefaultPath = TEXT("Content");
	newRoot.bScan = true;
	Registry.Roots.Add(newRoot);
	SetStatus(TEXT("Added project root. Edit fields and Save to persist."));
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleAddExternalRootClicked()
{
	FVdjmAssetRegistryPathRoot newRoot;
	newRoot.Key = FString::Printf(TEXT("external_%d"), Registry.ExternalPaths.Num() + 1);
	newRoot.bScan = true;
	Registry.ExternalPaths.Add(newRoot);
	SetStatus(TEXT("Added external root. Pick or enter an absolute path, then Save."));
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleAddDefinedRootClicked()
{
	FVdjmAssetRegistryDefinedRoot newRoot;
	newRoot.Key = FString::Printf(TEXT("defined_%d"), Registry.DefinedRoots.Num() + 1);
	newRoot.Root = Registry.Roots.Num() > 0 ? Registry.Roots[0].Key : FString();
	newRoot.bScan = true;
	Registry.DefinedRoots.Add(newRoot);
	SetStatus(TEXT("Added defined root. Set base root/relative path and Save."));
	RefreshAll();
	return FReply::Handled();
}

FReply SVdjmAssetRegistryPanel::HandleRemoveRootClicked()
{
	if (!SelectedRootRow.IsValid())
	{
		SetStatus(TEXT("No selected root entry."));
		return FReply::Handled();
	}

	const FString rootKey = SelectedRootRow->Key;
	if (SelectedRootRow->Kind == TEXT("root") && Registry.Roots.IsValidIndex(SelectedRootRow->SourceIndex))
	{
		Registry.Roots.RemoveAt(SelectedRootRow->SourceIndex);
	}
	else if (SelectedRootRow->Kind == TEXT("external") && Registry.ExternalPaths.IsValidIndex(SelectedRootRow->SourceIndex))
	{
		Registry.ExternalPaths.RemoveAt(SelectedRootRow->SourceIndex);
	}
	else if (SelectedRootRow->Kind == TEXT("defined") && Registry.DefinedRoots.IsValidIndex(SelectedRootRow->SourceIndex))
	{
		Registry.DefinedRoots.RemoveAt(SelectedRootRow->SourceIndex);
	}
	SelectedRootRow.Reset();
	SetStatus(FString::Printf(TEXT("Removed root %s. Save to persist."), *rootKey));
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

TSharedRef<ITableRow> SVdjmAssetRegistryPanel::GenerateRootRow(
	TSharedPtr<FVdjmAssetRegistryEditorRootRow> item,
	const TSharedRef<STableViewBase>& ownerTable)
{
	return SNew(SVdjmAssetRegistryRootTableRow, ownerTable)
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

void SVdjmAssetRegistryPanel::HandleRootSelectionChanged(TSharedPtr<FVdjmAssetRegistryEditorRootRow> item, ESelectInfo::Type selectInfo)
{
	SelectedRootRow = item;
	if (SelectedRootRow.IsValid())
	{
		ScanRootKey = SelectedRootRow->Key;
		ScanRelativePath = FString();
	}
}

void SVdjmAssetRegistryPanel::RefreshAll()
{
	RefreshAssetRows();
	RefreshRootRows();
	RefreshSummary();
	if (AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}
	if (RootListView.IsValid())
	{
		RootListView->RequestListRefresh();
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

void SVdjmAssetRegistryPanel::RefreshRootRows()
{
	RootRows.Reset();
	for (int32 rootIndex = 0; rootIndex < Registry.Roots.Num(); ++rootIndex)
	{
		const FVdjmAssetRegistryPathRoot& root = Registry.Roots[rootIndex];
		TSharedPtr<FVdjmAssetRegistryEditorRootRow> row = MakeShared<FVdjmAssetRegistryEditorRootRow>();
		row->SourceIndex = rootIndex;
		row->Kind = TEXT("root");
		row->Key = root.Key;
		row->DefaultPath = root.DefaultPath;
		row->WinPath = root.WinPath;
		row->AndroidPath = root.AndroidPath;
		row->IosPath = root.IosPath;
		row->bScanText = root.bScan ? TEXT("true") : TEXT("false");
		RootRows.Add(row);
	}

	for (int32 rootIndex = 0; rootIndex < Registry.ExternalPaths.Num(); ++rootIndex)
	{
		const FVdjmAssetRegistryPathRoot& root = Registry.ExternalPaths[rootIndex];
		TSharedPtr<FVdjmAssetRegistryEditorRootRow> row = MakeShared<FVdjmAssetRegistryEditorRootRow>();
		row->SourceIndex = rootIndex;
		row->Kind = TEXT("external");
		row->Key = root.Key;
		row->DefaultPath = root.DefaultPath;
		row->WinPath = root.WinPath;
		row->AndroidPath = root.AndroidPath;
		row->IosPath = root.IosPath;
		row->bScanText = root.bScan ? TEXT("true") : TEXT("false");
		RootRows.Add(row);
	}

	for (int32 rootIndex = 0; rootIndex < Registry.DefinedRoots.Num(); ++rootIndex)
	{
		const FVdjmAssetRegistryDefinedRoot& root = Registry.DefinedRoots[rootIndex];
		TSharedPtr<FVdjmAssetRegistryEditorRootRow> row = MakeShared<FVdjmAssetRegistryEditorRootRow>();
		row->SourceIndex = rootIndex;
		row->Kind = TEXT("defined");
		row->Key = root.Key;
		row->Root = root.Root;
		row->RelativePath = root.RelativePath;
		row->bScanText = root.bScan ? TEXT("true") : TEXT("false");
		RootRows.Add(row);
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

void SVdjmAssetRegistryPanel::ApplySelectedRootField(const FString& fieldName, const FString& value)
{
	if (!SelectedRootRow.IsValid())
	{
		SetStatus(TEXT("No selected root entry."));
		return;
	}

	FString trimmedValue = value;
	trimmedValue.TrimStartAndEndInline();
	if (SelectedRootRow->Kind == TEXT("root") && Registry.Roots.IsValidIndex(SelectedRootRow->SourceIndex))
	{
		FVdjmAssetRegistryPathRoot& root = Registry.Roots[SelectedRootRow->SourceIndex];
		if (fieldName == TEXT("key"))
		{
			root.Key = trimmedValue;
			ScanRootKey = root.Key;
		}
		else if (fieldName == TEXT("default_path"))
		{
			root.DefaultPath = trimmedValue;
		}
		else if (fieldName == TEXT("win_path"))
		{
			root.WinPath = trimmedValue;
		}
		else if (fieldName == TEXT("android_path"))
		{
			root.AndroidPath = trimmedValue;
		}
		else if (fieldName == TEXT("ios_path"))
		{
			root.IosPath = trimmedValue;
		}
		else if (fieldName == TEXT("scan"))
		{
			root.bScan = ParseBoolText(trimmedValue);
		}
	}
	else if (SelectedRootRow->Kind == TEXT("external") && Registry.ExternalPaths.IsValidIndex(SelectedRootRow->SourceIndex))
	{
		FVdjmAssetRegistryPathRoot& root = Registry.ExternalPaths[SelectedRootRow->SourceIndex];
		if (fieldName == TEXT("key"))
		{
			root.Key = trimmedValue;
			ScanRootKey = root.Key;
		}
		else if (fieldName == TEXT("default_path"))
		{
			root.DefaultPath = trimmedValue;
		}
		else if (fieldName == TEXT("win_path"))
		{
			root.WinPath = trimmedValue;
		}
		else if (fieldName == TEXT("android_path"))
		{
			root.AndroidPath = trimmedValue;
		}
		else if (fieldName == TEXT("ios_path"))
		{
			root.IosPath = trimmedValue;
		}
		else if (fieldName == TEXT("scan"))
		{
			root.bScan = ParseBoolText(trimmedValue);
		}
	}
	else if (SelectedRootRow->Kind == TEXT("defined") && Registry.DefinedRoots.IsValidIndex(SelectedRootRow->SourceIndex))
	{
		FVdjmAssetRegistryDefinedRoot& root = Registry.DefinedRoots[SelectedRootRow->SourceIndex];
		if (fieldName == TEXT("key"))
		{
			root.Key = trimmedValue;
			ScanRootKey = root.Key;
		}
		else if (fieldName == TEXT("base_root"))
		{
			root.Root = trimmedValue;
		}
		else if (fieldName == TEXT("relative_path"))
		{
			root.RelativePath = trimmedValue;
		}
		else if (fieldName == TEXT("scan"))
		{
			root.bScan = ParseBoolText(trimmedValue);
		}
	}

	SetStatus(FString::Printf(TEXT("Updated root %s. Save to persist."), *fieldName));
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

bool SVdjmAssetRegistryPanel::TryApplyFolderToScanRequest(const FString& folderPath)
{
	FString normalizedFolderPath = FPaths::ConvertRelativePathToFull(folderPath);
	FPaths::NormalizeDirectoryName(normalizedFolderPath);

	FString bestRootKey;
	FString bestRootPath;
	for (const TSharedPtr<FVdjmAssetRegistryEditorRootRow>& rootRow : RootRows)
	{
		if (!rootRow.IsValid())
		{
			continue;
		}

		FString rootPath;
		if (!TryResolveRootPath(rootRow->Key, rootPath))
		{
			continue;
		}

		if (normalizedFolderPath == rootPath || normalizedFolderPath.StartsWith(rootPath + TEXT("/")))
		{
			if (rootPath.Len() > bestRootPath.Len())
			{
				bestRootKey = rootRow->Key;
				bestRootPath = rootPath;
			}
		}
	}

	if (bestRootKey.IsEmpty())
	{
		SetStatus(TEXT("Selected folder is outside registered roots. Add an external root first."));
		return false;
	}

	FString relativePath = normalizedFolderPath;
	FPaths::MakePathRelativeTo(relativePath, *bestRootPath);
	FPaths::NormalizeFilename(relativePath);
	relativePath.RemoveFromStart(TEXT("./"));
	if (relativePath == TEXT("."))
	{
		relativePath.Reset();
	}
	ScanRootKey = bestRootKey;
	ScanRelativePath = relativePath;
	return true;
}

bool SVdjmAssetRegistryPanel::TryResolveRootPath(const FString& rootKey, FString& outFullPath) const
{
	TArray<FVdjmAssetRegistryMessage> messages;
	if (!UVdjmAssetRegistryBlueprintLibrary::ResolveRegistryRootFullPath(Registry, rootKey, outFullPath, messages))
	{
		return false;
	}
	FPaths::NormalizeDirectoryName(outFullPath);
	return true;
}

FVdjmAssetRegistryScanRequest SVdjmAssetRegistryPanel::MakeCurrentScanRequest(bool bRegisterDiscoveredAssets) const
{
	FVdjmAssetRegistryScanRequest scanRequest;
	scanRequest.bRegisterDiscoveredAssets = bRegisterDiscoveredAssets;
	scanRequest.bSaveAfterScan = false;
	scanRequest.bUseEnabledRoots = ScanRootKey.IsEmpty();
	scanRequest.bCheckMissingRegisteredAssets = true;
	scanRequest.RootKey = ScanRootKey;
	scanRequest.RelativePath = ScanRelativePath;
	return scanRequest;
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

FString SVdjmAssetRegistryPanel::GetSelectedRootField(const FString& fieldName) const
{
	if (!SelectedRootRow.IsValid())
	{
		return FString();
	}

	if (SelectedRootRow->Kind == TEXT("root") && Registry.Roots.IsValidIndex(SelectedRootRow->SourceIndex))
	{
		const FVdjmAssetRegistryPathRoot& root = Registry.Roots[SelectedRootRow->SourceIndex];
		if (fieldName == TEXT("kind")) return SelectedRootRow->Kind;
		if (fieldName == TEXT("key")) return root.Key;
		if (fieldName == TEXT("default_path")) return root.DefaultPath;
		if (fieldName == TEXT("win_path")) return root.WinPath;
		if (fieldName == TEXT("android_path")) return root.AndroidPath;
		if (fieldName == TEXT("ios_path")) return root.IosPath;
		if (fieldName == TEXT("scan")) return root.bScan ? TEXT("true") : TEXT("false");
	}
	else if (SelectedRootRow->Kind == TEXT("external") && Registry.ExternalPaths.IsValidIndex(SelectedRootRow->SourceIndex))
	{
		const FVdjmAssetRegistryPathRoot& root = Registry.ExternalPaths[SelectedRootRow->SourceIndex];
		if (fieldName == TEXT("kind")) return SelectedRootRow->Kind;
		if (fieldName == TEXT("key")) return root.Key;
		if (fieldName == TEXT("default_path")) return root.DefaultPath;
		if (fieldName == TEXT("win_path")) return root.WinPath;
		if (fieldName == TEXT("android_path")) return root.AndroidPath;
		if (fieldName == TEXT("ios_path")) return root.IosPath;
		if (fieldName == TEXT("scan")) return root.bScan ? TEXT("true") : TEXT("false");
	}
	else if (SelectedRootRow->Kind == TEXT("defined") && Registry.DefinedRoots.IsValidIndex(SelectedRootRow->SourceIndex))
	{
		const FVdjmAssetRegistryDefinedRoot& root = Registry.DefinedRoots[SelectedRootRow->SourceIndex];
		if (fieldName == TEXT("kind")) return SelectedRootRow->Kind;
		if (fieldName == TEXT("key")) return root.Key;
		if (fieldName == TEXT("base_root")) return root.Root;
		if (fieldName == TEXT("relative_path")) return root.RelativePath;
		if (fieldName == TEXT("scan")) return root.bScan ? TEXT("true") : TEXT("false");
	}

	return FString();
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

FText SVdjmAssetRegistryPanel::GetScanRootKeyText() const
{
	return FText::FromString(ScanRootKey);
}

FText SVdjmAssetRegistryPanel::GetScanRelativePathText() const
{
	return FText::FromString(ScanRelativePath);
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

FText SVdjmAssetRegistryPanel::GetSelectedRootKindText() const
{
	return FText::FromString(GetSelectedRootField(TEXT("kind")));
}

FText SVdjmAssetRegistryPanel::GetSelectedRootKeyText() const
{
	return FText::FromString(GetSelectedRootField(TEXT("key")));
}

FText SVdjmAssetRegistryPanel::GetSelectedRootBaseText() const
{
	return FText::FromString(GetSelectedRootField(TEXT("base_root")));
}

FText SVdjmAssetRegistryPanel::GetSelectedRootRelativePathText() const
{
	return FText::FromString(GetSelectedRootField(TEXT("relative_path")));
}

FText SVdjmAssetRegistryPanel::GetSelectedRootDefaultPathText() const
{
	return FText::FromString(GetSelectedRootField(TEXT("default_path")));
}

FText SVdjmAssetRegistryPanel::GetSelectedRootWinPathText() const
{
	return FText::FromString(GetSelectedRootField(TEXT("win_path")));
}

FText SVdjmAssetRegistryPanel::GetSelectedRootAndroidPathText() const
{
	return FText::FromString(GetSelectedRootField(TEXT("android_path")));
}

FText SVdjmAssetRegistryPanel::GetSelectedRootIosPathText() const
{
	return FText::FromString(GetSelectedRootField(TEXT("ios_path")));
}

FText SVdjmAssetRegistryPanel::GetSelectedRootScanText() const
{
	return FText::FromString(GetSelectedRootField(TEXT("scan")));
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

void SVdjmAssetRegistryPanel::HandleScanRootKeyCommitted(const FText& text, ETextCommit::Type commitType)
{
	ScanRootKey = text.ToString().TrimStartAndEnd();
}

void SVdjmAssetRegistryPanel::HandleScanRelativePathCommitted(const FText& text, ETextCommit::Type commitType)
{
	ScanRelativePath = text.ToString().TrimStartAndEnd();
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

void SVdjmAssetRegistryPanel::HandleSelectedRootKeyCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedRootField(TEXT("key"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedRootBaseCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedRootField(TEXT("base_root"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedRootRelativePathCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedRootField(TEXT("relative_path"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedRootDefaultPathCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedRootField(TEXT("default_path"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedRootWinPathCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedRootField(TEXT("win_path"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedRootAndroidPathCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedRootField(TEXT("android_path"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedRootIosPathCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedRootField(TEXT("ios_path"), text.ToString());
}

void SVdjmAssetRegistryPanel::HandleSelectedRootScanCommitted(const FText& text, ETextCommit::Type commitType)
{
	ApplySelectedRootField(TEXT("scan"), text.ToString());
}

#undef LOCTEXT_NAMESPACE
