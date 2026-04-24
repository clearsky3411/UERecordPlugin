#include "VdjmEvents/VdjmRecordEventManager.h"

#include "VdjmRecordBridgeActor.h"
#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"
#include "VdjmEvents/VdjmRecordEventFlowRuntime.h"
#include "VdjmEvents/VdjmRecordEventNode.h"
#include "VdjmRecorderWorldContextSubsystem.h"

namespace
{
	constexpr int32 MaxStepPerTick = 64;

	FVdjmRecordFlowStepResult MakeFlowStepResult(
		EVdjmRecordFlowStepDisposition disposition,
		EVdjmRecordFlowControlAction flowControlAction = EVdjmRecordFlowControlAction::ENone,
		float waitSeconds = 0.0f,
		bool bDidExecuteEvent = false)
	{
		FVdjmRecordFlowStepResult stepResult;
		stepResult.Disposition = disposition;
		stepResult.FlowControlAction = flowControlAction;
		stepResult.WaitSeconds = FMath::Max(0.0f, waitSeconds);
		stepResult.bDidExecuteEvent = bDidExecuteEvent;
		return stepResult;
	}

	FVdjmRecordFlowStepResult ResolveFlowControlStepResult(
		EVdjmRecordFlowControlAction flowControlAction,
		EVdjmRecordFlowStepDisposition defaultDisposition,
		float waitSeconds = 0.0f,
		bool bDidExecuteEvent = true)
	{
		switch (flowControlAction)
		{
		case EVdjmRecordFlowControlAction::EFail:
			return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishFailure, flowControlAction, waitSeconds, bDidExecuteEvent);
		case EVdjmRecordFlowControlAction::EAbort:
			return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishAbort, flowControlAction, waitSeconds, bDidExecuteEvent);
		case EVdjmRecordFlowControlAction::EStop:
		case EVdjmRecordFlowControlAction::EPause:
			return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EYield, flowControlAction, waitSeconds, bDidExecuteEvent);
		case EVdjmRecordFlowControlAction::EResume:
		case EVdjmRecordFlowControlAction::ENone:
		default:
			return MakeFlowStepResult(defaultDisposition, flowControlAction, waitSeconds, bDidExecuteEvent);
		}
	}
}

UVdjmRecordEventManager* UVdjmRecordEventManager::CreateEventManager(UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}

	UVdjmRecordEventManager* manager = NewObject<UVdjmRecordEventManager>(WorldContextObject);
	if (manager == nullptr)
	{
		return nullptr;
	}

	if (not manager->InitializeManager(WorldContextObject))
	{
		return nullptr;
	}

	return manager;
}

bool UVdjmRecordEventManager::InitializeManager(UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return false;
	}

	CachedWorld = WorldContextObject->GetWorld();
	if (not CachedWorld.IsValid())
	{
		return false;
	}

	ResetSessionState(EVdjmRecorderSessionState::ENew);

	if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this))
	{
		worldContextSubsystem->RegisterWeakObjectContext(
			UVdjmRecorderWorldContextSubsystem::GetEventManagerContextKey(),
			this,
			StaticClass());
	}

	if (AVdjmRecordBridgeActor* bridgeActor = AVdjmRecordBridgeActor::TryGetRecordBridgeActor(CachedWorld.Get()))
	{
		return BindBridge(bridgeActor);
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
		ApplySessionStateByBridgeSnapshot();
		return true;
	}

	if (AVdjmRecordBridgeActor* existingBridge = WeakBridgeActor.Get())
	{
		existingBridge->OnChainInitEvent.RemoveDynamic(this, &UVdjmRecordEventManager::HandleBridgeChainEvent);
		existingBridge->OnInitCompleteEvent.RemoveDynamic(this, &UVdjmRecordEventManager::HandleBridgeInitComplete);
		existingBridge->OnInitErrorEndEvent.RemoveDynamic(this, &UVdjmRecordEventManager::HandleBridgeInitErrorEnd);
		existingBridge->OnRecordStarted.RemoveDynamic(this, &UVdjmRecordEventManager::HandleBridgeRecordStarted);
		existingBridge->OnRecordStopped.RemoveDynamic(this, &UVdjmRecordEventManager::HandleBridgeRecordStopped);
	}

	WeakBridgeActor = InBridgeActor;
	bPendingFinalization = false;
	ResetSessionState(EVdjmRecorderSessionState::ENew);

	if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this))
	{
		worldContextSubsystem->RegisterBridgeContext(InBridgeActor);
	}

	InBridgeActor->OnChainInitEvent.AddDynamic(this, &UVdjmRecordEventManager::HandleBridgeChainEvent);
	InBridgeActor->OnInitCompleteEvent.AddDynamic(this, &UVdjmRecordEventManager::HandleBridgeInitComplete);
	InBridgeActor->OnInitErrorEndEvent.AddDynamic(this, &UVdjmRecordEventManager::HandleBridgeInitErrorEnd);
	InBridgeActor->OnRecordStarted.AddDynamic(this, &UVdjmRecordEventManager::HandleBridgeRecordStarted);
	InBridgeActor->OnRecordStopped.AddDynamic(this, &UVdjmRecordEventManager::HandleBridgeRecordStopped);
	ApplySessionStateByBridgeSnapshot();
	return true;
}

AVdjmRecordBridgeActor* UVdjmRecordEventManager::GetBoundBridge() const
{
	return WeakBridgeActor.Get();
}

bool UVdjmRecordEventManager::StartRecordingByManager()
{
	if (not WeakBridgeActor.IsValid() && CachedWorld.IsValid())
	{
		BindBridge(AVdjmRecordBridgeActor::TryGetRecordBridgeActor(CachedWorld.Get()));
	}

	if (AVdjmRecordBridgeActor* bridgeActor = WeakBridgeActor.Get())
	{
		bPendingFinalization = false;
		bridgeActor->StartRecording();
		ApplySessionStateByBridgeSnapshot();
		return bridgeActor->IsRecording();
	}

	return false;
}

void UVdjmRecordEventManager::StopRecordingByManager()
{
	if (AVdjmRecordBridgeActor* bridgeActor = WeakBridgeActor.Get())
	{
		if (bridgeActor->IsRecording())
		{
			bPendingFinalization = true;
			TransitionSessionState(EVdjmRecorderSessionState::EFinalizing);
		}

		bridgeActor->StopRecording();
	}
}

bool UVdjmRecordEventManager::StartEventFlow(UVdjmRecordEventFlowDataAsset* InFlowAsset, bool bResetRuntimeStates)
{
	if (InFlowAsset == nullptr)
	{
		return false;
	}

	FString buildError;
	UVdjmRecordEventFlowRuntime* newFlowRuntime = UVdjmRecordEventFlowRuntime::CreateFlowRuntimeFromAsset(this, InFlowAsset, buildError);
	if (newFlowRuntime == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartEventFlow - Failed to build runtime from asset: %s"), *buildError);
		return false;
	}

	if (not StartEventFlowRuntime(newFlowRuntime, bResetRuntimeStates))
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

	if (const FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession())
	{
		if (mainFlowSession->bFlowRunning || mainFlowSession->ActiveFlowRuntime != nullptr)
		{
			return false;
		}
	}

	const int32 initialFlowIndex = FindNextExecutableEventIndex(InFlowRuntime->Events, 0);
	if (initialFlowIndex == INDEX_NONE)
	{
		return false;
	}

	if (not MainSessionHandle.IsValid())
	{
		MainSessionHandle = CreateFlowSessionHandle();
	}

	FVdjmRecordFlowSession& mainFlowSession = FindOrCreateFlowSession(MainSessionHandle);
	ResetFlowSession(mainFlowSession);
	mainFlowSession.ActiveFlowRuntime = InFlowRuntime;
	mainFlowSession.ActiveFlowAsset = InFlowRuntime->GetSourceFlowAsset();
	mainFlowSession.bFlowRunning = true;
	FlowRuntimeKeepAlive.AddUnique(InFlowRuntime);

	mainFlowSession.RootFlowHandle = CreateFlowHandle();
	FVdjmRecordFlowExecutionState& rootFlowState = FindOrCreateFlowExecutionState(mainFlowSession.RootFlowHandle, nullptr);
	rootFlowState.CurrentIndex = initialFlowIndex;
	rootFlowState.NextExecutableTime = 0.0f;
	rootFlowState.bPaused = false;
	PushActiveFlow(mainFlowSession.RootFlowHandle);

	if (bResetRuntimeStates)
	{
		ResetFlowRuntimeStates();
	}

	return true;
}

bool UVdjmRecordEventManager::StartEventFlowFromJsonString(const FString& InJsonString, FString& OutError, bool bResetRuntimeStates)
{
	OutError.Reset();

	UVdjmRecordEventFlowRuntime* newFlowRuntime = UVdjmRecordEventFlowRuntime::CreateFlowRuntimeFromJsonString(this, InJsonString, OutError);
	if (newFlowRuntime == nullptr)
	{
		return false;
	}

	if (not StartEventFlowRuntime(newFlowRuntime, bResetRuntimeStates))
	{
		OutError = TEXT("Event flow is already running or runtime has no events.");
		return false;
	}

	return true;
}

