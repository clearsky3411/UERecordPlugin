#include "VdjmRecorderController.h"

#include "VdjmRecordBridgeActor.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecorderCore.h"
#include "VdjmRecorderStateObserver.h"
#include "VdjmRecorderWorldContextSubsystem.h"

UVdjmRecorderController* UVdjmRecorderController::CreateRecorderController(UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}

	UWorld* world = WorldContextObject->GetWorld();
	if (world == nullptr)
	{
		return nullptr;
	}

	UVdjmRecorderController* controller = NewObject<UVdjmRecorderController>(WorldContextObject);
	if (controller == nullptr)
	{
		return nullptr;
	}

	controller->CachedWorld = world;
	controller->InitializeController();
	return controller;
}

UVdjmRecorderController* UVdjmRecorderController::FindRecorderController(UObject* worldContextObject)
{
	const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject);
	if (worldContextSubsystem == nullptr)
	{
		return nullptr;
	}

	return Cast<UVdjmRecorderController>(
		worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetRecorderControllerContextKey()));
}

UVdjmRecorderController* UVdjmRecorderController::FindOrCreateRecorderController(UObject* worldContextObject)
{
	if (UVdjmRecorderController* existingController = FindRecorderController(worldContextObject))
	{
		return existingController;
	}

	if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject))
	{
		if (UVdjmRecorderController* newController = CreateRecorderController(worldContextSubsystem))
		{
			return newController;
		}
	}

	return CreateRecorderController(worldContextObject);
}

bool UVdjmRecorderController::InitializeController()
{
	if (UVdjmRecorderWorldContextSubsystem* WorldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this))
	{
		WorldContextSubsystem->RegisterStrongObjectContext(
			UVdjmRecorderWorldContextSubsystem::GetRecorderControllerContextKey(),
			this,
			StaticClass());
	}

	if (not EnsureEventManager())
	{
		return false;
	}

	EnsureBridge();
	WeakDataAsset = AVdjmRecordBridgeActor::TryGetRecordEnvConfigure();
	EnsureStateObserver();
	EnsureMetadataStore();
	return true;
}

bool UVdjmRecorderController::ApplyOptionRequest(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason)
{
	return SubmitOptionRequest(Request, OutErrorReason);
}

bool UVdjmRecorderController::SubmitOptionRequest(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason)
{
	OutErrorReason.Reset();
	if (!ValidateRequest(Request, OutErrorReason))
	{
		return false;
	}

	FVdjmRecorderOptionRequest mergedRequest;
	mergedRequest.Reset();
	if (bHasPendingOptionRequest)
	{
		mergedRequest = PendingOptionRequest;
	}
	mergedRequest.MergeFrom(Request);

	if (Request.SubmitPolicy == EVdjmRecorderOptionSubmitPolicy::EQueueOnly)
	{
		PendingOptionRequest = mergedRequest;
		bHasPendingOptionRequest = PendingOptionRequest.HasAnyMessage();
		return true;
	}

	if (!EnsureBridge())
	{
		PendingOptionRequest = mergedRequest;
		bHasPendingOptionRequest = PendingOptionRequest.HasAnyMessage();
		return true;
	}

	AVdjmRecordBridgeActor* bridge = WeakBridgeActor.Get();
	if (bridge == nullptr || bridge->IsRecording() || !bridge->DbcValidConfigureDataAsset())
	{
		PendingOptionRequest = mergedRequest;
		bHasPendingOptionRequest = PendingOptionRequest.HasAnyMessage();
		return true;
	}

	if (!ApplyOptionRequestToBridge(mergedRequest, OutErrorReason))
	{
		return false;
	}

	ClearPendingOptionRequest();
	return true;
}

bool UVdjmRecorderController::ProcessPendingOptionRequests(FString& OutErrorReason)
{
	OutErrorReason.Reset();
	if (!bHasPendingOptionRequest || !PendingOptionRequest.HasAnyMessage())
	{
		ClearPendingOptionRequest();
		return true;
	}

	if (!EnsureBridge())
	{
		return false;
	}

	AVdjmRecordBridgeActor* bridge = WeakBridgeActor.Get();
	if (bridge == nullptr || bridge->IsRecording() || !bridge->DbcValidConfigureDataAsset())
	{
		return false;
	}

	if (!ApplyOptionRequestToBridge(PendingOptionRequest, OutErrorReason))
	{
		ClearPendingOptionRequest();
		return false;
	}

	ClearPendingOptionRequest();
	return true;
}

