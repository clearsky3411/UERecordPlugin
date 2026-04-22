#include "VdjmEvents/VdjmRecordEventFlowFragment.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmEvents/VdjmRecordEventFlowRuntime.h"
#include "VdjmEvents/VdjmRecordEventNode.h"

namespace
{
	constexpr int32 FlowSchemaVersion = 1;

	TSharedPtr<FJsonObject> CloneJsonObject(const TSharedPtr<FJsonObject>& InSourceObject)
	{
		if (!InSourceObject.IsValid())
		{
			return MakeShared<FJsonObject>();
		}

		FString JsonString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(InSourceObject.ToSharedRef(), Writer);

		TSharedPtr<FJsonObject> ClonedObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, ClonedObject) || !ClonedObject.IsValid())
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
	if (!FJsonSerializer::Deserialize(Reader, ParsedPropertiesObject) || !ParsedPropertiesObject.IsValid())
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

	if (!IsValid())
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
			if (!ChildFragment.WriteEventJsonObject(ChildJsonObject, OutError))
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
	if (!WriteEventJsonObject(EventJsonObject, OutError) || !EventJsonObject.IsValid())
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
	if (!PropertiesObject.IsValid())
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
		if (!EventFragment.WriteEventJsonObject(EventJsonObject, OutError))
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
	if (!WriteJsonObject(RootJsonObject, OutError) || !RootJsonObject.IsValid())
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

namespace VdjmRecordEventFlowPresets
{
	FVdjmRecordEventNodeFragment MakeSequenceNode()
	{
		return FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventSequenceNode>();
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

	FVdjmRecordEventNodeFragment MakeSetEnvDataAssetPathNode(const FSoftObjectPath& EnvDataAssetPath, bool bRequireLoadSuccess)
	{
		FVdjmRecordEventNodeFragment Fragment = FVdjmRecordEventNodeFragment::Make<UVdjmRecordEventSetEnvDataAssetPathNode>();
		Fragment.SetSoftObjectPathProperty(TEXT("EnvDataAssetPath"), EnvDataAssetPath);
		Fragment.SetBoolProperty(TEXT("bRequireLoadSuccess"), bRequireLoadSuccess);
		return Fragment;
	}

	FVdjmRecordEventFlowFragment MakeBootstrapFlowFragment(
		const FSoftObjectPath& EnvDataAssetPath,
		bool bReuseExistingBridgeActor,
		const FSoftClassPath& BridgeActorClassPath,
		bool bRequireLoadSuccess)
	{
		FVdjmRecordEventFlowFragment Fragment;
		Fragment.AppendEvent(MakeSetEnvDataAssetPathNode(EnvDataAssetPath, bRequireLoadSuccess));
		Fragment.AppendEvent(MakeSpawnBridgeActorNode(bReuseExistingBridgeActor, BridgeActorClassPath));
		return Fragment;
	}
}