bool UVdjmRecordEventManager::StartEventFlowSession(
	UVdjmRecordEventFlowDataAsset* InFlowAsset,
	FVdjmRecordFlowSessionHandle& OutSessionHandle,
	bool bResetRuntimeStates)
{
	OutSessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();
	if (InFlowAsset == nullptr)
	{
		return false;
	}

	FString buildError;
	UVdjmRecordEventFlowRuntime* newFlowRuntime = UVdjmRecordEventFlowRuntime::CreateFlowRuntimeFromAsset(this, InFlowAsset, buildError);
	if (newFlowRuntime == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartEventFlowSession - Failed to build runtime from asset: %s"), *buildError);
		return false;
	}

	return StartEventFlowSessionRuntime(newFlowRuntime, OutSessionHandle, bResetRuntimeStates);
}

bool UVdjmRecordEventManager::StartEventFlowSessionFromJsonString(
	const FString& InJsonString,
	FVdjmRecordFlowSessionHandle& OutSessionHandle,
	FString& OutError,
	bool bResetRuntimeStates)
{
	OutSessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();
	OutError.Reset();

	UVdjmRecordEventFlowRuntime* newFlowRuntime = UVdjmRecordEventFlowRuntime::CreateFlowRuntimeFromJsonString(this, InJsonString, OutError);
	if (newFlowRuntime == nullptr)
	{
		return false;
	}

	if (not StartEventFlowSessionRuntime(newFlowRuntime, OutSessionHandle, bResetRuntimeStates))
	{
		OutError = TEXT("Event flow session runtime has no events.");
		return false;
	}

	return true;
}

void UVdjmRecordEventManager::StopEventFlow()
{
	FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession();
	if (mainFlowSession == nullptr)
	{
		return;
	}

	if (mainFlowSession->EventExecutionContextStack.Num() > 0)
	{
		RequestStopEventFlow();
		return;
	}

	FlowRuntimeKeepAlive.Remove(mainFlowSession->ActiveFlowRuntime);
	ResetFlowSession(*mainFlowSession);
}

bool UVdjmRecordEventManager::RequestStopEventFlow()
{
	return RequestStopEventFlowSession(MainSessionHandle);
}

bool UVdjmRecordEventManager::RequestPauseEventFlow()
{
	return RequestPauseEventFlowSession(MainSessionHandle);
}

bool UVdjmRecordEventManager::RequestResumeEventFlow()
{
	return RequestResumeEventFlowSession(MainSessionHandle);
}

bool UVdjmRecordEventManager::RequestAbortEventFlow()
{
	return RequestAbortEventFlowSession(MainSessionHandle);
}

bool UVdjmRecordEventManager::RequestFailEventFlow()
{
	return RequestFailEventFlowSession(MainSessionHandle);
}

bool UVdjmRecordEventManager::IsEventFlowRunning() const
{
	const FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession();
	return mainFlowSession != nullptr && mainFlowSession->bFlowRunning && mainFlowSession->ActiveFlowRuntime != nullptr;
}

bool UVdjmRecordEventManager::IsEventFlowPaused() const
{
	const FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession();
	if (mainFlowSession == nullptr)
	{
		return false;
	}

	const FVdjmRecordFlowExecutionState* rootFlowState = mainFlowSession->FlowExecutionStates.Find(mainFlowSession->RootFlowHandle);
	return IsEventFlowRunning() && rootFlowState != nullptr && rootFlowState->bPaused;
}

bool UVdjmRecordEventManager::EmitFlowSignalToSession(FVdjmRecordFlowSessionHandle SessionHandle, FName InSignalTag)
{
	if (InSignalTag.IsNone())
	{
		return false;
	}

	FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	if (flowSession == nullptr)
	{
		return false;
	}

	int32& signalCount = flowSession->PendingFlowSignals.FindOrAdd(InSignalTag);
	++signalCount;

	if (flowSession->EventExecutionContextStack.Num() > 0)
	{
		const FVdjmRecordEventExecutionContext& executionContext = flowSession->EventExecutionContextStack.Last();
		flowSession->LastSignalProducerHandles.Add(InSignalTag, executionContext.RuntimeHandle);
	}

	return true;
}

bool UVdjmRecordEventManager::RequestStopEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	if (flowSession == nullptr || not flowSession->bFlowRunning)
	{
		return false;
	}

	if (flowSession->EventExecutionContextStack.Num() > 0)
	{
		return RequestFlowControl(SessionHandle, EVdjmRecordFlowControlAction::EStop);
	}

	FlowRuntimeKeepAlive.Remove(flowSession->ActiveFlowRuntime);
	ResetFlowSession(*flowSession);
	return true;
}

bool UVdjmRecordEventManager::RequestPauseEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	if (flowSession == nullptr || not flowSession->bFlowRunning)
	{
		return false;
	}

	if (flowSession->EventExecutionContextStack.Num() > 0)
	{
		return RequestFlowControl(SessionHandle, EVdjmRecordFlowControlAction::EPause);
	}

	FVdjmRecordFlowExecutionState* rootFlowState = flowSession->FlowExecutionStates.Find(flowSession->RootFlowHandle);
	if (rootFlowState == nullptr)
	{
		return false;
	}

	rootFlowState->bPaused = true;
	return true;
}

bool UVdjmRecordEventManager::RequestResumeEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	if (flowSession == nullptr || not flowSession->bFlowRunning)
	{
		return false;
	}

	for (TPair<FVdjmRecordFlowHandle, FVdjmRecordFlowExecutionState>& pair : flowSession->FlowExecutionStates)
	{
		pair.Value.bPaused = false;
		FVdjmRecordPendingFlowControlRequest& pendingFlowControlRequest = pair.Value.PendingFlowControlRequest;
		if (pendingFlowControlRequest.Action == EVdjmRecordFlowControlAction::EPause ||
			pendingFlowControlRequest.Action == EVdjmRecordFlowControlAction::EResume)
		{
			pendingFlowControlRequest.Reset();
		}
	}

	return true;
}

bool UVdjmRecordEventManager::RequestAbortEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	if (flowSession == nullptr || not flowSession->bFlowRunning)
	{
		return false;
	}

	if (flowSession->EventExecutionContextStack.Num() > 0)
	{
		return RequestFlowControl(SessionHandle, EVdjmRecordFlowControlAction::EAbort);
	}

	FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EAbort);
	return true;
}

bool UVdjmRecordEventManager::RequestFailEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	if (flowSession == nullptr || not flowSession->bFlowRunning)
	{
		return false;
	}

	if (flowSession->EventExecutionContextStack.Num() > 0)
	{
		return RequestFlowControl(SessionHandle, EVdjmRecordFlowControlAction::EFail);
	}

	FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EFailure);
	return true;
}

bool UVdjmRecordEventManager::IsEventFlowSessionRunning(FVdjmRecordFlowSessionHandle SessionHandle) const
{
	const FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	return flowSession != nullptr && flowSession->bFlowRunning && flowSession->ActiveFlowRuntime != nullptr;
}

bool UVdjmRecordEventManager::IsEventFlowSessionPaused(FVdjmRecordFlowSessionHandle SessionHandle) const
{
	const FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	if (flowSession == nullptr)
	{
		return false;
	}

	const FVdjmRecordFlowExecutionState* rootFlowState = flowSession->FlowExecutionStates.Find(flowSession->RootFlowHandle);
	return IsEventFlowSessionRunning(SessionHandle) && rootFlowState != nullptr && rootFlowState->bPaused;
}

UVdjmRecordEventFlowDataAsset* UVdjmRecordEventManager::GetActiveFlowAsset() const
{
	const FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession();
	return mainFlowSession != nullptr ? mainFlowSession->ActiveFlowAsset.Get() : nullptr;
}

UVdjmRecordEventFlowRuntime* UVdjmRecordEventManager::GetActiveFlowRuntime() const
{
	const FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession();
	return mainFlowSession != nullptr ? mainFlowSession->ActiveFlowRuntime : nullptr;
}

int32 UVdjmRecordEventManager::GetCurrentFlowIndex() const
{
	const FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession();
	if (mainFlowSession == nullptr)
	{
		return INDEX_NONE;
	}

	const FVdjmRecordFlowExecutionState* rootFlowState = mainFlowSession->FlowExecutionStates.Find(mainFlowSession->RootFlowHandle);
	return rootFlowState != nullptr ? rootFlowState->CurrentIndex : INDEX_NONE;
}

UWorld* UVdjmRecordEventManager::GetManagerWorld() const
{
	return CachedWorld.Get();
}

EVdjmRecorderSessionState UVdjmRecordEventManager::GetSessionState() const
{
	return CurrentSessionState;
}

double UVdjmRecordEventManager::GetLastSessionTransitionSeconds() const
{
	return LastSessionTransitionSeconds;
}

