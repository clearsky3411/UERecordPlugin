#include "VdjmRecordEventManager.h"

#include "VdjmRecordBridgeActor.h"

UVdjmRecordEventManager* UVdjmRecordEventManager::CreateEventManager(UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}

	UVdjmRecordEventManager* Manager = NewObject<UVdjmRecordEventManager>(WorldContextObject);
	if (Manager == nullptr)
	{
		return nullptr;
	}

	if (!Manager->InitializeManager(WorldContextObject))
	{
		return nullptr;
	}

	return Manager;
}

bool UVdjmRecordEventManager::InitializeManager(UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return false;
	}

	CachedWorld = WorldContextObject->GetWorld();
	if (!CachedWorld.IsValid())
	{
		return false;
	}

	if (AVdjmRecordBridgeActor* BridgeActor = AVdjmRecordBridgeActor::TryGetRecordBridgeActor(CachedWorld.Get()))
	{
		return BindBridge(BridgeActor);
	}

	return true;
}

bool UVdjmRecordEventManager::BindBridge(AVdjmRecordBridgeActor* InBridgeActor)
{
	if (InBridgeActor == nullptr)
	{
		return false;
	}

	if (WeakBridgeActor.Get() == InBridgeActor)
	{
		return true;
	}

	if (AVdjmRecordBridgeActor* ExistingBridge = WeakBridgeActor.Get())
	{
		ExistingBridge->OnChainInitEvent.RemoveDynamic(this, &UVdjmRecordEventManager::HandleBridgeChainEvent);
	}

	WeakBridgeActor = InBridgeActor;
	InBridgeActor->OnChainInitEvent.AddDynamic(this, &UVdjmRecordEventManager::HandleBridgeChainEvent);
	return true;
}

AVdjmRecordBridgeActor* UVdjmRecordEventManager::GetBoundBridge() const
{
	return WeakBridgeActor.Get();
}

bool UVdjmRecordEventManager::StartRecordingByManager()
{
	if (!WeakBridgeActor.IsValid() && CachedWorld.IsValid())
	{
		BindBridge(AVdjmRecordBridgeActor::TryGetRecordBridgeActor(CachedWorld.Get()));
	}

	if (AVdjmRecordBridgeActor* BridgeActor = WeakBridgeActor.Get())
	{
		BridgeActor->StartRecording();
		return true;
	}

	return false;
}

void UVdjmRecordEventManager::StopRecordingByManager()
{
	if (AVdjmRecordBridgeActor* BridgeActor = WeakBridgeActor.Get())
	{
		BridgeActor->StopRecording();
	}
}

UWorld* UVdjmRecordEventManager::GetWorld() const
{
	return CachedWorld.Get();
}

void UVdjmRecordEventManager::HandleBridgeChainEvent(
	AVdjmRecordBridgeActor* InBridgeActor,
	EVdjmRecordBridgeInitStep PrevStep,
	EVdjmRecordBridgeInitStep CurrentStep)
{
	OnManagerObservedChainEvent.Broadcast(InBridgeActor, PrevStep, CurrentStep);
}
