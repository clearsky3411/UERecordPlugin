#include "VdjmRecordAppState.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VdjmRecordTypes.h"
#include "VdjmRecorderCore.h"
#include "VdjmRecorderWorldContextSubsystem.h"

namespace
{
	FString GetJsonStringField(
		const TSharedPtr<FJsonObject>& jsonObject,
		const FString& fieldName,
		const FString& fallbackValue = FString())
	{
		if (not jsonObject.IsValid())
		{
			return fallbackValue;
		}

		FString valueString;
		return jsonObject->TryGetStringField(fieldName, valueString)
			? valueString
			: fallbackValue;
	}

	int64 GetJsonInt64Field(
		const TSharedPtr<FJsonObject>& jsonObject,
		const FString& fieldName,
		int64 fallbackValue = 0)
	{
		if (not jsonObject.IsValid())
		{
			return fallbackValue;
		}

		double valueNumber = 0.0;
		return jsonObject->TryGetNumberField(fieldName, valueNumber)
			? static_cast<int64>(valueNumber)
			: fallbackValue;
	}

	int32 GetJsonInt32Field(
		const TSharedPtr<FJsonObject>& jsonObject,
		const FString& fieldName,
		int32 fallbackValue = 0)
	{
		return static_cast<int32>(GetJsonInt64Field(jsonObject, fieldName, fallbackValue));
	}

	bool GetJsonBoolField(
		const TSharedPtr<FJsonObject>& jsonObject,
		const FString& fieldName,
		bool bFallbackValue = false)
	{
		if (not jsonObject.IsValid())
		{
			return bFallbackValue;
		}

		bool bValue = false;
		return jsonObject->TryGetBoolField(fieldName, bValue)
			? bValue
			: bFallbackValue;
	}

	TSharedPtr<FJsonObject> GetJsonObjectField(const TSharedPtr<FJsonObject>& jsonObject, const FString& fieldName)
	{
		if (not jsonObject.IsValid())
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject>* objectPtr = nullptr;
		return jsonObject->TryGetObjectField(fieldName, objectPtr) && objectPtr != nullptr
			? *objectPtr
			: nullptr;
	}

	FString GetSoftPathString(const FSoftObjectPath& softObjectPath)
	{
		return softObjectPath.IsNull() ? FString() : softObjectPath.ToString();
	}

	template <typename EnumType>
	FString GetEnumValueString(EnumType enumValue)
	{
		const UEnum* enumObject = StaticEnum<EnumType>();
		return enumObject != nullptr
			? enumObject->GetValueAsString(enumValue)
			: FString();
	}

	FString ResolveLocalFileStatus(const FVdjmRecordMediaRegistryEntry& registryEntry)
	{
		if (registryEntry.bIsDeleted)
		{
			return TEXT("deleted");
		}

		if (not registryEntry.bMetadataFileExists)
		{
			return TEXT("missing_metadata");
		}

		if (not registryEntry.bOutputFileExists && registryEntry.PublishedContentUri.IsEmpty())
		{
			return TEXT("missing_media");
		}

		if (not registryEntry.bOutputFileExists)
		{
			return TEXT("remote_only");
		}

		return TEXT("available");
	}