FString UVdjmRecordEventManager::GetLastSessionError() const
{
	return LastSessionError;
}

int32 UVdjmRecordEventManager::FindNextEventIndex(const UVdjmRecordEventBase* SourceEvent, TSubclassOf<UVdjmRecordEventBase> TargetClass, FName TargetTag) const
{
	const FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	const UVdjmRecordEventFlowRuntime* flowRuntime = flowSession != nullptr ? flowSession->ActiveFlowRuntime : nullptr;
	if (flowRuntime == nullptr || SourceEvent == nullptr)
	{
		return INDEX_NONE;
	}

	const int32 sourceIndex = flowRuntime->Events.Find(const_cast<UVdjmRecordEventBase*>(SourceEvent));
	if (sourceIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	for (int32 index = sourceIndex + 1; index < flowRuntime->Events.Num(); ++index)
	{
		UVdjmRecordEventBase* candidate = flowRuntime->Events[index];
		if (candidate == nullptr)
		{
			continue;
		}

		const bool bClassMatched = (not TargetClass || candidate->IsA(TargetClass));
		const bool bTagMatched = (TargetTag.IsNone() || candidate->EventTag == TargetTag);
		if (bClassMatched && bTagMatched)
		{
			return index;
		}
	}

	return INDEX_NONE;
}

bool UVdjmRecordEventManager::EmitFlowSignal(FName InSignalTag)
{
	if (InSignalTag.IsNone())
	{
		return false;
	}

	FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	if (flowSession == nullptr)
	{
		return false;
	}

	return EmitFlowSignalToSession(flowSession->SessionHandle, InSignalTag);
}

bool UVdjmRecordEventManager::EmitFlowSignalByScope(FName InSignalTag, EVdjmRecordEventSignalScope InSignalScope)
{
	if (InSignalTag.IsNone())
	{
		return false;
	}

	switch (InSignalScope)
	{
	case EVdjmRecordEventSignalScope::ECurrentSession:
		return EmitFlowSignal(InSignalTag);
	case EVdjmRecordEventSignalScope::EMainSession:
		return EmitFlowSignalToSession(MainSessionHandle, InSignalTag);
	case EVdjmRecordEventSignalScope::EAllActiveSessions:
	{
		bool bEmittedAnySignal = false;
		for (TPair<FVdjmRecordFlowSessionHandle, FVdjmRecordFlowSession>& pair : FlowSessions)
		{
			if (pair.Value.bFlowRunning && pair.Value.ActiveFlowRuntime != nullptr)
			{
				bEmittedAnySignal |= EmitFlowSignalToSession(pair.Key, InSignalTag);
			}
		}
		return bEmittedAnySignal;
	}
	case EVdjmRecordEventSignalScope::EGlobal:
	{
		int32& signalCount = GlobalPendingFlowSignals.FindOrAdd(InSignalTag);
		++signalCount;

		if (const FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess())
		{
			if (flowSession->EventExecutionContextStack.Num() > 0)
			{
				const FVdjmRecordEventExecutionContext& executionContext = flowSession->EventExecutionContextStack.Last();
				GlobalLastSignalProducerHandles.Add(InSignalTag, executionContext.RuntimeHandle);
			}
		}
		return true;
	}
	default:
		return false;
	}
}

bool UVdjmRecordEventManager::ConsumeFlowSignal(FName InSignalTag)
{
	if (InSignalTag.IsNone())
	{
		return false;
	}

	FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	if (flowSession == nullptr)
	{
		return false;
	}

	bool bConsumedSignal = false;
	const FVdjmRecordEventRuntimeHandle* producerHandle = nullptr;

	if (int32* signalCount = flowSession->PendingFlowSignals.Find(InSignalTag))
	{
		if (*signalCount > 0)
		{
			--(*signalCount);
			if (*signalCount <= 0)
			{
				flowSession->PendingFlowSignals.Remove(InSignalTag);
			}

			producerHandle = flowSession->LastSignalProducerHandles.Find(InSignalTag);
			bConsumedSignal = true;
		}
	}

	if (not bConsumedSignal)
	{
		if (int32* signalCount = GlobalPendingFlowSignals.Find(InSignalTag))
		{
			if (*signalCount > 0)
			{
				--(*signalCount);
				if (*signalCount <= 0)
				{
					GlobalPendingFlowSignals.Remove(InSignalTag);
				}

				producerHandle = GlobalLastSignalProducerHandles.Find(InSignalTag);
				bConsumedSignal = true;
			}
		}
	}

	if (not bConsumedSignal)
	{
		return false;
	}

	if (flowSession->EventExecutionContextStack.Num() > 0)
	{
		const FVdjmRecordEventExecutionContext& executionContext = flowSession->EventExecutionContextStack.Last();
		FVdjmRecordFlowExecutionState& flowExecutionState = FindOrCreateFlowExecutionState(executionContext.FlowHandle, nullptr);
		flowExecutionState.PendingEdgeDirective.Reset();
		flowExecutionState.PendingEdgeDirective.bIsSet = true;
		flowExecutionState.PendingEdgeDirective.EdgeKind = EVdjmRecordEventEdgeKind::ESignal;
		flowExecutionState.PendingEdgeDirective.EdgeState = EVdjmRecordEventEdgeState::EAdvance;
		flowExecutionState.PendingEdgeDirective.ChannelKey = InSignalTag;
		flowExecutionState.PendingEdgeDirective.TerminalReason = EVdjmRecordEventResultType::ESuccess;
		flowExecutionState.PendingEdgeDirective.bHasExplicitToHandle = true;
		flowExecutionState.PendingEdgeDirective.ExplicitToHandle = executionContext.RuntimeHandle;
		flowExecutionState.PendingEdgeDirective.RequestedByTag = executionContext.EventNode.IsValid()
			? executionContext.EventNode->EventTag
			: NAME_None;
		flowExecutionState.PendingEdgeDirective.RequestedByClassName = executionContext.EventNode.IsValid()
			? executionContext.EventNode->GetClass()->GetName()
			: FString();

		if (producerHandle != nullptr)
		{
			flowExecutionState.PendingEdgeDirective.bHasExplicitFromHandle = true;
			flowExecutionState.PendingEdgeDirective.ExplicitFromHandle = *producerHandle;
		}
	}

	return true;
}

bool UVdjmRecordEventManager::HasPendingFlowSignal(FName InSignalTag) const
{
	if (InSignalTag.IsNone())
	{
		return false;
	}

	const FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	if (flowSession == nullptr)
	{
		return false;
	}

	const int32* signalCount = flowSession->PendingFlowSignals.Find(InSignalTag);
	if (signalCount != nullptr && *signalCount > 0)
	{
		return true;
	}

	const int32* globalSignalCount = GlobalPendingFlowSignals.Find(InSignalTag);
	return globalSignalCount != nullptr && *globalSignalCount > 0;
}

FVdjmRecordFlowHandle UVdjmRecordEventManager::GetCurrentFlowHandle() const
{
	const FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	if (flowSession == nullptr)
	{
		return FVdjmRecordFlowHandle::MakeInvalid();
	}

	if (flowSession->EventExecutionContextStack.Num() > 0)
	{
		return flowSession->EventExecutionContextStack.Last().FlowHandle;
	}

	if (flowSession->ActiveFlowStack.Num() > 0)
	{
		return flowSession->ActiveFlowStack.Last();
	}

	return FVdjmRecordFlowHandle::MakeInvalid();
}

FVdjmRecordObservedEdge UVdjmRecordEventManager::GetCurrentObservedEdge() const
{
	const FVdjmRecordFlowHandle flowHandle = GetCurrentFlowHandle();
	if (const FVdjmRecordFlowExecutionState* flowExecutionState = FindFlowExecutionState(flowHandle))
	{
		if (flowExecutionState->bHasObservedEdge)
		{
			return flowExecutionState->LastObservedEdge;
		}
	}

	return FVdjmRecordObservedEdge();
}

bool UVdjmRecordEventManager::RequestObservedEdgeDirective(
	EVdjmRecordEventEdgeKind InEdgeKind,
	EVdjmRecordEventEdgeState InEdgeState,
	const UVdjmRecordEventBase* InDestinationEvent,
	FName InChannelKey,
	EVdjmRecordEventResultType InTerminalReason)
{
	FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	if (flowSession == nullptr || flowSession->EventExecutionContextStack.Num() <= 0)
	{
		return false;
	}

	const FVdjmRecordEventExecutionContext& executionContext = flowSession->EventExecutionContextStack.Last();
	FVdjmRecordFlowExecutionState& flowExecutionState = FindOrCreateFlowExecutionState(executionContext.FlowHandle, nullptr);
	flowExecutionState.PendingEdgeDirective.Reset();
	flowExecutionState.PendingEdgeDirective.bIsSet = true;
	flowExecutionState.PendingEdgeDirective.EdgeKind = InEdgeKind;
	flowExecutionState.PendingEdgeDirective.EdgeState = InEdgeState;
	flowExecutionState.PendingEdgeDirective.ChannelKey = InChannelKey;
	flowExecutionState.PendingEdgeDirective.TerminalReason = InTerminalReason;
	flowExecutionState.PendingEdgeDirective.DestinationEvent = InDestinationEvent;
	flowExecutionState.PendingEdgeDirective.RequestedByTag = executionContext.EventNode.IsValid()
		? executionContext.EventNode->EventTag
		: NAME_None;
	flowExecutionState.PendingEdgeDirective.RequestedByClassName = executionContext.EventNode.IsValid()
		? executionContext.EventNode->GetClass()->GetName()
		: FString();
	return true;
}

UObject* UVdjmRecordEventManager::FindRuntimeObjectSlot(FName InSlotKey) const
{
	if (InSlotKey.IsNone())
	{
		return nullptr;
	}

	const FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	if (flowSession == nullptr)
	{
		return nullptr;
	}

	const TWeakObjectPtr<UObject>* foundObject = flowSession->RuntimeObjectSlots.Find(InSlotKey);
	return foundObject != nullptr ? foundObject->Get() : nullptr;
}

bool UVdjmRecordEventManager::SetRuntimeObjectSlot(FName InSlotKey, UObject* InObject)
{
	if (InSlotKey.IsNone() || InObject == nullptr)
	{
		return false;
	}

	FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	if (flowSession == nullptr)
	{
		return false;
	}

	flowSession->RuntimeObjectSlots.Add(InSlotKey, InObject);
	return true;
}

bool UVdjmRecordEventManager::ClearRuntimeObjectSlot(FName InSlotKey)
{
	if (InSlotKey.IsNone())
	{
		return false;
	}

	FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	return flowSession != nullptr && flowSession->RuntimeObjectSlots.Remove(InSlotKey) > 0;
}

FVdjmRecordFlowHandle UVdjmRecordEventManager::FindOrCreateChildFlowHandle(UVdjmRecordEventBase* OwnerEvent)
{
	if (OwnerEvent == nullptr)
	{
		return FVdjmRecordFlowHandle::MakeInvalid();
	}

	FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	if (flowSession == nullptr)
	{
		return FVdjmRecordFlowHandle::MakeInvalid();
	}

	if (const FVdjmRecordFlowHandle* foundHandle = flowSession->ChildFlowHandles.Find(OwnerEvent))
	{
		return *foundHandle;
	}

	const FVdjmRecordFlowHandle childFlowHandle = CreateFlowHandle();
	FVdjmRecordFlowExecutionState& flowExecutionState = FindOrCreateFlowExecutionState(childFlowHandle, OwnerEvent);
	flowExecutionState.ParentFlowHandle = GetCurrentFlowHandle();
	flowSession->ChildFlowHandles.Add(OwnerEvent, childFlowHandle);
	return childFlowHandle;
}

bool UVdjmRecordEventManager::PushActiveFlow(FVdjmRecordFlowHandle FlowHandle)
{
	if (not FlowHandle.IsValid())
	{
		return false;
	}

	FVdjmRecordFlowSession* flowSession = FindFlowSessionByFlowHandle(FlowHandle);
	if (flowSession == nullptr)
	{
		flowSession = ResolveFlowSessionForGeneralAccess();
	}

	if (flowSession == nullptr)
	{
		return false;
	}

	FindOrCreateFlowExecutionState(FlowHandle, nullptr);

	if (flowSession->ActiveFlowStack.Num() > 0 && flowSession->ActiveFlowStack.Last() == FlowHandle)
	{
		return true;
	}

	flowSession->ActiveFlowStack.Add(FlowHandle);
	return true;
}

void UVdjmRecordEventManager::PopActiveFlow(FVdjmRecordFlowHandle FlowHandle)
{
	FVdjmRecordFlowSession* flowSession = FindFlowSessionByFlowHandle(FlowHandle);
	if (not FlowHandle.IsValid() || flowSession == nullptr || flowSession->ActiveFlowStack.Num() <= 0)
	{
		return;
	}

	for (int32 index = flowSession->ActiveFlowStack.Num() - 1; index >= 0; --index)
	{
		if (flowSession->ActiveFlowStack[index] == FlowHandle)
		{
			flowSession->ActiveFlowStack.RemoveAt(index);
			return;
		}
	}
}

FVdjmRecordEventResult UVdjmRecordEventManager::ExecuteEventInFlow(
	UVdjmRecordEventBase* EventNode,
	AVdjmRecordBridgeActor* BridgeActor,
	FVdjmRecordFlowHandle FlowHandle)
{
	if (EventNode == nullptr || not FlowHandle.IsValid())
	{
		return UVdjmRecordEventBase::MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const FVdjmRecordFlowHandle currentFlowHandle = GetCurrentFlowHandle();
	const bool bShouldPushFlow = currentFlowHandle != FlowHandle;
	if (bShouldPushFlow && not PushActiveFlow(FlowHandle))
	{
		return UVdjmRecordEventBase::MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	FVdjmRecordFlowSession* flowSession = FindFlowSessionByFlowHandle(FlowHandle);
	if (flowSession == nullptr)
	{
		return UVdjmRecordEventBase::MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	FVdjmRecordEventExecutionContext executionContext;
	executionContext.SessionHandle = flowSession->SessionHandle;
	executionContext.FlowHandle = FlowHandle;
	executionContext.EventNode = EventNode;
	executionContext.RuntimeHandle = FindOrCreateRuntimeEventHandle(FlowHandle, EventNode);
	flowSession->EventExecutionContextStack.Add(executionContext);
	PushActiveSession(flowSession->SessionHandle);

	const FVdjmRecordEventResult result = EventNode->ExecuteEvent(this, BridgeActor);

	flowSession->EventExecutionContextStack.Pop();
	PopActiveSession(flowSession->SessionHandle);
	if (bShouldPushFlow)
	{
		PopActiveFlow(FlowHandle);
	}

	return result;
}

FVdjmRecordFlowStepResult UVdjmRecordEventManager::ExecuteFlowStep(const FVdjmRecordFlowStepContext& stepContext)
{
	if (not stepContext.IsValid())
	{
		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishFailure);
	}

	const TArray<TObjectPtr<UVdjmRecordEventBase>>& events = *stepContext.Events;
	int32& currentIndex = *stepContext.CurrentIndex;
	if (currentIndex < 0 || currentIndex >= events.Num())
	{
		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishSuccess);
	}

	UVdjmRecordEventBase* eventNode = events[currentIndex];
	if (eventNode == nullptr)
	{
		currentIndex = FindNextExecutableEventIndex(events, currentIndex + 1);
		if (currentIndex == INDEX_NONE)
		{
			return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishSuccess);
		}

		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EContinue);
	}

	const FVdjmRecordEventResult result = ExecuteEventInFlow(eventNode, stepContext.BridgeActor, stepContext.FlowHandle);
	switch (result.ResultType)
	{
	case EVdjmRecordEventResultType::ESuccess:
	{
		eventNode->ResetRuntimeState();
		const int32 nextEventIndex = FindNextExecutableEventIndex(events, currentIndex + 1);
		UVdjmRecordEventBase* destinationEvent = nextEventIndex != INDEX_NONE ? events[nextEventIndex] : nullptr;
		ObserveEventResult(eventNode, result, stepContext.FlowHandle, destinationEvent);
		currentIndex = nextEventIndex != INDEX_NONE ? nextEventIndex : events.Num();
		const EVdjmRecordFlowControlAction flowControlAction = ConsumePendingFlowControlRequest(stepContext.FlowHandle);
		if (nextEventIndex == INDEX_NONE)
		{
			return ResolveFlowControlStepResult(
				flowControlAction,
				EVdjmRecordFlowStepDisposition::EFinishSuccess,
				0.0f,
				true);
		}

		return ResolveFlowControlStepResult(
			flowControlAction,
			EVdjmRecordFlowStepDisposition::EContinue,
			0.0f,
			true);
	}
	case EVdjmRecordEventResultType::EFailure:
		ConsumePendingFlowControlRequest(stepContext.FlowHandle);
		ObserveEventResult(eventNode, result, stepContext.FlowHandle, nullptr);
		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishFailure, EVdjmRecordFlowControlAction::ENone, 0.0f, true);
	case EVdjmRecordEventResultType::EAbort:
	{
		const EVdjmRecordFlowControlAction flowControlAction = ConsumePendingFlowControlRequest(stepContext.FlowHandle);
		if (flowControlAction == EVdjmRecordFlowControlAction::EFail)
		{
			ObserveEventResult(
				eventNode,
				UVdjmRecordEventBase::MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f),
				stepContext.FlowHandle,
				nullptr);
			return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishFailure, flowControlAction, 0.0f, true);
		}

		ObserveEventResult(eventNode, result, stepContext.FlowHandle, nullptr);
		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishAbort, flowControlAction, 0.0f, true);
	}
	case EVdjmRecordEventResultType::ERunning:
	{
		const int32 nextEventIndex = FindNextExecutableEventIndex(events, currentIndex + 1);
		UVdjmRecordEventBase* destinationEvent = nextEventIndex != INDEX_NONE ? events[nextEventIndex] : nullptr;
		ObserveEventResult(eventNode, result, stepContext.FlowHandle, destinationEvent);
		const float waitSeconds = FMath::Max(0.0f, result.WaitSeconds);
		if (stepContext.NextExecutableTime != nullptr)
		{
			*stepContext.NextExecutableTime = stepContext.CurrentTimeSeconds + waitSeconds;
		}

		const EVdjmRecordFlowControlAction flowControlAction = ConsumePendingFlowControlRequest(stepContext.FlowHandle);
		return ResolveFlowControlStepResult(
			flowControlAction,
			EVdjmRecordFlowStepDisposition::EYield,
			waitSeconds,
			true);
	}
	case EVdjmRecordEventResultType::ESelectIndex:
		if (events.IsValidIndex(result.SelectedIndex))
		{
			ObserveEventResult(eventNode, result, stepContext.FlowHandle, events[result.SelectedIndex]);
			currentIndex = result.SelectedIndex;
			return ResolveFlowControlStepResult(
				ConsumePendingFlowControlRequest(stepContext.FlowHandle),
				EVdjmRecordFlowStepDisposition::EContinue,
				0.0f,
				true);
		}

		ObserveEventResult(
			eventNode,
			UVdjmRecordEventBase::MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f),
			stepContext.FlowHandle,
			nullptr);
		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishFailure, EVdjmRecordFlowControlAction::ENone, 0.0f, true);
	case EVdjmRecordEventResultType::EJumpToLabel:
	{
		const int32 jumpIndex = FindEventIndexByTag(events, result.JumpLabel);
		if (events.IsValidIndex(jumpIndex))
		{
			ObserveEventResult(eventNode, result, stepContext.FlowHandle, events[jumpIndex]);
			currentIndex = jumpIndex;
			return ResolveFlowControlStepResult(
				ConsumePendingFlowControlRequest(stepContext.FlowHandle),
				EVdjmRecordFlowStepDisposition::EContinue,
				0.0f,
				true);
		}

		ObserveEventResult(
			eventNode,
			UVdjmRecordEventBase::MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f),
			stepContext.FlowHandle,
			nullptr);
		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishFailure, EVdjmRecordFlowControlAction::ENone, 0.0f, true);
	}
	default:
		ObserveEventResult(
			eventNode,
			UVdjmRecordEventBase::MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f),
			stepContext.FlowHandle,
			nullptr);
		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishFailure, EVdjmRecordFlowControlAction::ENone, 0.0f, true);
	}
}

