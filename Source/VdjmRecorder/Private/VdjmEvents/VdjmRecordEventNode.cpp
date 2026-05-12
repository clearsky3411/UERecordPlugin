#include "VdjmEvents/VdjmRecordEventNode.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "VdjmRecordAppState.h"
#include "VdjmRecordBridgeActor.h"
#include "VdjmRecordMediaPreview.h"
#include "VdjmEvents/VdjmRecordEventFlowRuntime.h"
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

	FString GetEventManifestClassName(const UVdjmRecordEventBase* eventNode)
	{
		return eventNode != nullptr && eventNode->GetClass() != nullptr
			? eventNode->GetClass()->GetPathName()
			: FString();
	}

	UUserWidget* FindRuntimeWidgetBySlot(
		UVdjmRecordEventManager* eventManager,
		FName runtimeSlotKey)
	{
		if (eventManager == nullptr || runtimeSlotKey.IsNone())
		{
			return nullptr;
		}

		return Cast<UUserWidget>(eventManager->FindRuntimeObjectSlot(runtimeSlotKey));
	}

	struct FVdjmRecordWidgetLookupResult
	{
		UUserWidget* Widget = nullptr;
		FName RuntimeSlotKey = NAME_None;
		FName ContextKey = NAME_None;
		bool bFoundFromRuntimeSlot = false;
		bool bFoundFromContext = false;
	};

	FName ResolveWidgetContextLookupKey(FName runtimeSlotKey, FName contextKey)
	{
		return not contextKey.IsNone() ? contextKey : runtimeSlotKey;
	}

	UObject* FindObjectByContext(
		UVdjmRecordEventManager* eventManager,
		FName contextKey)
	{
		if (eventManager == nullptr || contextKey.IsNone())
		{
			return nullptr;
		}

		UVdjmRecorderWorldContextSubsystem* worldContextSubsystem =
			UVdjmRecorderWorldContextSubsystem::Get(eventManager);
		if (worldContextSubsystem == nullptr)
		{
			return nullptr;
		}

		return worldContextSubsystem->FindContextObject(contextKey);
	}

	struct FVdjmRecordObjectLookupResult
	{
		UObject* Object = nullptr;
		FName RuntimeSlotKey = NAME_None;
		FName ContextKey = NAME_None;
		bool bFoundFromRuntimeSlot = false;
		bool bFoundFromContext = false;
	};

	FName ResolveObjectContextLookupKey(
		FName runtimeSlotKey,
		FName sourceContextKey,
		FName contextKey)
	{
		if (not sourceContextKey.IsNone())
		{
			return sourceContextKey;
		}

		if (not contextKey.IsNone())
		{
			return contextKey;
		}

		return runtimeSlotKey;
	}

	bool TryResolveObjectByRuntimeSlot(
		UVdjmRecordEventManager* eventManager,
		FName runtimeSlotKey,
		FVdjmRecordObjectLookupResult& outLookupResult)
	{
		if (eventManager == nullptr || runtimeSlotKey.IsNone())
		{
			return false;
		}

		UObject* object = eventManager->FindRuntimeObjectSlot(runtimeSlotKey);
		if (object == nullptr)
		{
			return false;
		}

		outLookupResult.Object = object;
		outLookupResult.RuntimeSlotKey = runtimeSlotKey;
		outLookupResult.bFoundFromRuntimeSlot = true;
		return true;
	}

	bool TryResolveObjectByContext(
		UVdjmRecordEventManager* eventManager,
		FName runtimeSlotKey,
		FName sourceContextKey,
		FName contextKey,
		FVdjmRecordObjectLookupResult& outLookupResult)
	{
		const FName resolvedContextKey = ResolveObjectContextLookupKey(runtimeSlotKey, sourceContextKey, contextKey);
		UObject* object = FindObjectByContext(eventManager, resolvedContextKey);
		if (object == nullptr)
		{
			return false;
		}

		outLookupResult.Object = object;
		outLookupResult.ContextKey = resolvedContextKey;
		outLookupResult.bFoundFromContext = true;
		return true;
	}

	FVdjmRecordObjectLookupResult ResolveObjectByLookupPolicy(
		UVdjmRecordEventManager* eventManager,
		FName runtimeSlotKey,
		FName sourceContextKey,
		FName contextKey,
		EVdjmRecordEventObjectLookupPolicy lookupPolicy)
	{
		FVdjmRecordObjectLookupResult lookupResult;
		switch (lookupPolicy)
		{
		case EVdjmRecordEventObjectLookupPolicy::ERuntimeSlotOnly:
			TryResolveObjectByRuntimeSlot(eventManager, runtimeSlotKey, lookupResult);
			break;
		case EVdjmRecordEventObjectLookupPolicy::EContextOnly:
			TryResolveObjectByContext(eventManager, runtimeSlotKey, sourceContextKey, contextKey, lookupResult);
			break;
		case EVdjmRecordEventObjectLookupPolicy::EContextThenRuntime:
			if (not TryResolveObjectByContext(eventManager, runtimeSlotKey, sourceContextKey, contextKey, lookupResult))
			{
				TryResolveObjectByRuntimeSlot(eventManager, runtimeSlotKey, lookupResult);
			}
			break;
		case EVdjmRecordEventObjectLookupPolicy::ERuntimeSlotThenContext:
		default:
			if (not TryResolveObjectByRuntimeSlot(eventManager, runtimeSlotKey, lookupResult))
			{
				TryResolveObjectByContext(eventManager, runtimeSlotKey, sourceContextKey, contextKey, lookupResult);
			}
			break;
		}

		return lookupResult;
	}

	bool IsObjectCompatibleWithExpectedClass(UObject* object, UClass* expectedClass)
	{
		return object != nullptr && (expectedClass == nullptr || object->IsA(expectedClass));
	}

	FString GetEnumNameString(const UEnum* enumType, int64 enumValue)
	{
		return enumType != nullptr
			? enumType->GetNameStringByValue(enumValue)
			: FString::FromInt(static_cast<int32>(enumValue));
	}

	FString GetWidgetVisibilityString(ESlateVisibility visibility)
	{
		return GetEnumNameString(StaticEnum<ESlateVisibility>(), static_cast<int64>(visibility));
	}

	FString GetWidgetLookupPolicyString(EVdjmRecordEventWidgetLookupPolicy lookupPolicy)
	{
		return GetEnumNameString(StaticEnum<EVdjmRecordEventWidgetLookupPolicy>(), static_cast<int64>(lookupPolicy));
	}

	FString JoinNameArray(const TArray<FName>& nameArray)
	{
		TArray<FString> nameStrings;
		nameStrings.Reserve(nameArray.Num());
		for (const FName nameValue : nameArray)
		{
			nameStrings.Add(nameValue.ToString());
		}

		return FString::Join(nameStrings, TEXT(","));
	}

	FString GetWidgetDebugName(const UUserWidget* widget)
	{
		return FString::Printf(
			TEXT("%s(%p)"),
			*GetNameSafe(widget),
			widget);
	}

	UUserWidget* FindWidgetByContext(
		UVdjmRecordEventManager* eventManager,
		FName contextKey)
	{
		return Cast<UUserWidget>(FindObjectByContext(eventManager, contextKey));
	}

	bool TryResolveWidgetByRuntimeSlot(
		UVdjmRecordEventManager* eventManager,
		FName runtimeSlotKey,
		FVdjmRecordWidgetLookupResult& outLookupResult)
	{
		UUserWidget* widget = FindRuntimeWidgetBySlot(eventManager, runtimeSlotKey);
		if (widget == nullptr)
		{
			return false;
		}

		outLookupResult.Widget = widget;
		outLookupResult.RuntimeSlotKey = runtimeSlotKey;
		outLookupResult.bFoundFromRuntimeSlot = true;
		return true;
	}

	bool TryResolveWidgetByContext(
		UVdjmRecordEventManager* eventManager,
		FName runtimeSlotKey,
		FName contextKey,
		FVdjmRecordWidgetLookupResult& outLookupResult)
	{
		const FName resolvedContextKey = ResolveWidgetContextLookupKey(runtimeSlotKey, contextKey);
		UUserWidget* widget = FindWidgetByContext(eventManager, resolvedContextKey);
		if (widget == nullptr)
		{
			return false;
		}

		outLookupResult.Widget = widget;
		outLookupResult.ContextKey = resolvedContextKey;
		outLookupResult.bFoundFromContext = true;
		return true;
	}

	FVdjmRecordWidgetLookupResult ResolveWidgetByLookupPolicy(
		UVdjmRecordEventManager* eventManager,
		FName runtimeSlotKey,
		FName contextKey,
		EVdjmRecordEventWidgetLookupPolicy lookupPolicy)
	{
		FVdjmRecordWidgetLookupResult lookupResult;
		switch (lookupPolicy)
		{
		case EVdjmRecordEventWidgetLookupPolicy::ERuntimeSlotOnly:
			TryResolveWidgetByRuntimeSlot(eventManager, runtimeSlotKey, lookupResult);
			break;
		case EVdjmRecordEventWidgetLookupPolicy::EContextOnly:
			TryResolveWidgetByContext(eventManager, runtimeSlotKey, contextKey, lookupResult);
			break;
		case EVdjmRecordEventWidgetLookupPolicy::EContextThenRuntime:
			if (not TryResolveWidgetByContext(eventManager, runtimeSlotKey, contextKey, lookupResult))
			{
				TryResolveWidgetByRuntimeSlot(eventManager, runtimeSlotKey, lookupResult);
			}
			break;
		case EVdjmRecordEventWidgetLookupPolicy::ERuntimeSlotThenContext:
		default:
			if (not TryResolveWidgetByRuntimeSlot(eventManager, runtimeSlotKey, lookupResult))
			{
				TryResolveWidgetByContext(eventManager, runtimeSlotKey, contextKey, lookupResult);
			}
			break;
		}

		return lookupResult;
	}
}

