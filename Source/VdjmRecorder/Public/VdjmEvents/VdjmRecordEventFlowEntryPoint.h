#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecordEventFlowEntryPoint.generated.h"

class UUserWidget;
class UVdjmRecordEventFlowDataAsset;

UENUM(BlueprintType)
enum class EVdjmRecordEventPreStartWidgetPolicy : uint8
{
	EKeepVisible UMETA(DisplayName = "Keep Visible"),
	ERemoveOnFlowStart UMETA(DisplayName = "Remove On Flow Start")
};

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordEventStartupContextBinding
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	FName ContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	TObjectPtr<UObject> ContextObject = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	TSubclassOf<UObject> ExpectedClass;
};

/**
 * @brief 레벨에 배치해서 EventManager 생성과 EventFlow 시작을 시동하는 AInfo 기반 진입점이다.
 *
 * - FlowDataAsset 소비
 * - BeginPlay 자동 시동
 * - 시작 지연
 * - 시작 후 초기 signal 발행
 * 를 한 곳에 모아둔다.
 */
UCLASS(BlueprintType, Blueprintable, meta = (ToolTip = "레벨에서 EventManager 생성과 EventFlow 시작을 시동하는 AInfo 기반 진입점"))
class VDJMRECORDER_API AVdjmRecordEventFlowEntryPoint : public AInfo
{
	GENERATED_BODY()

public:
	AVdjmRecordEventFlowEntryPoint();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|EntryPoint")
	bool CreateEventManager();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|EntryPoint")
	bool StartConfiguredFlow();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|EntryPoint")
	void StopConfiguredFlow();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|EntryPoint")
	bool EmitConfiguredSignals();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|EntryPoint")
	bool RegisterStartupContexts();

	UFUNCTION(BlueprintCallable, Category = "Recorder|EventFlow|EntryPoint")
	bool PreparePreStartWidget();

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|EntryPoint")
	UVdjmRecordEventManager* GetEventManager() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|EventFlow|EntryPoint")
	bool HasValidEventManager() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	TObjectPtr<UVdjmRecordEventFlowDataAsset> FlowDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	bool bAutoCreateEventManagerOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	bool bAutoStartFlowOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	bool bTryBindExistingBridgeOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	bool bResetRuntimeStatesOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	bool bStopFlowOnEndPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint", meta = (ClampMin = "0.0"))
	float StartDelaySeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint", meta = (ToolTip = "Flow 시작 성공 직후 발행하는 초기 Signal 목록"))
	TArray<FName> InitialSignalTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	TArray<FVdjmRecordEventStartupContextBinding> StartupContextBindings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	TSubclassOf<UUserWidget> PreStartWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	bool bCreatePreStartWidgetBeforeFlowStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	bool bAddPreStartWidgetToViewport = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint", meta = (ClampMin = "0"))
	int32 PreStartWidgetPlayerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	bool bRequirePreStartOwningPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	bool bReusePreStartWidget = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	EVdjmRecordEventPreStartWidgetPolicy PreStartWidgetPolicy = EVdjmRecordEventPreStartWidgetPolicy::EKeepVisible;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	FName PreStartWidgetContextKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|EntryPoint")
	int32 PreStartWidgetZOrder = 1000;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleManagedFlowFinished(
		UVdjmRecordEventManager* EventManager,
		FVdjmRecordFlowSessionHandle SessionHandle,
		UVdjmRecordEventFlowDataAsset* FinishedFlowAsset,
		EVdjmRecordEventResultType FinalResult);

	void HandleAutoStart();
	void HandleDeferredStart();
	bool TryBindExistingBridge();
	bool IsStartOwnershipHeldByThis() const;
	bool TryClaimStartOwnership(bool& bOutAlreadyOwnedByThis);
	void ReleaseStartOwnership();
	bool RegisterStartupContextBinding(const FVdjmRecordEventStartupContextBinding& StartupContextBinding);
	void ApplyPreStartWidgetPolicyOnFlowStart();
	void ClearPendingStartTimer();

private:
	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordEventManager> RuntimeEventManager = nullptr;

	UPROPERTY(Transient)
	FTimerHandle DeferredStartTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> RuntimePreStartWidget = nullptr;
};