FVdjmRecordFlowStepResult UVdjmRecordEventManager::ExecuteFlowLoop(const FVdjmRecordFlowStepContext& stepContext, int32 maxStepCount)
{
	if (not stepContext.IsValid())
	{
		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishFailure);
	}

	FVdjmRecordFlowExecutionState* flowExecutionState = FindFlowExecutionState(stepContext.FlowHandle);
	if (flowExecutionState != nullptr && flowExecutionState->bPaused)
	{
		return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EYield, EVdjmRecordFlowControlAction::EPause);
	}

	FVdjmRecordFlowStepContext loopContext = stepContext;
	UWorld* world = GetWorld();
	if (world != nullptr)
	{
		loopContext.CurrentTimeSeconds = world->GetTimeSeconds();
	}

	if (loopContext.NextExecutableTime != nullptr && loopContext.CurrentTimeSeconds < *loopContext.NextExecutableTime)
	{
		return MakeFlowStepResult(
			EVdjmRecordFlowStepDisposition::EYield,
			EVdjmRecordFlowControlAction::ENone,
			*loopContext.NextExecutableTime - loopContext.CurrentTimeSeconds);
	}

	const int32 safeMaxStepCount = FMath::Max(1, maxStepCount);
	for (int32 stepCount = 0; stepCount < safeMaxStepCount; ++stepCount)
	{
		if (world != nullptr)
		{
			loopContext.CurrentTimeSeconds = world->GetTimeSeconds();
		}

		FVdjmRecordFlowStepResult stepResult = ExecuteFlowStep(loopContext);
		if (stepResult.FlowControlAction == EVdjmRecordFlowControlAction::EPause)
		{
			FindOrCreateFlowExecutionState(stepContext.FlowHandle, nullptr).bPaused = true;
		}

		switch (stepResult.Disposition)
		{
		case EVdjmRecordFlowStepDisposition::EContinue:
			continue;
		case EVdjmRecordFlowStepDisposition::EYield:
		case EVdjmRecordFlowStepDisposition::EFinishFailure:
		case EVdjmRecordFlowStepDisposition::EFinishAbort:
		case EVdjmRecordFlowStepDisposition::EFinishSuccess:
			return stepResult;
		default:
			return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EFinishFailure);
		}
	}

	return MakeFlowStepResult(EVdjmRecordFlowStepDisposition::EYield);
}