	TSharedRef<FJsonObject> MakeUserJsonObject(const FVdjmRecordAppStateUserSection& userSection)
	{
		TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("user_id"), userSection.UserId);
		jsonObject->SetStringField(TEXT("authority_role"), userSection.AuthorityRole);
		jsonObject->SetStringField(TEXT("developer_id"), userSection.DeveloperId);
		jsonObject->SetStringField(TEXT("session_token_id"), userSection.SessionTokenId);
		return jsonObject;
	}

	void ReadUserJsonObject(const TSharedPtr<FJsonObject>& jsonObject, FVdjmRecordAppStateUserSection& outUserSection)
	{
		outUserSection.UserId = GetJsonStringField(jsonObject, TEXT("user_id"));
		outUserSection.AuthorityRole = GetJsonStringField(jsonObject, TEXT("authority_role"), TEXT("developer"));
		outUserSection.DeveloperId = GetJsonStringField(jsonObject, TEXT("developer_id"));
		outUserSection.SessionTokenId = GetJsonStringField(jsonObject, TEXT("session_token_id"));
	}

	TSharedRef<FJsonObject> MakeRemoteJsonObject(const FVdjmRecordAppStateRemoteSection& remoteSection)
	{
		TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("provider"), remoteSection.Provider);
		jsonObject->SetStringField(TEXT("base_url"), remoteSection.BaseUrl);
		jsonObject->SetStringField(TEXT("health_endpoint_path"), remoteSection.HealthEndpointPath);
		jsonObject->SetStringField(TEXT("upload_request_endpoint_path"), remoteSection.UploadRequestEndpointPath);
		jsonObject->SetBoolField(TEXT("prefer_direct_upload"), remoteSection.bPreferDirectUpload);
		return jsonObject;
	}

	void ReadRemoteJsonObject(const TSharedPtr<FJsonObject>& jsonObject, FVdjmRecordAppStateRemoteSection& outRemoteSection)
	{
		outRemoteSection.Provider = GetJsonStringField(jsonObject, TEXT("provider"), TEXT("local"));
		outRemoteSection.BaseUrl = GetJsonStringField(jsonObject, TEXT("base_url"));
		outRemoteSection.HealthEndpointPath = GetJsonStringField(jsonObject, TEXT("health_endpoint_path"), TEXT("/health"));
		outRemoteSection.UploadRequestEndpointPath = GetJsonStringField(jsonObject, TEXT("upload_request_endpoint_path"), TEXT("/upload/request"));
		outRemoteSection.bPreferDirectUpload = GetJsonBoolField(jsonObject, TEXT("prefer_direct_upload"), true);
	}

	TSharedRef<FJsonObject> MakeAssetsJsonObject(const FVdjmRecordAppStateAssetSection& assetSection)
	{
		TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("env_data_asset_path"), GetSoftPathString(assetSection.EnvDataAssetPath));
		jsonObject->SetStringField(TEXT("event_flow_data_asset_path"), GetSoftPathString(assetSection.EventFlowDataAssetPath));
		jsonObject->SetStringField(TEXT("loading_widget_class_path"), GetSoftPathString(assetSection.LoadingWidgetClassPath));
		jsonObject->SetStringField(TEXT("main_widget_class_path"), GetSoftPathString(assetSection.MainWidgetClassPath));
		return jsonObject;
	}

	void ReadAssetsJsonObject(const TSharedPtr<FJsonObject>& jsonObject, FVdjmRecordAppStateAssetSection& outAssetSection)
	{
		outAssetSection.EnvDataAssetPath = FSoftObjectPath(GetJsonStringField(jsonObject, TEXT("env_data_asset_path")));
		outAssetSection.EventFlowDataAssetPath = FSoftObjectPath(GetJsonStringField(jsonObject, TEXT("event_flow_data_asset_path")));
		outAssetSection.LoadingWidgetClassPath = FSoftObjectPath(GetJsonStringField(jsonObject, TEXT("loading_widget_class_path")));
		outAssetSection.MainWidgetClassPath = FSoftObjectPath(GetJsonStringField(jsonObject, TEXT("main_widget_class_path")));
	}

	TSharedRef<FJsonObject> MakeRecordTocJsonObject(const FVdjmRecordAppStateRecordTocEntry& recordTocEntry)
	{
		TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("record_id"), recordTocEntry.RecordId);
		jsonObject->SetStringField(TEXT("metadata_file_path"), recordTocEntry.MetadataFilePath);
		jsonObject->SetStringField(TEXT("output_file_path"), recordTocEntry.OutputFilePath);
		jsonObject->SetStringField(TEXT("playback_locator_type"), recordTocEntry.PlaybackLocatorType);
		jsonObject->SetStringField(TEXT("playback_locator"), recordTocEntry.PlaybackLocator);
		jsonObject->SetStringField(TEXT("published_content_uri"), recordTocEntry.PublishedContentUri);
		jsonObject->SetStringField(TEXT("remote_object_key"), recordTocEntry.RemoteObjectKey);
		jsonObject->SetStringField(TEXT("thumbnail_file_path"), recordTocEntry.ThumbnailFilePath);
		jsonObject->SetStringField(TEXT("preview_clip_file_path"), recordTocEntry.PreviewClipFilePath);
		jsonObject->SetStringField(TEXT("preview_clip_mime_type"), recordTocEntry.PreviewClipMimeType);
		jsonObject->SetStringField(TEXT("registry_status"), recordTocEntry.RegistryStatus);
		jsonObject->SetStringField(TEXT("local_file_status"), recordTocEntry.LocalFileStatus);
		jsonObject->SetStringField(TEXT("media_publish_status"), recordTocEntry.MediaPublishStatus);
		jsonObject->SetStringField(TEXT("remote_upload_status"), recordTocEntry.RemoteUploadStatus);
		jsonObject->SetStringField(TEXT("preview_status"), recordTocEntry.PreviewStatus);
		jsonObject->SetStringField(TEXT("last_error_reason"), recordTocEntry.LastErrorReason);
		jsonObject->SetNumberField(TEXT("created_unix_time"), recordTocEntry.CreatedUnixTime);
		jsonObject->SetNumberField(TEXT("last_seen_unix_time"), recordTocEntry.LastSeenUnixTime);
		jsonObject->SetNumberField(TEXT("file_size_bytes"), recordTocEntry.FileSizeBytes);
		jsonObject->SetNumberField(TEXT("width"), recordTocEntry.VideoWidth);
		jsonObject->SetNumberField(TEXT("height"), recordTocEntry.VideoHeight);
		jsonObject->SetBoolField(TEXT("output_file_exists"), recordTocEntry.bOutputFileExists);
		jsonObject->SetBoolField(TEXT("metadata_file_exists"), recordTocEntry.bMetadataFileExists);
		jsonObject->SetBoolField(TEXT("is_deleted"), recordTocEntry.bIsDeleted);
		return jsonObject;
	}

	bool ReadRecordTocJsonObject(
		const TSharedPtr<FJsonObject>& jsonObject,
		FVdjmRecordAppStateRecordTocEntry& outRecordTocEntry)
	{
		if (not jsonObject.IsValid())
		{
			return false;
		}

		outRecordTocEntry.RecordId = GetJsonStringField(jsonObject, TEXT("record_id"));
		outRecordTocEntry.MetadataFilePath = GetJsonStringField(jsonObject, TEXT("metadata_file_path"));
		outRecordTocEntry.OutputFilePath = GetJsonStringField(jsonObject, TEXT("output_file_path"));
		outRecordTocEntry.PlaybackLocatorType = GetJsonStringField(jsonObject, TEXT("playback_locator_type"));
		outRecordTocEntry.PlaybackLocator = GetJsonStringField(jsonObject, TEXT("playback_locator"));
		outRecordTocEntry.PublishedContentUri = GetJsonStringField(jsonObject, TEXT("published_content_uri"));
		outRecordTocEntry.RemoteObjectKey = GetJsonStringField(jsonObject, TEXT("remote_object_key"));
		outRecordTocEntry.ThumbnailFilePath = GetJsonStringField(jsonObject, TEXT("thumbnail_file_path"));
		outRecordTocEntry.PreviewClipFilePath = GetJsonStringField(jsonObject, TEXT("preview_clip_file_path"));
		outRecordTocEntry.PreviewClipMimeType = GetJsonStringField(jsonObject, TEXT("preview_clip_mime_type"), TEXT("video/mp4"));
		outRecordTocEntry.RegistryStatus = GetJsonStringField(jsonObject, TEXT("registry_status"));
		outRecordTocEntry.LocalFileStatus = GetJsonStringField(jsonObject, TEXT("local_file_status"));
		outRecordTocEntry.MediaPublishStatus = GetJsonStringField(jsonObject, TEXT("media_publish_status"));
		outRecordTocEntry.RemoteUploadStatus = GetJsonStringField(jsonObject, TEXT("remote_upload_status"), TEXT("not_started"));
		outRecordTocEntry.PreviewStatus = GetJsonStringField(jsonObject, TEXT("preview_status"));
		outRecordTocEntry.LastErrorReason = GetJsonStringField(jsonObject, TEXT("last_error_reason"));
		outRecordTocEntry.CreatedUnixTime = GetJsonInt64Field(jsonObject, TEXT("created_unix_time"));
		outRecordTocEntry.LastSeenUnixTime = GetJsonInt64Field(jsonObject, TEXT("last_seen_unix_time"));
		outRecordTocEntry.FileSizeBytes = GetJsonInt64Field(jsonObject, TEXT("file_size_bytes"), -1);
		outRecordTocEntry.VideoWidth = GetJsonInt32Field(jsonObject, TEXT("width"));
		outRecordTocEntry.VideoHeight = GetJsonInt32Field(jsonObject, TEXT("height"));
		outRecordTocEntry.bOutputFileExists = GetJsonBoolField(jsonObject, TEXT("output_file_exists"));
		outRecordTocEntry.bMetadataFileExists = GetJsonBoolField(jsonObject, TEXT("metadata_file_exists"));
		outRecordTocEntry.bIsDeleted = GetJsonBoolField(jsonObject, TEXT("is_deleted"));
		return not outRecordTocEntry.RecordId.IsEmpty() || not outRecordTocEntry.MetadataFilePath.IsEmpty();
	}

	TSharedRef<FJsonObject> MakeRuntimeJsonObject(const FVdjmRecordAppStateRuntimeSection& runtimeSection)
	{
		TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("last_world_name"), runtimeSection.LastWorldName);
		jsonObject->SetStringField(TEXT("last_selected_record_id"), runtimeSection.LastSelectedRecordId);
		jsonObject->SetStringField(TEXT("last_flow_session_name"), runtimeSection.LastFlowSessionName);
		jsonObject->SetStringField(TEXT("restore_state_json"), runtimeSection.RestoreStateJson);
		jsonObject->SetBoolField(TEXT("restore_last_selection"), runtimeSection.bRestoreLastSelection);
		return jsonObject;
	}

	void ReadRuntimeJsonObject(const TSharedPtr<FJsonObject>& jsonObject, FVdjmRecordAppStateRuntimeSection& outRuntimeSection)
	{
		outRuntimeSection.LastWorldName = GetJsonStringField(jsonObject, TEXT("last_world_name"));
		outRuntimeSection.LastSelectedRecordId = GetJsonStringField(jsonObject, TEXT("last_selected_record_id"));
		outRuntimeSection.LastFlowSessionName = GetJsonStringField(jsonObject, TEXT("last_flow_session_name"));
		outRuntimeSection.RestoreStateJson = GetJsonStringField(jsonObject, TEXT("restore_state_json"));
		outRuntimeSection.bRestoreLastSelection = GetJsonBoolField(jsonObject, TEXT("restore_last_selection"), true);
	}

	TSharedRef<FJsonObject> MakePreviewJsonObject(const FVdjmRecordAppStatePreviewSection& previewSection)
	{
		TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("selected_record_id"), previewSection.SelectedRecordId);
		jsonObject->SetNumberField(TEXT("carousel_center_index"), previewSection.CarouselCenterIndex);
		jsonObject->SetNumberField(TEXT("slot_window_size"), previewSection.SlotWindowSize);
		jsonObject->SetNumberField(TEXT("active_slot_index"), previewSection.ActiveSlotIndex);
		jsonObject->SetBoolField(TEXT("auto_play_center_preview"), previewSection.bAutoPlayCenterPreview);
		return jsonObject;
	}

	void ReadPreviewJsonObject(const TSharedPtr<FJsonObject>& jsonObject, FVdjmRecordAppStatePreviewSection& outPreviewSection)
	{
		outPreviewSection.SelectedRecordId = GetJsonStringField(jsonObject, TEXT("selected_record_id"));
		outPreviewSection.CarouselCenterIndex = GetJsonInt32Field(jsonObject, TEXT("carousel_center_index"));
		outPreviewSection.SlotWindowSize = FMath::Max(1, GetJsonInt32Field(jsonObject, TEXT("slot_window_size"), 5));
		outPreviewSection.ActiveSlotIndex = FMath::Max(0, GetJsonInt32Field(jsonObject, TEXT("active_slot_index"), 2));
		outPreviewSection.bAutoPlayCenterPreview = GetJsonBoolField(jsonObject, TEXT("auto_play_center_preview"), true);
	}

	TSharedRef<FJsonObject> MakeEventFlowJsonObject(const FVdjmRecordAppStateEventFlowSection& eventFlowSection)
	{
		TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("main_flow_asset_path"), eventFlowSection.MainFlowAssetPath);
		jsonObject->SetStringField(TEXT("last_started_session_name"), eventFlowSection.LastStartedSessionName);
		jsonObject->SetStringField(TEXT("last_subgraph_name"), eventFlowSection.LastSubgraphName);
		jsonObject->SetStringField(TEXT("external_flow_json_path"), eventFlowSection.ExternalFlowJsonPath);
		return jsonObject;
	}

	void ReadEventFlowJsonObject(const TSharedPtr<FJsonObject>& jsonObject, FVdjmRecordAppStateEventFlowSection& outEventFlowSection)
	{
		outEventFlowSection.MainFlowAssetPath = GetJsonStringField(jsonObject, TEXT("main_flow_asset_path"));
		outEventFlowSection.LastStartedSessionName = GetJsonStringField(jsonObject, TEXT("last_started_session_name"));
		outEventFlowSection.LastSubgraphName = GetJsonStringField(jsonObject, TEXT("last_subgraph_name"));
		outEventFlowSection.ExternalFlowJsonPath = GetJsonStringField(jsonObject, TEXT("external_flow_json_path"));
	}

	TSharedRef<FJsonObject> MakeControllerJsonObject(const FVdjmRecordAppStateControllerSection& controllerSection)
	{
		TSharedRef<FJsonObject> jsonObject = MakeShared<FJsonObject>();
		jsonObject->SetStringField(TEXT("last_option_request_json"), controllerSection.LastOptionRequestJson);
		jsonObject->SetStringField(TEXT("last_controller_state_json"), controllerSection.LastControllerStateJson);
		return jsonObject;
	}

	void ReadControllerJsonObject(const TSharedPtr<FJsonObject>& jsonObject, FVdjmRecordAppStateControllerSection& outControllerSection)
	{
		outControllerSection.LastOptionRequestJson = GetJsonStringField(jsonObject, TEXT("last_option_request_json"));
		outControllerSection.LastControllerStateJson = GetJsonStringField(jsonObject, TEXT("last_controller_state_json"));
	}

	FVdjmRecordAppStateRecordTocEntry MakeRecordTocEntryFromRegistryEntry(
		const FVdjmRecordMediaRegistryEntry& registryEntry,
		int64 lastSeenUnixTime)
	{
		FVdjmRecordAppStateRecordTocEntry tocEntry;
		tocEntry.RecordId = registryEntry.RecordId;
		tocEntry.MetadataFilePath = registryEntry.MetadataFilePath;
		tocEntry.OutputFilePath = registryEntry.OutputFilePath;
		tocEntry.PlaybackLocatorType = registryEntry.PlaybackLocatorType;
		tocEntry.PlaybackLocator = registryEntry.PlaybackLocator;
		tocEntry.PublishedContentUri = registryEntry.PublishedContentUri;
		tocEntry.ThumbnailFilePath = registryEntry.ThumbnailFilePath;
		tocEntry.PreviewClipFilePath = registryEntry.PreviewClipFilePath;
		tocEntry.PreviewClipMimeType = registryEntry.PreviewClipMimeType.IsEmpty()
			? TEXT("video/mp4")
			: registryEntry.PreviewClipMimeType;
		tocEntry.RegistryStatus = GetEnumValueString(registryEntry.RegistryStatus);
		tocEntry.LocalFileStatus = ResolveLocalFileStatus(registryEntry);
		tocEntry.MediaPublishStatus = GetEnumValueString(registryEntry.MediaPublishStatus);
		tocEntry.PreviewStatus = GetEnumValueString(registryEntry.PreviewStatus);
		tocEntry.LastErrorReason = registryEntry.LastErrorReason;
		tocEntry.CreatedUnixTime = registryEntry.CreatedUnixTime;
		tocEntry.LastSeenUnixTime = lastSeenUnixTime;
		tocEntry.FileSizeBytes = registryEntry.FileSizeBytes;
		tocEntry.VideoWidth = registryEntry.VideoWidth;
		tocEntry.VideoHeight = registryEntry.VideoHeight;
		tocEntry.bOutputFileExists = registryEntry.bOutputFileExists;
		tocEntry.bMetadataFileExists = registryEntry.bMetadataFileExists;
		tocEntry.bIsDeleted = registryEntry.bIsDeleted;
		return tocEntry;
	}
}

