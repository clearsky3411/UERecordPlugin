#include "VdjmRecorderStateObserver.h"

#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecorderWorldContextSubsystem.h"

UVdjmRecorderStateObserver* UVdjmRecorderStateObserver::CreateObserver(UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}

	UVdjmRecorderStateObserver* Observer = NewObject<UVdjmRecorderStateObserver>(WorldContextObject);
	if (Observer == nullptr)
	{
		return nullptr;
	}

	return Observer->InitializeObserver(WorldContextObject) ? Observer : nullptr;
}

bool UVdjmRecorderStateObserver::InitializeObserver(UObject* WorldContextObject)
{
	if (UVdjmRecorderWorldContextSubsystem* WorldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(WorldContextObject))
	{
		WorldContextSubsystem->RegisterWeakObjectContext(
			UVdjmRecorderWorldContextSubsystem::GetStateObserverContextKey(),
			this,
			StaticClass());
	}

	return WorldContextObject != nullptr;
}

bool UVdjmRecorderStateObserver::BindEventManager(UVdjmRecordEventManager* InEventManager)
{
	if (InEventManager == nullptr)
	{
		return false;
	}

	if (WeakEventManager.Get() == InEventManager)
	{
		return true;
	}

	UnbindEventManager();
	WeakEventManager = InEventManager;
	CurrentState = InEventManager->GetSessionState();
	LastTransitionSeconds = InEventManager->GetLastSessionTransitionSeconds();
	LastError = InEventManager->GetLastSessionError();
	InEventManager->OnSessionStateChanged.AddDynamic(this, &UVdjmRecorderStateObserver::HandleManagerSessionStateChanged);
	return true;
}

void UVdjmRecorderStateObserver::UnbindEventManager()
{
	if (UVdjmRecordEventManager* EventManager = WeakEventManager.Get())
	{
		EventManager->OnSessionStateChanged.RemoveDynamic(this, &UVdjmRecorderStateObserver::HandleManagerSessionStateChanged);
	}

	WeakEventManager = nullptr;
}

void UVdjmRecorderStateObserver::HandleManagerSessionStateChanged(
	UVdjmRecordEventManager* InEventManager,
	EVdjmRecorderSessionState PreviousState,
	EVdjmRecorderSessionState NewState,
	double TransitionSeconds)
{
	CurrentState = NewState;
	LastTransitionSeconds = TransitionSeconds;
	LastError = InEventManager ? InEventManager->GetLastSessionError() : FString();
	OnObservedStateChanged.Broadcast(PreviousState, CurrentState, LastTransitionSeconds);
}