void UVdjmRecordEventManager::ObserveEventResult(
	UVdjmRecordEventBase* SourceEvent,
	const FVdjmRecordEventResult& Result,
	FVdjmRecordFlowHandle FlowHandle,
	const UVdjmRecordEventBase* DestinationEvent)
{
	if (SourceEvent == nullptr || not FlowHandle.IsValid())
	{
		return;
	}

	FVdjmRecordObservedEdge observedEdge = BuildObservedEdgeFromResult(SourceEvent, Result, FlowHandle, DestinationEvent);
	observedEdge = ApplyPendingEdgeDirective(observedEdge, FlowHandle);

	FVdjmRecordFlowExecutionState& flowExecutionState = FindOrCreateFlowExecutionState(FlowHandle, nullptr);
	flowExecutionState.LastObservedEdge = observedEdge;
	flowExecutionState.bHasObservedEdge = true;
}

UWorld* UVdjmRecordEventManager::GetWorld() const
{
	return CachedWorld.Get();
}

void UVdjmRecordEventManager::Tick(float DeltaTime)
{
	(void)DeltaTime;
	TickEventFlow();
	ApplySessionStateByBridgeSnapshot();
}

bool UVdjmRecordEventManager::IsTickable() const
{
	return not IsTemplate() && CachedWorld.IsValid();
}

TStatId UVdjmRecordEventManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVdjmRecordEventManager, STATGROUP_Tickables);
}

void UVdjmRecordEventManager::TickEventFlow()
{
	TArray<FVdjmRecordFlowSessionHandle> runningSessionHandles;
	for (const TPair<FVdjmRecordFlowSessionHandle, FVdjmRecordFlowSession>& pair : FlowSessions)
	{
		if (pair.Value.bFlowRunning && pair.Value.ActiveFlowRuntime != nullptr)
		{
			runningSessionHandles.Add(pair.Key);
		}
	}

	for (const FVdjmRecordFlowSessionHandle sessionHandle : runningSessionHandles)
	{
		TickFlowSession(sessionHandle);
	}
}

void UVdjmRecordEventManager::TickFlowSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	if (flowSession == nullptr || not flowSession->bFlowRunning || flowSession->ActiveFlowRuntime == nullptr)
	{
		return;
	}

	FVdjmRecordFlowExecutionState* rootFlowState = flowSession->FlowExecutionStates.Find(flowSession->RootFlowHandle);
	if (rootFlowState == nullptr)
	{
		FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EAbort);
		return;
	}

	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EAbort);
		return;
	}

	UVdjmRecordEventFlowRuntime* flowRuntime = flowSession->ActiveFlowRuntime;
	if (flowRuntime == nullptr)
	{
		FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EAbort);
		return;
	}

	FVdjmRecordFlowStepContext stepContext;
	stepContext.FlowHandle = flowSession->RootFlowHandle;
	stepContext.BridgeActor = WeakBridgeActor.Get();
	stepContext.Events = &flowRuntime->Events;
	stepContext.CurrentIndex = &rootFlowState->CurrentIndex;
	stepContext.NextExecutableTime = &rootFlowState->NextExecutableTime;
	stepContext.CurrentTimeSeconds = world->GetTimeSeconds();

	PushActiveSession(SessionHandle);
	const FVdjmRecordFlowStepResult stepResult = ExecuteFlowLoop(stepContext, MaxStepPerTick);
	PopActiveSession(SessionHandle);
	switch (stepResult.Disposition)
	{
	case EVdjmRecordFlowStepDisposition::EContinue:
	case EVdjmRecordFlowStepDisposition::EYield:
		switch (stepResult.FlowControlAction)
		{
		case EVdjmRecordFlowControlAction::EStop:
			RequestStopEventFlowSession(SessionHandle);
			return;
		case EVdjmRecordFlowControlAction::EAbort:
			FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EAbort);
			return;
		case EVdjmRecordFlowControlAction::EFail:
			FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EFailure);
			return;
		case EVdjmRecordFlowControlAction::EPause:
		default:
			return;
		}
	case EVdjmRecordFlowStepDisposition::EFinishFailure:
		FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EFailure);
		return;
	case EVdjmRecordFlowStepDisposition::EFinishAbort:
		FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EAbort);
		return;
	case EVdjmRecordFlowStepDisposition::EFinishSuccess:
		FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::ESuccess);
		return;
	default:
		FinishFlowSession(SessionHandle, EVdjmRecordEventResultType::EFailure);
		return;
	}
}

