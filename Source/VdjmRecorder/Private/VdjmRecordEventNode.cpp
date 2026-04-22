#include "VdjmRecordEventNode.h"

#include "VdjmRecordBridgeActor.h"
#include "VdjmRecordEventManager.h"
#include "VdjmRecorderCore.h"

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
	while (RuntimeChildIndex < Children.Num())
	{
		UVdjmRecordEventBase* Child = Children[RuntimeChildIndex];
		if (Child == nullptr)
		{
			++RuntimeChildIndex;
			continue;
		}

		const FVdjmRecordEventResult ChildResult = Child->ExecuteEvent(EventManager, BridgeActor);
		switch (ChildResult.ResultType)
		{
		case EVdjmRecordEventResultType::ESuccess:
			Child->ResetRuntimeState();
			++RuntimeChildIndex;
			break;
		case EVdjmRecordEventResultType::ERunning:
			return ChildResult;
		default:
			ResetRuntimeState();
			return ChildResult;
		}
	}

	ResetRuntimeState();
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}

void UVdjmRecordEventSequenceNode::ResetRuntimeState_Implementation()
{
	RuntimeChildIndex = 0;
	for (UVdjmRecordEventBase* Child : Children)
	{
		if (Child != nullptr)
		{
			Child->ResetRuntimeState();
		}
	}
}

FVdjmRecordEventResult UVdjmRecordEventSelectorNode::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	if (EventManager == nullptr)
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	const int32 NextIndex = EventManager->FindNextEventIndex(this, TargetClass, TargetTag);
	if (NextIndex != INDEX_NONE)
	{
		return MakeResult(EVdjmRecordEventResultType::ESelectIndex, NextIndex, NAME_None, 0.0f);
	}

	return MakeResult(bAbortIfNotFound ? EVdjmRecordEventResultType::EAbort : EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
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

FVdjmRecordEventResult UVdjmRecordEventSetEnvDataAssetPathNode::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	if (!EnvDataAssetPath.IsValid())
	{
		return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
	}

	if (bRequireLoadSuccess)
	{
		UObject* LoadedObject = EnvDataAssetPath.TryLoad();
		if (!IsValid(Cast<UVdjmRecordEnvDataAsset>(LoadedObject)))
		{
			return MakeResult(EVdjmRecordEventResultType::EFailure, INDEX_NONE, NAME_None, 0.0f);
		}
	}

	AVdjmRecordBridgeActor::SetRecordEnvDataAssetPath(EnvDataAssetPath);
	return MakeResult(EVdjmRecordEventResultType::ESuccess, INDEX_NONE, NAME_None, 0.0f);
}
