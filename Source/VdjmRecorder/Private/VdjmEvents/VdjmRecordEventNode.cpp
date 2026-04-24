#include "VdjmEvents/VdjmRecordEventNode.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "VdjmRecordBridgeActor.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmEvents/VdjmRecordEventWidgetBase.h"
#include "VdjmRecorderCore.h"
#include "VdjmRecorderWorldContextSubsystem.h"

namespace
{
	UObject* ResolveEventObjectOuter(
		UVdjmRecordEventManager* eventManager,
		AVdjmRecordBridgeActor* bridgeActor,
		EVdjmRecordEventObjectOuterPolicy outerPolicy)
	{
		if (outerPolicy == EVdjmRecordEventObjectOuterPolicy::EBridgeActor && bridgeActor != nullptr)
		{
			return bridgeActor;
		}

		if (outerPolicy == EVdjmRecordEventObjectOuterPolicy::EEventManager && eventManager != nullptr)
		{
			return eventManager;
		}

		if (outerPolicy == EVdjmRecordEventObjectOuterPolicy::EWorldContextSubsystem)
		{
			if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(eventManager))
			{
				return worldContextSubsystem;
			}
		}

		if (outerPolicy == EVdjmRecordEventObjectOuterPolicy::ETransientPackage)
		{
			return GetTransientPackage();
		}

		if (bridgeActor != nullptr)
		{
			return bridgeActor;
		}

		if (eventManager != nullptr)
		{
			return eventManager;
		}

		if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(eventManager))
		{
			return worldContextSubsystem;
		}

		return GetTransientPackage();
	}

	bool RegisterContextObject(
		UVdjmRecordEventManager* eventManager,
		UObject* contextObject,
		FName contextKey,
		UClass* expectedClass)
	{
		if (eventManager == nullptr || contextObject == nullptr || contextKey.IsNone())
		{
			return false;
		}

		UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(eventManager);
		if (worldContextSubsystem == nullptr)
		{
			return false;
		}

		UClass* resolvedExpectedClass = expectedClass != nullptr ? expectedClass : contextObject->GetClass();
		return worldContextSubsystem->RegisterWeakObjectContext(contextKey, contextObject, resolvedExpectedClass);
	}
}

FVdjmRecordEventResult UVdjmRecordEventBase::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventBase::ResetRuntimeState_Implementation()
{
}

FVdjmRecordEventResult UVdjmRecordEventBase::MakeResult(EVdjmRecordEventResultType InResultType, int32 InSelectedIndex, FName InJumpLabel, float InWaitSeconds)
{
	FVdjmRecordEventResult Result;
	Result.ResultType = InResultType;
	Result.SelectedIndex = InSelectedIndex;
	Result.JumpLabel = InJumpLabel;
	Result.WaitSeconds = FMath::Max(0.0f, InWaitSeconds);
	return Result;
}

FVdjmRecordEventResult UVdjmRecordEventSequenceNode::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	if (EventManager == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const FVdjmRecordFlowHandle childFlowHandle = EventManager->FindOrCreateChildFlowHandle(this);
	FVdjmRecordFlowStepContext stepContext;
	stepContext.FlowHandle = childFlowHandle;
	stepContext.BridgeActor = BridgeActor;
	stepContext.Events = &Children;
	stepContext.CurrentIndex = &RuntimeChildIndex;

	const FVdjmRecordFlowStepResult stepResult = EventManager->ExecuteFlowLoop(stepContext);
	switch (stepResult.Disposition)
	{
	case EVdjmRecordFlowStepDisposition::EContinue:
	case EVdjmRecordFlowStepDisposition::EYield:
		return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, stepResult.WaitSeconds);
	case EVdjmRecordFlowStepDisposition::EFinishSuccess:
		ResetRuntimeState();
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	case EVdjmRecordFlowStepDisposition::EFinishAbort:
		ResetRuntimeState();
		return MakeResult(EVdjmRecordEventResultType::EAbort, INDEX_NONE, NAME_None, 0.0f);
	case EVdjmRecordFlowStepDisposition::EFinishFailure:
		ResetRuntimeState();
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	default:
		ResetRuntimeState();
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}
}

void UVdjmRecordEventSequenceNode::ResetRuntimeState_Implementation()
{
	RuntimeChildIndex = 0;
	for (UVdjmRecordEventBase* childEvent : Children)
	{
		if (childEvent != nullptr)
		{
			childEvent->ResetRuntimeState();
		}
	}
}

