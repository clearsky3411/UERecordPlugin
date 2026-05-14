#include "VdjmAssetRegistryBlueprintLibrary.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmAssetRegistry.h"

namespace
{
	constexpr int32 VdjmAssetRegistrySchemaVersion = 1;
	constexpr int32 VdjmAssetRegistryMaxDefineDepth = 32;

	struct FVdjmAssetRegistryScanRoot
	{
		FString Key;
		FString FullPath;
		bool bDefinedRoot = false;
	};

	void AddMessage(
		TArray<FVdjmAssetRegistryMessage>& outMessages,
		const EVdjmAssetRegistryMessageSeverity severity,
		const FString& code,
		const FString& path,
		const FString& message)
	{
		FVdjmAssetRegistryMessage newMessage;
		newMessage.Severity = severity;
		newMessage.Code = code;
		newMessage.Path = path;
		newMessage.Message = message;
		outMessages.Add(newMessage);
	}

	bool HasError(const TArray<FVdjmAssetRegistryMessage>& messages)
	{
		for (const FVdjmAssetRegistryMessage& message : messages)
		{
			if (message.Severity == EVdjmAssetRegistryMessageSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}

	FString NormalizePathString(const FString& path)
	{
		FString normalizedPath = path;
		FPaths::NormalizeFilename(normalizedPath);
		while (normalizedPath.Contains(TEXT("//")))
		{
			normalizedPath = normalizedPath.Replace(TEXT("//"), TEXT("/"));
		}
		normalizedPath.RemoveFromStart(TEXT("./"));
		normalizedPath.RemoveFromEnd(TEXT("/"));
		return normalizedPath;
	}

	FString RemovePathExtension(const FString& path)
	{
		const FString extension = FPaths::GetExtension(path, true);
		return extension.IsEmpty() ? path : path.LeftChop(extension.Len());
	}

	bool IsParentPathBlocked(const FString& relativePath)
	{
		const FString normalizedPath = NormalizePathString(relativePath);
		TArray<FString> parts;
		normalizedPath.ParseIntoArray(parts, TEXT("/"), true);
		for (const FString& part : parts)
		{
			if (part == TEXT(".."))
			{
				return true;
			}
		}

		return false;
	}

	FString GetPluginBaseDir()
	{
		const TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin(VDJM_ASSET_REGISTRY_PLUGIN_NAME);
		return plugin.IsValid() ? plugin->GetBaseDir() : FPaths::ProjectPluginsDir() / VDJM_ASSET_REGISTRY_PLUGIN_NAME;
	}

	FString GetPluginContentDir()
	{
		const TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin(VDJM_ASSET_REGISTRY_PLUGIN_NAME);
		return plugin.IsValid() ? plugin->GetContentDir() : GetPluginBaseDir() / TEXT("Content");
	}

	FString GetPlatformPath(const FVdjmAssetRegistryPathRoot& root)
	{
#if PLATFORM_WINDOWS
		if (!root.WinPath.IsEmpty())
		{
			return root.WinPath;
		}
#elif PLATFORM_ANDROID
		if (!root.AndroidPath.IsEmpty())
		{
			return root.AndroidPath;
		}
#elif PLATFORM_IOS
		if (!root.IosPath.IsEmpty())
		{
			return root.IosPath;
		}
#endif
		return root.DefaultPath;
	}

	TArray<FString> GetSortedMapKeys(const TMap<FString, FString>& sourceMap)
	{
		TArray<FString> keys;
		sourceMap.GetKeys(keys);
		keys.Sort();
		return keys;
	}

	TSharedPtr<FJsonObject> ParseJsonObject(const FString& jsonText, TArray<FVdjmAssetRegistryMessage>& outMessages, const FString& sourcePath)
	{
		TSharedPtr<FJsonObject> rootObject;
		const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonText);
		if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("json_parse_failed"), sourcePath, TEXT("Failed to parse asset registry JSON."));
			return nullptr;
		}

		return rootObject;
	}

	FString JsonValueToLooseString(const TSharedPtr<FJsonValue>& value)
	{
		if (!value.IsValid())
		{
			return FString();
		}

		if (value->Type == EJson::String)
		{
			return value->AsString();
		}
		if (value->Type == EJson::Number)
		{
			return FString::SanitizeFloat(value->AsNumber());
		}
		if (value->Type == EJson::Boolean)
		{
			return value->AsBool() ? TEXT("true") : TEXT("false");
		}
		if (value->Type == EJson::Null)
		{
			return TEXT("null");
		}

		FString outJson;
		const TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&outJson);
		FJsonSerializer::Serialize(value, TEXT(""), writer);
		return outJson;
	}

	TMap<FString, FString> ReadStringMap(const TSharedPtr<FJsonObject>& jsonObject)
	{
		TMap<FString, FString> outMap;
		if (!jsonObject.IsValid())
		{
			return outMap;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& pair : jsonObject->Values)
		{
			outMap.Add(pair.Key, JsonValueToLooseString(pair.Value));
		}

		return outMap;
	}

	void WriteStringMap(const TSharedPtr<FJsonObject>& jsonObject, const FString& fieldName, const TMap<FString, FString>& sourceMap)
	{
		const TSharedPtr<FJsonObject> mapObject = MakeShared<FJsonObject>();
		for (const FString& key : GetSortedMapKeys(sourceMap))
		{
			mapObject->SetStringField(key, sourceMap[key]);
		}
		jsonObject->SetObjectField(fieldName, mapObject);
	}

	TArray<FString> ReadStringArray(const TSharedPtr<FJsonObject>& jsonObject, const FString& fieldName)
	{
		TArray<FString> outValues;
		if (!jsonObject.IsValid())
		{
			return outValues;
		}

		const TArray<TSharedPtr<FJsonValue>>* valueArray = nullptr;
		if (jsonObject->TryGetArrayField(fieldName, valueArray) && valueArray != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& value : *valueArray)
			{
				outValues.Add(JsonValueToLooseString(value));
			}
		}

		return outValues;
	}

	TArray<TSharedPtr<FJsonValue>> WriteStringArray(const TArray<FString>& sourceArray)
	{
		TArray<TSharedPtr<FJsonValue>> outValues;
		for (const FString& value : sourceArray)
		{
			outValues.Add(MakeShared<FJsonValueString>(value));
		}
		return outValues;
	}

	bool IsValidDefineKey(const FString& key)
	{
		if (key.IsEmpty())
		{
			return false;
		}

		for (const TCHAR character : key)
		{
			if (!(FChar::IsAlnum(character) || character == TEXT('_')))
			{
				return false;
			}
		}

		return true;
	}

	bool ReplaceDefineTokens(
		const FString& source,
		const TMap<FString, FString>& resolvedDefines,
		FString& outText,
		TArray<FVdjmAssetRegistryMessage>& outMessages,
		const FString& path)
	{
		outText.Reset();

		int32 searchIndex = 0;
		while (searchIndex < source.Len())
		{
			int32 tokenStart = INDEX_NONE;
			if (!source.FindChar(TEXT('!'), tokenStart) || tokenStart < searchIndex)
			{
				tokenStart = source.Find(TEXT("!{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, searchIndex);
			}
			else
			{
				tokenStart = source.Find(TEXT("!{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, searchIndex);
			}

			if (tokenStart == INDEX_NONE)
			{
				outText += source.Mid(searchIndex);
				break;
			}

			outText += source.Mid(searchIndex, tokenStart - searchIndex);

			const int32 keyStart = tokenStart + 2;
			const int32 tokenEnd = source.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, keyStart);
			if (tokenEnd == INDEX_NONE)
			{
				AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("define_unclosed"), path, TEXT("Unclosed define token."));
				return false;
			}

			const FString key = source.Mid(keyStart, tokenEnd - keyStart);
			const FString* value = resolvedDefines.Find(key);
			if (value == nullptr)
			{
				AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("define_missing"), path, FString::Printf(TEXT("Missing define '%s'."), *key));
				return false;
			}

			outText += *value;
			searchIndex = tokenEnd + 1;
		}

		if (outText.Contains(TEXT("!{")))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("define_unresolved"), path, TEXT("Unresolved define token remains after preprocessing."));
			return false;
		}

		return true;
	}

	bool ResolveDefineValue(
		const FString& key,
		const TMap<FString, FString>& rawDefines,
		TMap<FString, FString>& resolvedDefines,
		TArray<FString>& stack,
		TArray<FVdjmAssetRegistryMessage>& outMessages)
	{
		if (const FString* existingValue = resolvedDefines.Find(key))
		{
			return !existingValue->IsEmpty() || rawDefines.Contains(key);
		}

		const FString* rawValue = rawDefines.Find(key);
		if (rawValue == nullptr)
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("define_missing"), TEXT("defines"), FString::Printf(TEXT("Missing define '%s'."), *key));
			return false;
		}

		if (stack.Contains(key))
		{
			stack.Add(key);
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("define_cycle"), TEXT("defines"), FString::Printf(TEXT("Circular define reference: %s."), *FString::Join(stack, TEXT(" -> "))));
			return false;
		}

		if (stack.Num() >= VdjmAssetRegistryMaxDefineDepth)
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("define_depth"), TEXT("defines"), TEXT("Define expansion exceeded maximum depth."));
			return false;
		}

		stack.Add(key);

		FString expandedValue;
		int32 searchIndex = 0;
		while (searchIndex < rawValue->Len())
		{
			const int32 tokenStart = rawValue->Find(TEXT("!{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, searchIndex);
			if (tokenStart == INDEX_NONE)
			{
				expandedValue += rawValue->Mid(searchIndex);
				break;
			}

			expandedValue += rawValue->Mid(searchIndex, tokenStart - searchIndex);
			const int32 keyStart = tokenStart + 2;
			const int32 tokenEnd = rawValue->Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, keyStart);
			if (tokenEnd == INDEX_NONE)
			{
				AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("define_unclosed"), TEXT("defines"), FString::Printf(TEXT("Unclosed define token in '%s'."), *key));
				stack.Pop();
				return false;
			}

			const FString childKey = rawValue->Mid(keyStart, tokenEnd - keyStart);
			if (!ResolveDefineValue(childKey, rawDefines, resolvedDefines, stack, outMessages))
			{
				stack.Pop();
				return false;
			}

			expandedValue += resolvedDefines[childKey];
			searchIndex = tokenEnd + 1;
		}

		stack.Pop();
		resolvedDefines.Add(key, expandedValue);
		return true;
	}

	bool ResolveDefines(
		const TMap<FString, FString>& rawDefines,
		TMap<FString, FString>& outResolvedDefines,
		TArray<FVdjmAssetRegistryMessage>& outMessages)
	{
		outResolvedDefines.Reset();
		for (const TPair<FString, FString>& pair : rawDefines)
		{
			if (!IsValidDefineKey(pair.Key))
			{
				AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("define_key_invalid"), TEXT("defines"), FString::Printf(TEXT("Invalid define key '%s'. Use letters, digits, and underscores only."), *pair.Key));
			}
		}
		if (HasError(outMessages))
		{
			return false;
		}

		for (const TPair<FString, FString>& pair : rawDefines)
		{
			TArray<FString> stack;
			if (!ResolveDefineValue(pair.Key, rawDefines, outResolvedDefines, stack, outMessages))
			{
				return false;
			}
		}

		return true;
	}
}

