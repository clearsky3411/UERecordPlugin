#include "VdjmWidgetMediaCardWidget.h"

#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "VdjmWidgets.h"

namespace
{
	void SetOptionalWidgetVisibility(UWidget* widget, ESlateVisibility visibility)
	{
		if (widget != nullptr)
		{
			widget->SetVisibility(visibility);
		}
	}

	const TCHAR* GetVisibilityString(ESlateVisibility visibility)
	{
		switch (visibility)
		{
		case ESlateVisibility::Visible:
			return TEXT("Visible");
		case ESlateVisibility::Collapsed:
			return TEXT("Collapsed");
		case ESlateVisibility::Hidden:
			return TEXT("Hidden");
		case ESlateVisibility::HitTestInvisible:
			return TEXT("HitTestInvisible");
		case ESlateVisibility::SelfHitTestInvisible:
			return TEXT("SelfHitTestInvisible");
		default:
			return TEXT("Unknown");
		}
	}

	template <typename EnumType>
	FString GetEnumValueString(EnumType value)
	{
		const UEnum* enumObject = StaticEnum<EnumType>();
		return enumObject != nullptr
			? enumObject->GetValueAsString(value)
			: FString::FromInt(static_cast<int32>(value));
	}

	int32 GetOptionalPanelChildCount(const UWidget* widget)
	{
		const UPanelWidget* panelWidget = Cast<UPanelWidget>(widget);
		return panelWidget != nullptr ? panelWidget->GetChildrenCount() : INDEX_NONE;
	}
}

bool UVdjmWidgetMediaCardWidget::SetCardSource(const FVdjmWidgetMediaCardSource& cardSource)
{
	mCardSource = cardSource;
	BP_OnCardSourceChanged(mCardSource);
	return mCardSource.bValid;
}

void UVdjmWidgetMediaCardWidget::ClearCardSource()
{
	mCardSource = FVdjmWidgetMediaCardSource();
	BP_OnCardSourceChanged(mCardSource);
}

bool UVdjmWidgetMediaCardWidget::SetCardState(EVdjmWidgetMediaCardState newState)
{
	const EVdjmWidgetMediaCardState previousState = mCardState;
	mCardState = newState;

	switch (newState)
	{
	case EVdjmWidgetMediaCardState::EEmpty:
		EnterEmpty();
		break;
	case EVdjmWidgetMediaCardState::EWaiting:
		EnterWaiting();
		break;
	case EVdjmWidgetMediaCardState::EVisible:
		EnterVisible();
		break;
	case EVdjmWidgetMediaCardState::EActive:
		EnterActive();
		break;
	case EVdjmWidgetMediaCardState::EHidden:
		EnterHidden();
		break;
	case EVdjmWidgetMediaCardState::EError:
		EnterError(mLastErrorReason);
		break;
	default:
		return false;
	}

	BP_OnCardStateChanged(previousState, newState);
	OnCardStateChanged.Broadcast(previousState, newState);
	if (bDebugTraceState)
	{
		DumpDebugCardState(TEXT("SetCardState"));
	}
	return true;
}

