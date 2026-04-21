#include "VdjmRecorderController.h"

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
	if (!EnsureBridge())
	{
		return false;
	}

	WeakDataAsset = AVdjmRecordBridgeActor::TryGetRecordEnvConfigure();
	EnsureStateObserver();
	return true;
}

bool UVdjmRecorderController::ApplyOptionRequest(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason)
{
	OutErrorReason.Reset();
	if (!EnsureBridge())
	{
		OutErrorReason = TEXT("Recorder bridge actor is not available.");
		return false;
	}

	if (!ValidateRequest(Request, OutErrorReason))
	{
		return false;
	}

	AVdjmRecordBridgeActor* bridge = WeakBridgeActor.Get();
	if (bridge == nullptr)
	{
		OutErrorReason = TEXT("Recorder bridge actor is null.");
		return false;
	}

	if (Request.bOverrideQualityTier)
	{
		bridge->SetRequestedQualityTier(Request.QualityTier);
	}

	if (Request.bOverrideFileName)
	{
		bridge->SetCurrentFileName(Request.FileName);
	}

	if ((Request.bOverrideQualityTier || Request.bOverrideFileName))
	{
		if (!bridge->RefreshResolvedOptionsFromRequest(OutErrorReason))
		{
			return false;
		}
	}

	return true;
}

bool UVdjmRecorderController::StartRecording()
{
	if (!EnsureBridge())
	{
		return false;
	}

	if (AVdjmRecordBridgeActor* bridge = WeakBridgeActor.Get())
	{
		bridge->StartRecording();
		return true;
	}

	return false;
}

void UVdjmRecorderController::StopRecording()
{
	if (!EnsureBridge())
	{
		return;
	}

	if (AVdjmRecordBridgeActor* bridge = WeakBridgeActor.Get())
	{
		bridge->StopRecording();
	}
}

AVdjmRecordBridgeActor* UVdjmRecorderController::GetBridgeActor() const
{
	return WeakBridgeActor.Get();
}

UVdjmRecordEnvDataAsset* UVdjmRecorderController::GetResolvedDataAsset() const
{
	return WeakDataAsset.Get();
}

UVdjmRecorderStateObserver* UVdjmRecorderController::GetStateObserver() const
{
	return StateObserver;
}

UWorld* UVdjmRecorderController::GetWorld() const
{
	return CachedWorld.Get();
}

bool UVdjmRecorderController::EnsureBridge()
{
	if (WeakBridgeActor.IsValid())
	{
		return true;
	}

	UWorld* world = CachedWorld.Get();
	if (world == nullptr)
	{
		return false;
	}

	WeakBridgeActor = AVdjmRecordBridgeActor::TryGetRecordBridgeActor(world);
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

	if (StateObserver != nullptr && WeakBridgeActor.IsValid())
	{
		StateObserver->BindBridge(WeakBridgeActor.Get());
	}
}

bool UVdjmRecorderController::ValidateRequest(const FVdjmRecorderOptionRequest& Request, FString& OutErrorReason) const
{
	if (!Request.bOverrideQualityTier && !Request.bOverrideFileName)
	{
		OutErrorReason = TEXT("Option request is empty.");
		return false;
	}

	if (Request.bOverrideQualityTier && Request.QualityTier == EVdjmRecordQualityTiers::EUndefined)
	{
		OutErrorReason = TEXT("Quality tier cannot be Undefined.");
		return false;
	}

	if (Request.bOverrideFileName && Request.FileName.TrimStartAndEnd().IsEmpty())
	{
		OutErrorReason = TEXT("File name cannot be empty.");
		return false;
	}

	return true;
}
