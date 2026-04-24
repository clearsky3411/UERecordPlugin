#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.h"
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
 * - Debug
 *   - `UVdjmRecordEventLogNode`
 * - RecorderSpecific & Legacy
 *   - `UVdjmRecordEventSpawnRecordBridgeActorWait`
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
class AActor;
class UUserWidget;
class UVdjmRecordEventManager;
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

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow")
	static FVdjmRecordEventResult MakeResult(EVdjmRecordEventResultType InResultType, int32 InSelectedIndex, FName InJumpLabel, float InWaitSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Core")
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

/** @brief 브릿지 액터를 준비하고 시작 신호 및 초기화 완료까지 대기하는 브릿지 특화 composite 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "브릿지 액터를 준비하고 시작 신호 및 초기화 완료까지 대기하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventSpawnRecordBridgeActorWait : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;

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

private:
	bool ResolveRuntimeBridge(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor*& OutBridgeActor);
	bool ApplyBridgePreStartSettings(AVdjmRecordBridgeActor* InBridgeActor) const;
	bool CanTreatAsInitSuccess(const AVdjmRecordBridgeActor* InBridgeActor) const;
	bool IsInitFailureStep(EVdjmRecordBridgeInitStep InInitStep) const;

	TWeakObjectPtr<AVdjmRecordBridgeActor> RuntimeBridgeActor;
	bool bHasIssuedStart = false;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Object")
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Object")
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context")
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context")
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context")
	TSubclassOf<UObject> ExpectedClass;
};

/** @brief runtime slot의 위젯 객체를 월드 컨텍스트 entry로 등록하는 primitive 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "runtime slot의 위젯을 월드 컨텍스트 entry로 등록하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventRegisterWidgetContextNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context")
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Context")
	FName ContextKey = NAME_None;
};

/** @brief 위젯을 생성하고 필요하면 뷰포트에 붙이고 runtime slot에 저장하는 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "위젯을 생성하고 필요하면 뷰포트에 붙이는 노드"))
class VDJMRECORDER_API UVdjmRecordEventCreateWidgetNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;
	virtual void ResetRuntimeState_Implementation() override;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	FName RuntimeSlotKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	FName EmitSignalTag = NAME_None;

private:
	TWeakObjectPtr<UUserWidget> RuntimeWidget;
	bool bHasEmittedSignal = false;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	bool bClearRuntimeSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	bool bUnregisterContext = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Primitive|Widget")
	bool bSucceedIfMissing = false;
};

/** @brief 지정한 signal이 들어올 때까지 반복 대기하는 primitive 노드다. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "지정한 signal이 들어올 때까지 대기하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventWaitForSignalNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl")
	FName SignalTag = NAME_None;
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl")
	FName SignalTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|FlowControl")
	EVdjmRecordEventSignalScope SignalScope = EVdjmRecordEventSignalScope::ECurrentSession;
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
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "위젯 생성과 컨텍스트 등록을 한 번에 수행하는 노드"))
class VDJMRECORDER_API UVdjmRecordEventCreateWidgetAndRegisterContextNode : public UVdjmRecordEventCreateWidgetNode
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventNode|Composite|Convenience")
	FName ContextKey = NAME_None;
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
