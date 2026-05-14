#include "VdjmEvents/VdjmRecordEventFlowFragment.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"
#include "VdjmEvents/VdjmRecordEventFlowRuntime.h"
#include "VdjmEvents/VdjmRecordEventNode.h"

namespace
{
	constexpr int32 FlowSchemaVersion = 1;

	TSharedPtr<FJsonObject> CloneJsonObject(const TSharedPtr<FJsonObject>& InSourceObject)
	{
		if (not InSourceObject.IsValid())
		{
			return MakeShared<FJsonObject>();
		}

		FString JsonString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(InSourceObject.ToSharedRef(), Writer);

		TSharedPtr<FJsonObject> ClonedObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (not FJsonSerializer::Deserialize(Reader, ClonedObject) || not ClonedObject.IsValid())
		{
			return MakeShared<FJsonObject>();
		}

		return ClonedObject;
	}
}

FVdjmRecordEventNodeFragment::FVdjmRecordEventNodeFragment()
{
}

FVdjmRecordEventNodeFragment::FVdjmRecordEventNodeFragment(const UClass* InEventClass)
	: EventClassPath(InEventClass)
{
}

FVdjmRecordEventNodeFragment::FVdjmRecordEventNodeFragment(const FSoftClassPath& InEventClassPath)
	: EventClassPath(InEventClassPath)
{
}

FVdjmRecordEventNodeFragment FVdjmRecordEventNodeFragment::Make(const UClass* InEventClass)
{
	return FVdjmRecordEventNodeFragment(InEventClass);
}