void UVdjmRecordAppStateManifest::Clear()
{
	mSchemaVersion = 1;
	mGeneratedUnixTime = 0;
	mAppStateFilePath.Reset();
	mUser = FVdjmRecordAppStateUserSection();
	mRemote = FVdjmRecordAppStateRemoteSection();
	mAssets = FVdjmRecordAppStateAssetSection();
	mRuntime = FVdjmRecordAppStateRuntimeSection();
	mPreview = FVdjmRecordAppStatePreviewSection();
	mEventFlow = FVdjmRecordAppStateEventFlowSection();
	mController = FVdjmRecordAppStateControllerSection();
	mRecordTocEntries.Reset();
}

FString UVdjmRecordAppStateManifest::ToString() const
{
	return FString::Printf(
		TEXT("AppState(schema=%d, records=%d, remote=%s, selected=%s, file=%s)"),
		mSchemaVersion,
		mRecordTocEntries.Num(),
		*mRemote.BaseUrl,
		*mPreview.SelectedRecordId,
		*mAppStateFilePath);
}

FString UVdjmRecordAppStateManifest::ToJsonString() const
{
	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetNumberField(TEXT("schema_version"), mSchemaVersion);
	rootObject->SetNumberField(TEXT("generated_unix_time"), mGeneratedUnixTime);
	rootObject->SetStringField(TEXT("app_state_file_path"), mAppStateFilePath);
	rootObject->SetObjectField(TEXT("user"), MakeUserJsonObject(mUser));
	rootObject->SetObjectField(TEXT("remote"), MakeRemoteJsonObject(mRemote));
	rootObject->SetObjectField(TEXT("assets"), MakeAssetsJsonObject(mAssets));
	rootObject->SetObjectField(TEXT("runtime"), MakeRuntimeJsonObject(mRuntime));
	rootObject->SetObjectField(TEXT("preview"), MakePreviewJsonObject(mPreview));
	rootObject->SetObjectField(TEXT("event_flow"), MakeEventFlowJsonObject(mEventFlow));
	rootObject->SetObjectField(TEXT("controller"), MakeControllerJsonObject(mController));

	TArray<TSharedPtr<FJsonValue>> tocValues;
	tocValues.Reserve(mRecordTocEntries.Num());
	for (const FVdjmRecordAppStateRecordTocEntry& recordTocEntry : mRecordTocEntries)
	{
		tocValues.Add(MakeShared<FJsonValueObject>(MakeRecordTocJsonObject(recordTocEntry)));
	}
	rootObject->SetArrayField(TEXT("records_toc"), MoveTemp(tocValues));

	FString appStateJsonString;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&appStateJsonString);
	return FJsonSerializer::Serialize(rootObject, writer)
		? appStateJsonString
		: FString();
}

