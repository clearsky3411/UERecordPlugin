#include "VdjmVcardTileImageEventNodes.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "VdjmRecordBridgeActor.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecorderWorldContextSubsystem.h"
#include "VdjmVcard.h"

namespace
{
	FVdjmRecordEventResult MakeTileImageBrokerNodeResult(bool bSuccess, bool bSucceedIfMissing)
	{
		return UVdjmRecordEventBase::MakeResult(
			(bSuccess || bSucceedIfMissing) ? EVdjmRecordEventResultType::ESuccess : EVdjmRecordEventResultType::EFailure,
			INDEX_NONE,
			NAME_None,
			0.0f);
	}

	AVcardTileImageLoadBroker* FindBrokerInRuntimeSlot(UVdjmRecordEventManager* eventManager, FName runtimeSlotKey)
	{
		if (eventManager == nullptr || runtimeSlotKey.IsNone())
		{
			return nullptr;
		}

		return Cast<AVcardTileImageLoadBroker>(eventManager->FindRuntimeObjectSlot(runtimeSlotKey));
	}

	AVcardTileImageLoadBroker* FindBrokerInContext(UObject* worldContextObject, FName contextKey)
	{
		if (worldContextObject == nullptr || contextKey.IsNone())
		{
			return nullptr;
		}

		const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject);
		if (worldContextSubsystem == nullptr)
		{
			return nullptr;
		}

		return Cast<AVcardTileImageLoadBroker>(worldContextSubsystem->FindContextObject(contextKey));
	}

	AVcardTileImageLoadBroker* FindBrokerInWorld(UWorld* world)
	{
		if (world == nullptr)
		{
			return nullptr;
		}

		for (TActorIterator<AVcardTileImageLoadBroker> actorIt(world); actorIt; ++actorIt)
		{
			if (IsValid(*actorIt))
			{
				return *actorIt;
			}
		}

		return nullptr;
	}

	AVcardTileImageLoadBroker* SpawnBroker(UWorld* world)
	{
		if (world == nullptr)
		{
			return nullptr;
		}

		FActorSpawnParameters spawnParameters;
		spawnParameters.Name = MakeUniqueObjectName(world, AVcardTileImageLoadBroker::StaticClass(), TEXT("VcardTileImageLoadBroker"));
		spawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return world->SpawnActor<AVcardTileImageLoadBroker>(AVcardTileImageLoadBroker::StaticClass(), FTransform::Identity, spawnParameters);
	}
}

FVdjmRecordEventResult UVcardEventEnsureTileImageLoadBrokerNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	UObject* worldContextObject = EventManager != nullptr ? Cast<UObject>(EventManager) : Cast<UObject>(BridgeActor);
	UWorld* world = worldContextObject != nullptr ? worldContextObject->GetWorld() : nullptr;

	if (world == nullptr)
	{
		if (bLogResult)
		{
			UE_LOG(LogVdjmVcard, Warning, TEXT("TileImageBrokerNode failed: world is missing."));
		}
		return MakeTileImageBrokerNodeResult(false, bSucceedIfMissing);
	}

	AVcardTileImageLoadBroker* broker = nullptr;
	if (bReuseExistingBroker)
	{
		broker = FindBrokerInRuntimeSlot(EventManager, RuntimeSlotKey);
		if (!IsValid(broker))
		{
			broker = FindBrokerInContext(worldContextObject, ContextKey);
		}
		if (!IsValid(broker))
		{
			broker = FindBrokerInWorld(world);
		}
	}

	if (!IsValid(broker))
	{
		broker = SpawnBroker(world);
	}

	if (!IsValid(broker))
	{
		if (bLogResult)
		{
			UE_LOG(LogVdjmVcard, Warning, TEXT("TileImageBrokerNode failed: broker spawn failed."));
		}
		return MakeTileImageBrokerNodeResult(false, bSucceedIfMissing);
	}

	broker->MaxActiveJobs = FMath::Max(1, MaxActiveJobs);
	broker->MaxJobsPerStep = FMath::Max(1, MaxJobsPerStep);
	broker->StepIntervalSeconds = FMath::Max(0.001f, StepIntervalSeconds);
	broker->ThumbnailSize = FMath::Max(1, ThumbnailSize);
	broker->MaxSourceTextureSize = FMath::Max(1, MaxSourceTextureSize);

	FString storageErrorReason;
	const bool bStorageConfigured = broker->ConfigureStoragePath(
		StorageMode,
		RelativeFolder,
		CustomAbsolutePath,
		bCreateIfMissing,
		storageErrorReason);

	if (!bStorageConfigured)
	{
		if (bLogResult)
		{
			UE_LOG(LogVdjmVcard, Warning, TEXT("TileImageBrokerNode storage failed: %s"), *storageErrorReason);
		}
		return MakeTileImageBrokerNodeResult(false, bSucceedIfMissing);
	}

	if (bStoreRuntimeSlot && EventManager != nullptr && !RuntimeSlotKey.IsNone())
	{
		EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, broker);
	}

	if (bRegisterContext && !ContextKey.IsNone())
	{
		if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject))
		{
			worldContextSubsystem->RegisterWeakObjectContext(ContextKey, broker, AVcardTileImageLoadBroker::StaticClass());
		}
	}

	if (bLogResult)
	{
		UE_LOG(
			LogVdjmVcard,
			Log,
			TEXT("TileImageBrokerNode ensured. Broker=%s RuntimeSlotKey=%s ContextKey=%s Storage=%s MaxActive=%d MaxPerStep=%d Step=%.3f Thumbnail=%d"),
			*GetNameSafe(broker),
			*RuntimeSlotKey.ToString(),
			*ContextKey.ToString(),
			*broker->GetStorageRootPath(),
			broker->MaxActiveJobs,
			broker->MaxJobsPerStep,
			broker->StepIntervalSeconds,
			broker->ThumbnailSize);
	}

	return MakeTileImageBrokerNodeResult(true, bSucceedIfMissing);
}
