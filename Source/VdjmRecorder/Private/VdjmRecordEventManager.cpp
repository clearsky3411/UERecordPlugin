#include "VdjmRecordEventManager.h"

#include "VdjmRecordBridgeActor.h"
#include "VdjmRecordEventFlowDataAsset.h"
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
	if (InFlowAsset == nullptr || InFlowAsset->Events.IsEmpty())
	{
		return false;
	}

	ActiveFlowAsset = InFlowAsset;
	CurrentFlowIndex = 0;
	bFlowRunning = true;
	NextExecutableTime = 0.0f;

	if (bResetRuntimeStates)
	{
		ResetFlowRuntimeStates();
	}

	return true;
}

void UVdjmRecordEventManager::StopEventFlow()
{
	bFlowRunning = false;
	CurrentFlowIndex = INDEX_NONE;
	NextExecutableTime = 0.0f;
	ActiveFlowAsset.Reset();
}

bool UVdjmRecordEventManager::IsEventFlowRunning() const
{
	return bFlowRunning && ActiveFlowAsset.IsValid();
}

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventManager::GetActiveFlowAsset() const
{
	return ActiveFlowAsset.Get();
}

int32 UVdjmRecordEventManager::GetCurrentFlowIndex() const
{
	return CurrentFlowIndex;
}

int32 UVdjmRecordEventManager::FindNextEventIndex(const UVdjmRecordEventBase* SourceEvent, TSubclassOf<UVdjmRecordEventBase> TargetClass, FName TargetTag) const
{
	const UVdjmRecordEventFlowDataAsset* FlowAsset = ActiveFlowAsset.Get();
	if (FlowAsset == nullptr || SourceEvent == nullptr)
	{
		return INDEX_NONE;
	}

	const int32 SourceIndex = FlowAsset->Events.Find(const_cast<UVdjmRecordEventBase*>(SourceEvent));
	if (SourceIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	for (int32 Index = SourceIndex + 1; Index < FlowAsset->Events.Num(); ++Index)
	{
		UVdjmRecordEventBase* Candidate = FlowAsset->Events[Index];
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
	if (!bFlowRunning || !ActiveFlowAsset.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		FinishFlow(EVdjmRecordEventResultType::Abort);
		return;
	}

	if (World->GetTimeSeconds() < NextExecutableTime)
	{
		return;
	}

	UVdjmRecordEventFlowDataAsset* FlowAsset = ActiveFlowAsset.Get();
	if (FlowAsset == nullptr)
	{
		FinishFlow(EVdjmRecordEventResultType::Abort);
		return;
	}

	constexpr int32 MaxStepPerTick = 64;
	for (int32 StepCount = 0; StepCount < MaxStepPerTick; ++StepCount)
	{
		if (CurrentFlowIndex < 0 || CurrentFlowIndex >= FlowAsset->Events.Num())
		{
			FinishFlow(EVdjmRecordEventResultType::Success);
			return;
		}

		UVdjmRecordEventBase* Event = FlowAsset->Events[CurrentFlowIndex];
		if (Event == nullptr)
		{
			++CurrentFlowIndex;
			continue;
		}

		const FVdjmRecordEventResult Result = Event->ExecuteEvent(this, WeakBridgeActor.Get());
		switch (Result.ResultType)
		{
		case EVdjmRecordEventResultType::Success:
			Event->ResetRuntimeState();
			++CurrentFlowIndex;
			break;
		case EVdjmRecordEventResultType::Failure:
			FinishFlow(EVdjmRecordEventResultType::Failure);
			return;
		case EVdjmRecordEventResultType::Abort:
			FinishFlow(EVdjmRecordEventResultType::Abort);
			return;
		case EVdjmRecordEventResultType::Running:
			NextExecutableTime = World->GetTimeSeconds() + FMath::Max(0.0f, Result.WaitSeconds);
			return;
		case EVdjmRecordEventResultType::SelectIndex:
			if (FlowAsset->Events.IsValidIndex(Result.SelectedIndex))
			{
				CurrentFlowIndex = Result.SelectedIndex;
				break;
			}
			FinishFlow(EVdjmRecordEventResultType::Failure);
			return;
		case EVdjmRecordEventResultType::JumpToLabel:
		{
			const int32 JumpIndex = FlowAsset->FindEventIndexByTag(Result.JumpLabel);
			if (FlowAsset->Events.IsValidIndex(JumpIndex))
			{
				CurrentFlowIndex = JumpIndex;
				break;
			}
			FinishFlow(EVdjmRecordEventResultType::Failure);
			return;
		}
		default:
			FinishFlow(EVdjmRecordEventResultType::Failure);
			return;
		}
	}
}

void UVdjmRecordEventManager::ResetFlowRuntimeStates()
{
	if (!ActiveFlowAsset.IsValid())
	{
		return;
	}

	for (UVdjmRecordEventBase* Event : ActiveFlowAsset->Events)
	{
		if (Event != nullptr)
		{
			Event->ResetRuntimeState();
		}
	}
}

void UVdjmRecordEventManager::FinishFlow(EVdjmRecordEventResultType FinalResultType)
{
	UVdjmRecordEventFlowDataAsset* FinishedAsset = ActiveFlowAsset.Get();
	bFlowRunning = false;
	CurrentFlowIndex = INDEX_NONE;
	NextExecutableTime = 0.0f;
	ActiveFlowAsset.Reset();

	OnEventFlowFinished.Broadcast(this, FinishedAsset, FinalResultType);
}

void UVdjmRecordEventManager::HandleBridgeChainEvent(
	AVdjmRecordBridgeActor* InBridgeActor,
	EVdjmRecordBridgeInitStep PrevStep,
	EVdjmRecordBridgeInitStep CurrentStep)
{
	OnManagerObservedChainEvent.Broadcast(InBridgeActor, PrevStep, CurrentStep);
}
