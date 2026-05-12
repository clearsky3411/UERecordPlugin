#include "VdjmWidgetMediaCardWidget.h"

#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
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

	bool IsExternalMediaSource(const FString& source)
	{
		return source.StartsWith(TEXT("content://"), ESearchCase::IgnoreCase) ||
			source.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) ||
			source.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
	}

	bool TryResolveLocalFileSource(
		const FString& source,
		EVdjmWidgetMediaSourceKind sourceKind,
		const TCHAR* sourceName,
		FString& outSource,
		EVdjmWidgetMediaSourceKind& outSourceKind,
		FString& outErrorReason)
	{
		if (source.IsEmpty())
		{
			return false;
		}

		if (IsExternalMediaSource(source) || FPaths::FileExists(source))
		{
			outSource = source;
			outSourceKind = sourceKind;
			outErrorReason.Reset();
			return true;
		}

		outErrorReason = FString::Printf(TEXT("%s does not exist. Source=%s"), sourceName, *source);
		return false;
	}

	bool TryResolveExternalSource(
		const FString& source,
		EVdjmWidgetMediaSourceKind sourceKind,
		FString& outSource,
		EVdjmWidgetMediaSourceKind& outSourceKind,
		FString& outErrorReason)
	{
		if (source.IsEmpty())
		{
			return false;
		}

		outSource = source;
		outSourceKind = sourceKind;
		outErrorReason.Reset();
		return true;
	}

	UImage* FindFirstImageInWidget(UWidget* widget)
	{
		if (widget == nullptr)
		{
			return nullptr;
		}

		if (UImage* image = Cast<UImage>(widget))
		{
			return image;
		}

		if (UPanelWidget* panelWidget = Cast<UPanelWidget>(widget))
		{
			for (int32 childIndex = 0; childIndex < panelWidget->GetChildrenCount(); ++childIndex)
			{
				if (UImage* childImage = FindFirstImageInWidget(panelWidget->GetChildAt(childIndex)))
				{
					return childImage;
				}
			}
		}

		return nullptr;
	}
}

void UVdjmWidgetMediaCardWidget::NativeDestruct()
{
	ReleaseMediaResources();
	Super::NativeDestruct();
}

void UVdjmWidgetMediaCardWidget::EnsureMediaObjects()
{
	if (mMediaPlayer == nullptr)
	{
		mMediaPlayer = NewObject<UMediaPlayer>(this);
	}

	if (mMediaTexture == nullptr)
	{
		mMediaTexture = NewObject<UMediaTexture>(this);
		mMediaTexture->AutoClear = true;
		mMediaTexture->ClearColor = FLinearColor::Black;
	}

	if (mMediaTexture != nullptr && mMediaPlayer != nullptr)
	{
		mMediaTexture->SetMediaPlayer(mMediaPlayer);
		mMediaTexture->UpdateResource();
	}
}

UImage* UVdjmWidgetMediaCardWidget::ResolveActivePreviewImage() const
{
	if (Image_ActivePreview != nullptr)
	{
		return Image_ActivePreview;
	}

	return FindFirstImageInWidget(ActiveLayer);
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
	BP_ShowActiveCard();
	StartActivePreviewLoop();
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
	if (not bAutoManagePreviewMedia)
	{
		return;
	}

	FString errorReason;
	if (not StartManagedActivePreview(errorReason) && bDebugTraceState)
	{
		UE_LOG(
			LogVdjmWidgets,
			Warning,
			TEXT("VdjmCard AutoPreview Failed Widget=%s Reason=%s"),
			*GetNameSafe(this),
			*errorReason);
	}
}

void UVdjmWidgetMediaCardWidget::StopPreview(bool bReleaseMediaResources)
{
	BP_StopPreview(bReleaseMediaResources);
	if (mPreviewPlayer != nullptr)
	{
		mPreviewPlayer->StopPreview(bReleaseMediaResources);
	}
	else if (mMediaPlayer != nullptr)
	{
		mMediaPlayer->Pause();
		if (bReleaseMediaResources)
		{
			mMediaPlayer->Close();
		}
	}

	if (bReleaseMediaResources)
	{
		ReleaseMediaResources();
	}
}

