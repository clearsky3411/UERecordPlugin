#include "VdjmWidgetMediaCardWidget.h"

#include "Components/Widget.h"

namespace
{
	void SetOptionalWidgetVisibility(UWidget* widget, ESlateVisibility visibility)
	{
		if (widget != nullptr)
		{
			widget->SetVisibility(visibility);
		}
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