void UVdjmWidgetMediaCardWidget::EnterEmpty()
{
	StopPreview(true);
	SetVisibility(ESlateVisibility::Visible);
	SetOptionalWidgetVisibility(EmptyLayer, ESlateVisibility::Visible);
	SetOptionalWidgetVisibility(WaitingLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(VisibleLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ActiveLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ErrorLayer, ESlateVisibility::Collapsed);
	BP_ShowEmptyCard();
}

void UVdjmWidgetMediaCardWidget::EnterWaiting()
{
	StopPreview(false);
	SetVisibility(ESlateVisibility::Visible);
	SetOptionalWidgetVisibility(EmptyLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(WaitingLayer, ESlateVisibility::Visible);
	SetOptionalWidgetVisibility(VisibleLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ActiveLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ErrorLayer, ESlateVisibility::Collapsed);
	BP_ShowWaitingCard();
}

void UVdjmWidgetMediaCardWidget::EnterVisible()
{
	StopPreview(false);
	SetVisibility(ESlateVisibility::Visible);
	SetOptionalWidgetVisibility(EmptyLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(WaitingLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(VisibleLayer, ESlateVisibility::Visible);
	SetOptionalWidgetVisibility(ActiveLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ErrorLayer, ESlateVisibility::Collapsed);
	BP_ShowVisibleCard();
}

void UVdjmWidgetMediaCardWidget::EnterActive()
{
	SetVisibility(ESlateVisibility::Visible);
	SetOptionalWidgetVisibility(EmptyLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(WaitingLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(VisibleLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ActiveLayer, ESlateVisibility::Visible);
	SetOptionalWidgetVisibility(ErrorLayer, ESlateVisibility::Collapsed);
	StartActivePreviewLoop();
	BP_ShowActiveCard();
}

void UVdjmWidgetMediaCardWidget::EnterHidden()
{
	StopPreview(true);
	SetVisibility(ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(EmptyLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(WaitingLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(VisibleLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ActiveLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ErrorLayer, ESlateVisibility::Collapsed);
	BP_ShowHiddenCard();
}

void UVdjmWidgetMediaCardWidget::EnterError(const FString& errorReason)
{
	mLastErrorReason = errorReason;
	StopPreview(true);
	SetVisibility(ESlateVisibility::Visible);
	SetOptionalWidgetVisibility(EmptyLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(WaitingLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(VisibleLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ActiveLayer, ESlateVisibility::Collapsed);
	SetOptionalWidgetVisibility(ErrorLayer, ESlateVisibility::Visible);
	BP_ShowErrorCard(mLastErrorReason);
}

void UVdjmWidgetMediaCardWidget::StartActivePreviewLoop()
{
	BP_StartActivePreviewLoop();
}

void UVdjmWidgetMediaCardWidget::StopPreview(bool bReleaseMediaResources)
{
	BP_StopPreview(bReleaseMediaResources);
	if (bReleaseMediaResources)
	{
		ReleaseMediaResources();
	}
}

void UVdjmWidgetMediaCardWidget::ReleaseMediaResources()
{
}

void UVdjmWidgetMediaCardWidget::DumpDebugCardState(const FString& reason) const
{
	UE_LOG(
		LogVdjmWidgets,
		Log,
		TEXT("VdjmCard State Reason=%s Widget=%s State=%s Visibility=%s SourceIndex=%d Valid=%s RecordId=%s"),
		*reason,
		*GetNameSafe(this),
		*GetEnumValueString(mCardState),
		GetVisibilityString(GetVisibility()),
		mCardSource.SourceRegistryIndex,
		mCardSource.bValid ? TEXT("true") : TEXT("false"),
		*mCardSource.RegistryEntry.RecordId);

	UE_LOG(
		LogVdjmWidgets,
		Log,
		TEXT("VdjmCard Layers Widget=%s Empty=%s/%s/%d Waiting=%s/%s/%d Visible=%s/%s/%d Active=%s/%s/%d Error=%s/%s/%d"),
		*GetNameSafe(this),
		EmptyLayer != nullptr ? TEXT("Valid") : TEXT("None"),
		EmptyLayer != nullptr ? GetVisibilityString(EmptyLayer->GetVisibility()) : TEXT("None"),
		GetOptionalPanelChildCount(EmptyLayer),
		WaitingLayer != nullptr ? TEXT("Valid") : TEXT("None"),
		WaitingLayer != nullptr ? GetVisibilityString(WaitingLayer->GetVisibility()) : TEXT("None"),
		GetOptionalPanelChildCount(WaitingLayer),
		VisibleLayer != nullptr ? TEXT("Valid") : TEXT("None"),
		VisibleLayer != nullptr ? GetVisibilityString(VisibleLayer->GetVisibility()) : TEXT("None"),
		GetOptionalPanelChildCount(VisibleLayer),
		ActiveLayer != nullptr ? TEXT("Valid") : TEXT("None"),
		ActiveLayer != nullptr ? GetVisibilityString(ActiveLayer->GetVisibility()) : TEXT("None"),
		GetOptionalPanelChildCount(ActiveLayer),
		ErrorLayer != nullptr ? TEXT("Valid") : TEXT("None"),
		ErrorLayer != nullptr ? GetVisibilityString(ErrorLayer->GetVisibility()) : TEXT("None"),
		GetOptionalPanelChildCount(ErrorLayer));
}
