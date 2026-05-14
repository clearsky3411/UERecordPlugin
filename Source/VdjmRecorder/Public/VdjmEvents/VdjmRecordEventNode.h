#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecorderController.h"
#include "VdjmRecordEventNode.generated.h"

/**
 * @file VdjmRecordEventNode.h
 *
 * @brief Vdjm Recorder Event Flow에서 사용하는 이벤트 노드 선언 모음.
 *
 * @section overview 개요
 * - 이 파일은 Event Flow를 구성하는 개별 실행 노드들을 선언한다.
 * - 각 노드는 `UVdjmRecordEventBase`를 공통 기반으로 사용하며, 실행 결과는 `FVdjmRecordEventResult`로 반환한다.
 * - 실제 실행과 흐름 제어는 `UVdjmRecordEventManager`가 담당하고, 여기의 노드는 "작은 동작 단위"를 표현한다.
 * @section event_groups 이벤트 분류
 * - Core & Composite
 *   - `UVdjmRecordEventSequenceNode`
 * - FlowControl
 *   - `UVdjmRecordEventJumpToNextNode`
 *   - `UVdjmRecordEventSelectorNode` (호환 alias)
 *   - `UVdjmRecordEventWaitForSignalNode`
 *   - `UVdjmRecordEventDelayNode`
 *   - `UVdjmRecordEventEmitSignalNode`
 *   - `UVdjmRecordEventStartSubgraphSessionNode`
 *   - `UVdjmRecordEventRegisterSubgraphSignalBranchNode`
 *   - `UVdjmRecordEventUnregisterSubgraphSignalBranchNode`
 * - Debug
 *   - `UVdjmRecordEventLogNode`
 * - RecorderSpecific & Legacy
 *   - `UVdjmRecordEventSpawnRecordBridgeActorWait`
 *   - `UVdjmRecordEventStartRecordBridgeActorNode`
 *   - `UVdjmRecordEventEnsureRecorderControllerNode`
 *   - `UVdjmRecordEventLoadAppStateNode`
 *   - `UVdjmRecordEventEnsureMediaPreviewManagerNode`
 *   - `UVdjmRecordEventInitializeMediaPreviewManagerNode`
 *   - `UVdjmRecordEventSubmitRecorderOptionRequestNode`
 *   - `UVdjmRecordEventSetEnvDataAssetPathNode`
 *   - `UVdjmRecordEventSpawnBridgeActorNode` (구형 단순 bridge spawn)
 * - Primitive.Object
 *   - `UVdjmRecordEventCreateObjectNode`
 *   - `UVdjmRecordEventSpawnActorNode`
 * - Primitive.Context
 *   - `UVdjmRecordEventRegisterContextEntryNode`
 *   - `UVdjmRecordEventRegisterWidgetContextNode`
 * - Primitive.Widget
 *   - `UVdjmRecordEventCreateWidgetNode`
 *   - `UVdjmRecordEventShowWidgetNode`
 *   - `UVdjmRecordEventLowerWidgetNode`
 *   - `UVdjmRecordEventRemoveWidgetNode`
 * - Composite.Convenience
 *   - `UVdjmRecordEventCreateObjectAndRegisterContextNode`
 *   - `UVdjmRecordEventSpawnActorAndRegisterContextNode`
 *   - `UVdjmRecordEventCreateWidgetAndRegisterContextNode`
 *
 * @section runtime_notes 런타임 메모
 * - 이벤트 사이의 임시 객체 전달은 주로 `UVdjmRecordEventManager`의 runtime slot을 통해 이뤄진다.
 * - 월드 전역에 공유해야 하는 참조는 `UVdjmRecorderWorldContextSubsystem`에 등록하는 방향을 전제로 한다.
 * - 일부 노드는 단독 primitive이고, 일부 노드는 primitive 여러 개를 합친 composite 역할을 한다.
 *
 * @section authoring_notes 작성 메모
 * - 새 이벤트를 추가할 때는 가능한 한 책임을 작게 유지한다.
 * - 자주 같이 쓰는 조합만 별도 composite로 올리고, 나머지는 flow 조립으로 해결한다.
 */

class AVdjmRecordBridgeActor;
class AVdjmRecordMediaPreviewManagerActor;
class AActor;
class UUserWidget;
class UVdjmRecordEventManager;
class UVdjmRecordEventFlowDataAsset;
class UVdjmRecordAppStateStore;
class UVdjmRecordMetadataStore;
struct FVdjmRecordEventFlowManifest;
enum class EVdjmRecordBridgeInitStep : uint8;


