#include "VdjmRecordEventManager.h"

#include "VdjmRecordBridgeActor.h"
#include "VdjmRecordEventFlowDataAsset.h"
#include "VdjmRecordEventFlowRuntime.h"
#include "VdjmRecordEventNode.h"

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

bool UVdjmRecordEventManager::StartEventFlow(UVdjmRecordEventFlowDataAsset* InFlowAsset, bool bResetRuntimeStates)
{
	if (InFlowAsset == nullptr)
	{
		return false;
	}

	FString BuildError;
	UVdjmRecordEventFlowRuntime* NewFlowRuntime = UVdjmRecordEventFlowRuntime::CreateFlowRuntimeFromAsset(this, InFlowAsset, BuildError);
	if (NewFlowRuntime == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartEventFlow - Failed to build runtime from asset: %s"), *BuildError);
		return false;
	}

	if (!StartEventFlowRuntime(NewFlowRuntime, bResetRuntimeStates))
	{
		UE_LOG(LogTemp, Warning, TEXT("StartEventFlow - Failed to start runtime flow."));
		return false;
	}

	return true;
}

bool UVdjmRecordEventManager::StartEventFlowRuntime(UVdjmRecordEventFlowRuntime* InFlowRuntime, bool bResetRuntimeStates)
{
	if (InFlowRuntime == nullptr || InFlowRuntime->Events.IsEmpty())
	{
		return false;
	}

	if (bFlowRunning || ActiveFlowRuntime != nullptr)
	{
		return false;
	}

	ActiveFlowRuntime = InFlowRuntime;
	ActiveFlowAsset = InFlowRuntime->GetSourceFlowAsset();
	CurrentFlowIndex = 0;
	bFlowRunning = true;
	NextExecutableTime = 0.0f;

	if (bResetRuntimeStates)
	{
		ResetFlowRuntimeStates();
	}

	return true;
}

bool UVdjmRecordEventManager::StartEventFlowFromJsonString(const FString& InJsonString, FString& OutError, bool bResetRuntimeStates)
{
	OutError.Reset();

	UVdjmRecordEventFlowRuntime* NewFlowRuntime = UVdjmRecordEventFlowRuntime::CreateFlowRuntimeFromJsonString(this, InJsonString, OutError);
	if (NewFlowRuntime == nullptr)
	{
		return false;
	}

	if (!StartEventFlowRuntime(NewFlowRuntime, bResetRuntimeStates))
	{
		OutError = TEXT("Event flow is already running or runtime has no events.");
		return false;
	}

	return true;
}

void UVdjmRecordEventManager::StopEventFlow()
{
	bFlowRunning = false;
	CurrentFlowIndex = INDEX_NONE;
	NextExecutableTime = 0.0f;
	ActiveFlowAsset.Reset();
	ActiveFlowRuntime = nullptr;
}

bool UVdjmRecordEventManager::IsEventFlowRunning() const
{
	return bFlowRunning && ActiveFlowRuntime != nullptr;
}

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventManager::GetActiveFlowAsset() const
{
	return ActiveFlowAsset.Get();
}

UVdjmRecordEventFlowRuntime* UVdjmRecordEventManager::GetActiveFlowRuntime() const
{
	return ActiveFlowRuntime;
}

int32 UVdjmRecordEventManager::GetCurrentFlowIndex() const
{
	return CurrentFlowIndex;
}

UWorld* UVdjmRecordEventManager::GetManagerWorld() const
{
	return CachedWorld.Get();
}

