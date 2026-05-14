#include "VdjmEvents/VdjmRecordEventFlowEntryPoint.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "VdjmRecordBridgeActor.h"
#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmEvents/VdjmRecordEventWidgetBase.h"
#include "VdjmRecorderCore.h"
#include "VdjmRecorderWorldContextSubsystem.h"

AVdjmRecordEventFlowEntryPoint::AVdjmRecordEventFlowEntryPoint()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AVdjmRecordEventFlowEntryPoint::CreateEventManager()
{
	if (RuntimeEventManager != nullptr)
	{
		return true;
	}

	if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this))
	{
		if (UVdjmRecordEventManager* existingEventManager = Cast<UVdjmRecordEventManager>(
			worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetEventManagerContextKey())))
		{
			RuntimeEventManager = existingEventManager;
			RuntimeEventManager->OnEventFlowSessionFinished.AddUniqueDynamic(this, &AVdjmRecordEventFlowEntryPoint::HandleManagedFlowFinished);

			if (bTryBindExistingBridgeOnStart)
			{
				TryBindExistingBridge();
			}

			return true;
		}
	}

	RuntimeEventManager = UVdjmRecordEventManager::CreateEventManager(this);
	if (RuntimeEventManager == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("EventFlowEntryPoint - Failed to create event manager."));
		return false;
	}

	RuntimeEventManager->OnEventFlowSessionFinished.AddUniqueDynamic(this, &AVdjmRecordEventFlowEntryPoint::HandleManagedFlowFinished);

	if (bTryBindExistingBridgeOnStart)
	{
		TryBindExistingBridge();
	}

	return true;
}

bool AVdjmRecordEventFlowEntryPoint::StartConfiguredFlow()
{
	if (FlowDataAsset == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("EventFlowEntryPoint - FlowDataAsset is not assigned."));
		return false;
	}

	if (not CreateEventManager())
	{
		return false;
	}

	bool bAlreadyOwnedByThis = false;
	if (not TryClaimStartOwnership(bAlreadyOwnedByThis))
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("EventFlowEntryPoint - Another entry point already owns flow startup in this world."));
		return false;
	}

	if (bTryBindExistingBridgeOnStart)
	{
		TryBindExistingBridge();
	}

	if (not RegisterStartupContexts())
	{
		if (not bAlreadyOwnedByThis)
		{
			ReleaseStartOwnership();
		}

		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("EventFlowEntryPoint - Failed to register startup contexts."));
		return false;
	}

	if (bCreatePreStartWidgetBeforeFlowStart && not PreparePreStartWidget())
	{
		if (not bAlreadyOwnedByThis)
		{
			ReleaseStartOwnership();
		}

		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("EventFlowEntryPoint - Failed to prepare pre-start widget."));
		return false;
	}

	if (RuntimeEventManager->IsEventFlowRunning())
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("EventFlowEntryPoint - Event flow is already running."));

		if (not bAlreadyOwnedByThis)
		{
			ReleaseStartOwnership();
		}

		return false;
	}

	const bool bStartResult = RuntimeEventManager->StartEventFlow(FlowDataAsset, bResetRuntimeStatesOnStart);
	if (not bStartResult)
	{
		if (not bAlreadyOwnedByThis)
		{
			ReleaseStartOwnership();
		}

		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("EventFlowEntryPoint - Failed to start configured flow."));
		return false;
	}

	ApplyPreStartWidgetPolicyOnFlowStart();
	EmitConfiguredSignals();
	return true;
}

void AVdjmRecordEventFlowEntryPoint::StopConfiguredFlow()
{
	if (RuntimeEventManager == nullptr)
	{
		return;
	}

	if (not IsStartOwnershipHeldByThis())
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("EventFlowEntryPoint - StopConfiguredFlow ignored because this entry point does not own the active start."));
		return;
	}

	RuntimeEventManager->StopEventFlow();
	ReleaseStartOwnership();
}

bool AVdjmRecordEventFlowEntryPoint::EmitConfiguredSignals()
{
	if (RuntimeEventManager == nullptr)
	{
		return false;
	}

	bool bEmittedAnySignal = false;
	for (const FName signalTag : InitialSignalTags)
	{
		if (signalTag.IsNone())
		{
			continue;
		}

		if (RuntimeEventManager->EmitFlowSignal(signalTag))
		{
			bEmittedAnySignal = true;
		}
	}

	return bEmittedAnySignal;
}