namespace
{
	FVdjmAssetRegistryPathRoot ReadPathRoot(const FString& key, const TSharedPtr<FJsonObject>& jsonObject)
	{
		FVdjmAssetRegistryPathRoot root;
		root.Key = key;
		if (!jsonObject.IsValid())
		{
			return root;
		}

		jsonObject->TryGetStringField(TEXT("default_path"), root.DefaultPath);
		jsonObject->TryGetStringField(TEXT("win_path"), root.WinPath);
		jsonObject->TryGetStringField(TEXT("android_path"), root.AndroidPath);
		jsonObject->TryGetStringField(TEXT("ios_path"), root.IosPath);
		jsonObject->TryGetBoolField(TEXT("scan"), root.bScan);

		const TSharedPtr<FJsonObject>* metaObject = nullptr;
		if (jsonObject->TryGetObjectField(TEXT("meta"), metaObject) && metaObject != nullptr)
		{
			root.Meta = ReadStringMap(*metaObject);
		}

		return root;
	}

	TSharedPtr<FJsonObject> WritePathRoot(const FVdjmAssetRegistryPathRoot& root)
	{
		const TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("default_path"), root.DefaultPath);
		if (!root.WinPath.IsEmpty())
		{
			jsonObject->SetStringField(TEXT("win_path"), root.WinPath);
		}
		if (!root.AndroidPath.IsEmpty())
		{
			jsonObject->SetStringField(TEXT("android_path"), root.AndroidPath);
		}
		if (!root.IosPath.IsEmpty())
		{
			jsonObject->SetStringField(TEXT("ios_path"), root.IosPath);
		}
		jsonObject->SetBoolField(TEXT("scan"), root.bScan);
		WriteStringMap(jsonObject, TEXT("meta"), root.Meta);
		return jsonObject;
	}

	FVdjmAssetRegistryDefinedRoot ReadDefinedRoot(const TSharedPtr<FJsonObject>& jsonObject)
	{
		FVdjmAssetRegistryDefinedRoot root;
		if (!jsonObject.IsValid())
		{
			return root;
		}

		jsonObject->TryGetStringField(TEXT("key"), root.Key);
		jsonObject->TryGetStringField(TEXT("root"), root.Root);
		jsonObject->TryGetStringField(TEXT("relative_path"), root.RelativePath);
		jsonObject->TryGetBoolField(TEXT("scan"), root.bScan);

		const TSharedPtr<FJsonObject>* metaObject = nullptr;
		if (jsonObject->TryGetObjectField(TEXT("meta"), metaObject) && metaObject != nullptr)
		{
			root.Meta = ReadStringMap(*metaObject);
		}

		return root;
	}

	TSharedPtr<FJsonObject> WriteDefinedRoot(const FVdjmAssetRegistryDefinedRoot& root)
	{
		const TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("key"), root.Key);
		jsonObject->SetStringField(TEXT("root"), root.Root);
		jsonObject->SetStringField(TEXT("relative_path"), root.RelativePath);
		jsonObject->SetBoolField(TEXT("scan"), root.bScan);
		WriteStringMap(jsonObject, TEXT("meta"), root.Meta);
		return jsonObject;
	}

	void ReadVirtualNode(
		const TSharedPtr<FJsonObject>& jsonObject,
		const FString& parentPath,
		TArray<FVdjmAssetRegistryVirtualFolder>& outFolders)
	{
		if (!jsonObject.IsValid())
		{
			return;
		}

		FVdjmAssetRegistryVirtualFolder folder;
		jsonObject->TryGetStringField(TEXT("key"), folder.Key);
		jsonObject->TryGetStringField(TEXT("label"), folder.Label);
		folder.ParentPath = parentPath;
		const bool bIsRoot = parentPath.IsEmpty() && folder.Key == TEXT("root");
		folder.VirtualPath = bIsRoot
			? FString()
			: (parentPath.IsEmpty() ? folder.Key : FString::Printf(TEXT("%s-%s"), *parentPath, *folder.Key));

		const TSharedPtr<FJsonObject>* metaObject = nullptr;
		if (jsonObject->TryGetObjectField(TEXT("meta"), metaObject) && metaObject != nullptr)
		{
			folder.Meta = ReadStringMap(*metaObject);
		}
		outFolders.Add(folder);

		const TArray<TSharedPtr<FJsonValue>>* childValues = nullptr;
		if (jsonObject->TryGetArrayField(TEXT("children"), childValues) && childValues != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& childValue : *childValues)
			{
				ReadVirtualNode(childValue->AsObject(), folder.VirtualPath, outFolders);
			}
		}
	}

	TSharedPtr<FJsonObject> WriteVirtualNode(
		const FString& virtualPath,
		const TMap<FString, FVdjmAssetRegistryVirtualFolder>& folderByPath,
		const TMultiMap<FString, FString>& childrenByParent)
	{
		const FVdjmAssetRegistryVirtualFolder* folder = folderByPath.Find(virtualPath);
		const TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("key"), folder != nullptr && !folder->Key.IsEmpty() ? folder->Key : TEXT("root"));
		jsonObject->SetStringField(TEXT("label"), folder != nullptr ? folder->Label : TEXT("Root"));
		if (folder != nullptr)
		{
			WriteStringMap(jsonObject, TEXT("meta"), folder->Meta);
		}
		else
		{
			WriteStringMap(jsonObject, TEXT("meta"), TMap<FString, FString>());
		}

		TArray<FString> childPaths;
		childrenByParent.MultiFind(virtualPath, childPaths);
		childPaths.Sort();
		TArray<TSharedPtr<FJsonValue>> childValues;
		for (const FString& childPath : childPaths)
		{
			childValues.Add(MakeShared<FJsonValueObject>(WriteVirtualNode(childPath, folderByPath, childrenByParent)));
		}
		jsonObject->SetArrayField(TEXT("children"), childValues);
		return jsonObject;
	}

	FVdjmAssetRegistryAssetType ReadAssetType(const FString& key, const TSharedPtr<FJsonObject>& jsonObject)
	{
		FVdjmAssetRegistryAssetType assetType;
		assetType.Key = key;
		if (!jsonObject.IsValid())
		{
			return assetType;
		}

		jsonObject->TryGetStringField(TEXT("kind"), assetType.Kind);
		jsonObject->TryGetStringField(TEXT("default_class"), assetType.DefaultClass);
		assetType.Extensions = ReadStringArray(jsonObject, TEXT("extensions"));

		const TSharedPtr<FJsonObject>* metaObject = nullptr;
		if (jsonObject->TryGetObjectField(TEXT("meta"), metaObject) && metaObject != nullptr)
		{
			assetType.Meta = ReadStringMap(*metaObject);
		}

		return assetType;
	}

	TSharedPtr<FJsonObject> WriteAssetType(const FVdjmAssetRegistryAssetType& assetType)
	{
		const TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("kind"), assetType.Kind);
		jsonObject->SetStringField(TEXT("default_class"), assetType.DefaultClass);
		jsonObject->SetArrayField(TEXT("extensions"), WriteStringArray(assetType.Extensions));
		WriteStringMap(jsonObject, TEXT("meta"), assetType.Meta);
		return jsonObject;
	}

	FString MakeStoredVirtualPath(const FString& virtualPath)
	{
		if (virtualPath.IsEmpty() || (virtualPath.StartsWith(TEXT("#{")) && virtualPath.EndsWith(TEXT("}"))))
		{
			return virtualPath;
		}

		return FString::Printf(TEXT("#{%s}"), *virtualPath);
	}

	FVdjmAssetRegistryAssetEntry ReadAssetEntry(const TSharedPtr<FJsonObject>& jsonObject)
	{
		FVdjmAssetRegistryAssetEntry asset;
		if (!jsonObject.IsValid())
		{
			return asset;
		}

		jsonObject->TryGetStringField(TEXT("asset_key"), asset.AssetKey);
		jsonObject->TryGetStringField(TEXT("type"), asset.Type);
		jsonObject->TryGetStringField(TEXT("root"), asset.Root);
		jsonObject->TryGetStringField(TEXT("relative_path"), asset.RelativePath);
		jsonObject->TryGetStringField(TEXT("virtual_path"), asset.VirtualPath);
		jsonObject->TryGetStringField(TEXT("importance"), asset.Importance);
		jsonObject->TryGetStringField(TEXT("class"), asset.Class);
		asset.Tags = ReadStringArray(jsonObject, TEXT("tags"));

		const TSharedPtr<FJsonObject>* metaObject = nullptr;
		if (jsonObject->TryGetObjectField(TEXT("meta"), metaObject) && metaObject != nullptr)
		{
			asset.Meta = ReadStringMap(*metaObject);
		}

		asset.AssetKey = UVdjmAssetRegistryBlueprintLibrary::MakeAssetKey(asset.Type, asset.Root, asset.RelativePath);
		return asset;
	}

	TSharedPtr<FJsonObject> WriteAssetEntry(const FVdjmAssetRegistryAssetEntry& asset)
	{
		const TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("asset_key"), UVdjmAssetRegistryBlueprintLibrary::MakeAssetKey(asset.Type, asset.Root, asset.RelativePath));
		jsonObject->SetStringField(TEXT("type"), asset.Type);
		jsonObject->SetStringField(TEXT("root"), asset.Root);
		jsonObject->SetStringField(TEXT("relative_path"), NormalizePathString(asset.RelativePath));
		if (!asset.VirtualPath.IsEmpty())
		{
			jsonObject->SetStringField(TEXT("virtual_path"), MakeStoredVirtualPath(asset.VirtualPath));
		}
		jsonObject->SetStringField(TEXT("importance"), asset.Importance.IsEmpty() ? TEXT("optional") : asset.Importance);
		if (!asset.Class.IsEmpty())
		{
			jsonObject->SetStringField(TEXT("class"), asset.Class);
		}
		jsonObject->SetArrayField(TEXT("tags"), WriteStringArray(asset.Tags));
		WriteStringMap(jsonObject, TEXT("meta"), asset.Meta);
		return jsonObject;
	}

	FVdjmAssetRegistryRequirement ReadRequirement(const TSharedPtr<FJsonObject>& jsonObject)
	{
		FVdjmAssetRegistryRequirement requirement;
		if (!jsonObject.IsValid())
		{
			return requirement;
		}

		jsonObject->TryGetStringField(TEXT("virtual_path"), requirement.VirtualPath);
		jsonObject->TryGetStringField(TEXT("mode"), requirement.Mode);
		jsonObject->TryGetStringField(TEXT("type"), requirement.Type);
		jsonObject->TryGetStringField(TEXT("importance"), requirement.Importance);

		const TSharedPtr<FJsonObject>* metaObject = nullptr;
		if (jsonObject->TryGetObjectField(TEXT("meta"), metaObject) && metaObject != nullptr)
		{
			requirement.Meta = ReadStringMap(*metaObject);
		}

		return requirement;
	}

	TSharedPtr<FJsonObject> WriteRequirement(const FVdjmAssetRegistryRequirement& requirement)
	{
		const TSharedPtr<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("virtual_path"), MakeStoredVirtualPath(requirement.VirtualPath));
		jsonObject->SetStringField(TEXT("mode"), requirement.Mode);
		if (!requirement.Type.IsEmpty())
		{
			jsonObject->SetStringField(TEXT("type"), requirement.Type);
		}
		jsonObject->SetStringField(TEXT("importance"), requirement.Importance);
		WriteStringMap(jsonObject, TEXT("meta"), requirement.Meta);
		return jsonObject;
	}

	bool ReadDocumentFromJsonObject(
		const TSharedPtr<FJsonObject>& rootObject,
		FVdjmAssetRegistryDocument& outRegistry,
		TArray<FVdjmAssetRegistryMessage>& outMessages)
	{
		if (!rootObject.IsValid())
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("document_invalid"), TEXT("root"), TEXT("Registry JSON root is invalid."));
			return false;
		}

		outRegistry = FVdjmAssetRegistryDocument();
		outRegistry.SchemaVersion = static_cast<int32>(rootObject->GetNumberField(TEXT("schema_version")));
		if (outRegistry.SchemaVersion != VdjmAssetRegistrySchemaVersion)
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("schema_unsupported"), TEXT("schema_version"), FString::Printf(TEXT("Unsupported asset registry schema version %d."), outRegistry.SchemaVersion));
			return false;
		}

		const TSharedPtr<FJsonObject>* metaObject = nullptr;
		if (rootObject->TryGetObjectField(TEXT("meta"), metaObject) && metaObject != nullptr)
		{
			outRegistry.Meta = ReadStringMap(*metaObject);
		}

		const TSharedPtr<FJsonObject>* definesObject = nullptr;
		if (rootObject->TryGetObjectField(TEXT("defines"), definesObject) && definesObject != nullptr)
		{
			outRegistry.Defines = ReadStringMap(*definesObject);
		}

		const TSharedPtr<FJsonObject>* pathsObject = nullptr;
		if (rootObject->TryGetObjectField(TEXT("paths"), pathsObject) && pathsObject != nullptr)
		{
			const TSharedPtr<FJsonObject>* rootsObject = nullptr;
			if ((*pathsObject)->TryGetObjectField(TEXT("roots"), rootsObject) && rootsObject != nullptr)
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& pair : (*rootsObject)->Values)
				{
					outRegistry.Roots.Add(ReadPathRoot(pair.Key, pair.Value->AsObject()));
				}
			}

			const TSharedPtr<FJsonObject>* externalPathsObject = nullptr;
			if ((*pathsObject)->TryGetObjectField(TEXT("external_paths"), externalPathsObject) && externalPathsObject != nullptr)
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& pair : (*externalPathsObject)->Values)
				{
					outRegistry.ExternalPaths.Add(ReadPathRoot(pair.Key, pair.Value->AsObject()));
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* definedRootValues = nullptr;
			if ((*pathsObject)->TryGetArrayField(TEXT("defined_roots"), definedRootValues) && definedRootValues != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& definedRootValue : *definedRootValues)
				{
					outRegistry.DefinedRoots.Add(ReadDefinedRoot(definedRootValue->AsObject()));
				}
			}
		}

		const TSharedPtr<FJsonObject>* virtualsObject = nullptr;
		if (rootObject->TryGetObjectField(TEXT("virtuals"), virtualsObject) && virtualsObject != nullptr)
		{
			ReadVirtualNode(*virtualsObject, FString(), outRegistry.VirtualFolders);
		}

		const TSharedPtr<FJsonObject>* assetTypesObject = nullptr;
		if (rootObject->TryGetObjectField(TEXT("asset_types"), assetTypesObject) && assetTypesObject != nullptr)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& pair : (*assetTypesObject)->Values)
			{
				outRegistry.AssetTypes.Add(ReadAssetType(pair.Key, pair.Value->AsObject()));
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* assetValues = nullptr;
		if (rootObject->TryGetArrayField(TEXT("assets"), assetValues) && assetValues != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& assetValue : *assetValues)
			{
				outRegistry.Assets.Add(ReadAssetEntry(assetValue->AsObject()));
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* requirementValues = nullptr;
		if (rootObject->TryGetArrayField(TEXT("requirements"), requirementValues) && requirementValues != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& requirementValue : *requirementValues)
			{
				outRegistry.Requirements.Add(ReadRequirement(requirementValue->AsObject()));
			}
		}

		return true;
	}

	TSharedPtr<FJsonObject> WriteDocumentToJsonObject(const FVdjmAssetRegistryDocument& registry)
	{
		const TSharedPtr<FJsonObject> rootObject = MakeShared<FJsonObject>();
		rootObject->SetNumberField(TEXT("schema_version"), registry.SchemaVersion);
		WriteStringMap(rootObject, TEXT("meta"), registry.Meta);
		WriteStringMap(rootObject, TEXT("defines"), registry.Defines);

		const TSharedPtr<FJsonObject> pathsObject = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject> rootsObject = MakeShared<FJsonObject>();
		for (const FVdjmAssetRegistryPathRoot& root : registry.Roots)
		{
			rootsObject->SetObjectField(root.Key, WritePathRoot(root));
		}
		pathsObject->SetObjectField(TEXT("roots"), rootsObject);

		const TSharedPtr<FJsonObject> externalPathsObject = MakeShared<FJsonObject>();
		for (const FVdjmAssetRegistryPathRoot& root : registry.ExternalPaths)
		{
			externalPathsObject->SetObjectField(root.Key, WritePathRoot(root));
		}
		pathsObject->SetObjectField(TEXT("external_paths"), externalPathsObject);

		TArray<TSharedPtr<FJsonValue>> definedRootValues;
		for (const FVdjmAssetRegistryDefinedRoot& root : registry.DefinedRoots)
		{
			definedRootValues.Add(MakeShared<FJsonValueObject>(WriteDefinedRoot(root)));
		}
		pathsObject->SetArrayField(TEXT("defined_roots"), definedRootValues);
		rootObject->SetObjectField(TEXT("paths"), pathsObject);

		TMap<FString, FVdjmAssetRegistryVirtualFolder> folderByPath;
		TMultiMap<FString, FString> childrenByParent;
		for (const FVdjmAssetRegistryVirtualFolder& folder : registry.VirtualFolders)
		{
			folderByPath.Add(folder.VirtualPath, folder);
			if (!folder.VirtualPath.IsEmpty())
			{
				childrenByParent.Add(folder.ParentPath, folder.VirtualPath);
			}
		}
		rootObject->SetObjectField(TEXT("virtuals"), WriteVirtualNode(FString(), folderByPath, childrenByParent));

		const TSharedPtr<FJsonObject> assetTypesObject = MakeShared<FJsonObject>();
		for (const FVdjmAssetRegistryAssetType& assetType : registry.AssetTypes)
		{
			assetTypesObject->SetObjectField(assetType.Key, WriteAssetType(assetType));
		}
		rootObject->SetObjectField(TEXT("asset_types"), assetTypesObject);

		TArray<TSharedPtr<FJsonValue>> assetValues;
		for (const FVdjmAssetRegistryAssetEntry& asset : registry.Assets)
		{
			assetValues.Add(MakeShared<FJsonValueObject>(WriteAssetEntry(asset)));
		}
		rootObject->SetArrayField(TEXT("assets"), assetValues);

		TArray<TSharedPtr<FJsonValue>> requirementValues;
		for (const FVdjmAssetRegistryRequirement& requirement : registry.Requirements)
		{
			requirementValues.Add(MakeShared<FJsonValueObject>(WriteRequirement(requirement)));
		}
		rootObject->SetArrayField(TEXT("requirements"), requirementValues);
		return rootObject;
	}

	bool SerializeDocument(const FVdjmAssetRegistryDocument& registry, FString& outJsonText)
	{
		const TSharedPtr<FJsonObject> rootObject = WriteDocumentToJsonObject(registry);
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&outJsonText);
		return FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer);
	}

	bool ExtractVirtualPath(const FString& source, FString& outVirtualPath)
	{
		outVirtualPath.Reset();
		if (source.IsEmpty())
		{
			return true;
		}
		if (!source.StartsWith(TEXT("#{")) || !source.EndsWith(TEXT("}")))
		{
			return false;
		}

		outVirtualPath = source.Mid(2, source.Len() - 3);
		return !outVirtualPath.IsEmpty();
	}

	TSet<FString> BuildVirtualPathSet(const FVdjmAssetRegistryDocument& registry, TArray<FVdjmAssetRegistryMessage>& outMessages)
	{
		TSet<FString> virtualPaths;
		for (const FVdjmAssetRegistryVirtualFolder& folder : registry.VirtualFolders)
		{
			if (!folder.Key.IsEmpty() && folder.Key.Contains(TEXT("-")))
			{
				AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("virtual_key_invalid"), folder.ParentPath, FString::Printf(TEXT("Virtual folder key '%s' contains '-' which is reserved for virtual hierarchy."), *folder.Key));
			}
			if (!folder.VirtualPath.IsEmpty())
			{
				if (virtualPaths.Contains(folder.VirtualPath))
				{
					AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("virtual_duplicate"), folder.VirtualPath, TEXT("Duplicate virtual path."));
				}
				virtualPaths.Add(folder.VirtualPath);
			}
		}
		return virtualPaths;
	}

	void BuildLookupSets(
		const FVdjmAssetRegistryDocument& registry,
		TSet<FString>& outRootKeys,
		TSet<FString>& outAssetTypes)
	{
		outRootKeys.Reset();
		outAssetTypes.Reset();

		for (const FVdjmAssetRegistryPathRoot& root : registry.Roots)
		{
			outRootKeys.Add(root.Key);
		}
		for (const FVdjmAssetRegistryPathRoot& root : registry.ExternalPaths)
		{
			outRootKeys.Add(root.Key);
		}
		for (const FVdjmAssetRegistryDefinedRoot& root : registry.DefinedRoots)
		{
			outRootKeys.Add(root.Key);
		}
		for (const FVdjmAssetRegistryAssetType& assetType : registry.AssetTypes)
		{
			outAssetTypes.Add(assetType.Key);
		}
	}

	const FVdjmAssetRegistryPathRoot* FindRoot(const FVdjmAssetRegistryDocument& registry, const FString& key)
	{
		for (const FVdjmAssetRegistryPathRoot& root : registry.Roots)
		{
			if (root.Key == key)
			{
				return &root;
			}
		}
		for (const FVdjmAssetRegistryPathRoot& root : registry.ExternalPaths)
		{
			if (root.Key == key)
			{
				return &root;
			}
		}
		return nullptr;
	}

	const FVdjmAssetRegistryDefinedRoot* FindDefinedRoot(const FVdjmAssetRegistryDocument& registry, const FString& key)
	{
		for (const FVdjmAssetRegistryDefinedRoot& root : registry.DefinedRoots)
		{
			if (root.Key == key)
			{
				return &root;
			}
		}
		return nullptr;
	}

	bool ResolveRootFullPath(
		const FVdjmAssetRegistryDocument& registry,
		const FString& key,
		FString& outFullPath,
		TArray<FVdjmAssetRegistryMessage>& outMessages,
		const TSet<FString>* stack = nullptr)
	{
		TSet<FString> localStack = stack != nullptr ? *stack : TSet<FString>();
		if (localStack.Contains(key))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("root_cycle"), key, TEXT("Defined root cycle detected."));
			return false;
		}
		localStack.Add(key);

		if (const FVdjmAssetRegistryPathRoot* root = FindRoot(registry, key))
		{
			FString rootPath = GetPlatformPath(*root);
			if (rootPath.IsEmpty())
			{
				AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("root_empty"), key, TEXT("Root path is empty."));
				return false;
			}

			outFullPath = FPaths::IsRelative(rootPath) ? FPaths::Combine(GetPluginBaseDir(), rootPath) : rootPath;
			outFullPath = FPaths::ConvertRelativePathToFull(outFullPath);
			FPaths::NormalizeDirectoryName(outFullPath);
			return true;
		}

		if (const FVdjmAssetRegistryDefinedRoot* definedRoot = FindDefinedRoot(registry, key))
		{
			FString basePath;
			if (!ResolveRootFullPath(registry, definedRoot->Root, basePath, outMessages, &localStack))
			{
				return false;
			}
			if (IsParentPathBlocked(definedRoot->RelativePath))
			{
				AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("defined_root_parent_path"), definedRoot->Key, TEXT("Defined root relative_path cannot contain '..'."));
				return false;
			}

			outFullPath = FPaths::Combine(basePath, definedRoot->RelativePath);
			outFullPath = FPaths::ConvertRelativePathToFull(outFullPath);
			FPaths::NormalizeDirectoryName(outFullPath);
			return true;
		}

		AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("root_missing"), key, FString::Printf(TEXT("Root '%s' is not defined."), *key));
		return false;
	}

	FString GetDefaultClassForType(const FVdjmAssetRegistryDocument& registry, const FString& type)
	{
		for (const FVdjmAssetRegistryAssetType& assetType : registry.AssetTypes)
		{
			if (assetType.Key == type)
			{
				return assetType.DefaultClass;
			}
		}
		return FString();
	}

	FString InferTypeFromExtension(const FString& extension)
	{
		const FString lowerExtension = extension.ToLower();
		if (lowerExtension == TEXT(".umap"))
		{
			return TEXT("map");
		}
		if (lowerExtension == TEXT(".png") || lowerExtension == TEXT(".jpg") || lowerExtension == TEXT(".jpeg") || lowerExtension == TEXT(".tga") || lowerExtension == TEXT(".bmp") || lowerExtension == TEXT(".webp"))
		{
			return TEXT("image");
		}
		if (lowerExtension == TEXT(".svg"))
		{
			return TEXT("svg");
		}
		if (lowerExtension == TEXT(".wav") || lowerExtension == TEXT(".mp3") || lowerExtension == TEXT(".ogg") || lowerExtension == TEXT(".flac"))
		{
			return TEXT("sound");
		}
		if (lowerExtension == TEXT(".ttf") || lowerExtension == TEXT(".otf"))
		{
			return TEXT("font");
		}
		if (lowerExtension == TEXT(".usf") || lowerExtension == TEXT(".ush") || lowerExtension == TEXT(".hlsl"))
		{
			return TEXT("shader");
		}
		if (lowerExtension == TEXT(".mp4") || lowerExtension == TEXT(".mov") || lowerExtension == TEXT(".webm"))
		{
			return TEXT("media");
		}
		if (lowerExtension == TEXT(".json") || lowerExtension == TEXT(".txt") || lowerExtension == TEXT(".md") || lowerExtension == TEXT(".ps1"))
		{
			return TEXT("raw_file");
		}
		return TEXT("other");
	}

	FString InferTypeFromClassPath(const FString& classPath, const FString& extension)
	{
		const FString lowerClassPath = classPath.ToLower();
		if (extension.Equals(TEXT(".umap"), ESearchCase::IgnoreCase) || lowerClassPath.Contains(TEXT("world")))
		{
			return TEXT("map");
		}
		if (lowerClassPath.Contains(TEXT("widgetblueprint")) || lowerClassPath.Contains(TEXT("userwidget")))
		{
			return TEXT("widget_bp");
		}
		if (lowerClassPath.Contains(TEXT("texture")))
		{
			return TEXT("image");
		}
		if (lowerClassPath.Contains(TEXT("sound")))
		{
			return TEXT("sound");
		}
		if (lowerClassPath.Contains(TEXT("material")))
		{
			return TEXT("material");
		}
		if (lowerClassPath.Contains(TEXT("font")))
		{
			return TEXT("font");
		}
		if (lowerClassPath.Contains(TEXT("levelsequence")))
		{
			return TEXT("level_sequence");
		}
		if (lowerClassPath.Contains(TEXT("media")))
		{
			return TEXT("media");
		}
		if (lowerClassPath.Contains(TEXT("dataasset")))
		{
			return TEXT("data_asset");
		}
		if (lowerClassPath.Contains(TEXT("blueprint")))
		{
			return TEXT("blueprint");
		}
		return InferTypeFromExtension(extension);
	}

	bool TryGetUnrealAssetClassPath(const FString& filePath, FString& outClassPath)
	{
		outClassPath.Reset();
		const FString extension = FPaths::GetExtension(filePath, true);
		if (!extension.Equals(TEXT(".uasset"), ESearchCase::IgnoreCase) && !extension.Equals(TEXT(".umap"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		if (extension.Equals(TEXT(".umap"), ESearchCase::IgnoreCase))
		{
			outClassPath = TEXT("/Script/Engine.World");
			return true;
		}

		FString contentDir = GetPluginContentDir();
		FPaths::NormalizeDirectoryName(contentDir);

		FString normalizedFilePath = FPaths::ConvertRelativePathToFull(filePath);
		FPaths::NormalizeFilename(normalizedFilePath);
		if (!normalizedFilePath.StartsWith(contentDir))
		{
			return false;
		}

		FString relativePackagePath = normalizedFilePath.RightChop(contentDir.Len());
		relativePackagePath.RemoveFromStart(TEXT("/"));
		relativePackagePath = RemovePathExtension(relativePackagePath);
		const FString packageName = FString::Printf(TEXT("/%s/%s"), VDJM_ASSET_REGISTRY_PLUGIN_NAME, *NormalizePathString(relativePackagePath));

		FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FString> filesToScan;
		filesToScan.Add(normalizedFilePath);
		assetRegistryModule.Get().ScanFilesSynchronous(filesToScan, false);

		FARFilter filter;
		filter.PackageNames.Add(FName(*packageName));
		TArray<FAssetData> assetDataList;
		assetRegistryModule.Get().GetAssets(filter, assetDataList);
		if (assetDataList.Num() <= 0)
		{
			return false;
		}

		outClassPath = assetDataList[0].AssetClassPath.ToString();
		return true;
	}

	FString MakeRelativeAssetPathForFile(const FString& scanRootFullPath, const FString& filePath)
	{
		FString relativePath = filePath;
		FPaths::MakePathRelativeTo(relativePath, *scanRootFullPath);
		relativePath = NormalizePathString(relativePath);

		const FString extension = FPaths::GetExtension(relativePath, true);
		if (extension.Equals(TEXT(".uasset"), ESearchCase::IgnoreCase) || extension.Equals(TEXT(".umap"), ESearchCase::IgnoreCase))
		{
			relativePath = RemovePathExtension(relativePath);
		}

		return NormalizePathString(relativePath);
	}

	bool ShouldSkipScanFile(const FString& filePath)
	{
		const FString normalizedPath = NormalizePathString(filePath);
		return normalizedPath.Contains(TEXT("/Binaries/"))
			|| normalizedPath.Contains(TEXT("/Intermediate/"))
			|| normalizedPath.Contains(TEXT("/Saved/"))
			|| normalizedPath.Contains(TEXT("/DerivedDataCache/"));
	}

	bool AssetEntryPhysicalExists(
		const FVdjmAssetRegistryDocument& registry,
		const FVdjmAssetRegistryAssetEntry& asset,
		TArray<FVdjmAssetRegistryMessage>& outMessages)
	{
		FString rootPath;
		if (!ResolveRootFullPath(registry, asset.Root, rootPath, outMessages))
		{
			return false;
		}

		const FString basePath = FPaths::Combine(rootPath, asset.RelativePath);
		if (FPaths::FileExists(basePath))
		{
			return true;
		}

		static const TArray<FString> unrealExtensions = { TEXT(".uasset"), TEXT(".umap") };
		for (const FString& extension : unrealExtensions)
		{
			if (FPaths::FileExists(basePath + extension))
			{
				return true;
			}
		}

		return false;
	}

	TArray<FVdjmAssetRegistryScanRoot> BuildScanRoots(
		const FVdjmAssetRegistryDocument& registry,
		TArray<FVdjmAssetRegistryMessage>& outMessages)
	{
		TArray<FVdjmAssetRegistryScanRoot> scanRoots;
		for (const FVdjmAssetRegistryPathRoot& root : registry.Roots)
		{
			if (!root.bScan)
			{
				continue;
			}

			FString fullPath;
			if (ResolveRootFullPath(registry, root.Key, fullPath, outMessages))
			{
				scanRoots.Add({ root.Key, fullPath, false });
			}
		}
		for (const FVdjmAssetRegistryPathRoot& root : registry.ExternalPaths)
		{
			if (!root.bScan)
			{
				continue;
			}

			FString fullPath;
			if (ResolveRootFullPath(registry, root.Key, fullPath, outMessages))
			{
				scanRoots.Add({ root.Key, fullPath, false });
			}
		}
		for (const FVdjmAssetRegistryDefinedRoot& root : registry.DefinedRoots)
		{
			if (!root.bScan)
			{
				continue;
			}

			FString fullPath;
			if (ResolveRootFullPath(registry, root.Key, fullPath, outMessages))
			{
				scanRoots.Add({ root.Key, fullPath, true });
			}
		}

		scanRoots.Sort([](const FVdjmAssetRegistryScanRoot& lhs, const FVdjmAssetRegistryScanRoot& rhs)
		{
			return lhs.FullPath.Len() > rhs.FullPath.Len();
		});
		return scanRoots;
	}
}

bool UVdjmAssetRegistryBlueprintLibrary::GetDefaultRegistryFilePath(FString& outFilePath)
{
	outFilePath = FPaths::Combine(GetPluginBaseDir(), TEXT("Config"), VDJM_ASSET_REGISTRY_DEFAULT_CONFIG);
	return !outFilePath.IsEmpty();
}

FString UVdjmAssetRegistryBlueprintLibrary::MakeAssetKey(const FString& type, const FString& root, const FString& relativePath)
{
	return FString::Printf(TEXT("%s:%s:%s"), *type, *root, *NormalizePathString(relativePath));
}

bool UVdjmAssetRegistryBlueprintLibrary::LoadDefaultRegistry(FVdjmAssetRegistryDocument& outRegistry, TArray<FVdjmAssetRegistryMessage>& outMessages)
{
	outMessages.Reset();
	outRegistry = FVdjmAssetRegistryDocument();

	FString registryPath;
	GetDefaultRegistryFilePath(registryPath);
	if (!FPaths::FileExists(registryPath))
	{
		AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("file_missing"), registryPath, TEXT("Default asset registry JSON does not exist."));
		return false;
	}

	FString rawJson;
	if (!FFileHelper::LoadFileToString(rawJson, *registryPath))
	{
		AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("file_read_failed"), registryPath, TEXT("Failed to read default asset registry JSON."));
		return false;
	}

	TArray<FVdjmAssetRegistryMessage> parseMessages;
	const TSharedPtr<FJsonObject> rawRootObject = ParseJsonObject(rawJson, parseMessages, registryPath);
	outMessages.Append(parseMessages);
	if (!rawRootObject.IsValid())
	{
		return false;
	}

	TMap<FString, FString> rawDefines;
	const TSharedPtr<FJsonObject>* definesObject = nullptr;
	if (rawRootObject->TryGetObjectField(TEXT("defines"), definesObject) && definesObject != nullptr)
	{
		rawDefines = ReadStringMap(*definesObject);
	}

	TMap<FString, FString> resolvedDefines;
	if (!ResolveDefines(rawDefines, resolvedDefines, outMessages))
	{
		return false;
	}

	FString expandedJson;
	if (!ReplaceDefineTokens(rawJson, resolvedDefines, expandedJson, outMessages, registryPath))
	{
		return false;
	}

	const TSharedPtr<FJsonObject> expandedRootObject = ParseJsonObject(expandedJson, outMessages, registryPath);
	if (!expandedRootObject.IsValid())
	{
		return false;
	}

	if (!ReadDocumentFromJsonObject(expandedRootObject, outRegistry, outMessages))
	{
		return false;
	}

	outRegistry.Defines = resolvedDefines;
	ValidateRegistry(outRegistry, outMessages);
	return !HasError(outMessages);
}