bool UVdjmRecorderController::StartRecording()
{
	if (IsPostProcessingMedia())
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("UVdjmRecorderController::StartRecording - Cannot start while media postprocess is running."));
		return false;
	}

	if (!EnsureEventManager())
	{
		return false;
	}

	const bool bStarted = EventManager->StartRecordingByManager();
	if (bStarted)
	{
		ClearLatestArtifact();
	}

	return bStarted;
}

void UVdjmRecorderController::StopRecording()
{
	if (!EnsureEventManager())
	{
		return;
	}

	EventManager->StopRecordingByManager();
}

FVdjmRecorderControllerStatusSnapshot UVdjmRecorderController::GetControllerStatusSnapshot() const
{
	FVdjmRecorderControllerStatusSnapshot statusSnapshot;
	statusSnapshot.bHasEventManager = EventManager != nullptr;
	statusSnapshot.bHasMetadataStore = MetadataStore != nullptr;
	statusSnapshot.bHasPendingOptionRequest = bHasPendingOptionRequest;
	if (MetadataStore != nullptr)
	{
		const FVdjmRecordMediaPostProcessSnapshot postProcessSnapshot = MetadataStore->GetMediaPostProcessSnapshot();
		statusSnapshot.bIsPostProcessingMedia = postProcessSnapshot.bIsPostProcessingMedia;
		statusSnapshot.ActiveMediaPublishJobCount = postProcessSnapshot.ActiveMediaPublishJobCount;
		statusSnapshot.LatestMediaPublishStatus = postProcessSnapshot.LastMediaPublishStatus;
		statusSnapshot.LatestPublishedContentUri = postProcessSnapshot.LastPublishedContentUri;
	}

	const AVdjmRecordBridgeActor* bridge = GetBridgeActor();
	statusSnapshot.bHasBridgeActor = bridge != nullptr;
	statusSnapshot.bIsRecording = bridge != nullptr && bridge->IsRecording();
	statusSnapshot.bIsFinalizingRecording = bridge != nullptr && bridge->IsFinalizingRecording();

	statusSnapshot.bHasLatestArtifact = IsValid(LatestArtifact);
	if (IsValid(LatestArtifact))
	{
		statusSnapshot.bIsLatestArtifactValid = LatestArtifact->IsValidArtifact();
		statusSnapshot.bHasLatestMetadata = LatestArtifact->HasMetadata();
		statusSnapshot.LatestMediaPublishStatus = LatestArtifact->GetMediaPublishStatus();
		statusSnapshot.LatestOutputFilePath = LatestArtifact->GetOutputFilePath();
		statusSnapshot.LatestMetadataFilePath = LatestArtifact->GetMetadataFilePath();
		statusSnapshot.LatestPublishedContentUri = LatestArtifact->GetPublishedContentUri();
	}

	if (statusSnapshot.bIsFinalizingRecording)
	{
		statusSnapshot.StatusText = TEXT("Recorder is finalizing the current recording.");
	}
	else if (statusSnapshot.bIsPostProcessingMedia)
	{
		statusSnapshot.StatusText = TEXT("Recorder is post-processing media.");
	}
	else if (statusSnapshot.bIsRecording)
	{
		statusSnapshot.StatusText = TEXT("Recorder is recording.");
	}
	else if (statusSnapshot.bHasLatestArtifact)
	{
		statusSnapshot.StatusText = statusSnapshot.bIsLatestArtifactValid
			? TEXT("Recorder has a valid latest artifact.")
			: TEXT("Recorder has an invalid latest artifact.");
	}
	else
	{
		statusSnapshot.StatusText = TEXT("Recorder is idle.");
	}

	return statusSnapshot;
}