FVdjmRecordEventResult UVdjmRecordEventBase::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventBase::ResetRuntimeState_Implementation()
{
}

void UVdjmRecordEventBase::CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const
{
	manifest.AddEventNode(eventIndex, EventTag, GetEventManifestClassName(this));
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

void UVdjmRecordEventSequenceNode::CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);
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
	if (StartPolicy == EVdjmRecordEventBridgeStartPolicy::EPrepareOnly)
	{
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

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
				if (ConditionMode != EVdjmRecordEventConditionMode::ERunning &&
					not EventManager->RequestCurrentFlowConditionForSignal(StartSignalTag, ConditionMode))
				{
					return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
				}

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

	if (ConditionMode != EVdjmRecordEventConditionMode::ERunning &&
		not EventManager->RequestCurrentFlowConditionForBridgeInit(ConditionMode))
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

void UVdjmRecordEventSpawnRecordBridgeActorWait::CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);

	if (StartPolicy == EVdjmRecordEventBridgeStartPolicy::EWaitForSignal)
	{
		manifest.AddSignalWaiter(StartSignalTag, eventIndex, EventTag, GetEventManifestClassName(this), true);
	}
}

FVdjmRecordEventResult UVdjmRecordEventStartRecordBridgeActorNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	AVdjmRecordBridgeActor* targetBridgeActor = BridgeActor;
	if (targetBridgeActor == nullptr && EventManager != nullptr)
	{
		targetBridgeActor = EventManager->GetBoundBridge();
	}

	if (targetBridgeActor == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const EVdjmRecordBridgeInitStep currentInitStep = targetBridgeActor->GetCurrentInitStep();
	if (currentInitStep == EVdjmRecordBridgeInitStep::EComplete)
	{
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	if (currentInitStep != EVdjmRecordBridgeInitStep::EInitializeStart)
	{
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	targetBridgeActor->StartRecordBridgeActor();
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventStartRecordBridgeActorNode::CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);
}

FVdjmRecordEventResult UVdjmRecordEventEnsureRecorderControllerNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	LastErrorReason.Reset();

	UObject* worldContextObject = EventManager != nullptr ? Cast<UObject>(EventManager) : Cast<UObject>(BridgeActor);
	if (worldContextObject == nullptr)
	{
		LastErrorReason = TEXT("World context object is not available.");
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UVdjmRecorderController* recorderController = UVdjmRecorderController::FindOrCreateRecorderController(worldContextObject);
	if (recorderController == nullptr)
	{
		LastErrorReason = TEXT("Recorder controller is not available.");
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bStoreRuntimeSlot && not RuntimeSlotKey.IsNone())
	{
		if (EventManager == nullptr || not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, recorderController))
		{
			LastErrorReason = TEXT("Failed to store recorder controller in runtime slot.");
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	if (bRegisterContext)
	{
		const FName resolvedContextKey = ContextKey.IsNone()
			? UVdjmRecorderWorldContextSubsystem::GetRecorderControllerContextKey()
			: ContextKey;
		if (not RegisterContextObject(EventManager, recorderController, resolvedContextKey, UVdjmRecorderController::StaticClass()))
		{
			LastErrorReason = TEXT("Failed to register recorder controller context.");
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventEnsureRecorderControllerNode::CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);
}

FVdjmRecordEventResult UVdjmRecordEventLoadAppStateNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	LastErrorReason.Reset();

	UObject* worldContextObject = EventManager != nullptr ? Cast<UObject>(EventManager) : Cast<UObject>(BridgeActor);
	if (worldContextObject == nullptr)
	{
		LastErrorReason = TEXT("World context object is not available.");
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UVdjmRecordAppStateStore* appStateStore = RuntimeAppStateStore.Get();
	if (not bHasLoadedAppState)
	{
		appStateStore = UVdjmRecordAppStateStore::FindOrCreateAppStateStore(worldContextObject);
		if (appStateStore == nullptr)
		{
			LastErrorReason = TEXT("AppState store is not available.");
			ResetRuntimeState();
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		if (not appStateStore->LoadAppState(LastErrorReason, bCreateIfMissing))
		{
			ResetRuntimeState();
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		RuntimeAppStateStore = appStateStore;
		bHasLoadedAppState = true;
	}

	if (appStateStore == nullptr)
	{
		LastErrorReason = TEXT("AppState store runtime reference is invalid.");
		ResetRuntimeState();
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bRefreshRecordsToc)
	{
		UVdjmRecordMetadataStore* metadataStore = RuntimeMetadataStore.Get();
		if (not bHasStartedRegistryScan)
		{
			metadataStore = UVdjmRecordMetadataStore::FindOrCreateMetadataStore(worldContextObject);
			if (metadataStore == nullptr)
			{
				LastErrorReason = TEXT("Metadata store is not available.");
				ResetRuntimeState();
				return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
			}

			FVdjmRecordMetadataRegistryScanRequest scanRequest;
			scanRequest.MaxManifestFilesPerStep = MaxManifestFilesPerStep;
			scanRequest.MaxRegistryEntriesPerStep = MaxRegistryEntryStateChecksPerStep;
			scanRequest.bSaveRegistryOnComplete = true;
			if (not metadataStore->StartRegistryScanFromDisk(scanRequest, LastErrorReason))
			{
				ResetRuntimeState();
				return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
			}

			RuntimeMetadataStore = metadataStore;
			bHasStartedRegistryScan = true;
			return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, 0.0f);
		}

		if (metadataStore == nullptr)
		{
			LastErrorReason = TEXT("Metadata store runtime reference is invalid.");
			ResetRuntimeState();
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		const EVdjmRecordMetadataRegistryScanRunResult scanRunResult =
			metadataStore->AdvanceRegistryScanFromDisk(LastErrorReason);
		if (scanRunResult == EVdjmRecordMetadataRegistryScanRunResult::ERunning)
		{
			return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, 0.0f);
		}

		if (scanRunResult == EVdjmRecordMetadataRegistryScanRunResult::EFailed)
		{
			if (LastErrorReason.IsEmpty())
			{
				LastErrorReason = TEXT("Failed to refresh media registry from disk.");
			}
			ResetRuntimeState();
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		if (not appStateStore->RefreshRecordsTocFromMetadataStore(metadataStore, LastErrorReason))
		{
			ResetRuntimeState();
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	if (bSaveAfterRefresh && not appStateStore->SaveAppState(LastErrorReason))
	{
		ResetRuntimeState();
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bStoreRuntimeSlot && not RuntimeSlotKey.IsNone())
	{
		if (EventManager == nullptr || not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, appStateStore))
		{
			LastErrorReason = TEXT("Failed to store AppState store in runtime slot.");
			ResetRuntimeState();
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	if (bRegisterContext)
	{
		const FName resolvedContextKey = ContextKey.IsNone()
			? UVdjmRecorderWorldContextSubsystem::GetAppStateStoreContextKey()
			: ContextKey;
		if (not RegisterContextObject(EventManager, appStateStore, resolvedContextKey, UVdjmRecordAppStateStore::StaticClass()))
		{
			LastErrorReason = TEXT("Failed to register AppState store context.");
			ResetRuntimeState();
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	ResetRuntimeState();
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventLoadAppStateNode::ResetRuntimeState_Implementation()
{
	RuntimeAppStateStore.Reset();
	RuntimeMetadataStore.Reset();
	bHasLoadedAppState = false;
	bHasStartedRegistryScan = false;
}

void UVdjmRecordEventLoadAppStateNode::CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);
}

FVdjmRecordEventResult UVdjmRecordEventEnsureMediaPreviewManagerNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	LastErrorReason.Reset();

	UObject* worldContextObject = EventManager != nullptr ? Cast<UObject>(EventManager) : Cast<UObject>(BridgeActor);
	if (worldContextObject == nullptr)
	{
		LastErrorReason = TEXT("World context object is not available.");
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	AVdjmRecordMediaPreviewManagerActor* previewManager =
		AVdjmRecordMediaPreviewManagerActor::FindOrSpawnMediaPreviewManagerActor(worldContextObject);
	if (previewManager == nullptr)
	{
		LastErrorReason = TEXT("Media preview manager is not available.");
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bStoreRuntimeSlot && not RuntimeSlotKey.IsNone())
	{
		if (EventManager == nullptr || not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, previewManager))
		{
			LastErrorReason = TEXT("Failed to store media preview manager in runtime slot.");
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	if (bRegisterContext)
	{
		const FName resolvedContextKey = ContextKey.IsNone()
			? UVdjmRecorderWorldContextSubsystem::GetMediaPreviewManagerContextKey()
			: ContextKey;
		if (not RegisterContextObject(
			EventManager,
			previewManager,
			resolvedContextKey,
			AVdjmRecordMediaPreviewManagerActor::StaticClass()))
		{
			LastErrorReason = TEXT("Failed to register media preview manager context.");
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventEnsureMediaPreviewManagerNode::CollectFlowManifest(
	FVdjmRecordEventFlowManifest& manifest,
	int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);
}

FVdjmRecordEventResult UVdjmRecordEventInitializeMediaPreviewManagerNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	LastErrorReason.Reset();

	AVdjmRecordMediaPreviewManagerActor* previewManager = RuntimePreviewManager.Get();
	if (not bHasStartedPreviewManagerInit)
	{
		UObject* worldContextObject = EventManager != nullptr ? Cast<UObject>(EventManager) : Cast<UObject>(BridgeActor);
		if (worldContextObject == nullptr)
		{
			LastErrorReason = TEXT("World context object is not available.");
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		if (EventManager != nullptr && not RuntimeSlotKey.IsNone())
		{
			previewManager = Cast<AVdjmRecordMediaPreviewManagerActor>(
				EventManager->FindRuntimeObjectSlot(RuntimeSlotKey));
		}

		if (previewManager == nullptr && bFindOrSpawnIfMissing)
		{
			previewManager = AVdjmRecordMediaPreviewManagerActor::FindOrSpawnMediaPreviewManagerActor(worldContextObject);
		}

		if (previewManager == nullptr)
		{
			LastErrorReason = TEXT("Media preview manager is not available.");
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		if (EventManager != nullptr && not RuntimeSlotKey.IsNone())
		{
			if (not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, previewManager))
			{
				LastErrorReason = TEXT("Failed to store media preview manager in runtime slot.");
				return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
			}
		}

		FVdjmRecordMediaPreviewInitRequest initRequest;
		initRequest.bForceRefresh = bForceRefresh;
		initRequest.bApplyCarouselWindowAfterInit = bApplyCarouselWindowAfterInit;
		initRequest.bSucceedWithEmptyRegistry = bSucceedWithEmptyRegistry;
		initRequest.SlotCount = SlotCount;
		initRequest.ActiveSlotIndex = ActiveSlotIndex;
		initRequest.InitialCenterSourceIndex = InitialCenterSourceIndex;
		initRequest.bAutoStartCenterPreview = bAutoStartCenterPreview;
		initRequest.MaxManifestFilesPerStep = MaxManifestFilesPerStep;
		initRequest.MaxRegistryEntryStateChecksPerStep = MaxRegistryEntryStateChecksPerStep;
		initRequest.MaxRegistryEntriesPerStep = MaxRegistryEntriesPerStep;

		if (not previewManager->StartPreviewManagerInit(initRequest, LastErrorReason))
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		RuntimePreviewManager = previewManager;
		bHasStartedPreviewManagerInit = true;
	}

	if (previewManager == nullptr)
	{
		LastErrorReason = TEXT("Media preview manager runtime reference is invalid.");
		ResetRuntimeState();
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const EVdjmRecordMediaPreviewInitRunResult runResult =
		previewManager->AdvancePreviewManagerInitStep(LastErrorReason);
	if (runResult == EVdjmRecordMediaPreviewInitRunResult::ERunning)
	{
		return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, 0.0f);
	}

	if (runResult == EVdjmRecordMediaPreviewInitRunResult::ESucceeded)
	{
		ResetRuntimeState();
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	if (LastErrorReason.IsEmpty())
	{
		LastErrorReason = previewManager->GetLastPreviewManagerErrorReason();
	}

	if (LastErrorReason.IsEmpty())
	{
		LastErrorReason = TEXT("Media preview manager init failed.");
	}
	ResetRuntimeState();
	return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventInitializeMediaPreviewManagerNode::ResetRuntimeState_Implementation()
{
	RuntimePreviewManager.Reset();
	bHasStartedPreviewManagerInit = false;
}

void UVdjmRecordEventInitializeMediaPreviewManagerNode::CollectFlowManifest(
	FVdjmRecordEventFlowManifest& manifest,
	int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);
}

FVdjmRecordEventResult UVdjmRecordEventSubmitRecorderOptionRequestNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	LastErrorReason.Reset();

	UObject* worldContextObject = EventManager != nullptr ? Cast<UObject>(EventManager) : Cast<UObject>(BridgeActor);
	UVdjmRecorderController* recorderController = UVdjmRecorderController::FindOrCreateRecorderController(worldContextObject);
	if (recorderController == nullptr)
	{
		LastErrorReason = TEXT("Recorder controller is not available.");
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not bHasSubmitted)
	{
		if (not recorderController->SubmitOptionRequest(OptionRequest, LastErrorReason))
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		bHasSubmitted = true;
	}

	if (bProcessPendingAfterSubmit)
	{
		FString processErrorReason;
		const bool bProcessResult = recorderController->ProcessPendingOptionRequests(processErrorReason);
		if (not bProcessResult && not recorderController->HasPendingOptionRequest())
		{
			LastErrorReason = processErrorReason.IsEmpty()
				? TEXT("Failed to process recorder option request.")
				: processErrorReason;
			bHasSubmitted = false;
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		if (not bProcessResult && not processErrorReason.IsEmpty())
		{
			LastErrorReason = processErrorReason;
		}
	}

	if (recorderController->HasPendingOptionRequest())
	{
		if (bSucceedIfQueued)
		{
			bHasSubmitted = false;
			return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
		}

		return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, 0.0f);
	}

	bHasSubmitted = false;
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventSubmitRecorderOptionRequestNode::ResetRuntimeState_Implementation()
{
	bHasSubmitted = false;
	LastErrorReason.Reset();
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

	if (EventManager == nullptr || ContextKey.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const FVdjmRecordObjectLookupResult lookupResult = ResolveObjectByLookupPolicy(
		EventManager,
		RuntimeSlotKey,
		SourceContextKey,
		ContextKey,
		LookupPolicy);
	if (lookupResult.Object == nullptr)
	{
		return MakeResult(
			bSucceedIfMissing ? EVdjmRecordEventResultType::ESuccess : EVdjmRecordEventResultType::EFailure,
			INDEX_NONE,
			NAME_None,
			0.0f);
	}

	UClass* expectedClass = ExpectedClass.Get();
	if (not IsObjectCompatibleWithExpectedClass(lookupResult.Object, expectedClass))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bRefreshRuntimeSlot && lookupResult.bFoundFromContext && not RuntimeSlotKey.IsNone())
	{
		if (not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, lookupResult.Object))
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	if (not RegisterContextObject(EventManager, lookupResult.Object, ContextKey, expectedClass))
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

	if (EventManager == nullptr || ContextKey.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const FVdjmRecordObjectLookupResult lookupResult = ResolveObjectByLookupPolicy(
		EventManager,
		RuntimeSlotKey,
		SourceContextKey,
		ContextKey,
		LookupPolicy);
	if (lookupResult.Object == nullptr)
	{
		return MakeResult(
			bSucceedIfMissing ? EVdjmRecordEventResultType::ESuccess : EVdjmRecordEventResultType::EFailure,
			INDEX_NONE,
			NAME_None,
			0.0f);
	}

	UUserWidget* widgetInstance = Cast<UUserWidget>(lookupResult.Object);
	if (widgetInstance == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bRefreshRuntimeSlot && lookupResult.bFoundFromContext && not RuntimeSlotKey.IsNone())
	{
		if (not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, widgetInstance))
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
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

	const bool shouldReuseCurrentExecutionWidget =
		bReuseCreatedWidget ||
		(DestroyPolicy != EVdjmRecordEventWidgetDestroyPolicy::ENone && bDestroyWaitStarted);
	UUserWidget* widgetInstance = shouldReuseCurrentExecutionWidget ? RuntimeWidget.Get() : nullptr;
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

	if (DestroyPolicy != EVdjmRecordEventWidgetDestroyPolicy::ENone && not bDestroyWaitStarted)
	{
		bDestroyWaitStarted = true;
		DestroyStartSeconds = world->GetTimeSeconds();
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

	if (DestroyPolicy == EVdjmRecordEventWidgetDestroyPolicy::ENone)
	{
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	if (DestroyPolicy == EVdjmRecordEventWidgetDestroyPolicy::ERemoveOnSignal)
	{
		if (DestroySignalTag.IsNone())
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		if (not EventManager->ConsumeFlowSignal(DestroySignalTag))
		{
			if (DestroyConditionMode != EVdjmRecordEventConditionMode::ERunning &&
				not EventManager->RequestCurrentFlowConditionForSignal(DestroySignalTag, DestroyConditionMode))
			{
				return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
			}

			return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, 0.0f);
		}

		RemoveRuntimeWidget(EventManager);
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	if (DestroyPolicy == EVdjmRecordEventWidgetDestroyPolicy::ERemoveAfterDelay)
	{
		const double elapsedSeconds = world->GetTimeSeconds() - DestroyStartSeconds;
		if (elapsedSeconds < DestroyDelaySeconds)
		{
			return MakeResult(
				EVdjmRecordEventResultType::ERunning,
				INDEX_NONE,
				NAME_None,
				DestroyDelaySeconds - static_cast<float>(elapsedSeconds));
		}

		RemoveRuntimeWidget(EventManager);
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventCreateWidgetNode::ResetRuntimeState_Implementation()
{
	bHasEmittedSignal = false;
	bDestroyWaitStarted = false;
	DestroyStartSeconds = 0.0;

	if (DestroyPolicy != EVdjmRecordEventWidgetDestroyPolicy::ENone)
	{
		if (UUserWidget* widgetInstance = RuntimeWidget.Get())
		{
			widgetInstance->RemoveFromParent();
		}
		RuntimeWidget.Reset();
	}
	else if (not bReuseCreatedWidget)
	{
		RuntimeWidget.Reset();
	}
}

void UVdjmRecordEventCreateWidgetNode::CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);
	manifest.AddSignalEmitter(EmitSignalTag, eventIndex, EventTag, GetEventManifestClassName(this));
	if (DestroyPolicy == EVdjmRecordEventWidgetDestroyPolicy::ERemoveOnSignal)
	{
		manifest.AddSignalWaiter(DestroySignalTag, eventIndex, EventTag, GetEventManifestClassName(this));
	}
}

void UVdjmRecordEventCreateWidgetNode::RemoveRuntimeWidget(UVdjmRecordEventManager* eventManager)
{
	UUserWidget* widgetInstance = RuntimeWidget.Get();
	if (widgetInstance == nullptr && eventManager != nullptr && not RuntimeSlotKey.IsNone())
	{
		widgetInstance = Cast<UUserWidget>(eventManager->FindRuntimeObjectSlot(RuntimeSlotKey));
	}

	if (widgetInstance != nullptr)
	{
		widgetInstance->RemoveFromParent();
	}

	if (eventManager != nullptr)
	{
		if (bUnregisterContextOnDestroy && not DestroyContextKey.IsNone())
		{
			if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem =
				UVdjmRecorderWorldContextSubsystem::Get(eventManager))
			{
				worldContextSubsystem->UnregisterContext(DestroyContextKey);
			}
		}

		if (bClearRuntimeSlotOnDestroy && not RuntimeSlotKey.IsNone())
		{
			eventManager->ClearRuntimeObjectSlot(RuntimeSlotKey);
		}
	}

	RuntimeWidget.Reset();
	bHasEmittedSignal = false;
	bDestroyWaitStarted = false;
	DestroyStartSeconds = 0.0;
}

FVdjmRecordEventResult UVdjmRecordEventRemoveWidgetNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || (RuntimeSlotKey.IsNone() && ContextKey.IsNone()))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const FVdjmRecordWidgetLookupResult widgetLookup = ResolveWidgetByLookupPolicy(
		EventManager,
		RuntimeSlotKey,
		ContextKey,
		LookupPolicy);
	UUserWidget* widgetInstance = widgetLookup.Widget;
	if (widgetInstance == nullptr)
	{
		return MakeResult(
			bSucceedIfMissing ? EVdjmRecordEventResultType::ESuccess : EVdjmRecordEventResultType::EFailure,
			INDEX_NONE,
			NAME_None,
			0.0f);
	}

	widgetInstance->RemoveFromParent();

	const FName contextKeyToUnregister = not widgetLookup.ContextKey.IsNone()
		? widgetLookup.ContextKey
		: ResolveWidgetContextLookupKey(RuntimeSlotKey, ContextKey);
	if (bUnregisterContext && not contextKeyToUnregister.IsNone())
	{
		if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(EventManager))
		{
			worldContextSubsystem->UnregisterContext(contextKeyToUnregister);
		}
	}

	if (bClearRuntimeSlot && not RuntimeSlotKey.IsNone())
	{
		EventManager->ClearRuntimeObjectSlot(RuntimeSlotKey);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventShowWidgetNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	FName targetSlotKey = RuntimeSlotKey;
	const FName previousSlotKey = EventManager->GetCurrentRuntimeWidgetSlotKey();
	UUserWidget* targetWidget = nullptr;
	UE_LOG(
		LogVdjmRecorderCore,
		Verbose,
		TEXT("ShowWidgetNode - Start. EventTag=%s RuntimeSlotKey=%s ContextKey=%s LookupPolicy=%s CursorDelta=%d PreviousSlotKey=%s bLowerPreviousWidget=%s bSetVisibleOnShow=%s"),
		*EventTag.ToString(),
		*RuntimeSlotKey.ToString(),
		*ContextKey.ToString(),
		*GetWidgetLookupPolicyString(LookupPolicy),
		CursorDelta,
		*previousSlotKey.ToString(),
		bLowerPreviousWidget ? TEXT("true") : TEXT("false"),
		bSetVisibleOnShow ? TEXT("true") : TEXT("false"));

	if (targetSlotKey.IsNone() && ContextKey.IsNone())
	{
		if (not EventManager->MoveRuntimeWidgetStackCursorBy(CursorDelta, targetSlotKey))
		{
			UE_LOG(
				LogVdjmRecorderCore,
				Verbose,
				TEXT("ShowWidgetNode - Stack cursor move found no target. EventTag=%s CursorDelta=%d PreviousSlotKey=%s"),
				*EventTag.ToString(),
				CursorDelta,
				*previousSlotKey.ToString());
			return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
		}
		targetWidget = FindRuntimeWidgetBySlot(EventManager, targetSlotKey);
	}
	else
	{
		const FVdjmRecordWidgetLookupResult widgetLookup = ResolveWidgetByLookupPolicy(
			EventManager,
			RuntimeSlotKey,
			ContextKey,
			LookupPolicy);
		targetWidget = widgetLookup.Widget;
		UE_LOG(
			LogVdjmRecorderCore,
			Verbose,
			TEXT("ShowWidgetNode - Lookup result. EventTag=%s TargetWidget=%s FoundFromRuntime=%s FoundFromContext=%s LookupRuntimeSlot=%s LookupContextKey=%s"),
			*EventTag.ToString(),
			*GetWidgetDebugName(targetWidget),
			widgetLookup.bFoundFromRuntimeSlot ? TEXT("true") : TEXT("false"),
			widgetLookup.bFoundFromContext ? TEXT("true") : TEXT("false"),
			*widgetLookup.RuntimeSlotKey.ToString(),
			*widgetLookup.ContextKey.ToString());
		if (targetWidget != nullptr && widgetLookup.bFoundFromContext && not RuntimeSlotKey.IsNone())
		{
			EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, targetWidget);
			UE_LOG(
				LogVdjmRecorderCore,
				Verbose,
				TEXT("ShowWidgetNode - Refreshed runtime slot from context. EventTag=%s RuntimeSlotKey=%s Widget=%s"),
				*EventTag.ToString(),
				*RuntimeSlotKey.ToString(),
				*GetWidgetDebugName(targetWidget));
		}
		if (not RuntimeSlotKey.IsNone())
		{
			EventManager->SetRuntimeWidgetStackCursor(RuntimeSlotKey);
			targetSlotKey = RuntimeSlotKey;
		}
	}

	if (targetWidget == nullptr)
	{
		UE_LOG(
			LogVdjmRecorderCore,
			Verbose,
			TEXT("ShowWidgetNode - Target widget not found. EventTag=%s RuntimeSlotKey=%s ContextKey=%s LookupPolicy=%s"),
			*EventTag.ToString(),
			*RuntimeSlotKey.ToString(),
			*ContextKey.ToString(),
			*GetWidgetLookupPolicyString(LookupPolicy));
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bLowerPreviousWidget && not previousSlotKey.IsNone() && previousSlotKey != targetSlotKey)
	{
		if (UUserWidget* previousWidget = FindRuntimeWidgetBySlot(EventManager, previousSlotKey))
		{
			const ESlateVisibility previousBeforeVisibility = previousWidget->GetVisibility();
			previousWidget->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(
				LogVdjmRecorderCore,
				Verbose,
				TEXT("ShowWidgetNode - Lowered previous widget. EventTag=%s PreviousSlotKey=%s PreviousWidget=%s VisibilityBefore=%s VisibilityAfter=%s"),
				*EventTag.ToString(),
				*previousSlotKey.ToString(),
				*GetWidgetDebugName(previousWidget),
				*GetWidgetVisibilityString(previousBeforeVisibility),
				*GetWidgetVisibilityString(previousWidget->GetVisibility()));
		}
		else
		{
			UE_LOG(
				LogVdjmRecorderCore,
				Verbose,
				TEXT("ShowWidgetNode - Previous slot had no live widget. EventTag=%s PreviousSlotKey=%s"),
				*EventTag.ToString(),
				*previousSlotKey.ToString());
		}
	}

	if (not targetWidget->IsInViewport())
	{
		targetWidget->AddToViewport(ZOrder);
		UE_LOG(
			LogVdjmRecorderCore,
			Verbose,
			TEXT("ShowWidgetNode - AddToViewport. EventTag=%s TargetSlotKey=%s TargetWidget=%s ZOrder=%d"),
			*EventTag.ToString(),
			*targetSlotKey.ToString(),
			*GetWidgetDebugName(targetWidget),
			ZOrder);
	}

	const ESlateVisibility targetBeforeVisibility = targetWidget->GetVisibility();
	if (bSetVisibleOnShow)
	{
		targetWidget->SetVisibility(ESlateVisibility::Visible);
	}
	UE_LOG(
		LogVdjmRecorderCore,
		Verbose,
		TEXT("ShowWidgetNode - Finished. EventTag=%s TargetSlotKey=%s TargetWidget=%s IsInViewport=%s VisibilityBefore=%s VisibilityAfter=%s CurrentCursor=%s"),
		*EventTag.ToString(),
		*targetSlotKey.ToString(),
		*GetWidgetDebugName(targetWidget),
		targetWidget->IsInViewport() ? TEXT("true") : TEXT("false"),
		*GetWidgetVisibilityString(targetBeforeVisibility),
		*GetWidgetVisibilityString(targetWidget->GetVisibility()),
		*EventManager->GetCurrentRuntimeWidgetSlotKey().ToString());

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventLowerWidgetNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr)
	{
		UE_LOG(
			LogVdjmRecorderCore,
			Error,
			TEXT("LowerWidgetNode - Failed. EventManager is null. EventTag=%s"),
			*EventTag.ToString());
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const FName resolvedContextKey = ResolveWidgetContextLookupKey(RuntimeSlotKey, ContextKey);
	UE_LOG(
		LogVdjmRecorderCore,
		Verbose,
		TEXT("LowerWidgetNode - Start. EventTag=%s RuntimeSlotKey=%s ContextKey=%s ResolvedContextKey=%s LookupPolicy=%s LowerCount=%d bMoveCursorAfterDirectLower=%s"),
		*EventTag.ToString(),
		*RuntimeSlotKey.ToString(),
		*ContextKey.ToString(),
		*resolvedContextKey.ToString(),
		*GetWidgetLookupPolicyString(LookupPolicy),
		LowerCount,
		bMoveCursorAfterDirectLower ? TEXT("true") : TEXT("false"));

	if (not RuntimeSlotKey.IsNone() || not ContextKey.IsNone())
	{
		const FVdjmRecordWidgetLookupResult widgetLookup = ResolveWidgetByLookupPolicy(
			EventManager,
			RuntimeSlotKey,
			ContextKey,
			LookupPolicy);
		UUserWidget* widgetInstance = widgetLookup.Widget;
		if (widgetInstance == nullptr)
		{
			UE_LOG(
				LogVdjmRecorderCore,
				Verbose,
				TEXT("LowerWidgetNode - Widget not found. EventTag=%s RuntimeSlotKey=%s ContextKey=%s ResolvedContextKey=%s LookupPolicy=%s bSucceedIfMissing=%s"),
				*EventTag.ToString(),
				*RuntimeSlotKey.ToString(),
				*ContextKey.ToString(),
				*resolvedContextKey.ToString(),
				*GetWidgetLookupPolicyString(LookupPolicy),
				bSucceedIfMissing ? TEXT("true") : TEXT("false"));
			return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
		}

		const ESlateVisibility beforeVisibility = widgetInstance->GetVisibility();
		const bool bWasInViewport = widgetInstance->IsInViewport();
		widgetInstance->SetVisibility(ESlateVisibility::Collapsed);
		const ESlateVisibility afterVisibility = widgetInstance->GetVisibility();
		UE_LOG(
			LogVdjmRecorderCore,
			Verbose,
			TEXT("LowerWidgetNode - Lowered direct widget. EventTag=%s Widget=%s Class=%s FoundFromRuntime=%s FoundFromContext=%s LookupRuntimeSlot=%s LookupContextKey=%s WasInViewport=%s VisibilityBefore=%s VisibilityAfter=%s"),
			*EventTag.ToString(),
			*GetNameSafe(widgetInstance),
			*GetNameSafe(widgetInstance->GetClass()),
			widgetLookup.bFoundFromRuntimeSlot ? TEXT("true") : TEXT("false"),
			widgetLookup.bFoundFromContext ? TEXT("true") : TEXT("false"),
			*widgetLookup.RuntimeSlotKey.ToString(),
			*widgetLookup.ContextKey.ToString(),
			bWasInViewport ? TEXT("true") : TEXT("false"),
			*GetWidgetVisibilityString(beforeVisibility),
			*GetWidgetVisibilityString(afterVisibility));
		if (bMoveCursorAfterDirectLower && not RuntimeSlotKey.IsNone())
		{
			EventManager->StepRuntimeWidgetStackCursorAfterLower(RuntimeSlotKey);
			UE_LOG(
				LogVdjmRecorderCore,
				Verbose,
				TEXT("LowerWidgetNode - Stepped runtime widget cursor after direct lower. EventTag=%s RuntimeSlotKey=%s CurrentCursor=%s"),
				*EventTag.ToString(),
				*RuntimeSlotKey.ToString(),
				*EventManager->GetCurrentRuntimeWidgetSlotKey().ToString());
		}
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	TArray<FName> lowerSlotKeys;
	if (not EventManager->CollectRuntimeWidgetSlotKeysForLower(LowerCount, lowerSlotKeys))
	{
		UE_LOG(
			LogVdjmRecorderCore,
			Verbose,
			TEXT("LowerWidgetNode - Stack lower found no slot keys. EventTag=%s LowerCount=%d CurrentCursor=%s"),
			*EventTag.ToString(),
			LowerCount,
			*EventManager->GetCurrentRuntimeWidgetSlotKey().ToString());
		return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
	}

	UE_LOG(
		LogVdjmRecorderCore,
		Verbose,
		TEXT("LowerWidgetNode - Stack lower slot keys collected. EventTag=%s LowerCount=%d SlotKeys=[%s]"),
		*EventTag.ToString(),
		LowerCount,
		*JoinNameArray(lowerSlotKeys));

	for (const FName lowerSlotKey : lowerSlotKeys)
	{
		if (UUserWidget* widgetInstance = FindRuntimeWidgetBySlot(EventManager, lowerSlotKey))
		{
			const ESlateVisibility beforeVisibility = widgetInstance->GetVisibility();
			const bool bWasInViewport = widgetInstance->IsInViewport();
			widgetInstance->SetVisibility(ESlateVisibility::Collapsed);
			const ESlateVisibility afterVisibility = widgetInstance->GetVisibility();
			UE_LOG(
				LogVdjmRecorderCore,
				Verbose,
				TEXT("LowerWidgetNode - Lowered stack widget. EventTag=%s SlotKey=%s Widget=%s Class=%s WasInViewport=%s VisibilityBefore=%s VisibilityAfter=%s"),
				*EventTag.ToString(),
				*lowerSlotKey.ToString(),
				*GetNameSafe(widgetInstance),
				*GetNameSafe(widgetInstance->GetClass()),
				bWasInViewport ? TEXT("true") : TEXT("false"),
				*GetWidgetVisibilityString(beforeVisibility),
				*GetWidgetVisibilityString(afterVisibility));
		}
		else
		{
			UE_LOG(
				LogVdjmRecorderCore,
				Verbose,
				TEXT("LowerWidgetNode - Stack slot had no live widget. EventTag=%s SlotKey=%s"),
				*EventTag.ToString(),
				*lowerSlotKey.ToString());
		}
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
		if (ConditionMode != EVdjmRecordEventConditionMode::ERunning &&
			not EventManager->RequestCurrentFlowConditionForSignal(SignalTag, ConditionMode))
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}

		return MakeResult(EVdjmRecordEventResultType::ERunning, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventWaitForSignalNode::CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);
	manifest.AddSignalWaiter(SignalTag, eventIndex, EventTag, GetEventManifestClassName(this));
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

	if (not EventManager->EmitFlowSignalByRoute(SignalTag, SignalRoute))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventEmitSignalNode::CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const
{
	Super::CollectFlowManifest(manifest, eventIndex);
	manifest.AddSignalEmitter(SignalTag, eventIndex, EventTag, GetEventManifestClassName(this));
}

FVdjmRecordEventResult UVdjmRecordEventStartSubgraphSessionNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || SubgraphTag.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UVdjmRecordEventFlowDataAsset* resolvedFlowAsset = FlowAsset != nullptr
		? FlowAsset.Get()
		: EventManager->GetCurrentOrMainFlowAsset();
	if (resolvedFlowAsset == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	FVdjmRecordFlowSessionHandle sessionHandle;
	if (not EventManager->StartEventSubgraphSession(
		resolvedFlowAsset,
		SubgraphTag,
		sessionHandle,
		bResetRuntimeStates))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventRegisterSubgraphSignalBranchNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || SignalTag.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UVdjmRecordEventFlowDataAsset* resolvedFlowAsset = FlowAsset != nullptr
		? FlowAsset.Get()
		: EventManager->GetCurrentOrMainFlowAsset();
	if (resolvedFlowAsset == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	FVdjmRecordSubgraphSignalBranch branch;
	branch.BranchTag = BranchTag.IsNone() ? SignalTag : BranchTag;
	branch.SignalTag = SignalTag;
	branch.FlowAsset = resolvedFlowAsset;
	branch.BranchCases = BranchCases;
	branch.bEnabled = bEnabled;
	branch.bTriggerOnce = bTriggerOnce;

	if (branch.BranchCases.IsEmpty() && not SubgraphTag.IsNone())
	{
		FVdjmRecordSubgraphBranchCase& branchCase = branch.BranchCases.AddDefaulted_GetRef();
		branchCase.CaseTag = SubgraphTag;
		branchCase.MatchCondition = EVdjmRecordSubgraphBranchCaseCondition::EAlways;
		branchCase.SubgraphTag = SubgraphTag;
		branchCase.DuplicatePolicy = EVdjmRecordSubgraphBranchDuplicatePolicy::EIgnoreAndSucceed;
		branchCase.bResetRuntimeStates = true;
	}

	FString registerErrorReason;
	if (not EventManager->RegisterSubgraphSignalBranch(branch, registerErrorReason, bReplaceExisting))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RegisterSubgraphSignalBranchNode failed. Branch=%s Signal=%s Error=%s"),
			*branch.BranchTag.ToString(),
			*SignalTag.ToString(),
			*registerErrorReason);
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

FVdjmRecordEventResult UVdjmRecordEventUnregisterSubgraphSignalBranchNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	(void)BridgeActor;

	if (EventManager == nullptr || BranchTag.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not EventManager->UnregisterSubgraphSignalBranch(BranchTag) && not bSucceedIfMissing)
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
	if (EventManager == nullptr || ContextKey.IsNone())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UE_LOG(
		LogVdjmRecorderCore,
		Verbose,
		TEXT("CreateWidgetAndRegisterContextNode - Start. EventTag=%s RuntimeSlotKey=%s ContextKey=%s WidgetClass=%s bReuseRegisteredContext=%s bSetVisibleWhenReused=%s"),
		*EventTag.ToString(),
		*RuntimeSlotKey.ToString(),
		*ContextKey.ToString(),
		*GetNameSafe(WidgetClass.Get()),
		bReuseRegisteredContext ? TEXT("true") : TEXT("false"),
		bSetVisibleWhenReused ? TEXT("true") : TEXT("false"));

	if (bReuseRegisteredContext && DestroyPolicy == EVdjmRecordEventWidgetDestroyPolicy::ENone)
	{
		UObject* contextObject = FindObjectByContext(EventManager, ContextKey);
		if (contextObject != nullptr)
		{
			UUserWidget* widgetInstance = Cast<UUserWidget>(contextObject);
			if (widgetInstance == nullptr)
			{
				UE_LOG(
					LogVdjmRecorderCore,
					Warning,
					TEXT("CreateWidgetAndRegisterContextNode - Context object is not widget. EventTag=%s ContextKey=%s Object=%s Class=%s"),
					*EventTag.ToString(),
					*ContextKey.ToString(),
					*GetNameSafe(contextObject),
					*GetNameSafe(contextObject->GetClass()));
				return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
			}

			if (WidgetClass != nullptr && not widgetInstance->IsA(WidgetClass.Get()))
			{
				UE_LOG(
					LogVdjmRecorderCore,
					Warning,
					TEXT("CreateWidgetAndRegisterContextNode - Context widget class mismatch. EventTag=%s ContextKey=%s Widget=%s WidgetClass=%s ExpectedClass=%s"),
					*EventTag.ToString(),
					*ContextKey.ToString(),
					*GetWidgetDebugName(widgetInstance),
					*GetNameSafe(widgetInstance->GetClass()),
					*GetNameSafe(WidgetClass.Get()));
				return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
			}

			const ESlateVisibility beforeVisibility = widgetInstance->GetVisibility();
			const bool bWasInViewport = widgetInstance->IsInViewport();
			UE_LOG(
				LogVdjmRecorderCore,
				Verbose,
				TEXT("CreateWidgetAndRegisterContextNode - Reusing context widget. EventTag=%s RuntimeSlotKey=%s ContextKey=%s Widget=%s Class=%s WasInViewport=%s VisibilityBefore=%s"),
				*EventTag.ToString(),
				*RuntimeSlotKey.ToString(),
				*ContextKey.ToString(),
				*GetWidgetDebugName(widgetInstance),
				*GetNameSafe(widgetInstance->GetClass()),
				bWasInViewport ? TEXT("true") : TEXT("false"),
				*GetWidgetVisibilityString(beforeVisibility));

			if (UVdjmRecordEventWidgetBase* eventWidgetBase = Cast<UVdjmRecordEventWidgetBase>(widgetInstance))
			{
				eventWidgetBase->ApplyEventContext(EventManager, BridgeActor);
			}

			if (bAddToViewport && not widgetInstance->IsInViewport())
			{
				widgetInstance->AddToViewport(ZOrder);
				UE_LOG(
					LogVdjmRecorderCore,
					Verbose,
					TEXT("CreateWidgetAndRegisterContextNode - Reused widget AddToViewport. EventTag=%s Widget=%s ZOrder=%d"),
					*EventTag.ToString(),
					*GetWidgetDebugName(widgetInstance),
					ZOrder);
			}

			if (bSetVisibleWhenReused)
			{
				widgetInstance->SetVisibility(ESlateVisibility::Visible);
				UE_LOG(
					LogVdjmRecorderCore,
					Verbose,
					TEXT("CreateWidgetAndRegisterContextNode - Reused widget set visible. EventTag=%s Widget=%s VisibilityBefore=%s VisibilityAfter=%s"),
					*EventTag.ToString(),
					*GetWidgetDebugName(widgetInstance),
					*GetWidgetVisibilityString(beforeVisibility),
					*GetWidgetVisibilityString(widgetInstance->GetVisibility()));
			}

			if (bRefreshRuntimeSlotWhenReused && not RuntimeSlotKey.IsNone())
			{
				if (not EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, widgetInstance))
				{
					return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
				}
				UE_LOG(
					LogVdjmRecorderCore,
					Verbose,
					TEXT("CreateWidgetAndRegisterContextNode - Reused widget refreshed runtime slot. EventTag=%s RuntimeSlotKey=%s Widget=%s CurrentCursor=%s"),
					*EventTag.ToString(),
					*RuntimeSlotKey.ToString(),
					*GetWidgetDebugName(widgetInstance),
					*EventManager->GetCurrentRuntimeWidgetSlotKey().ToString());
			}

			if (bRefreshContextRegistrationWhenReused)
			{
				if (not RegisterContextObject(EventManager, widgetInstance, ContextKey, widgetInstance->GetClass()))
				{
					return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
				}
				UE_LOG(
					LogVdjmRecorderCore,
					Verbose,
					TEXT("CreateWidgetAndRegisterContextNode - Reused widget refreshed context. EventTag=%s ContextKey=%s Widget=%s"),
					*EventTag.ToString(),
					*ContextKey.ToString(),
					*GetWidgetDebugName(widgetInstance));
			}

			if (not EmitSignalTag.IsNone())
			{
				EventManager->EmitFlowSignal(EmitSignalTag);
			}

			return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	const FVdjmRecordEventResult createResult = Super::ExecuteEvent_Implementation(EventManager, BridgeActor);
	if (createResult.ResultType != EVdjmRecordEventResultType::ESuccess)
	{
		UE_LOG(
			LogVdjmRecorderCore,
			Warning,
			TEXT("CreateWidgetAndRegisterContextNode - Create widget failed before context register. EventTag=%s RuntimeSlotKey=%s ContextKey=%s Result=%d"),
			*EventTag.ToString(),
			*RuntimeSlotKey.ToString(),
			*ContextKey.ToString(),
			static_cast<int32>(createResult.ResultType));
		return createResult;
	}

	UUserWidget* widgetInstance = EventManager != nullptr ? Cast<UUserWidget>(EventManager->FindRuntimeObjectSlot(RuntimeSlotKey)) : nullptr;
	if (widgetInstance == nullptr)
	{
		UE_LOG(
			LogVdjmRecorderCore,
			Warning,
			TEXT("CreateWidgetAndRegisterContextNode - Created widget not found in runtime slot. EventTag=%s RuntimeSlotKey=%s ContextKey=%s"),
			*EventTag.ToString(),
			*RuntimeSlotKey.ToString(),
			*ContextKey.ToString());
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (not RegisterContextObject(EventManager, widgetInstance, ContextKey, widgetInstance->GetClass()))
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	UE_LOG(
		LogVdjmRecorderCore,
		Verbose,
		TEXT("CreateWidgetAndRegisterContextNode - Created and registered widget. EventTag=%s RuntimeSlotKey=%s ContextKey=%s Widget=%s Class=%s IsInViewport=%s Visibility=%s"),
		*EventTag.ToString(),
		*RuntimeSlotKey.ToString(),
		*ContextKey.ToString(),
		*GetWidgetDebugName(widgetInstance),
		*GetNameSafe(widgetInstance->GetClass()),
		widgetInstance->IsInViewport() ? TEXT("true") : TEXT("false"),
		*GetWidgetVisibilityString(widgetInstance->GetVisibility()));

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