bool UVdjmAssetRegistryBlueprintLibrary::SaveDefaultRegistry(const FVdjmAssetRegistryDocument& registry, TArray<FVdjmAssetRegistryMessage>& outMessages)
{
	outMessages.Reset();

	FString registryPath;
	GetDefaultRegistryFilePath(registryPath);
	const FString registryDir = FPaths::GetPath(registryPath);
	if (!FPaths::DirectoryExists(registryDir))
	{
		IFileManager::Get().MakeDirectory(*registryDir, true);
	}

	if (FPaths::FileExists(registryPath))
	{
		const FString backupPath = registryPath + TEXT(".bak");
		IFileManager::Get().Copy(*backupPath, *registryPath, true, true);
	}

	FString jsonText;
	if (!SerializeDocument(registry, jsonText))
	{
		AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("json_serialize_failed"), registryPath, TEXT("Failed to serialize asset registry JSON."));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(jsonText, *registryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("file_write_failed"), registryPath, TEXT("Failed to save default asset registry JSON."));
		return false;
	}

	AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Info, TEXT("saved"), registryPath, TEXT("Saved default asset registry JSON."));
	return true;
}

bool UVdjmAssetRegistryBlueprintLibrary::ValidateRegistry(const FVdjmAssetRegistryDocument& registry, TArray<FVdjmAssetRegistryMessage>& outMessages)
{
	if (registry.SchemaVersion != VdjmAssetRegistrySchemaVersion)
	{
		AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("schema_unsupported"), TEXT("schema_version"), FString::Printf(TEXT("Unsupported schema version %d."), registry.SchemaVersion));
	}

	TSet<FString> rootKeys;
	TSet<FString> assetTypes;
	BuildLookupSets(registry, rootKeys, assetTypes);
	const TSet<FString> virtualPaths = BuildVirtualPathSet(registry, outMessages);

	TSet<FString> assetKeys;
	for (const FVdjmAssetRegistryAssetEntry& asset : registry.Assets)
	{
		const FString assetKey = MakeAssetKey(asset.Type, asset.Root, asset.RelativePath);
		if (assetKeys.Contains(assetKey))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("asset_duplicate"), assetKey, TEXT("Duplicate asset key."));
		}
		assetKeys.Add(assetKey);

		if (!assetTypes.Contains(asset.Type))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("asset_type_missing"), assetKey, FString::Printf(TEXT("Unknown asset type '%s'."), *asset.Type));
		}
		if (!rootKeys.Contains(asset.Root))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("asset_root_missing"), assetKey, FString::Printf(TEXT("Unknown asset root '%s'."), *asset.Root));
		}
		if (IsParentPathBlocked(asset.RelativePath))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("asset_parent_path"), assetKey, TEXT("Asset relative_path cannot contain '..'."));
		}
		if (!asset.Importance.IsEmpty()
			&& asset.Importance != TEXT("required")
			&& asset.Importance != TEXT("recommended")
			&& asset.Importance != TEXT("optional"))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("asset_importance_invalid"), assetKey, FString::Printf(TEXT("Invalid importance '%s'."), *asset.Importance));
		}

		FString virtualPath;
		if (!ExtractVirtualPath(asset.VirtualPath, virtualPath))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("asset_virtual_syntax"), assetKey, TEXT("virtual_path must use #{path-to-node} syntax."));
		}
		else if (!virtualPath.IsEmpty() && !virtualPaths.Contains(virtualPath))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("asset_virtual_missing"), assetKey, FString::Printf(TEXT("Virtual path '%s' is not declared."), *virtualPath));
		}

		if (!AssetEntryPhysicalExists(registry, asset, outMessages))
		{
			const EVdjmAssetRegistryMessageSeverity severity = asset.Importance == TEXT("required")
				? EVdjmAssetRegistryMessageSeverity::Error
				: EVdjmAssetRegistryMessageSeverity::Warning;
			AddMessage(outMessages, severity, TEXT("asset_file_missing"), assetKey, TEXT("Registered asset file is missing."));
		}
	}

	for (const FVdjmAssetRegistryRequirement& requirement : registry.Requirements)
	{
		FString virtualPath;
		if (!ExtractVirtualPath(requirement.VirtualPath, virtualPath))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("requirement_virtual_syntax"), TEXT("requirements"), TEXT("Requirement virtual_path must use #{path-to-node} syntax."));
		}
		else if (!virtualPath.IsEmpty() && !virtualPaths.Contains(virtualPath))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("requirement_virtual_missing"), virtualPath, TEXT("Requirement virtual_path is not declared."));
		}
		if (!requirement.Type.IsEmpty() && !assetTypes.Contains(requirement.Type))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Error, TEXT("requirement_type_missing"), requirement.Type, TEXT("Requirement references an unknown asset type."));
		}
	}

	return !HasError(outMessages);
}

