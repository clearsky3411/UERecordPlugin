#include "VdjmRecorderWorldContextSubsystem.h"

#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecordBridgeActor.h"
#include "VdjmRecorderController.h"
#include "VdjmRecorderStateObserver.h"

namespace
{
	const FName BRIDGE_CONTEXT_KEY(TEXT("BridgeActor"));
	const FName EVENT_MANAGER_CONTEXT_KEY(TEXT("EventManager"));
	const FName RECORDER_CONTROLLER_CONTEXT_KEY(TEXT("RecorderController"));
	const FName STATE_OBSERVER_CONTEXT_KEY(TEXT("StateObserver"));
	const FName EVENT_FLOW_ENTRY_POINT_CONTEXT_KEY(TEXT("EventFlowEntryPoint"));
}

void UVdjmRecorderWorldContextEntry::InitializeContextEntry(FName InContextKey)
{
	ContextKey = InContextKey;
}

FName UVdjmRecorderWorldContextEntry::GetContextKey() const
{
	return ContextKey;
}

UObject* UVdjmRecorderWorldContextEntry::GetContextObject() const
{
	return ResolveContextObjectInternal();
}

bool UVdjmRecorderWorldContextEntry::ValidateContext(FString& OutValidationMessage) const
{
	return ValidateContextInternal(OutValidationMessage);
}

UObject* UVdjmRecorderWorldContextEntry::ResolveContextObjectInternal() const
{
	return nullptr;
}

bool UVdjmRecorderWorldContextEntry::ValidateContextInternal(FString& OutValidationMessage) const
{
	OutValidationMessage = TEXT("Context entry has no validation rule.");
	return false;
}

void UVdjmRecorderWeakObjectContextEntry::SetContextObject(UObject* InContextObject)
{
	WeakContextObject = InContextObject;
}

void UVdjmRecorderWeakObjectContextEntry::SetExpectedClass(UClass* InExpectedClass)
{
	ExpectedClass = InExpectedClass;
}

UClass* UVdjmRecorderWeakObjectContextEntry::GetExpectedClass() const
{
	return ExpectedClass;
}

UObject* UVdjmRecorderWeakObjectContextEntry::ResolveContextObjectInternal() const
{
	return WeakContextObject.Get();
}

bool UVdjmRecorderWeakObjectContextEntry::ValidateContextInternal(FString& OutValidationMessage) const
{
	UObject* ContextObject = WeakContextObject.Get();
	if (ContextObject == nullptr)
	{
		OutValidationMessage = TEXT("Context object is not available.");
		return false;
	}

	if (ExpectedClass != nullptr && !ContextObject->IsA(ExpectedClass))
	{
		OutValidationMessage = FString::Printf(
			TEXT("Context object class mismatch. Expected '%s' but got '%s'."),
			*ExpectedClass->GetName(),
			*ContextObject->GetClass()->GetName());
		return false;
	}

	OutValidationMessage = FString::Printf(TEXT("Context object '%s' is available."), *ContextObject->GetName());
	return true;
}

void UVdjmRecorderBridgeWorldContextEntry::SetBridgeActor(AVdjmRecordBridgeActor* InBridgeActor)
{
	WeakBridgeActor = InBridgeActor;
}

AVdjmRecordBridgeActor* UVdjmRecorderBridgeWorldContextEntry::GetBridgeActor() const
{
	return WeakBridgeActor.Get();
}

bool UVdjmRecorderBridgeWorldContextEntry::IsBridgeInitializationComplete() const
{
	if (const AVdjmRecordBridgeActor* BridgeActor = WeakBridgeActor.Get())
	{
		return BridgeActor->DbcValidInitializeComplete();
	}

	return false;
}

UObject* UVdjmRecorderBridgeWorldContextEntry::ResolveContextObjectInternal() const
{
	return WeakBridgeActor.Get();
}