void UVdjmRecordEventManager::ResetFlowRuntimeStates()
{
	FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession();
	if (mainFlowSession == nullptr || mainFlowSession->ActiveFlowRuntime == nullptr)
	{
		return;
	}

	mainFlowSession->ActiveFlowRuntime->ResetRuntimeStates();
}

void UVdjmRecordEventManager::FinishFlow(EVdjmRecordEventResultType FinalResultType)
{
	FinishFlowSession(MainSessionHandle, FinalResultType);
}

void UVdjmRecordEventManager::FinishFlowSession(FVdjmRecordFlowSessionHandle SessionHandle, EVdjmRecordEventResultType FinalResultType)
{
	FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	UVdjmRecordEventFlowDataAsset* finishedAsset = flowSession != nullptr
		? flowSession->ActiveFlowAsset.Get()
		: nullptr;

	if (flowSession != nullptr)
	{
		FlowRuntimeKeepAlive.Remove(flowSession->ActiveFlowRuntime);
		ResetFlowSession(*flowSession);
	}

	if (SessionHandle == MainSessionHandle &&
		(FinalResultType == EVdjmRecordEventResultType::EFailure || FinalResultType == EVdjmRecordEventResultType::EAbort))
	{
		TransitionSessionState(
			EVdjmRecorderSessionState::EFailed,
			FString::Printf(TEXT("Event flow finished with result '%s'."), *StaticEnum<EVdjmRecordEventResultType>()->GetValueAsString(FinalResultType)));
	}

	OnEventFlowSessionFinished.Broadcast(this, SessionHandle, finishedAsset, FinalResultType);
}

void UVdjmRecordEventManager::ResetFlowSignals()
{
	if (FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession())
	{
		mainFlowSession->PendingFlowSignals.Reset();
		mainFlowSession->LastSignalProducerHandles.Reset();
	}
}

void UVdjmRecordEventManager::ResetFlowExecutionStates()
{
	if (FVdjmRecordFlowSession* mainFlowSession = FindMainFlowSession())
	{
		mainFlowSession->RootFlowHandle = FVdjmRecordFlowHandle::MakeInvalid();
		mainFlowSession->FlowExecutionStates.Reset();
		mainFlowSession->ChildFlowHandles.Reset();
		mainFlowSession->ActiveFlowStack.Reset();
		mainFlowSession->EventExecutionContextStack.Reset();
		mainFlowSession->RuntimeObjectSlots.Reset();
	}
	NextFlowHandleValue = 1;
	NextRuntimeEventHandleValue = 1;
}

FVdjmRecordFlowSessionHandle UVdjmRecordEventManager::CreateFlowSessionHandle()
{
	FVdjmRecordFlowSessionHandle sessionHandle;
	sessionHandle.Value = NextFlowSessionHandleValue++;
	return sessionHandle;
}

bool UVdjmRecordEventManager::StartEventFlowSessionRuntime(
	UVdjmRecordEventFlowRuntime* InFlowRuntime,
	FVdjmRecordFlowSessionHandle& OutSessionHandle,
	bool bResetRuntimeStates,
	FVdjmRecordFlowSessionHandle ParentSessionHandle)
{
	OutSessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();
	if (InFlowRuntime == nullptr || InFlowRuntime->Events.IsEmpty())
	{
		return false;
	}

	const int32 initialFlowIndex = FindNextExecutableEventIndex(InFlowRuntime->Events, 0);
	if (initialFlowIndex == INDEX_NONE)
	{
		return false;
	}

	const FVdjmRecordFlowSessionHandle sessionHandle = CreateFlowSessionHandle();
	FVdjmRecordFlowSession& flowSession = FindOrCreateFlowSession(sessionHandle);
	ResetFlowSession(flowSession);
	flowSession.SessionHandle = sessionHandle;
	flowSession.ParentSessionHandle = ParentSessionHandle;
	flowSession.ActiveFlowRuntime = InFlowRuntime;
	flowSession.ActiveFlowAsset = InFlowRuntime->GetSourceFlowAsset();
	flowSession.bFlowRunning = true;
	FlowRuntimeKeepAlive.AddUnique(InFlowRuntime);

	flowSession.RootFlowHandle = CreateFlowHandle();
	FVdjmRecordFlowExecutionState& rootFlowState = FindOrCreateFlowExecutionState(flowSession.RootFlowHandle, nullptr);
	rootFlowState.CurrentIndex = initialFlowIndex;
	rootFlowState.NextExecutableTime = 0.0f;
	rootFlowState.bPaused = false;
	PushActiveFlow(flowSession.RootFlowHandle);

	if (bResetRuntimeStates)
	{
		InFlowRuntime->ResetRuntimeStates();
	}

	OutSessionHandle = sessionHandle;
	return true;
}

UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::FindFlowSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	if (not SessionHandle.IsValid())
	{
		return nullptr;
	}

	return FlowSessions.Find(SessionHandle);
}

const UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::FindFlowSession(FVdjmRecordFlowSessionHandle SessionHandle) const
{
	if (not SessionHandle.IsValid())
	{
		return nullptr;
	}

	return FlowSessions.Find(SessionHandle);
}

UVdjmRecordEventManager::FVdjmRecordFlowSession& UVdjmRecordEventManager::FindOrCreateFlowSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	check(SessionHandle.IsValid());

	if (FVdjmRecordFlowSession* existingSession = FindFlowSession(SessionHandle))
	{
		return *existingSession;
	}

	FVdjmRecordFlowSession newSession;
	newSession.SessionHandle = SessionHandle;
	return FlowSessions.Add(SessionHandle, MoveTemp(newSession));
}

UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::FindCurrentFlowSession()
{
	if (ActiveSessionStack.Num() > 0)
	{
		return FindFlowSession(ActiveSessionStack.Last());
	}

	return nullptr;
}

const UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::FindCurrentFlowSession() const
{
	if (ActiveSessionStack.Num() > 0)
	{
		return FindFlowSession(ActiveSessionStack.Last());
	}

	return nullptr;
}

UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::FindMainFlowSession()
{
	return FindFlowSession(MainSessionHandle);
}

const UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::FindMainFlowSession() const
{
	return FindFlowSession(MainSessionHandle);
}

UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::FindFlowSessionByFlowHandle(FVdjmRecordFlowHandle FlowHandle)
{
	if (not FlowHandle.IsValid())
	{
		return nullptr;
	}

	for (TPair<FVdjmRecordFlowSessionHandle, FVdjmRecordFlowSession>& pair : FlowSessions)
	{
		if (pair.Value.RootFlowHandle == FlowHandle || pair.Value.FlowExecutionStates.Contains(FlowHandle))
		{
			return &pair.Value;
		}
	}

	return nullptr;
}

const UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::FindFlowSessionByFlowHandle(FVdjmRecordFlowHandle FlowHandle) const
{
	if (not FlowHandle.IsValid())
	{
		return nullptr;
	}

	for (const TPair<FVdjmRecordFlowSessionHandle, FVdjmRecordFlowSession>& pair : FlowSessions)
	{
		if (pair.Value.RootFlowHandle == FlowHandle || pair.Value.FlowExecutionStates.Contains(FlowHandle))
		{
			return &pair.Value;
		}
	}

	return nullptr;
}

UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::ResolveFlowSessionForGeneralAccess()
{
	if (FVdjmRecordFlowSession* currentSession = FindCurrentFlowSession())
	{
		return currentSession;
	}

	return FindMainFlowSession();
}

const UVdjmRecordEventManager::FVdjmRecordFlowSession* UVdjmRecordEventManager::ResolveFlowSessionForGeneralAccess() const
{
	if (const FVdjmRecordFlowSession* currentSession = FindCurrentFlowSession())
	{
		return currentSession;
	}

	return FindMainFlowSession();
}

bool UVdjmRecordEventManager::PushActiveSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	if (not SessionHandle.IsValid() || FindFlowSession(SessionHandle) == nullptr)
	{
		return false;
	}

	ActiveSessionStack.Add(SessionHandle);
	return true;
}

void UVdjmRecordEventManager::PopActiveSession(FVdjmRecordFlowSessionHandle SessionHandle)
{
	if (not SessionHandle.IsValid())
	{
		return;
	}

	for (int32 index = ActiveSessionStack.Num() - 1; index >= 0; --index)
	{
		if (ActiveSessionStack[index] == SessionHandle)
		{
			ActiveSessionStack.RemoveAt(index);
			return;
		}
	}
}