bool UVdjmAssetRegistryBlueprintLibrary::ScanDefaultRegistry(
	const bool bRegisterDiscoveredAssets,
	const bool bSaveAfterScan,
	FVdjmAssetRegistryDocument& outRegistry,
	FVdjmAssetRegistryScanResult& outScanResult,
	TArray<FVdjmAssetRegistryMessage>& outMessages)
{
	outMessages.Reset();
	outScanResult = FVdjmAssetRegistryScanResult();

	if (!LoadDefaultRegistry(outRegistry, outMessages))
	{
		return false;
	}

	TArray<FVdjmAssetRegistryMessage> scanMessages;
	const TArray<FVdjmAssetRegistryScanRoot> scanRoots = BuildScanRoots(outRegistry, scanMessages);
	outMessages.Append(scanMessages);
	if (scanRoots.Num() <= 0)
	{
		AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Warning, TEXT("scan_root_empty"), TEXT("paths"), TEXT("No scan-enabled root was found."));
		return !HasError(outMessages);
	}

	TMap<FString, int32> assetIndexByKey;
	for (int32 assetIndex = 0; assetIndex < outRegistry.Assets.Num(); ++assetIndex)
	{
		FVdjmAssetRegistryAssetEntry& asset = outRegistry.Assets[assetIndex];
		asset.AssetKey = MakeAssetKey(asset.Type, asset.Root, asset.RelativePath);
		assetIndexByKey.Add(asset.AssetKey, assetIndex);
	}

	TSet<FString> discoveredKeys;
	TSet<FString> seenFiles;
	for (const FVdjmAssetRegistryScanRoot& scanRoot : scanRoots)
	{
		if (!FPaths::DirectoryExists(scanRoot.FullPath))
		{
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Warning, TEXT("scan_root_missing"), scanRoot.Key, FString::Printf(TEXT("Scan root '%s' does not exist."), *scanRoot.FullPath));
			continue;
		}

		TArray<FString> filePaths;
		IFileManager::Get().FindFilesRecursive(filePaths, *scanRoot.FullPath, TEXT("*.*"), true, false);
		for (FString filePath : filePaths)
		{
			filePath = FPaths::ConvertRelativePathToFull(filePath);
			FPaths::NormalizeFilename(filePath);
			if (seenFiles.Contains(filePath) || ShouldSkipScanFile(filePath))
			{
				continue;
			}
			seenFiles.Add(filePath);
			++outScanResult.ScannedFileCount;

			const FString extension = FPaths::GetExtension(filePath, true);
			FString classPath;
			TryGetUnrealAssetClassPath(filePath, classPath);

			FVdjmAssetRegistryAssetEntry scannedAsset;
			scannedAsset.Root = scanRoot.Key;
			scannedAsset.RelativePath = MakeRelativeAssetPathForFile(scanRoot.FullPath, filePath);
			scannedAsset.Type = classPath.IsEmpty() ? InferTypeFromExtension(extension) : InferTypeFromClassPath(classPath, extension);
			scannedAsset.Class = classPath.IsEmpty() ? GetDefaultClassForType(outRegistry, scannedAsset.Type) : classPath;
			scannedAsset.Importance = TEXT("optional");
			scannedAsset.Meta.Add(TEXT("source"), TEXT("scan"));
			scannedAsset.AssetKey = MakeAssetKey(scannedAsset.Type, scannedAsset.Root, scannedAsset.RelativePath);
			discoveredKeys.Add(scannedAsset.AssetKey);

			if (int32* existingIndex = assetIndexByKey.Find(scannedAsset.AssetKey))
			{
				FVdjmAssetRegistryAssetEntry& existingAsset = outRegistry.Assets[*existingIndex];
				bool bUpdated = false;
				if (existingAsset.Class.IsEmpty() && !scannedAsset.Class.IsEmpty())
				{
					existingAsset.Class = scannedAsset.Class;
					bUpdated = true;
				}
				if (existingAsset.Importance.IsEmpty())
				{
					existingAsset.Importance = TEXT("optional");
					bUpdated = true;
				}

				if (bUpdated)
				{
					++outScanResult.UpdatedAssetCount;
					outScanResult.UpdatedAssetKeys.Add(scannedAsset.AssetKey);
				}
				else
				{
					++outScanResult.UnchangedAssetCount;
				}
			}
			else if (bRegisterDiscoveredAssets)
			{
				outRegistry.Assets.Add(scannedAsset);
				assetIndexByKey.Add(scannedAsset.AssetKey, outRegistry.Assets.Num() - 1);
				++outScanResult.AddedAssetCount;
				outScanResult.AddedAssetKeys.Add(scannedAsset.AssetKey);
			}
		}
	}

	for (const FVdjmAssetRegistryAssetEntry& asset : outRegistry.Assets)
	{
		const FString assetKey = MakeAssetKey(asset.Type, asset.Root, asset.RelativePath);
		if (!discoveredKeys.Contains(assetKey) && !AssetEntryPhysicalExists(outRegistry, asset, outMessages))
		{
			++outScanResult.MissingRegisteredAssetCount;
			outScanResult.MissingAssetKeys.Add(assetKey);
		}
	}

	if (bRegisterDiscoveredAssets && bSaveAfterScan)
	{
		TArray<FVdjmAssetRegistryMessage> saveMessages;
		SaveDefaultRegistry(outRegistry, saveMessages);
		outMessages.Append(saveMessages);
	}

	return !HasError(outMessages);
}