bool UVdjmRecorderBridgeWorldContextEntry::ValidateContextInternal(FString& OutValidationMessage) const
{
	AVdjmRecordBridgeActor* BridgeActor = WeakBridgeActor.Get();
	if (BridgeActor == nullptr)
	{
		OutValidationMessage = TEXT("Bridge actor is not registered.");
		return false;
	}

	const EVdjmRecordBridgeInitStep CurrentInitStep = BridgeActor->GetCurrentInitStep();
	if (CurrentInitStep == EVdjmRecordBridgeInitStep::EInitError ||
		CurrentInitStep == EVdjmRecordBridgeInitStep::EInitErrorEnd)
	{
		OutValidationMessage = FString::Printf(
			TEXT("Bridge actor is in error state. CurrentStep='%s'."),
			AVdjmRecordBridgeActor::GetInitStepName(CurrentInitStep));
		return false;
	}

	if (!BridgeActor->DbcValidInitializeComplete())
	{
		OutValidationMessage = FString::Printf(
			TEXT("Bridge actor is not ready yet. CurrentStep='%s'."),
			AVdjmRecordBridgeActor::GetInitStepName(CurrentInitStep));
		return false;
	}

	OutValidationMessage = FString::Printf(
		TEXT("Bridge actor '%s' is initialized. CurrentStep='%s'."),
		*BridgeActor->GetName(),
		AVdjmRecordBridgeActor::GetInitStepName(CurrentInitStep));
	return true;
}

UVdjmRecorderWorldContextSubsystem* UVdjmRecorderWorldContextSubsystem::Get(UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	return World->GetSubsystem<UVdjmRecorderWorldContextSubsystem>();
}

FName UVdjmRecorderWorldContextSubsystem::GetBridgeContextKey()
{
	return BRIDGE_CONTEXT_KEY;
}

FName UVdjmRecorderWorldContextSubsystem::GetEventManagerContextKey()
{
	return EVENT_MANAGER_CONTEXT_KEY;
}

FName UVdjmRecorderWorldContextSubsystem::GetRecorderControllerContextKey()
{
	return RECORDER_CONTROLLER_CONTEXT_KEY;
}

FName UVdjmRecorderWorldContextSubsystem::GetStateObserverContextKey()
{
	return STATE_OBSERVER_CONTEXT_KEY;
}

FName UVdjmRecorderWorldContextSubsystem::GetEventFlowEntryPointContextKey()
{
	return EVENT_FLOW_ENTRY_POINT_CONTEXT_KEY;
}

UVdjmRecorderWorldContextEntry* UVdjmRecorderWorldContextSubsystem::FindContextEntry(FName InContextKey) const
{
	if (InContextKey.IsNone())
	{
		return nullptr;
	}

	const TObjectPtr<UVdjmRecorderWorldContextEntry>* FoundEntry = ContextEntries.Find(InContextKey);
	return FoundEntry != nullptr ? FoundEntry->Get() : nullptr;
}

UVdjmRecorderWorldContextEntry* UVdjmRecorderWorldContextSubsystem::GetOrCreateContextEntry(
	FName InContextKey,
	TSubclassOf<UVdjmRecorderWorldContextEntry> InEntryClass)
{
	if (InContextKey.IsNone() || !InEntryClass)
	{
		return nullptr;
	}

	if (UVdjmRecorderWorldContextEntry* ExistingEntry = FindContextEntry(InContextKey))
	{
		if (ExistingEntry->IsA(InEntryClass))
		{
			return ExistingEntry;
		}

		ContextEntries.Remove(InContextKey);
	}

	UVdjmRecorderWorldContextEntry* NewEntry = NewObject<UVdjmRecorderWorldContextEntry>(this, InEntryClass, NAME_None, RF_Transient);
	if (NewEntry == nullptr)
	{
		return nullptr;
	}

	NewEntry->InitializeContextEntry(InContextKey);
	ContextEntries.Add(InContextKey, NewEntry);
	return NewEntry;
}

bool UVdjmRecorderWorldContextSubsystem::RegisterWeakObjectContext(
	FName InContextKey,
	UObject* InContextObject,
	UClass* InExpectedClass)
{
	if (InContextKey.IsNone() || InContextObject == nullptr)
	{
		return false;
	}

	UVdjmRecorderWeakObjectContextEntry* Entry = FindContextEntryAs<UVdjmRecorderWeakObjectContextEntry>(InContextKey);
	if (Entry == nullptr)
	{
		Entry = Cast<UVdjmRecorderWeakObjectContextEntry>(
			GetOrCreateContextEntry(InContextKey, UVdjmRecorderWeakObjectContextEntry::StaticClass()));
	}

	if (Entry == nullptr)
	{
		return false;
	}

	Entry->SetContextObject(InContextObject);
	Entry->SetExpectedClass(InExpectedClass != nullptr ? InExpectedClass : InContextObject->GetClass());
	return true;
}

