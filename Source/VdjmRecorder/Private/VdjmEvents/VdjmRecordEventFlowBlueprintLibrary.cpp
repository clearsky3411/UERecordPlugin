#include "VdjmEvents/VdjmRecordEventFlowBlueprintLibrary.h"

#include "VdjmEvents/VdjmRecordEventFlowEntryPoint.h"
#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"
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

bool UVdjmRecordEventFlowBlueprintLibrary::BindRecordFlowSignal(
	UObject* worldContextObject,
	UObject* listenerObject,
	FName signalTag,
	const FVdjmRecordFlowSignalCallback& callback)
{
	if (signalTag.IsNone() || listenerObject == nullptr || not callback.IsBound())
	{
		return false;
	}

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->BindFlowSignal(signalTag, listenerObject, callback);
}

bool UVdjmRecordEventFlowBlueprintLibrary::UnbindRecordFlowSignal(
	UObject* worldContextObject,
	UObject* listenerObject,
	FName signalTag)
{
	if (signalTag.IsNone() || listenerObject == nullptr)
	{
		return false;
	}

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->UnbindFlowSignal(signalTag, listenerObject);
}

int32 UVdjmRecordEventFlowBlueprintLibrary::UnbindRecordFlowSignalsForObject(
	UObject* worldContextObject,
	UObject* listenerObject)
{
	if (listenerObject == nullptr)
	{
		return 0;
	}

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return 0;
	}

	return eventManager->UnbindFlowSignalsForObject(listenerObject);
}

FVdjmRecordEventSignalRoute UVdjmRecordEventFlowBlueprintLibrary::MakeCurrentSessionSignalRoute()
{
	FVdjmRecordEventSignalRoute signalRoute;
	signalRoute.Value = FVdjmRecordEventSignalRoute::CurrentSession;
	return signalRoute;
}

FVdjmRecordEventSignalRoute UVdjmRecordEventFlowBlueprintLibrary::MakeMainSessionSignalRoute()
{
	FVdjmRecordEventSignalRoute signalRoute;
	signalRoute.Value = FVdjmRecordEventSignalRoute::MainSession;
	return signalRoute;
}

FVdjmRecordEventSignalRoute UVdjmRecordEventFlowBlueprintLibrary::MakeAllActiveSessionsSignalRoute()
{
	FVdjmRecordEventSignalRoute signalRoute;
	signalRoute.Value = FVdjmRecordEventSignalRoute::AllActiveSessions;
	return signalRoute;
}

FVdjmRecordEventSignalRoute UVdjmRecordEventFlowBlueprintLibrary::MakeGlobalSignalRoute()
{
	FVdjmRecordEventSignalRoute signalRoute;
	signalRoute.Value = FVdjmRecordEventSignalRoute::Global;
	return signalRoute;
}

bool UVdjmRecordEventFlowBlueprintLibrary::EmitRecordFlowSignalByRoute(
	UObject* worldContextObject,
	FName signalTag,
	FVdjmRecordEventSignalRoute signalRoute)
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

	return eventManager->EmitFlowSignalByRoute(signalTag, signalRoute);
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

bool UVdjmRecordEventFlowBlueprintLibrary::StartRecordEventSubgraphSession(
	UObject* worldContextObject,
	UVdjmRecordEventFlowDataAsset* flowDataAsset,
	FName subgraphTag,
	FVdjmRecordFlowSessionHandle& outSessionHandle,
	bool bResetRuntimeStates)
{
	outSessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->StartEventSubgraphSession(flowDataAsset, subgraphTag, outSessionHandle, bResetRuntimeStates);
}

bool UVdjmRecordEventFlowBlueprintLibrary::RunRecordEventSubgraph(
	UObject* worldContextObject,
	UVdjmRecordEventFlowDataAsset* flowDataAsset,
	FName subgraphTag,
	FVdjmRecordFlowSessionHandle& outSessionHandle,
	FString& outErrorReason,
	bool bResetRuntimeStates,
	bool bAllowCurrentFlowAssetFallback)
{
	outSessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();
	outErrorReason.Reset();

	if (subgraphTag.IsNone())
	{
		outErrorReason = TEXT("SubgraphTag is None.");
		return false;
	}

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		outErrorReason = TEXT("Record event manager is not available.");
		return false;
	}

	UVdjmRecordEventFlowDataAsset* resolvedFlowDataAsset = flowDataAsset;
	if (resolvedFlowDataAsset == nullptr && bAllowCurrentFlowAssetFallback)
	{
		resolvedFlowDataAsset = eventManager->GetCurrentOrMainFlowAsset();
	}

	if (resolvedFlowDataAsset == nullptr)
	{
		outErrorReason = TEXT("FlowDataAsset is not available.");
		return false;
	}

	if (resolvedFlowDataAsset->FindSubgraphIndexByTag(subgraphTag) == INDEX_NONE)
	{
		outErrorReason = FString::Printf(
			TEXT("Subgraph was not found. FlowAsset=%s SubgraphTag=%s"),
			*resolvedFlowDataAsset->GetPathName(),
			*subgraphTag.ToString());
		return false;
	}

	if (not eventManager->StartEventSubgraphSession(
		resolvedFlowDataAsset,
		subgraphTag,
		outSessionHandle,
		bResetRuntimeStates))
	{
		outErrorReason = FString::Printf(
			TEXT("Failed to start subgraph session. FlowAsset=%s SubgraphTag=%s"),
			*resolvedFlowDataAsset->GetPathName(),
			*subgraphTag.ToString());
		return false;
	}

	return true;
}

bool UVdjmRecordEventFlowBlueprintLibrary::RegisterRecordSubgraphSignalBranch(
	UObject* worldContextObject,
	const FVdjmRecordSubgraphSignalBranch& branch,
	FString& outErrorReason,
	bool bReplaceExisting)
{
	outErrorReason.Reset();

	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		outErrorReason = TEXT("Record event manager is not available.");
		return false;
	}

	return eventManager->RegisterSubgraphSignalBranch(branch, outErrorReason, bReplaceExisting);
}

bool UVdjmRecordEventFlowBlueprintLibrary::UnregisterRecordSubgraphSignalBranch(
	UObject* worldContextObject,
	FName branchTag)
{
	UVdjmRecordEventManager* eventManager = GetRecordEventManager(worldContextObject);
	if (eventManager == nullptr)
	{
		return false;
	}

	return eventManager->UnregisterSubgraphSignalBranch(branchTag);
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