bool FVdjmRecordEventNodeFragment::IsValid() const
{
	return EventClassPath.IsValid();
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::ResetProperties()
{
	PropertiesObject = MakeShared<FJsonObject>();
	Children.Reset();
	return *this;
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::SetEventTag(FName InEventTag)
{
	return SetNameProperty(TEXT("EventTag"), InEventTag);
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::SetBoolProperty(const FString& PropertyName, bool bInValue)
{
	GetOrCreatePropertiesObject()->SetBoolField(PropertyName, bInValue);
	return *this;
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::SetNumberProperty(const FString& PropertyName, double InValue)
{
	GetOrCreatePropertiesObject()->SetNumberField(PropertyName, InValue);
	return *this;
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::SetStringProperty(const FString& PropertyName, const FString& InValue)
{
	GetOrCreatePropertiesObject()->SetStringField(PropertyName, InValue);
	return *this;
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::SetNameProperty(const FString& PropertyName, FName InValue)
{
	GetOrCreatePropertiesObject()->SetStringField(PropertyName, InValue.ToString());
	return *this;
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::SetSoftObjectPathProperty(const FString& PropertyName, const FSoftObjectPath& InValue)
{
	return SetStringProperty(PropertyName, InValue.ToString());
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::SetSoftClassPathProperty(const FString& PropertyName, const FSoftClassPath& InValue)
{
	return SetStringProperty(PropertyName, InValue.ToString());
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::SetPropertiesFromJsonString(const FString& InPropertiesJsonString, FString* OutError)
{
	FString LocalError;

	TSharedPtr<FJsonObject> ParsedPropertiesObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InPropertiesJsonString);
	if (not FJsonSerializer::Deserialize(Reader, ParsedPropertiesObject) || not ParsedPropertiesObject.IsValid())
	{
		LocalError = TEXT("Failed to parse fragment properties json.");
		if (OutError != nullptr)
		{
			*OutError = LocalError;
		}
		return *this;
	}

	PropertiesObject = ParsedPropertiesObject;
	if (OutError != nullptr)
	{
		OutError->Reset();
	}
	return *this;
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::SetJsonValueProperty(const FString& PropertyName, const TSharedPtr<FJsonValue>& InValue)
{
	if (InValue.IsValid())
	{
		GetOrCreatePropertiesObject()->SetField(PropertyName, InValue);
	}
	return *this;
}

FVdjmRecordEventNodeFragment& FVdjmRecordEventNodeFragment::AddChild(const FVdjmRecordEventNodeFragment& ChildFragment)
{
	Children.Add(ChildFragment);
	return *this;
}

bool FVdjmRecordEventNodeFragment::WriteEventJsonObject(TSharedPtr<FJsonObject>& OutEventJsonObject, FString& OutError) const
{
	OutError.Reset();
	OutEventJsonObject = nullptr;

	if (not IsValid())
	{
		OutError = TEXT("Event fragment class path is invalid.");
		return false;
	}

	const TSharedPtr<FJsonObject> PropertiesJsonObject = CloneJsonObject(PropertiesObject);
	if (Children.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ChildJsonValues;
		const TArray<TSharedPtr<FJsonValue>>* ExistingChildValues = nullptr;
		if (PropertiesJsonObject->TryGetArrayField(TEXT("Children"), ExistingChildValues) && ExistingChildValues != nullptr)
		{
			ChildJsonValues = *ExistingChildValues;
		}

		for (const FVdjmRecordEventNodeFragment& ChildFragment : Children)
		{
			TSharedPtr<FJsonObject> ChildJsonObject;
			if (not ChildFragment.WriteEventJsonObject(ChildJsonObject, OutError))
			{
				return false;
			}

			ChildJsonValues.Add(MakeShared<FJsonValueObject>(ChildJsonObject));
		}

		PropertiesJsonObject->SetArrayField(TEXT("Children"), ChildJsonValues);
	}

	OutEventJsonObject = MakeShared<FJsonObject>();
	OutEventJsonObject->SetStringField(TEXT("class"), EventClassPath.ToString());
	OutEventJsonObject->SetObjectField(TEXT("properties"), PropertiesJsonObject);
	return true;
}

FString FVdjmRecordEventNodeFragment::WriteEventJsonString(bool bPrettyPrint) const
{
	TSharedPtr<FJsonObject> EventJsonObject;
	FString OutError;
	if (not WriteEventJsonObject(EventJsonObject, OutError) || not EventJsonObject.IsValid())
	{
		return FString();
	}

	FString OutJsonString;
	if (bPrettyPrint)
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
		FJsonSerializer::Serialize(EventJsonObject.ToSharedRef(), Writer);
	}
	else
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJsonString);
		FJsonSerializer::Serialize(EventJsonObject.ToSharedRef(), Writer);
	}

	return OutJsonString;
}

TSharedPtr<FJsonObject> FVdjmRecordEventNodeFragment::GetOrCreatePropertiesObject()
{
	if (not PropertiesObject.IsValid())
	{
		PropertiesObject = MakeShared<FJsonObject>();
	}

	return PropertiesObject;
}

FVdjmRecordEventFlowFragment& FVdjmRecordEventFlowFragment::AppendEvent(const FVdjmRecordEventNodeFragment& EventFragment)
{
	Events.Add(EventFragment);
	return *this;
}

FVdjmRecordEventFlowFragment& FVdjmRecordEventFlowFragment::AppendFragment(const FVdjmRecordEventFlowFragment& OtherFragment)
{
	Events.Append(OtherFragment.Events);
	return *this;
}

bool FVdjmRecordEventFlowFragment::IsEmpty() const
{
	return Events.IsEmpty();
}

void FVdjmRecordEventFlowFragment::Reset()
{
	Events.Reset();
}

bool FVdjmRecordEventFlowFragment::WriteJsonObject(TSharedPtr<FJsonObject>& OutRootJsonObject, FString& OutError) const
{
	OutError.Reset();
	OutRootJsonObject = MakeShared<FJsonObject>();
	OutRootJsonObject->SetNumberField(TEXT("schema_version"), FlowSchemaVersion);

	TArray<TSharedPtr<FJsonValue>> EventJsonValues;
	EventJsonValues.Reserve(Events.Num());

	for (int32 EventIndex = 0; EventIndex < Events.Num(); ++EventIndex)
	{
		const FVdjmRecordEventNodeFragment& EventFragment = Events[EventIndex];
		TSharedPtr<FJsonObject> EventJsonObject;
		if (not EventFragment.WriteEventJsonObject(EventJsonObject, OutError))
		{
			OutError = FString::Printf(TEXT("Fragment event index %d serialize failed: %s"), EventIndex, *OutError);
			return false;
		}

		EventJsonValues.Add(MakeShared<FJsonValueObject>(EventJsonObject));
	}

	OutRootJsonObject->SetArrayField(TEXT("events"), EventJsonValues);
	return true;
}

FString FVdjmRecordEventFlowFragment::WriteJsonString(bool bPrettyPrint) const
{
	TSharedPtr<FJsonObject> RootJsonObject;
	FString OutError;
	if (not WriteJsonObject(RootJsonObject, OutError) || not RootJsonObject.IsValid())
	{
		return FString();
	}

	FString OutJsonString;
	if (bPrettyPrint)
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
		FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), Writer);
	}
	else
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJsonString);
		FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), Writer);
	}

	return OutJsonString;
}

