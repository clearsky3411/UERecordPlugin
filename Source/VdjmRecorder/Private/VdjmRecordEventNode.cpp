#include "VdjmRecordEventNode.h"

#include "VdjmRecordBridgeActor.h"
#include "VdjmRecordEventManager.h"

FVdjmRecordEventResult UVdjmRecordEventBase::ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor)
{
	return MakeResult(EVdjmRecordEventResultType::Success, INDEX_NONE, NAME_None, 0.0f);
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
		case EVdjmRecordEventResultType::Success:
			Child->ResetRuntimeState();
			++RuntimeChildIndex;
			break;
		case EVdjmRecordEventResultType::Running:
			return ChildResult;
		default:
			ResetRuntimeState();
			return ChildResult;
		}
	}

	ResetRuntimeState();
	return MakeResult(EVdjmRecordEventResultType::Success, INDEX_NONE, NAME_None, 0.0f);
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
		return MakeResult(EVdjmRecordEventResultType::Failure, INDEX_NONE, NAME_None, 0.0f);
	}

	const int32 NextIndex = EventManager->FindNextEventIndex(this, TargetClass, TargetTag);
	if (NextIndex != INDEX_NONE)
	{
		return MakeResult(EVdjmRecordEventResultType::SelectIndex, NextIndex, NAME_None, 0.0f);
	}

	return MakeResult(bAbortIfNotFound ? EVdjmRecordEventResultType::Abort : EVdjmRecordEventResultType::Failure, INDEX_NONE, NAME_None, 0.0f);
}
