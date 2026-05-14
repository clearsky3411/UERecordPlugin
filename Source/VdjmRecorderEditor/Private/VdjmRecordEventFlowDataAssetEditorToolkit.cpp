#include "VdjmRecordEventFlowDataAssetEditorToolkit.h"

#include "HAL/PlatformApplicationMisc.h"
#include "IDetailsView.h"
#include "Misc/MessageDialog.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FVdjmRecordEventFlowDataAssetEditorToolkit"

namespace
{
	const FName EditorAppIdentifier(TEXT("VdjmRecordEventFlowDataAssetEditor"));
	const FName DetailsTabId(TEXT("VdjmRecordEventFlowDataAssetEditor_Details"));
	const FName JsonTabId(TEXT("VdjmRecordEventFlowDataAssetEditor_Json"));
	const FName SummaryTabId(TEXT("VdjmRecordEventFlowDataAssetEditor_Summary"));
	const FName GuideTabId(TEXT("VdjmRecordEventFlowDataAssetEditor_Guide"));
}

void FVdjmRecordEventFlowDataAssetEditorToolkit::InitEditor(
	const EToolkitMode::Type mode,
	const TSharedPtr<IToolkitHost>& initToolkitHost,
	UVdjmRecordEventFlowDataAsset* flowDataAsset)
{
	EditingAsset = flowDataAsset;
	SetStatusMessage(TEXT("FlowDataAsset editor is ready."));

	FPropertyEditorModule& propertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs detailsViewArgs;
	detailsViewArgs.bAllowSearch = true;
	detailsViewArgs.bHideSelectionTip = true;
	detailsViewArgs.bShowOptions = true;
	detailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = propertyEditorModule.CreateDetailView(detailsViewArgs);
	DetailsView->SetObject(EditingAsset);

	const TSharedRef<FTabManager::FLayout> defaultLayout =
		FTabManager::NewLayout(TEXT("VdjmRecordEventFlowDataAssetEditor_Layout_v1"))
		->AddArea(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.45f)
				->AddTab(DetailsTabId, ETabState::OpenedTab))
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.55f)
				->AddTab(JsonTabId, ETabState::OpenedTab)
				->AddTab(SummaryTabId, ETabState::OpenedTab)
				->AddTab(GuideTabId, ETabState::OpenedTab)
				->SetForegroundTab(JsonTabId)));

	InitAssetEditor(
		mode,
		initToolkitHost,
		EditorAppIdentifier,
		defaultLayout,
		true,
		true,
		EditingAsset.Get());

	RefreshAllViews();
}

void FVdjmRecordEventFlowDataAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& tabManager)
{
	WorkspaceMenuCategory = tabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenuCategory", "Vdjm Event Flow"));
	const TSharedRef<FWorkspaceItem> workspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(tabManager);

	tabManager->RegisterTabSpawner(
		DetailsTabId,
		FOnSpawnTab::CreateSP(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(workspaceMenuCategoryRef);

	tabManager->RegisterTabSpawner(
		JsonTabId,
		FOnSpawnTab::CreateSP(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::SpawnJsonTab))
		.SetDisplayName(LOCTEXT("JsonTab", "JSON"))
		.SetGroup(workspaceMenuCategoryRef);

	tabManager->RegisterTabSpawner(
		SummaryTabId,
		FOnSpawnTab::CreateSP(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::SpawnSummaryTab))
		.SetDisplayName(LOCTEXT("SummaryTab", "Summary"))
		.SetGroup(workspaceMenuCategoryRef);

	tabManager->RegisterTabSpawner(
		GuideTabId,
		FOnSpawnTab::CreateSP(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::SpawnGuideTab))
		.SetDisplayName(LOCTEXT("GuideTab", "Guide"))
		.SetGroup(workspaceMenuCategoryRef);
}

void FVdjmRecordEventFlowDataAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& tabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(tabManager);

	tabManager->UnregisterTabSpawner(DetailsTabId);
	tabManager->UnregisterTabSpawner(JsonTabId);
	tabManager->UnregisterTabSpawner(SummaryTabId);
	tabManager->UnregisterTabSpawner(GuideTabId);
}

FName FVdjmRecordEventFlowDataAssetEditorToolkit::GetToolkitFName() const
{
	return EditorAppIdentifier;
}

FText FVdjmRecordEventFlowDataAssetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Vdjm Record Event Flow");
}

FString FVdjmRecordEventFlowDataAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("VdjmEventFlow");
}

FLinearColor FVdjmRecordEventFlowDataAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.22f, 0.58f, 0.72f, 1.0f);
}

void FVdjmRecordEventFlowDataAssetEditorToolkit::AddReferencedObjects(FReferenceCollector& collector)
{
	collector.AddReferencedObject(EditingAsset);
}

FString FVdjmRecordEventFlowDataAssetEditorToolkit::GetReferencerName() const
{
	return TEXT("FVdjmRecordEventFlowDataAssetEditorToolkit");
}

