#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecorderSessionTypes.h"
#include "VdjmRecordEventManager.generated.h"

class AVdjmRecordBridgeActor;
class UVdjmRecordEventBase;
class UVdjmRecordEventFlowDataAsset;
class UVdjmRecordEventFlowRuntime;
class UVdjmRecordResource;
struct FVdjmRecordEventResult;

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordFlowHandle
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	int64 Value = INDEX_NONE;

	static FVdjmRecordFlowHandle MakeInvalid()
	{
		return FVdjmRecordFlowHandle();
	}

	bool IsValid() const
	{
		return Value >= 0;
	}

	friend bool operator==(const FVdjmRecordFlowHandle& Left, const FVdjmRecordFlowHandle& Right)
	{
		return Left.Value == Right.Value;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVdjmRecordFlowHandle& FlowHandle)
{
	return GetTypeHash(FlowHandle.Value);
}

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordFlowSessionHandle
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	int64 Value = INDEX_NONE;

	static FVdjmRecordFlowSessionHandle MakeInvalid()
	{
		return FVdjmRecordFlowSessionHandle();
	}

	bool IsValid() const
	{
		return Value >= 0;
	}

	friend bool operator==(const FVdjmRecordFlowSessionHandle& Left, const FVdjmRecordFlowSessionHandle& Right)
	{
		return Left.Value == Right.Value;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVdjmRecordFlowSessionHandle& SessionHandle)
{
	return GetTypeHash(SessionHandle.Value);
}

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordEventRuntimeHandle
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	int64 Value = INDEX_NONE;

	static FVdjmRecordEventRuntimeHandle MakeInvalid()
	{
		return FVdjmRecordEventRuntimeHandle();
	}

	static FVdjmRecordEventRuntimeHandle MakeTerminalSink()
	{
		FVdjmRecordEventRuntimeHandle RuntimeHandle;
		RuntimeHandle.Value = 0;
		return RuntimeHandle;
	}

	bool IsValid() const
	{
		return Value >= 0;
	}

	bool IsTerminalSink() const
	{
		return Value == 0;
	}

	friend bool operator==(const FVdjmRecordEventRuntimeHandle& Left, const FVdjmRecordEventRuntimeHandle& Right)
	{
		return Left.Value == Right.Value;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVdjmRecordEventRuntimeHandle& RuntimeHandle)
{
	return GetTypeHash(RuntimeHandle.Value);
}

UENUM(BlueprintType)
enum class EVdjmRecordEventEdgeKind : uint8
{
	ENext,
	EJump,
	ESignal,
	ETerminal
};

UENUM(BlueprintType)
enum class EVdjmRecordEventEdgeState : uint8
{
	EAdvance,
	ERepeat,
	EDiscard
};

enum class EVdjmRecordFlowControlAction : uint8
{
	ENone,
	EResume,
	EPause,
	EStop,
	EAbort,
	EFail
};

enum class EVdjmRecordFlowStepDisposition : uint8
{
	EContinue,
	EYield,
	EFinishSuccess,
	EFinishAbort,
	EFinishFailure
};

struct FVdjmRecordFlowStepContext
{
public:
	FVdjmRecordFlowHandle FlowHandle = FVdjmRecordFlowHandle::MakeInvalid();
	AVdjmRecordBridgeActor* BridgeActor = nullptr;
	const TArray<TObjectPtr<UVdjmRecordEventBase>>* Events = nullptr;
	int32* CurrentIndex = nullptr;
	float* NextExecutableTime = nullptr;
	float CurrentTimeSeconds = 0.0f;

	bool IsValid() const
	{
		return FlowHandle.IsValid() && Events != nullptr && CurrentIndex != nullptr;
	}
};

