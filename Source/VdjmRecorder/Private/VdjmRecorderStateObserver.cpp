#include "VdjmRecorderStateObserver.h"

#include "VdjmRecordBridgeActor.h"
#include "VdjmRecorderCore.h"

UVdjmRecorderStateObserver* UVdjmRecorderStateObserver::CreateObserver(UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	UVdjmRecorderStateObserver* Observer = NewObject<UVdjmRecorderStateObserver>(WorldContextObject);
	if (Observer == nullptr)
	{
		return nullptr;
	}

	Observer->InitializeObserver(WorldContextObject);
	return Observer;
}

bool UVdjmRecorderStateObserver::InitializeObserver(UObject* WorldContextObject)
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

bool UVdjmRecorderStateObserver::BindBridge(AVdjmRecordBridgeActor* InBridgeActor)
{
	if (InBridgeActor == nullptr)
	{
		return false;
	}

	if (WeakBridgeActor.Get() == InBridgeActor)
	{
		return true;
	}

	UnbindBridge();
	WeakBridgeActor = InBridgeActor;
	LastInitStep = InBridgeActor->GetCurrentInitStep();

	InBridgeActor->OnInitCompleteEvent.AddDynamic(this, &UVdjmRecorderStateObserver::HandleInitComplete);
	InBridgeActor->OnInitErrorEndEvent.AddDynamic(this, &UVdjmRecorderStateObserver::HandleInitErrorEnd);
	InBridgeActor->OnRecordStarted.AddDynamic(this, &UVdjmRecorderStateObserver::HandleRecordStarted);
	InBridgeActor->OnRecordStopped.AddDynamic(this, &UVdjmRecorderStateObserver::HandleRecordStopped);
	InBridgeActor->OnChainInitEvent.AddDynamic(this, &UVdjmRecorderStateObserver::HandleChainInitChanged);

	ApplyStateByBridgeSnapshot();
	return true;
}

void UVdjmRecorderStateObserver::UnbindBridge()
{
	if (AVdjmRecordBridgeActor* BridgeActor = WeakBridgeActor.Get())
	{
		BridgeActor->OnInitCompleteEvent.RemoveDynamic(this, &UVdjmRecorderStateObserver::HandleInitComplete);
		BridgeActor->OnInitErrorEndEvent.RemoveDynamic(this, &UVdjmRecorderStateObserver::HandleInitErrorEnd);
		BridgeActor->OnRecordStarted.RemoveDynamic(this, &UVdjmRecorderStateObserver::HandleRecordStarted);
		BridgeActor->OnRecordStopped.RemoveDynamic(this, &UVdjmRecorderStateObserver::HandleRecordStopped);
		BridgeActor->OnChainInitEvent.RemoveDynamic(this, &UVdjmRecorderStateObserver::HandleChainInitChanged);
	}

	WeakBridgeActor = nullptr;
}

void UVdjmRecorderStateObserver::Tick(float DeltaTime)
{
	if (!WeakBridgeActor.IsValid())
	{
		if (CachedWorld.IsValid())
		{
			if (AVdjmRecordBridgeActor* BridgeActor = AVdjmRecordBridgeActor::TryGetRecordBridgeActor(CachedWorld.Get()))
			{
				BindBridge(BridgeActor);
			}
		}
		return;
	}

	ApplyStateByBridgeSnapshot();
}

TStatId UVdjmRecorderStateObserver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVdjmRecorderStateObserver, STATGROUP_Tickables);
}

bool UVdjmRecorderStateObserver::IsTickable() const
{
	return !IsTemplate();
}

UWorld* UVdjmRecorderStateObserver::GetWorld() const
{
	return CachedWorld.Get();
}

void UVdjmRecorderStateObserver::HandleInitComplete(AVdjmRecordBridgeActor* InBridgeActor)
{
	TransitionTo(EVdjmRecorderObservedState::EReady);
}

void UVdjmRecorderStateObserver::HandleInitErrorEnd(AVdjmRecordBridgeActor* InBridgeActor)
{
	TransitionTo(EVdjmRecorderObservedState::EError);
}

void UVdjmRecorderStateObserver::HandleRecordStarted(UVdjmRecordResource* InRecordResource)
{
	TransitionTo(EVdjmRecorderObservedState::ERunning);
}

void UVdjmRecorderStateObserver::HandleRecordStopped(UVdjmRecordResource* InRecordResource)
{
	TransitionTo(EVdjmRecorderObservedState::ETerminated);
}

void UVdjmRecorderStateObserver::HandleChainInitChanged(
	AVdjmRecordBridgeActor* InBridgeActor,
	EVdjmRecordBridgeInitStep PrevStep,
	EVdjmRecordBridgeInitStep CurrentStep)
{
	LastInitStep = CurrentStep;
	if (CurrentStep == EVdjmRecordBridgeInitStep::EInitError ||
		CurrentStep == EVdjmRecordBridgeInitStep::EInitErrorEnd)
	{
		TransitionTo(EVdjmRecorderObservedState::EError);
		return;
	}

	if (CurrentStep == EVdjmRecordBridgeInitStep::EComplete)
	{
		TransitionTo(EVdjmRecorderObservedState::EReady);
		return;
	}

	if (CurrentStep != EVdjmRecordBridgeInitStep::EInitializeStart)
	{
		TransitionTo(EVdjmRecorderObservedState::EWaiting);
	}
}

void UVdjmRecorderStateObserver::TransitionTo(EVdjmRecorderObservedState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	const EVdjmRecorderObservedState PreviousState = CurrentState;
	CurrentState = NewState;
	LastTransitionSeconds = FPlatformTime::Seconds();
	OnObservedStateChanged.Broadcast(PreviousState, CurrentState, LastInitStep, LastTransitionSeconds);
}

void UVdjmRecorderStateObserver::ApplyStateByBridgeSnapshot()
{
	AVdjmRecordBridgeActor* BridgeActor = WeakBridgeActor.Get();
	if (BridgeActor == nullptr)
	{
		return;
	}

	LastInitStep = BridgeActor->GetCurrentInitStep();

	if (BridgeActor->IsRecording())
	{
		TransitionTo(EVdjmRecorderObservedState::ERunning);
		return;
	}

	if (LastInitStep == EVdjmRecordBridgeInitStep::EInitError ||
		LastInitStep == EVdjmRecordBridgeInitStep::EInitErrorEnd)
	{
		TransitionTo(EVdjmRecorderObservedState::EError);
		return;
	}

	if (LastInitStep == EVdjmRecordBridgeInitStep::EComplete)
	{
		TransitionTo(EVdjmRecorderObservedState::EReady);
		return;
	}

	if (LastInitStep == EVdjmRecordBridgeInitStep::EInitializeStart)
	{
		TransitionTo(EVdjmRecorderObservedState::ENew);
		return;
	}

	TransitionTo(EVdjmRecorderObservedState::EWaiting);
}
