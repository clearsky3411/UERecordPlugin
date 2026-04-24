#pragma once

#include "CoreMinimal.h"
#include "VdjmRecordTypes.h"
#include "Dom/JsonObject.h"

class UVdjmRecordEventFlowRuntime;
class UVdjmRecordEventFlowDataAsset;
enum class EVdjmRecordEventBridgeStartPolicy : uint8;

/**
 * Overview:
 * `FVdjmRecordEventNodeFragment` is a lightweight, code-first description of a single
 * event node. It does not own a `UVdjmRecordEventBase` instance. Instead it stores:
 * - event class path
 * - json-serializable property overrides
 * - optional child fragments for composite nodes such as sequence
 *
 * Usage:
 * Build fragments in code, serialize them with the same flow json schema used by
 * `UVdjmRecordEventFlowDataAsset`, then merge them into runtime through one of:
 * - json string export
 * - transient flow data asset creation
 * - runtime creation
 *
 * Dependency notes:
 * - This layer depends on json serialization, not on live event UObject instances.
 * - Child fragments are written into the `Children` property in json when present.
 * - Runtime/data asset construction is intentionally deferred to keep this struct
 *   cheap to compose and easy to reuse as presets.
 */
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

/**
 * Overview:
 * `FVdjmRecordEventFlowFragment` is a chain-builder style container for event node
 * fragments. Its role is to let code assemble a flow without directly constructing
 * `UVdjmRecordEventBase` objects.
 *
 * Primary intent:
 * - compose reusable preset flows in C++
 * - export the same json schema used by flow assets
 * - converge `DataAsset / Json / Fragment` authoring paths into the same runtime path
 *
 * Typical usage:
 * 1. append one or more `FVdjmRecordEventNodeFragment`
 * 2. optionally append another flow fragment
 * 3. export to json for inspection or persistence
 * 4. build a transient `UVdjmRecordEventFlowDataAsset` or `UVdjmRecordEventFlowRuntime`
 *
 * Current limitation:
 * This struct is a composition layer only. It does not execute events, own runtime
 * state, or provide flow-level global settings by itself.
 */
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
	bool BuildFlowDataAsset(UObject* Outer, UVdjmRecordEventFlowDataAsset*& OutDataAsset, FString& OutError) const;

	TArray<FVdjmRecordEventNodeFragment> Events;
};

/**
 * Built-in helpers for assembling common event fragments and small flow presets.
 *
 * Design note:
 * These helpers intentionally stay thin. They produce fragment data only and do not
 * bypass the shared json/runtime conversion path. That keeps code presets aligned with
 * asset-authored and json-authored flows.
 */