FVdjmRecordEventResult UVdjmRecordEventJumpToNextNode::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	if (EventManager == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const int32 nextIndex = EventManager->FindNextEventIndex(this, TargetClass, TargetTag);
	if (nextIndex != INDEX_NONE)
	{
		return MakeResult(EVdjmRecordEventResultType::ESelectIndex, nextIndex, NAME_None, 0.0f);
	}

	return MakeResult(bAbortIfNotFound ? EVdjmRecordEventResultType::EAbort : EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventLogNode::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	(void)EventManager;
	(void)BridgeActor;

	const FString safeMessage = Message.IsEmpty()
		? TEXT("UVdjmRecordEventLogNode executed.")
		: Message;

	if (bLogAsWarning)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("EventLog(%s)"), *safeMessage);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("EventLog(%s)"), *safeMessage);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventSpawnBridgeActorNode::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	if (EventManager == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UWorld* World = EventManager->GetManagerWorld();
	if (World == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bReuseExistingBridgeActor)
	{
		if (AVdjmRecordBridgeActor* ExistingBridge = AVdjmRecordBridgeActor::TryGetRecordBridgeActor(World))
		{
			EventManager->BindBridge(ExistingBridge);
			return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	TSubclassOf<AVdjmRecordBridgeActor> SpawnClass = BridgeActorClass ? BridgeActorClass.Get() : AVdjmRecordBridgeActor::StaticClass();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVdjmRecordBridgeActor* SpawnedBridge = World->SpawnActor<AVdjmRecordBridgeActor>(SpawnClass, FTransform::Identity, SpawnParameters);
	if (SpawnedBridge == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	EventManager->BindBridge(SpawnedBridge);
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventSpawnRecordBridgeActorWait::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	AVdjmRecordBridgeActor* TargetBridgeActor = nullptr;
	if (not ResolveRuntimeBridge(EventManager, TargetBridgeActor) || TargetBridgeActor == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const EVdjmRecordBridgeInitStep CurrentInitStep = TargetBridgeActor->GetCurrentInitStep();
	if (CanTreatAsInitSuccess(TargetBridgeActor))
	{
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	if (IsInitFailureStep(CurrentInitStep))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not bHasIssuedStart)
	{
		if (CurrentInitStep != EVdjmRecordBridgeInitStep::EInitializeStart)
		{
			bHasIssuedStart = true;
		}
		else if (StartPolicy == EVdjmRecordEventBridgeStartPolicy::EStartImmediately)
		{
			TargetBridgeActor->StartRecordBridgeActor();
			bHasIssuedStart = true;
		}
		else
		{
			if (StartSignalTag.IsNone())
			{
				return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
			}

			if (not EventManager->ConsumeFlowSignal(StartSignalTag))
			{
				return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, 0.0f);
			}

			TargetBridgeActor->StartRecordBridgeActor();
			bHasIssuedStart = true;
		}
	}

	if (CanTreatAsInitSuccess(TargetBridgeActor))
	{
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	if (IsInitFailureStep(TargetBridgeActor->GetCurrentInitStep()))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventSpawnRecordBridgeActorWait::ResetRuntimeState_Implementation()
{
	RuntimeBridgeActor.Reset();
	bHasIssuedStart = false;
}

FVdjmRecordEventResult UVdjmRecordEventCreateObjectNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	if (EventManager == nullptr || ObjectClass == nullptr || RuntimeSlotKey.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bReuseSlotObject)
	{
		if (UObject* existingObject = EventManager->FindRuntimeObjectSlot(RuntimeSlotKey))
		{
			if (existingObject->IsA(ObjectClass))
			{
				return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
			}
		}
	}

	UObject* outerObject = ResolveEventObjectOuter(EventManager, BridgeActor, OuterPolicy);
	if (outerObject == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UObject* createdObject = NewObject<UObject>(outerObject, ObjectClass);
	if (createdObject == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, createdObject))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventSpawnActorNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || ActorClass == nullptr || RuntimeSlotKey.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bReuseSlotActor)
	{
		if (AActor* existingActor = Cast<AActor>(EventManager->FindRuntimeObjectSlot(RuntimeSlotKey)))
		{
			if (existingActor->IsA(ActorClass))
			{
				return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
			}
		}
	}

	UWorld* world = EventManager->GetManagerWorld();
	if (world == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	FActorSpawnParameters spawnParameters;
	spawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* spawnedActor = world->SpawnActor<AActor>(ActorClass, FTransform::Identity, spawnParameters);
	if (spawnedActor == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, spawnedActor))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventRegisterContextEntryNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || RuntimeSlotKey.IsNone() || ContextKey.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UObject* contextObject = EventManager->FindRuntimeObjectSlot(RuntimeSlotKey);
	if (contextObject == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not RegisterContextObject(EventManager, contextObject, ContextKey, ExpectedClass.Get()))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventRegisterWidgetContextNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || RuntimeSlotKey.IsNone() || ContextKey.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UUserWidget* widgetInstance = Cast<UUserWidget>(EventManager->FindRuntimeObjectSlot(RuntimeSlotKey));
	if (widgetInstance == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not RegisterContextObject(EventManager, widgetInstance, ContextKey, widgetInstance->GetClass()))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

bool UVdjmRecordEventSpawnRecordBridgeActorWait::ResolveRuntimeBridge(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor*& OutBridgeActor)
{
	OutBridgeActor = RuntimeBridgeActor.Get();
	if (OutBridgeActor == nullptr)
	{
		UWorld* world = EventManager != nullptr ? EventManager->GetManagerWorld() : nullptr;
		if (world == nullptr)
		{
			return false;
		}

		if (bReuseExistingBridgeActor)
		{
			OutBridgeActor = AVdjmRecordBridgeActor::TryGetRecordBridgeActor(world);
		}

		if (OutBridgeActor == nullptr)
		{
			TSubclassOf<AVdjmRecordBridgeActor> spawnClass = BridgeActorClass ? BridgeActorClass.Get() : AVdjmRecordBridgeActor::StaticClass();
			FActorSpawnParameters spawnParameters;
			spawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			OutBridgeActor = world->SpawnActor<AVdjmRecordBridgeActor>(spawnClass, FTransform::Identity, spawnParameters);
			if (OutBridgeActor == nullptr)
			{
				return false;
			}
		}

		RuntimeBridgeActor = OutBridgeActor;
	}

	if (not ApplyBridgePreStartSettings(OutBridgeActor))
	{
		return false;
	}

	return EventManager != nullptr && EventManager->BindBridge(OutBridgeActor);
}

bool UVdjmRecordEventSpawnRecordBridgeActorWait::ApplyBridgePreStartSettings(AVdjmRecordBridgeActor* InBridgeActor) const
{
	if (InBridgeActor == nullptr)
	{
		return false;
	}

	if (not EnvDataAssetPath.IsNull())
	{
		if (bRequireLoadSuccess)
		{
			UObject* loadedObject = EnvDataAssetPath.TryLoad();
			if (not IsValid(Cast<UVdjmRecordEnvDataAsset>(loadedObject)))
			{
				return false;
			}
		}

		AVdjmRecordBridgeActor::SetRecordEnvDataAssetPath(EnvDataAssetPath);
	}

	return true;
}

bool UVdjmRecordEventSpawnRecordBridgeActorWait::CanTreatAsInitSuccess(const AVdjmRecordBridgeActor* InBridgeActor) const
{
	if (InBridgeActor == nullptr)
	{
		return false;
	}

	return InBridgeActor->GetCurrentInitStep() == EVdjmRecordBridgeInitStep::EComplete;
}

bool UVdjmRecordEventSpawnRecordBridgeActorWait::IsInitFailureStep(EVdjmRecordBridgeInitStep InInitStep) const
{
	return InInitStep == EVdjmRecordBridgeInitStep::EInitError ||
		InInitStep == EVdjmRecordBridgeInitStep::EInitErrorEnd;
}

FVdjmRecordEventResult UVdjmRecordEventCreateWidgetNode::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	if (EventManager == nullptr || WidgetClass == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UWorld* world = EventManager->GetManagerWorld();
	if (world == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	APlayerController* owningPlayer = UGameplayStatics::GetPlayerController(world, PlayerIndex);
	if (bRequireOwningPlayer && owningPlayer == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UUserWidget* widgetInstance = bReuseCreatedWidget ? RuntimeWidget.Get() : nullptr;
	if (widgetInstance == nullptr)
	{
		if (owningPlayer != nullptr)
		{
			widgetInstance = CreateWidget<UUserWidget>(owningPlayer, WidgetClass);
		}
		else
		{
			widgetInstance = CreateWidget<UUserWidget>(world, WidgetClass);
		}

		if (widgetInstance == nullptr)
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		RuntimeWidget = widgetInstance;
	}

	if (UVdjmRecordEventWidgetBase* eventWidgetBase = Cast<UVdjmRecordEventWidgetBase>(widgetInstance))
	{
		eventWidgetBase->ApplyEventContext(EventManager, BridgeActor);
	}

	if (bAddToViewport && not widgetInstance->IsInViewport())
	{
		widgetInstance->AddToViewport(ZOrder);
	}

	if (not RuntimeSlotKey.IsNone())
	{
		if (not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, widgetInstance))
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	if (not bHasEmittedSignal && not EmitSignalTag.IsNone())
	{
		EventManager->EmitFlowSignal(EmitSignalTag);
		bHasEmittedSignal = true;
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventCreateWidgetNode::ResetRuntimeState_Implementation()
{
	bHasEmittedSignal = false;
	if (not bReuseCreatedWidget)
	{
		RuntimeWidget.Reset();
	}
}

FVdjmRecordEventResult UVdjmRecordEventRemoveWidgetNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || RuntimeSlotKey.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UUserWidget* widgetInstance = Cast<UUserWidget>(EventManager->FindRuntimeObjectSlot(RuntimeSlotKey));
	if (widgetInstance == nullptr)
	{
		return MakeResult(
			bSucceedIfMissing ? EVdjmRecordEventResultType::ESuccess : EVdjmRecordEventResultType::EFailure,
			INDEX_NONE,
			NAME_None,
			0.0f);
	}

	widgetInstance->RemoveFromParent();

	if (bUnregisterContext && not ContextKey.IsNone())
	{
		if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(EventManager))
		{
			worldContextSubsystem->UnregisterContext(ContextKey);
		}
	}

	if (bClearRuntimeSlot)
	{
		EventManager->ClearRuntimeObjectSlot(RuntimeSlotKey);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventWaitForSignalNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || SignalTag.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not EventManager->ConsumeFlowSignal(SignalTag))
	{
		return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventDelayNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UWorld* world = EventManager->GetManagerWorld();
	if (world == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (DelaySeconds <= 0.0f)
	{
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not bDelayStarted)
	{
		bDelayStarted = true;
		DelayStartSeconds = world->GetTimeSeconds();
		return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, DelaySeconds);
	}

	const double elapsedSeconds = world->GetTimeSeconds() - DelayStartSeconds;
	if (elapsedSeconds < DelaySeconds)
	{
		return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, DelaySeconds - static_cast<float>(elapsedSeconds));
	}

	bDelayStarted = false;
	DelayStartSeconds = 0.0;
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventDelayNode::ResetRuntimeState_Implementation()
{
	bDelayStarted = false;
	DelayStartSeconds = 0.0;
}

FVdjmRecordEventResult UVdjmRecordEventEmitSignalNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || SignalTag.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not EventManager->EmitFlowSignalByScope(SignalTag, SignalScope))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventCreateObjectAndRegisterContextNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	const FVdjmRecordEventResult createResult = Super::ExecuteEvent_Implementation(EventManager, BridgeActor);
	if (createResult.ResultType != EVdjmRecordEventResultType::ESuccess)
	{
		return createResult;
	}

	UObject* contextObject = EventManager != nullptr ? EventManager->FindRuntimeObjectSlot(RuntimeSlotKey) : nullptr;
	if (contextObject == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not RegisterContextObject(EventManager, contextObject, ContextKey, ExpectedClass.Get()))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventSpawnActorAndRegisterContextNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	const FVdjmRecordEventResult spawnResult = Super::ExecuteEvent_Implementation(EventManager, BridgeActor);
	if (spawnResult.ResultType != EVdjmRecordEventResultType::ESuccess)
	{
		return spawnResult;
	}

	UObject* contextObject = EventManager != nullptr ? EventManager->FindRuntimeObjectSlot(RuntimeSlotKey) : nullptr;
	if (contextObject == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not RegisterContextObject(EventManager, contextObject, ContextKey, ExpectedClass.Get()))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventCreateWidgetAndRegisterContextNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	const FVdjmRecordEventResult createResult = Super::ExecuteEvent_Implementation(EventManager, BridgeActor);
	if (createResult.ResultType != EVdjmRecordEventResultType::ESuccess)
	{
		return createResult;
	}

	UUserWidget* widgetInstance = EventManager != nullptr ? Cast<UUserWidget>(EventManager->FindRuntimeObjectSlot(RuntimeSlotKey)) : nullptr;
	if (widgetInstance == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not RegisterContextObject(EventManager, widgetInstance, ContextKey, widgetInstance->GetClass()))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventSetEnvDataAssetPathNode::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	if (not EnvDataAssetPath.IsValid())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bRequireLoadSuccess)
	{
		UObject* loadedObject = EnvDataAssetPath.TryLoad();
		if (not IsValid(Cast<UVdjmRecordEnvDataAsset>(loadedObject)))
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	AVdjmRecordBridgeActor::SetRecordEnvDataAssetPath(EnvDataAssetPath);
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}