USTRUCT(BlueprintType)
struct FVdjmRecordEventResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	EVdjmRecordEventResultType ResultType = EVdjmRecordEventResultType::ESuccess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow", meta = (ClampMin = "0"))
	int32 SelectedIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow")
	FName JumpLabel = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|EventFlow", meta = (ClampMin = "0.0"))
	float WaitSeconds = 0.0f;
};

UENUM(BlueprintType)
enum class EVdjmRecordEventWidgetDestroyPolicy : uint8
{
	ENone UMETA(DisplayName = "None", ToolTip = "위젯 생성 후 바로 성공하고 수명 관리는 외부 이벤트가 담당합니다."),
	ERemoveOnSignal UMETA(DisplayName = "Remove On Signal", ToolTip = "위젯 생성 후 DestroySignalTag를 기다렸다가 제거하고 다음 flow로 진행합니다."),
	ERemoveAfterDelay UMETA(DisplayName = "Remove After Delay", ToolTip = "위젯 생성 후 지정한 시간이 지나면 제거하고 다음 flow로 진행합니다.")
};

UENUM(BlueprintType)
enum class EVdjmRecordEventWidgetLookupPolicy : uint8
{
	ERuntimeSlotOnly UMETA(DisplayName = "Runtime Slot Only", ToolTip = "현재 flow session의 RuntimeSlotKey에서만 위젯을 찾습니다."),
	EContextOnly UMETA(DisplayName = "Context Only", ToolTip = "WorldContextSubsystem의 ContextKey에서만 위젯을 찾습니다. ContextKey가 None이면 RuntimeSlotKey를 context key처럼 사용합니다."),
	ERuntimeSlotThenContext UMETA(DisplayName = "Runtime Slot Then Context", ToolTip = "RuntimeSlotKey를 먼저 찾고, 없으면 ContextKey 또는 RuntimeSlotKey로 world context를 찾습니다."),
	EContextThenRuntime UMETA(DisplayName = "Context Then Runtime Slot", ToolTip = "World context를 먼저 찾고, 없으면 현재 flow session의 RuntimeSlotKey를 찾습니다.")
};

UENUM(BlueprintType)
enum class EVdjmRecordEventObjectLookupPolicy : uint8
{
	ERuntimeSlotOnly UMETA(DisplayName = "Runtime Slot Only", ToolTip = "현재 flow session의 RuntimeSlotKey에서만 객체를 찾습니다."),
	EContextOnly UMETA(DisplayName = "Context Only", ToolTip = "WorldContextSubsystem의 SourceContextKey 또는 ContextKey에서만 객체를 찾습니다."),
	ERuntimeSlotThenContext UMETA(DisplayName = "Runtime Slot Then Context", ToolTip = "RuntimeSlotKey를 먼저 찾고, 없으면 SourceContextKey 또는 ContextKey로 world context를 찾습니다."),
	EContextThenRuntime UMETA(DisplayName = "Context Then Runtime Slot", ToolTip = "World context를 먼저 찾고, 없으면 현재 flow session의 RuntimeSlotKey를 찾습니다.")
};

/** @brief 모든 레코드 이벤트 노드의 공통 베이스 클래스다. */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "모든 레코드 이벤트 노드의 공통 베이스 클래스"))
class VDJMRECORDER_API UVdjmRecordEventBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Recorder|EventFlow")
	FVdjmRecordEventResult ExecuteEvent(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor);
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Recorder|EventFlow")
	void ResetRuntimeState();
	virtual void ResetRuntimeState_Implementation();

	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	static FVdjmRecordEventResult MakeResult(EVdjmRecordEventResultType InResultType, int32 InSelectedIndex, FName InJumpLabel, float InWaitSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Core", meta = (ToolTip = "이 이벤트 노드 자체를 식별하는 작성/디버그용 태그입니다. RuntimeSlotKey나 SignalTag와 달리 객체 저장/대기 신호에는 직접 쓰이지 않습니다."))
	FName EventTag = NAME_None;
};