bool UVdjmAssetRegistryBlueprintLibrary::RemoveAssetFromDefaultRegistry(
	const FString& assetKey,
	const bool bSaveAfterRemove,
	FVdjmAssetRegistryDocument& outRegistry,
	TArray<FVdjmAssetRegistryMessage>& outMessages)
{
	outMessages.Reset();
	if (!LoadDefaultRegistry(outRegistry, outMessages))
	{
		return false;
	}

	for (int32 assetIndex = 0; assetIndex < outRegistry.Assets.Num(); ++assetIndex)
	{
		if (MakeAssetKey(outRegistry.Assets[assetIndex].Type, outRegistry.Assets[assetIndex].Root, outRegistry.Assets[assetIndex].RelativePath) == assetKey)
		{
			outRegistry.Assets.RemoveAt(assetIndex);
			AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Info, TEXT("asset_removed"), assetKey, TEXT("Removed asset entry from registry. The file was not deleted."));
			if (bSaveAfterRemove)
			{
				TArray<FVdjmAssetRegistryMessage> saveMessages;
				SaveDefaultRegistry(outRegistry, saveMessages);
				outMessages.Append(saveMessages);
			}
			return !HasError(outMessages);
		}
	}

	AddMessage(outMessages, EVdjmAssetRegistryMessageSeverity::Warning, TEXT("asset_remove_missing"), assetKey, TEXT("Asset key was not found in registry."));
	return !HasError(outMessages);
}

