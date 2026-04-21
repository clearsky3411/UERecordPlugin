#include "VdjmRecordEventBase.h"

#include "VdjmRecordBridgeActor.h"
#include "VdjmRecordEventManager.h"

bool UVdjmRecordEventBase::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, FString& OutErrorReason)
{
	OutErrorReason.Reset();
	return (EventManager != nullptr);
}

bool UVdjmRecordEventSpawnBridgeActor::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, FString& OutErrorReason)
{
	OutErrorReason.Reset();
	if (EventManager == nullptr)
	{
		OutErrorReason = TEXT("EventManager is null.");
		return false;
	}

	UWorld* World = EventManager->GetManagerWorld();
	if (World == nullptr)
	{
		OutErrorReason = TEXT("World is null.");
		return false;
	}

	if (bReuseExistingBridgeIfFound)
	{
		if (AVdjmRecordBridgeActor* ExistingBridge = AVdjmRecordBridgeActor::TryGetRecordBridgeActor(World))
		{
			if (bBindSpawnedBridgeToManager)
			{
				EventManager->BindBridge(ExistingBridge);
			}
			return true;
		}
	}

	UClass* SpawnClass = AVdjmRecordBridgeActor::StaticClass();
	if (UClass* OverrideClass = BridgeActorClass.LoadSynchronous())
	{
		SpawnClass = OverrideClass;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVdjmRecordBridgeActor* SpawnedBridge = World->SpawnActor<AVdjmRecordBridgeActor>(SpawnClass, FTransform::Identity, SpawnParams);
	if (SpawnedBridge == nullptr)
	{
		OutErrorReason = TEXT("Failed to spawn AVdjmRecordBridgeActor.");
		return false;
	}

	if (bBindSpawnedBridgeToManager)
	{
		EventManager->BindBridge(SpawnedBridge);
	}

	return true;
}

bool UVdjmRecordEventSetEnvDataAssetPath::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, FString& OutErrorReason)
{
	OutErrorReason.Reset();
	if (RecordEnvDataAssetPath.IsValid())
	{
		AVdjmRecordBridgeActor::SetRecordEnvConfigureAssetPath(RecordEnvDataAssetPath);
		return true;
	}

	if (bResetToDefaultWhenPathIsInvalid)
	{
		AVdjmRecordBridgeActor::ResetRecordEnvConfigureAssetPath();
		return true;
	}

	OutErrorReason = TEXT("RecordEnvDataAssetPath is invalid.");
	return false;
}
