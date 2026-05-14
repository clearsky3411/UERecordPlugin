#include "VdjmRecordMediaPreview.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmRecorderWorldContextSubsystem.h"

namespace
{
	FString GetPreviewSourceFromRegistryEntry(const FVdjmRecordMediaRegistryEntry& registryEntry)
	{
		if (not registryEntry.PreviewClipFilePath.IsEmpty())
		{
			return registryEntry.PreviewClipFilePath;
		}

		if (not registryEntry.OutputFilePath.IsEmpty())
		{
			return registryEntry.OutputFilePath;
		}

		if (not registryEntry.PlaybackLocator.IsEmpty())
		{
			return registryEntry.PlaybackLocator;
		}

		return registryEntry.PublishedContentUri;
	}

	FString GetPreviewSourceFromSlot(const UVdjmRecordMediaPreviewSlot* previewSlot)
	{
		if (previewSlot == nullptr)
		{
			return FString();
		}

		if (const UVdjmRecordMediaManifest* manifest = previewSlot->GetManifest())
		{
			if (not manifest->GetPreviewClipFilePath().IsEmpty())
			{
				return manifest->GetPreviewClipFilePath();
			}

			if (not manifest->GetOutputFilePath().IsEmpty())
			{
				return manifest->GetOutputFilePath();
			}

			if (not manifest->GetPlaybackLocator().IsEmpty())
			{
				return manifest->GetPlaybackLocator();
			}

			if (not manifest->GetPublishedContentUri().IsEmpty())
			{
				return manifest->GetPublishedContentUri();
			}
		}

		return GetPreviewSourceFromRegistryEntry(previewSlot->RegistryEntry);
	}

	template <typename EnumType>
	FString GetEnumValueString(EnumType value)
	{
		const UEnum* enumObject = StaticEnum<EnumType>();
		return enumObject != nullptr
			? enumObject->GetValueAsString(value)
			: FString::FromInt(static_cast<int32>(value));
	}

	template <typename EnumType>
	EnumType ParseEnumValueString(const FString& valueString, EnumType fallbackValue)
	{
		const UEnum* enumObject = StaticEnum<EnumType>();
		if (enumObject == nullptr || valueString.TrimStartAndEnd().IsEmpty())
		{
			return fallbackValue;
		}

		int64 enumValue = enumObject->GetValueByNameString(valueString);
		if (enumValue == INDEX_NONE)
		{
			FString shortValueString = valueString;
			const int32 scopeIndex = valueString.Find(TEXT("::"));
			if (scopeIndex != INDEX_NONE)
			{
				shortValueString = valueString.RightChop(scopeIndex + 2);
			}
			enumValue = enumObject->GetValueByNameString(shortValueString);
		}

		return enumValue == INDEX_NONE
			? fallbackValue
			: static_cast<EnumType>(enumValue);
	}
}

void UVdjmRecordMediaPreviewSlot::ClearSlot()
{
	SourceRegistryIndex = INDEX_NONE;
	RegistryEntry = FVdjmRecordMediaRegistryEntry();
	Manifest = nullptr;
	SlotState = EVdjmRecordMediaPreviewSlotState::EEmpty;
}

void UVdjmRecordMediaPreviewSlot::SetRegistryEntry(
	const FVdjmRecordMediaRegistryEntry& registryEntry,
	int32 sourceRegistryIndex)
{
	RegistryEntry = registryEntry;
	SourceRegistryIndex = sourceRegistryIndex;
	Manifest = nullptr;
	SlotState = registryEntry.RecordId.IsEmpty() && registryEntry.MetadataFilePath.IsEmpty()
		? EVdjmRecordMediaPreviewSlotState::EEmpty
		: EVdjmRecordMediaPreviewSlotState::EStatic;
}

void UVdjmRecordMediaPreviewSlot::SetManifest(UVdjmRecordMediaManifest* manifest)
{
	Manifest = manifest;
	if (IsValid(manifest))
	{
		SlotState = EVdjmRecordMediaPreviewSlotState::EPrepared;
	}
}

void UVdjmRecordMediaPreviewSlot::SetSlotState(EVdjmRecordMediaPreviewSlotState newState)
{
	SlotState = newState;
}

FString UVdjmRecordMediaPreviewSlot::GetRecordId() const
{
	if (IsValid(Manifest))
	{
		return Manifest->GetRecordId();
	}

	return RegistryEntry.RecordId;
}

FString UVdjmRecordMediaPreviewSlot::GetOutputFilePath() const
{
	if (IsValid(Manifest))
	{
		return Manifest->GetOutputFilePath();
	}

	return RegistryEntry.OutputFilePath;
}

FString UVdjmRecordMediaPreviewSlot::GetMetadataFilePath() const
{
	if (IsValid(Manifest))
	{
		return Manifest->GetMetadataFilePath();
	}

	return RegistryEntry.MetadataFilePath;
}

AVdjmRecordMediaPreviewManagerActor::AVdjmRecordMediaPreviewManagerActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorHiddenInGame(true);
}

AVdjmRecordMediaPreviewManagerActor* AVdjmRecordMediaPreviewManagerActor::FindMediaPreviewManagerActor(
	UObject* worldContextObject)
{
	if (worldContextObject == nullptr)
	{
		return nullptr;
	}

	UWorld* world = worldContextObject->GetWorld();
	if (world == nullptr)
	{
		return nullptr;
	}

	if (const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem =
		UVdjmRecorderWorldContextSubsystem::Get(worldContextObject))
	{
		UObject* contextObject = worldContextSubsystem->FindContextObject(
			UVdjmRecorderWorldContextSubsystem::GetMediaPreviewManagerContextKey());
		if (AVdjmRecordMediaPreviewManagerActor* existingActor = Cast<AVdjmRecordMediaPreviewManagerActor>(contextObject))
		{
			return existingActor;
		}
	}

	for (TActorIterator<AVdjmRecordMediaPreviewManagerActor> actorIt(world); actorIt; ++actorIt)
	{
		return *actorIt;
	}

	return nullptr;
}