bool UVdjmRecordAppStateManifest::LoadFromJsonString(
	const FString& appStateJsonString,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	if (appStateJsonString.TrimStartAndEnd().IsEmpty())
	{
		outErrorReason = TEXT("AppState json string is empty.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(appStateJsonString);
	if (not FJsonSerializer::Deserialize(reader, rootObject) || not rootObject.IsValid())
	{
		outErrorReason = TEXT("Failed to parse AppState json string.");
		return false;
	}

	Clear();
	mSchemaVersion = GetJsonInt32Field(rootObject, TEXT("schema_version"), 1);
	mGeneratedUnixTime = GetJsonInt64Field(rootObject, TEXT("generated_unix_time"));
	mAppStateFilePath = GetJsonStringField(rootObject, TEXT("app_state_file_path"));

	ReadUserJsonObject(GetJsonObjectField(rootObject, TEXT("user")), mUser);
	ReadRemoteJsonObject(GetJsonObjectField(rootObject, TEXT("remote")), mRemote);
	ReadAssetsJsonObject(GetJsonObjectField(rootObject, TEXT("assets")), mAssets);
	ReadRuntimeJsonObject(GetJsonObjectField(rootObject, TEXT("runtime")), mRuntime);
	ReadPreviewJsonObject(GetJsonObjectField(rootObject, TEXT("preview")), mPreview);
	ReadEventFlowJsonObject(GetJsonObjectField(rootObject, TEXT("event_flow")), mEventFlow);
	ReadControllerJsonObject(GetJsonObjectField(rootObject, TEXT("controller")), mController);

	const TArray<TSharedPtr<FJsonValue>>* tocValues = nullptr;
	if (rootObject->TryGetArrayField(TEXT("records_toc"), tocValues) && tocValues != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& tocValue : *tocValues)
		{
			FVdjmRecordAppStateRecordTocEntry tocEntry;
			if (tocValue.IsValid() && ReadRecordTocJsonObject(tocValue->AsObject(), tocEntry))
			{
				mRecordTocEntries.Add(tocEntry);
			}
		}
	}

	return true;
}

bool UVdjmRecordAppStateManifest::SaveToFile(
	const FString& appStateFilePath,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	FString normalizedFilePath = appStateFilePath;
	normalizedFilePath.TrimStartAndEndInline();
	FPaths::NormalizeFilename(normalizedFilePath);
	if (normalizedFilePath.IsEmpty())
	{
		outErrorReason = TEXT("AppState file path is empty.");
		return false;
	}

	const FString directoryPath = FPaths::GetPath(normalizedFilePath);
	if (directoryPath.IsEmpty())
	{
		outErrorReason = TEXT("AppState directory path is empty.");
		return false;
	}

	if (not IFileManager::Get().DirectoryExists(*directoryPath) &&
		not IFileManager::Get().MakeDirectory(*directoryPath, true))
	{
		outErrorReason = FString::Printf(TEXT("Failed to create AppState directory. Directory=%s"), *directoryPath);
		return false;
	}

	mGeneratedUnixTime = FDateTime::UtcNow().ToUnixTimestamp();
	mAppStateFilePath = normalizedFilePath;

	const FString appStateJsonString = ToJsonString();
	if (appStateJsonString.IsEmpty())
	{
		outErrorReason = TEXT("Failed to serialize AppState json.");
		return false;
	}

	if (not FFileHelper::SaveStringToFile(
		appStateJsonString,
		*normalizedFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outErrorReason = FString::Printf(TEXT("Failed to save AppState file. Path=%s"), *normalizedFilePath);
		return false;
	}

	return true;
}

bool UVdjmRecordAppStateManifest::LoadFromFile(
	const FString& appStateFilePath,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	FString normalizedFilePath = appStateFilePath;
	normalizedFilePath.TrimStartAndEndInline();
	FPaths::NormalizeFilename(normalizedFilePath);
	if (normalizedFilePath.IsEmpty())
	{
		outErrorReason = TEXT("AppState file path is empty.");
		return false;
	}

	if (not IFileManager::Get().FileExists(*normalizedFilePath))
	{
		outErrorReason = FString::Printf(TEXT("AppState file does not exist. Path=%s"), *normalizedFilePath);
		return false;
	}

	FString appStateJsonString;
	if (not FFileHelper::LoadFileToString(appStateJsonString, *normalizedFilePath))
	{
		outErrorReason = FString::Printf(TEXT("Failed to load AppState file. Path=%s"), *normalizedFilePath);
		return false;
	}

	if (not LoadFromJsonString(appStateJsonString, outErrorReason))
	{
		return false;
	}

	mAppStateFilePath = normalizedFilePath;
	return true;
}

void UVdjmRecordAppStateManifest::SetAppStateFilePath(const FString& appStateFilePath)
{
	mAppStateFilePath = appStateFilePath;
	FPaths::NormalizeFilename(mAppStateFilePath);
}

void UVdjmRecordAppStateManifest::SetRecordTocEntries(
	const TArray<FVdjmRecordAppStateRecordTocEntry>& recordTocEntries)
{
	mRecordTocEntries = recordTocEntries;
}

UVdjmRecordAppStateStore* UVdjmRecordAppStateStore::FindAppStateStore(UObject* worldContextObject)
{
	const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject);
	if (worldContextSubsystem == nullptr)
	{
		return nullptr;
	}

	return Cast<UVdjmRecordAppStateStore>(
		worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetAppStateStoreContextKey()));
}