bool AVdjmRecordEventFlowEntryPoint::RegisterStartupContexts()
{
	for (const FVdjmRecordEventStartupContextBinding& startupContextBinding : StartupContextBindings)
	{
		if (not RegisterStartupContextBinding(startupContextBinding))
		{
			return false;
		}
	}

	return true;
}

bool AVdjmRecordEventFlowEntryPoint::PreparePreStartWidget()
{
	if (PreStartWidgetClass == nullptr)
	{
		return true;
	}

	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return false;
	}

	UUserWidget* widgetInstance = bReusePreStartWidget ? RuntimePreStartWidget.Get() : nullptr;
	if (widgetInstance == nullptr || not widgetInstance->IsA(PreStartWidgetClass))
	{
		APlayerController* owningPlayer = UGameplayStatics::GetPlayerController(world, PreStartWidgetPlayerIndex);
		if (bRequirePreStartOwningPlayer && owningPlayer == nullptr)
		{
			return false;
		}

		if (owningPlayer != nullptr)
		{
			widgetInstance = CreateWidget<UUserWidget>(owningPlayer, PreStartWidgetClass);
		}
		else
		{
			widgetInstance = CreateWidget<UUserWidget>(world, PreStartWidgetClass);
		}

		if (widgetInstance == nullptr)
		{
			return false;
		}

		RuntimePreStartWidget = widgetInstance;
	}

	if (UVdjmRecordEventWidgetBase* eventWidgetBase = Cast<UVdjmRecordEventWidgetBase>(widgetInstance))
	{
		AVdjmRecordBridgeActor* bridgeActor = RuntimeEventManager != nullptr ? RuntimeEventManager->GetBoundBridge() : nullptr;
		eventWidgetBase->ApplyEventContext(RuntimeEventManager, bridgeActor);
	}

	if (bAddPreStartWidgetToViewport && not widgetInstance->IsInViewport())
	{
		widgetInstance->AddToViewport(PreStartWidgetZOrder);
	}

	if (not PreStartWidgetContextKey.IsNone())
	{
		if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this))
		{
			if (not worldContextSubsystem->RegisterWeakObjectContext(
				PreStartWidgetContextKey,
				widgetInstance,
				widgetInstance->GetClass()))
			{
				return false;
			}
		}
	}

	return true;
}

UVdjmRecordEventManager* AVdjmRecordEventFlowEntryPoint::GetEventManager() const
{
	return RuntimeEventManager;
}

bool AVdjmRecordEventFlowEntryPoint::HasValidEventManager() const
{
	return RuntimeEventManager != nullptr;
}

void AVdjmRecordEventFlowEntryPoint::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoCreateEventManagerOnBeginPlay)
	{
		CreateEventManager();
	}

	if (bAutoStartFlowOnBeginPlay)
	{
		HandleAutoStart();
	}
}

void AVdjmRecordEventFlowEntryPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPendingStartTimer();

	if (RuntimeEventManager != nullptr)
	{
		RuntimeEventManager->OnEventFlowSessionFinished.RemoveDynamic(this, &AVdjmRecordEventFlowEntryPoint::HandleManagedFlowFinished);
	}

	if (bStopFlowOnEndPlay && RuntimeEventManager != nullptr && IsStartOwnershipHeldByThis())
	{
		RuntimeEventManager->StopEventFlow();
	}

	ReleaseStartOwnership();

	if (RuntimePreStartWidget != nullptr)
	{
		RuntimePreStartWidget->RemoveFromParent();
	}

	Super::EndPlay(EndPlayReason);
}

void AVdjmRecordEventFlowEntryPoint::HandleManagedFlowFinished(
	UVdjmRecordEventManager* EventManager,
	FVdjmRecordFlowSessionHandle SessionHandle,
	UVdjmRecordEventFlowDataAsset* FinishedFlowAsset,
	EVdjmRecordEventResultType FinalResult)
{
	(void)EventManager;
	(void)SessionHandle;
	(void)FinishedFlowAsset;
	(void)FinalResult;

	ReleaseStartOwnership();
}