AVdjmRecordMediaPreviewManagerActor* AVdjmRecordMediaPreviewManagerActor::FindOrSpawnMediaPreviewManagerActor(
	UObject* worldContextObject)
{
	if (worldContextObject == nullptr)
	{
		return nullptr;
	}

	if (AVdjmRecordMediaPreviewManagerActor* existingActor = FindMediaPreviewManagerActor(worldContextObject))
	{
		existingActor->RegisterSelfAsPrimitive();
		return existingActor;
	}

	UWorld* world = worldContextObject->GetWorld();
	if (world == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters spawnParameters;
	spawnParameters.Name = MakeUniqueObjectName(world, StaticClass(), TEXT("VdjmRecordMediaPreviewManagerActor"));
	spawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVdjmRecordMediaPreviewManagerActor* spawnedActor =
		world->SpawnActor<AVdjmRecordMediaPreviewManagerActor>(StaticClass(), spawnParameters);
	if (spawnedActor != nullptr)
	{
		spawnedActor->RegisterSelfAsPrimitive();
	}

	return spawnedActor;
}

bool AVdjmRecordMediaPreviewManagerActor::RefreshPreviewStoreFromDisk(FString& outErrorReason)
{
	outErrorReason.Reset();
	OnPreviewStoreRefreshStarted.Broadcast();

	mMetadataStore = UVdjmRecordMetadataStore::FindOrCreateMetadataStore(this);
	if (not IsValid(mMetadataStore))
	{
		outErrorReason = TEXT("Metadata store is invalid.");
		OnPreviewStoreRefreshFinished.Broadcast(false, outErrorReason);
		return false;
	}

	if (not mMetadataStore->RefreshRegistryFromDisk(outErrorReason))
	{
		OnPreviewStoreRefreshFinished.Broadcast(false, outErrorReason);
		return false;
	}

	mRegistryEntries = mMetadataStore->GetMediaRegistryEntries();
	OnPreviewRegistryChanged.Broadcast();
	OnPreviewStoreRefreshFinished.Broadcast(true, FString());

	if (mCenterSourceIndex == INDEX_NONE && mRegistryEntries.Num() > 0)
	{
		mCenterSourceIndex = 0;
	}

	ApplyCarouselWindow(mCenterSourceIndex, SlotCount, ActiveSlotIndex);
	return true;
}

bool AVdjmRecordMediaPreviewManagerActor::GetMediaPreviewManagerInitStatus(
	UObject* worldContextObject,
	FVdjmRecordMediaPreviewInitStatus& outStatus)
{
	outStatus = FVdjmRecordMediaPreviewInitStatus();
	AVdjmRecordMediaPreviewManagerActor* previewManager = FindMediaPreviewManagerActor(worldContextObject);
	if (previewManager == nullptr)
	{
		return false;
	}

	outStatus = previewManager->GetPreviewManagerInitStatus();
	return true;
}

bool AVdjmRecordMediaPreviewManagerActor::InitializePreviewManagerFromDisk(
	bool bForceRefresh,
	FString& outErrorReason)
{
	FVdjmRecordMediaPreviewInitRequest initRequest;
	initRequest.bForceRefresh = bForceRefresh;
	initRequest.bApplyCarouselWindowAfterInit = true;
	initRequest.bSucceedWithEmptyRegistry = true;
	initRequest.SlotCount = SlotCount;
	initRequest.ActiveSlotIndex = ActiveSlotIndex;
	initRequest.InitialCenterSourceIndex = mCenterSourceIndex;
	initRequest.bAutoStartCenterPreview = bAutoStartCenterPreview;

	if (not StartPreviewManagerInit(initRequest, outErrorReason))
	{
		return false;
	}

	for (int32 guardIndex = 0; guardIndex < 1024; ++guardIndex)
	{
		const EVdjmRecordMediaPreviewInitRunResult runResult = AdvancePreviewManagerInitStep(outErrorReason);
		if (runResult == EVdjmRecordMediaPreviewInitRunResult::ESucceeded)
		{
			return true;
		}

		if (runResult == EVdjmRecordMediaPreviewInitRunResult::EFailed)
		{
			return false;
		}
	}

	outErrorReason = TEXT("Preview manager init exceeded the synchronous guard limit.");
	FailPreviewManagerInit(outErrorReason);
	return false;
}

bool AVdjmRecordMediaPreviewManagerActor::StartPreviewManagerInit(
	const FVdjmRecordMediaPreviewInitRequest& initRequest,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	if (mbPreviewManagerInitRunning)
	{
		outErrorReason = TEXT("Preview manager init is already running.");
		return false;
	}

	if (mbPreviewManagerInitialized && not initRequest.bForceRefresh)
	{
		mLastPreviewManagerErrorReason.Reset();
		SetPreviewManagerInitStep(
			EVdjmRecordMediaPreviewInitStep::EComplete,
			TEXT("Preview manager is already initialized."));
		OnPreviewManagerInitFinished.Broadcast(true, FString());
		return true;
	}

	mPreviewInitRequest = initRequest;
	mPreviewInitRequest.SlotCount = FMath::Clamp(mPreviewInitRequest.SlotCount, 1, 32);
	mPreviewInitRequest.ActiveSlotIndex = FMath::Clamp(
		mPreviewInitRequest.ActiveSlotIndex,
		0,
		mPreviewInitRequest.SlotCount - 1);
	mPreviewInitRequest.MaxManifestFilesPerStep = FMath::Max(1, mPreviewInitRequest.MaxManifestFilesPerStep);
	mPreviewInitRequest.MaxRegistryEntryStateChecksPerStep =
		FMath::Max(1, mPreviewInitRequest.MaxRegistryEntryStateChecksPerStep);
	mPreviewInitRequest.MaxRegistryEntriesPerStep = FMath::Max(1, mPreviewInitRequest.MaxRegistryEntriesPerStep);

	SlotCount = mPreviewInitRequest.SlotCount;
	ActiveSlotIndex = mPreviewInitRequest.ActiveSlotIndex;
	bAutoStartCenterPreview = mPreviewInitRequest.bAutoStartCenterPreview;
	mPreviewInitRegistryEntries.Reset();
	mRegistryCopyCursor = 0;
	mPreviewInitPendingWorkCount = 0;
	mPreviewInitProcessedWorkCount = 0;
	mLastPreviewManagerErrorReason.Reset();
	mbPreviewManagerInitialized = false;
	mbPreviewManagerInitRunning = true;
	mbPreviewInitRegistryScanStarted = false;

	OnPreviewManagerInitStarted.Broadcast();
	SetPreviewManagerInitStep(
		EVdjmRecordMediaPreviewInitStep::EInitializeStart,
		TEXT("Preview manager init started."));
	return true;
}

EVdjmRecordMediaPreviewInitRunResult AVdjmRecordMediaPreviewManagerActor::AdvancePreviewManagerInitStep(
	FString& outErrorReason)
{
	outErrorReason.Reset();

	if (not mbPreviewManagerInitRunning)
	{
		return mbPreviewManagerInitialized
			? EVdjmRecordMediaPreviewInitRunResult::ESucceeded
			: EVdjmRecordMediaPreviewInitRunResult::EFailed;
	}

	switch (mCurrentPreviewManagerInitStep)
	{
	case EVdjmRecordMediaPreviewInitStep::EInitializeStart:
		SetPreviewManagerInitStep(
			EVdjmRecordMediaPreviewInitStep::EEnsureMetadataStore,
			TEXT("Ensuring metadata store."));
		return EVdjmRecordMediaPreviewInitRunResult::ERunning;

	case EVdjmRecordMediaPreviewInitStep::EEnsureMetadataStore:
		mMetadataStore = UVdjmRecordMetadataStore::FindOrCreateMetadataStore(this);
		if (not IsValid(mMetadataStore))
		{
			outErrorReason = TEXT("Metadata store is invalid.");
			return FailPreviewManagerInit(outErrorReason);
		}

		SetPreviewManagerInitStep(
			EVdjmRecordMediaPreviewInitStep::ERefreshRegistry,
			TEXT("Refreshing media registry from disk."));
		return EVdjmRecordMediaPreviewInitRunResult::ERunning;

	case EVdjmRecordMediaPreviewInitStep::ERefreshRegistry:
		if (not IsValid(mMetadataStore))
		{
			outErrorReason = TEXT("Metadata store is invalid.");
			return FailPreviewManagerInit(outErrorReason);
		}

		if (not mbPreviewInitRegistryScanStarted)
		{
			FVdjmRecordMetadataRegistryScanRequest scanRequest;
			scanRequest.MaxManifestFilesPerStep = mPreviewInitRequest.MaxManifestFilesPerStep;
			scanRequest.MaxRegistryEntriesPerStep = mPreviewInitRequest.MaxRegistryEntryStateChecksPerStep;
			scanRequest.bSaveRegistryOnComplete = true;
			if (not mMetadataStore->StartRegistryScanFromDisk(scanRequest, outErrorReason))
			{
				return FailPreviewManagerInit(outErrorReason);
			}

			mbPreviewInitRegistryScanStarted = true;
			mPreviewInitPendingWorkCount = mMetadataStore->GetRegistryScanPendingCount();
			mPreviewInitProcessedWorkCount = mMetadataStore->GetRegistryScanProcessedCount();
			return EVdjmRecordMediaPreviewInitRunResult::ERunning;
		}

		{
			const EVdjmRecordMetadataRegistryScanRunResult registryScanRunResult =
				mMetadataStore->AdvanceRegistryScanFromDisk(outErrorReason);
			mPreviewInitPendingWorkCount = mMetadataStore->GetRegistryScanPendingCount();
			mPreviewInitProcessedWorkCount = mMetadataStore->GetRegistryScanProcessedCount();
			if (registryScanRunResult == EVdjmRecordMetadataRegistryScanRunResult::ERunning)
			{
				return EVdjmRecordMediaPreviewInitRunResult::ERunning;
			}

			if (registryScanRunResult == EVdjmRecordMetadataRegistryScanRunResult::EFailed)
			{
				return FailPreviewManagerInit(outErrorReason);
			}
		}

		mPreviewInitRegistryEntries = mMetadataStore->GetMediaRegistryEntries();
		mRegistryCopyCursor = 0;
		mPreviewInitPendingWorkCount = mPreviewInitRegistryEntries.Num();
		mPreviewInitProcessedWorkCount = 0;
		SetPreviewManagerInitStep(
			EVdjmRecordMediaPreviewInitStep::ECopyRegistryEntries,
			TEXT("Copying registry entries."));
		return EVdjmRecordMediaPreviewInitRunResult::ERunning;

	case EVdjmRecordMediaPreviewInitStep::ECopyRegistryEntries:
		if (mRegistryCopyCursor == 0)
		{
			mRegistryEntries.Reset();
		}

		{
			const int32 copyEndIndex = FMath::Min(
				mPreviewInitRegistryEntries.Num(),
				mRegistryCopyCursor + mPreviewInitRequest.MaxRegistryEntriesPerStep);
			for (; mRegistryCopyCursor < copyEndIndex; ++mRegistryCopyCursor)
			{
				mRegistryEntries.Add(mPreviewInitRegistryEntries[mRegistryCopyCursor]);
			}
		}

		mPreviewInitProcessedWorkCount = mRegistryCopyCursor;
		mPreviewInitPendingWorkCount = FMath::Max(0, mPreviewInitRegistryEntries.Num() - mRegistryCopyCursor);
		if (mRegistryCopyCursor < mPreviewInitRegistryEntries.Num())
		{
			return EVdjmRecordMediaPreviewInitRunResult::ERunning;
		}

		OnPreviewRegistryChanged.Broadcast();
		SetPreviewManagerInitStep(
			EVdjmRecordMediaPreviewInitStep::EApplyCarouselWindow,
			TEXT("Applying carousel window."));
		return EVdjmRecordMediaPreviewInitRunResult::ERunning;

	case EVdjmRecordMediaPreviewInitStep::EApplyCarouselWindow:
		if (mRegistryEntries.Num() <= 0)
		{
			EnsureSlotCount(mPreviewInitRequest.SlotCount);
			for (UVdjmRecordMediaPreviewSlot* previewSlot : mPreviewSlots)
			{
				if (previewSlot != nullptr)
				{
					previewSlot->ClearSlot();
				}
			}

			StopAllPreviews(true);
			mCenterSourceIndex = INDEX_NONE;
			if (not mPreviewInitRequest.bSucceedWithEmptyRegistry)
			{
				outErrorReason = TEXT("Media registry is empty.");
				return FailPreviewManagerInit(outErrorReason);
			}
		}
		else if (mPreviewInitRequest.bApplyCarouselWindowAfterInit)
		{
			const int32 centerSourceIndex = mPreviewInitRequest.InitialCenterSourceIndex != INDEX_NONE
				? mPreviewInitRequest.InitialCenterSourceIndex
				: (mCenterSourceIndex != INDEX_NONE ? mCenterSourceIndex : 0);
			if (not ApplyCarouselWindow(
				centerSourceIndex,
				mPreviewInitRequest.SlotCount,
				mPreviewInitRequest.ActiveSlotIndex))
			{
				outErrorReason = TEXT("Failed to apply preview carousel window.");
				return FailPreviewManagerInit(outErrorReason);
			}
		}

		SetPreviewManagerInitStep(
			EVdjmRecordMediaPreviewInitStep::EFinalizeInitialization,
			TEXT("Finalizing preview manager init."));
		return EVdjmRecordMediaPreviewInitRunResult::ERunning;

	case EVdjmRecordMediaPreviewInitStep::EFinalizeInitialization:
		mbPreviewManagerInitRunning = false;
		mbPreviewManagerInitialized = true;
		mPreviewInitRegistryEntries.Reset();
		mPreviewInitPendingWorkCount = 0;
		SetPreviewManagerInitStep(
			EVdjmRecordMediaPreviewInitStep::EComplete,
			TEXT("Preview manager init complete."));
		OnPreviewManagerInitFinished.Broadcast(true, FString());
		return EVdjmRecordMediaPreviewInitRunResult::ESucceeded;

	case EVdjmRecordMediaPreviewInitStep::EComplete:
		mbPreviewManagerInitRunning = false;
		mbPreviewManagerInitialized = true;
		return EVdjmRecordMediaPreviewInitRunResult::ESucceeded;

	case EVdjmRecordMediaPreviewInitStep::EInitError:
	case EVdjmRecordMediaPreviewInitStep::EInitErrorEnd:
	default:
		mbPreviewManagerInitRunning = false;
		mbPreviewManagerInitialized = false;
		outErrorReason = mLastPreviewManagerErrorReason;
		return EVdjmRecordMediaPreviewInitRunResult::EFailed;
	}
}

float AVdjmRecordMediaPreviewManagerActor::GetPreviewManagerInitProgress() const
{
	if (mbPreviewManagerInitialized || mCurrentPreviewManagerInitStep == EVdjmRecordMediaPreviewInitStep::EComplete)
	{
		return 1.0f;
	}

	if (mCurrentPreviewManagerInitStep == EVdjmRecordMediaPreviewInitStep::ERefreshRegistry && IsValid(mMetadataStore))
	{
		return FMath::Clamp(mMetadataStore->GetRegistryScanProgress(), 0.0f, 1.0f);
	}

	if (mCurrentPreviewManagerInitStep == EVdjmRecordMediaPreviewInitStep::ECopyRegistryEntries &&
		mPreviewInitRegistryEntries.Num() > 0)
	{
		return FMath::Clamp(
			static_cast<float>(mRegistryCopyCursor) / static_cast<float>(mPreviewInitRegistryEntries.Num()),
			0.0f,
			1.0f);
	}

	switch (mCurrentPreviewManagerInitStep)
	{
	case EVdjmRecordMediaPreviewInitStep::EInitializeStart:
		return 0.0f;
	case EVdjmRecordMediaPreviewInitStep::EEnsureMetadataStore:
		return 0.12f;
	case EVdjmRecordMediaPreviewInitStep::ERefreshRegistry:
		return 0.35f;
	case EVdjmRecordMediaPreviewInitStep::ECopyRegistryEntries:
		return 0.62f;
	case EVdjmRecordMediaPreviewInitStep::EApplyCarouselWindow:
		return 0.82f;
	case EVdjmRecordMediaPreviewInitStep::EFinalizeInitialization:
		return 0.94f;
	default:
		return 0.0f;
	}
}

FVdjmRecordMediaPreviewInitStatus AVdjmRecordMediaPreviewManagerActor::GetPreviewManagerInitStatus() const
{
	FVdjmRecordMediaPreviewInitStatus status;
	status.CurrentStep = mCurrentPreviewManagerInitStep;
	status.Progress = GetPreviewManagerInitProgress();
	status.PendingCount = mPreviewInitPendingWorkCount;
	status.ProcessedCount = mPreviewInitProcessedWorkCount;
	status.LastErrorReason = mLastPreviewManagerErrorReason;
	status.bInitialized = mbPreviewManagerInitialized;
	status.bRunning = mbPreviewManagerInitRunning;
	return status;
}

bool AVdjmRecordMediaPreviewManagerActor::ApplyCarouselWindow(
	int32 centerSourceIndex,
	int32 slotCount,
	int32 activeSlotIndex)
{
	const int32 safeSlotCount = FMath::Max(1, slotCount);
	EnsureSlotCount(safeSlotCount);

	SlotCount = safeSlotCount;
	ActiveSlotIndex = FMath::Clamp(activeSlotIndex, 0, safeSlotCount - 1);

	if (mRegistryEntries.Num() <= 0)
	{
		for (UVdjmRecordMediaPreviewSlot* previewSlot : mPreviewSlots)
		{
			if (previewSlot != nullptr)
			{
				previewSlot->ClearSlot();
			}
		}
		StopAllPreviews(true);
		mCenterSourceIndex = INDEX_NONE;
		return false;
	}

	mCenterSourceIndex = FMath::Clamp(centerSourceIndex, 0, mRegistryEntries.Num() - 1);

	for (int32 slotIndex = 0; slotIndex < mPreviewSlots.Num(); ++slotIndex)
	{
		const int32 sourceRegistryIndex = mCenterSourceIndex - ActiveSlotIndex + slotIndex;
		if (not AssignEntryToSlot(slotIndex, sourceRegistryIndex))
		{
			HideSlot(slotIndex);
		}
	}

	if (bAutoStartCenterPreview)
	{
		StartPreviewSlot(ActiveSlotIndex);
	}

	return true;
}

bool AVdjmRecordMediaPreviewManagerActor::SetCenterSourceIndex(int32 centerSourceIndex)
{
	return ApplyCarouselWindow(centerSourceIndex, SlotCount, ActiveSlotIndex);
}

bool AVdjmRecordMediaPreviewManagerActor::SlideNext()
{
	const int32 nextCenterSourceIndex = mCenterSourceIndex == INDEX_NONE ? 0 : mCenterSourceIndex + 1;
	return SetCenterSourceIndex(nextCenterSourceIndex);
}

bool AVdjmRecordMediaPreviewManagerActor::SlidePrevious()
{
	const int32 previousCenterSourceIndex = mCenterSourceIndex == INDEX_NONE ? 0 : mCenterSourceIndex - 1;
	return SetCenterSourceIndex(previousCenterSourceIndex);
}

bool AVdjmRecordMediaPreviewManagerActor::RegisterPreviewWidget(UVdjmRecordMediaPreviewWidget* previewWidget)
{
	if (not IsValid(previewWidget))
	{
		return false;
	}

	CompactWidgetRefs();
	if (FindWidgetIndex(previewWidget) == INDEX_NONE)
	{
		mPreviewWidgets.Add(previewWidget);
	}

	int32 previewIndex = previewWidget->GetPreviewIndex();
	if (previewIndex == INDEX_NONE)
	{
		previewIndex = mPreviewWidgets.Num() - 1;
		previewWidget->SetPreviewIndex(previewIndex);
	}

	EnsureSlotCount(FMath::Max(SlotCount, previewIndex + 1));
	if (UVdjmRecordMediaPreviewSlot* previewSlot = GetPreviewSlot(previewIndex))
	{
		previewWidget->BindPreviewSlot(previewSlot);
	}

	BroadcastPayload(BuildPayload(
		previewWidget,
		previewWidget->GetPreviewSlot(),
		EVdjmRecordMediaPreviewInputEvent::ERegistered,
		EVdjmRecordMediaPreviewSlotState::EEmpty));
	return true;
}

bool AVdjmRecordMediaPreviewManagerActor::UnregisterPreviewWidget(UVdjmRecordMediaPreviewWidget* previewWidget)
{
	if (not IsValid(previewWidget))
	{
		return false;
	}

	StopPreviewSlot(previewWidget->GetPreviewIndex(), true);
	BroadcastPayload(BuildPayload(
		previewWidget,
		previewWidget->GetPreviewSlot(),
		EVdjmRecordMediaPreviewInputEvent::EUnregistered,
		previewWidget->GetPreviewSlot() != nullptr
			? previewWidget->GetPreviewSlot()->SlotState
			: EVdjmRecordMediaPreviewSlotState::EEmpty));

	for (int32 widgetIndex = mPreviewWidgets.Num() - 1; widgetIndex >= 0; --widgetIndex)
	{
		if (mPreviewWidgets[widgetIndex].Get() == previewWidget)
		{
			mPreviewWidgets.RemoveAt(widgetIndex);
		}
	}

	return true;
}

bool AVdjmRecordMediaPreviewManagerActor::StartPreviewSlot(int32 slotIndex)
{
	UVdjmRecordMediaPreviewWidget* previewWidget = FindWidgetBySlotIndex(slotIndex);
	UVdjmRecordMediaPreviewSlot* previewSlot = GetPreviewSlot(slotIndex);
	if (previewWidget == nullptr || previewSlot == nullptr || not previewSlot->HasRegistryEntry())
	{
		return false;
	}

	if (mPreviewingSlotIndex != INDEX_NONE && mPreviewingSlotIndex != slotIndex)
	{
		StopPreviewSlot(mPreviewingSlotIndex, false);
	}

	FString errorReason;
	if (not previewWidget->StartPreview(errorReason))
	{
		SetSlotStateAndBroadcast(
			previewWidget,
			previewSlot,
			EVdjmRecordMediaPreviewSlotState::EFailed,
			EVdjmRecordMediaPreviewInputEvent::EPreviewFailed);
		return false;
	}

	mLastPreviewIndex = mPreviewingSlotIndex;
	mPreviewingSlotIndex = slotIndex;
	SetSlotStateAndBroadcast(
		previewWidget,
		previewSlot,
		EVdjmRecordMediaPreviewSlotState::EPreviewing,
		EVdjmRecordMediaPreviewInputEvent::EPreviewStarted);
	return true;
}

bool AVdjmRecordMediaPreviewManagerActor::StopPreviewSlot(int32 slotIndex, bool bCloseMedia)
{
	UVdjmRecordMediaPreviewWidget* previewWidget = FindWidgetBySlotIndex(slotIndex);
	UVdjmRecordMediaPreviewSlot* previewSlot = GetPreviewSlot(slotIndex);
	if (previewWidget == nullptr || previewSlot == nullptr)
	{
		return false;
	}

	const EVdjmRecordMediaPreviewSlotState previousState = previewSlot->SlotState;
	previewWidget->StopPreview(bCloseMedia);
	if (mPreviewingSlotIndex == slotIndex)
	{
		mLastPreviewIndex = mPreviewingSlotIndex;
		mPreviewingSlotIndex = INDEX_NONE;
	}

	previewSlot->SetSlotState(EVdjmRecordMediaPreviewSlotState::EStopped);
	FVdjmRecordMediaPreviewNotifyPayload payload = BuildPayload(
		previewWidget,
		previewSlot,
		EVdjmRecordMediaPreviewInputEvent::EPreviewStopped,
		previousState);
	OnPreviewStopped.Broadcast(payload);
	BroadcastPayload(payload);
	return true;
}

bool AVdjmRecordMediaPreviewManagerActor::HideSlot(int32 slotIndex)
{
	UVdjmRecordMediaPreviewWidget* previewWidget = FindWidgetBySlotIndex(slotIndex);
	UVdjmRecordMediaPreviewSlot* previewSlot = GetPreviewSlot(slotIndex);
	if (previewSlot == nullptr)
	{
		return false;
	}

	if (previewWidget != nullptr)
	{
		previewWidget->SetHiddenByPreviewManager(true);
	}
	SetSlotStateAndBroadcast(
		previewWidget,
		previewSlot,
		EVdjmRecordMediaPreviewSlotState::EHidden,
		EVdjmRecordMediaPreviewInputEvent::EStateChanged);
	return true;
}

bool AVdjmRecordMediaPreviewManagerActor::ReleaseSlot(int32 slotIndex)
{
	UVdjmRecordMediaPreviewWidget* previewWidget = FindWidgetBySlotIndex(slotIndex);
	UVdjmRecordMediaPreviewSlot* previewSlot = GetPreviewSlot(slotIndex);
	if (previewSlot == nullptr)
	{
		return false;
	}

	if (previewWidget != nullptr)
	{
		previewWidget->ReleasePreviewVisuals(true);
	}
	SetSlotStateAndBroadcast(
		previewWidget,
		previewSlot,
		EVdjmRecordMediaPreviewSlotState::EReleased,
		EVdjmRecordMediaPreviewInputEvent::EStateChanged);
	return true;
}

bool AVdjmRecordMediaPreviewManagerActor::DeleteSlot(int32 slotIndex)
{
	StopPreviewSlot(slotIndex, true);
	UVdjmRecordMediaPreviewSlot* previewSlot = GetPreviewSlot(slotIndex);
	if (previewSlot == nullptr)
	{
		return false;
	}

	previewSlot->SetSlotState(EVdjmRecordMediaPreviewSlotState::EDeleted);
	return true;
}

void AVdjmRecordMediaPreviewManagerActor::StopAllPreviews(bool bCloseMedia)
{
	for (int32 slotIndex = 0; slotIndex < mPreviewSlots.Num(); ++slotIndex)
	{
		StopPreviewSlot(slotIndex, bCloseMedia);
	}
}

void AVdjmRecordMediaPreviewManagerActor::NotifyPreviewWidgetInput(
	UVdjmRecordMediaPreviewWidget* previewWidget,
	EVdjmRecordMediaPreviewInputEvent inputEventType)
{
	if (not IsValid(previewWidget))
	{
		return;
	}

	UVdjmRecordMediaPreviewSlot* previewSlot = previewWidget->GetPreviewSlot();
	const EVdjmRecordMediaPreviewSlotState previousState = previewSlot != nullptr
		? previewSlot->SlotState
		: EVdjmRecordMediaPreviewSlotState::EEmpty;

	FVdjmRecordMediaPreviewNotifyPayload payload = BuildPayload(
		previewWidget,
		previewSlot,
		inputEventType,
		previousState);
	BroadcastPayload(payload);

	if (inputEventType == EVdjmRecordMediaPreviewInputEvent::EClicked &&
		previewWidget->ShouldStartPreviewOnClick())
	{
		StartPreviewSlot(previewWidget->GetPreviewIndex());
	}
}

TArray<UVdjmRecordMediaPreviewSlot*> AVdjmRecordMediaPreviewManagerActor::GetPreviewSlots() const
{
	TArray<UVdjmRecordMediaPreviewSlot*> previewSlots;
	for (UVdjmRecordMediaPreviewSlot* previewSlot : mPreviewSlots)
	{
		previewSlots.Add(previewSlot);
	}
	return previewSlots;
}

UVdjmRecordMediaPreviewSlot* AVdjmRecordMediaPreviewManagerActor::GetPreviewSlot(int32 slotIndex) const
{
	return mPreviewSlots.IsValidIndex(slotIndex) ? mPreviewSlots[slotIndex] : nullptr;
}

FString AVdjmRecordMediaPreviewManagerActor::BuildPreviewStateJson() const
{
	TSharedPtr<FJsonObject> rootObject = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> previewObject = MakeShared<FJsonObject>();
	previewObject->SetNumberField(TEXT("schema_version"), 1);
	previewObject->SetNumberField(TEXT("center_source_index"), mCenterSourceIndex);
	previewObject->SetNumberField(TEXT("active_slot_index"), ActiveSlotIndex);
	previewObject->SetNumberField(TEXT("slot_count"), SlotCount);

	TArray<TSharedPtr<FJsonValue>> slotValues;
	for (const UVdjmRecordMediaPreviewSlot* previewSlot : mPreviewSlots)
	{
		if (previewSlot == nullptr)
		{
			continue;
		}

		TSharedPtr<FJsonObject> slotObject = MakeShared<FJsonObject>();
		slotObject->SetNumberField(TEXT("slot_index"), previewSlot->SlotIndex);
		slotObject->SetNumberField(TEXT("source_registry_index"), previewSlot->SourceRegistryIndex);
		slotObject->SetStringField(TEXT("record_id"), previewSlot->GetRecordId());
		slotObject->SetStringField(TEXT("state"), GetEnumValueString(previewSlot->SlotState));
		slotValues.Add(MakeShared<FJsonValueObject>(slotObject));
	}
	previewObject->SetArrayField(TEXT("slots"), slotValues);
	rootObject->SetObjectField(TEXT("preview"), previewObject);

	FString jsonString;
	TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&jsonString);
	if (not FJsonSerializer::Serialize(rootObject.ToSharedRef(), jsonWriter))
	{
		return TEXT("{}");
	}
	return jsonString;
}

