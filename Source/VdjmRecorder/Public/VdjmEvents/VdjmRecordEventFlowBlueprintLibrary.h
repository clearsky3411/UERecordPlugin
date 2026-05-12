#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecorderController.h"
#include "VdjmRecordEventFlowBlueprintLibrary.generated.h"

class UVdjmRecordEventFlowDataAsset;

/**
 * @brief 위젯 상속 구조와 무관하게 현재 월드의 Record Event Flow를 제어하는 Blueprint 헬퍼다.
 *
 * CommonUI 위젯처럼 `UVdjmRecordEventWidgetBase`를 상속하기 어려운 경우,
 * 위젯 자신을 WorldContextObject로 넘겨 현재 월드에 등록된 EventManager를 찾아 쓴다.
 */
UCLASS()
class VDJMRECORDER_API UVdjmRecordEventFlowBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecordEventManager* GetRecordEventManager(UObject* worldContextObject);

	UFUNCTION(BlueprintPure, Category = "Recorder|Controller", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecorderController* FindRecorderController(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecorderController* GetOrCreateRecorderController(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller", meta = (WorldContext = "worldContextObject"))
	static bool SubmitRecorderOptionRequest(
		UObject* worldContextObject,
		const FVdjmRecorderOptionRequest& request,
		FString& outErrorReason,
		bool bProcessPendingAfterSubmit = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|Controller", meta = (WorldContext = "worldContextObject"))
	static bool ProcessPendingRecorderOptionRequests(UObject* worldContextObject, FString& outErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool EmitRecordFlowSignal(UObject* worldContextObject, FName signalTag);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Signal", meta = (WorldContext = "worldContextObject"))
	static bool BindRecordFlowSignal(
		UObject* worldContextObject,
		UObject* listenerObject,
		FName signalTag,
		const FVdjmRecordFlowSignalCallback& callback);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Signal", meta = (WorldContext = "worldContextObject"))
	static bool UnbindRecordFlowSignal(
		UObject* worldContextObject,
		UObject* listenerObject,
		FName signalTag);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Signal", meta = (WorldContext = "worldContextObject"))
	static int32 UnbindRecordFlowSignalsForObject(UObject* worldContextObject, UObject* listenerObject);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Signal")
	static FVdjmRecordEventSignalRoute MakeCurrentSessionSignalRoute();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Signal")
	static FVdjmRecordEventSignalRoute MakeMainSessionSignalRoute();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Signal")
	static FVdjmRecordEventSignalRoute MakeAllActiveSessionsSignalRoute();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Signal")
	static FVdjmRecordEventSignalRoute MakeGlobalSignalRoute();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool EmitRecordFlowSignalByRoute(
		UObject* worldContextObject,
		FName signalTag,
		FVdjmRecordEventSignalRoute signalRoute);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Debug", meta = (WorldContext = "worldContextObject"))
	static bool EmitRecordFlowSignalWithDebug(UObject* worldContextObject, FName signalTag, FString& outDebugMessage);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Debug", meta = (WorldContext = "worldContextObject"))
	static FString GetRecordEventFlowDebugString(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool RequestPauseRecordEventFlow(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool RequestResumeRecordEventFlow(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool StopRecordEventFlow(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool RequestStopRecordEventFlow(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool RequestAbortRecordEventFlow(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool RequestFailRecordEventFlow(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool StartRecordEventFlow(
		UObject* worldContextObject,
		UVdjmRecordEventFlowDataAsset* flowDataAsset,
		bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Session", meta = (WorldContext = "worldContextObject"))
	static bool StartRecordEventFlowSession(
		UObject* worldContextObject,
		UVdjmRecordEventFlowDataAsset* flowDataAsset,
		FVdjmRecordFlowSessionHandle& outSessionHandle,
		bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Subgraph", meta = (WorldContext = "worldContextObject"))
	static bool StartRecordEventSubgraphSession(
		UObject* worldContextObject,
		UVdjmRecordEventFlowDataAsset* flowDataAsset,
		FName subgraphTag,
		FVdjmRecordFlowSessionHandle& outSessionHandle,
		bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Subgraph", meta = (WorldContext = "worldContextObject", DisplayName = "Run Record Event Subgraph", ToolTip = "FlowDataAsset의 SubgraphTag를 새 flow session으로 실행합니다. FlowDataAsset이 None이고 fallback이 켜져 있으면 현재/main flow asset을 사용합니다."))
	static bool RunRecordEventSubgraph(
		UObject* worldContextObject,
		UVdjmRecordEventFlowDataAsset* flowDataAsset,
		FName subgraphTag,
		FVdjmRecordFlowSessionHandle& outSessionHandle,
		FString& outErrorReason,
		bool bResetRuntimeStates = true,
		bool bAllowCurrentFlowAssetFallback = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Subgraph", meta = (WorldContext = "worldContextObject"))
	static bool RegisterRecordSubgraphSignalBranch(
		UObject* worldContextObject,
		const FVdjmRecordSubgraphSignalBranch& branch,
		FString& outErrorReason,
		bool bReplaceExisting = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Subgraph", meta = (WorldContext = "worldContextObject"))
	static bool UnregisterRecordSubgraphSignalBranch(UObject* worldContextObject, FName branchTag);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Session", meta = (WorldContext = "worldContextObject"))
	static bool EmitRecordFlowSignalToSession(
		UObject* worldContextObject,
		FVdjmRecordFlowSessionHandle sessionHandle,
		FName signalTag);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Session", meta = (WorldContext = "worldContextObject"))
	static bool RequestPauseRecordEventFlowSession(UObject* worldContextObject, FVdjmRecordFlowSessionHandle sessionHandle);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Session", meta = (WorldContext = "worldContextObject"))
	static bool RequestResumeRecordEventFlowSession(UObject* worldContextObject, FVdjmRecordFlowSessionHandle sessionHandle);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Session", meta = (WorldContext = "worldContextObject"))
	static bool RequestStopRecordEventFlowSession(UObject* worldContextObject, FVdjmRecordFlowSessionHandle sessionHandle);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Session", meta = (WorldContext = "worldContextObject"))
	static bool RequestAbortRecordEventFlowSession(UObject* worldContextObject, FVdjmRecordFlowSessionHandle sessionHandle);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|Session", meta = (WorldContext = "worldContextObject"))
	static bool RequestFailRecordEventFlowSession(UObject* worldContextObject, FVdjmRecordFlowSessionHandle sessionHandle);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool IsRecordEventFlowRunning(UObject* worldContextObject);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow", meta = (WorldContext = "worldContextObject"))
	static bool IsRecordEventFlowPaused(UObject* worldContextObject);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Session", meta = (WorldContext = "worldContextObject"))
	static bool IsRecordEventFlowSessionRunning(UObject* worldContextObject, FVdjmRecordFlowSessionHandle sessionHandle);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|Session", meta = (WorldContext = "worldContextObject"))
	static bool IsRecordEventFlowSessionPaused(UObject* worldContextObject, FVdjmRecordFlowSessionHandle sessionHandle);
};