void UVdjmWidgetMediaCardWidget::ReleaseMediaResources()
{
	if (mPreviewPlayer != nullptr)
	{
		mPreviewPlayer->StopPreview(true);
		mPreviewPlayer = nullptr;
	}

	if (mMediaPlayer != nullptr)
	{
		mMediaPlayer->Close();
	}

	if (mMediaTexture != nullptr)
	{
		mMediaTexture->SetMediaPlayer(nullptr);
		mMediaTexture->UpdateResource();
		mMediaTexture = nullptr;
	}

	mMediaPlayer = nullptr;
}

bool UVdjmWidgetMediaCardWidget::PrepareActivePreviewVisuals()
{
	EnsureMediaObjects();
	return ApplyActiveMediaTextureBrush();
}

bool UVdjmWidgetMediaCardWidget::StartManagedActivePreview(FString& outErrorReason)
{
	outErrorReason.Reset();
	if (not HasValidCardSource())
	{
		outErrorReason = TEXT("Card source is empty or invalid.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	if (not PrepareActivePreviewVisuals())
	{
		outErrorReason = TEXT("Failed to prepare active preview visuals.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	if (mPreviewPlayer == nullptr)
	{
		mPreviewPlayer = UVdjmRecordMediaPreviewPlayer::CreateMediaPreviewPlayer(this, mMediaPlayer);
	}

	if (mPreviewPlayer == nullptr)
	{
		outErrorReason = TEXT("Preview player is invalid.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	FString previewSource;
	EVdjmWidgetMediaSourceKind sourceKind = EVdjmWidgetMediaSourceKind::ENone;
	FString sourceErrorReason;
	GetActivePreviewSource(previewSource, sourceKind, sourceErrorReason);

	const bool bResult = mPreviewPlayer->StartPreviewFromRegistryEntry(
		mMediaPlayer,
		mCardSource.RegistryEntry,
		outErrorReason);
	if (not bResult)
	{
		mLastErrorReason = outErrorReason;
	}
	else
	{
		mLastErrorReason.Reset();
	}

	if (bDebugTraceState)
	{
		if (bResult)
		{
			UE_LOG(
				LogVdjmWidgets,
				Log,
				TEXT("VdjmCard AutoPreview Start Widget=%s Result=true SourceKind=%s Source=%s Reason=None"),
				*GetNameSafe(this),
				*GetEnumValueString(sourceKind),
				previewSource.IsEmpty() ? TEXT("None") : *previewSource);
		}
		else
		{
			UE_LOG(
				LogVdjmWidgets,
				Warning,
				TEXT("VdjmCard AutoPreview Start Widget=%s Result=false SourceKind=%s Source=%s Reason=%s"),
				*GetNameSafe(this),
				*GetEnumValueString(sourceKind),
				previewSource.IsEmpty() ? TEXT("None") : *previewSource,
				outErrorReason.IsEmpty() ? TEXT("None") : *outErrorReason);
		}
	}
	return bResult;
}

bool UVdjmWidgetMediaCardWidget::ApplyActiveMediaTextureBrush()
{
	if (mMediaTexture == nullptr)
	{
		return false;
	}

	UImage* activePreviewImage = ResolveActivePreviewImage();
	if (activePreviewImage == nullptr)
	{
		if (bDebugTraceState)
		{
			UE_LOG(
				LogVdjmWidgets,
				Warning,
				TEXT("VdjmCard AutoPreview BrushFailed Widget=%s Reason=ActivePreviewImageMissing ActiveLayer=%s"),
				*GetNameSafe(this),
				*GetNameSafe(ActiveLayer));
		}
		return false;
	}

	FSlateBrush mediaBrush;
	mediaBrush.SetResourceObject(mMediaTexture);
	mediaBrush.ImageSize = FVector2D(512.0f, 512.0f);
	activePreviewImage->SetBrush(mediaBrush);

	if (bDebugTraceState)
	{
		UE_LOG(
			LogVdjmWidgets,
			Log,
			TEXT("VdjmCard AutoPreview BrushApplied Widget=%s Image=%s Texture=%s Player=%s"),
			*GetNameSafe(this),
			*GetNameSafe(activePreviewImage),
			*GetNameSafe(mMediaTexture),
			*GetNameSafe(mMediaPlayer));
	}
	return true;
}

bool UVdjmWidgetMediaCardWidget::HasValidCardSource() const
{
	return mCardSource.bValid &&
		(not mCardSource.RegistryEntry.RecordId.IsEmpty() ||
			not mCardSource.RegistryEntry.MetadataFilePath.IsEmpty() ||
			not mCardSource.RegistryEntry.OutputFilePath.IsEmpty() ||
			not mCardSource.RegistryEntry.PlaybackLocator.IsEmpty() ||
			not mCardSource.RegistryEntry.PublishedContentUri.IsEmpty());
}

bool UVdjmWidgetMediaCardWidget::ValidateCardSource(FString& outErrorReason) const
{
	outErrorReason.Reset();
	if (not HasValidCardSource())
	{
		outErrorReason = TEXT("Card source is empty or invalid.");
		return false;
	}

	FString source;
	EVdjmWidgetMediaSourceKind sourceKind = EVdjmWidgetMediaSourceKind::ENone;
	return GetActivePreviewSource(source, sourceKind, outErrorReason);
}

bool UVdjmWidgetMediaCardWidget::GetVisiblePreviewSource(
	FString& outSource,
	EVdjmWidgetMediaSourceKind& outSourceKind,
	FString& outErrorReason) const
{
	outSource.Reset();
	outSourceKind = EVdjmWidgetMediaSourceKind::ENone;
	outErrorReason.Reset();
	if (not HasValidCardSource())
	{
		outErrorReason = TEXT("Card source is empty or invalid.");
		return false;
	}

	const FVdjmRecordMediaRegistryEntry& registryEntry = mCardSource.RegistryEntry;
	if (TryResolveLocalFileSource(
		registryEntry.ThumbnailFilePath,
		EVdjmWidgetMediaSourceKind::EThumbnailFile,
		TEXT("ThumbnailFilePath"),
		outSource,
		outSourceKind,
		outErrorReason))
	{
		return true;
	}

	return GetActivePreviewSource(outSource, outSourceKind, outErrorReason);
}

bool UVdjmWidgetMediaCardWidget::GetActivePreviewSource(
	FString& outSource,
	EVdjmWidgetMediaSourceKind& outSourceKind,
	FString& outErrorReason) const
{
	outSource.Reset();
	outSourceKind = EVdjmWidgetMediaSourceKind::ENone;
	outErrorReason.Reset();
	if (not HasValidCardSource())
	{
		outErrorReason = TEXT("Card source is empty or invalid.");
		return false;
	}

	const FVdjmRecordMediaRegistryEntry& registryEntry = mCardSource.RegistryEntry;
	if (TryResolveLocalFileSource(
		registryEntry.PreviewClipFilePath,
		EVdjmWidgetMediaSourceKind::EPreviewClipFile,
		TEXT("PreviewClipFilePath"),
		outSource,
		outSourceKind,
		outErrorReason))
	{
		return true;
	}

	if (TryResolveLocalFileSource(
		registryEntry.OutputFilePath,
		EVdjmWidgetMediaSourceKind::EOutputFile,
		TEXT("OutputFilePath"),
		outSource,
		outSourceKind,
		outErrorReason))
	{
		return true;
	}

	if (registryEntry.PlaybackLocatorType.Equals(TEXT("local_path"), ESearchCase::IgnoreCase))
	{
		if (TryResolveLocalFileSource(
			registryEntry.PlaybackLocator,
			EVdjmWidgetMediaSourceKind::EPlaybackLocator,
			TEXT("PlaybackLocator"),
			outSource,
			outSourceKind,
			outErrorReason))
		{
			return true;
		}
	}
	else if (TryResolveExternalSource(
		registryEntry.PlaybackLocator,
		EVdjmWidgetMediaSourceKind::EPlaybackLocator,
		outSource,
		outSourceKind,
		outErrorReason))
	{
		return true;
	}

	if (TryResolveExternalSource(
		registryEntry.PublishedContentUri,
		EVdjmWidgetMediaSourceKind::EPublishedContentUri,
		outSource,
		outSourceKind,
		outErrorReason))
	{
		return true;
	}

	if (outErrorReason.IsEmpty())
	{
		outErrorReason = TEXT("No playable preview source was found.");
	}
	return false;
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