bool AVdjmRecordMediaPreviewManagerActor::RestorePreviewStateJson(
	const FString& previewStateJson,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(previewStateJson);
	if (not FJsonSerializer::Deserialize(reader, rootObject) || not rootObject.IsValid())
	{
		outErrorReason = TEXT("Failed to parse preview state json.");
		return false;
	}

	const TSharedPtr<FJsonObject>* previewObjectPtr = nullptr;
	if (not rootObject->TryGetObjectField(TEXT("preview"), previewObjectPtr) || previewObjectPtr == nullptr)
	{
		outErrorReason = TEXT("Preview state json has no preview object.");
		return false;
	}

	const TSharedPtr<FJsonObject> previewObject = *previewObjectPtr;
	const int32 centerSourceIndex = static_cast<int32>(previewObject->GetNumberField(TEXT("center_source_index")));
	const int32 activeSlotIndex = static_cast<int32>(previewObject->GetNumberField(TEXT("active_slot_index")));
	const int32 slotCount = static_cast<int32>(previewObject->GetNumberField(TEXT("slot_count")));
	return ApplyCarouselWindow(centerSourceIndex, slotCount, activeSlotIndex);
}

void AVdjmRecordMediaPreviewManagerActor::BeginPlay()
{
	Super::BeginPlay();
	RegisterSelfAsPrimitive();

	if (bAutoRefreshStoreOnBeginPlay)
	{
		FString errorReason;
		RefreshPreviewStoreFromDisk(errorReason);
	}
}

