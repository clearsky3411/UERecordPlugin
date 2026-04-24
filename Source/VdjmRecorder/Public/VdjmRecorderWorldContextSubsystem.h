#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VdjmRecorderWorldContextSubsystem.generated.h"

class AVdjmRecordBridgeActor;

USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecorderWorldContextValidationItem
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|WorldContext")
	FName ContextKey = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|WorldContext")
	bool bIsRegistered = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|WorldContext")
	bool bIsValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|WorldContext")
	FString EntryClassName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|WorldContext")
	FString ContextClassName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|WorldContext")
	FString ValidationMessage;
};

UCLASS(Abstract, BlueprintType)
class VDJMRECORDER_API UVdjmRecorderWorldContextEntry : public UObject
{
	GENERATED_BODY()

public:
	void InitializeContextEntry(FName InContextKey);

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	FName GetContextKey() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	UObject* GetContextObject() const;

	bool ValidateContext(FString& OutValidationMessage) const;

protected:
	virtual UObject* ResolveContextObjectInternal() const;
	virtual bool ValidateContextInternal(FString& OutValidationMessage) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Recorder|WorldContext")
	FName ContextKey = NAME_None;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecorderWeakObjectContextEntry : public UVdjmRecorderWorldContextEntry
{
	GENERATED_BODY()

public:
	void SetContextObject(UObject* InContextObject);
	void SetExpectedClass(UClass* InExpectedClass);

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	UClass* GetExpectedClass() const;

protected:
	virtual UObject* ResolveContextObjectInternal() const override;
	virtual bool ValidateContextInternal(FString& OutValidationMessage) const override;

private:
	TWeakObjectPtr<UObject> WeakContextObject;

	UPROPERTY(Transient)
	TObjectPtr<UClass> ExpectedClass = nullptr;
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecorderBridgeWorldContextEntry : public UVdjmRecorderWorldContextEntry
{
	GENERATED_BODY()

public:
	void SetBridgeActor(AVdjmRecordBridgeActor* InBridgeActor);

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	AVdjmRecordBridgeActor* GetBridgeActor() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	bool IsBridgeInitializationComplete() const;

protected:
	virtual UObject* ResolveContextObjectInternal() const override;
	virtual bool ValidateContextInternal(FString& OutValidationMessage) const override;

private:
	TWeakObjectPtr<AVdjmRecordBridgeActor> WeakBridgeActor;
};

UCLASS()
class VDJMRECORDER_API UVdjmRecorderWorldContextSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext", meta = (WorldContext = "WorldContextObject"))
	static UVdjmRecorderWorldContextSubsystem* Get(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	static FName GetBridgeContextKey();

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	static FName GetEventManagerContextKey();

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	static FName GetRecorderControllerContextKey();

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	static FName GetStateObserverContextKey();

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	static FName GetEventFlowEntryPointContextKey();

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	UVdjmRecorderWorldContextEntry* FindContextEntry(FName InContextKey) const;

	UVdjmRecorderWorldContextEntry* GetOrCreateContextEntry(FName InContextKey, TSubclassOf<UVdjmRecorderWorldContextEntry> InEntryClass);

	bool RegisterWeakObjectContext(FName InContextKey, UObject* InContextObject, UClass* InExpectedClass = nullptr);
	bool RegisterBridgeContext(AVdjmRecordBridgeActor* InBridgeActor);

	UFUNCTION(BlueprintCallable, Category = "Recorder|WorldContext")
	bool UnregisterContext(FName InContextKey);

	bool UnregisterContext(FName InContextKey, UVdjmRecorderWorldContextEntry* InExpectedEntry);

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	UObject* FindContextObject(FName InContextKey) const;

	bool ValidateContext(FName InContextKey, FString& OutValidationMessage) const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|WorldContext")
	TArray<FVdjmRecorderWorldContextValidationItem> BuildValidationSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	UVdjmRecorderBridgeWorldContextEntry* GetBridgeContextEntry() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|WorldContext")
	AVdjmRecordBridgeActor* GetBridgeActor() const;

	template <typename EntryType>
	EntryType* FindContextEntryAs(FName InContextKey) const
	{
		return Cast<EntryType>(FindContextEntry(InContextKey));
	}

private:
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UVdjmRecorderWorldContextEntry>> ContextEntries;
};
