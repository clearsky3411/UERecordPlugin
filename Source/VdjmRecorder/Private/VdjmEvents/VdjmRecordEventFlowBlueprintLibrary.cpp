#include "VdjmEvents/VdjmRecordEventFlowBlueprintLibrary.h"

#include "VdjmEvents/VdjmRecordEventFlowEntryPoint.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecorderWorldContextSubsystem.h"

UVdjmRecordEventManager* UVdjmRecordEventFlowBlueprintLibrary::GetRecordEventManager(UObject* worldContextObject)
{
	if (UVdjmRecordEventManager* directEventManager = Cast<UVdjmRecordEventManager>(worldContextObject))
	{
		return directEventManager;
	}

	const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject);
	if (worldContextSubsystem == nullptr)
	{
		return nullptr;
	}

	if (UVdjmRecordEventManager* eventManager = Cast<UVdjmRecordEventManager>(
		worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetEventManagerContextKey())))
	{
		return eventManager;
	}

	if (AVdjmRecordEventFlowEntryPoint* entryPoint = Cast<AVdjmRecordEventFlowEntryPoint>(
		worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetEventFlowEntryPointContextKey())))
	{
		return entryPoint->GetEventManager();
	}

	return nullptr;
}

UVdjmRecorderController* UVdjmRecordEventFlowBlueprintLibrary::FindRecorderController(UObject* worldContextObject)
{
	return UVdjmRecorderController::FindRecorderController(worldContextObject);
}

UVdjmRecorderController* UVdjmRecordEventFlowBlueprintLibrary::GetOrCreateRecorderController(UObject* worldContextObject)
{
	return UVdjmRecorderController::FindOrCreateRecorderController(worldContextObject);
}

bool UVdjmRecordEventFlowBlueprintLibrary::SubmitRecorderOptionRequest(
	UObject* worldContextObject,
	const FVdjmRecorderOptionRequest& request,
	FString& outErrorReason,
	bool bProcessPendingAfterSubmit)
{
	outErrorReason.Reset();

	UVdjmRecorderController* recorderController = GetOrCreateRecorderController(worldContextObject);
	if (recorderController == nullptr)
	{
		outErrorReason = TEXT("Recorder controller is not available.");
		return false;
	}

	if (not recorderController->SubmitOptionRequest(request, outErrorReason))
	{
		return false;
	}

	if (bProcessPendingAfterSubmit)
	{
		FString processErrorReason;
		const bool bProcessResult = recorderController->ProcessPendingOptionRequests(processErrorReason);
		if (not bProcessResult && not recorderController->HasPendingOptionRequest())
		{
			outErrorReason = processErrorReason.IsEmpty()
				? TEXT("Failed to process recorder option request.")
				: processErrorReason;
			return false;
		}
	}

	return true;
}

bool UVdjmRecordEventFlowBlueprintLibrary::ProcessPendingRecorderOptionRequests(
	UObject* worldContextObject,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	UVdjmRecorderController* recorderController = GetOrCreateRecorderController(worldContextObject);
	if (recorderController == nullptr)
	{
		outErrorReason = TEXT("Recorder controller is not available.");
		return false;
	}

	return recorderController->ProcessPendingOptionRequests(outErrorReason);
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

bool UVdjmRecordEventFlowBlueprintLibrary::EmitRecordFlowSignalWithDebug(
	UObject* worldContextObject,
	FName signalTag,
	FString& outDebugMessage)
{
	outDebugMessage.Reset();

	if (signalTag.IsNone())
	{
		outDebugMessage = TEXT("EmitRecordFlowSignalWithDebug failed: signalTag is None.");
		return false;
	}

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		const FString contextName = worldContextObject != nullptr ? worldContextObject->GetPathName() : TEXT("None");
		outDebugMessage = FString::Printf(
			TEXT("EmitRecordFlowSignalWithDebug failed: EventManager was not found. WorldContext=%s"),
			*contextName);
		return false;
	}

	const bool bEmitResult = eventManager->EmitFlowSignal(signalTag);
	outDebugMessage = FString::Printf(
		TEXT("EmitRecordFlowSignalWithDebug signal=%s result=%s\n%s"),
		*signalTag.ToString(),
		bEmitResult ? TEXT("true") : TEXT("false"),
		*eventManager->GetEventFlowDebugString());
	return bEmitResult;
}

FString UVdjmRecordEventFlowBlueprintLibrary::GetRecordEventFlowDebugString(UObject* worldContextObject)
{
	const UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		const FString contextName = worldContextObject != nullptr ? worldContextObject->GetPathName() : TEXT("None");
		return FString::Printf(TEXT("RecordEventManager=None WorldContext=%s"), *contextName);
	}

	return eventManager->GetEventFlowDebugString();
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