int32 UVdjmRecordEventManager::FindNextEventIndex(const UVdjmRecordEventBase* SourceEvent, TSubclassOf<UVdjmRecordEventBase> TargetClass, FName TargetTag) const
{
	const UVdjmRecordEventFlowRuntime* FlowRuntime = ActiveFlowRuntime;
	if (FlowRuntime == nullptr || SourceEvent == nullptr)
	{
		return INDEX_NONE;
	}

	const int32 SourceIndex = FlowRuntime->Events.Find(const_cast<UVdjmRecordEventBase*>(SourceEvent));
	if (SourceIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	for (int32 Index = SourceIndex + 1; Index < FlowRuntime->Events.Num(); ++Index)
	{
		UVdjmRecordEventBase* Candidate = FlowRuntime->Events[Index];
		if (Candidate == nullptr)
		{
			continue;
		}

		const bool bClassMatched = (!TargetClass || Candidate->IsA(TargetClass));
		const bool bTagMatched = (TargetTag.IsNone() || Candidate->EventTag == TargetTag);
		if (bClassMatched && bTagMatched)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

UWorld* UVdjmRecordEventManager::GetWorld() const
{
	return CachedWorld.Get();
}

void UVdjmRecordEventManager::Tick(float DeltaTime)
{
	TickEventFlow();
}

bool UVdjmRecordEventManager::IsTickable() const
{
	return !IsTemplate() && CachedWorld.IsValid();
}

TStatId UVdjmRecordEventManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVdjmRecordEventManager, STATGROUP_Tickables);
}

void UVdjmRecordEventManager::TickEventFlow()
{
	if (!bFlowRunning || ActiveFlowRuntime == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		FinishFlow(EVdjmRecordEventResultType::EAbort);
		return;
	}

	if (World->GetTimeSeconds() < NextExecutableTime)
	{
		return;
	}

	UVdjmRecordEventFlowRuntime* FlowRuntime = ActiveFlowRuntime;
	if (FlowRuntime == nullptr)
	{
		FinishFlow(EVdjmRecordEventResultType::EAbort);
		return;
	}

	constexpr int32 MaxStepPerTick = 64;
	for (int32 StepCount = 0; StepCount < MaxStepPerTick; ++StepCount)
	{
		if (CurrentFlowIndex < 0 || CurrentFlowIndex >= FlowRuntime->Events.Num())
		{
			FinishFlow(EVdjmRecordEventResultType::ESuccess);
			return;
		}

		UVdjmRecordEventBase* Event = FlowRuntime->Events[CurrentFlowIndex];
		if (Event == nullptr)
		{
			++CurrentFlowIndex;
			continue;
		}

		const FVdjmRecordEventResult Result = Event->ExecuteEvent(this, WeakBridgeActor.Get());
		switch (Result.ResultType)
		{
		case EVdjmRecordEventResultType::ESuccess:
			Event->ResetRuntimeState();
			++CurrentFlowIndex;
			break;
		case EVdjmRecordEventResultType::EFailure:
			FinishFlow(EVdjmRecordEventResultType::EFailure);
			return;
		case EVdjmRecordEventResultType::EAbort:
			FinishFlow(EVdjmRecordEventResultType::EAbort);
			return;
		case EVdjmRecordEventResultType::ERunning:
			NextExecutableTime = World->GetTimeSeconds() + FMath::Max(0.0f, Result.WaitSeconds);
			return;
		case EVdjmRecordEventResultType::ESelectIndex:
			if (FlowRuntime->Events.IsValidIndex(Result.SelectedIndex))
			{
				CurrentFlowIndex = Result.SelectedIndex;
				break;
			}
			FinishFlow(EVdjmRecordEventResultType::EFailure);
			return;
		case EVdjmRecordEventResultType::EJumpToLabel:
		{
			const int32 JumpIndex = FlowRuntime->FindEventIndexByTag(Result.JumpLabel);
			if (FlowRuntime->Events.IsValidIndex(JumpIndex))
			{
				CurrentFlowIndex = JumpIndex;
				break;
			}
			FinishFlow(EVdjmRecordEventResultType::EFailure);
			return;
		}
		default:
			FinishFlow(EVdjmRecordEventResultType::EFailure);
			return;
		}
	}
}

void UVdjmRecordEventManager::ResetFlowRuntimeStates()
{
	if (ActiveFlowRuntime == nullptr)
	{
		return;
	}

	ActiveFlowRuntime->ResetRuntimeStates();
}

void UVdjmRecordEventManager::FinishFlow(EVdjmRecordEventResultType FinalResultType)
{
	UVdjmRecordEventFlowDataAsset* FinishedAsset = ActiveFlowAsset.Get();
	bFlowRunning = false;
	CurrentFlowIndex = INDEX_NONE;
	NextExecutableTime = 0.0f;
	ActiveFlowAsset.Reset();
	ActiveFlowRuntime = nullptr;

	OnEventFlowFinished.Broadcast(this, FinishedAsset, FinalResultType);
}

void UVdjmRecordEventManager::HandleBridgeChainEvent(
	AVdjmRecordBridgeActor* InBridgeActor,
	EVdjmRecordBridgeInitStep PrevStep,
	EVdjmRecordBridgeInitStep CurrentStep)
{
	OnManagerObservedChainEvent.Broadcast(InBridgeActor, PrevStep, CurrentStep);
}