UVdjmRecordAppStateStore* UVdjmRecordAppStateStore::FindOrCreateAppStateStore(UObject* worldContextObject)
{
	if (UVdjmRecordAppStateStore* existingStore = FindAppStateStore(worldContextObject))
	{
		return existingStore;
	}

	UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject);
	UObject* storeOuter = worldContextSubsystem != nullptr ? Cast<UObject>(worldContextSubsystem) : worldContextObject;
	if (storeOuter == nullptr)
	{
		return nullptr;
	}

	UVdjmRecordAppStateStore* newStore = NewObject<UVdjmRecordAppStateStore>(storeOuter);
	if (not IsValid(newStore))
	{
		return nullptr;
	}

	if (not newStore->InitializeStore(worldContextObject))
	{
		return nullptr;
	}

	return newStore;
}

bool UVdjmRecordAppStateStore::InitializeStore(UObject* worldContextObject)
{
	if (worldContextObject == nullptr)
	{
		mLastErrorReason = TEXT("World context object is null.");
		return false;
	}

	UWorld* world = worldContextObject->GetWorld();
	if (world == nullptr)
	{
		mLastErrorReason = TEXT("World is not available.");
		return false;
	}

	mCachedWorld = world;
	mAppStateFilePath = ResolveDefaultAppStateFilePath();
	if (mAppStateFilePath.IsEmpty())
	{
		mLastErrorReason = TEXT("Failed to resolve AppState file path.");
		return false;
	}

	UVdjmRecordAppStateManifest* appStateManifest = EnsureManifest();
	if (appStateManifest == nullptr)
	{
		mLastErrorReason = TEXT("Failed to create AppState manifest.");
		return false;
	}
	appStateManifest->SetAppStateFilePath(mAppStateFilePath);

	return RegisterStoreContext(worldContextObject);
}

