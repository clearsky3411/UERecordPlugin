#include "VdjmRecorderController.h"

#include "VdjmRecordBridgeActor.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecorderCore.h"
#include "VdjmRecorderStateObserver.h"

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

bool UVdjmRecorderController::InitializeController()
{
	if (!EnsureEventManager())
	{
		return false;
	}

	EnsureBridge();
	WeakDataAsset = AVdjmRecordBridgeActor::TryGetRecordEnvConfigure();
	EnsureStateObserver();
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
	if (!EnsureEventManager())
	{
		return false;
	}

	return EventManager->StartRecordingByManager();
}

void UVdjmRecorderController::StopRecording()
{
	if (!EnsureEventManager())
	{
		return;
	}

	EventManager->StopRecordingByManager();
}

void UVdjmRecorderController::Tick(float DeltaTime)
{
	UE_UNUSED(DeltaTime);

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

	if (!bAnyChangeApplied)
	{
		OutErrorReason = TEXT("Option request is empty.");
		return false;
	}

	return bridge->RefreshResolvedOptionsFromRequest(OutErrorReason);
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

	return true;
}