void AVdjmRecordMediaPreviewManagerActor::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	StopAllPreviews(true);
	if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this))
	{
		worldContextSubsystem->UnregisterContext(UVdjmRecorderWorldContextSubsystem::GetMediaPreviewManagerContextKey());
	}
	Super::EndPlay(endPlayReason);
}

void AVdjmRecordMediaPreviewManagerActor::HandleWidgetHovered()
{
}

void AVdjmRecordMediaPreviewManagerActor::HandleWidgetUnhovered()
{
}

void AVdjmRecordMediaPreviewManagerActor::HandleWidgetPressed()
{
}

void AVdjmRecordMediaPreviewManagerActor::HandleWidgetReleased()
{
}

void AVdjmRecordMediaPreviewManagerActor::HandleWidgetClicked()
{
}

bool AVdjmRecordMediaPreviewManagerActor::RegisterSelfAsPrimitive()
{
	if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this))
	{
		return worldContextSubsystem->RegisterStrongObjectContext(
			UVdjmRecorderWorldContextSubsystem::GetMediaPreviewManagerContextKey(),
			this,
			StaticClass());
	}

	return false;
}

void AVdjmRecordMediaPreviewManagerActor::EnsureSlotCount(int32 desiredSlotCount)
{
	const int32 safeSlotCount = FMath::Max(1, desiredSlotCount);
	while (mPreviewSlots.Num() < safeSlotCount)
	{
		UVdjmRecordMediaPreviewSlot* previewSlot = NewObject<UVdjmRecordMediaPreviewSlot>(this);
		previewSlot->SlotIndex = mPreviewSlots.Num();
		mPreviewSlots.Add(previewSlot);
	}

	while (mPreviewSlots.Num() > safeSlotCount)
	{
		ReleaseSlot(mPreviewSlots.Num() - 1);
		mPreviewSlots.RemoveAt(mPreviewSlots.Num() - 1);
	}
}