namespace VdjmRecordEventFlowPresets
{
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSequenceNode();
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSequenceNode(const FVdjmRecordEventFlowFragment& ChildFlowFragment);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeJumpToNextNode(
		const FSoftClassPath& TargetClassPath = FSoftClassPath(),
		FName TargetTag = NAME_None,
		bool bAbortIfNotFound = false);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeLogNode(
		const FString& Message,
		bool bLogAsWarning = false);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSpawnBridgeActorNode(
		bool bReuseExistingBridgeActor = true,
		const FSoftClassPath& BridgeActorClassPath = FSoftClassPath());
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSpawnRecordBridgeActorWaitNode(
		bool bReuseExistingBridgeActor = true,
		const FSoftClassPath& BridgeActorClassPath = FSoftClassPath(),
		const FSoftObjectPath& EnvDataAssetPath = FSoftObjectPath(),
		bool bRequireLoadSuccess = false,
		EVdjmRecordEventBridgeStartPolicy StartPolicy = EVdjmRecordEventBridgeStartPolicy::EStartImmediately,
		FName StartSignalTag = NAME_None);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeCreateObjectNode(
		const FSoftClassPath& ObjectClassPath,
		FName RuntimeSlotKey,
		bool bReuseSlotObject = true,
		EVdjmRecordEventObjectOuterPolicy OuterPolicy = EVdjmRecordEventObjectOuterPolicy::EBridgeActor);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSpawnActorNode(
		const FSoftClassPath& ActorClassPath,
		FName RuntimeSlotKey,
		bool bReuseSlotActor = true);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeRegisterContextEntryNode(
		FName RuntimeSlotKey,
		FName ContextKey,
		const FSoftClassPath& ExpectedClassPath = FSoftClassPath());
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeRegisterWidgetContextNode(
		FName RuntimeSlotKey,
		FName ContextKey);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeCreateWidgetNode(
		const FSoftClassPath& WidgetClassPath,
		int32 PlayerIndex = 0,
		bool bRequireOwningPlayer = false,
		bool bReuseCreatedWidget = true,
		bool bAddToViewport = true,
		int32 ZOrder = 0,
		FName RuntimeSlotKey = NAME_None,
		FName EmitSignalTag = NAME_None);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeRemoveWidgetNode(
		FName runtimeSlotKey,
		FName contextKey = NAME_None,
		bool bClearRuntimeSlot = true,
		bool bUnregisterContext = false,
		bool bSucceedIfMissing = false);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeWaitForSignalNode(FName SignalTag);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeDelayNode(float DelaySeconds);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeEmitSignalNode(FName SignalTag);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeCreateObjectAndRegisterContextNode(
		const FSoftClassPath& ObjectClassPath,
		FName RuntimeSlotKey,
		FName ContextKey,
		const FSoftClassPath& ExpectedClassPath = FSoftClassPath(),
		bool bReuseSlotObject = true,
		EVdjmRecordEventObjectOuterPolicy OuterPolicy = EVdjmRecordEventObjectOuterPolicy::EBridgeActor);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSpawnActorAndRegisterContextNode(
		const FSoftClassPath& ActorClassPath,
		FName RuntimeSlotKey,
		FName ContextKey,
		const FSoftClassPath& ExpectedClassPath = FSoftClassPath(),
		bool bReuseSlotActor = true);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeCreateWidgetAndRegisterContextNode(
		const FSoftClassPath& WidgetClassPath,
		FName RuntimeSlotKey,
		FName ContextKey,
		int32 PlayerIndex = 0,
		bool bRequireOwningPlayer = false,
		bool bReuseCreatedWidget = true,
		bool bAddToViewport = true,
		int32 ZOrder = 0,
		FName EmitSignalTag = NAME_None);
	VDJMRECORDER_API FVdjmRecordEventNodeFragment MakeSetEnvDataAssetPathNode(
		const FSoftObjectPath& EnvDataAssetPath,
		bool bRequireLoadSuccess = false);
	VDJMRECORDER_API FVdjmRecordEventFlowFragment MakeSetEnvOnlyFlowFragment(
		const FSoftObjectPath& EnvDataAssetPath,
		bool bRequireLoadSuccess = false);
	VDJMRECORDER_API FVdjmRecordEventFlowFragment MakeBindBridgeFlowFragment(
		bool bReuseExistingBridgeActor = true,
		const FSoftClassPath& BridgeActorClassPath = FSoftClassPath());
	VDJMRECORDER_API FVdjmRecordEventFlowFragment MakeBootstrapReuseBridgeFlowFragment(
		const FSoftObjectPath& EnvDataAssetPath,
		bool bRequireLoadSuccess = false);
	VDJMRECORDER_API FVdjmRecordEventFlowFragment MakeBootstrapSpawnBridgeFlowFragment(
		const FSoftObjectPath& EnvDataAssetPath,
		const FSoftClassPath& BridgeActorClassPath = FSoftClassPath(),
		bool bRequireLoadSuccess = false);
	VDJMRECORDER_API FVdjmRecordEventFlowFragment MakeJumpToNextByTagFlowFragment(
		FName TargetTag,
		bool bAbortIfNotFound = false);
	VDJMRECORDER_API FVdjmRecordEventFlowFragment MakeLogOnlyFlowFragment(
		const FString& Message,
		bool bLogAsWarning = false,
		FName EventTag = NAME_None);
	VDJMRECORDER_API FVdjmRecordEventFlowFragment MakeBootstrapFlowFragment(
		const FSoftObjectPath& EnvDataAssetPath,
		bool bReuseExistingBridgeActor = true,
		const FSoftClassPath& BridgeActorClassPath = FSoftClassPath(),
		bool bRequireLoadSuccess = false);
}
