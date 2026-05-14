#include "VdjmEvents/VdjmRecordEventWidgetBase.h"

#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecordBridgeActor.h"
#include "VdjmRecorderWorldContextSubsystem.h"

void UVdjmRecordEventWidgetBase::ApplyEventContext(UVdjmRecordEventManager* InEventManager, AVdjmRecordBridgeActor* InBridgeActor)
{
	LinkedEventManager = InEventManager;
	LinkedBridgeActor = InBridgeActor;
	HandleEventContextApplied(InEventManager, InBridgeActor);
}

UVdjmRecordEventManager* UVdjmRecordEventWidgetBase::GetLinkedEventManager() const
{
	if (not LinkedEventManager.IsValid())
	{
		if (const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = GetWorldContextSubsystem())
		{
			return Cast<UVdjmRecordEventManager>(
				worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetEventManagerContextKey()));
		}
	}

	return LinkedEventManager.Get();
}

AVdjmRecordBridgeActor* UVdjmRecordEventWidgetBase::GetLinkedBridgeActor() const
{
	if (not LinkedBridgeActor.IsValid())
	{
		if (const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = GetWorldContextSubsystem())
		{
			return worldContextSubsystem->GetBridgeActor();
		}
	}

	return LinkedBridgeActor.Get();
}

UVdjmRecorderWorldContextSubsystem* UVdjmRecordEventWidgetBase::GetWorldContextSubsystem() const
{
	return UVdjmRecorderWorldContextSubsystem::Get(const_cast<UVdjmRecordEventWidgetBase*>(this));
}

bool UVdjmRecordEventWidgetBase::EmitLinkedFlowSignal(FName signalTag) const
{
	if (signalTag.IsNone())
	{
		return false;
	}

	UVdjmRecordEventManager* eventManager = GetLinkedEventManager();
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->EmitFlowSignal(signalTag);
}

bool UVdjmRecordEventWidgetBase::RequestPauseLinkedEventFlow() const
{
	UVdjmRecordEventManager* eventManager = GetLinkedEventManager();
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestPauseEventFlow();
}

bool UVdjmRecordEventWidgetBase::RequestResumeLinkedEventFlow() const
{
	UVdjmRecordEventManager* eventManager = GetLinkedEventManager();
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestResumeEventFlow();
}

bool UVdjmRecordEventWidgetBase::StopLinkedEventFlow() const
{
	UVdjmRecordEventManager* eventManager = GetLinkedEventManager();
	if (eventManager == nullptr)
	{
		return false;
	}

	eventManager->StopEventFlow();
	return true;
}

bool UVdjmRecordEventWidgetBase::RequestStopLinkedEventFlow() const
{
	UVdjmRecordEventManager* eventManager = GetLinkedEventManager();
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestStopEventFlow();
}

bool UVdjmRecordEventWidgetBase::RequestAbortLinkedEventFlow() const
{
	UVdjmRecordEventManager* eventManager = GetLinkedEventManager();
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestAbortEventFlow();
}

bool UVdjmRecordEventWidgetBase::RequestFailLinkedEventFlow() const
{
	UVdjmRecordEventManager* eventManager = GetLinkedEventManager();
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestFailEventFlow();
}

bool UVdjmRecordEventWidgetBase::StartLinkedEventFlow(UVdjmRecordEventFlowDataAsset* flowDataAsset, bool bResetRuntimeStates) const
{
	UVdjmRecordEventManager* eventManager = GetLinkedEventManager();
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->StartEventFlow(flowDataAsset, bResetRuntimeStates);
}

void UVdjmRecordEventWidgetBase::HandleEventContextApplied_Implementation(UVdjmRecordEventManager* InEventManager, AVdjmRecordBridgeActor* InBridgeActor)
{
	(void)InEventManager;
	(void)InBridgeActor;
}