void UVdjmRecordEventManager::ResetFlowSession(FVdjmRecordFlowSession& FlowSession)
{
	FlowSession.ActiveFlowAsset.Reset();
	FlowSession.ActiveFlowRuntime = nullptr;
	FlowSession.RootFlowHandle = FVdjmRecordFlowHandle::MakeInvalid();
	FlowSession.FlowExecutionStates.Reset();
	FlowSession.ChildFlowHandles.Reset();
	FlowSession.ActiveFlowStack.Reset();
	FlowSession.EventExecutionContextStack.Reset();
	FlowSession.PendingFlowSignals.Reset();
	FlowSession.LastSignalProducerHandles.Reset();
	FlowSession.RuntimeObjectSlots.Reset();
	FlowSession.bFlowRunning = false;
}

FVdjmRecordFlowHandle UVdjmRecordEventManager::CreateFlowHandle()
{
	FVdjmRecordFlowHandle flowHandle;
	flowHandle.Value = NextFlowHandleValue++;
	return flowHandle;
}

UVdjmRecordEventManager::FVdjmRecordFlowExecutionState* UVdjmRecordEventManager::FindFlowExecutionState(FVdjmRecordFlowHandle FlowHandle)
{
	if (not FlowHandle.IsValid())
	{
		return nullptr;
	}

	FVdjmRecordFlowSession* flowSession = FindFlowSessionByFlowHandle(FlowHandle);
	return flowSession != nullptr ? flowSession->FlowExecutionStates.Find(FlowHandle) : nullptr;
}

const UVdjmRecordEventManager::FVdjmRecordFlowExecutionState* UVdjmRecordEventManager::FindFlowExecutionState(FVdjmRecordFlowHandle FlowHandle) const
{
	if (not FlowHandle.IsValid())
	{
		return nullptr;
	}

	const FVdjmRecordFlowSession* flowSession = FindFlowSessionByFlowHandle(FlowHandle);
	return flowSession != nullptr ? flowSession->FlowExecutionStates.Find(FlowHandle) : nullptr;
}

UVdjmRecordEventManager::FVdjmRecordFlowExecutionState& UVdjmRecordEventManager::FindOrCreateFlowExecutionState(
	FVdjmRecordFlowHandle FlowHandle,
	UVdjmRecordEventBase* OwnerEvent)
{
	FVdjmRecordFlowExecutionState* existingState = FindFlowExecutionState(FlowHandle);
	if (existingState != nullptr)
	{
		if (OwnerEvent != nullptr && not existingState->OwnerEvent.IsValid())
		{
			existingState->OwnerEvent = OwnerEvent;
		}
		return *existingState;
	}

	FVdjmRecordFlowExecutionState newState;
	newState.FlowHandle = FlowHandle;
	newState.OwnerEvent = OwnerEvent;
	FVdjmRecordFlowSession* flowSession = FindFlowSessionByFlowHandle(FlowHandle);
	if (flowSession == nullptr)
	{
		flowSession = ResolveFlowSessionForGeneralAccess();
	}

	check(flowSession != nullptr);
	return flowSession->FlowExecutionStates.Add(FlowHandle, MoveTemp(newState));
}

FVdjmRecordEventRuntimeHandle UVdjmRecordEventManager::FindOrCreateRuntimeEventHandle(
	FVdjmRecordFlowHandle FlowHandle,
	const UVdjmRecordEventBase* EventNode)
{
	if (EventNode == nullptr)
	{
		return FVdjmRecordEventRuntimeHandle::MakeInvalid();
	}

	FVdjmRecordFlowExecutionState& flowExecutionState = FindOrCreateFlowExecutionState(FlowHandle, nullptr);
	if (const FVdjmRecordEventRuntimeHandle* foundHandle = flowExecutionState.RuntimeEventHandles.Find(EventNode))
	{
		return *foundHandle;
	}

	FVdjmRecordEventRuntimeHandle runtimeHandle;
	runtimeHandle.Value = NextRuntimeEventHandleValue++;
	flowExecutionState.RuntimeEventHandles.Add(EventNode, runtimeHandle);
	return runtimeHandle;
}

FVdjmRecordObservedEdge UVdjmRecordEventManager::BuildObservedEdgeFromResult(
	UVdjmRecordEventBase* SourceEvent,
	const FVdjmRecordEventResult& Result,
	FVdjmRecordFlowHandle FlowHandle,
	const UVdjmRecordEventBase* DestinationEvent)
{
	FVdjmRecordObservedEdge observedEdge;
	observedEdge.FlowHandle = FlowHandle;
	observedEdge.TerminalReason = Result.ResultType;
	observedEdge.FromHandle = FindOrCreateRuntimeEventHandle(FlowHandle, SourceEvent);
	if (DestinationEvent != nullptr)
	{
		observedEdge.ToHandle = FindOrCreateRuntimeEventHandle(FlowHandle, DestinationEvent);
	}

	switch (Result.ResultType)
	{
	case EVdjmRecordEventResultType::ESuccess:
		if (observedEdge.ToHandle.IsValid())
		{
			observedEdge.EdgeKind = EVdjmRecordEventEdgeKind::ENext;
			observedEdge.EdgeState = EVdjmRecordEventEdgeState::EAdvance;
		}
		else
		{
			observedEdge.EdgeKind = EVdjmRecordEventEdgeKind::ETerminal;
			observedEdge.EdgeState = EVdjmRecordEventEdgeState::EDiscard;
			observedEdge.ToHandle = FVdjmRecordEventRuntimeHandle::MakeTerminalSink();
		}
		break;
	case EVdjmRecordEventResultType::ERunning:
		observedEdge.EdgeKind = EVdjmRecordEventEdgeKind::ENext;
		observedEdge.EdgeState = EVdjmRecordEventEdgeState::ERepeat;
		if (not observedEdge.ToHandle.IsValid())
		{
			observedEdge.ToHandle = observedEdge.FromHandle;
		}
		break;
	case EVdjmRecordEventResultType::ESelectIndex:
	case EVdjmRecordEventResultType::EJumpToLabel:
		if (observedEdge.ToHandle.IsValid())
		{
			observedEdge.EdgeKind = EVdjmRecordEventEdgeKind::EJump;
			observedEdge.EdgeState = EVdjmRecordEventEdgeState::EAdvance;
		}
		else
		{
			observedEdge.EdgeKind = EVdjmRecordEventEdgeKind::ETerminal;
			observedEdge.EdgeState = EVdjmRecordEventEdgeState::EDiscard;
			observedEdge.ToHandle = FVdjmRecordEventRuntimeHandle::MakeTerminalSink();
			observedEdge.TerminalReason = EVdjmRecordEventResultType::EFailure;
		}
		break;
	case EVdjmRecordEventResultType::EFailure:
	case EVdjmRecordEventResultType::EAbort:
	default:
		observedEdge.EdgeKind = EVdjmRecordEventEdgeKind::ETerminal;
		observedEdge.EdgeState = EVdjmRecordEventEdgeState::EDiscard;
		observedEdge.ToHandle = FVdjmRecordEventRuntimeHandle::MakeTerminalSink();
		break;
	}

	return observedEdge;
}

FVdjmRecordObservedEdge UVdjmRecordEventManager::ApplyPendingEdgeDirective(
	const FVdjmRecordObservedEdge& DefaultEdge,
	FVdjmRecordFlowHandle FlowHandle)
{
	FVdjmRecordFlowExecutionState& flowExecutionState = FindOrCreateFlowExecutionState(FlowHandle, nullptr);
	if (not flowExecutionState.PendingEdgeDirective.bIsSet)
	{
		return DefaultEdge;
	}

	FVdjmRecordObservedEdge observedEdge = DefaultEdge;
	const FVdjmRecordPendingEdgeDirective& pendingEdgeDirective = flowExecutionState.PendingEdgeDirective;
	observedEdge.EdgeKind = pendingEdgeDirective.EdgeKind;
	observedEdge.EdgeState = pendingEdgeDirective.EdgeState;
	observedEdge.ChannelKey = pendingEdgeDirective.ChannelKey;
	observedEdge.TerminalReason = pendingEdgeDirective.TerminalReason;
	observedEdge.RequestedByTag = pendingEdgeDirective.RequestedByTag;
	observedEdge.RequestedByClassName = pendingEdgeDirective.RequestedByClassName;

	if (pendingEdgeDirective.bHasExplicitFromHandle)
	{
		observedEdge.FromHandle = pendingEdgeDirective.ExplicitFromHandle;
	}

	if (pendingEdgeDirective.bHasExplicitToHandle)
	{
		observedEdge.ToHandle = pendingEdgeDirective.ExplicitToHandle;
	}
	else if (pendingEdgeDirective.DestinationEvent != nullptr)
	{
		observedEdge.ToHandle = FindOrCreateRuntimeEventHandle(FlowHandle, pendingEdgeDirective.DestinationEvent);
	}

	if (observedEdge.EdgeKind == EVdjmRecordEventEdgeKind::ETerminal)
	{
		observedEdge.ToHandle = FVdjmRecordEventRuntimeHandle::MakeTerminalSink();
	}

	flowExecutionState.PendingEdgeDirective.Reset();
	return observedEdge;
}