UVdjmRecordMediaPreviewSlot* AVdjmRecordMediaPreviewManagerActor::FindSlotByWidget(
	const UVdjmRecordMediaPreviewWidget* previewWidget) const
{
	return IsValid(previewWidget) ? GetPreviewSlot(previewWidget->GetPreviewIndex()) : nullptr;
}

UVdjmRecordMediaPreviewWidget* AVdjmRecordMediaPreviewManagerActor::FindWidgetBySlotIndex(int32 slotIndex) const
{
	for (const TWeakObjectPtr<UVdjmRecordMediaPreviewWidget>& weakWidget : mPreviewWidgets)
	{
		UVdjmRecordMediaPreviewWidget* previewWidget = weakWidget.Get();
		if (previewWidget != nullptr && previewWidget->GetPreviewIndex() == slotIndex)
		{
			return previewWidget;
		}
	}

	return nullptr;
}

int32 AVdjmRecordMediaPreviewManagerActor::FindWidgetIndex(UVdjmRecordMediaPreviewWidget* previewWidget) const
{
	for (int32 widgetIndex = 0; widgetIndex < mPreviewWidgets.Num(); ++widgetIndex)
	{
		if (mPreviewWidgets[widgetIndex].Get() == previewWidget)
		{
			return widgetIndex;
		}
	}

	return INDEX_NONE;
}