bool UVdjmRecorderController::ValidateControllerState(FString& outStatusText) const
{
	const FVdjmRecorderControllerStatusSnapshot statusSnapshot = GetControllerStatusSnapshot();
	outStatusText = statusSnapshot.StatusText;

	if (not statusSnapshot.bHasEventManager)
	{
		outStatusText = TEXT("RecorderController has no EventManager.");
		return false;
	}

	if (not statusSnapshot.bHasBridgeActor)
	{
		outStatusText = TEXT("RecorderController has no BridgeActor.");
		return false;
	}

	if (not statusSnapshot.bHasMetadataStore)
	{
		outStatusText = TEXT("RecorderController has no MetadataStore.");
		return false;
	}

	if (statusSnapshot.bHasLatestArtifact && not statusSnapshot.bIsLatestArtifactValid)
	{
		outStatusText = TEXT("Latest record artifact is invalid.");
		return false;
	}

	if (statusSnapshot.bIsPostProcessingMedia)
	{
		outStatusText = TEXT("Recorder media postprocess is still running.");
		return false;
	}

	if (statusSnapshot.LatestMediaPublishStatus == EVdjmRecordMediaPublishStatus::EFailed)
	{
		outStatusText = TEXT("Latest media publish failed.");
		return false;
	}

	return true;
}

bool UVdjmRecorderController::IsRecording() const
{
	const AVdjmRecordBridgeActor* bridge = GetBridgeActor();
	return bridge != nullptr && bridge->IsRecording();
}

bool UVdjmRecorderController::IsFinalizingRecording() const
{
	const AVdjmRecordBridgeActor* bridge = GetBridgeActor();
	return bridge != nullptr && bridge->IsFinalizingRecording();
}

bool UVdjmRecorderController::IsControllerBusy() const
{
	return IsRecording() || IsFinalizingRecording() || IsPostProcessingMedia() || bHasPendingOptionRequest;
}

bool UVdjmRecorderController::IsPostProcessingMedia() const
{
	return MetadataStore != nullptr && MetadataStore->IsPostProcessingMedia();
}

int32 UVdjmRecorderController::GetActiveMediaPublishJobCount() const
{
	return MetadataStore != nullptr ? MetadataStore->GetActiveMediaPublishJobCount() : 0;
}

UVdjmRecordArtifact* UVdjmRecorderController::GetLatestArtifact() const
{
	return LatestArtifact;
}

UVdjmRecordMetadataStore* UVdjmRecorderController::GetMetadataStore() const
{
	return MetadataStore;
}

EVdjmRecordMediaPublishStatus UVdjmRecorderController::GetLatestMediaPublishStatus() const
{
	if (IsValid(LatestArtifact))
	{
		return LatestArtifact->GetMediaPublishStatus();
	}

	return MetadataStore != nullptr
		? MetadataStore->GetLastMediaPublishStatus()
		: EVdjmRecordMediaPublishStatus::ENotStarted;
}

FString UVdjmRecorderController::GetLatestPublishedContentUri() const
{
	if (IsValid(LatestArtifact))
	{
		return LatestArtifact->GetPublishedContentUri();
	}

	return MetadataStore != nullptr ? MetadataStore->GetLastPublishedContentUri() : FString();
}

FString UVdjmRecorderController::GetLatestMediaPublishErrorReason() const
{
	if (IsValid(LatestArtifact))
	{
		return LatestArtifact->GetMediaPublishErrorReason();
	}

	return MetadataStore != nullptr ? MetadataStore->GetLastMediaPublishErrorReason() : FString();
}

void UVdjmRecorderController::ClearLatestArtifact()
{
	LatestArtifact = nullptr;
}

void UVdjmRecorderController::SetLatestArtifact(UVdjmRecordArtifact* artifact)
{
	LatestArtifact = artifact;
}

void UVdjmRecorderController::Tick(float DeltaTime)
{
	//UE_UNUSED(DeltaTime);
	(void)DeltaTime;
	
	if (!bHasPendingOptionRequest)
	{
		return;
	}

	FString errorReason;
	ProcessPendingOptionRequests(errorReason);
}

bool UVdjmRecorderController::IsTickable() const
{
	return !IsTemplate() && bHasPendingOptionRequest;
}

UWorld* UVdjmRecorderController::GetTickableGameObjectWorld() const
{
	return CachedWorld.Get();
}

TStatId UVdjmRecorderController::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVdjmRecorderController, STATGROUP_Tickables);
}

AVdjmRecordBridgeActor* UVdjmRecorderController::GetBridgeActor() const
{
	if (WeakBridgeActor.IsValid())
	{
		return WeakBridgeActor.Get();
	}

	if (EventManager != nullptr)
	{
		return EventManager->GetBoundBridge();
	}

	return nullptr;
}