bool FVdjmRecordEventFlowFragment::BuildRuntime(UObject* Outer, UVdjmRecordEventFlowRuntime*& OutRuntime, FString& OutError) const
{
	OutError.Reset();
	OutRuntime = nullptr;

	const FString FragmentJsonString = WriteJsonString(false);
	if (FragmentJsonString.IsEmpty())
	{
		OutError = TEXT("Flow fragment json string is empty.");
		return false;
	}

	OutRuntime = UVdjmRecordEventFlowRuntime::CreateFlowRuntimeFromJsonString(Outer, FragmentJsonString, OutError);
	return OutRuntime != nullptr;
}

bool FVdjmRecordEventFlowFragment::BuildFlowDataAsset(UObject* Outer, UVdjmRecordEventFlowDataAsset*& OutDataAsset, FString& OutError) const
{
	OutError.Reset();
	OutDataAsset = UVdjmRecordEventFlowDataAsset::CreateTransientFlowDataAssetFromFragment(Outer, *this, OutError);
	return OutDataAsset != nullptr;
}

namespace VdjmRecordEventFlowPresets
{
	FVdjmRecordEventNodeFragment MakeSequenceNode()
	{
		return FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventSequenceNode>();
	}

	FVdjmRecordEventNodeFragment MakeSequenceNode(const FVdjmRecordEventFlowFragment& ChildFlowFragment)
	{
		FVdjmRecordEventNodeFragment Fragment = MakeSequenceNode();
		for (const FVdjmRecordEventNodeFragment& ChildEvent : ChildFlowFragment.Events)
		{
			Fragment.AddChild(ChildEvent);
		}
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeJumpToNextNode(
		const FSoftClassPath& TargetClassPath,
		FName TargetTag,
		bool bAbortIfNotFound)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventJumpToNextNode>();
		if (TargetClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("TargetClass"), TargetClassPath);
		}
		if (not TargetTag.IsNone())
		{
			Fragment.SetNameProperty(TEXT("TargetTag"), TargetTag);
		}
		Fragment.SetBoolProperty(TEXT("bAbortIfNotFound"), bAbortIfNotFound);
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeLogNode(const FString& Message, bool bLogAsWarning)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventLogNode>();
		Fragment.SetStringProperty(TEXT("Message"), Message);
		Fragment.SetBoolProperty(TEXT("bLogAsWarning"), bLogAsWarning);
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeSpawnBridgeActorNode(bool bReuseExistingBridgeActor, const FSoftClassPath& BridgeActorClassPath)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventSpawnBridgeActorNode>();
		Fragment.SetBoolProperty(TEXT("bReuseExistingBridgeActor"), bReuseExistingBridgeActor);
		if (BridgeActorClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("BridgeActorClass"), BridgeActorClassPath);
		}
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeSpawnRecordBridgeActorWaitNode(
		bool bReuseExistingBridgeActor,
		const FSoftClassPath& BridgeActorClassPath,
		const FSoftObjectPath& EnvDataAssetPath,
		bool bRequireLoadSuccess,
		EVdjmRecordEventBridgeStartPolicy StartPolicy,
		FName StartSignalTag)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventSpawnRecordBridgeActorWait>();
		Fragment.SetBoolProperty(TEXT("bReuseExistingBridgeActor"), bReuseExistingBridgeActor);
		if (BridgeActorClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("BridgeActorClass"), BridgeActorClassPath);
		}
		if (not EnvDataAssetPath.IsNull())
		{
			Fragment.SetSoftObjectPathProperty(TEXT("EnvDataAssetPath"), EnvDataAssetPath);
		}
		Fragment.SetBoolProperty(TEXT("bRequireLoadSuccess"), bRequireLoadSuccess);
		Fragment.SetNumberProperty(TEXT("StartPolicy"), static_cast<int32>(StartPolicy));
		if (not StartSignalTag.IsNone())
		{
			Fragment.SetNameProperty(TEXT("StartSignalTag"), StartSignalTag);
		}
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeStartRecordBridgeActorNode()
	{
		return FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventStartRecordBridgeActorNode>();
	}

	FVdjmRecordEventNodeFragment MakeCreateObjectNode(
		const FSoftClassPath& ObjectClassPath,
		FName RuntimeSlotKey,
		bool bReuseSlotObject,
		EVdjmRecordEventObjectOuterPolicy OuterPolicy)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventCreateObjectNode>();
		if (ObjectClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("ObjectClass"), ObjectClassPath);
		}
		if (not RuntimeSlotKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("RuntimeSlotKey"), RuntimeSlotKey);
		}
		Fragment.SetBoolProperty(TEXT("bReuseSlotObject"), bReuseSlotObject);
		Fragment.SetNumberProperty(TEXT("OuterPolicy"), static_cast<int32>(OuterPolicy));
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeSpawnActorNode(
		const FSoftClassPath& ActorClassPath,
		FName RuntimeSlotKey,
		bool bReuseSlotActor)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventSpawnActorNode>();
		if (ActorClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("ActorClass"), ActorClassPath);
		}
		if (not RuntimeSlotKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("RuntimeSlotKey"), RuntimeSlotKey);
		}
		Fragment.SetBoolProperty(TEXT("bReuseSlotActor"), bReuseSlotActor);
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeRegisterContextEntryNode(
		FName RuntimeSlotKey,
		FName ContextKey,
		const FSoftClassPath& ExpectedClassPath)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventRegisterContextEntryNode>();
		if (not RuntimeSlotKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("RuntimeSlotKey"), RuntimeSlotKey);
		}
		if (not ContextKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("ContextKey"), ContextKey);
		}
		if (ExpectedClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("ExpectedClass"), ExpectedClassPath);
		}
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeRegisterWidgetContextNode(FName RuntimeSlotKey, FName ContextKey)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventRegisterWidgetContextNode>();
		if (not RuntimeSlotKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("RuntimeSlotKey"), RuntimeSlotKey);
		}
		if (not ContextKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("ContextKey"), ContextKey);
		}
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeCreateWidgetNode(
		const FSoftClassPath& WidgetClassPath,
		int32 PlayerIndex,
		bool bRequireOwningPlayer,
		bool bReuseCreatedWidget,
		bool bAddToViewport,
		int32 ZOrder,
		FName RuntimeSlotKey,
		FName EmitSignalTag)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventCreateWidgetNode>();
		if (WidgetClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("WidgetClass"), WidgetClassPath);
		}
		Fragment.SetNumberProperty(TEXT("PlayerIndex"), PlayerIndex);
		Fragment.SetBoolProperty(TEXT("bRequireOwningPlayer"), bRequireOwningPlayer);
		Fragment.SetBoolProperty(TEXT("bReuseCreatedWidget"), bReuseCreatedWidget);
		Fragment.SetBoolProperty(TEXT("bAddToViewport"), bAddToViewport);
		Fragment.SetNumberProperty(TEXT("ZOrder"), ZOrder);
		if (not RuntimeSlotKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("RuntimeSlotKey"), RuntimeSlotKey);
		}
		if (not EmitSignalTag.IsNone())
		{
			Fragment.SetNameProperty(TEXT("EmitSignalTag"), EmitSignalTag);
		}
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeRemoveWidgetNode(
		FName runtimeSlotKey,
		FName contextKey,
		bool bClearRuntimeSlot,
		bool bUnregisterContext,
		bool bSucceedIfMissing)
	{
		FVdjmRecordEventNodeFragment fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventRemoveWidgetNode>();
		if (not runtimeSlotKey.IsNone())
		{
			fragment.SetNameProperty(TEXT("RuntimeSlotKey"), runtimeSlotKey);
		}
		if (not contextKey.IsNone())
		{
			fragment.SetNameProperty(TEXT("ContextKey"), contextKey);
		}
		fragment.SetBoolProperty(TEXT("bClearRuntimeSlot"), bClearRuntimeSlot);
		fragment.SetBoolProperty(TEXT("bUnregisterContext"), bUnregisterContext);
		fragment.SetBoolProperty(TEXT("bSucceedIfMissing"), bSucceedIfMissing);
		return fragment;
	}

	FVdjmRecordEventNodeFragment MakeWaitForSignalNode(FName SignalTag)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventWaitForSignalNode>();
		if (not SignalTag.IsNone())
		{
			Fragment.SetNameProperty(TEXT("SignalTag"), SignalTag);
		}
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeDelayNode(float DelaySeconds)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventDelayNode>();
		Fragment.SetNumberProperty(TEXT("DelaySeconds"), DelaySeconds);
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeEmitSignalNode(FName SignalTag)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventEmitSignalNode>();
		if (not SignalTag.IsNone())
		{
			Fragment.SetNameProperty(TEXT("SignalTag"), SignalTag);
		}
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeCreateObjectAndRegisterContextNode(
		const FSoftClassPath& ObjectClassPath,
		FName RuntimeSlotKey,
		FName ContextKey,
		const FSoftClassPath& ExpectedClassPath,
		bool bReuseSlotObject,
		EVdjmRecordEventObjectOuterPolicy OuterPolicy)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventCreateObjectAndRegisterContextNode>();
		if (ObjectClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("ObjectClass"), ObjectClassPath);
		}
		if (not RuntimeSlotKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("RuntimeSlotKey"), RuntimeSlotKey);
		}
		if (not ContextKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("ContextKey"), ContextKey);
		}
		if (ExpectedClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("ExpectedClass"), ExpectedClassPath);
		}
		Fragment.SetBoolProperty(TEXT("bReuseSlotObject"), bReuseSlotObject);
		Fragment.SetNumberProperty(TEXT("OuterPolicy"), static_cast<int32>(OuterPolicy));
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeSpawnActorAndRegisterContextNode(
		const FSoftClassPath& ActorClassPath,
		FName RuntimeSlotKey,
		FName ContextKey,
		const FSoftClassPath& ExpectedClassPath,
		bool bReuseSlotActor)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventSpawnActorAndRegisterContextNode>();
		if (ActorClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("ActorClass"), ActorClassPath);
		}
		if (not RuntimeSlotKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("RuntimeSlotKey"), RuntimeSlotKey);
		}
		if (not ContextKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("ContextKey"), ContextKey);
		}
		if (ExpectedClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("ExpectedClass"), ExpectedClassPath);
		}
		Fragment.SetBoolProperty(TEXT("bReuseSlotActor"), bReuseSlotActor);
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeCreateWidgetAndRegisterContextNode(
		const FSoftClassPath& WidgetClassPath,
		FName RuntimeSlotKey,
		FName ContextKey,
		int32 PlayerIndex,
		bool bRequireOwningPlayer,
		bool bReuseCreatedWidget,
		bool bAddToViewport,
		int32 ZOrder,
		FName EmitSignalTag)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventCreateWidgetAndRegisterContextNode>();
		if (WidgetClassPath.IsValid())
		{
			Fragment.SetSoftClassPathProperty(TEXT("WidgetClass"), WidgetClassPath);
		}
		Fragment.SetNumberProperty(TEXT("PlayerIndex"), PlayerIndex);
		Fragment.SetBoolProperty(TEXT("bRequireOwningPlayer"), bRequireOwningPlayer);
		Fragment.SetBoolProperty(TEXT("bReuseCreatedWidget"), bReuseCreatedWidget);
		Fragment.SetBoolProperty(TEXT("bAddToViewport"), bAddToViewport);
		Fragment.SetNumberProperty(TEXT("ZOrder"), ZOrder);
		if (not RuntimeSlotKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("RuntimeSlotKey"), RuntimeSlotKey);
		}
		if (not ContextKey.IsNone())
		{
			Fragment.SetNameProperty(TEXT("ContextKey"), ContextKey);
		}
		if (not EmitSignalTag.IsNone())
		{
			Fragment.SetNameProperty(TEXT("EmitSignalTag"), EmitSignalTag);
		}
		return Fragment;
	}

	FVdjmRecordEventNodeFragment MakeSetEnvDataAssetPathNode(const FSoftObjectPath& EnvDataAssetPath, bool bRequireLoadSuccess)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventSetEnvDataAssetPathNode>();
		Fragment.SetSoftObjectPathProperty(TEXT("EnvDataAssetPath"), EnvDataAssetPath);
		Fragment.SetBoolProperty(TEXT("bRequireLoadSuccess"), bRequireLoadSuccess);
		return Fragment;
	}

	FVdjmRecordEventFlowFragment MakeSetEnvOnlyFlowFragment(const FSoftObjectPath& EnvDataAssetPath, bool bRequireLoadSuccess)
	{
		FVdjmRecordEventFlowFragment Fragment;
		Fragment.AppendEvent(MakeSetEnvDataAssetPathNode(EnvDataAssetPath, bRequireLoadSuccess));
		return Fragment;
	}

	FVdjmRecordEventFlowFragment MakeBindBridgeFlowFragment(bool bReuseExistingBridgeActor, const FSoftClassPath& BridgeActorClassPath)
	{
		FVdjmRecordEventFlowFragment Fragment;
		Fragment.AppendEvent(MakeSpawnBridgeActorNode(bReuseExistingBridgeActor, BridgeActorClassPath));
		return Fragment;
	}

	FVdjmRecordEventFlowFragment MakeBootstrapReuseBridgeFlowFragment(
		const FSoftObjectPath& EnvDataAssetPath,
		bool bRequireLoadSuccess)
	{
		return MakeBootstrapFlowFragment(
			EnvDataAssetPath,
			true,
			FSoftClassPath(),
			bRequireLoadSuccess);
	}

	FVdjmRecordEventFlowFragment MakeBootstrapSpawnBridgeFlowFragment(
		const FSoftObjectPath& EnvDataAssetPath,
		const FSoftClassPath& BridgeActorClassPath,
		bool bRequireLoadSuccess)
	{
		return MakeBootstrapFlowFragment(
			EnvDataAssetPath,
			false,
			BridgeActorClassPath,
			bRequireLoadSuccess);
	}

	FVdjmRecordEventFlowFragment MakeJumpToNextByTagFlowFragment(FName TargetTag, bool bAbortIfNotFound)
	{
		FVdjmRecordEventFlowFragment Fragment;
		Fragment.AppendEvent(MakeJumpToNextNode(FSoftClassPath(), TargetTag, bAbortIfNotFound));
		return Fragment;
	}

	FVdjmRecordEventFlowFragment MakeLogOnlyFlowFragment(const FString& Message, bool bLogAsWarning, FName EventTag)
	{
		FVdjmRecordEventFlowFragment Fragment;
		FVdjmRecordEventNodeFragment LogNode = MakeLogNode(Message, bLogAsWarning);
		if (not EventTag.IsNone())
		{
			LogNode.SetEventTag(EventTag);
		}
		Fragment.AppendEvent(LogNode);
		return Fragment;
	}

	FVdjmRecordEventFlowFragment MakeBootstrapFlowFragment(
		const FSoftObjectPath& EnvDataAssetPath,
		bool bReuseExistingBridgeActor,
		const FSoftClassPath& BridgeActorClassPath,
		bool bRequireLoadSuccess)
	{
		FVdjmRecordEventFlowFragment Fragment;
		Fragment.AppendEvent(MakeSpawnRecordBridgeActorWaitNode(
			bReuseExistingBridgeActor,
			BridgeActorClassPath,
			EnvDataAssetPath,
			bRequireLoadSuccess,
			EVdjmRecordEventBridgeStartPolicy::EStartImmediately,
			NAME_None));
		return Fragment;
	}
}