/** @brief 자식 이벤트들을 순서대로 실행하는 composite 시퀀스 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "자식 이벤트를 순서대로 실행하는 시퀀스 노드"))
class VDJMRECORDER_API UVdjmRecordEventSequenceNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Recorder|EventNode|Composite")
	TArray<TObjectPtr<UVdjmRecordEventBase>> Children;

private:
	UPROPERTY(Transient)
	int32 RuntimeChildIndex = 0;
};

/** @brief 현재 이벤트 이후에서 다음 대상 이벤트를 찾아 점프하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "현재 이벤트 이후에서 다음 대상 이벤트를 찾아 점프하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventJumpToNextNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl")
	TSubclassOf<UVdjmRecordEventBase> TargetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl")
	FName TargetTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl")
	bool bAbortIfNotFound = false;
};

/** @brief 이전 이름 호환을 위해 남겨둔 selector alias 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "기존 selector 이름 호환을 위한 jump alias 노드"))
class VDJMRECORDER_API UVdjmRecordEventSelectorNode : public UVdjmRecordEventJumpToNextNode
{
	GENERATED_BODY()
};

/** @brief 지정한 메시지를 로그로 남기는 테스트/디버그용 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "지정한 메시지를 로그로 남기는 노드"))
class VDJMRECORDER_API UVdjmRecordEventLogNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Debug")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Debug")
	bool bLogAsWarning = false;
};

/** @brief 브릿지 액터를 단순 생성 또는 재사용 후 바인드만 수행하는 구형 spawn 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "브릿지 액터를 단순 생성 또는 재사용 후 바인드만 수행하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventSpawnBridgeActorNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Legacy")
	bool bReuseExistingBridgeActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Legacy")
	TSubclassOf<AVdjmRecordBridgeActor> BridgeActorClass;
};

/** @brief 브릿지 액터를 준비하고 정책에 따라 시작 또는 초기화 완료 대기까지 수행하는 브릿지 특화 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "브릿지 액터를 준비하고 정책에 따라 prepare/start/wait를 수행하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventSpawnRecordBridgeActorWait : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific")
	bool bReuseExistingBridgeActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific")
	TSubclassOf<AVdjmRecordBridgeActor> BridgeActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific")
	FSoftObjectPath EnvDataAssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific")
	bool bRequireLoadSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific")
	EVdjmRecordEventBridgeStartPolicy StartPolicy = EVdjmRecordEventBridgeStartPolicy::EStartImmediately;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific")
	FName StartSignalTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific", meta = (ToolTip = "브릿지 시작 신호 또는 init 완료 조건을 기다릴 때의 처리 방식입니다. Running은 기존 tick polling, Passive/Conditional은 flow를 멈추고 manager가 다시 열어줍니다."))
	EVdjmRecordEventConditionMode ConditionMode = EVdjmRecordEventConditionMode::ERunning;

private:
	bool ResolveRuntimeBridge(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor*& OutBridgeActor);
	bool ApplyBridgePreStartSettings(AVdjmRecordBridgeActor* InBridgeActor) const;
	bool CanTreatAsInitSuccess(const AVdjmRecordBridgeActor* InBridgeActor) const;
	bool IsInitFailureStep(EVdjmRecordBridgeInitStep InInitStep) const;

	TWeakObjectPtr<AVdjmRecordBridgeActor> RuntimeBridgeActor;
	bool bHasIssuedStart = false;
};

/** @brief 현재 바인드된 브릿지 액터의 초기화 체인을 시작하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "현재 바인드된 브릿지 액터의 StartRecordBridgeActor를 호출하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventStartRecordBridgeActorNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;
};

/** @brief RecorderController를 안전하게 생성/조회하고 runtime slot/context에 등록하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "RecorderController를 FindOrCreate 경로로 보장하고 필요하면 runtime slot과 world context에 등록하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventEnsureRecorderControllerNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|Controller", meta = (ToolTip = "현재 flow session 안에서 Controller를 임시로 찾을 이름입니다. None이면 runtime slot에 저장하지 않습니다."))
	FName RuntimeSlotKey = FName(TEXT("controller"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|Controller", meta = (ToolTip = "WorldContextSubsystem에 등록할 전역 조회 키입니다. None이면 표준 RecorderController context key를 자동으로 사용합니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|Controller", meta = (ToolTip = "true면 Controller를 RuntimeSlotKey에 저장합니다."))
	bool bStoreRuntimeSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|Controller", meta = (ToolTip = "true면 Controller를 WorldContextSubsystem에 등록합니다."))
	bool bRegisterContext = true;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|Controller")
	FString LastErrorReason;
};

/** @brief 앱 전역 AppState JSON을 로드하고 runtime slot/context에 등록하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "앱 전역 AppState JSON을 로드하고 필요하면 media registry TOC를 갱신하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventLoadAppStateNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState", meta = (ToolTip = "현재 flow session 안에서 AppStateStore를 임시로 찾을 이름입니다. None이면 runtime slot에 저장하지 않습니다."))
	FName RuntimeSlotKey = FName(TEXT("app-state"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState", meta = (ToolTip = "WorldContextSubsystem에 등록할 전역 조회 키입니다. None이면 표준 AppStateStore context key를 자동으로 사용합니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState", meta = (ToolTip = "true면 AppState 파일이 없을 때 기본 JSON을 만들어 저장합니다."))
	bool bCreateIfMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState", meta = (ToolTip = "true면 MetadataStore registry를 디스크에서 재스캔한 뒤 AppState records_toc에 반영합니다."))
	bool bRefreshRecordsToc = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState", meta = (ToolTip = "true면 로드/TOC 갱신 후 AppState JSON을 다시 저장합니다."))
	bool bSaveAfterRefresh = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState", meta = (ClampMin = "1", ToolTip = "Records TOC 갱신을 위해 manifest를 스캔할 때 한 step에서 처리할 manifest 파일 수입니다."))
	int32 MaxManifestFilesPerStep = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState", meta = (ClampMin = "1", ToolTip = "Records TOC 갱신을 위해 registry entry 상태를 확인할 때 한 step에서 처리할 entry 수입니다."))
	int32 MaxRegistryEntryStateChecksPerStep = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState", meta = (ToolTip = "true면 AppStateStore를 RuntimeSlotKey에 저장합니다."))
	bool bStoreRuntimeSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState", meta = (ToolTip = "true면 AppStateStore를 WorldContextSubsystem에 등록합니다."))
	bool bRegisterContext = true;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|AppState")
	FString LastErrorReason;

private:
	TWeakObjectPtr<UVdjmRecordAppStateStore> RuntimeAppStateStore;
	TWeakObjectPtr<UVdjmRecordMetadataStore> RuntimeMetadataStore;
	bool bHasLoadedAppState = false;
	bool bHasStartedRegistryScan = false;
};

/** @brief MediaPreviewManagerActor를 안전하게 생성/조회하고 runtime slot/context에 등록하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "MediaPreviewManagerActor를 FindOrSpawn 경로로 보장하고 필요하면 runtime slot과 world context에 등록하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventEnsureMediaPreviewManagerNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ToolTip = "현재 flow session 안에서 PreviewManager를 임시로 찾을 이름입니다. None이면 runtime slot에 저장하지 않습니다."))
	FName RuntimeSlotKey = FName(TEXT("media-preview-manager"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ToolTip = "WorldContextSubsystem에 등록할 전역 조회 키입니다. None이면 표준 MediaPreviewManager context key를 자동으로 사용합니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ToolTip = "true면 PreviewManager를 RuntimeSlotKey에 저장합니다."))
	bool bStoreRuntimeSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ToolTip = "true면 PreviewManager를 WorldContextSubsystem에 등록합니다."))
	bool bRegisterContext = true;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview")
	FString LastErrorReason;
};

/** @brief MediaPreviewManagerActor의 명시 초기화/registry refresh를 수행하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "MediaPreviewManagerActor를 명시적으로 초기화합니다. 이미 초기화된 경우 ForceRefresh가 아니면 no-op 성공으로 처리합니다."))
class VDJMRECORDER_API UVdjmRecordEventInitializeMediaPreviewManagerNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ToolTip = "먼저 조회할 runtime slot 이름입니다. 없으면 FindOrSpawn 경로로 manager를 보장합니다."))
	FName RuntimeSlotKey = FName(TEXT("media-preview-manager"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ToolTip = "true면 runtime slot에 manager가 없을 때 자동으로 찾거나 스폰합니다."))
	bool bFindOrSpawnIfMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ToolTip = "true면 이미 초기화가 끝났어도 registry를 다시 refresh합니다."))
	bool bForceRefresh = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ToolTip = "true면 초기화 후 carousel window를 현재 설정으로 적용합니다. registry가 비어있으면 빈 상태를 정상으로 취급할 수 있습니다."))
	bool bApplyCarouselWindowAfterInit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ToolTip = "true면 registry가 비어 있어 carousel window 적용이 실패해도 초기화 성공으로 취급합니다."))
	bool bSucceedWithEmptyRegistry = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ClampMin = "1", ClampMax = "32"))
	int32 SlotCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ClampMin = "0", ClampMax = "31"))
	int32 ActiveSlotIndex = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview")
	int32 InitialCenterSourceIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview")
	bool bAutoStartCenterPreview = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ClampMin = "1", ToolTip = "한 번의 이벤트 실행 step에서 읽고 등록할 manifest 파일 수입니다. 값이 작을수록 registry scan이 여러 tick에 나눠집니다."))
	int32 MaxManifestFilesPerStep = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ClampMin = "1", ToolTip = "한 번의 이벤트 실행 step에서 파일 존재 여부를 다시 확인할 registry entry 수입니다."))
	int32 MaxRegistryEntryStateChecksPerStep = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview", meta = (ClampMin = "1", ToolTip = "한 번의 이벤트 실행 step에서 복사할 registry entry 수입니다. 값이 작을수록 여러 tick에 나눠 진행됩니다."))
	int32 MaxRegistryEntriesPerStep = 32;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|MediaPreview")
	FString LastErrorReason;

private:
	TWeakObjectPtr<AVdjmRecordMediaPreviewManagerActor> RuntimePreviewManager;
	bool bHasStartedPreviewManagerInit = false;
};

/** @brief RecorderController에 녹화 옵션 변경 요청을 제출하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "RecorderController에 녹화 옵션 변경 요청을 제출하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventSubmitRecorderOptionRequestNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|Option")
	FVdjmRecorderOptionRequest OptionRequest;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|Option")
	bool bProcessPendingAfterSubmit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|Option")
	bool bSucceedIfQueued = true;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific|Option")
	FString LastErrorReason;

private:
	bool bHasSubmitted = false;
};

/** @brief 일반 UObject를 생성하고 runtime object slot에 넣는 primitive 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "일반 UObject를 생성하고 runtime slot에 저장하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventCreateObjectNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Object")
	TSubclassOf<UObject> ObjectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Object", meta = (ToolTip = "생성한 객체를 현재 flow session 안에 보관할 임시 이름입니다. 다른 이벤트가 이 이름으로 객체를 가져올 수 있습니다."))
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Object")
	bool bReuseSlotObject = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Object")
	EVdjmRecordEventObjectOuterPolicy OuterPolicy = EVdjmRecordEventObjectOuterPolicy::EBridgeActor;
};

/** @brief 일반 Actor를 스폰하고 runtime object slot에 넣는 primitive 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "일반 Actor를 스폰하고 runtime slot에 저장하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventSpawnActorNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Object")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Object", meta = (ToolTip = "스폰한 Actor를 현재 flow session 안에 보관할 임시 이름입니다."))
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Object")
	bool bReuseSlotActor = true;
};

/** @brief runtime slot의 객체를 월드 컨텍스트 entry로 등록하는 primitive 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "runtime slot의 객체를 월드 컨텍스트 entry로 등록하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventRegisterContextEntryNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "등록할 객체를 찾거나, context에서 찾은 객체를 현재 flow session에 다시 저장할 runtime slot key입니다."))
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "등록할 객체를 먼저 가져올 world context key입니다. None이면 ContextKey를 source context key로 사용합니다."))
	FName SourceContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "WorldContextSubsystem에 등록할 전역 조회 키입니다. RuntimeSlotKey와 달리 flow session 밖에서도 찾기 위한 이름입니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "등록할 객체를 RuntimeSlotKey와 SourceContextKey/ContextKey 중 어디에서 찾을지 정합니다."))
	EVdjmRecordEventObjectLookupPolicy LookupPolicy = EVdjmRecordEventObjectLookupPolicy::ERuntimeSlotOnly;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context")
	TSubclassOf<UObject> ExpectedClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "context에서 찾은 객체를 RuntimeSlotKey에도 다시 저장합니다. subgraph/session 안에서 후속 event가 runtime slot처럼 쓰게 하는 용도입니다."))
	bool bRefreshRuntimeSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "true면 source 객체가 없어도 성공 처리합니다. 이미 등록되어 있으면 갱신하고, 없으면 그냥 지나가야 하는 optional UI에 사용합니다."))
	bool bSucceedIfMissing = false;
};

/** @brief runtime slot의 위젯 객체를 월드 컨텍스트 entry로 등록하는 primitive 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "runtime slot의 위젯을 월드 컨텍스트 entry로 등록하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventRegisterWidgetContextNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "등록할 위젯을 찾거나, context에서 찾은 위젯을 현재 flow session에 다시 저장할 runtime slot key입니다."))
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "등록할 위젯을 먼저 가져올 world context key입니다. None이면 ContextKey를 source context key로 사용합니다."))
	FName SourceContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "WorldContextSubsystem에 등록할 전역 조회 키입니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "등록할 위젯을 RuntimeSlotKey와 SourceContextKey/ContextKey 중 어디에서 찾을지 정합니다."))
	EVdjmRecordEventObjectLookupPolicy LookupPolicy = EVdjmRecordEventObjectLookupPolicy::ERuntimeSlotOnly;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "context에서 찾은 위젯을 RuntimeSlotKey에도 다시 저장합니다. subgraph/session 안에서 후속 event가 runtime slot처럼 쓰게 하는 용도입니다."))
	bool bRefreshRuntimeSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context", meta = (ToolTip = "true면 source 위젯이 없어도 성공 처리합니다. optional UI context 갱신에 사용합니다."))
	bool bSucceedIfMissing = false;
};

/** @brief 위젯을 생성하고 필요하면 뷰포트에 붙이고 runtime slot에 저장하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "위젯을 생성하고 필요하면 뷰포트에 붙이는 노드"))
class VDJMRECORDER_API UVdjmRecordEventCreateWidgetNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ClampMin = "0"))
	int32 PlayerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	bool bRequireOwningPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	bool bReuseCreatedWidget = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	bool bAddToViewport = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	int32 ZOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "생성한 위젯을 현재 flow session 안에 보관할 임시 이름입니다. None이면 slot 저장 없이 생성/표시만 합니다."))
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "위젯 생성 후 즉시 발행할 signal 이름입니다. WaitForSignalNode/EmitSignalNode의 SignalTag와 같은 신호 계층입니다."))
	FName EmitSignalTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget|Destroy", meta = (ToolTip = "위젯 생성 후 자동 제거를 언제 수행할지 정합니다. None이면 기존 CreateWidget처럼 즉시 성공합니다."))
	EVdjmRecordEventWidgetDestroyPolicy DestroyPolicy = EVdjmRecordEventWidgetDestroyPolicy::ENone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget|Destroy", meta = (ToolTip = "DestroyPolicy가 RemoveOnSignal일 때 기다릴 signal입니다. 이 signal을 받으면 위젯을 제거하고 다음 flow로 진행합니다."))
	FName DestroySignalTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget|Destroy", meta = (ToolTip = "DestroyPolicy가 RemoveOnSignal일 때의 대기 방식입니다."))
	EVdjmRecordEventConditionMode DestroyConditionMode = EVdjmRecordEventConditionMode::EConditional;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget|Destroy", meta = (ClampMin = "0.0", ToolTip = "DestroyPolicy가 RemoveAfterDelay일 때 위젯 제거까지 기다릴 시간입니다."))
	float DestroyDelaySeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget|Destroy", meta = (ToolTip = "자동 제거 시 RuntimeSlotKey에 저장된 위젯 참조를 함께 정리합니다."))
	bool bClearRuntimeSlotOnDestroy = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget|Destroy", meta = (ToolTip = "자동 제거 시 WorldContextSubsystem의 위젯 context도 함께 제거합니다."))
	bool bUnregisterContextOnDestroy = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget|Destroy", meta = (ToolTip = "자동 제거 시 제거할 context key입니다. None이면 context 제거를 하지 않습니다."))
	FName DestroyContextKey = NAME_None;

private:
	void RemoveRuntimeWidget(UVdjmRecordEventManager* eventManager);

	TWeakObjectPtr<UUserWidget> RuntimeWidget;
	bool bHasEmittedSignal = false;
	bool bDestroyWaitStarted = false;
	double DestroyStartSeconds = 0.0;
};

/** @brief runtime slot의 위젯을 화면에서 제거하고 필요하면 slot/context를 정리하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "runtime slot의 위젯을 제거하고 필요하면 slot/context를 정리하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventRemoveWidgetNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "위젯을 찾을 때 runtime slot과 world context를 어떤 순서로 조회할지 정합니다."))
	EVdjmRecordEventWidgetLookupPolicy LookupPolicy = EVdjmRecordEventWidgetLookupPolicy::ERuntimeSlotThenContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	bool bClearRuntimeSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	bool bUnregisterContext = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	bool bSucceedIfMissing = false;
};

/** @brief runtime slot에 있는 위젯을 화면에 올리는 노드다. SlotKey가 없으면 위젯 stack cursor를 앞으로 이동해 표시한다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "runtime slot의 위젯을 화면에 표시하거나, 최근 위젯 stack cursor를 앞으로 이동해 표시하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventShowWidgetNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "표시할 위젯의 runtime slot key입니다. None이면 최근 위젯 stack cursor를 CursorDelta만큼 이동해 표시합니다."))
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "WorldContextSubsystem에서 위젯을 찾을 전역 key입니다. None이면 RuntimeSlotKey를 context fallback key로 사용합니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "위젯을 찾을 때 runtime slot과 world context를 어떤 순서로 조회할지 정합니다."))
	EVdjmRecordEventWidgetLookupPolicy LookupPolicy = EVdjmRecordEventWidgetLookupPolicy::ERuntimeSlotThenContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "RuntimeSlotKey가 None일 때 stack cursor를 얼마나 이동할지 정합니다. 1이면 다음/최근 방향으로 한 칸 올립니다."))
	int32 CursorDelta = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	int32 ZOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "true면 새 위젯을 올리기 전에 이전 cursor 위젯을 화면에서 내립니다."))
	bool bLowerPreviousWidget = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "true면 AddToViewport 이후 Visibility를 Visible로 맞춥니다."))
	bool bSetVisibleOnShow = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "기존 DataAsset 호환용 옵션입니다. 현재 ShowWidget은 화면 전환용 no-op 명령이므로 표시할 위젯이 없어도 항상 성공 처리합니다."))
	bool bSucceedIfMissing = true;
};

/** @brief runtime slot에 있는 위젯을 화면에서만 내리는 노드다. SlotKey가 없으면 최근 위젯부터 LowerCount개를 내린다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "runtime slot의 위젯을 화면에서만 내리거나, 최근 위젯 stack 기준으로 여러 개를 내리는 노드"))
class VDJMRECORDER_API UVdjmRecordEventLowerWidgetNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "내릴 위젯의 runtime slot key입니다. None이면 최근 위젯 stack에서 LowerCount개를 내립니다."))
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "WorldContextSubsystem에서 위젯을 찾을 전역 key입니다. None이면 RuntimeSlotKey를 context fallback key로 사용합니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "위젯을 찾을 때 runtime slot과 world context를 어떤 순서로 조회할지 정합니다."))
	EVdjmRecordEventWidgetLookupPolicy LookupPolicy = EVdjmRecordEventWidgetLookupPolicy::ERuntimeSlotThenContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ClampMin = "1", ToolTip = "RuntimeSlotKey가 None일 때 최근 위젯부터 몇 개를 내릴지 정합니다. stack보다 커도 가능한 만큼만 내리고 성공합니다."))
	int32 LowerCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "true면 직접 지정한 위젯이 현재 cursor 위젯일 때 cursor를 이전 위젯으로 내립니다."))
	bool bMoveCursorAfterDirectLower = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget", meta = (ToolTip = "기존 DataAsset 호환용 옵션입니다. 현재 LowerWidget은 화면 정리용 no-op 명령이므로 내릴 위젯이 없어도 항상 성공 처리합니다."))
	bool bSucceedIfMissing = true;
};

/** @brief 지정한 signal이 들어올 때까지 반복 대기하는 primitive 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "지정한 signal이 들어올 때까지 대기하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventWaitForSignalNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl", meta = (ToolTip = "대기할 신호 이름입니다. EmitSignalNode 또는 다른 노드가 같은 SignalTag를 발행하면 진행됩니다."))
	FName SignalTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl", meta = (ToolTip = "signal 조건을 기다릴 때의 처리 방식입니다. Running은 기존 tick polling, Passive는 signal emit 때만 재개, Conditional은 manager가 조건 감시도 함께 수행합니다."))
	EVdjmRecordEventConditionMode ConditionMode = EVdjmRecordEventConditionMode::ERunning;
};

/** @brief 상대 시간 기준으로 일정 시간 대기하는 delay 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "지정한 시간만큼 대기하는 delay 노드"))
class VDJMRECORDER_API UVdjmRecordEventDelayNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl", meta = (ClampMin = "0.0"))
	float DelaySeconds = 0.0f;

private:
	bool bDelayStarted = false;
	double DelayStartSeconds = 0.0;
};

/** @brief 지정한 signal을 발행하는 primitive 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "지정한 signal을 발행하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventEmitSignalNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void CollectFlowManifest(FVdjmRecordEventFlowManifest& manifest, int32 eventIndex) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl", meta = (ToolTip = "발행할 신호 이름입니다. WaitForSignalNode의 SignalTag와 맞춰 사용합니다."))
	FName SignalTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl")
	FVdjmRecordEventSignalRoute SignalRoute;
};

/** @brief 지정한 subgraph를 새 flow session으로 즉시 시작하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "EventFlow DataAsset 안의 named subgraph를 독립 session으로 시작하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventStartSubgraphSessionNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph", meta = (ToolTip = "subgraph를 포함한 EventFlow DataAsset입니다. None이면 현재 main flow asset을 사용합니다."))
	TObjectPtr<UVdjmRecordEventFlowDataAsset> FlowAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph", meta = (ToolTip = "시작할 subgraph tag입니다."))
	FName SubgraphTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph")
	bool bResetRuntimeStates = true;
};

/** @brief signal이 발생했을 때 subgraph session을 시작하는 branch rule을 manager에 등록하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "SignalTag를 듣다가 BranchCases[0]부터 검사해 매칭된 subgraph를 새 session으로 시작하도록 manager에 등록하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventRegisterSubgraphSignalBranchNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph", meta = (ToolTip = "branch rule 식별자입니다. None이면 SignalTag를 key로 사용합니다."))
	FName BranchTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph", meta = (ToolTip = "이 signal이 들어오면 branch case 검사를 시작합니다."))
	FName SignalTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph", meta = (ToolTip = "subgraph를 포함한 EventFlow DataAsset입니다. None이면 현재 main flow asset을 사용합니다."))
	TObjectPtr<UVdjmRecordEventFlowDataAsset> FlowAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph", meta = (ToolTip = "BranchCases가 비어 있을 때 자동으로 만들 기본 subgraph tag입니다."))
	FName SubgraphTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph", meta = (TitleProperty = "CaseTag", ToolTip = "if/else-if/else처럼 0번부터 검사합니다. 첫 매칭 case만 실행됩니다."))
	TArray<FVdjmRecordSubgraphBranchCase> BranchCases;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph")
	bool bTriggerOnce = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph")
	bool bReplaceExisting = true;
};

/** @brief manager에 등록된 subgraph signal branch rule을 제거하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "등록된 subgraph signal branch rule을 BranchTag로 제거하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventUnregisterSubgraphSignalBranchNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph", meta = (ToolTip = "제거할 branch rule 식별자입니다."))
	FName BranchTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl|Subgraph", meta = (ToolTip = "true면 없는 branch여도 성공 처리합니다."))
	bool bSucceedIfMissing = true;
};

/** @brief UObject 생성과 컨텍스트 등록을 한 번에 수행하는 composite 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "UObject 생성과 컨텍스트 등록을 한 번에 수행하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventCreateObjectAndRegisterContextNode : public UVdjmRecordEventCreateObjectNode
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience")
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience")
	TSubclassOf<UObject> ExpectedClass;
};

/** @brief Actor 스폰과 컨텍스트 등록을 한 번에 수행하는 composite 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "Actor 스폰과 컨텍스트 등록을 한 번에 수행하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventSpawnActorAndRegisterContextNode : public UVdjmRecordEventSpawnActorNode
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience")
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience")
	TSubclassOf<UObject> ExpectedClass;
};

/** @brief 위젯 생성과 컨텍스트 등록을 한 번에 수행하는 composite 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "위젯 context를 보장하는 노드입니다. ContextKey에 등록된 위젯이 있으면 재사용하고, 없으면 생성 후 등록합니다."))
class VDJMRECORDER_API UVdjmRecordEventCreateWidgetAndRegisterContextNode : public UVdjmRecordEventCreateWidgetNode
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience", meta = (ToolTip = "WorldContextSubsystem에 등록할 전역 위젯 key입니다. 이 key가 장기 생존 UI를 flow 밖에서도 다시 찾는 기준입니다."))
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience", meta = (ToolTip = "true면 ContextKey에 이미 등록된 위젯을 새로 만들지 않고 재사용합니다. DestroyPolicy가 None일 때만 적용됩니다."))
	bool bReuseRegisteredContext = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience", meta = (ToolTip = "context 위젯을 재사용할 때 현재 flow session의 RuntimeSlotKey에도 다시 저장합니다. 이후 Show/Lower/RemoveWidget 노드가 runtime slot으로 찾을 수 있게 합니다."))
	bool bRefreshRuntimeSlotWhenReused = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience", meta = (ToolTip = "context 위젯을 재사용할 때 WorldContextSubsystem 등록 정보를 다시 갱신합니다. 약한 참조 context의 expected class를 다시 맞추는 용도입니다."))
	bool bRefreshContextRegistrationWhenReused = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience", meta = (ToolTip = "context 위젯을 재사용해 화면에 올릴 때 Visibility를 Visible로 맞춥니다."))
	bool bSetVisibleWhenReused = true;
};

/** @brief 레코더용 EnvDataAsset 경로를 전역 브릿지 설정에 반영하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "레코더용 EnvDataAsset 경로를 브릿지 설정에 반영하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventSetEnvDataAssetPathNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific")
	FSoftObjectPath EnvDataAssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|RecorderSpecific")
	bool bRequireLoadSuccess = false;
};