UVdjmRecordEnvDataAsset* UVdjmRecorderController::GetResolvedDataAsset() const
{
	return WeakDataAsset.Get();
}

UVdjmRecorderStateObserver* UVdjmRecorderController::GetStateObserver() const
{
	return StateObserver;
}

UVdjmRecordEventManager* UVdjmRecorderController::GetEventManager() const
{
	return EventManager;
}

bool UVdjmRecorderController::HasPendingOptionRequest() const
{
	return bHasPendingOptionRequest && PendingOptionRequest.HasAnyMessage();
}

UWorld* UVdjmRecorderController::GetWorld() const
{
	return CachedWorld.Get();
}

bool UVdjmRecorderController::ApplyOptionRequestToBridge(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason)
{
	OutErrorReason.Reset();

	if (!EnsureBridge())
	{
		OutErrorReason = TEXT("Recorder bridge actor is not available.");
		return false;
	}

	AVdjmRecordBridgeActor* bridge = WeakBridgeActor.Get();
	if (bridge == nullptr)
	{
		OutErrorReason = TEXT("Recorder bridge actor is null.");
		return false;
	}

	bool bAnyChangeApplied = false;
	switch (Request.QualityTier.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedQualityTier(Request.QualityTier.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedQualityTier();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.FileName.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetCurrentFileName(Request.FileName.Value.TrimStartAndEnd());
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearCurrentFileName();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.FrameRate.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedFrameRate(Request.FrameRate.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedFrameRate();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.Bitrate.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedBitrate(Request.Bitrate.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedBitrate();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.Resolution.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedResolution(Request.Resolution.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedResolution();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.ResolutionFitToDisplay.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedResolutionFitToDisplay(Request.ResolutionFitToDisplay.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedResolutionFitToDisplay();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.KeyframeInterval.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedKeyframeInterval(Request.KeyframeInterval.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedKeyframeInterval();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.MaxRecordDurationSeconds.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedMaxRecordDurationSeconds(Request.MaxRecordDurationSeconds.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedMaxRecordDurationSeconds();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.OutputFilePath.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedOutputFilePath(Request.OutputFilePath.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedOutputFilePath();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.SessionId.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedSessionId(Request.SessionId.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedSessionId();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	switch (Request.OverwriteExists.Action)
	{
	case EVdjmRecorderOptionValueAction::ESet:
		bridge->SetRequestedOverwriteExists(Request.OverwriteExists.Value);
		bAnyChangeApplied = true;
		break;
	case EVdjmRecorderOptionValueAction::EClear:
		bridge->ClearRequestedOverwriteExists();
		bAnyChangeApplied = true;
		break;
	default:
		break;
	}

	if (!bAnyChangeApplied)
	{
		OutErrorReason = TEXT("Option request is empty.");
		return false;
	}

	return bridge->RefreshResolvedOptionsFromRequest(OutErrorReason);
}

void UVdjmRecorderController::ResetOptionHistory()
{
	UndoHistory.Reset();
	RedoHistory.Reset();
}

void UVdjmRecorderController::StageAppliedRequestForUndo(const FVdjmRecorderOptionRequest& AppliedRequest)
{
	//UE_UNUSED(AppliedRequest);
	//	TODO(20260422): 추후에 추가할 내용임.
}

void UVdjmRecorderController::ClearPendingOptionRequest()
{
	PendingOptionRequest.Reset();
	bHasPendingOptionRequest = false;
}

bool UVdjmRecorderController::EnsureEventManager()
{
	if (EventManager != nullptr)
	{
		return true;
	}

	if (const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this))
	{
		if (UVdjmRecordEventManager* existingEventManager = Cast<UVdjmRecordEventManager>(
			worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetEventManagerContextKey())))
		{
			EventManager = existingEventManager;
			EnsureStateObserver();
			return true;
		}
	}

	EventManager = UVdjmRecordEventManager::CreateEventManager(this);
	if (EventManager == nullptr)
	{
		return false;
	}

	EnsureStateObserver();
	return true;
}

bool UVdjmRecorderController::EnsureBridge()
{
	if (WeakBridgeActor.IsValid())
	{
		return true;
	}

	if (!EnsureEventManager())
	{
		return false;
	}

	if (AVdjmRecordBridgeActor* BoundBridge = EventManager->GetBoundBridge())
	{
		WeakBridgeActor = BoundBridge;
		EnsureStateObserver();
		return true;
	}

	UWorld* world = CachedWorld.Get();
	if (world == nullptr)
	{
		return false;
	}

	WeakBridgeActor = AVdjmRecordBridgeActor::TryGetRecordBridgeActor(world);
	if (WeakBridgeActor.IsValid())
	{
		EventManager->BindBridge(WeakBridgeActor.Get());
	}
	EnsureStateObserver();
	return WeakBridgeActor.IsValid();
}

void UVdjmRecorderController::EnsureStateObserver()
{
	if (StateObserver == nullptr)
	{
		StateObserver = NewObject<UVdjmRecorderStateObserver>(this);
		if (StateObserver != nullptr)
		{
			StateObserver->InitializeObserver(this);
		}
	}

	if (StateObserver != nullptr && EventManager != nullptr)
	{
		StateObserver->BindEventManager(EventManager);
	}
}

bool UVdjmRecorderController::EnsureMetadataStore()
{
	if (MetadataStore != nullptr)
	{
		return true;
	}

	MetadataStore = UVdjmRecordMetadataStore::FindOrCreateMetadataStore(this);
	return MetadataStore != nullptr;
}

bool UVdjmRecorderController::ValidateRequest(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason) const
{
	if (!Request.HasAnyMessage())
	{
		OutErrorReason = TEXT("Option request is empty.");
		return false;
	}

	if (Request.QualityTier.Action == EVdjmRecorderOptionValueAction::ESet &&
		Request.QualityTier.Value == EVdjmRecordQualityTiers::EUndefined)
	{
		OutErrorReason = TEXT("Quality tier cannot be Undefined.");
		return false;
	}

	if (Request.FileName.Action == EVdjmRecorderOptionValueAction::ESet &&
		Request.FileName.Value.TrimStartAndEnd().IsEmpty())
	{
		OutErrorReason = TEXT("File name cannot be empty.");
		return false;
	}

	if (Request.FrameRate.Action == EVdjmRecorderOptionValueAction::ESet &&
		Request.FrameRate.Value <= 0)
	{
		OutErrorReason = TEXT("Frame rate must be greater than zero.");
		return false;
	}

	if (Request.Bitrate.Action == EVdjmRecorderOptionValueAction::ESet)
	{
		int32 safeBitrate = 0;
		if (!VdjmRecordUtils::Validations::DbcValidateBitrate(
			Request.Bitrate.Value,
			safeBitrate,
			TEXT("UVdjmRecorderController::ValidateRequest")))
		{
			OutErrorReason = TEXT("Bitrate is outside the supported safe range.");
			return false;
		}
	}

	if (Request.Resolution.Action == EVdjmRecorderOptionValueAction::ESet &&
		(Request.Resolution.Value.X <= 0 || Request.Resolution.Value.Y <= 0))
	{
		OutErrorReason = TEXT("Resolution must be greater than zero.");
		return false;
	}

	if (Request.KeyframeInterval.Action == EVdjmRecorderOptionValueAction::ESet &&
		Request.KeyframeInterval.Value < 0)
	{
		OutErrorReason = TEXT("Keyframe interval must be zero or greater.");
		return false;
	}

	if (Request.MaxRecordDurationSeconds.Action == EVdjmRecorderOptionValueAction::ESet &&
		Request.MaxRecordDurationSeconds.Value <= 0.0f)
	{
		OutErrorReason = TEXT("Max record duration must be greater than zero.");
		return false;
	}

	if (Request.OutputFilePath.Action == EVdjmRecorderOptionValueAction::ESet &&
		Request.OutputFilePath.Value.TrimStartAndEnd().IsEmpty())
	{
		OutErrorReason = TEXT("Output file path cannot be empty.");
		return false;
	}

	if (Request.SessionId.Action == EVdjmRecorderOptionValueAction::ESet &&
		Request.SessionId.Value.TrimStartAndEnd().IsEmpty())
	{
		OutErrorReason = TEXT("Session id cannot be empty.");
		return false;
	}

	return true;
}