void AVdjmRecordEventFlowEntryPoint::HandleAutoStart()
{
	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return;
	}

	ClearPendingStartTimer();
	if (StartDelaySeconds <= 0.0f)
	{
		StartConfiguredFlow();
		return;
	}

	world->GetTimerManager().SetTimer(
		DeferredStartTimerHandle,
		this,
		&AVdjmRecordEventFlowEntryPoint::HandleDeferredStart,
		StartDelaySeconds,
		false);
}

void AVdjmRecordEventFlowEntryPoint::HandleDeferredStart()
{
	StartConfiguredFlow();
}

bool AVdjmRecordEventFlowEntryPoint::TryBindExistingBridge()
{
	if (RuntimeEventManager == nullptr)
	{
		return false;
	}

	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return false;
	}

	AVdjmRecordBridgeActor* bridgeActor = AVdjmRecordBridgeActor::TryGetRecordBridgeActor(world);
	if (bridgeActor == nullptr)
	{
		return false;
	}

	return RuntimeEventManager->BindBridge(bridgeActor);
}

bool AVdjmRecordEventFlowEntryPoint::IsStartOwnershipHeldByThis() const
{
	const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(const_cast<AVdjmRecordEventFlowEntryPoint*>(this));
	if (worldContextSubsystem == nullptr)
	{
		return false;
	}

	return worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetEventFlowEntryPointContextKey()) == this;
}

bool AVdjmRecordEventFlowEntryPoint::TryClaimStartOwnership(bool& bOutAlreadyOwnedByThis)
{
	bOutAlreadyOwnedByThis = false;

	UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this);
	if (worldContextSubsystem == nullptr)
	{
		return false;
	}

	const FName contextKey = UVdjmRecorderWorldContextSubsystem::GetEventFlowEntryPointContextKey();
	UObject* existingOwner = worldContextSubsystem->FindContextObject(contextKey);
	if (existingOwner != nullptr)
	{
		if (existingOwner == this)
		{
			bOutAlreadyOwnedByThis = true;
			return true;
		}

		return false;
	}

	return worldContextSubsystem->RegisterWeakObjectContext(contextKey, this, StaticClass());
}

void AVdjmRecordEventFlowEntryPoint::ReleaseStartOwnership()
{
	UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this);
	if (worldContextSubsystem == nullptr)
	{
		return;
	}

	const FName contextKey = UVdjmRecorderWorldContextSubsystem::GetEventFlowEntryPointContextKey();
	if (worldContextSubsystem->FindContextObject(contextKey) == this)
	{
		worldContextSubsystem->UnregisterContext(contextKey);
	}
}

bool AVdjmRecordEventFlowEntryPoint::RegisterStartupContextBinding(
	const FVdjmRecordEventStartupContextBinding& startupContextBinding)
{
	if (startupContextBinding.ContextKey.IsNone())
	{
		return false;
	}

	if (startupContextBinding.ContextObject == nullptr)
	{
		return false;
	}

	UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(this);
	if (worldContextSubsystem == nullptr)
	{
		return false;
	}

	UClass* expectedClass = startupContextBinding.ExpectedClass != nullptr
		? startupContextBinding.ExpectedClass.Get()
		: startupContextBinding.ContextObject->GetClass();

	return worldContextSubsystem->RegisterWeakObjectContext(
		startupContextBinding.ContextKey,
		startupContextBinding.ContextObject,
		expectedClass);
}

void AVdjmRecordEventFlowEntryPoint::ApplyPreStartWidgetPolicyOnFlowStart()
{
	if (RuntimePreStartWidget == nullptr)
	{
		return;
	}

	if (PreStartWidgetPolicy == EVdjmRecordEventPreStartWidgetPolicy::ERemoveOnFlowStart)
	{
		RuntimePreStartWidget->RemoveFromParent();
	}
}

void AVdjmRecordEventFlowEntryPoint::ClearPendingStartTimer()
{
	UWorld* world = GetWorld();
	if (world == nullptr || not DeferredStartTimerHandle.IsValid())
	{
		return;
	}

	world->GetTimerManager().ClearTimer(DeferredStartTimerHandle);
}