EVdjmRecordFlowControlAction UVdjmRecordEventManager::ConsumePendingFlowControlRequest(FVdjmRecordFlowHandle FlowHandle)
{
	FVdjmRecordFlowExecutionState* flowExecutionState = FindFlowExecutionState(FlowHandle);
	if (flowExecutionState == nullptr)
	{
		return EVdjmRecordFlowControlAction::ENone;
	}

	const EVdjmRecordFlowControlAction flowControlAction = flowExecutionState->PendingFlowControlRequest.Action;
	flowExecutionState->PendingFlowControlRequest.Reset();
	return flowControlAction;
}

bool UVdjmRecordEventManager::RequestFlowControl(EVdjmRecordFlowControlAction Action)
{
	FVdjmRecordFlowSession* flowSession = ResolveFlowSessionForGeneralAccess();
	if (flowSession == nullptr)
	{
		return false;
	}

	return RequestFlowControl(flowSession->SessionHandle, Action);
}

bool UVdjmRecordEventManager::RequestFlowControl(FVdjmRecordFlowSessionHandle SessionHandle, EVdjmRecordFlowControlAction Action)
{
	if (Action == EVdjmRecordFlowControlAction::ENone)
	{
		return false;
	}

	FVdjmRecordFlowSession* flowSession = FindFlowSession(SessionHandle);
	if (flowSession == nullptr || not flowSession->bFlowRunning)
	{
		return false;
	}

	FVdjmRecordFlowHandle flowHandle = FVdjmRecordFlowHandle::MakeInvalid();
	if (flowSession->EventExecutionContextStack.Num() > 0)
	{
		flowHandle = flowSession->EventExecutionContextStack.Last().FlowHandle;
	}
	else if (flowSession->ActiveFlowStack.Num() > 0)
	{
		flowHandle = flowSession->ActiveFlowStack.Last();
	}
	else
	{
		flowHandle = flowSession->RootFlowHandle;
	}

	if (not flowHandle.IsValid())
	{
		return false;
	}

	ApplyFlowControlRequestToFlowChain(flowHandle, Action);
	return true;
}

void UVdjmRecordEventManager::ApplyFlowControlRequestToFlowChain(
	FVdjmRecordFlowHandle FlowHandle,
	EVdjmRecordFlowControlAction Action)
{
	FVdjmRecordFlowHandle targetFlowHandle = FlowHandle;
	while (targetFlowHandle.IsValid())
	{
		FVdjmRecordFlowExecutionState& flowExecutionState = FindOrCreateFlowExecutionState(targetFlowHandle, nullptr);
		flowExecutionState.PendingFlowControlRequest.Action = MergeFlowControlAction(
			flowExecutionState.PendingFlowControlRequest.Action,
			Action);
		targetFlowHandle = flowExecutionState.ParentFlowHandle;
	}
}

EVdjmRecordFlowControlAction UVdjmRecordEventManager::MergeFlowControlAction(
	EVdjmRecordFlowControlAction CurrentAction,
	EVdjmRecordFlowControlAction RequestedAction) const
{
	switch (RequestedAction)
	{
	case EVdjmRecordFlowControlAction::EFail:
		return EVdjmRecordFlowControlAction::EFail;
	case EVdjmRecordFlowControlAction::EAbort:
		return CurrentAction == EVdjmRecordFlowControlAction::EFail
			? CurrentAction
			: EVdjmRecordFlowControlAction::EAbort;
	case EVdjmRecordFlowControlAction::EStop:
		return (CurrentAction == EVdjmRecordFlowControlAction::EFail ||
			CurrentAction == EVdjmRecordFlowControlAction::EAbort)
			? CurrentAction
			: EVdjmRecordFlowControlAction::EStop;
	case EVdjmRecordFlowControlAction::EPause:
		return (CurrentAction == EVdjmRecordFlowControlAction::EFail ||
			CurrentAction == EVdjmRecordFlowControlAction::EAbort ||
			CurrentAction == EVdjmRecordFlowControlAction::EStop)
			? CurrentAction
			: EVdjmRecordFlowControlAction::EPause;
	case EVdjmRecordFlowControlAction::EResume:
		return (CurrentAction == EVdjmRecordFlowControlAction::EFail ||
			CurrentAction == EVdjmRecordFlowControlAction::EAbort ||
			CurrentAction == EVdjmRecordFlowControlAction::EStop)
			? CurrentAction
			: EVdjmRecordFlowControlAction::EResume;
	default:
		return CurrentAction;
	}
}

int32 UVdjmRecordEventManager::FindNextExecutableEventIndex(const TArray<TObjectPtr<UVdjmRecordEventBase>>& Events, int32 StartIndex) const
{
	for (int32 index = StartIndex; index < Events.Num(); ++index)
	{
		if (Events[index] != nullptr)
		{
			return index;
		}
	}

	return INDEX_NONE;
}

int32 UVdjmRecordEventManager::FindEventIndexByTag(const TArray<TObjectPtr<UVdjmRecordEventBase>>& events, FName eventTag) const
{
	if (eventTag.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 index = 0; index < events.Num(); ++index)
	{
		const UVdjmRecordEventBase* eventNode = events[index];
		if (eventNode != nullptr && eventNode->EventTag == eventTag)
		{
			return index;
		}
	}

	return INDEX_NONE;
}

void UVdjmRecordEventManager::HandleBridgeChainEvent(
	AVdjmRecordBridgeActor* InBridgeActor,
	EVdjmRecordBridgeInitStep PrevStep,
	EVdjmRecordBridgeInitStep CurrentStep)
{
	OnManagerObservedChainEvent.Broadcast(InBridgeActor, PrevStep, CurrentStep);
	ApplySessionStateByBridgeSnapshot();
}

void UVdjmRecordEventManager::HandleBridgeInitComplete(AVdjmRecordBridgeActor* InBridgeActor)
{
	(void)InBridgeActor;
	ApplySessionStateByBridgeSnapshot();
}

void UVdjmRecordEventManager::HandleBridgeInitErrorEnd(AVdjmRecordBridgeActor* InBridgeActor)
{
	(void)InBridgeActor;
	TransitionSessionState(EVdjmRecorderSessionState::EFailed, TEXT("Bridge initialization failed."));
}

void UVdjmRecordEventManager::HandleBridgeRecordStarted(UVdjmRecordResource* InRecordResource)
{
	(void)InRecordResource;
	bPendingFinalization = false;
	TransitionSessionState(EVdjmRecorderSessionState::ERecording);
}

void UVdjmRecordEventManager::HandleBridgeRecordStopped(UVdjmRecordResource* InRecordResource)
{
	(void)InRecordResource;
	bPendingFinalization = false;
	TransitionSessionState(EVdjmRecorderSessionState::ETerminated);
}

void UVdjmRecordEventManager::ApplySessionStateByBridgeSnapshot()
{
	AVdjmRecordBridgeActor* bridgeActor = WeakBridgeActor.Get();
	if (bridgeActor == nullptr)
	{
		if (CurrentSessionState != EVdjmRecorderSessionState::EFailed)
		{
			TransitionSessionState(EVdjmRecorderSessionState::ENew);
		}
		return;
	}

	if (bridgeActor->IsRecording())
	{
		bPendingFinalization = false;
		TransitionSessionState(EVdjmRecorderSessionState::ERecording);
		return;
	}

	const EVdjmRecordBridgeInitStep currentInitStep = bridgeActor->GetCurrentInitStep();
	if (currentInitStep == EVdjmRecordBridgeInitStep::EInitError ||
		currentInitStep == EVdjmRecordBridgeInitStep::EInitErrorEnd)
	{
		TransitionSessionState(EVdjmRecorderSessionState::EFailed, TEXT("Bridge reported an initialization error."));
		return;
	}

	if (bPendingFinalization || CurrentSessionState == EVdjmRecorderSessionState::EFinalizing)
	{
		return;
	}

	if (currentInitStep == EVdjmRecordBridgeInitStep::EComplete)
	{
		TransitionSessionState(EVdjmRecorderSessionState::EReady);
		return;
	}

	if (currentInitStep != EVdjmRecordBridgeInitStep::EInitializeStart)
	{
		TransitionSessionState(EVdjmRecorderSessionState::EPreparing);
		return;
	}

	TransitionSessionState(EVdjmRecorderSessionState::ENew);
}

void UVdjmRecordEventManager::ResetSessionState(EVdjmRecorderSessionState NewState)
{
	CurrentSessionState = NewState;
	LastSessionTransitionSeconds = FPlatformTime::Seconds();
	LastSessionError.Reset();
}

void UVdjmRecordEventManager::TransitionSessionState(EVdjmRecorderSessionState NewState, const FString& InErrorMessage)
{
	if (CurrentSessionState == NewState && LastSessionError == InErrorMessage)
	{
		return;
	}

	const EVdjmRecorderSessionState previousState = CurrentSessionState;
	CurrentSessionState = NewState;
	LastSessionTransitionSeconds = FPlatformTime::Seconds();
	LastSessionError = InErrorMessage;
	OnSessionStateChanged.Broadcast(this, previousState, CurrentSessionState, LastSessionTransitionSeconds);
}