FVdjmRecordMediaPreviewNotifyPayload AVdjmRecordMediaPreviewManagerActor::BuildPayload(
	UVdjmRecordMediaPreviewWidget* previewWidget,
	UVdjmRecordMediaPreviewSlot* previewSlot,
	EVdjmRecordMediaPreviewInputEvent inputEventType,
	EVdjmRecordMediaPreviewSlotState previousState) const
{
	FVdjmRecordMediaPreviewNotifyPayload payload;
	payload.PreviewWidget = previewWidget;
	payload.PreviewSlot = previewSlot;
	payload.InputEventType = inputEventType;
	payload.PreviousState = previousState;
	payload.CurrentState = previewSlot != nullptr ? previewSlot->SlotState : EVdjmRecordMediaPreviewSlotState::EEmpty;
	payload.PreviewIndex = previewWidget != nullptr ? previewWidget->GetPreviewIndex() : INDEX_NONE;
	payload.PreviousPreviewIndex = mLastPreviewIndex;
	payload.PreviewPolicyTag = previewWidget != nullptr ? previewWidget->GetPreviewPolicyTag() : NAME_None;

	if (previewSlot != nullptr)
	{
		payload.SourceRegistryIndex = previewSlot->SourceRegistryIndex;
		payload.RegistryEntry = previewSlot->RegistryEntry;
		payload.Manifest = previewSlot->GetManifest();
		payload.RecordId = previewSlot->GetRecordId();
		payload.MetadataFilePath = previewSlot->GetMetadataFilePath();
		payload.OutputFilePath = previewSlot->GetOutputFilePath();
		payload.PreviewSource = GetPreviewSourceFromSlot(previewSlot);
	}

	payload.PreviousPreviewWidget = FindWidgetBySlotIndex(mLastPreviewIndex);
	return payload;
}

void AVdjmRecordMediaPreviewManagerActor::BroadcastPayload(const FVdjmRecordMediaPreviewNotifyPayload& payload)
{
	OnPreviewNotify.Broadcast(payload);
}

void AVdjmRecordMediaPreviewManagerActor::SetSlotStateAndBroadcast(
	UVdjmRecordMediaPreviewWidget* previewWidget,
	UVdjmRecordMediaPreviewSlot* previewSlot,
	EVdjmRecordMediaPreviewSlotState newState,
	EVdjmRecordMediaPreviewInputEvent inputEventType)
{
	if (previewSlot == nullptr)
	{
		return;
	}

	const EVdjmRecordMediaPreviewSlotState previousState = previewSlot->SlotState;
	previewSlot->SetSlotState(newState);
	FVdjmRecordMediaPreviewNotifyPayload payload = BuildPayload(previewWidget, previewSlot, inputEventType, previousState);

	if (inputEventType == EVdjmRecordMediaPreviewInputEvent::EPreviewStarted)
	{
		OnPreviewStarted.Broadcast(payload);
	}
	else if (inputEventType == EVdjmRecordMediaPreviewInputEvent::EPreviewStopped)
	{
		OnPreviewStopped.Broadcast(payload);
	}
	else if (inputEventType == EVdjmRecordMediaPreviewInputEvent::EPreviewFailed)
	{
		OnPreviewFailed.Broadcast(payload);
	}

	BroadcastPayload(payload);
}

bool AVdjmRecordMediaPreviewManagerActor::AssignEntryToSlot(int32 slotIndex, int32 sourceRegistryIndex)
{
	UVdjmRecordMediaPreviewSlot* previewSlot = GetPreviewSlot(slotIndex);
	if (previewSlot == nullptr)
	{
		return false;
	}

	UVdjmRecordMediaPreviewWidget* previewWidget = FindWidgetBySlotIndex(slotIndex);
	if (not mRegistryEntries.IsValidIndex(sourceRegistryIndex))
	{
		previewSlot->ClearSlot();
		if (previewWidget != nullptr)
		{
			previewWidget->SetHiddenByPreviewManager(true);
		}
		return false;
	}

	const EVdjmRecordMediaPreviewSlotState previousState = previewSlot->SlotState;
	previewSlot->SetRegistryEntry(mRegistryEntries[sourceRegistryIndex], sourceRegistryIndex);
	if (previewWidget != nullptr)
	{
		previewWidget->SetHiddenByPreviewManager(false);
		previewWidget->BindPreviewSlot(previewSlot);
		previewWidget->PreparePreviewVisuals();
	}

	BroadcastPayload(BuildPayload(
		previewWidget,
		previewSlot,
		EVdjmRecordMediaPreviewInputEvent::EStateChanged,
		previousState));
	return true;
}

void AVdjmRecordMediaPreviewManagerActor::CompactWidgetRefs()
{
	for (int32 widgetIndex = mPreviewWidgets.Num() - 1; widgetIndex >= 0; --widgetIndex)
	{
		if (not mPreviewWidgets[widgetIndex].IsValid())
		{
			mPreviewWidgets.RemoveAt(widgetIndex);
		}
	}
}

void AVdjmRecordMediaPreviewManagerActor::SetPreviewManagerInitStep(
	EVdjmRecordMediaPreviewInitStep initStep,
	const FString& message)
{
	mCurrentPreviewManagerInitStep = initStep;
	OnPreviewManagerInitStepChanged.Broadcast(initStep, message);
}

EVdjmRecordMediaPreviewInitRunResult AVdjmRecordMediaPreviewManagerActor::FailPreviewManagerInit(
	const FString& errorReason)
{
	mLastPreviewManagerErrorReason = errorReason;
	mbPreviewManagerInitRunning = false;
	mbPreviewManagerInitialized = false;
	mPreviewInitRegistryEntries.Reset();
	mPreviewInitPendingWorkCount = 0;
	SetPreviewManagerInitStep(EVdjmRecordMediaPreviewInitStep::EInitError, mLastPreviewManagerErrorReason);
	SetPreviewManagerInitStep(EVdjmRecordMediaPreviewInitStep::EInitErrorEnd, mLastPreviewManagerErrorReason);
	OnPreviewManagerInitFinished.Broadcast(false, mLastPreviewManagerErrorReason);
	return EVdjmRecordMediaPreviewInitRunResult::EFailed;
}

bool UVdjmRecordMediaPreviewWidget::SetRegistryEntry(
	const FVdjmRecordMediaRegistryEntry& registryEntry,
	int32 sourceRegistryIndex)
{
	if (mPreviewSlot == nullptr)
	{
		mPreviewSlot = NewObject<UVdjmRecordMediaPreviewSlot>(this);
		mPreviewSlot->SlotIndex = PreviewIndex;
	}

	mPreviewSlot->SetRegistryEntry(registryEntry, sourceRegistryIndex);
	PreparePreviewVisuals();
	if (EnsurePreviewManager())
	{
		mPreviewManager->NotifyPreviewWidgetInput(this, EVdjmRecordMediaPreviewInputEvent::EManifestChanged);
	}
	return true;
}