void UVdjmRecordAppStateStore::Clear()
{
	if (UVdjmRecordAppStateManifest* appStateManifest = EnsureManifest())
	{
		appStateManifest->Clear();
		appStateManifest->SetAppStateFilePath(mAppStateFilePath);
	}

	mLastErrorReason.Reset();
}

bool UVdjmRecordAppStateStore::LoadAppState(FString& outErrorReason, bool bCreateIfMissing)
{
	outErrorReason.Reset();
	mLastErrorReason.Reset();

	UVdjmRecordAppStateManifest* appStateManifest = EnsureManifest();
	if (appStateManifest == nullptr)
	{
		outErrorReason = TEXT("AppState manifest is not available.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	if (mAppStateFilePath.IsEmpty())
	{
		mAppStateFilePath = ResolveDefaultAppStateFilePath();
	}

	if (mAppStateFilePath.IsEmpty())
	{
		outErrorReason = TEXT("AppState file path is empty.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	if (not IFileManager::Get().FileExists(*mAppStateFilePath))
	{
		if (not bCreateIfMissing)
		{
			outErrorReason = FString::Printf(TEXT("AppState file does not exist. Path=%s"), *mAppStateFilePath);
			mLastErrorReason = outErrorReason;
			return false;
		}

		appStateManifest->Clear();
		appStateManifest->SetAppStateFilePath(mAppStateFilePath);
		return SaveAppState(outErrorReason);
	}

	if (not appStateManifest->LoadFromFile(mAppStateFilePath, outErrorReason))
	{
		mLastErrorReason = outErrorReason;
		return false;
	}

	return true;
}

bool UVdjmRecordAppStateStore::SaveAppState(FString& outErrorReason)
{
	outErrorReason.Reset();
	mLastErrorReason.Reset();

	UVdjmRecordAppStateManifest* appStateManifest = EnsureManifest();
	if (appStateManifest == nullptr)
	{
		outErrorReason = TEXT("AppState manifest is not available.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	if (mAppStateFilePath.IsEmpty())
	{
		mAppStateFilePath = ResolveDefaultAppStateFilePath();
	}

	if (not appStateManifest->SaveToFile(mAppStateFilePath, outErrorReason))
	{
		mLastErrorReason = outErrorReason;
		return false;
	}

	return true;
}

bool UVdjmRecordAppStateStore::RefreshRecordsTocFromMetadataStore(
	UVdjmRecordMetadataStore* metadataStore,
	FString& outErrorReason)
{
	outErrorReason.Reset();
	mLastErrorReason.Reset();

	if (metadataStore == nullptr)
	{
		outErrorReason = TEXT("Metadata store is not available.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	UVdjmRecordAppStateManifest* appStateManifest = EnsureManifest();
	if (appStateManifest == nullptr)
	{
		outErrorReason = TEXT("AppState manifest is not available.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	TArray<FVdjmRecordMediaRegistryEntry> registryEntries = metadataStore->GetMediaRegistryEntries();
	registryEntries.Sort([](const FVdjmRecordMediaRegistryEntry& leftEntry, const FVdjmRecordMediaRegistryEntry& rightEntry)
	{
		return leftEntry.CreatedUnixTime > rightEntry.CreatedUnixTime;
	});

	const int64 lastSeenUnixTime = FDateTime::UtcNow().ToUnixTimestamp();
	TArray<FVdjmRecordAppStateRecordTocEntry> tocEntries;
	tocEntries.Reserve(registryEntries.Num());
	for (const FVdjmRecordMediaRegistryEntry& registryEntry : registryEntries)
	{
		tocEntries.Add(MakeRecordTocEntryFromRegistryEntry(registryEntry, lastSeenUnixTime));
	}

	appStateManifest->SetRecordTocEntries(tocEntries);
	return true;
}

FString UVdjmRecordAppStateStore::ToString() const
{
	return mAppStateManifest != nullptr
		? mAppStateManifest->ToString()
		: FString::Printf(TEXT("AppStateStore(manifest=None, file=%s)"), *mAppStateFilePath);
}

UVdjmRecordAppStateManifest* UVdjmRecordAppStateStore::EnsureManifest()
{
	if (mAppStateManifest == nullptr)
	{
		mAppStateManifest = NewObject<UVdjmRecordAppStateManifest>(this);
	}

	return mAppStateManifest;
}

FString UVdjmRecordAppStateStore::ResolveDefaultAppStateFilePath() const
{
	const EVdjmRecordEnvPlatform targetPlatform = VdjmRecordUtils::Platforms::GetTargetPlatform();
	FString appStateDirectoryPath = VdjmRecordUtils::FilePaths::GetPlatformRecordBaseDir(targetPlatform);
	FPaths::NormalizeFilename(appStateDirectoryPath);
	while (appStateDirectoryPath.EndsWith(TEXT("/")))
	{
		appStateDirectoryPath.LeftChopInline(1, EAllowShrinking::No);
	}

	if (appStateDirectoryPath.IsEmpty())
	{
		return FString();
	}

	return FPaths::Combine(appStateDirectoryPath, TEXT("VdjmRecordAppState.json"));
}

bool UVdjmRecordAppStateStore::RegisterStoreContext(UObject* worldContextObject)
{
	UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject);
	if (worldContextSubsystem == nullptr)
	{
		mLastErrorReason = TEXT("World context subsystem is not available.");
		return false;
	}

	return worldContextSubsystem->RegisterStrongObjectContext(
		UVdjmRecorderWorldContextSubsystem::GetAppStateStoreContextKey(),
		this,
		StaticClass());
}
