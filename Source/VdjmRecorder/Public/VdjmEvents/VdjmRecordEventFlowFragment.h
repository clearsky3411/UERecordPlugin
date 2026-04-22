#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UVdjmRecordEventFlowRuntime;

struct VDJMRECORDER_API FVdjmRecordEventNodeFragment
{
public:
	FVdjmRecordEventNodeFragment();
	explicit FVdjmRecordEventNodeFragment(const UClass* InEventClass);
	explicit FVdjmRecordEventNodeFragment(const FSoftClassPath& InEventClassPath);

	static FVdjmRecordEventNodeFragment Make(const UClass* InEventClass);

	template<typename TEventClass>
	static FVdjmRecordEventNodeFragment Make()
	{
		return FVdjmRecordEventNodeFragment(TEventClass::StaticClass());
	}

	bool IsValid() const;

	FVdjmRecordEventNodeFragment& ResetProperties();
	FVdjmRecordEventNodeFragment& SetEventTag(FName InEventTag);
	FVdjmRecordEventNodeFragment& SetBoolProperty(const FString& PropertyName, bool bInValue);
	FVdjmRecordEventNodeFragment& SetNumberProperty(const FString& PropertyName, double InValue);
	FVdjmRecordEventNodeFragment& SetStringProperty(const FString& PropertyName, const FString& InValue);
	FVdjmRecordEventNodeFragment& SetNameProperty(const FString& PropertyName, FName InValue);
	FVdjmRecordEventNodeFragment& SetSoftObjectPathProperty(const FString& PropertyName, const FSoftObjectPath& InValue);
	FVdjmRecordEventNodeFragment& SetSoftClassPathProperty(const FString& PropertyName, const FSoftClassPath& InValue);
	FVdjmRecordEventNodeFragment& SetPropertiesFromJsonString(const FString& InPropertiesJsonString, FString* OutError = nullptr);
	FVdjmRecordEventNodeFragment& SetJsonValueProperty(const FString& PropertyName, const TSharedPtr<FJsonValue>& InValue);
	FVdjmRecordEventNodeFragment& AddChild(const FVdjmRecordEventNodeFragment& ChildFragment);

	bool WriteEventJsonObject(TSharedPtr<FJsonObject>& OutEventJsonObject, FString& OutError) const;
	FString WriteEventJsonString(bool bPrettyPrint = true) const;

	FSoftClassPath EventClassPath;
	TArray<FVdjmRecordEventNodeFragment> Children;

private:
	TSharedPtr<FJsonObject> PropertiesObject;

	TSharedPtr<FJsonObject> GetOrCreatePropertiesObject();
};

struct VDJMRECORDER_API FVdjmRecordEventFlowFragment
{
public:
	FVdjmRecordEventFlowFragment& AppendEvent(const FVdjmRecordEventNodeFragment& EventFragment);
	FVdjmRecordEventFlowFragment& AppendFragment(const FVdjmRecordEventFlowFragment& OtherFragment);

	bool IsEmpty() const;
	void Reset();

	bool WriteJsonObject(TSharedPtr<FJsonObject>& OutRootJsonObject, FString& OutError) const;
	FString WriteJsonString(bool bPrettyPrint = true) const;
	bool BuildRuntime(UObject* Outer, UVdjmRecordEventFlowRuntime*& OutRuntime, FString& OutError) const;

	TArray<FVdjmRecordEventNodeFragment> Events;
};

namespace VdjmRecordEventFlowPresets
{
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSequenceNode();
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSpawnBridgeActorNode(
		bool bReuseExistingBridgeActor = true,
		const FSoftClassPath& BridgeActorClassPath = FSoftClassPath());
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSetEnvDataAssetPathNode(
		const FSoftObjectPath& EnvDataAssetPath,
		bool bRequireLoadSuccess = false);
	VDJMRECORDER_API FVdjmRecordEventFlowFragment MakeBootstrapFlowFragment(
		const FSoftObjectPath& EnvDataAssetPath,
		bool bReuseExistingBridgeActor = true,
		const FSoftClassPath& BridgeActorClassPath = FSoftClassPath(),
		bool bRequireLoadSuccess = false);
}