bool UVdjmAssetRegistryBlueprintLibrary::GetRegistrySummary(const FVdjmAssetRegistryDocument& registry, FString& outSummary)
{
	TMap<FString, int32> countByType;
	TMap<FString, int32> countByVirtualPath;
	int32 requiredCount = 0;
	int32 recommendedCount = 0;
	int32 optionalCount = 0;

	for (const FVdjmAssetRegistryAssetEntry& asset : registry.Assets)
	{
		countByType.FindOrAdd(asset.Type)++;

		FString virtualPath;
		if (ExtractVirtualPath(asset.VirtualPath, virtualPath) && !virtualPath.IsEmpty())
		{
			countByVirtualPath.FindOrAdd(virtualPath)++;
		}
		else
		{
			countByVirtualPath.FindOrAdd(TEXT("<unclassified>"))++;
		}

		if (asset.Importance == TEXT("required"))
		{
			++requiredCount;
		}
		else if (asset.Importance == TEXT("recommended"))
		{
			++recommendedCount;
		}
		else
		{
			++optionalCount;
		}
	}

	TArray<FString> lines;
	lines.Add(FString::Printf(TEXT("VdjmAssetRegistry schema=%d assets=%d requirements=%d"), registry.SchemaVersion, registry.Assets.Num(), registry.Requirements.Num()));
	lines.Add(FString::Printf(TEXT("Importance required=%d recommended=%d optional=%d"), requiredCount, recommendedCount, optionalCount));
	lines.Add(TEXT(""));
	lines.Add(TEXT("[Types]"));

	TArray<FString> typeKeys;
	countByType.GetKeys(typeKeys);
	typeKeys.Sort();
	for (const FString& typeKey : typeKeys)
	{
		lines.Add(FString::Printf(TEXT("- %s: %d"), *typeKey, countByType[typeKey]));
	}

	lines.Add(TEXT(""));
	lines.Add(TEXT("[Virtual Folders]"));
	TArray<FString> virtualKeys;
	countByVirtualPath.GetKeys(virtualKeys);
	virtualKeys.Sort();
	for (const FString& virtualKey : virtualKeys)
	{
		lines.Add(FString::Printf(TEXT("- %s: %d"), *virtualKey, countByVirtualPath[virtualKey]));
	}

	lines.Add(TEXT(""));
	lines.Add(TEXT("[Assets]"));
	TArray<FString> assetLines;
	for (const FVdjmAssetRegistryAssetEntry& asset : registry.Assets)
	{
		assetLines.Add(FString::Printf(
			TEXT("- %s | %s | %s | %s"),
			*MakeAssetKey(asset.Type, asset.Root, asset.RelativePath),
			*asset.Importance,
			*asset.VirtualPath,
			*asset.Class));
	}
	assetLines.Sort();
	lines.Append(assetLines);

	outSummary = FString::Join(lines, LINE_TERMINATOR);
	return true;
}