bool UVdjmRecordMediaPreviewWidget::SetManifest(UVdjmRecordMediaManifest* manifest)
{
	if (not IsValid(manifest))
	{
		return false;
	}

	if (mPreviewSlot == nullptr)
	{
		mPreviewSlot = NewObject<UVdjmRecordMediaPreviewSlot>(this);
		mPreviewSlot->SlotIndex = PreviewIndex;
	}

	mPreviewSlot->SetManifest(manifest);
	PreparePreviewVisuals();
	if (EnsurePreviewManager())
	{
		mPreviewManager->NotifyPreviewWidgetInput(this, EVdjmRecordMediaPreviewInputEvent::EManifestChanged);
	}
	return true;
}

bool UVdjmRecordMediaPreviewWidget::BindPreviewSlot(UVdjmRecordMediaPreviewSlot* previewSlot)
{
	if (not IsValid(previewSlot))
	{
		return false;
	}

	mPreviewSlot = previewSlot;
	PreviewIndex = previewSlot->SlotIndex;
	PreparePreviewVisuals();
	return true;
}

bool UVdjmRecordMediaPreviewWidget::StartPreview(FString& outErrorReason)
{
	outErrorReason.Reset();
	EnsureMediaObjects();

	if (mPreviewPlayer == nullptr)
	{
		mPreviewPlayer = UVdjmRecordMediaPreviewPlayer::CreateMediaPreviewPlayer(this, mMediaPlayer);
	}

	if (mPreviewSlot == nullptr || mPreviewPlayer == nullptr)
	{
		outErrorReason = TEXT("Preview slot or preview player is invalid.");
		return false;
	}

	if (IsValid(mPreviewSlot->GetManifest()))
	{
		return mPreviewPlayer->StartPreviewFromManifest(mMediaPlayer, mPreviewSlot->GetManifest(), outErrorReason);
	}

	return mPreviewPlayer->StartPreviewFromRegistryEntry(mMediaPlayer, mPreviewSlot->RegistryEntry, outErrorReason);
}

void UVdjmRecordMediaPreviewWidget::StopPreview(bool bCloseMedia)
{
	if (mPreviewPlayer != nullptr)
	{
		mPreviewPlayer->StopPreview(bCloseMedia);
	}
}

void UVdjmRecordMediaPreviewWidget::SetHiddenByPreviewManager(bool bHidden)
{
	SetVisibility(bHidden ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

bool UVdjmRecordMediaPreviewWidget::PreparePreviewVisuals()
{
	EnsureInternalWidgets();
	EnsureMediaObjects();
	ApplyMediaTextureBrush();
	return PreviewImage != nullptr && mMediaTexture != nullptr;
}

void UVdjmRecordMediaPreviewWidget::ReleasePreviewVisuals(bool bCloseMedia)
{
	StopPreview(bCloseMedia);
}

UVdjmRecordMediaManifest* UVdjmRecordMediaPreviewWidget::GetPreviewManifest() const
{
	return mPreviewSlot != nullptr ? mPreviewSlot->GetManifest() : nullptr;
}

void UVdjmRecordMediaPreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureInternalWidgets();
	EnsureMediaObjects();
	ApplyMediaTextureBrush();
	RegisterToPreviewManager();
}

void UVdjmRecordMediaPreviewWidget::NativeDestruct()
{
	UnregisterFromPreviewManager();
	StopPreview(true);
	Super::NativeDestruct();
}

void UVdjmRecordMediaPreviewWidget::HandleInputHovered()
{
	if (EnsurePreviewManager())
	{
		mPreviewManager->NotifyPreviewWidgetInput(this, EVdjmRecordMediaPreviewInputEvent::EHovered);
	}
}

void UVdjmRecordMediaPreviewWidget::HandleInputUnhovered()
{
	if (EnsurePreviewManager())
	{
		mPreviewManager->NotifyPreviewWidgetInput(this, EVdjmRecordMediaPreviewInputEvent::EUnhovered);
	}
}

void UVdjmRecordMediaPreviewWidget::HandleInputPressed()
{
	if (EnsurePreviewManager())
	{
		mPreviewManager->NotifyPreviewWidgetInput(this, EVdjmRecordMediaPreviewInputEvent::EPressed);
	}
}

void UVdjmRecordMediaPreviewWidget::HandleInputReleased()
{
	if (EnsurePreviewManager())
	{
		mPreviewManager->NotifyPreviewWidgetInput(this, EVdjmRecordMediaPreviewInputEvent::EReleased);
	}
}

void UVdjmRecordMediaPreviewWidget::HandleInputClicked()
{
	if (EnsurePreviewManager())
	{
		mPreviewManager->NotifyPreviewWidgetInput(this, EVdjmRecordMediaPreviewInputEvent::EClicked);
	}
}

void UVdjmRecordMediaPreviewWidget::EnsureInternalWidgets()
{
	if (not bAutoCreateInternalWidgets || WidgetTree == nullptr)
	{
		return;
	}

	if (PreviewOverlay == nullptr)
	{
		PreviewOverlay = Cast<UOverlay>(WidgetTree->RootWidget);
		if (PreviewOverlay == nullptr && WidgetTree->RootWidget == nullptr)
		{
			PreviewOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PreviewOverlay"));
			WidgetTree->RootWidget = PreviewOverlay;
		}
	}

	if (PreviewOverlay == nullptr)
	{
		return;
	}

	if (PreviewImage == nullptr)
	{
		PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewImage"));
		UOverlaySlot* imageSlot = PreviewOverlay->AddChildToOverlay(PreviewImage);
		if (imageSlot != nullptr)
		{
			imageSlot->SetHorizontalAlignment(HAlign_Fill);
			imageSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	if (InputButton == nullptr)
	{
		InputButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InputButton"));
		UOverlaySlot* buttonSlot = PreviewOverlay->AddChildToOverlay(InputButton);
		if (buttonSlot != nullptr)
		{
			buttonSlot->SetHorizontalAlignment(HAlign_Fill);
			buttonSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	ApplyInputButtonStyle();
}

void UVdjmRecordMediaPreviewWidget::EnsureMediaObjects()
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

bool UVdjmRecordMediaPreviewWidget::EnsurePreviewManager()
{
	if (mPreviewManager.IsValid())
	{
		return true;
	}

	mPreviewManager = AVdjmRecordMediaPreviewManagerActor::FindOrSpawnMediaPreviewManagerActor(this);
	return mPreviewManager.IsValid();
}

void UVdjmRecordMediaPreviewWidget::RegisterToPreviewManager()
{
	if (not bAutoRegisterToPreviewManager)
	{
		return;
	}

	if (DesiredPreviewIndex != INDEX_NONE)
	{
		PreviewIndex = DesiredPreviewIndex;
	}

	if (EnsurePreviewManager())
	{
		mPreviewManager->RegisterPreviewWidget(this);
	}
}

void UVdjmRecordMediaPreviewWidget::UnregisterFromPreviewManager()
{
	if (mPreviewManager.IsValid())
	{
		mPreviewManager->UnregisterPreviewWidget(this);
	}
}

void UVdjmRecordMediaPreviewWidget::ApplyMediaTextureBrush()
{
	if (PreviewImage == nullptr || mMediaTexture == nullptr)
	{
		return;
	}

	FSlateBrush mediaBrush;
	mediaBrush.SetResourceObject(mMediaTexture);
	mediaBrush.ImageSize = FVector2D(512.0f, 512.0f);
	PreviewImage->SetBrush(mediaBrush);
}

void UVdjmRecordMediaPreviewWidget::ApplyInputButtonStyle()
{
	if (InputButton == nullptr)
	{
		return;
	}

	InputButton->OnHovered.RemoveDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputHovered);
	InputButton->OnUnhovered.RemoveDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputUnhovered);
	InputButton->OnPressed.RemoveDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputPressed);
	InputButton->OnReleased.RemoveDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputReleased);
	InputButton->OnClicked.RemoveDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputClicked);

	InputButton->OnHovered.AddDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputHovered);
	InputButton->OnUnhovered.AddDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputUnhovered);
	InputButton->OnPressed.AddDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputPressed);
	InputButton->OnReleased.AddDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputReleased);
	InputButton->OnClicked.AddDynamic(this, &UVdjmRecordMediaPreviewWidget::HandleInputClicked);
	InputButton->SetBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
	InputButton->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
}

