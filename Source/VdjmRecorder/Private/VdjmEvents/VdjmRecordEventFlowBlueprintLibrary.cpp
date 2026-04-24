#include "VdjmEvents/VdjmRecordEventFlowBlueprintLibrary.h"

#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecorderWorldContextSubsystem.h"

UVdjmRecordEventManager* UVdjmRecordEventFlowBlueprintLibrary::GetRecordEventManager(UObject* worldContextObject)
{
	const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject);
	if (worldContextSubsystem == nullptr)
	{
		return nullptr;
	}

	return Cast<UVdjmRecordEventManager>(
		worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetEventManagerContextKey()));
}

bool UVdjmRecordEventFlowBlueprintLibrary::EmitRecordFlowSignal(UObject* worldContextObject, FName signalTag)
{
	if (signalTag.IsNone())
	{
		return false;
	}

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->EmitFlowSignal(signalTag);
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestPauseRecordEventFlow(UObject* worldContextObject)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestPauseEventFlow();
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestResumeRecordEventFlow(UObject* worldContextObject)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestResumeEventFlow();
}

bool UVdjmRecordEventFlowBlueprintLibrary::StopRecordEventFlow(UObject* worldContextObject)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	eventManager->StopEventFlow();
	return true;
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestStopRecordEventFlow(UObject* worldContextObject)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestStopEventFlow();
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestAbortRecordEventFlow(UObject* worldContextObject)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestAbortEventFlow();
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestFailRecordEventFlow(UObject* worldContextObject)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestFailEventFlow();
}

bool UVdjmRecordEventFlowBlueprintLibrary::StartRecordEventFlow(
	UObject* worldContextObject,
	UVdjmRecordEventFlowDataAsset* flowDataAsset,
	bool bResetRuntimeStates)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->StartEventFlow(flowDataAsset, bResetRuntimeStates);
}

bool UVdjmRecordEventFlowBlueprintLibrary::StartRecordEventFlowSession(
	UObject* worldContextObject,
	UVdjmRecordEventFlowDataAsset* flowDataAsset,
	FVdjmRecordFlowSessionHandle& outSessionHandle,
	bool bResetRuntimeStates)
{
	outSessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->StartEventFlowSession(flowDataAsset, outSessionHandle, bResetRuntimeStates);
}

bool UVdjmRecordEventFlowBlueprintLibrary::EmitRecordFlowSignalToSession(
	UObject* worldContextObject,
	FVdjmRecordFlowSessionHandle sessionHandle,
	FName signalTag)
{
	if (signalTag.IsNone())
	{
		return false;
	}

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->EmitFlowSignalToSession(sessionHandle, signalTag);
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestPauseRecordEventFlowSession(
	UObject* worldContextObject,
	FVdjmRecordFlowSessionHandle sessionHandle)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestPauseEventFlowSession(sessionHandle);
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestResumeRecordEventFlowSession(
	UObject* worldContextObject,
	FVdjmRecordFlowSessionHandle sessionHandle)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestResumeEventFlowSession(sessionHandle);
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestStopRecordEventFlowSession(
	UObject* worldContextObject,
	FVdjmRecordFlowSessionHandle sessionHandle)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestStopEventFlowSession(sessionHandle);
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestAbortRecordEventFlowSession(
	UObject* worldContextObject,
	FVdjmRecordFlowSessionHandle sessionHandle)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestAbortEventFlowSession(sessionHandle);
}

bool UVdjmRecordEventFlowBlueprintLibrary::RequestFailRecordEventFlowSession(
	UObject* worldContextObject,
	FVdjmRecordFlowSessionHandle sessionHandle)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->RequestFailEventFlowSession(sessionHandle);
}

bool UVdjmRecordEventFlowBlueprintLibrary::IsRecordEventFlowRunning(UObject* worldContextObject)
{
	const UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->IsEventFlowRunning();
}

bool UVdjmRecordEventFlowBlueprintLibrary::IsRecordEventFlowPaused(UObject* worldContextObject)
{
	const UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->IsEventFlowPaused();
}

bool UVdjmRecordEventFlowBlueprintLibrary::IsRecordEventFlowSessionRunning(
	UObject* worldContextObject,
	FVdjmRecordFlowSessionHandle sessionHandle)
{
	const UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->IsEventFlowSessionRunning(sessionHandle);
}

bool UVdjmRecordEventFlowBlueprintLibrary::IsRecordEventFlowSessionPaused(
	UObject* worldContextObject,
	FVdjmRecordFlowSessionHandle sessionHandle)
{
	const UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->IsEventFlowSessionPaused(sessionHandle);
}