bool UVdjmRecorderWorldContextSubsystem::RegisterBridgeContext(AVdjmRecordBridgeActor* InBridgeActor)
{
	if (InBridgeActor == nullptr)
	{
		return false;
	}

	UVdjmRecorderBridgeWorldContextEntry* Entry = GetBridgeContextEntry();
	if (Entry == nullptr)
	{
		Entry = Cast<UVdjmRecorderBridgeWorldContextEntry>(
			GetOrCreateContextEntry(GetBridgeContextKey(), UVdjmRecorderBridgeWorldContextEntry::StaticClass()));
	}

	if (Entry == nullptr)
	{
		return false;
	}

	Entry->SetBridgeActor(InBridgeActor);
	return true;
}

bool UVdjmRecorderWorldContextSubsystem::UnregisterContext(FName InContextKey)
{
	return UnregisterContext(InContextKey, nullptr);
}

bool UVdjmRecorderWorldContextSubsystem::UnregisterContext(FName InContextKey, UVdjmRecorderWorldContextEntry* InExpectedEntry)
{
	if (InContextKey.IsNone())
	{
		return false;
	}

	UVdjmRecorderWorldContextEntry* ExistingEntry = FindContextEntry(InContextKey);
	if (ExistingEntry == nullptr)
	{
		return false;
	}

	if (InExpectedEntry != nullptr && ExistingEntry != InExpectedEntry)
	{
		return false;
	}

	ContextEntries.Remove(InContextKey);
	return true;
}

UObject* UVdjmRecorderWorldContextSubsystem::FindContextObject(FName InContextKey) const
{
	if (const UVdjmRecorderWorldContextEntry* Entry = FindContextEntry(InContextKey))
	{
		return Entry->GetContextObject();
	}

	return nullptr;
}

bool UVdjmRecorderWorldContextSubsystem::ValidateContext(FName InContextKey, FString& OutValidationMessage) const
{
	OutValidationMessage.Reset();

	const UVdjmRecorderWorldContextEntry* Entry = FindContextEntry(InContextKey);
	if (Entry == nullptr)
	{
		OutValidationMessage = FString::Printf(TEXT("Context '%s' is not registered."), *InContextKey.ToString());
		return false;
	}

	return Entry->ValidateContext(OutValidationMessage);
}

TArray<FVdjmRecorderWorldContextValidationItem> UVdjmRecorderWorldContextSubsystem::BuildValidationSnapshot() const
{
	TArray<FVdjmRecorderWorldContextValidationItem> Result;
	Result.Reserve(ContextEntries.Num());

	for (const TPair<FName, TObjectPtr<UVdjmRecorderWorldContextEntry>>& Pair : ContextEntries)
	{
		FVdjmRecorderWorldContextValidationItem Item;
		Item.ContextKey = Pair.Key;
		Item.bIsRegistered = Pair.Value != nullptr;

		if (Pair.Value != nullptr)
		{
			FString ValidationMessage;
			Item.bIsValid = Pair.Value->ValidateContext(ValidationMessage);
			Item.EntryClassName = Pair.Value->GetClass()->GetName();
			Item.ValidationMessage = ValidationMessage;

			if (UObject* ContextObject = Pair.Value->GetContextObject())
			{
				Item.ContextClassName = ContextObject->GetClass()->GetName();
			}
		}
		else
		{
			Item.ValidationMessage = TEXT("Context entry pointer is null.");
		}

		Result.Add(MoveTemp(Item));
	}

	Result.Sort([](const FVdjmRecorderWorldContextValidationItem& Left, const FVdjmRecorderWorldContextValidationItem& Right)
	{
		return Left.ContextKey.LexicalLess(Right.ContextKey);
	});

	return Result;
}

UVdjmRecorderBridgeWorldContextEntry* UVdjmRecorderWorldContextSubsystem::GetBridgeContextEntry() const
{
	return FindContextEntryAs<UVdjmRecorderBridgeWorldContextEntry>(GetBridgeContextKey());
}

AVdjmRecordBridgeActor* UVdjmRecorderWorldContextSubsystem::GetBridgeActor() const
{
	if (const UVdjmRecorderBridgeWorldContextEntry* Entry = GetBridgeContextEntry())
	{
		return Entry->GetBridgeActor();
	}

	return nullptr;
}