bool UVdjmRecordMediaPreviewProbeWidget::RefreshAndOpenLatestPreview(FString& outErrorReason)
{
	outErrorReason.Reset();
	AVdjmRecordMediaPreviewManagerActor* previewManager =
		AVdjmRecordMediaPreviewManagerActor::FindOrSpawnMediaPreviewManagerActor(this);
	if (previewManager == nullptr)
	{
		outErrorReason = TEXT("Preview manager is invalid.");
		return false;
	}

	if (not previewManager->RefreshPreviewStoreFromDisk(outErrorReason))
	{
		return false;
	}

	const TArray<FVdjmRecordMediaRegistryEntry> registryEntries = previewManager->GetPreviewRegistryEntries();
	if (registryEntries.Num() <= 0)
	{
		outErrorReason = TEXT("Preview registry is empty.");
		return false;
	}

	return OpenPreviewFromRegistryEntry(registryEntries.Last(), outErrorReason);
}

bool UVdjmRecordMediaPreviewProbeWidget::RefreshAndOpenPreviewAtIndex(
	int32 registryEntryIndex,
	FString& outErrorReason)
{
	outErrorReason.Reset();
	AVdjmRecordMediaPreviewManagerActor* previewManager =
		AVdjmRecordMediaPreviewManagerActor::FindOrSpawnMediaPreviewManagerActor(this);
	if (previewManager == nullptr)
	{
		outErrorReason = TEXT("Preview manager is invalid.");
		return false;
	}

	if (not previewManager->RefreshPreviewStoreFromDisk(outErrorReason))
	{
		return false;
	}

	const TArray<FVdjmRecordMediaRegistryEntry> registryEntries = previewManager->GetPreviewRegistryEntries();
	if (not registryEntries.IsValidIndex(registryEntryIndex))
	{
		outErrorReason = FString::Printf(TEXT("Preview registry index is invalid. Index=%d Count=%d"), registryEntryIndex, registryEntries.Num());
		return false;
	}

	return OpenPreviewFromRegistryEntry(registryEntries[registryEntryIndex], outErrorReason);
}

bool UVdjmRecordMediaPreviewProbeWidget::OpenPreviewFromRegistryEntry(
	const FVdjmRecordMediaRegistryEntry& registryEntry,
	FString& outErrorReason)
{
	outErrorReason.Reset();
	EnsureProbeMediaObjects();
	ApplyProbeMediaTextureBrush();

	if (mProbeMediaPlayer == nullptr)
	{
		outErrorReason = TEXT("Probe media player is invalid.");
		return false;
	}

	const FString previewSource = GetPreviewSourceFromRegistryEntry(registryEntry);
	if (previewSource.IsEmpty())
	{
		outErrorReason = TEXT("Preview source is empty.");
		return false;
	}

	StopProbePreview(true);
	EnsureProbeMediaObjects();
	ApplyProbeMediaTextureBrush();
	mProbeMediaPlayer->SetLooping(bLoopPreview);

	const bool bOpened = previewSource.StartsWith(TEXT("content://")) || previewSource.Contains(TEXT("://"))
		? mProbeMediaPlayer->OpenUrl(previewSource)
		: mProbeMediaPlayer->OpenFile(previewSource);
	if (not bOpened)
	{
		outErrorReason = FString::Printf(TEXT("Failed to open preview source. Source=%s"), *previewSource);
		return false;
	}

	if (bPlayOnOpen)
	{
		mProbeMediaPlayer->Play();
	}
	return true;
}

void UVdjmRecordMediaPreviewProbeWidget::StopProbePreview(bool bCloseMedia)
{
	if (mProbeMediaPlayer == nullptr)
	{
		return;
	}

	if (bCloseMedia)
	{
		mProbeMediaPlayer->Close();
	}
	else
	{
		mProbeMediaPlayer->Pause();
	}
}

void UVdjmRecordMediaPreviewProbeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureProbeMediaObjects();
	ApplyProbeMediaTextureBrush();

	if (bAutoOpenOnConstruct)
	{
		FString errorReason;
		if (bUseLatestRegistryEntry || RegistryEntryIndex == INDEX_NONE)
		{
			RefreshAndOpenLatestPreview(errorReason);
		}
		else
		{
			RefreshAndOpenPreviewAtIndex(RegistryEntryIndex, errorReason);
		}
	}
}

void UVdjmRecordMediaPreviewProbeWidget::NativeDestruct()
{
	StopProbePreview(true);
	Super::NativeDestruct();
}

void UVdjmRecordMediaPreviewProbeWidget::EnsureProbeMediaObjects()
{
	if (mProbeMediaPlayer == nullptr)
	{
		mProbeMediaPlayer = NewObject<UMediaPlayer>(this);
	}

	if (mProbeMediaTexture == nullptr)
	{
		mProbeMediaTexture = NewObject<UMediaTexture>(this);
		mProbeMediaTexture->AutoClear = true;
		mProbeMediaTexture->ClearColor = FLinearColor::Black;
	}

	if (mProbeMediaTexture != nullptr && mProbeMediaPlayer != nullptr)
	{
		mProbeMediaTexture->SetMediaPlayer(mProbeMediaPlayer);
		mProbeMediaTexture->UpdateResource();
	}
}

void UVdjmRecordMediaPreviewProbeWidget::ApplyProbeMediaTextureBrush()
{
	if (PreviewImage == nullptr || mProbeMediaTexture == nullptr)
	{
		return;
	}

	FSlateBrush mediaBrush;
	mediaBrush.SetResourceObject(mProbeMediaTexture);
	mediaBrush.ImageSize = FVector2D(512.0f, 512.0f);
	PreviewImage->SetBrush(mediaBrush);
}

bool UVdjmRecordMediaPreviewCarouselWidget::RefreshCarousel(FString& outErrorReason)
{
	outErrorReason.Reset();
	if (not EnsurePreviewManager())
	{
		outErrorReason = TEXT("Preview manager is invalid.");
		return false;
	}

	if (not mPreviewManager->RefreshPreviewStoreFromDisk(outErrorReason))
	{
		return false;
	}

	const int32 centerSourceIndex = InitialCenterSourceIndex != INDEX_NONE
		? InitialCenterSourceIndex
		: mPreviewManager->GetCenterSourceIndex();
	return mPreviewManager->ApplyCarouselWindow(centerSourceIndex, SlotCount, ActiveSlotIndex);
}

bool UVdjmRecordMediaPreviewCarouselWidget::SetCenterSourceIndex(int32 centerSourceIndex)
{
	return EnsurePreviewManager() && mPreviewManager->ApplyCarouselWindow(centerSourceIndex, SlotCount, ActiveSlotIndex);
}

bool UVdjmRecordMediaPreviewCarouselWidget::SlideNext()
{
	return EnsurePreviewManager() && mPreviewManager->SlideNext();
}

bool UVdjmRecordMediaPreviewCarouselWidget::SlidePrevious()
{
	return EnsurePreviewManager() && mPreviewManager->SlidePrevious();
}

int32 UVdjmRecordMediaPreviewCarouselWidget::GetCenterSourceIndex() const
{
	return mPreviewManager.IsValid() ? mPreviewManager->GetCenterSourceIndex() : INDEX_NONE;
}

void UVdjmRecordMediaPreviewCarouselWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsurePreviewManager();

	if (bRefreshStoreOnConstruct)
	{
		FString errorReason;
		RefreshCarousel(errorReason);
	}
}

bool UVdjmRecordMediaPreviewCarouselWidget::EnsurePreviewManager()
{
	if (mPreviewManager.IsValid())
	{
		return true;
	}

	mPreviewManager = AVdjmRecordMediaPreviewManagerActor::FindOrSpawnMediaPreviewManagerActor(this);
	return mPreviewManager.IsValid();
}