TSharedRef<SDockTab> FVdjmRecordEventFlowDataAssetEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& args)
{
	const TSharedRef<SWidget> detailsWidget = DetailsView.IsValid()
		? StaticCastSharedRef<SWidget>(DetailsView.ToSharedRef())
		: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(LOCTEXT("MissingDetailsView", "DetailsView is not available.")));

	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabLabel", "Details"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT(
					"DetailsHelp",
					"Root Events와 Subgraphs는 여기서 기존 DataAsset 방식 그대로 편집합니다. JSON/Summary 탭은 현재 asset 상태를 보기 좋게 뽑는 보조 창입니다."))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				detailsWidget
			]
		];
}

TSharedRef<SDockTab> FVdjmRecordEventFlowDataAssetEditorToolkit::SpawnJsonTab(const FSpawnTabArgs& args)
{
	const FString initialJson = EditingAsset != nullptr
		? EditingAsset->ExportFlowToJsonString(true)
		: FString();

	return SNew(SDockTab)
		.Label(LOCTEXT("JsonTabLabel", "JSON"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshJsonButton", "Refresh JSON"))
					.ToolTipText(LOCTEXT("RefreshJsonTooltip", "현재 DataAsset 상태를 다시 JSON으로 출력합니다."))
					.OnClicked(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::OnRefreshJsonClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CopyJsonButton", "Copy JSON"))
					.ToolTipText(LOCTEXT("CopyJsonTooltip", "현재 JSON 텍스트를 클립보드에 복사합니다."))
					.OnClicked(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::OnCopyJsonClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ValidateJsonButton", "Validate JSON"))
					.ToolTipText(LOCTEXT("ValidateJsonTooltip", "현재 JSON 텍스트가 FlowDataAsset으로 import 가능한지 검사합니다."))
					.OnClicked(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::OnValidateJsonClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("ImportJsonButton", "Import JSON To Asset"))
					.ToolTipText(LOCTEXT("ImportJsonTooltip", "현재 JSON 텍스트로 DataAsset을 덮어씁니다. 확인창을 거칩니다."))
					.OnClicked(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::OnImportJsonClicked)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f, 0.0f, 6.0f, 6.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::GetStatusText)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(6.0f)
			[
				SAssignNew(JsonTextBox, SMultiLineEditableTextBox)
				.Text(FText::FromString(initialJson))
				.AutoWrapText(false)
				.IsReadOnly(false)
			]
		];
}

TSharedRef<SDockTab> FVdjmRecordEventFlowDataAssetEditorToolkit::SpawnSummaryTab(const FSpawnTabArgs& args)
{
	const FString initialSummary = EditingAsset != nullptr
		? EditingAsset->EventSummary
		: FString();

	return SNew(SDockTab)
		.Label(LOCTEXT("SummaryTabLabel", "Summary"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshSummaryButton", "Refresh Summary"))
				.ToolTipText(LOCTEXT("RefreshSummaryTooltip", "Root Events와 Subgraphs의 이벤트 목록/주요 tag/key 상태를 다시 요약합니다."))
				.OnClicked(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::OnRefreshSummaryClicked)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(6.0f)
			[
				SAssignNew(SummaryTextBox, SMultiLineEditableTextBox)
				.Text(FText::FromString(initialSummary))
				.AutoWrapText(false)
				.IsReadOnly(true)
			]
		];
}

TSharedRef<SDockTab> FVdjmRecordEventFlowDataAssetEditorToolkit::SpawnGuideTab(const FSpawnTabArgs& args)
{
	const FString initialGuide = EditingAsset != nullptr
		? FString::Printf(
			TEXT("%s%s%s%sAuthor Notes:%s%s"),
			*EditingAsset->FlowAuthoringGuide,
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			LINE_TERMINATOR,
			*EditingAsset->FlowAuthorNotes)
		: FString();

	return SNew(SDockTab)
		.Label(LOCTEXT("GuideTabLabel", "Guide"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshGuideButton", "Refresh Guide"))
					.OnClicked_Lambda([this]()
					{
						RefreshGuideText();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("ResetGuideButton", "Reset Guide"))
					.ToolTipText(LOCTEXT("ResetGuideTooltip", "FlowAuthoringGuide를 기본 설명으로 되돌립니다."))
					.OnClicked(this, &FVdjmRecordEventFlowDataAssetEditorToolkit::OnResetGuideClicked)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(6.0f)
			[
				SAssignNew(GuideTextBox, SMultiLineEditableTextBox)
				.Text(FText::FromString(initialGuide))
				.AutoWrapText(true)
				.IsReadOnly(true)
			]
		];
}

void FVdjmRecordEventFlowDataAssetEditorToolkit::RefreshAllViews()
{
	RefreshJsonText();
	RefreshSummaryText();
	RefreshGuideText();
}

void FVdjmRecordEventFlowDataAssetEditorToolkit::RefreshJsonText()
{
	if (EditingAsset == nullptr || not JsonTextBox.IsValid())
	{
		return;
	}

	JsonTextBox->SetText(FText::FromString(EditingAsset->ExportFlowToJsonString(true)));
}

void FVdjmRecordEventFlowDataAssetEditorToolkit::RefreshSummaryText()
{
	if (EditingAsset == nullptr)
	{
		return;
	}

	EditingAsset->RefreshEventSummary();
	if (SummaryTextBox.IsValid())
	{
		SummaryTextBox->SetText(FText::FromString(EditingAsset->EventSummary));
	}
}

void FVdjmRecordEventFlowDataAssetEditorToolkit::RefreshGuideText()
{
	if (EditingAsset == nullptr || not GuideTextBox.IsValid())
	{
		return;
	}

	const FString guideText = FString::Printf(
		TEXT("%s%s%s%sAuthor Notes:%s%s"),
		*EditingAsset->FlowAuthoringGuide,
		LINE_TERMINATOR,
		LINE_TERMINATOR,
		LINE_TERMINATOR,
		LINE_TERMINATOR,
		*EditingAsset->FlowAuthorNotes);
	GuideTextBox->SetText(FText::FromString(guideText));
}

void FVdjmRecordEventFlowDataAssetEditorToolkit::SetStatusMessage(const FString& statusMessage)
{
	LastStatusMessage = statusMessage;
}

FReply FVdjmRecordEventFlowDataAssetEditorToolkit::OnRefreshJsonClicked()
{
	RefreshJsonText();
	SetStatusMessage(TEXT("JSON refreshed from DataAsset."));
	return FReply::Handled();
}

FReply FVdjmRecordEventFlowDataAssetEditorToolkit::OnCopyJsonClicked()
{
	const FString jsonText = GetCurrentJsonText();
	FPlatformApplicationMisc::ClipboardCopy(*jsonText);
	SetStatusMessage(FString::Printf(TEXT("Copied JSON to clipboard. Length=%d"), jsonText.Len()));
	return FReply::Handled();
}

FReply FVdjmRecordEventFlowDataAssetEditorToolkit::OnValidateJsonClicked()
{
	if (EditingAsset == nullptr)
	{
		SetStatusMessage(TEXT("Validate failed. Editing asset is null."));
		return FReply::Handled();
	}

	FString errorReason;
	const bool bValidJson = EditingAsset->ValidateFlowJson(GetCurrentJsonText(), errorReason);
	SetStatusMessage(bValidJson
		? TEXT("JSON validation succeeded.")
		: FString::Printf(TEXT("JSON validation failed. %s"), *errorReason));
	return FReply::Handled();
}

FReply FVdjmRecordEventFlowDataAssetEditorToolkit::OnImportJsonClicked()
{
	if (EditingAsset == nullptr)
	{
		SetStatusMessage(TEXT("Import failed. Editing asset is null."));
		return FReply::Handled();
	}

	const EAppReturnType::Type answer = FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT(
			"ImportJsonConfirm",
			"현재 JSON 텍스트로 FlowDataAsset의 Root Events와 Subgraphs를 덮어씁니다.\n계속할까요?"));
	if (answer != EAppReturnType::Yes)
	{
		SetStatusMessage(TEXT("Import canceled."));
		return FReply::Handled();
	}

	FScopedTransaction transaction(LOCTEXT("ImportJsonTransaction", "Import Vdjm Record Event Flow JSON"));
	EditingAsset->Modify();

	FString errorReason;
	if (not EditingAsset->ImportFlowFromJsonString(GetCurrentJsonText(), errorReason))
	{
		transaction.Cancel();
		SetStatusMessage(FString::Printf(TEXT("Import failed. %s"), *errorReason));
		return FReply::Handled();
	}

	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(EditingAsset, true);
	}
	RefreshAllViews();
	SetStatusMessage(TEXT("Import succeeded."));
	return FReply::Handled();
}

FReply FVdjmRecordEventFlowDataAssetEditorToolkit::OnRefreshSummaryClicked()
{
	RefreshSummaryText();
	SetStatusMessage(TEXT("Summary refreshed."));
	return FReply::Handled();
}

FReply FVdjmRecordEventFlowDataAssetEditorToolkit::OnResetGuideClicked()
{
	if (EditingAsset != nullptr)
	{
		EditingAsset->ResetAuthoringGuide();
		if (DetailsView.IsValid())
		{
			DetailsView->SetObject(EditingAsset, true);
		}
		RefreshGuideText();
		SetStatusMessage(TEXT("Guide reset."));
	}
	return FReply::Handled();
}

FText FVdjmRecordEventFlowDataAssetEditorToolkit::GetStatusText() const
{
	return FText::FromString(LastStatusMessage);
}

FString FVdjmRecordEventFlowDataAssetEditorToolkit::GetCurrentJsonText() const
{
	return JsonTextBox.IsValid()
		? JsonTextBox->GetText().ToString()
		: FString();
}

#undef LOCTEXT_NAMESPACE