struct FVdjmRecordFlowStepResult
{
public:
	EVdjmRecordFlowStepDisposition Disposition = EVdjmRecordFlowStepDisposition::EContinue;
	EVdjmRecordFlowControlAction FlowControlAction = EVdjmRecordFlowControlAction::ENone;
	float WaitSeconds = 0.0f;
	bool bDidExecuteEvent = false;
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordObservedEdge
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	FVdjmRecordFlowHandle FlowHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	FVdjmRecordEventRuntimeHandle FromHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	FVdjmRecordEventRuntimeHandle ToHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	EVdjmRecordEventEdgeKind EdgeKind = EVdjmRecordEventEdgeKind::ENext;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	EVdjmRecordEventEdgeState EdgeState = EVdjmRecordEventEdgeState::EAdvance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	FName ChannelKey = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	EVdjmRecordEventResultType TerminalReason = EVdjmRecordEventResultType::ESuccess;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	FName RequestedByTag = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventManager")
	FString RequestedByClassName;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FVdjmRecordManagerChainEvent,
	AVdjmRecordBridgeActor*,
	BridgeActor,
	EVdjmRecordBridgeInitStep,
	PreviousStep,
	EVdjmRecordBridgeInitStep,
	CurrentStep);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FVdjmRecordManagerFlowSessionFinished,
	UVdjmRecordEventManager*,
	EventManager,
	FVdjmRecordFlowSessionHandle,
	SessionHandle,
	UVdjmRecordEventFlowDataAsset*,
	FlowAsset,
	EVdjmRecordEventResultType,
	FinalResult);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FVdjmRecordManagerSessionStateChanged,
	UVdjmRecordEventManager*,
	EventManager,
	EVdjmRecorderSessionState,
	PreviousState,
	EVdjmRecorderSessionState,
	CurrentState,
	double,
	TransitionSeconds);

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager", meta = (WorldContext = "WorldContextObject"))
	static UVdjmRecordEventManager* CreateEventManager(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool InitializeManager(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool BindBridge(AVdjmRecordBridgeActor* InBridgeActor);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	AVdjmRecordBridgeActor* GetBoundBridge() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartRecordingByManager();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	void StopRecordingByManager();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartEventFlow(UVdjmRecordEventFlowDataAsset* InFlowAsset, bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartEventFlowRuntime(UVdjmRecordEventFlowRuntime* InFlowRuntime, bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartEventFlowFromJsonString(const FString& InJsonString, FString& OutError, bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartEventFlowSession(
		UVdjmRecordEventFlowDataAsset* InFlowAsset,
		FVdjmRecordFlowSessionHandle& OutSessionHandle,
		bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool StartEventFlowSessionFromJsonString(
		const FString& InJsonString,
		FVdjmRecordFlowSessionHandle& OutSessionHandle,
		FString& OutError,
		bool bResetRuntimeStates = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	void StopEventFlow();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestStopEventFlow();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestPauseEventFlow();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestResumeEventFlow();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestAbortEventFlow();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestFailEventFlow();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	bool IsEventFlowRunning() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	bool IsEventFlowPaused() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool EmitFlowSignalToSession(FVdjmRecordFlowSessionHandle SessionHandle, FName InSignalTag);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestStopEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestPauseEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestResumeEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestAbortEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestFailEventFlowSession(FVdjmRecordFlowSessionHandle SessionHandle);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	bool IsEventFlowSessionRunning(FVdjmRecordFlowSessionHandle SessionHandle) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	bool IsEventFlowSessionPaused(FVdjmRecordFlowSessionHandle SessionHandle) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	UVdjmRecordEventFlowDataAsset* GetActiveFlowAsset() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	UVdjmRecordEventFlowRuntime* GetActiveFlowRuntime() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	int32 GetCurrentFlowIndex() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	UWorld* GetManagerWorld() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	EVdjmRecorderSessionState GetSessionState() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	double GetLastSessionTransitionSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	FString GetLastSessionError() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	int32 FindNextEventIndex(const UVdjmRecordEventBase* SourceEvent, TSubclassOf<UVdjmRecordEventBase> TargetClass, FName TargetTag) const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool EmitFlowSignal(FName InSignalTag);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool EmitFlowSignalByScope(FName InSignalTag, EVdjmRecordEventSignalScope InSignalScope);

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool ConsumeFlowSignal(FName InSignalTag);

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	bool HasPendingFlowSignal(FName InSignalTag) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	FVdjmRecordFlowHandle GetCurrentFlowHandle() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventManager")
	FVdjmRecordObservedEdge GetCurrentObservedEdge() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventManager")
	bool RequestObservedEdgeDirective(
		EVdjmRecordEventEdgeKind InEdgeKind,
		EVdjmRecordEventEdgeState InEdgeState,
		const UVdjmRecordEventBase* InDestinationEvent = nullptr,
		FName InChannelKey = NAME_None,
		EVdjmRecordEventResultType InTerminalReason = EVdjmRecordEventResultType::ESuccess);

	UObject* FindRuntimeObjectSlot(FName InSlotKey) const;
	bool SetRuntimeObjectSlot(FName InSlotKey, UObject* InObject);
	bool ClearRuntimeObjectSlot(FName InSlotKey);

	FVdjmRecordFlowHandle FindOrCreateChildFlowHandle(UVdjmRecordEventBase* OwnerEvent);
	bool PushActiveFlow(FVdjmRecordFlowHandle FlowHandle);
	void PopActiveFlow(FVdjmRecordFlowHandle FlowHandle);
	FVdjmRecordEventResult ExecuteEventInFlow(UVdjmRecordEventBase* EventNode, AVdjmRecordBridgeActor* BridgeActor, FVdjmRecordFlowHandle FlowHandle);
	FVdjmRecordFlowStepResult ExecuteFlowStep(const FVdjmRecordFlowStepContext& stepContext);
	FVdjmRecordFlowStepResult ExecuteFlowLoop(const FVdjmRecordFlowStepContext& stepContext, int32 maxStepCount = 64);
	void ObserveEventResult(
		UVdjmRecordEventBase* SourceEvent,
		const FVdjmRecordEventResult& Result,
		FVdjmRecordFlowHandle FlowHandle,
		const UVdjmRecordEventBase* DestinationEvent = nullptr);
	EVdjmRecordFlowControlAction ConsumePendingFlowControlRequest(FVdjmRecordFlowHandle FlowHandle);

	UPROPERTY(BlueprintAssignable, Category = "Recorder|EventManager")
	FVdjmRecordManagerChainEvent OnManagerObservedChainEvent;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|EventManager")
	FVdjmRecordManagerFlowSessionFinished OnEventFlowSessionFinished;

	UPROPERTY(BlueprintAssignable, Category = "Recorder|EventManager")
	FVdjmRecordManagerSessionStateChanged OnSessionStateChanged;

protected:
	virtual UWorld* GetWorld() const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	struct FVdjmRecordPendingEdgeDirective
	{
	public:
		bool bIsSet = false;
		bool bHasExplicitFromHandle = false;
		bool bHasExplicitToHandle = false;
		EVdjmRecordEventEdgeKind EdgeKind = EVdjmRecordEventEdgeKind::ENext;
		EVdjmRecordEventEdgeState EdgeState = EVdjmRecordEventEdgeState::EAdvance;
		FName ChannelKey = NAME_None;
		EVdjmRecordEventResultType TerminalReason = EVdjmRecordEventResultType::ESuccess;
		const UVdjmRecordEventBase* DestinationEvent = nullptr;
		FVdjmRecordEventRuntimeHandle ExplicitFromHandle;
		FVdjmRecordEventRuntimeHandle ExplicitToHandle;
		FName RequestedByTag = NAME_None;
		FString RequestedByClassName;

		void Reset()
		{
			bIsSet = false;
			bHasExplicitFromHandle = false;
			bHasExplicitToHandle = false;
			EdgeKind = EVdjmRecordEventEdgeKind::ENext;
			EdgeState = EVdjmRecordEventEdgeState::EAdvance;
			ChannelKey = NAME_None;
			TerminalReason = EVdjmRecordEventResultType::ESuccess;
			DestinationEvent = nullptr;
			ExplicitFromHandle = FVdjmRecordEventRuntimeHandle::MakeInvalid();
			ExplicitToHandle = FVdjmRecordEventRuntimeHandle::MakeInvalid();
			RequestedByTag = NAME_None;
			RequestedByClassName.Reset();
		}
	};

	struct FVdjmRecordPendingFlowControlRequest
	{
	public:
		EVdjmRecordFlowControlAction Action = EVdjmRecordFlowControlAction::ENone;

		bool IsSet() const
		{
			return Action != EVdjmRecordFlowControlAction::ENone;
		}

		void Reset()
		{
			Action = EVdjmRecordFlowControlAction::ENone;
		}
	};

	struct FVdjmRecordFlowExecutionState
	{
	public:
		FVdjmRecordFlowHandle FlowHandle = FVdjmRecordFlowHandle::MakeInvalid();
		FVdjmRecordFlowHandle ParentFlowHandle = FVdjmRecordFlowHandle::MakeInvalid();
		TWeakObjectPtr<UVdjmRecordEventBase> OwnerEvent;
		int32 CurrentIndex = INDEX_NONE;
		float NextExecutableTime = 0.0f;
		TMap<const UVdjmRecordEventBase*, FVdjmRecordEventRuntimeHandle> RuntimeEventHandles;
		FVdjmRecordPendingEdgeDirective PendingEdgeDirective;
		FVdjmRecordPendingFlowControlRequest PendingFlowControlRequest;
		FVdjmRecordObservedEdge LastObservedEdge;
		bool bHasObservedEdge = false;
		bool bPaused = false;
	};

	struct FVdjmRecordEventExecutionContext
	{
	public:
		FVdjmRecordFlowSessionHandle SessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();
		FVdjmRecordFlowHandle FlowHandle = FVdjmRecordFlowHandle::MakeInvalid();
		TWeakObjectPtr<UVdjmRecordEventBase> EventNode;
		FVdjmRecordEventRuntimeHandle RuntimeHandle = FVdjmRecordEventRuntimeHandle::MakeInvalid();
	};

	struct FVdjmRecordFlowSession
	{
	public:
		FVdjmRecordFlowSessionHandle SessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();
		FVdjmRecordFlowSessionHandle ParentSessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();
		TWeakObjectPtr<UVdjmRecordEventFlowDataAsset> ActiveFlowAsset;
		TObjectPtr<UVdjmRecordEventFlowRuntime> ActiveFlowRuntime;
		FVdjmRecordFlowHandle RootFlowHandle = FVdjmRecordFlowHandle::MakeInvalid();
		TMap<FVdjmRecordFlowHandle, FVdjmRecordFlowExecutionState> FlowExecutionStates;
		TMap<const UVdjmRecordEventBase*, FVdjmRecordFlowHandle> ChildFlowHandles;
		TArray<FVdjmRecordFlowHandle> ActiveFlowStack;
		TArray<FVdjmRecordEventExecutionContext> EventExecutionContextStack;
		TMap<FName, int32> PendingFlowSignals;
		TMap<FName, FVdjmRecordEventRuntimeHandle> LastSignalProducerHandles;
		TMap<FName, TWeakObjectPtr<UObject>> RuntimeObjectSlots;
		bool bFlowRunning = false;
	};

	void TickEventFlow();
	void TickFlowSession(FVdjmRecordFlowSessionHandle SessionHandle);
	void ResetFlowRuntimeStates();
	void FinishFlow(EVdjmRecordEventResultType FinalResultType);
	void FinishFlowSession(FVdjmRecordFlowSessionHandle SessionHandle, EVdjmRecordEventResultType FinalResultType);
	void ResetFlowSignals();
	void ResetFlowExecutionStates();
	void ApplySessionStateByBridgeSnapshot();
	void ResetSessionState(EVdjmRecorderSessionState NewState);
	void TransitionSessionState(EVdjmRecorderSessionState NewState, const FString& InErrorMessage = FString());
	FVdjmRecordFlowSessionHandle CreateFlowSessionHandle();
	FVdjmRecordFlowHandle CreateFlowHandle();
	bool StartEventFlowSessionRuntime(
		UVdjmRecordEventFlowRuntime* InFlowRuntime,
		FVdjmRecordFlowSessionHandle& OutSessionHandle,
		bool bResetRuntimeStates = true,
		FVdjmRecordFlowSessionHandle ParentSessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid());
	FVdjmRecordFlowSession* FindFlowSession(FVdjmRecordFlowSessionHandle SessionHandle);
	const FVdjmRecordFlowSession* FindFlowSession(FVdjmRecordFlowSessionHandle SessionHandle) const;
	FVdjmRecordFlowSession& FindOrCreateFlowSession(FVdjmRecordFlowSessionHandle SessionHandle);
	FVdjmRecordFlowSession* FindCurrentFlowSession();
	const FVdjmRecordFlowSession* FindCurrentFlowSession() const;
	FVdjmRecordFlowSession* FindMainFlowSession();
	const FVdjmRecordFlowSession* FindMainFlowSession() const;
	FVdjmRecordFlowSession* FindFlowSessionByFlowHandle(FVdjmRecordFlowHandle FlowHandle);
	const FVdjmRecordFlowSession* FindFlowSessionByFlowHandle(FVdjmRecordFlowHandle FlowHandle) const;
	FVdjmRecordFlowSession* ResolveFlowSessionForGeneralAccess();
	const FVdjmRecordFlowSession* ResolveFlowSessionForGeneralAccess() const;
	bool PushActiveSession(FVdjmRecordFlowSessionHandle SessionHandle);
	void PopActiveSession(FVdjmRecordFlowSessionHandle SessionHandle);
	void ResetFlowSession(FVdjmRecordFlowSession& FlowSession);
	FVdjmRecordFlowExecutionState* FindFlowExecutionState(FVdjmRecordFlowHandle FlowHandle);
	const FVdjmRecordFlowExecutionState* FindFlowExecutionState(FVdjmRecordFlowHandle FlowHandle) const;
	FVdjmRecordFlowExecutionState& FindOrCreateFlowExecutionState(FVdjmRecordFlowHandle FlowHandle, UVdjmRecordEventBase* OwnerEvent = nullptr);
	FVdjmRecordEventRuntimeHandle FindOrCreateRuntimeEventHandle(FVdjmRecordFlowHandle FlowHandle, const UVdjmRecordEventBase* EventNode);
	FVdjmRecordObservedEdge BuildObservedEdgeFromResult(
		UVdjmRecordEventBase* SourceEvent,
		const FVdjmRecordEventResult& Result,
		FVdjmRecordFlowHandle FlowHandle,
		const UVdjmRecordEventBase* DestinationEvent);
	FVdjmRecordObservedEdge ApplyPendingEdgeDirective(
		const FVdjmRecordObservedEdge& DefaultEdge,
		FVdjmRecordFlowHandle FlowHandle);
	bool RequestFlowControl(EVdjmRecordFlowControlAction Action);
	bool RequestFlowControl(FVdjmRecordFlowSessionHandle SessionHandle, EVdjmRecordFlowControlAction Action);
	void ApplyFlowControlRequestToFlowChain(FVdjmRecordFlowHandle FlowHandle, EVdjmRecordFlowControlAction Action);
	EVdjmRecordFlowControlAction MergeFlowControlAction(
		EVdjmRecordFlowControlAction CurrentAction,
		EVdjmRecordFlowControlAction RequestedAction) const;
	int32 FindNextExecutableEventIndex(const TArray<TObjectPtr<UVdjmRecordEventBase>>& Events, int32 StartIndex) const;
	int32 FindEventIndexByTag(const TArray<TObjectPtr<UVdjmRecordEventBase>>& events, FName eventTag) const;

	UFUNCTION()
	void HandleBridgeChainEvent(AVdjmRecordBridgeActor* InBridgeActor, EVdjmRecordBridgeInitStep PrevStep, EVdjmRecordBridgeInitStep CurrentStep);
	UFUNCTION()
	void HandleBridgeInitComplete(AVdjmRecordBridgeActor* InBridgeActor);
	UFUNCTION()
	void HandleBridgeInitErrorEnd(AVdjmRecordBridgeActor* InBridgeActor);
	UFUNCTION()
	void HandleBridgeRecordStarted(UVdjmRecordResource* InRecordResource);
	UFUNCTION()
	void HandleBridgeRecordStopped(UVdjmRecordResource* InRecordResource);

	TWeakObjectPtr<UWorld> CachedWorld;
	TWeakObjectPtr<AVdjmRecordBridgeActor> WeakBridgeActor;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVdjmRecordEventFlowRuntime>> FlowRuntimeKeepAlive;

	bool bPendingFinalization = false;
	EVdjmRecorderSessionState CurrentSessionState = EVdjmRecorderSessionState::ENew;
	double LastSessionTransitionSeconds = 0.0;
	FString LastSessionError;
	FVdjmRecordFlowSessionHandle MainSessionHandle = FVdjmRecordFlowSessionHandle::MakeInvalid();
	TMap<FVdjmRecordFlowSessionHandle, FVdjmRecordFlowSession> FlowSessions;
	TArray<FVdjmRecordFlowSessionHandle> ActiveSessionStack;
	TMap<FName, int32> GlobalPendingFlowSignals;
	TMap<FName, FVdjmRecordEventRuntimeHandle> GlobalLastSignalProducerHandles;
	int64 NextFlowSessionHandleValue = 1;
	int64 NextFlowHandleValue = 1;
	int64 NextRuntimeEventHandleValue = 1;
};
