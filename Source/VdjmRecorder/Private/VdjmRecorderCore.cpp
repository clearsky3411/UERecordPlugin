// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmRecorderCore.h"
#include "VdjmRecordBridgeActor.h"
#include "VdjmAndroid/VdjmAndroidMediaStore.h"
#include "VdjmRecorderController.h"
#include "VdjmRecorderWorldContextSubsystem.h"
// Fill out your copyright notice in the Description page of Project Settings.
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "Kismet/GameplayStatics.h"
#include "MediaPlayer.h"
#include "VdjmRecordTypes.h"
#include "HAL/FileManager.h"
#include "VdjmRecoderEncoderImpl.h"
#include "DSP/AudioDebuggingUtilities.h"
#include "Slate/SceneViewport.h"

namespace
{
	struct FVdjmRecordMediaPublishJobPayload
	{
		int32 JobId = INDEX_NONE;
		FString SourceFilePath;
		FString DisplayName;
		FString RelativePath;
	};

	FString NormalizeRecordOutputPathForRuntime(
		const FString& outputFilePath,
		EVdjmRecordEnvPlatform targetPlatform)
	{
		FString normalizedPath = outputFilePath;
		normalizedPath.TrimStartAndEndInline();
		FPaths::NormalizeFilename(normalizedPath);
		VdjmRecordUtils::FilePaths::StripEmbeddedWindowsAbsolutePathInline(normalizedPath);

		if (normalizedPath.IsEmpty())
		{
			return FString();
		}

		FString baseDir = VdjmRecordUtils::FilePaths::GetPlatformRecordBaseDir(targetPlatform);
		FPaths::NormalizeFilename(baseDir);
		while (baseDir.EndsWith(TEXT("/")))
		{
			baseDir.LeftChopInline(1, EAllowShrinking::No);
		}

		normalizedPath = VdjmRecordUtils::FilePaths::NormalizeCandidateOutputPath(normalizedPath, baseDir);
		if (FPaths::IsRelative(normalizedPath))
		{
			normalizedPath = FPaths::Combine(baseDir, normalizedPath);
			FPaths::NormalizeFilename(normalizedPath);
			VdjmRecordUtils::FilePaths::StripEmbeddedWindowsAbsolutePathInline(normalizedPath);
			normalizedPath = VdjmRecordUtils::FilePaths::NormalizeCandidateOutputPath(normalizedPath, baseDir);
		}

		return normalizedPath;
	}

	template <typename EnumType>
	EnumType ParseEnumValueString(const FString& valueString, EnumType fallbackValue)
	{
		const UEnum* enumType = StaticEnum<EnumType>();
		if (enumType == nullptr || valueString.TrimStartAndEnd().IsEmpty())
		{
			return fallbackValue;
		}

		int64 enumValue = enumType->GetValueByNameString(valueString);
		if (enumValue == INDEX_NONE)
		{
			FString shortValueString = valueString;
			const int32 scopeIndex = valueString.Find(TEXT("::"));
			if (scopeIndex != INDEX_NONE)
			{
				shortValueString = valueString.RightChop(scopeIndex + 2);
			}
			enumValue = enumType->GetValueByNameString(shortValueString);
		}

		return enumValue == INDEX_NONE
			? fallbackValue
			: static_cast<EnumType>(enumValue);
	}

	FString GetJsonObjectStringField(
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

	int64 GetJsonObjectInt64Field(
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

	int32 GetJsonObjectInt32Field(
		const TSharedPtr<FJsonObject>& jsonObject,
		const FString& fieldName,
		int32 fallbackValue = 0)
	{
		return static_cast<int32>(GetJsonObjectInt64Field(jsonObject, fieldName, fallbackValue));
	}

	double GetJsonObjectDoubleField(
		const TSharedPtr<FJsonObject>& jsonObject,
		const FString& fieldName,
		double fallbackValue = 0.0)
	{
		if (not jsonObject.IsValid())
		{
			return fallbackValue;
		}

		double valueNumber = 0.0;
		return jsonObject->TryGetNumberField(fieldName, valueNumber)
			? valueNumber
			: fallbackValue;
	}

	bool GetJsonObjectBoolField(
		const TSharedPtr<FJsonObject>& jsonObject,
		const FString& fieldName,
		bool fallbackValue = false)
	{
		if (not jsonObject.IsValid())
		{
			return fallbackValue;
		}

		bool bValue = false;
		return jsonObject->TryGetBoolField(fieldName, bValue)
			? bValue
			: fallbackValue;
	}

	void SetRegistryEntryFileState(FVdjmRecordMediaRegistryEntry& entry)
	{
		const FString metadataFilePath = FPaths::ConvertRelativePathToFull(entry.MetadataFilePath);
		const FString outputFilePath = FPaths::ConvertRelativePathToFull(entry.OutputFilePath);
		entry.bMetadataFileExists = not entry.MetadataFilePath.IsEmpty() &&
			IFileManager::Get().FileExists(*metadataFilePath);
		entry.bOutputFileExists = not entry.OutputFilePath.IsEmpty() &&
			IFileManager::Get().FileExists(*outputFilePath);

		if (entry.bIsDeleted)
		{
			entry.RegistryStatus = EVdjmRecordMediaRegistryEntryStatus::EDeleted;
		}
		else if (not entry.bMetadataFileExists)
		{
			entry.RegistryStatus = EVdjmRecordMediaRegistryEntryStatus::EMissingMetadata;
		}
		else if (not entry.bOutputFileExists && entry.PublishedContentUri.IsEmpty())
		{
			entry.RegistryStatus = EVdjmRecordMediaRegistryEntryStatus::EMissingMedia;
		}
		else
		{
			entry.RegistryStatus = EVdjmRecordMediaRegistryEntryStatus::EAvailable;
		}
	}

	TSharedRef<FJsonObject> MakeRegistryEntryJsonObject(const FVdjmRecordMediaRegistryEntry& entry)
	{
		TSharedRef<FJsonObject> entryObject = MakeShared<FJsonObject>();
		entryObject->SetStringField(TEXT("record_id"), entry.RecordId);
		entryObject->SetStringField(TEXT("output_file_path"), entry.OutputFilePath);
		entryObject->SetStringField(TEXT("metadata_file_path"), entry.MetadataFilePath);
		entryObject->SetStringField(TEXT("playback_locator_type"), entry.PlaybackLocatorType);
		entryObject->SetStringField(TEXT("playback_locator"), entry.PlaybackLocator);
		entryObject->SetStringField(TEXT("thumbnail_file_path"), entry.ThumbnailFilePath);
		entryObject->SetStringField(TEXT("preview_clip_file_path"), entry.PreviewClipFilePath);
		entryObject->SetStringField(TEXT("preview_clip_mime_type"), entry.PreviewClipMimeType);
		entryObject->SetStringField(TEXT("preview_error_reason"), entry.PreviewErrorReason);
		entryObject->SetStringField(TEXT("published_content_uri"), entry.PublishedContentUri);
		entryObject->SetStringField(TEXT("published_display_name"), entry.PublishedDisplayName);
		entryObject->SetStringField(TEXT("published_relative_path"), entry.PublishedRelativePath);
		entryObject->SetStringField(TEXT("video_mime_type"), entry.VideoMimeType);
		entryObject->SetStringField(TEXT("last_error_reason"), entry.LastErrorReason);
		entryObject->SetNumberField(TEXT("created_unix_time"), entry.CreatedUnixTime);
		entryObject->SetNumberField(TEXT("file_size_bytes"), entry.FileSizeBytes);
		entryObject->SetNumberField(TEXT("recorded_frame_count"), entry.RecordedFrameCount);
		entryObject->SetNumberField(TEXT("width"), entry.VideoWidth);
		entryObject->SetNumberField(TEXT("height"), entry.VideoHeight);
		entryObject->SetNumberField(TEXT("fps"), entry.VideoFrameRate);
		entryObject->SetNumberField(TEXT("bitrate"), entry.VideoBitrate);
		entryObject->SetNumberField(TEXT("preview_start_time_sec"), entry.PreviewStartTimeSec);
		entryObject->SetNumberField(TEXT("preview_duration_sec"), entry.PreviewDurationSec);
		entryObject->SetStringField(TEXT("platform"), StaticEnum<EVdjmRecordEnvPlatform>()->GetValueAsString(entry.TargetPlatform));
		entryObject->SetStringField(TEXT("media_publish_status"), StaticEnum<EVdjmRecordMediaPublishStatus>()->GetValueAsString(entry.MediaPublishStatus));
		entryObject->SetStringField(TEXT("registry_status"), StaticEnum<EVdjmRecordMediaRegistryEntryStatus>()->GetValueAsString(entry.RegistryStatus));
		entryObject->SetStringField(TEXT("preview_status"), StaticEnum<EVdjmRecordMediaPreviewStatus>()->GetValueAsString(entry.PreviewStatus));
		entryObject->SetBoolField(TEXT("output_file_exists"), entry.bOutputFileExists);
		entryObject->SetBoolField(TEXT("metadata_file_exists"), entry.bMetadataFileExists);
		entryObject->SetBoolField(TEXT("is_deleted"), entry.bIsDeleted);
		return entryObject;
	}

	bool ReadRegistryEntryJsonObject(
		const TSharedPtr<FJsonObject>& entryObject,
		FVdjmRecordMediaRegistryEntry& outEntry)
	{
		if (not entryObject.IsValid())
		{
			return false;
		}

		outEntry.RecordId = GetJsonObjectStringField(entryObject, TEXT("record_id"));
		outEntry.OutputFilePath = GetJsonObjectStringField(entryObject, TEXT("output_file_path"));
		outEntry.MetadataFilePath = GetJsonObjectStringField(entryObject, TEXT("metadata_file_path"));
		outEntry.PlaybackLocatorType = GetJsonObjectStringField(entryObject, TEXT("playback_locator_type"));
		outEntry.PlaybackLocator = GetJsonObjectStringField(entryObject, TEXT("playback_locator"));
		outEntry.ThumbnailFilePath = GetJsonObjectStringField(entryObject, TEXT("thumbnail_file_path"));
		outEntry.PreviewClipFilePath = GetJsonObjectStringField(entryObject, TEXT("preview_clip_file_path"));
		outEntry.PreviewClipMimeType = GetJsonObjectStringField(entryObject, TEXT("preview_clip_mime_type"), TEXT("video/mp4"));
		outEntry.PreviewErrorReason = GetJsonObjectStringField(entryObject, TEXT("preview_error_reason"));
		outEntry.PublishedContentUri = GetJsonObjectStringField(entryObject, TEXT("published_content_uri"));
		outEntry.PublishedDisplayName = GetJsonObjectStringField(entryObject, TEXT("published_display_name"));
		outEntry.PublishedRelativePath = GetJsonObjectStringField(entryObject, TEXT("published_relative_path"));
		outEntry.VideoMimeType = GetJsonObjectStringField(entryObject, TEXT("video_mime_type"));
		outEntry.LastErrorReason = GetJsonObjectStringField(entryObject, TEXT("last_error_reason"));
		outEntry.CreatedUnixTime = GetJsonObjectInt64Field(entryObject, TEXT("created_unix_time"));
		outEntry.FileSizeBytes = GetJsonObjectInt64Field(entryObject, TEXT("file_size_bytes"), -1);
		outEntry.RecordedFrameCount = GetJsonObjectInt32Field(entryObject, TEXT("recorded_frame_count"));
		outEntry.VideoWidth = GetJsonObjectInt32Field(entryObject, TEXT("width"));
		outEntry.VideoHeight = GetJsonObjectInt32Field(entryObject, TEXT("height"));
		outEntry.VideoFrameRate = GetJsonObjectInt32Field(entryObject, TEXT("fps"));
		outEntry.VideoBitrate = GetJsonObjectInt32Field(entryObject, TEXT("bitrate"));
		outEntry.PreviewStartTimeSec = GetJsonObjectDoubleField(entryObject, TEXT("preview_start_time_sec"));
		outEntry.PreviewDurationSec = GetJsonObjectDoubleField(entryObject, TEXT("preview_duration_sec"), 3.0);
		outEntry.TargetPlatform = ParseEnumValueString(
			GetJsonObjectStringField(entryObject, TEXT("platform")),
			EVdjmRecordEnvPlatform::EDefault);
		outEntry.MediaPublishStatus = ParseEnumValueString(
			GetJsonObjectStringField(entryObject, TEXT("media_publish_status")),
			EVdjmRecordMediaPublishStatus::ENotStarted);
		outEntry.RegistryStatus = ParseEnumValueString(
			GetJsonObjectStringField(entryObject, TEXT("registry_status")),
			EVdjmRecordMediaRegistryEntryStatus::EUnknown);
		outEntry.PreviewStatus = ParseEnumValueString(
			GetJsonObjectStringField(entryObject, TEXT("preview_status")),
			EVdjmRecordMediaPreviewStatus::ENotReady);
		outEntry.bOutputFileExists = GetJsonObjectBoolField(entryObject, TEXT("output_file_exists"));
		outEntry.bMetadataFileExists = GetJsonObjectBoolField(entryObject, TEXT("metadata_file_exists"));
		outEntry.bIsDeleted = GetJsonObjectBoolField(entryObject, TEXT("is_deleted"));
		return not outEntry.RecordId.IsEmpty() || not outEntry.MetadataFilePath.IsEmpty();
	}

	bool IsUrlLikeMediaSource(const FString& source)
	{
		return source.StartsWith(TEXT("content://"), ESearchCase::IgnoreCase) ||
			source.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) ||
			source.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase) ||
			source.StartsWith(TEXT("file://"), ESearchCase::IgnoreCase);
	}

	FString ResolvePreviewSourceFromManifest(const UVdjmRecordMediaManifest* mediaManifest, FString& outSourceType)
	{
		outSourceType.Reset();
		if (not IsValid(mediaManifest))
		{
			return FString();
		}

		if (not mediaManifest->GetPreviewClipFilePath().IsEmpty())
		{
			outSourceType = TEXT("local_path");
			return mediaManifest->GetPreviewClipFilePath();
		}

		if (not mediaManifest->GetOutputFilePath().IsEmpty())
		{
			outSourceType = TEXT("local_path");
			return mediaManifest->GetOutputFilePath();
		}

		if (not mediaManifest->GetPlaybackLocator().IsEmpty())
		{
			outSourceType = mediaManifest->GetPlaybackLocatorType();
			return mediaManifest->GetPlaybackLocator();
		}

		if (not mediaManifest->GetPublishedContentUri().IsEmpty())
		{
			outSourceType = TEXT("content_uri");
			return mediaManifest->GetPublishedContentUri();
		}

		return FString();
	}

	FString ResolvePreviewSourceFromRegistryEntry(const FVdjmRecordMediaRegistryEntry& registryEntry, FString& outSourceType)
	{
		outSourceType.Reset();
		if (not registryEntry.PreviewClipFilePath.IsEmpty())
		{
			outSourceType = TEXT("local_path");
			return registryEntry.PreviewClipFilePath;
		}

		if (not registryEntry.OutputFilePath.IsEmpty())
		{
			outSourceType = TEXT("local_path");
			return registryEntry.OutputFilePath;
		}

		if (not registryEntry.PlaybackLocator.IsEmpty())
		{
			outSourceType = registryEntry.PlaybackLocatorType;
			return registryEntry.PlaybackLocator;
		}

		if (not registryEntry.PublishedContentUri.IsEmpty())
		{
			outSourceType = TEXT("content_uri");
			return registryEntry.PublishedContentUri;
		}

		return FString();
	}
}


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓	UVdjmRecordEncoderReadBackUnit : UVdjmRecordEncoderUnit	↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::FVdjmReadBackTextureWrapper()
{
	ReadBackBuffer = nullptr;
	bHasRequest = false;
	TimeStamp = 0.0;
}

void FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::MakeRHIGPUReadback()
{
	if (ReadBackBuffer != nullptr)
	{
		ReadBackBuffer.Reset();
		ReadBackBuffer = nullptr;
	}
	
	ReadBackBuffer = MakeUnique<FRHIGPUTextureReadback>(FName("VdjmRB_NV12"));
	bHasRequest = false;
	TimeStamp = 0.0;
}

void FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::EnqueueCopy(FRHICommandList& RHICmdList, FTextureRHIRef srcTexture, double inTimeStamp)
{
	if (IsValidTexture())
	{
		ReadBackBuffer->EnqueueCopy(RHICmdList, srcTexture);
		TimeStamp = inTimeStamp;
		bHasRequest = true;
	}
}

void* FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::TextureLock(int32& outWidth, int32& outHeight) 
{
	if (IsValidTexture())
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::TextureLock - Locking texture with timestamp %f"),TimeStamp);
		return ReadBackBuffer->Lock(outWidth,&outHeight);;
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore,Warning,TEXT("FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::TextureLock - Invalid texture, cannot lock"));
		return nullptr;
	}
}

void FVdjmReadBackHelper::FVdjmReadBackTextureWrapper::TextureUnLock() 
{
	if (IsValidTexture())
	{
		ReadBackBuffer->Unlock();
		bHasRequest = false;
	}
}

FVdjmReadBackHelper::FVdjmReadBackHelper()
{
	mCurrentWriteIndex = 0;
	mCurrentReadIndex = 0;
}

void FVdjmReadBackHelper::EnqueueFrame(FRHICommandList& RHICmdList, FTextureRHIRef SourceTexture, double TimeStamp)
{
	UE_LOG(LogVdjmRecorderCore,Log,TEXT("FVdjmReadBackHelper::EnqueueFrame - SourceTexture : %p Enqueuing frame with timestamp %f at index %d"),SourceTexture.GetReference(),TimeStamp,mCurrentWriteIndex);
	mReadBackWrappers[mCurrentWriteIndex].EnqueueCopy(
		RHICmdList,
		SourceTexture,
		TimeStamp);
	mCurrentWriteIndex = (mCurrentWriteIndex + 1) % ReadBackBufferCount;
}

void FVdjmRecordUnitParamContext::DbcSetupContextExtended(UWorld* world, UVdjmRecordEnvResolver* resolver,
	FRDGBuilder* graphBuilder, double currentRecordTimeSec)
{
	WorldContext = world;
	RecordEnvResolver = resolver;
	RecordBridge = resolver->LinkedOwnerBridge;
	RecordResource = resolver->LinkedRecordResource;
	GraphBuilder = graphBuilder;
	CurrentRecordTimeSec = currentRecordTimeSec;
}

void FVdjmReadBackHelper::Initialize()
{
	for (int i = 0; i < ReadBackBufferCount; i++)
	{
		mReadBackWrappers[i].MakeRHIGPUReadback();
	}
}

void* FVdjmReadBackHelper::TryLockOldest(int32& outWidth, int32& outHeight,double& outTimeStamp)
{
	FVdjmReadBackTextureWrapper& target = mReadBackWrappers[mCurrentReadIndex];
	
	if (target.IsReadReady())
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("FVdjmReadBackHelper::TryLockOldest -{{{ Readback ready at index %d with timestamp %f"),mCurrentReadIndex,target.TimeStamp);
		outTimeStamp = target.TimeStamp;
		return target.TextureLock(outWidth, outHeight);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("FVdjmReadBackHelper::TryLockOldest - No readback ready at index %d"),mCurrentReadIndex);
		return nullptr;
	}
}

void FVdjmReadBackHelper::UnlockOldest() 
{
	mReadBackWrappers[mCurrentReadIndex].TextureUnLock();
	mCurrentReadIndex = (mCurrentReadIndex + 1) % ReadBackBufferCount;
}

void FVdjmReadBackHelper::StopAllReadBacks()
{
	for (int i = 0; i < ReadBackBufferCount; i++)
	{
		mReadBackWrappers[i].TextureUnLock();
	}
}


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
class UVdjmRecordUnitPipeline : public UObject 		
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

bool UVdjmRecordUnitPipeline::InitializeRecordPipeline(UVdjmRecordResource* recordResource)
{
	if (recordResource == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::InitializeRecordPipeline - recordResource is null."));
		return false;
	}
	LinkedRecordResource = recordResource;
	LinkedBridgeActor = recordResource->LinkedOwnerBridge;
	if (LinkedBridgeActor.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitPipeline::InitializeRecordPipeline - Successfully linked Bridge Actor : %s"),*LinkedBridgeActor->GetName());
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::InitializeRecordPipeline - Failed to link Bridge Actor."));
		LinkedRecordResource = nullptr;
		return false;
	}
	return true;
}

bool UVdjmRecordUnitPipeline::DbcUnitCheck() const
{
	bool result = RecordUnits.Num() > 0;
	if (not result)
	{
		UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcUnitCheck - RecordUnits.Num() == 0"));
		return false;
	}
	for (auto recordUnit:RecordUnits)
	{
		if (not recordUnit->DbcIsValidUnitInit())
		{
			UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcUnitCheck - recordUnit %s DbcIsValidUnitInit() == false"),*recordUnit->GetName());
			result = false;
			break;
		}
	}
	return result;
}

void UVdjmRecordUnitPipeline::TravelLoopUnits(TFunctionRef<int32(UVdjmRecordUnit* unit)> travelFunc) const
{
	for (TObjectPtr<UVdjmRecordUnit> Unit : RecordUnits)
	{
		if (IsValid(Unit))
		{
			if (travelFunc(Unit) < 0)
			{
				return;
			}
		}
	}
}

void UVdjmRecordUnitPipeline::ReleaseRecordPipeline()
{
	for (TObjectPtr<UVdjmRecordUnit>& Unit : RecordUnits)
	{
		if (IsValid(Unit))
		{
			// 각 유닛별 정리 함수 호출 (가상 함수)
			Unit->ReleaseUnit();
		}
	}
}

bool UVdjmRecordUnitPipeline::DbcIsValidPipelineInit() const
{
	if (DbcUnitCheck()&& LinkedRecordResource.IsValid()	&& LinkedBridgeActor.IsValid())
	{
		return true;
	}
	else
	{
		if(not DbcUnitCheck())
		{
			UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcIsValidUnitInit - DbcUnitCheck() == false"));
		}
		if (not LinkedRecordResource.IsValid())
		{
			UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcIsValidUnitInit - not LinkedRecordResource.IsValid()"));
		}
		if (not LinkedBridgeActor.IsValid())
		{
			UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::DbcIsValidUnitInit - not LinkedBridgeActor.IsValid()"));
		}
		
		return false;
	}
	// return DbcUnitCheck()
	// 	&& LinkedRecordResource.IsValid()
	// 	&& LinkedBridgeActor.IsValid();
}

bool UVdjmRecordUnit::InitializeUnit(UVdjmRecordResource* recordResource)
{
	LinkedRecordResource = recordResource;
	return LinkedRecordResource.IsValid();
}

UVdjmRecordUnit* UVdjmRecordUnitPipeline::CreateUnit(TSubclassOf<UVdjmRecordUnit> unitCls)
{
	if (unitCls != nullptr && LinkedRecordResource.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Creating Record Unit of class : %s"),*unitCls->GetName());
		UVdjmRecordUnit* newUnit = NewObject<UVdjmRecordUnit>(this,unitCls);
		if (IsValid(newUnit))
		{
			newUnit->LinkedPipeline = this;
			RecordUnits.Add(newUnit);
			if (newUnit->InitializeUnit(LinkedRecordResource.Get()))
			{
				UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Successfully initialized Record Unit : %s"),*newUnit->GetName());
			}
			else
			{
				 UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Failed to initialize Record Unit : %s"),*newUnit->GetName());
			}
			if (newUnit->DbcIsValidUnitInit())
			{
				UE_LOG(LogVdjmRecorderCore,Log,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Record Unit passed DbcIsValidUnitInit check : %s"),*newUnit->GetName());
			}
			else
			{
				UE_LOG(LogVdjmRecorderCore,Warning,TEXT("UVdjmRecordUnitPipeline::CreateUnit - Record Unit failed DbcIsValidUnitInit check : %s"),*newUnit->GetName());
				//	유닛 초기화 실패시 일단 파이프라인에서 제거. (생성은 했지만 초기화 실패한 유닛이 파이프라인에 남아있는게 더 위험하다고 판단)
				RecordUnits.Remove(newUnit);
				return nullptr;
			}
			return newUnit;
		}
	}
	return nullptr;
}


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	class UVdjmRecordDescriptor : public UObject
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
void UVdjmRecordDescriptor::CopyForSnapshot(const UVdjmRecordDescriptor*& sourceData)
{
	bUseWindowResolution = sourceData->bUseWindowResolution;
	RecordResolution = sourceData->RecordResolution;
	FrameRate = sourceData->FrameRate;
	//BitrateMap = sourceData.BitrateMap;
	SelectedBitrateType = sourceData->SelectedBitrateType;
	FilePrefix = sourceData->FilePrefix;
	SavePathDirectoryType = sourceData->SavePathDirectoryType;
	CustomSaveDirectory = sourceData->CustomSaveDirectory;
	CustomFileSaverClass = sourceData->CustomFileSaverClass;
	RenderTargetFormat = sourceData->RenderTargetFormat;
}

FIntPoint UVdjmRecordDescriptor::GetRecordResolution() const
{
	return bUseWindowResolution ?
		       FIntPoint(
			       GEngine->GameViewport->Viewport->GetSizeXY().X,
			       GEngine->GameViewport->Viewport->GetSizeXY().Y) :
		       RecordResolution;
}

int32 UVdjmRecordDescriptor::GetRecordFrameRate() const
{
	return FrameRate;
}

int32 UVdjmRecordDescriptor::GetRecordBitrate()
{
	int32* bitratePtr = BitrateMap.Find(SelectedBitrateType);
	if (bitratePtr)
	{
		return *bitratePtr;
	}
	return 2000000; //	default 2Mbps
}

FString UVdjmRecordDescriptor::GetRecordFilePrefix()
{
	return FilePrefix;
}

FString UVdjmRecordDescriptor::GetRecordFilePath()
{
	FString baseDir;
	switch (SavePathDirectoryType)
	{
	case EVdjmRecordSavePathDirectoryType::EProjectDir:
		baseDir = FPaths::ProjectDir();
		break;
	case EVdjmRecordSavePathDirectoryType::EDocumentsDir:
		baseDir = FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/Documents/");
		break;
	case EVdjmRecordSavePathDirectoryType::EVideoDir:
		baseDir = FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/Videos/");
		break;
	case EVdjmRecordSavePathDirectoryType::ECustomDir:
		baseDir = CustomSaveDirectory;
		break;
	case EVdjmRecordSavePathDirectoryType::ECustomSaverClass:
		//	Custom Saver Class will handle path
		baseDir = TEXT("");
		break;
	default:
		baseDir = FPaths::ProjectDir();
		break;
	}
	return baseDir;
}

EPixelFormat UVdjmRecordDescriptor::GetRenderTargetPixelFormat() const
{
	switch (RenderTargetFormat)
	{
	case ETextureRenderTargetFormat::RTF_R8:
		return PF_R8;
	case ETextureRenderTargetFormat::RTF_RG8:
		return PF_R8G8;
	case ETextureRenderTargetFormat::RTF_RGBA8:
		return PF_R8G8B8A8;
	case ETextureRenderTargetFormat::RTF_R16f:
		return PF_R16F;
	case ETextureRenderTargetFormat::RTF_RG16f:
		return PF_G16R16F;
	case ETextureRenderTargetFormat::RTF_RGBA16f:
		return PF_A16B16G16R16;
	case ETextureRenderTargetFormat::RTF_R32f:
		return PF_R32_FLOAT;
	case ETextureRenderTargetFormat::RTF_RG32f:
		return PF_G32R32F;
	case ETextureRenderTargetFormat::RTF_RGBA32f:
		return PF_A32B32G32R32F;
	default:
		return PF_G8;//	TODO: check this
	}
}



/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	class UVdjmRecordEnvDataAsset :public UPrimaryDataAsset
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
// bool UVdjmRecordEnvCurrentInfo_deprecated::InitializeCurrentEnvironment(AVdjmRecordBridgeActor* ownerBridge)
// {
// 	if (ownerBridge && ownerBridge->DbcValidConfigureDataAsset())
// 	{
// 		mLinkedDataAsset = ownerBridge->GetRecordEnvConfigureDataAsset();
// 		mCurrentGlobalRules = ownerBridge->GetGlobalRules();
// 		
// 		if (FVdjmRecordEnvPlatformInfo* perPlatform = ownerBridge->GetCurrentPlatformInfo())
// 		{
// 			mCurrentPlatform = ownerBridge->GetTargetPlatform();
// 			mCurrentGlobalRules = ownerBridge->GetCurrentGlobalRules();
// 			if (perPlatform->bUseAutoTargetPlatformResolution)
// 			{
// 				FIntPoint outputResolution = FIntPoint::ZeroValue;
// 				if (ownerBridge->TryResolveViewportSize(outputResolution))
// 				{
// 					mCurrentResolution = outputResolution;
// 				}
// 				else
// 				{
// 					mCurrentResolution = FIntPoint(1920,1080);
// 					ownerBridge->CriticalErrorStop(TEXT("Failed to resolve viewport size for recording. Recording cannot proceed."));
// 					return false;
// 				}
// 			}
// 			else
// 			{
// 				mCurrentResolution = perPlatform->Resolution;
// 			}
// 			
// 			mCurrentFrameRate = perPlatform->FrameRate;
// 			mCurrentPixelFormat = perPlatform->PixelFormat;
// 			mAllBitrateMap = perPlatform->BitrateMap;
//
// 			float floatBitrate = 
// 				mAllBitrateMap.Contains(ownerBridge->SelectedBitrateType)?
// 					FVdjmFunctionLibraryHelper::ConvertToBitrateValue(mAllBitrateMap[ownerBridge->SelectedBitrateType]) :
// 					mAllBitrateMap.Contains(EVdjmRecordQualityTiers::EDefault)?
// 						mAllBitrateMap[EVdjmRecordQualityTiers::EDefault] : 2000000.0f;
// 			mCurrentBitrate = FVdjmFunctionLibraryHelper::ConvertToBitrateValue(floatBitrate);
// 			
// 			//mCurrentBitrate =
// 			mCurrentFilePrefix = perPlatform->FilePrefix;
// 			FString defaultFilePath;
// 			switch (mCurrentPlatform)
// 			{
// 			case EVdjmRecordEnvPlatform::EAndroid:
// 				defaultFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());
// 				break;
// 			case EVdjmRecordEnvPlatform::EWindows:
// 			default:
// 				defaultFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
// 				break;
// 			}
// 			if (perPlatform->CustomFileSaverClass != nullptr)
// 			{
// 				// TODO
// 			}
// 			mCurrentFilePath = defaultFilePath;
// 			return true;
// 		}
// 		else
// 		{
// 			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecorderCore::Initialize - ownerBridge->SelectedBitrateType is invalid."));
// 		}
// 	}
// 	else
// 	{
// 		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecorderCore::Initialize - ownerBridge is null."));
// 	}
// 	return false;
// }

//
// bool UVdjmRecordEnvCurrentInfo_deprecated::DbcIsValidCurrentInfo() const
// {
// 	/*
// 	 * 원래라면 mCurrentCustomFileSaverInstance 이걸 가지게 해야함.
// 	* if(mCurrentCustomFileSaverInstance)
// 	{
// 	}
// 	 */
// 	return mLinkedDataAsset.IsValid() && not mAllBitrateMap.IsEmpty();
// }

// FString UVdjmRecordEnvCurrentInfo_deprecated::MakeFinalFilePath(const FString& customFileName)
// {
// 	FString basePath;
// 	
// 	switch (mCurrentPlatform)
// 	{
// 	case EVdjmRecordEnvPlatform::EWindows:
// 		basePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
// 		break;
// 	case EVdjmRecordEnvPlatform::EAndroid:
// 		basePath =  FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());
// 		break;
// 	case EVdjmRecordEnvPlatform::EIOS:
// 		basePath = FPaths::ProjectSavedDir();
// 		break;
// 	case EVdjmRecordEnvPlatform::EMac:
// 		basePath = FPaths::ProjectSavedDir();
// 		break;
// 	case EVdjmRecordEnvPlatform::ELinux:
// 		basePath = FPaths::ProjectSavedDir();
// 		break;
// 	default:
// 		basePath = FPaths::ProjectSavedDir();
// 		break;
// 	}
// 	if(mCurrentFilePrefix.IsEmpty())
// 	{
// 		mCurrentFilePrefix = TEXT("Vcard");
// 	}
// 	if (mCurrentFilePrefix.EndsWith(TEXT("_")))
// 	{
// 		mCurrentFilePrefix.RemoveFromEnd(TEXT("_"));
// 	}
// 	FString finalPath = mCurrentFilePrefix + TEXT("_");
// 	if(customFileName.IsEmpty())
// 	{
// 		finalPath += TEXT("_")+FString::FromInt(FDateTime::Now().ToUnixTimestamp());
// 	}
// 	else
// 	{
// 		mFileName = customFileName;
// 		finalPath += customFileName + TEXT("_") + FString::FromInt(FDateTime::Now().ToUnixTimestamp());
// 	}
// 	
// 	return FPaths::Combine(basePath, finalPath + TEXT(".mp4"));
// }

bool UVdjmRecordEnvResolver::InitResolverEnvironment(AVdjmRecordBridgeActor* ownerBridge)
{
	if (ownerBridge == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::InitResolverEnvironment - ownerBridge is null."));
		return false;
	}
	LinkedOwnerBridge = ownerBridge;
	if (not ownerBridge->DbcValidConfigureDataAsset())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::InitResolverEnvironment - ownerBridge does not have a valid configure data asset."));
		return false;
	}
	SetResolvedGlobalRules(ownerBridge->GetCurrentGlobalRules());
	return true;
}

UVdjmRecordResource* UVdjmRecordEnvResolver::CreateResolvedRecordResource(const FVdjmRecordEnvPlatformPreset* presetData) 
{
	if (not LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - ownerBridge is null."));
		return nullptr;
	}
	
	if (presetData == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - presetData is null."));
		return nullptr;
	}
	
	//	FVdjmRecordEnvPlatformPreset 검증 및 해석 후 UVdjmRecordResource 생성,
	if (not ResolveEnvPlatform(presetData))
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to resolve environment platform."));
		return nullptr;
	}
	
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Successfully resolved environment platform. Resolved Quality Tier: %s"), *StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(mResolvedQualityTier));
	
	if (UVdjmRecordResource* newResource = NewObject<UVdjmRecordResource>(this,presetData->RecordResourceClass))
	{
		newResource->LinkedOwnerBridge = LinkedOwnerBridge.Get();
		newResource->LinkedResolver = this;
		if (not newResource->InitializeResource(this))
		{
			newResource->ReleaseResources();
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to initialize record resource."));
			newResource = nullptr;
			return nullptr;
		}
		return newResource;
		
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::CreateResolvedRecordResource - Failed to create record resource instance."));
		return nullptr;
	}
	
}

bool UVdjmRecordEnvResolver::InitComplete(AVdjmRecordBridgeActor* ownerBridge, UVdjmRecordResource* resource,
	UVdjmRecordUnitPipeline* pipeline)
{
	if (ownerBridge != nullptr && resource != nullptr && pipeline != nullptr)
	{
		LinkedOwnerBridge = ownerBridge;
		LinkedRecordResource = resource;
		LinkedPipeline = pipeline;
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordEnvResolver::InitComplete - One or more parameters are null. ownerBridge: %s, resource: %s, pipeline: %s"),
			ownerBridge ? *ownerBridge->GetName() : TEXT("null"),
			resource ? *resource->GetName() : TEXT("null"),
			pipeline ? *pipeline->GetName() : TEXT("null"));
	}
	return DbcIsValidEnvResolverInit();
}

bool UVdjmRecordEnvResolver::ResolveEnvPlatform(const FVdjmRecordEnvPlatformPreset* presetData)
{
	mResolvedPreset.Clear();
	mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
	mHasResolved = false;

	if (presetData == nullptr || !presetData->DbcIsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform - presetData is null or invalid."));
		return false;
	}

	if (!LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform - LinkedOwnerBridge is invalid."));
		return false;
	}

	const EVdjmRecordQualityTiers requestedTier =
		(LinkedOwnerBridge->GetRequestedQualityTier() != EVdjmRecordQualityTiers::EUndefined)
			? LinkedOwnerBridge->GetRequestedQualityTier()
			: presetData->DefaultQualityTier;

	constexpr EVdjmRecordQualityTiers tierOrder[] = {
		EVdjmRecordQualityTiers::EUltra,
		EVdjmRecordQualityTiers::EHigh,
		EVdjmRecordQualityTiers::EMediumHigh,
		EVdjmRecordQualityTiers::EMedium,
		EVdjmRecordQualityTiers::EMdeiumLow,
		EVdjmRecordQualityTiers::ELow,
		EVdjmRecordQualityTiers::ELowest
	};

	auto findTierIndex = [tierOrder](EVdjmRecordQualityTiers tier) -> int32
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(tierOrder); ++i)
		{
			if (tierOrder[i] == tier)
			{
				return i;
			}
		}
		return INDEX_NONE;
	};

	int32 beginTier = findTierIndex(requestedTier);
	if (beginTier == INDEX_NONE)
	{
		beginTier = findTierIndex(presetData->DefaultQualityTier);
	}
	if (beginTier == INDEX_NONE)
	{
		beginTier = 0;
	}

	FIntPoint viewportSize = FIntPoint::ZeroValue;
	if (!LinkedOwnerBridge->TryResolveViewportSize(viewportSize) ||
		viewportSize.X <= 0 || viewportSize.Y <= 0)
	{
		viewportSize = VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution(beginTier);
	}

	const UVdjmRecordEnvDataAsset* configAsset = LinkedOwnerBridge->GetRecordEnvConfigureDataAsset();
	FVdjmRecordGlobalRules activeRules = configAsset ? configAsset->GlobalRules : FVdjmRecordGlobalRules();
	const float requestedMaxRecordDurationSeconds = LinkedOwnerBridge->GetRequestedMaxRecordDurationSeconds();
	if (requestedMaxRecordDurationSeconds > 0.0f)
	{
		activeRules.MaxRecordDurationSeconds = requestedMaxRecordDurationSeconds;
	}
	SetResolvedGlobalRules(activeRules);
	const int32 safeDisplayRefreshHz = FMath::Max(1, activeRules.MaxFrameRate);

	const FString customFileName = LinkedOwnerBridge->GetCurrentFileName();

	auto tryResolveTier = [&](EVdjmRecordQualityTiers candidateTier, int32 candidateTierIndex) -> bool
	{
		const FVdjmEncoderInitRequest* sourceRequest = presetData->GetEncoderInitRequest(candidateTier);
		if (sourceRequest == nullptr)
		{
			return false;
		}

		FVdjmEncoderInitRequest candidateRequest = *sourceRequest;
		if (!candidateRequest.EvaluateValidation())
		{
			UE_LOG(LogVdjmRecorderCore, Warning,
				TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform - Tier %s has invalid EncoderInitRequest, skipping."),
				*StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(candidateTier));
			return false;
		}

		const FIntPoint tierMaxResolution =
			VdjmRecordUtils::FeaturePresets::GetPresetFeatureResolution(FMath::Max(0, candidateTierIndex));
		const FIntPoint requestedResolution = LinkedOwnerBridge->GetRequestedResolution();
		const bool bHasRequestedResolution =
			requestedResolution.X > 0 &&
			requestedResolution.Y > 0;
		const FIntPoint presetResolution = bHasRequestedResolution
			? requestedResolution
			: FIntPoint(candidateRequest.VideoConfig.Width, candidateRequest.VideoConfig.Height);
		bool bResolutionFitToDisplay = candidateRequest.VideoConfig.bResolutionFitToDisplay;
		if (LinkedOwnerBridge->HasRequestedResolutionFitToDisplay())
		{
			bResolutionFitToDisplay = LinkedOwnerBridge->GetRequestedResolutionFitToDisplay();
		}
		else if (bHasRequestedResolution)
		{
			bResolutionFitToDisplay = false;
		}
		const FIntPoint safeResolution = VdjmRecordUtils::Resolvers::ResolveVideoResolution(
			viewportSize,
			presetResolution,
			bResolutionFitToDisplay,
			tierMaxResolution);

		const int32 requestedFrameRate = LinkedOwnerBridge->GetRequestedFrameRate();
		const int32 safeFrameRate = VdjmRecordUtils::Resolvers::ResolveVideoFrameRate(
			requestedFrameRate > 0 ? requestedFrameRate : candidateRequest.VideoConfig.FrameRate,
			activeRules,
			safeDisplayRefreshHz);

		const int32 resolvedBitrateByTheory = VdjmRecordUtils::Resolvers::ResolveVideoBitrateBps(
			safeResolution,
			safeFrameRate,
			candidateTier,
			EVdjmRecordContentComplexity::EGameplay);

		const int32 requestedBitrate = LinkedOwnerBridge->GetRequestedBitrate();
		int32 safeBitrate = 0;
		if (!VdjmRecordUtils::Validations::DbcValidateBitrate(
			requestedBitrate > 0 ? requestedBitrate : resolvedBitrateByTheory,
			safeBitrate,
			TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform")))
		{
			return false;
		}

		const int32 requestedKeyframeInterval = LinkedOwnerBridge->GetRequestedKeyframeInterval();
		if (requestedKeyframeInterval >= 0)
		{
			candidateRequest.VideoConfig.KeyframeInterval = requestedKeyframeInterval;
		}

		candidateRequest.VideoConfig.bResolutionFitToDisplay = bResolutionFitToDisplay;
		candidateRequest.VideoConfig.Width = safeResolution.X;
		candidateRequest.VideoConfig.Height = safeResolution.Y;
		candidateRequest.VideoConfig.FrameRate = safeFrameRate;
		candidateRequest.VideoConfig.Bitrate = safeBitrate;

		candidateRequest.AudioConfig.ChannelCount = FMath::Clamp(candidateRequest.AudioConfig.ChannelCount, 1, 2);
		const int32 requestedSampleRate = candidateRequest.AudioConfig.SampleRate;
		if (requestedSampleRate <= 0)
		{
			candidateRequest.AudioConfig.SampleRate = 44100;
		}
		else if (requestedSampleRate > 48000)
		{
			candidateRequest.AudioConfig.SampleRate = 48000;
		}
		else if (requestedSampleRate >= 44100)
		{
			candidateRequest.AudioConfig.SampleRate = 44100;
		}
		else if (requestedSampleRate >= 32000)
		{
			candidateRequest.AudioConfig.SampleRate = 32000;
		}
		else
		{
			candidateRequest.AudioConfig.SampleRate = 44100;
		}

		const bool bMusicHeavy = candidateRequest.AudioConfig.ChannelCount >= 2;
		candidateRequest.AudioConfig.Bitrate = VdjmRecordUtils::Resolvers::ResolveAudioBitrateBps(
			candidateRequest.AudioConfig.SampleRate,
			candidateRequest.AudioConfig.ChannelCount,
			bMusicHeavy);

		FVdjmRecordEnvPlatformPreset candidatePreset = *presetData;
		candidatePreset.EncoderInitRequestMap.FindOrAdd(candidateTier) = candidateRequest;

		//	이 시점에서 CandidatePreset은 후보 Tier에 맞게 보정된 해상도와 비트레이트를 가지고 있음.
		mResolvedPreset = MoveTemp(candidatePreset);
		mResolvedQualityTier = candidateTier;

		if (not ResolvedFinalFilePath(customFileName))
		{
			mResolvedPreset.Clear();
			mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
			return false;
		}

		const FVdjmEncoderInitRequest* finalResolvedRequest = TryGetResolvedEncoderInitRequest();
		if (finalResolvedRequest == nullptr || !finalResolvedRequest->EvaluateValidation())
		{
			mResolvedPreset.Clear();
			mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
			return false;
		}

		if (!LinkedOwnerBridge->EvaluateInitRequest(finalResolvedRequest))
		{
			UE_LOG(LogVdjmRecorderCore, Warning,
				TEXT("UVdjmRecordEnvResolver::ResolveEnvPlatform - Tier %s rejected by hardware probe."),
				*StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(candidateTier));

			mResolvedPreset.Clear();
			mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
			return false;
		}

		return mResolvedPreset.DbcIsValid();
	};

	// 1) Requested/default 가 EDefault 면 그걸 먼저 시도
	const bool bTryDefaultFirst =
		requestedTier == EVdjmRecordQualityTiers::EDefault ||
		(requestedTier == EVdjmRecordQualityTiers::EUndefined &&
		 presetData->DefaultQualityTier == EVdjmRecordQualityTiers::EDefault);

	if (bTryDefaultFirst &&
		presetData->EncoderInitRequestMap.Contains(EVdjmRecordQualityTiers::EDefault))
	{
		if (tryResolveTier(EVdjmRecordQualityTiers::EDefault, 0))
		{
			mHasResolved = true;
			return true;
		}
	}

	// 2) ordered tier down
	for (int32 tierIndex = beginTier; tierIndex < UE_ARRAY_COUNT(tierOrder); ++tierIndex)
	{
		if (tryResolveTier(tierOrder[tierIndex], tierIndex))
		{
			mHasResolved = true;
			return true;
		}
	}

	// 3) 마지막 fallback 으로 EDefault
	if (presetData->EncoderInitRequestMap.Contains(EVdjmRecordQualityTiers::EDefault))
	{
		if (tryResolveTier(EVdjmRecordQualityTiers::EDefault, 0))
		{
			mHasResolved = true;
			return true;
		}
	}

	mResolvedPreset.Clear();
	mResolvedQualityTier = EVdjmRecordQualityTiers::EUndefined;
	return false;
}

bool UVdjmRecordEnvResolver::ResolvedFinalFilePath(const FString& customFileName)
{
	if (!LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordEnvResolver::ResolvedFinalFilePath - LinkedOwnerBridge is invalid."));
		return false;
	}

	FVdjmEncoderInitRequest* mutableRequest =
		mResolvedPreset.EncoderInitRequestMap.Find(mResolvedQualityTier);

	if (mutableRequest == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordEnvResolver::ResolvedFinalFilePath - No mutable request found for tier %s."),
			*StaticEnum<EVdjmRecordQualityTiers>()->GetValueAsString(mResolvedQualityTier));
		return false;
	}

	const FString requestedOutputPath = LinkedOwnerBridge->GetRequestedOutputFilePath();
	const FString requestedSessionId = LinkedOwnerBridge->GetRequestedSessionId();
	const bool bOverwriteExists = LinkedOwnerBridge->HasRequestedOverwriteExists()
		? LinkedOwnerBridge->GetRequestedOverwriteExists()
		: mutableRequest->OutputConfig.bOverwriteExists;
	const FString outputPath = requestedOutputPath.TrimStartAndEnd().IsEmpty()
		? mutableRequest->OutputConfig.OutputFilePath
		: requestedOutputPath;
	const FString sessionId = requestedSessionId.TrimStartAndEnd().IsEmpty()
		? mutableRequest->OutputConfig.SessionId
		: requestedSessionId.TrimStartAndEnd();

	const FString safeOutputPath = VdjmRecordUtils::Resolvers::ResolveOutputPath(
		LinkedOwnerBridge->GetTargetPlatform(),
		outputPath,
		customFileName,
		sessionId,
		bOverwriteExists,
		TEXT("UVdjmRecordEnvResolver::ResolvedFinalFilePath"));

	if (safeOutputPath.IsEmpty())
	{
		return false;
	}

	mutableRequest->OutputConfig.OutputFilePath = NormalizeRecordOutputPathForRuntime(
		safeOutputPath,
		LinkedOwnerBridge->GetTargetPlatform());
	mutableRequest->OutputConfig.SessionId = sessionId;
	mutableRequest->OutputConfig.bOverwriteExists = bOverwriteExists;
	return mutableRequest->OutputConfig.EvaluateValidation();
}

bool UVdjmRecordEnvResolver::RefreshResolvedOutputPath()
{
	if (!LinkedOwnerBridge.IsValid())
	{
		return false;
	}

	const UVdjmRecordEnvDataAsset* configAsset = LinkedOwnerBridge->GetRecordEnvConfigureDataAsset();
	if (configAsset == nullptr)
	{
		return false;
	}

	const FVdjmRecordEnvPlatformPreset* platformPreset = configAsset->GetPlatformPreset(LinkedOwnerBridge->GetTargetPlatform());
	if (platformPreset == nullptr)
	{
		return false;
	}

	// Refresh는 기존 mResolvedPreset을 다시 입력으로 쓰지 않는다.
	// mResolvedPreset은 "결과"이고, 재계산의 source of truth는 항상 DataAsset 원본 preset이다.
	return ResolveEnvPlatform(platformPreset);
}

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
§	↓		class UVdjmRecordResource : public UObject			↓
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

void UVdjmRecordMediaManifest::Clear()
{
	mSchemaVersion = 1;
	mRecordId.Reset();
	mCreatedUnixTime = 0;
	mSourceApp = TEXT("vdjm");
	mOutputFilePath.Reset();
	mMetadataFilePath.Reset();
	mFileSizeBytes = -1;
	mRecordedFrameCount = 0;
	mTargetPlatform = EVdjmRecordEnvPlatform::EDefault;
	mVideoWidth = 0;
	mVideoHeight = 0;
	mVideoFrameRate = 0;
	mVideoBitrate = 0;
	mVideoMimeType.Reset();
	mPlaybackLocatorType = TEXT("local_path");
	mPlaybackLocator.Reset();
	mStreamTokenId.Reset();
	mPlaybackExpiresUnixTime = 0;
	mThumbnailFilePath.Reset();
	mPreviewClipFilePath.Reset();
	mPreviewClipMimeType = TEXT("video/mp4");
	mPreviewErrorReason.Reset();
	mPreviewStartTimeSec = 0.0;
	mPreviewDurationSec = 3.0;
	mPreviewStatus = EVdjmRecordMediaPreviewStatus::ENotReady;
	mAuthorityRole = EVdjmRecordManifestAuthorityRole::EDeveloper;
	mAuthorityUserId.Reset();
	mAuthorityTokenId.Reset();
	mAuthorityKeyId.Reset();
	mVideoSha256.Reset();
	mMetadataSha256.Reset();
	mSignature.Reset();
	mMediaPublishStatus = EVdjmRecordMediaPublishStatus::ENotStarted;
	mPublishedContentUri.Reset();
	mPublishedDisplayName.Reset();
	mPublishedRelativePath.Reset();
	mMediaPublishErrorReason.Reset();
	mbRequiresAuth = true;
	mbInitialized = false;
}

bool UVdjmRecordMediaManifest::InitializeFromArtifact(
	const UVdjmRecordArtifact* artifact,
	const FString& metadataFilePath,
	FString& outErrorReason)
{
	Clear();
	outErrorReason.Reset();

	if (not IsValid(artifact))
	{
		outErrorReason = TEXT("Record artifact is invalid.");
		return false;
	}

	if (not artifact->IsValidArtifact())
	{
		outErrorReason = FString::Printf(
			TEXT("Record artifact is not valid. Reason=%s"),
			*artifact->GetValidationError());
		return false;
	}

	const FVdjmRecordEncoderSnapshot& encoderSnapshot = artifact->GetEncoderSnapshot();
	mRecordId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	mCreatedUnixTime = FDateTime::UtcNow().ToUnixTimestamp();
	mOutputFilePath = artifact->GetOutputFilePath();
	mMetadataFilePath = metadataFilePath.TrimStartAndEnd();
	mFileSizeBytes = artifact->GetFileSizeBytes();
	mRecordedFrameCount = artifact->GetRecordedFrameCount();
	mTargetPlatform = encoderSnapshot.TargetPlatform;
	mVideoWidth = encoderSnapshot.VideoConfig.VideoWidth;
	mVideoHeight = encoderSnapshot.VideoConfig.VideoHeight;
	mVideoFrameRate = encoderSnapshot.VideoConfig.VideoFPS;
	mVideoBitrate = encoderSnapshot.VideoConfig.VideoBitrate;
	mVideoMimeType = encoderSnapshot.VideoConfig.MimeType;
	mPlaybackLocatorType = TEXT("local_path");
	mPlaybackLocator = mOutputFilePath;
	mPreviewStartTimeSec = 0.0;
	mPreviewDurationSec = 3.0;
	mPreviewClipMimeType = mVideoMimeType.IsEmpty() ? TEXT("video/mp4") : mVideoMimeType;
	mPreviewStatus = EVdjmRecordMediaPreviewStatus::EReady;
	mbInitialized = true;

	return ValidateManifest(outErrorReason);
}

void UVdjmRecordMediaManifest::SetAuthorityIdentity(
	EVdjmRecordManifestAuthorityRole authorityRole,
	const FString& userId,
	const FString& tokenId,
	const FString& keyId)
{
	mAuthorityRole = authorityRole;
	mAuthorityUserId = userId.TrimStartAndEnd();
	mAuthorityTokenId = tokenId.TrimStartAndEnd();
	mAuthorityKeyId = keyId.TrimStartAndEnd();
}

void UVdjmRecordMediaManifest::SetPlaybackLocator(
	const FString& locatorType,
	const FString& locator,
	const FString& streamTokenId,
	int64 expiresUnixTime)
{
	mPlaybackLocatorType = locatorType.TrimStartAndEnd();
	mPlaybackLocator = locator.TrimStartAndEnd();
	mStreamTokenId = streamTokenId.TrimStartAndEnd();
	mPlaybackExpiresUnixTime = FMath::Max<int64>(0, expiresUnixTime);
}

void UVdjmRecordMediaManifest::SetPreviewInfo(
	const FString& thumbnailFilePath,
	const FString& previewClipFilePath,
	const FString& previewClipMimeType,
	double previewStartTimeSec,
	double previewDurationSec,
	EVdjmRecordMediaPreviewStatus previewStatus,
	const FString& previewErrorReason)
{
	mThumbnailFilePath = thumbnailFilePath.TrimStartAndEnd();
	mPreviewClipFilePath = previewClipFilePath.TrimStartAndEnd();
	mPreviewClipMimeType = previewClipMimeType.TrimStartAndEnd();
	mPreviewStartTimeSec = FMath::Max(0.0, previewStartTimeSec);
	mPreviewDurationSec = FMath::Max(0.1, previewDurationSec);
	mPreviewStatus = previewStatus;
	mPreviewErrorReason = previewErrorReason.TrimStartAndEnd();

	if (mPreviewClipMimeType.IsEmpty())
	{
		mPreviewClipMimeType = mVideoMimeType.IsEmpty() ? TEXT("video/mp4") : mVideoMimeType;
	}
}

void UVdjmRecordMediaManifest::SetMediaPublishResult(
	EVdjmRecordMediaPublishStatus publishStatus,
	const FString& publishedContentUri,
	const FString& publishedDisplayName,
	const FString& publishedRelativePath,
	const FString& publishErrorReason)
{
	mMediaPublishStatus = publishStatus;
	mPublishedContentUri = publishedContentUri.TrimStartAndEnd();
	mPublishedDisplayName = publishedDisplayName.TrimStartAndEnd();
	mPublishedRelativePath = publishedRelativePath.TrimStartAndEnd();
	mMediaPublishErrorReason = publishErrorReason.TrimStartAndEnd();
}

bool UVdjmRecordMediaManifest::ValidateManifest(FString& outErrorReason) const
{
	outErrorReason.Reset();

	if (not mbInitialized)
	{
		outErrorReason = TEXT("Media manifest is not initialized.");
		return false;
	}

	if (mRecordId.IsEmpty())
	{
		outErrorReason = TEXT("Media manifest record id is empty.");
		return false;
	}

	if (mOutputFilePath.IsEmpty())
	{
		outErrorReason = TEXT("Media manifest output file path is empty.");
		return false;
	}

	if (mMetadataFilePath.IsEmpty())
	{
		outErrorReason = TEXT("Media manifest metadata file path is empty.");
		return false;
	}

	if (mFileSizeBytes <= 0)
	{
		outErrorReason = FString::Printf(TEXT("Media manifest file size is invalid. FileSizeBytes=%lld"), mFileSizeBytes);
		return false;
	}

	if (mRecordedFrameCount <= 0)
	{
		outErrorReason = FString::Printf(TEXT("Media manifest frame count is invalid. RecordedFrameCount=%d"), mRecordedFrameCount);
		return false;
	}

	if (mVideoWidth <= 0 || mVideoHeight <= 0 || mVideoFrameRate <= 0 || mVideoBitrate <= 0)
	{
		outErrorReason = FString::Printf(
			TEXT("Media manifest video config is invalid. Width=%d Height=%d FPS=%d Bitrate=%d"),
			mVideoWidth,
			mVideoHeight,
			mVideoFrameRate,
			mVideoBitrate);
		return false;
	}

	if (mVideoMimeType.IsEmpty())
	{
		outErrorReason = TEXT("Media manifest video mime type is empty.");
		return false;
	}

	if (mPlaybackLocatorType.IsEmpty() || mPlaybackLocator.IsEmpty())
	{
		outErrorReason = TEXT("Media manifest playback locator is empty.");
		return false;
	}

	if (mAuthorityRole == EVdjmRecordManifestAuthorityRole::EUndefined)
	{
		outErrorReason = TEXT("Media manifest authority role is undefined.");
		return false;
	}

	return true;
}

FString UVdjmRecordMediaManifest::ToString() const
{
	return FString::Printf(
		TEXT("RecordId=%s, Output=%s, Metadata=%s, FileSize=%lld, Frames=%d, Video=%dx%d@%dfps Bitrate=%d, Playback=%s:%s, Preview=%0.2f-%0.2fs Clip=%s, Publish=%s Uri=%s, Authority=%s User=%s Token=%s Key=%s"),
		*mRecordId,
		*mOutputFilePath,
		*mMetadataFilePath,
		mFileSizeBytes,
		mRecordedFrameCount,
		mVideoWidth,
		mVideoHeight,
		mVideoFrameRate,
		mVideoBitrate,
		*mPlaybackLocatorType,
		*mPlaybackLocator,
		mPreviewStartTimeSec,
		mPreviewStartTimeSec + mPreviewDurationSec,
		*mPreviewClipFilePath,
		*StaticEnum<EVdjmRecordMediaPublishStatus>()->GetValueAsString(mMediaPublishStatus),
		*mPublishedContentUri,
		*StaticEnum<EVdjmRecordManifestAuthorityRole>()->GetValueAsString(mAuthorityRole),
		*mAuthorityUserId,
		*mAuthorityTokenId,
		*mAuthorityKeyId);
}

FString UVdjmRecordMediaManifest::ToJsonString() const
{
	TSharedPtr<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetNumberField(TEXT("schema_version"), mSchemaVersion);

	TSharedPtr<FJsonObject> recordObject = MakeShared<FJsonObject>();
	recordObject->SetStringField(TEXT("record_id"), mRecordId);
	recordObject->SetNumberField(TEXT("created_unix_time"), mCreatedUnixTime);
	recordObject->SetStringField(TEXT("source_app"), mSourceApp);
	rootObject->SetObjectField(TEXT("record"), recordObject);

	TSharedPtr<FJsonObject> mediaObject = MakeShared<FJsonObject>();
	mediaObject->SetStringField(TEXT("output_file_path"), mOutputFilePath);
	mediaObject->SetStringField(TEXT("metadata_file_path"), mMetadataFilePath);
	mediaObject->SetNumberField(TEXT("file_size_bytes"), mFileSizeBytes);
	mediaObject->SetNumberField(TEXT("recorded_frame_count"), mRecordedFrameCount);
	mediaObject->SetStringField(TEXT("platform"), StaticEnum<EVdjmRecordEnvPlatform>()->GetValueAsString(mTargetPlatform));
	mediaObject->SetStringField(TEXT("mime_type"), mVideoMimeType);
	mediaObject->SetNumberField(TEXT("width"), mVideoWidth);
	mediaObject->SetNumberField(TEXT("height"), mVideoHeight);
	mediaObject->SetNumberField(TEXT("fps"), mVideoFrameRate);
	mediaObject->SetNumberField(TEXT("bitrate"), mVideoBitrate);
	rootObject->SetObjectField(TEXT("media"), mediaObject);

	TSharedPtr<FJsonObject> playbackObject = MakeShared<FJsonObject>();
	playbackObject->SetStringField(TEXT("locator_type"), mPlaybackLocatorType);
	playbackObject->SetStringField(TEXT("locator"), mPlaybackLocator);
	playbackObject->SetStringField(TEXT("stream_token_id"), mStreamTokenId);
	playbackObject->SetNumberField(TEXT("expires_unix_time"), mPlaybackExpiresUnixTime);
	rootObject->SetObjectField(TEXT("playback"), playbackObject);

	TSharedPtr<FJsonObject> previewObject = MakeShared<FJsonObject>();
	previewObject->SetStringField(TEXT("thumbnail_file_path"), mThumbnailFilePath);
	previewObject->SetStringField(TEXT("preview_clip_file_path"), mPreviewClipFilePath);
	previewObject->SetStringField(TEXT("preview_clip_mime_type"), mPreviewClipMimeType);
	previewObject->SetNumberField(TEXT("start_time_sec"), mPreviewStartTimeSec);
	previewObject->SetNumberField(TEXT("duration_sec"), mPreviewDurationSec);
	previewObject->SetStringField(TEXT("status"), StaticEnum<EVdjmRecordMediaPreviewStatus>()->GetValueAsString(mPreviewStatus));
	previewObject->SetStringField(TEXT("error_reason"), mPreviewErrorReason);
	rootObject->SetObjectField(TEXT("preview"), previewObject);

	TSharedPtr<FJsonObject> publicationObject = MakeShared<FJsonObject>();
	publicationObject->SetStringField(TEXT("status"), StaticEnum<EVdjmRecordMediaPublishStatus>()->GetValueAsString(mMediaPublishStatus));
	publicationObject->SetStringField(TEXT("content_uri"), mPublishedContentUri);
	publicationObject->SetStringField(TEXT("display_name"), mPublishedDisplayName);
	publicationObject->SetStringField(TEXT("relative_path"), mPublishedRelativePath);
	publicationObject->SetStringField(TEXT("error_reason"), mMediaPublishErrorReason);
	rootObject->SetObjectField(TEXT("publication"), publicationObject);

	TSharedPtr<FJsonObject> authorityObject = MakeShared<FJsonObject>();
	authorityObject->SetStringField(TEXT("role"), StaticEnum<EVdjmRecordManifestAuthorityRole>()->GetValueAsString(mAuthorityRole));
	authorityObject->SetStringField(TEXT("user_id"), mAuthorityUserId);
	authorityObject->SetStringField(TEXT("token_id"), mAuthorityTokenId);
	authorityObject->SetStringField(TEXT("key_id"), mAuthorityKeyId);
	authorityObject->SetBoolField(TEXT("requires_auth"), mbRequiresAuth);
	rootObject->SetObjectField(TEXT("authority"), authorityObject);

	TSharedPtr<FJsonObject> integrityObject = MakeShared<FJsonObject>();
	integrityObject->SetStringField(TEXT("video_sha256"), mVideoSha256);
	integrityObject->SetStringField(TEXT("metadata_sha256"), mMetadataSha256);
	integrityObject->SetStringField(TEXT("signature"), mSignature);
	integrityObject->SetStringField(TEXT("key_id"), mAuthorityKeyId);
	rootObject->SetObjectField(TEXT("integrity"), integrityObject);

	FString jsonString;
	TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&jsonString);
	if (not FJsonSerializer::Serialize(rootObject.ToSharedRef(), jsonWriter))
	{
		return TEXT("{}");
	}

	return jsonString;
}

bool UVdjmRecordMediaManifest::LoadFromJsonString(
	const FString& manifestJsonString,
	const FString& fallbackMetadataFilePath,
	FString& outErrorReason)
{
	Clear();
	outErrorReason.Reset();

	if (manifestJsonString.TrimStartAndEnd().IsEmpty())
	{
		outErrorReason = TEXT("Manifest json string is empty.");
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(manifestJsonString);
	if (not FJsonSerializer::Deserialize(reader, rootObject) || not rootObject.IsValid())
	{
		outErrorReason = TEXT("Failed to parse manifest json string.");
		return false;
	}

	const TSharedPtr<FJsonObject>* recordObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* mediaObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* playbackObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* previewObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* publicationObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* authorityObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* integrityObjectPtr = nullptr;
	rootObject->TryGetObjectField(TEXT("record"), recordObjectPtr);
	rootObject->TryGetObjectField(TEXT("media"), mediaObjectPtr);
	rootObject->TryGetObjectField(TEXT("playback"), playbackObjectPtr);
	rootObject->TryGetObjectField(TEXT("preview"), previewObjectPtr);
	rootObject->TryGetObjectField(TEXT("publication"), publicationObjectPtr);
	rootObject->TryGetObjectField(TEXT("authority"), authorityObjectPtr);
	rootObject->TryGetObjectField(TEXT("integrity"), integrityObjectPtr);

	const TSharedPtr<FJsonObject> recordObject = recordObjectPtr != nullptr ? *recordObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> mediaObject = mediaObjectPtr != nullptr ? *mediaObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> playbackObject = playbackObjectPtr != nullptr ? *playbackObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> previewObject = previewObjectPtr != nullptr ? *previewObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> publicationObject = publicationObjectPtr != nullptr ? *publicationObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> authorityObject = authorityObjectPtr != nullptr ? *authorityObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> integrityObject = integrityObjectPtr != nullptr ? *integrityObjectPtr : nullptr;

	mSchemaVersion = GetJsonObjectInt32Field(rootObject, TEXT("schema_version"), 1);
	mRecordId = GetJsonObjectStringField(recordObject, TEXT("record_id"));
	mCreatedUnixTime = GetJsonObjectInt64Field(recordObject, TEXT("created_unix_time"));
	mSourceApp = GetJsonObjectStringField(recordObject, TEXT("source_app"), TEXT("vdjm"));

	mOutputFilePath = GetJsonObjectStringField(mediaObject, TEXT("output_file_path"));
	mMetadataFilePath = GetJsonObjectStringField(mediaObject, TEXT("metadata_file_path"), fallbackMetadataFilePath);
	mFileSizeBytes = GetJsonObjectInt64Field(mediaObject, TEXT("file_size_bytes"), -1);
	mRecordedFrameCount = GetJsonObjectInt32Field(mediaObject, TEXT("recorded_frame_count"));
	mTargetPlatform = ParseEnumValueString(
		GetJsonObjectStringField(mediaObject, TEXT("platform")),
		EVdjmRecordEnvPlatform::EDefault);
	mVideoMimeType = GetJsonObjectStringField(mediaObject, TEXT("mime_type"));
	mVideoWidth = GetJsonObjectInt32Field(mediaObject, TEXT("width"));
	mVideoHeight = GetJsonObjectInt32Field(mediaObject, TEXT("height"));
	mVideoFrameRate = GetJsonObjectInt32Field(mediaObject, TEXT("fps"));
	mVideoBitrate = GetJsonObjectInt32Field(mediaObject, TEXT("bitrate"));

	mPlaybackLocatorType = GetJsonObjectStringField(playbackObject, TEXT("locator_type"), TEXT("local_path"));
	mPlaybackLocator = GetJsonObjectStringField(playbackObject, TEXT("locator"), mOutputFilePath);
	mStreamTokenId = GetJsonObjectStringField(playbackObject, TEXT("stream_token_id"));
	mPlaybackExpiresUnixTime = GetJsonObjectInt64Field(playbackObject, TEXT("expires_unix_time"));

	mThumbnailFilePath = GetJsonObjectStringField(previewObject, TEXT("thumbnail_file_path"));
	mPreviewClipFilePath = GetJsonObjectStringField(previewObject, TEXT("preview_clip_file_path"));
	mPreviewClipMimeType = GetJsonObjectStringField(previewObject, TEXT("preview_clip_mime_type"), TEXT("video/mp4"));
	mPreviewStartTimeSec = FMath::Max(0.0, GetJsonObjectDoubleField(previewObject, TEXT("start_time_sec")));
	mPreviewDurationSec = FMath::Max(0.1, GetJsonObjectDoubleField(previewObject, TEXT("duration_sec"), 3.0));
	mPreviewStatus = ParseEnumValueString(
		GetJsonObjectStringField(previewObject, TEXT("status")),
		EVdjmRecordMediaPreviewStatus::EReady);
	mPreviewErrorReason = GetJsonObjectStringField(previewObject, TEXT("error_reason"));

	mMediaPublishStatus = ParseEnumValueString(
		GetJsonObjectStringField(publicationObject, TEXT("status")),
		EVdjmRecordMediaPublishStatus::ENotStarted);
	mPublishedContentUri = GetJsonObjectStringField(publicationObject, TEXT("content_uri"));
	mPublishedDisplayName = GetJsonObjectStringField(publicationObject, TEXT("display_name"));
	mPublishedRelativePath = GetJsonObjectStringField(publicationObject, TEXT("relative_path"));
	mMediaPublishErrorReason = GetJsonObjectStringField(publicationObject, TEXT("error_reason"));

	mAuthorityRole = ParseEnumValueString(
		GetJsonObjectStringField(authorityObject, TEXT("role")),
		EVdjmRecordManifestAuthorityRole::EDeveloper);
	mAuthorityUserId = GetJsonObjectStringField(authorityObject, TEXT("user_id"));
	mAuthorityTokenId = GetJsonObjectStringField(authorityObject, TEXT("token_id"));
	mAuthorityKeyId = GetJsonObjectStringField(authorityObject, TEXT("key_id"));
	mbRequiresAuth = GetJsonObjectBoolField(authorityObject, TEXT("requires_auth"), true);

	mVideoSha256 = GetJsonObjectStringField(integrityObject, TEXT("video_sha256"));
	mMetadataSha256 = GetJsonObjectStringField(integrityObject, TEXT("metadata_sha256"));
	mSignature = GetJsonObjectStringField(integrityObject, TEXT("signature"));
	if (mAuthorityKeyId.IsEmpty())
	{
		mAuthorityKeyId = GetJsonObjectStringField(integrityObject, TEXT("key_id"));
	}

	if (mPreviewStatus == EVdjmRecordMediaPreviewStatus::ENotReady && not mPlaybackLocator.IsEmpty())
	{
		mPreviewStatus = EVdjmRecordMediaPreviewStatus::EReady;
	}

	mbInitialized = true;
	return ValidateManifest(outErrorReason);
}

bool UVdjmRecordMediaManifest::SaveToFile(const FString& metadataFilePath, FString& outErrorReason)
{
	outErrorReason.Reset();

	const FString requestedMetadataFilePath = metadataFilePath.TrimStartAndEnd();
	if (not requestedMetadataFilePath.IsEmpty())
	{
		mMetadataFilePath = requestedMetadataFilePath;
	}

	if (not ValidateManifest(outErrorReason))
	{
		return false;
	}

	const FString normalizedMetadataPath = FPaths::ConvertRelativePathToFull(mMetadataFilePath);
	const FString metadataDirectory = FPaths::GetPath(normalizedMetadataPath);
	if (metadataDirectory.IsEmpty())
	{
		outErrorReason = TEXT("Media manifest metadata directory is empty.");
		return false;
	}

	if (not IFileManager::Get().DirectoryExists(*metadataDirectory) &&
		not IFileManager::Get().MakeDirectory(*metadataDirectory, true))
	{
		outErrorReason = FString::Printf(TEXT("Failed to create media manifest directory. Directory=%s"), *metadataDirectory);
		return false;
	}

	const FString jsonString = ToJsonString();
	if (jsonString.IsEmpty() || jsonString == TEXT("{}"))
	{
		outErrorReason = TEXT("Media manifest json string is empty.");
		return false;
	}

	if (not FFileHelper::SaveStringToFile(
		jsonString,
		*normalizedMetadataPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outErrorReason = FString::Printf(TEXT("Failed to save media manifest. Path=%s"), *normalizedMetadataPath);
		return false;
	}

	mMetadataFilePath = normalizedMetadataPath;
	return true;
}

bool UVdjmRecordArtifact::InitializeFromSnapshot(
	const FVdjmRecordEncoderSnapshot& encoderSnapshot,
	int32 recordedFrameCount,
	UVdjmRecorderController* ownerController,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	mEncoderSnapshot = encoderSnapshot;
	mOwnerController = ownerController;
	mOutputFilePath = NormalizeRecordOutputPathForRuntime(
		encoderSnapshot.VideoConfig.OutputFilePath,
		encoderSnapshot.TargetPlatform);
	mEncoderSnapshot.VideoConfig.OutputFilePath = mOutputFilePath;
	mRecordedFrameCount = FMath::Max(0, recordedFrameCount);
	mCreatedAtSeconds = FPlatformTime::Seconds();
	mbInitialized = true;

	return ValidateArtifact(outErrorReason);
}

bool UVdjmRecordArtifact::ValidateArtifact(FString& outErrorReason)
{
	outErrorReason.Reset();
	mbValidated = false;
	mbFileExists = false;
	mFileSizeBytes = -1;
	mValidationError.Reset();

	if (not mbInitialized)
	{
		outErrorReason = TEXT("Record artifact is not initialized.");
		mValidationError = outErrorReason;
		return false;
	}

	mOutputFilePath = NormalizeRecordOutputPathForRuntime(
		mOutputFilePath,
		mEncoderSnapshot.TargetPlatform);
	mEncoderSnapshot.VideoConfig.OutputFilePath = mOutputFilePath;

	if (not mEncoderSnapshot.IsValidateCommonEncoderArguments())
	{
		outErrorReason = TEXT("Record artifact encoder snapshot is invalid.");
		mValidationError = outErrorReason;
		return false;
	}

	if (mOutputFilePath.IsEmpty())
	{
		outErrorReason = TEXT("Record artifact output file path is empty.");
		mValidationError = outErrorReason;
		return false;
	}

	const FString normalizedPath = NormalizeRecordOutputPathForRuntime(
		mOutputFilePath,
		mEncoderSnapshot.TargetPlatform);
	mFileSizeBytes = IFileManager::Get().FileSize(*normalizedPath);
	mbFileExists = mFileSizeBytes >= 0;

	if (not mbFileExists)
	{
		outErrorReason = FString::Printf(TEXT("Record artifact output file does not exist. Path=%s"), *normalizedPath);
		mValidationError = outErrorReason;
		return false;
	}

	if (mFileSizeBytes <= 0)
	{
		outErrorReason = FString::Printf(TEXT("Record artifact output file is empty. Path=%s"), *normalizedPath);
		mValidationError = outErrorReason;
		return false;
	}

	if (mRecordedFrameCount <= 0)
	{
		outErrorReason = FString::Printf(TEXT("Record artifact has no recorded frames. Path=%s"), *normalizedPath);
		mValidationError = outErrorReason;
		return false;
	}

	mOutputFilePath = normalizedPath;
	mbValidated = true;
	return true;
}

UObject* UVdjmRecordArtifact::GetOwnerControllerObject() const
{
	return mOwnerController.Get();
}

void UVdjmRecordArtifact::SetMediaManifest(
	UVdjmRecordMediaManifest* mediaManifest,
	bool bMetadataValidated,
	const FString& validationError)
{
	mMediaManifest = mediaManifest;
	mbHasMetadata = IsValid(mediaManifest);
	mbMetadataValidated = mbHasMetadata && bMetadataValidated;
	mMetadataValidationError = validationError;
	mMetadataFilePath = IsValid(mediaManifest) ? mediaManifest->GetMetadataFilePath() : FString();
}

void UVdjmRecordArtifact::SetMediaPublishResult(
	EVdjmRecordMediaPublishStatus publishStatus,
	const FString& publishedContentUri,
	const FString& publishErrorReason)
{
	mMediaPublishStatus = publishStatus;
	mPublishedContentUri = publishedContentUri.TrimStartAndEnd();
	mMediaPublishErrorReason = publishErrorReason.TrimStartAndEnd();
}

void UVdjmRecordArtifact::MarkOutputDeleted(const FString& deletionReason)
{
	mbValidated = false;
	mbFileExists = false;
	mFileSizeBytes = -1;
	mValidationError = deletionReason;
}

UVdjmRecordMetadataStore* UVdjmRecordMetadataStore::FindMetadataStore(UObject* worldContextObject)
{
	const UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject);
	if (worldContextSubsystem == nullptr)
	{
		return nullptr;
	}

	return Cast<UVdjmRecordMetadataStore>(
		worldContextSubsystem->FindContextObject(UVdjmRecorderWorldContextSubsystem::GetMetadataStoreContextKey()));
}

UVdjmRecordMetadataStore* UVdjmRecordMetadataStore::FindOrCreateMetadataStore(UObject* worldContextObject)
{
	if (UVdjmRecordMetadataStore* existingStore = FindMetadataStore(worldContextObject))
	{
		return existingStore;
	}

	UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject);
	UObject* storeOuter = worldContextSubsystem != nullptr ? Cast<UObject>(worldContextSubsystem) : worldContextObject;
	if (storeOuter == nullptr)
	{
		return nullptr;
	}

	UVdjmRecordMetadataStore* newStore = NewObject<UVdjmRecordMetadataStore>(storeOuter);
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

bool UVdjmRecordMetadataStore::InitializeStore(UObject* worldContextObject)
{
	if (worldContextObject == nullptr)
	{
		return false;
	}

	UWorld* world = worldContextObject->GetWorld();
	if (world == nullptr)
	{
		return false;
	}

	mCachedWorld = world;
	if (UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(worldContextObject))
	{
		worldContextSubsystem->RegisterWeakObjectContext(
			UVdjmRecorderWorldContextSubsystem::GetMetadataStoreContextKey(),
			this,
			StaticClass());
	}

	FString registryErrorReason;
	if (not LoadRegistry(registryErrorReason))
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("UVdjmRecordMetadataStore::InitializeStore - Failed to load media registry. Reason=%s"),
			*registryErrorReason);
	}

	return true;
}

void UVdjmRecordMetadataStore::Clear()
{
	mLastManifest = nullptr;
	mRegistryEntries.Reset();
	mAuthorityRole = EVdjmRecordManifestAuthorityRole::EDeveloper;
	mAuthorityUserId.Reset();
	mAuthorityTokenId.Reset();
	mAuthorityKeyId.Reset();
	mLastPublishedContentUri.Reset();
	mLastMediaPublishErrorReason.Reset();
	mNextPostProcessJobId = 1;
	mActiveMediaPublishJobCount = 0;
	mCompletedMediaPublishJobCount = 0;
	mLastMediaPublishStatus = EVdjmRecordMediaPublishStatus::ENotStarted;
	mbDeleteVideoIfMetadataMissing = true;
}

void UVdjmRecordMetadataStore::SetAuthorityIdentity(
	EVdjmRecordManifestAuthorityRole authorityRole,
	const FString& userId,
	const FString& tokenId,
	const FString& keyId)
{
	mAuthorityRole = authorityRole;
	mAuthorityUserId = userId.TrimStartAndEnd();
	mAuthorityTokenId = tokenId.TrimStartAndEnd();
	mAuthorityKeyId = keyId.TrimStartAndEnd();
}

void UVdjmRecordMetadataStore::SetDeleteVideoIfMetadataMissing(bool bDeleteVideoIfMetadataMissing)
{
	mbDeleteVideoIfMetadataMissing = bDeleteVideoIfMetadataMissing;
}

bool UVdjmRecordMetadataStore::LoadRegistry(FString& outErrorReason)
{
	outErrorReason.Reset();
	mRegistryEntries.Reset();

	const FString registryFilePath = GetRegistryFilePath();
	if (registryFilePath.IsEmpty())
	{
		outErrorReason = TEXT("Media registry file path is empty.");
		return false;
	}

	if (not IFileManager::Get().FileExists(*registryFilePath))
	{
		return true;
	}

	FString registryJsonString;
	if (not FFileHelper::LoadFileToString(registryJsonString, *registryFilePath))
	{
		outErrorReason = FString::Printf(TEXT("Failed to load media registry. Path=%s"), *registryFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(registryJsonString);
	if (not FJsonSerializer::Deserialize(reader, rootObject) || not rootObject.IsValid())
	{
		outErrorReason = FString::Printf(TEXT("Failed to parse media registry json. Path=%s"), *registryFilePath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* entryValues = nullptr;
	if (not rootObject->TryGetArrayField(TEXT("entries"), entryValues))
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& entryValue : *entryValues)
	{
		if (not entryValue.IsValid())
		{
			continue;
		}

		FVdjmRecordMediaRegistryEntry entry;
		if (ReadRegistryEntryJsonObject(entryValue->AsObject(), entry))
		{
			RefreshRegistryEntryFileState(entry);
			UpsertRegistryEntry(entry);
		}
	}

	return true;
}

bool UVdjmRecordMetadataStore::SaveRegistry(FString& outErrorReason) const
{
	outErrorReason.Reset();

	const FString registryFilePath = GetRegistryFilePath();
	if (registryFilePath.IsEmpty())
	{
		outErrorReason = TEXT("Media registry file path is empty.");
		return false;
	}

	const FString registryDirectoryPath = FPaths::GetPath(registryFilePath);
	if (registryDirectoryPath.IsEmpty())
	{
		outErrorReason = TEXT("Media registry directory path is empty.");
		return false;
	}

	if (not IFileManager::Get().DirectoryExists(*registryDirectoryPath) &&
		not IFileManager::Get().MakeDirectory(*registryDirectoryPath, true))
	{
		outErrorReason = FString::Printf(TEXT("Failed to create media registry directory. Directory=%s"), *registryDirectoryPath);
		return false;
	}

	TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
	rootObject->SetNumberField(TEXT("schema_version"), 1);
	rootObject->SetNumberField(TEXT("generated_unix_time"), FDateTime::UtcNow().ToUnixTimestamp());
	rootObject->SetStringField(TEXT("registry_file_path"), registryFilePath);

	TArray<TSharedPtr<FJsonValue>> entryValues;
	entryValues.Reserve(mRegistryEntries.Num());
	for (const FVdjmRecordMediaRegistryEntry& entry : mRegistryEntries)
	{
		entryValues.Add(MakeShared<FJsonValueObject>(MakeRegistryEntryJsonObject(entry)));
	}
	rootObject->SetArrayField(TEXT("entries"), MoveTemp(entryValues));

	FString registryJsonString;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&registryJsonString);
	if (not FJsonSerializer::Serialize(rootObject, writer))
	{
		outErrorReason = TEXT("Failed to serialize media registry json.");
		return false;
	}

	if (not FFileHelper::SaveStringToFile(
		registryJsonString,
		*registryFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		outErrorReason = FString::Printf(TEXT("Failed to save media registry. Path=%s"), *registryFilePath);
		return false;
	}

	return true;
}

bool UVdjmRecordMetadataStore::RefreshRegistryFromDisk(FString& outErrorReason)
{
	outErrorReason.Reset();

	FString loadErrorReason;
	if (not LoadRegistry(loadErrorReason))
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("UVdjmRecordMetadataStore::RefreshRegistryFromDisk - Existing registry load failed. Reason=%s"),
			*loadErrorReason);
	}

	const FString manifestDirectoryPath = GetManifestDirectoryPath();
	if (manifestDirectoryPath.IsEmpty())
	{
		outErrorReason = TEXT("Manifest directory path is empty.");
		return false;
	}

	if (IFileManager::Get().DirectoryExists(*manifestDirectoryPath))
	{
		TArray<FString> manifestFileNames;
		const FString searchPattern = FPaths::Combine(manifestDirectoryPath, TEXT("*.vdjm.json"));
		IFileManager::Get().FindFiles(manifestFileNames, *searchPattern, true, false);

		for (const FString& manifestFileName : manifestFileNames)
		{
			const FString metadataFilePath = FPaths::Combine(manifestDirectoryPath, manifestFileName);
			FVdjmRecordMediaRegistryEntry entry;
			FString entryErrorReason;
			if (BuildRegistryEntryFromManifestFile(metadataFilePath, entry, entryErrorReason))
			{
				UpsertRegistryEntry(entry);
			}
			else
			{
				UE_LOG(LogVdjmRecorderCore, Warning,
					TEXT("UVdjmRecordMetadataStore::RefreshRegistryFromDisk - Failed to register manifest file. Path=%s Reason=%s"),
					*metadataFilePath,
					*entryErrorReason);
			}
		}
	}

	for (FVdjmRecordMediaRegistryEntry& entry : mRegistryEntries)
	{
		RefreshRegistryEntryFileState(entry);
	}

	return SaveRegistry(outErrorReason);
}

bool UVdjmRecordMetadataStore::RegisterManifest(UVdjmRecordMediaManifest* mediaManifest, FString& outErrorReason)
{
	outErrorReason.Reset();

	FVdjmRecordMediaRegistryEntry entry;
	if (not BuildRegistryEntryFromManifest(mediaManifest, entry, outErrorReason))
	{
		return false;
	}

	UpsertRegistryEntry(entry);
	return SaveRegistry(outErrorReason);
}

bool UVdjmRecordMetadataStore::LoadManifestFromFile(
	const FString& metadataFilePath,
	UVdjmRecordMediaManifest*& outManifest,
	FString& outErrorReason)
{
	outManifest = nullptr;
	outErrorReason.Reset();

	const FString normalizedMetadataFilePath = FPaths::ConvertRelativePathToFull(metadataFilePath);
	if (normalizedMetadataFilePath.IsEmpty())
	{
		outErrorReason = TEXT("Metadata file path is empty.");
		return false;
	}

	FString manifestJsonString;
	if (not FFileHelper::LoadFileToString(manifestJsonString, *normalizedMetadataFilePath))
	{
		outErrorReason = FString::Printf(TEXT("Failed to load manifest json. Path=%s"), *normalizedMetadataFilePath);
		return false;
	}

	UVdjmRecordMediaManifest* loadedManifest = NewObject<UVdjmRecordMediaManifest>(this);
	if (not IsValid(loadedManifest))
	{
		outErrorReason = TEXT("Failed to create media manifest object.");
		return false;
	}

	if (not loadedManifest->LoadFromJsonString(manifestJsonString, normalizedMetadataFilePath, outErrorReason))
	{
		return false;
	}

	outManifest = loadedManifest;
	mLastManifest = loadedManifest;
	return true;
}

bool UVdjmRecordMetadataStore::LoadManifestFromRegistryEntry(
	const FVdjmRecordMediaRegistryEntry& registryEntry,
	UVdjmRecordMediaManifest*& outManifest,
	FString& outErrorReason)
{
	outManifest = nullptr;
	outErrorReason.Reset();

	if (registryEntry.MetadataFilePath.IsEmpty())
	{
		outErrorReason = TEXT("Registry entry metadata file path is empty.");
		return false;
	}

	return LoadManifestFromFile(registryEntry.MetadataFilePath, outManifest, outErrorReason);
}

void UVdjmRecordMetadataStore::ClearRegistry()
{
	mRegistryEntries.Reset();
}

bool UVdjmRecordMetadataStore::BuildAndSaveManifest(UVdjmRecordArtifact* artifact, FString& outErrorReason)
{
	outErrorReason.Reset();
	mLastManifest = nullptr;

	if (not IsValid(artifact))
	{
		outErrorReason = TEXT("Record artifact is invalid.");
		return false;
	}

	const FString metadataFilePath = MakeMetadataFilePathForArtifact(artifact);
	if (metadataFilePath.IsEmpty())
	{
		outErrorReason = TEXT("Failed to resolve metadata file path.");
		if (mbDeleteVideoIfMetadataMissing)
		{
			const FString cleanupReason = outErrorReason;
			DeleteVideoFileForMissingMetadata(artifact, cleanupReason, outErrorReason);
		}
		return false;
	}

	UVdjmRecordMediaManifest* mediaManifest = NewObject<UVdjmRecordMediaManifest>(this);
	if (not IsValid(mediaManifest))
	{
		outErrorReason = TEXT("Failed to create media manifest.");
		if (mbDeleteVideoIfMetadataMissing)
		{
			const FString cleanupReason = outErrorReason;
			DeleteVideoFileForMissingMetadata(artifact, cleanupReason, outErrorReason);
		}
		return false;
	}

	if (not mediaManifest->InitializeFromArtifact(artifact, metadataFilePath, outErrorReason))
	{
		artifact->SetMediaManifest(mediaManifest, false, outErrorReason);
		if (mbDeleteVideoIfMetadataMissing)
		{
			const FString cleanupReason = outErrorReason;
			DeleteVideoFileForMissingMetadata(artifact, cleanupReason, outErrorReason);
		}
		return false;
	}

	mediaManifest->SetAuthorityIdentity(mAuthorityRole, mAuthorityUserId, mAuthorityTokenId, mAuthorityKeyId);
	mediaManifest->SetPlaybackLocator(TEXT("local_path"), artifact->GetOutputFilePath(), mAuthorityTokenId, 0);

	if (not mediaManifest->SaveToFile(metadataFilePath, outErrorReason))
	{
		artifact->SetMediaManifest(mediaManifest, false, outErrorReason);
		if (mbDeleteVideoIfMetadataMissing)
		{
			const FString cleanupReason = outErrorReason;
			DeleteVideoFileForMissingMetadata(artifact, cleanupReason, outErrorReason);
		}
		return false;
	}

	FString validationError;
	if (not mediaManifest->ValidateManifest(validationError))
	{
		outErrorReason = validationError;
		artifact->SetMediaManifest(mediaManifest, false, outErrorReason);
		if (mbDeleteVideoIfMetadataMissing)
		{
			const FString cleanupReason = outErrorReason;
			DeleteVideoFileForMissingMetadata(artifact, cleanupReason, outErrorReason);
		}
		return false;
	}

	mLastManifest = mediaManifest;
	artifact->SetMediaManifest(mediaManifest, true, FString());

	FString registryErrorReason;
	if (not RegisterManifest(mediaManifest, registryErrorReason))
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("UVdjmRecordMetadataStore::BuildAndSaveManifest - Failed to register manifest. Reason=%s"),
			*registryErrorReason);
	}

	FString publishEnqueueErrorReason;
	if (not EnqueueArtifactMediaPublish(artifact, mediaManifest, publishEnqueueErrorReason))
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("UVdjmRecordMetadataStore::BuildAndSaveManifest - Media publish enqueue skipped or failed. Reason=%s"),
			*publishEnqueueErrorReason);
	}
	return true;
}

bool UVdjmRecordMetadataStore::EnqueueArtifactMediaPublish(
	UVdjmRecordArtifact* artifact,
	UVdjmRecordMediaManifest* mediaManifest,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	if (not IsValid(artifact) || not IsValid(mediaManifest))
	{
		outErrorReason = TEXT("Artifact or media manifest is invalid.");
		return false;
	}

#if PLATFORM_ANDROID || defined(__RESHARPER__)
	const int32 jobId = mNextPostProcessJobId++;
	artifact->SetMediaPublishResult(EVdjmRecordMediaPublishStatus::EPublishing, FString(), FString());
	mediaManifest->SetMediaPublishResult(
		EVdjmRecordMediaPublishStatus::EPublishing,
		FString(),
		FString(),
		FString(),
		FString());

	FString pendingManifestSaveError;
	if (not mediaManifest->SaveToFile(mediaManifest->GetMetadataFilePath(), pendingManifestSaveError))
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("UVdjmRecordMetadataStore::EnqueueArtifactMediaPublish - Failed to save pending publish state. Reason=%s"),
			*pendingManifestSaveError);
	}

	FVdjmRecordMediaPublishJobPayload jobPayload;
	jobPayload.JobId = jobId;
	jobPayload.SourceFilePath = artifact->GetOutputFilePath();
	jobPayload.DisplayName = FPaths::GetCleanFilename(artifact->GetOutputFilePath());
	jobPayload.RelativePath = TEXT("Movies/VdjmRecorder");

	++mActiveMediaPublishJobCount;
	mLastMediaPublishStatus = EVdjmRecordMediaPublishStatus::EPublishing;
	mLastPublishedContentUri.Reset();
	mLastMediaPublishErrorReason.Reset();

	TWeakObjectPtr<UVdjmRecordMetadataStore> weakStore(this);
	TWeakObjectPtr<UVdjmRecordArtifact> weakArtifact(artifact);
	TWeakObjectPtr<UVdjmRecordMediaManifest> weakManifest(mediaManifest);

	Async(EAsyncExecution::ThreadPool, [jobPayload, weakStore, weakArtifact, weakManifest]()
	{
		FVdjmRecordAndroidMediaStorePublishResult publishResult;
		const bool bPublished = VdjmRecordAndroidMediaStore::PublishVideoFileToMediaStore(
			jobPayload.SourceFilePath,
			jobPayload.DisplayName,
			jobPayload.RelativePath,
			publishResult);

		const EVdjmRecordMediaPublishStatus publishStatus = bPublished
			? EVdjmRecordMediaPublishStatus::EPublished
			: EVdjmRecordMediaPublishStatus::EFailed;
		const FString publishedContentUri = publishResult.PublishedContentUri;
		const FString publishedDisplayName = publishResult.DisplayName;
		const FString publishedRelativePath = publishResult.RelativePath;
		const FString publishErrorReason = bPublished ? FString() : publishResult.ErrorReason;

		AsyncTask(ENamedThreads::GameThread, [
			weakStore,
			weakArtifact,
			weakManifest,
			jobId = jobPayload.JobId,
			publishStatus,
			publishedContentUri,
			publishedDisplayName,
			publishedRelativePath,
			publishErrorReason]()
		{
			if (UVdjmRecordMetadataStore* store = weakStore.Get())
			{
				store->CompleteArtifactMediaPublishOnGameThread(
					jobId,
					weakArtifact.Get(),
					weakManifest.Get(),
					publishStatus,
					publishedContentUri,
					publishedDisplayName,
					publishedRelativePath,
					publishErrorReason);
			}
		});
	});
	return true;
#else
	artifact->SetMediaPublishResult(
		EVdjmRecordMediaPublishStatus::ESkippedUnsupportedPlatform,
		FString(),
		FString());
	mediaManifest->SetMediaPublishResult(
		EVdjmRecordMediaPublishStatus::ESkippedUnsupportedPlatform,
		FString(),
		FString(),
		FString(),
		FString());
	mLastMediaPublishStatus = EVdjmRecordMediaPublishStatus::ESkippedUnsupportedPlatform;
	mLastPublishedContentUri.Reset();
	mLastMediaPublishErrorReason.Reset();
	return true;
#endif
}

void UVdjmRecordMetadataStore::CompleteArtifactMediaPublishOnGameThread(
	int32 jobId,
	UVdjmRecordArtifact* artifact,
	UVdjmRecordMediaManifest* mediaManifest,
	EVdjmRecordMediaPublishStatus publishStatus,
	const FString& publishedContentUri,
	const FString& publishedDisplayName,
	const FString& publishedRelativePath,
	const FString& publishErrorReason)
{
	(void)jobId;

	mActiveMediaPublishJobCount = FMath::Max(0, mActiveMediaPublishJobCount - 1);
	++mCompletedMediaPublishJobCount;
	mLastMediaPublishStatus = publishStatus;
	mLastPublishedContentUri = publishedContentUri;
	mLastMediaPublishErrorReason = publishErrorReason;

	if (IsValid(artifact))
	{
		artifact->SetMediaPublishResult(publishStatus, publishedContentUri, publishErrorReason);
	}

	if (IsValid(mediaManifest))
	{
		if (publishStatus == EVdjmRecordMediaPublishStatus::EPublished)
		{
			mediaManifest->SetPlaybackLocator(
				TEXT("android_content_uri"),
				publishedContentUri,
				mAuthorityTokenId,
				0);
		}

		mediaManifest->SetMediaPublishResult(
			publishStatus,
			publishedContentUri,
			publishedDisplayName,
			publishedRelativePath,
			publishErrorReason);

		FString saveErrorReason;
		if (not mediaManifest->SaveToFile(mediaManifest->GetMetadataFilePath(), saveErrorReason))
		{
			mLastMediaPublishStatus = EVdjmRecordMediaPublishStatus::EFailed;
			mLastMediaPublishErrorReason = FString::Printf(
				TEXT("Media publish completed but failed to save manifest update. Reason=%s"),
				*saveErrorReason);
			mediaManifest->SetMediaPublishResult(
				EVdjmRecordMediaPublishStatus::EFailed,
				publishedContentUri,
				publishedDisplayName,
				publishedRelativePath,
				mLastMediaPublishErrorReason);
			if (IsValid(artifact))
			{
				artifact->SetMediaPublishResult(
					EVdjmRecordMediaPublishStatus::EFailed,
					publishedContentUri,
					mLastMediaPublishErrorReason);
			}
		}

		FString registryErrorReason;
		if (not RegisterManifest(mediaManifest, registryErrorReason))
		{
			UE_LOG(LogVdjmRecorderCore, Warning,
				TEXT("UVdjmRecordMetadataStore::CompleteArtifactMediaPublishOnGameThread - Failed to update media registry. Reason=%s"),
				*registryErrorReason);
		}
	}

	if (publishStatus == EVdjmRecordMediaPublishStatus::EPublished)
	{
		UE_LOG(LogVdjmRecorderCore, Log,
			TEXT("UVdjmRecordMetadataStore::CompleteArtifactMediaPublishOnGameThread - Media publish completed. Uri=%s"),
			*publishedContentUri);
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Warning,
			TEXT("UVdjmRecordMetadataStore::CompleteArtifactMediaPublishOnGameThread - Media publish failed. Reason=%s"),
			*publishErrorReason);
	}
}

bool UVdjmRecordMetadataStore::BuildRegistryEntryFromManifest(
	const UVdjmRecordMediaManifest* mediaManifest,
	FVdjmRecordMediaRegistryEntry& outEntry,
	FString& outErrorReason) const
{
	outErrorReason.Reset();
	outEntry = FVdjmRecordMediaRegistryEntry();

	if (not IsValid(mediaManifest))
	{
		outErrorReason = TEXT("Media manifest is invalid.");
		return false;
	}

	outEntry.RecordId = mediaManifest->GetRecordId();
	outEntry.OutputFilePath = mediaManifest->GetOutputFilePath();
	outEntry.MetadataFilePath = mediaManifest->GetMetadataFilePath();
	outEntry.PlaybackLocatorType = mediaManifest->GetPlaybackLocatorType();
	outEntry.PlaybackLocator = mediaManifest->GetPlaybackLocator();
	outEntry.ThumbnailFilePath = mediaManifest->GetThumbnailFilePath();
	outEntry.PreviewClipFilePath = mediaManifest->GetPreviewClipFilePath();
	outEntry.PreviewClipMimeType = mediaManifest->GetPreviewClipMimeType();
	outEntry.PreviewErrorReason = mediaManifest->GetPreviewErrorReason();
	outEntry.PublishedContentUri = mediaManifest->GetPublishedContentUri();
	outEntry.PublishedDisplayName = mediaManifest->GetPublishedDisplayName();
	outEntry.PublishedRelativePath = mediaManifest->GetPublishedRelativePath();
	outEntry.VideoMimeType = mediaManifest->GetVideoMimeType();
	outEntry.LastErrorReason = mediaManifest->GetMediaPublishErrorReason();
	outEntry.CreatedUnixTime = mediaManifest->GetCreatedUnixTime();
	outEntry.FileSizeBytes = mediaManifest->GetFileSizeBytes();
	outEntry.RecordedFrameCount = mediaManifest->GetRecordedFrameCount();
	outEntry.VideoWidth = mediaManifest->GetVideoWidth();
	outEntry.VideoHeight = mediaManifest->GetVideoHeight();
	outEntry.VideoFrameRate = mediaManifest->GetVideoFrameRate();
	outEntry.VideoBitrate = mediaManifest->GetVideoBitrate();
	outEntry.PreviewStartTimeSec = mediaManifest->GetPreviewStartTimeSec();
	outEntry.PreviewDurationSec = mediaManifest->GetPreviewDurationSec();
	outEntry.TargetPlatform = mediaManifest->GetTargetPlatform();
	outEntry.MediaPublishStatus = mediaManifest->GetMediaPublishStatus();
	outEntry.PreviewStatus = mediaManifest->GetPreviewStatus();

	if (outEntry.RecordId.IsEmpty())
	{
		outEntry.RecordId = FPaths::GetBaseFilename(outEntry.MetadataFilePath, false);
	}

	if (outEntry.RecordId.IsEmpty())
	{
		outErrorReason = TEXT("Media registry entry record id is empty.");
		return false;
	}

	RefreshRegistryEntryFileState(outEntry);
	return true;
}

bool UVdjmRecordMetadataStore::BuildRegistryEntryFromManifestFile(
	const FString& metadataFilePath,
	FVdjmRecordMediaRegistryEntry& outEntry,
	FString& outErrorReason) const
{
	outErrorReason.Reset();
	outEntry = FVdjmRecordMediaRegistryEntry();

	const FString normalizedMetadataFilePath = FPaths::ConvertRelativePathToFull(metadataFilePath);
	if (normalizedMetadataFilePath.IsEmpty())
	{
		outErrorReason = TEXT("Metadata file path is empty.");
		return false;
	}

	FString manifestJsonString;
	if (not FFileHelper::LoadFileToString(manifestJsonString, *normalizedMetadataFilePath))
	{
		outErrorReason = FString::Printf(TEXT("Failed to load manifest json. Path=%s"), *normalizedMetadataFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> rootObject;
	const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(manifestJsonString);
	if (not FJsonSerializer::Deserialize(reader, rootObject) || not rootObject.IsValid())
	{
		outErrorReason = FString::Printf(TEXT("Failed to parse manifest json. Path=%s"), *normalizedMetadataFilePath);
		return false;
	}

	const TSharedPtr<FJsonObject>* recordObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* mediaObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* playbackObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* previewObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* publicationObjectPtr = nullptr;
	rootObject->TryGetObjectField(TEXT("record"), recordObjectPtr);
	rootObject->TryGetObjectField(TEXT("media"), mediaObjectPtr);
	rootObject->TryGetObjectField(TEXT("playback"), playbackObjectPtr);
	rootObject->TryGetObjectField(TEXT("preview"), previewObjectPtr);
	rootObject->TryGetObjectField(TEXT("publication"), publicationObjectPtr);

	const TSharedPtr<FJsonObject> recordObject = recordObjectPtr != nullptr ? *recordObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> mediaObject = mediaObjectPtr != nullptr ? *mediaObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> playbackObject = playbackObjectPtr != nullptr ? *playbackObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> previewObject = previewObjectPtr != nullptr ? *previewObjectPtr : nullptr;
	const TSharedPtr<FJsonObject> publicationObject = publicationObjectPtr != nullptr ? *publicationObjectPtr : nullptr;

	outEntry.RecordId = GetJsonObjectStringField(recordObject, TEXT("record_id"));
	outEntry.CreatedUnixTime = GetJsonObjectInt64Field(recordObject, TEXT("created_unix_time"));
	outEntry.OutputFilePath = GetJsonObjectStringField(mediaObject, TEXT("output_file_path"));
	outEntry.MetadataFilePath = GetJsonObjectStringField(mediaObject, TEXT("metadata_file_path"), normalizedMetadataFilePath);
	outEntry.FileSizeBytes = GetJsonObjectInt64Field(mediaObject, TEXT("file_size_bytes"), -1);
	outEntry.RecordedFrameCount = GetJsonObjectInt32Field(mediaObject, TEXT("recorded_frame_count"));
	outEntry.TargetPlatform = ParseEnumValueString(
		GetJsonObjectStringField(mediaObject, TEXT("platform")),
		EVdjmRecordEnvPlatform::EDefault);
	outEntry.VideoMimeType = GetJsonObjectStringField(mediaObject, TEXT("mime_type"));
	outEntry.VideoWidth = GetJsonObjectInt32Field(mediaObject, TEXT("width"));
	outEntry.VideoHeight = GetJsonObjectInt32Field(mediaObject, TEXT("height"));
	outEntry.VideoFrameRate = GetJsonObjectInt32Field(mediaObject, TEXT("fps"));
	outEntry.VideoBitrate = GetJsonObjectInt32Field(mediaObject, TEXT("bitrate"));
	outEntry.PlaybackLocatorType = GetJsonObjectStringField(playbackObject, TEXT("locator_type"));
	outEntry.PlaybackLocator = GetJsonObjectStringField(playbackObject, TEXT("locator"));
	outEntry.ThumbnailFilePath = GetJsonObjectStringField(previewObject, TEXT("thumbnail_file_path"));
	outEntry.PreviewClipFilePath = GetJsonObjectStringField(previewObject, TEXT("preview_clip_file_path"));
	outEntry.PreviewClipMimeType = GetJsonObjectStringField(previewObject, TEXT("preview_clip_mime_type"), TEXT("video/mp4"));
	outEntry.PreviewStartTimeSec = GetJsonObjectDoubleField(previewObject, TEXT("start_time_sec"));
	outEntry.PreviewDurationSec = GetJsonObjectDoubleField(previewObject, TEXT("duration_sec"), 3.0);
	outEntry.PreviewStatus = ParseEnumValueString(
		GetJsonObjectStringField(previewObject, TEXT("status")),
		EVdjmRecordMediaPreviewStatus::EReady);
	outEntry.PreviewErrorReason = GetJsonObjectStringField(previewObject, TEXT("error_reason"));
	outEntry.MediaPublishStatus = ParseEnumValueString(
		GetJsonObjectStringField(publicationObject, TEXT("status")),
		EVdjmRecordMediaPublishStatus::ENotStarted);
	outEntry.PublishedContentUri = GetJsonObjectStringField(publicationObject, TEXT("content_uri"));
	outEntry.PublishedDisplayName = GetJsonObjectStringField(publicationObject, TEXT("display_name"));
	outEntry.PublishedRelativePath = GetJsonObjectStringField(publicationObject, TEXT("relative_path"));
	outEntry.LastErrorReason = GetJsonObjectStringField(publicationObject, TEXT("error_reason"));

	if (outEntry.MetadataFilePath.IsEmpty())
	{
		outEntry.MetadataFilePath = normalizedMetadataFilePath;
	}

	if (outEntry.RecordId.IsEmpty())
	{
		outEntry.RecordId = FPaths::GetBaseFilename(normalizedMetadataFilePath, false);
	}

	if (outEntry.RecordId.IsEmpty())
	{
		outErrorReason = TEXT("Manifest record id is empty.");
		return false;
	}

	RefreshRegistryEntryFileState(outEntry);
	return true;
}

void UVdjmRecordMetadataStore::RefreshRegistryEntryFileState(FVdjmRecordMediaRegistryEntry& entry) const
{
	SetRegistryEntryFileState(entry);
}

bool UVdjmRecordMetadataStore::UpsertRegistryEntry(const FVdjmRecordMediaRegistryEntry& entry)
{
	if (entry.RecordId.IsEmpty() && entry.MetadataFilePath.IsEmpty())
	{
		return false;
	}

	for (FVdjmRecordMediaRegistryEntry& existingEntry : mRegistryEntries)
	{
		const bool bSameRecordId = not entry.RecordId.IsEmpty() &&
			existingEntry.RecordId.Equals(entry.RecordId, ESearchCase::IgnoreCase);
		const bool bSameMetadataFilePath = not entry.MetadataFilePath.IsEmpty() &&
			existingEntry.MetadataFilePath.Equals(entry.MetadataFilePath, ESearchCase::IgnoreCase);
		if (bSameRecordId || bSameMetadataFilePath)
		{
			existingEntry = entry;
			return true;
		}
	}

	mRegistryEntries.Add(entry);
	return true;
}

FString UVdjmRecordMetadataStore::GetManifestDirectoryPath() const
{
	const EVdjmRecordEnvPlatform targetPlatform = VdjmRecordUtils::Platforms::GetTargetPlatform();
	FString manifestDirectoryPath = VdjmRecordUtils::FilePaths::GetPlatformRecordBaseDir(targetPlatform);
	FPaths::NormalizeFilename(manifestDirectoryPath);
	while (manifestDirectoryPath.EndsWith(TEXT("/")))
	{
		manifestDirectoryPath.LeftChopInline(1, EAllowShrinking::No);
	}
	return manifestDirectoryPath;
}

FString UVdjmRecordMetadataStore::GetRegistryFilePath() const
{
	const FString manifestDirectoryPath = GetManifestDirectoryPath();
	if (manifestDirectoryPath.IsEmpty())
	{
		return FString();
	}

	return FPaths::Combine(manifestDirectoryPath, TEXT("VdjmRecordMediaRegistry.json"));
}

FVdjmRecordMediaPostProcessSnapshot UVdjmRecordMetadataStore::GetMediaPostProcessSnapshot() const
{
	FVdjmRecordMediaPostProcessSnapshot snapshot;
	snapshot.bIsPostProcessingMedia = IsPostProcessingMedia();
	snapshot.ActiveMediaPublishJobCount = mActiveMediaPublishJobCount;
	snapshot.CompletedMediaPublishJobCount = mCompletedMediaPublishJobCount;
	snapshot.LastMediaPublishStatus = mLastMediaPublishStatus;
	snapshot.LastPublishedContentUri = mLastPublishedContentUri;
	snapshot.LastErrorReason = mLastMediaPublishErrorReason;
	return snapshot;
}

UVdjmRecordMediaPreviewPlayer* UVdjmRecordMediaPreviewPlayer::CreateMediaPreviewPlayer(
	UObject* worldContextObject,
	UMediaPlayer* mediaPlayer)
{
	if (worldContextObject == nullptr || mediaPlayer == nullptr)
	{
		return nullptr;
	}

	UObject* outerObject = worldContextObject;
	if (UWorld* world = worldContextObject->GetWorld())
	{
		outerObject = worldContextObject;
	}

	UVdjmRecordMediaPreviewPlayer* previewPlayer = NewObject<UVdjmRecordMediaPreviewPlayer>(outerObject);
	if (not IsValid(previewPlayer))
	{
		return nullptr;
	}

	previewPlayer->mCachedWorld = worldContextObject->GetWorld();
	previewPlayer->mMediaPlayer = mediaPlayer;
	return previewPlayer;
}

bool UVdjmRecordMediaPreviewPlayer::StartPreviewFromManifest(
	UMediaPlayer* mediaPlayer,
	UVdjmRecordMediaManifest* mediaManifest,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	if (not IsValid(mediaManifest))
	{
		outErrorReason = TEXT("Media manifest is invalid.");
		return false;
	}

	FString sourceType;
	const FString source = ResolvePreviewSourceFromManifest(mediaManifest, sourceType);
	return StartPreviewInternal(
		mediaPlayer,
		source,
		sourceType,
		mediaManifest->GetPreviewStartTimeSec(),
		mediaManifest->GetPreviewDurationSec(),
		outErrorReason);
}

bool UVdjmRecordMediaPreviewPlayer::StartPreviewFromRegistryEntry(
	UMediaPlayer* mediaPlayer,
	const FVdjmRecordMediaRegistryEntry& registryEntry,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	FString sourceType;
	const FString source = ResolvePreviewSourceFromRegistryEntry(registryEntry, sourceType);
	return StartPreviewInternal(
		mediaPlayer,
		source,
		sourceType,
		registryEntry.PreviewStartTimeSec,
		registryEntry.PreviewDurationSec,
		outErrorReason);
}

void UVdjmRecordMediaPreviewPlayer::StopPreview(bool bCloseMedia)
{
	mbPreviewActive = false;
	mbPreviewOpened = false;
	mbPendingInitialSeek = false;
	UnbindMediaPlayerEvents();

	if (IsValid(mMediaPlayer))
	{
		mMediaPlayer->Pause();
		if (bCloseMedia)
		{
			mMediaPlayer->Close();
		}
	}
}

bool UVdjmRecordMediaPreviewPlayer::RestartPreview(FString& outErrorReason)
{
	outErrorReason.Reset();

	if (not IsValid(mMediaPlayer))
	{
		outErrorReason = TEXT("Media player is invalid.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	if (mCurrentSource.IsEmpty())
	{
		outErrorReason = TEXT("Preview source is empty.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	mbPreviewActive = true;
	SeekPreviewStartAndPlay();
	return true;
}

void UVdjmRecordMediaPreviewPlayer::Tick(float deltaTime)
{
	(void)deltaTime;

	if (not mbPreviewActive || not IsValid(mMediaPlayer))
	{
		return;
	}

	if (mbPendingInitialSeek && mMediaPlayer->IsReady())
	{
		SeekPreviewStartAndPlay();
		return;
	}

	if (not mbPreviewOpened)
	{
		return;
	}

	const double currentTimeSec = mMediaPlayer->GetTime().GetTotalSeconds();
	if (currentTimeSec >= mPreviewEndTimeSec)
	{
		SeekPreviewStartAndPlay();
	}
}

bool UVdjmRecordMediaPreviewPlayer::IsTickable() const
{
	return not HasAnyFlags(RF_ClassDefaultObject) && mbPreviewActive && IsValid(mMediaPlayer);
}

UWorld* UVdjmRecordMediaPreviewPlayer::GetTickableGameObjectWorld() const
{
	return mCachedWorld.Get();
}

TStatId UVdjmRecordMediaPreviewPlayer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVdjmRecordMediaPreviewPlayer, STATGROUP_Tickables);
}

UWorld* UVdjmRecordMediaPreviewPlayer::GetWorld() const
{
	if (UWorld* cachedWorld = mCachedWorld.Get())
	{
		return cachedWorld;
	}

	return Super::GetWorld();
}

void UVdjmRecordMediaPreviewPlayer::HandleMediaOpened(FString openedUrl)
{
	(void)openedUrl;
	mbPreviewOpened = true;
	SeekPreviewStartAndPlay();
}

void UVdjmRecordMediaPreviewPlayer::HandleMediaOpenFailed(FString failedUrl)
{
	mLastErrorReason = FString::Printf(TEXT("Failed to open preview media. Source=%s"), *failedUrl);
	mbPreviewActive = false;
	mbPreviewOpened = false;
	mbPendingInitialSeek = false;
}

void UVdjmRecordMediaPreviewPlayer::HandleMediaEndReached()
{
	if (mbPreviewActive)
	{
		SeekPreviewStartAndPlay();
	}
}

bool UVdjmRecordMediaPreviewPlayer::StartPreviewInternal(
	UMediaPlayer* mediaPlayer,
	const FString& source,
	const FString& sourceType,
	double previewStartTimeSec,
	double previewDurationSec,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	if (mediaPlayer != nullptr)
	{
		mMediaPlayer = mediaPlayer;
		if (UWorld* mediaWorld = mediaPlayer->GetWorld())
		{
			mCachedWorld = mediaWorld;
		}
	}

	if (not IsValid(mMediaPlayer))
	{
		outErrorReason = TEXT("Media player is invalid.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	const FString trimmedSource = source.TrimStartAndEnd();
	if (trimmedSource.IsEmpty())
	{
		outErrorReason = TEXT("Preview media source is empty.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	StopPreview(true);
	mCurrentSource = trimmedSource;
	mCurrentSourceType = sourceType.TrimStartAndEnd();
	mPreviewStartTimeSec = FMath::Max(0.0, previewStartTimeSec);
	const double safePreviewDurationSec = FMath::Max(0.1, previewDurationSec);
	mPreviewEndTimeSec = mPreviewStartTimeSec + safePreviewDurationSec;
	mLastErrorReason.Reset();
	mbPreviewActive = true;
	mbPreviewOpened = false;
	mbPendingInitialSeek = true;

	BindMediaPlayerEvents();
	return OpenCurrentSource(outErrorReason);
}

bool UVdjmRecordMediaPreviewPlayer::OpenCurrentSource(FString& outErrorReason)
{
	outErrorReason.Reset();

	if (not IsValid(mMediaPlayer))
	{
		outErrorReason = TEXT("Media player is invalid.");
		mLastErrorReason = outErrorReason;
		return false;
	}

	mMediaPlayer->SetLooping(false);

	const bool bShouldOpenAsUrl = IsUrlLikeMediaSource(mCurrentSource) ||
		mCurrentSourceType.Equals(TEXT("content_uri"), ESearchCase::IgnoreCase) ||
		mCurrentSourceType.Equals(TEXT("remote_url"), ESearchCase::IgnoreCase);
	const bool bOpenRequested = bShouldOpenAsUrl
		? mMediaPlayer->OpenUrl(mCurrentSource)
		: mMediaPlayer->OpenFile(mCurrentSource);

	if (not bOpenRequested)
	{
		outErrorReason = FString::Printf(TEXT("Failed to request preview media open. Source=%s"), *mCurrentSource);
		mLastErrorReason = outErrorReason;
		StopPreview(false);
		return false;
	}

	if (mMediaPlayer->IsReady())
	{
		mbPreviewOpened = true;
		SeekPreviewStartAndPlay();
	}

	return true;
}

void UVdjmRecordMediaPreviewPlayer::BindMediaPlayerEvents()
{
	if (not IsValid(mMediaPlayer))
	{
		return;
	}

	mMediaPlayer->OnMediaOpened.RemoveDynamic(this, &UVdjmRecordMediaPreviewPlayer::HandleMediaOpened);
	mMediaPlayer->OnMediaOpenFailed.RemoveDynamic(this, &UVdjmRecordMediaPreviewPlayer::HandleMediaOpenFailed);
	mMediaPlayer->OnEndReached.RemoveDynamic(this, &UVdjmRecordMediaPreviewPlayer::HandleMediaEndReached);
	mMediaPlayer->OnMediaOpened.AddDynamic(this, &UVdjmRecordMediaPreviewPlayer::HandleMediaOpened);
	mMediaPlayer->OnMediaOpenFailed.AddDynamic(this, &UVdjmRecordMediaPreviewPlayer::HandleMediaOpenFailed);
	mMediaPlayer->OnEndReached.AddDynamic(this, &UVdjmRecordMediaPreviewPlayer::HandleMediaEndReached);
}

void UVdjmRecordMediaPreviewPlayer::UnbindMediaPlayerEvents()
{
	if (not IsValid(mMediaPlayer))
	{
		return;
	}

	mMediaPlayer->OnMediaOpened.RemoveDynamic(this, &UVdjmRecordMediaPreviewPlayer::HandleMediaOpened);
	mMediaPlayer->OnMediaOpenFailed.RemoveDynamic(this, &UVdjmRecordMediaPreviewPlayer::HandleMediaOpenFailed);
	mMediaPlayer->OnEndReached.RemoveDynamic(this, &UVdjmRecordMediaPreviewPlayer::HandleMediaEndReached);
}

void UVdjmRecordMediaPreviewPlayer::SeekPreviewStartAndPlay()
{
	if (not IsValid(mMediaPlayer))
	{
		return;
	}

	const FTimespan startTime = FTimespan::FromSeconds(mPreviewStartTimeSec);
	if (mMediaPlayer->SupportsSeeking())
	{
		mMediaPlayer->Seek(startTime);
	}

	mMediaPlayer->Play();
	mbPreviewOpened = true;
	mbPendingInitialSeek = false;
}

void UVdjmRecordMediaPreviewPlayer::ResetPreviewState()
{
	StopPreview(false);
	mCurrentSource.Reset();
	mCurrentSourceType.Reset();
	mLastErrorReason.Reset();
	mPreviewStartTimeSec = 0.0;
	mPreviewEndTimeSec = 3.0;
}

FString UVdjmRecordMetadataStore::MakeMetadataFilePathForArtifact(const UVdjmRecordArtifact* artifact) const
{
	if (not IsValid(artifact))
	{
		return FString();
	}

	const FString outputFilePath = artifact->GetOutputFilePath();
	if (outputFilePath.IsEmpty())
	{
		return FString();
	}

	const FString normalizedOutputPath = FPaths::ConvertRelativePathToFull(outputFilePath);
	const FString outputDirectory = FPaths::GetPath(normalizedOutputPath);
	const FString outputBaseName = FPaths::GetBaseFilename(normalizedOutputPath, false);
	if (outputDirectory.IsEmpty() || outputBaseName.IsEmpty())
	{
		return FString();
	}

	return FPaths::Combine(outputDirectory, outputBaseName + TEXT(".vdjm.json"));
}

bool UVdjmRecordMetadataStore::DeleteVideoFileForMissingMetadata(
	UVdjmRecordArtifact* artifact,
	const FString& reason,
	FString& outErrorReason) const
{
	if (not IsValid(artifact))
	{
		outErrorReason = FString::Printf(TEXT("%s Delete skipped because artifact is invalid."), *reason);
		return false;
	}

	const FString outputFilePath = artifact->GetOutputFilePath();
	if (outputFilePath.IsEmpty())
	{
		outErrorReason = FString::Printf(TEXT("%s Delete skipped because output file path is empty."), *reason);
		return false;
	}

	const FString normalizedOutputPath = FPaths::ConvertRelativePathToFull(outputFilePath);
	const bool bFileExists = IFileManager::Get().FileExists(*normalizedOutputPath);
	if (bFileExists && not IFileManager::Get().Delete(*normalizedOutputPath, false, true))
	{
		outErrorReason = FString::Printf(
			TEXT("%s Failed to delete video without metadata. Path=%s"),
			*reason,
			*normalizedOutputPath);
		return false;
	}

	const FString deletionReason = FString::Printf(
		TEXT("%s Video without metadata was deleted. Path=%s"),
		*reason,
		*normalizedOutputPath);
	artifact->MarkOutputDeleted(deletionReason);
	outErrorReason = deletionReason;
	UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordMetadataStore::DeleteVideoFileForMissingMetadata - %s"), *deletionReason);
	return true;
}

void UVdjmRecordResource::BeginDestroy()
{
	UObject::BeginDestroy();
	ReleaseResources();
}

bool UVdjmRecordResource::InitializeResource(UVdjmRecordEnvResolver* resolver)
{
	UVdjmRecordEnvResolver* inResolver = resolver;
	if (not LinkedOwnerBridge.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::InitializeResource - resolver's LinkedOwnerBridge is invalid."));
		return false;
	}

	if (inResolver == nullptr)
	{
		if (LinkedResolver.IsValid())
		{
			inResolver = LinkedResolver.Get();
			UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecord Resource::InitializeResourceExtended - Using existing LinkedResolver."));
		}
		else
		{
			UE_LOG(LogVdjmRecorderCore, Error, TEXT("UVdjmRecordResource::InitializeResourceExtended - resolver is null."));
			return false;
		}
	}

	if (not inResolver->HasResolved())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordResource::InitializeResource - Existing LinkedResolver has not resolved yet. This may lead to incomplete or default configuration being used."));
		LinkedResolver = nullptr; // Clear the invalid resolver reference
		return false;
	}
	LinkedResolver = inResolver;

	const FVdjmEncoderInitRequest* resolvedIniRequest = LinkedResolver->TryGetResolvedEncoderInitRequest();
	if (resolvedIniRequest == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("UVdjmRecordResource::InitializeResourceExtended - Resolved init request is null."));
		return false;
	}
	OriginResolution = FIntPoint(
		resolvedIniRequest->VideoConfig.Width,
		resolvedIniRequest->VideoConfig.Height);

	TextureResolution = OriginResolution;
	FinalFrameRate = resolvedIniRequest->VideoConfig.FrameRate;
	FinalBitrate = resolvedIniRequest->VideoConfig.Bitrate;
	FinalPixelFormat = resolvedIniRequest->VideoConfig.PixelFormat;
	FinalFilePath = NormalizeRecordOutputPathForRuntime(
		resolvedIniRequest->OutputConfig.OutputFilePath,
		LinkedOwnerBridge.IsValid()
			? LinkedOwnerBridge->GetTargetPlatform()
			: VdjmRecordUtils::Platforms::GetTargetPlatform());

	OnResourceReadyForPostInit.Broadcast(this);
	OnResourceReadyForFilePath.Broadcast(this, FinalFilePath);
	return true;
}

bool UVdjmRecordResource::RefreshResolvedRuntimeConfigFromResolver()
{
	if (!LinkedResolver.IsValid())
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordResource::RefreshResolvedRuntimeConfigFromResolver - LinkedResolver is invalid."));
		return false;
	}

	const FVdjmEncoderInitRequest* resolvedInitRequest = LinkedResolver->TryGetResolvedEncoderInitRequest();
	if (resolvedInitRequest == nullptr)
	{
		UE_LOG(LogVdjmRecorderCore, Warning, TEXT("UVdjmRecordResource::RefreshResolvedRuntimeConfigFromResolver - Resolved init request is null."));
		return false;
	}

	int32 safeBitrate = 0;
	if (!VdjmRecordUtils::Validations::DbcValidateBitrate(
		resolvedInitRequest->VideoConfig.Bitrate,
		safeBitrate,
		TEXT("UVdjmRecordResource::RefreshResolvedRuntimeConfigFromResolver")))
	{
		return false;
	}

	FString safeOutputFilePath;
	if (!VdjmRecordUtils::Validations::DbcValidateOutputFilePath(
		resolvedInitRequest->OutputConfig.OutputFilePath,
		safeOutputFilePath,
		TEXT("UVdjmRecordResource::RefreshResolvedRuntimeConfigFromResolver")))
	{
		return false;
	}

	FinalFrameRate = FMath::Max(1, resolvedInitRequest->VideoConfig.FrameRate);
	FinalBitrate = safeBitrate;
	FinalFilePath = NormalizeRecordOutputPathForRuntime(
		safeOutputFilePath,
		LinkedOwnerBridge.IsValid()
			? LinkedOwnerBridge->GetTargetPlatform()
			: VdjmRecordUtils::Platforms::GetTargetPlatform());
	OnResourceReadyForFilePath.Broadcast(this, FinalFilePath);
	return true;
}

bool UVdjmRecordResource::UpdateFinalFilePathFromResolver()
{
	return RefreshResolvedRuntimeConfigFromResolver();
}

bool UVdjmRecordResource::BuildEncoderSnapshot(FVdjmRecordEncoderSnapshot& outSnapshot) const
{
	outSnapshot.Clear();

	outSnapshot.TargetPlatform = LinkedOwnerBridge.IsValid()
		? LinkedOwnerBridge->GetTargetPlatform()
		: VdjmRecordUtils::Platforms::GetTargetPlatform();
	outSnapshot.VideoConfig.OutputFilePath = NormalizeRecordOutputPathForRuntime(
		FinalFilePath,
		outSnapshot.TargetPlatform);
	outSnapshot.VideoConfig.VideoWidth = OriginResolution.X;
	outSnapshot.VideoConfig.VideoHeight = OriginResolution.Y;
	outSnapshot.VideoConfig.VideoBitrate = FinalBitrate;
	outSnapshot.VideoConfig.VideoFPS = FinalFrameRate;
	outSnapshot.VideoConfig.GraphicBackend = EVdjmRecordGraphicBackend::EUnknown;

	if (LinkedResolver.IsValid())
	{
		const UVdjmRecordEnvResolver* resolver = LinkedResolver.Get();
		if (const FVdjmEncoderInitRequestVideo* videoConfig = resolver->TryGetResolvedVideoConfig())
		{
			outSnapshot.VideoConfig.MimeType = videoConfig->MimeType.IsEmpty()
				? outSnapshot.VideoConfig.MimeType
				: videoConfig->MimeType.ToLower();
			outSnapshot.VideoConfig.VideoIntervalSec = FMath::Max(1, videoConfig->KeyframeInterval);
		}

		if (const FVdjmEncoderInitRequestAudio* audioConfig = resolver->TryGetResolvedAudioConfig())
		{
			outSnapshot.AudioConfig.bEnableAudio = audioConfig->bEnableInternalAudioCapture;
			outSnapshot.AudioConfig.AudioSampleRate = audioConfig->SampleRate;
			outSnapshot.AudioConfig.AudioChannelCount = audioConfig->ChannelCount;
			outSnapshot.AudioConfig.AudioBitrate = audioConfig->Bitrate;
			outSnapshot.AudioConfig.AudioAacProfile = audioConfig->AacProfile;
			outSnapshot.AudioConfig.AudioMimeType = audioConfig->AudioMimeType.IsEmpty()
				? outSnapshot.AudioConfig.AudioMimeType
				: audioConfig->AudioMimeType.ToLower();
			outSnapshot.AudioConfig.AudioSourceId = audioConfig->SourceSubMixName.ToString();
		}

		if (const FVdjmEncoderInitRequestRuntimePolicy* runtimePolicy = resolver->TryGetResolvedRuntimePolicyConfig())
		{
			outSnapshot.AudioConfig.bAudioRequired = runtimePolicy->bRequireAVSync;
			outSnapshot.AudioConfig.AudioDriftToleranceMs = runtimePolicy->AllowedDriftMs;
		}
	}

	if (outSnapshot.VideoConfig.VideoIntervalSec <= 0)
	{
		outSnapshot.VideoConfig.VideoIntervalSec = 1;
	}

	return outSnapshot.IsValidateCommonEncoderArguments();
}

UVdjmRecordArtifact* UVdjmRecordResource::BuildRecordArtifact(
	UObject* artifactOuter,
	UVdjmRecorderController* ownerController,
	int32 recordedFrameCount,
	FString& outErrorReason) const
{
	outErrorReason.Reset();

	FVdjmRecordEncoderSnapshot encoderSnapshot;
	if (not BuildEncoderSnapshot(encoderSnapshot))
	{
		outErrorReason = TEXT("Failed to build encoder snapshot for record artifact.");
		return nullptr;
	}

	UObject* resolvedOuter = IsValid(artifactOuter) ? artifactOuter : GetTransientPackage();
	UVdjmRecordArtifact* recordArtifact = NewObject<UVdjmRecordArtifact>(resolvedOuter);
	if (not IsValid(recordArtifact))
	{
		outErrorReason = TEXT("Failed to create record artifact object.");
		return nullptr;
	}

	if (not recordArtifact->InitializeFromSnapshot(encoderSnapshot, recordedFrameCount, ownerController, outErrorReason))
	{
		return recordArtifact;
	}

	return recordArtifact;
}

void UVdjmRecordResource::ResetResource()
{
}

void UVdjmRecordResource::ReleaseResources()
{
	LinkedOwnerBridge = nullptr;
	LinkedResolver = nullptr;
	UE_LOG(LogVdjmRecorderCore, Log, TEXT("UVdjmRecordResource::ReleaseResources - Resources released."));
}

FTextureRHIRef UVdjmRecordResource::CreateTextureForNV12(FIntPoint resolution, EPixelFormat pixelformat,ETextureCreateFlags createFlags)
{
	OriginResolution = resolution;
	FIntPoint NV12Resolution;
	NV12Resolution.X = resolution.X;
	NV12Resolution.Y = resolution.Y * 3 / 2; // 여기에서 늘려줌. 
	
	TextureResolution = NV12Resolution;	//	Fixed for NV12
	
	EPixelFormat Nv12Format = EPixelFormat::PF_G8;
	FinalPixelFormat = Nv12Format;	//	Fixed for NV12
	
	CachedGroupCount = FIntVector(
		FMath::DivideAndRoundUp(NV12Resolution.X/2, 8),
		FMath::DivideAndRoundUp(NV12Resolution.Y/2, 8),
		1);	//	fixed for NV12
	
	FRHITextureCreateDesc texDesc = FRHITextureCreateDesc::Create2D(TEXT("VdjmRecordResource_RenderTarget"), NV12Resolution,Nv12Format);
	texDesc.SetFlags(createFlags);
	
	return RHICreateTexture(texDesc);
}


/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
§	↓		class AVdjmRecordBridgeActor : public AActor		↓
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

bool VdjmRecordUtils::Validations::DbcValidateResolution(const FIntPoint& inResolution, FIntPoint& outSafeResolution,
	const TCHAR* debugOwner)
{
	outSafeResolution = FIntPoint::ZeroValue;

	if (inResolution.X <= 0 || inResolution.Y <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Invalid resolution. X=%d Y=%d"),
		       debugOwner,
		       inResolution.X,
		       inResolution.Y);
		return false;
	}

	// 영상 인코더/서피스 계열에서 짝수 해상도를 요구하는 경우가 많으므로 방어적으로 보정
	const int32 SafeX = FMath::Max(2, inResolution.X & ~1);
	const int32 SafeY = FMath::Max(2, inResolution.Y & ~1);

	if (SafeX <= 0 || SafeY <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Safe resolution collapsed to invalid value. X=%d Y=%d"),
		       debugOwner,
		       SafeX,
		       SafeY);
		return false;
	}

	outSafeResolution = FIntPoint(SafeX, SafeY);
	return true;
}

bool VdjmRecordUtils::Validations::DbcValidateBitrate(const int32 inBitrate, int32& outSafeBitrate, const TCHAR* debugOwner)
{
	outSafeBitrate = 0;

	if (inBitrate <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Invalid bitrate. Bitrate=%d"),
		       debugOwner,
		       inBitrate);
		return false;
	}

	// 지나치게 작은 값/비정상 큰 값 방어
	constexpr int32 MinBitrate = 100000;      // 100 Kbps
	constexpr int32 MaxBitrate = 100000000;   // 100 Mbps

	if (inBitrate < MinBitrate || inBitrate > MaxBitrate)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Bitrate out of safe range. Bitrate=%d Range=[%d,%d]"),
		       debugOwner,
		       inBitrate,
		       MinBitrate,
		       MaxBitrate);
		return false;
	}

	outSafeBitrate = inBitrate;
	return true;
}

bool VdjmRecordUtils::Validations::DbcValidateOutputFilePath(const FString& inFilePath, FString& outSafeFilePath,
	const TCHAR* debugOwner)
{
	outSafeFilePath.Reset();

	FString SafePath = inFilePath;
	SafePath.TrimStartAndEndInline();

	if (SafePath.IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file path is empty."),
		       debugOwner);
		return false;
	}

	FPaths::NormalizeFilename(SafePath);
	VdjmRecordUtils::FilePaths::StripEmbeddedWindowsAbsolutePathInline(SafePath);

	// 상대경로면 한 번 절대경로화 시도
	if (FPaths::IsRelative(SafePath))
	{
		SafePath = FPaths::ConvertRelativePathToFull(SafePath);
		FPaths::NormalizeFilename(SafePath);
		VdjmRecordUtils::FilePaths::StripEmbeddedWindowsAbsolutePathInline(SafePath);
	}

	if (FPaths::IsRelative(SafePath))
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file path is still relative after normalization. Path=%s"),
		       debugOwner,
		       *SafePath);
		return false;
	}

	const FString DirectoryPath = FPaths::GetPath(SafePath);
	if (DirectoryPath.IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file path has no parent directory. Path=%s"),
		       debugOwner,
		       *SafePath);
		return false;
	}

	// 디렉터리가 없으면 생성 시도
	if (!IFileManager::Get().DirectoryExists(*DirectoryPath))
	{
		if (!IFileManager::Get().MakeDirectory(*DirectoryPath, true))
		{
			UE_LOG(LogVdjmRecorderCore, Error,
			       TEXT("%s - Failed to create output directory. Directory=%s"),
			       debugOwner,
			       *DirectoryPath);
			return false;
		}
	}

	const FString CleanFilename = FPaths::GetCleanFilename(SafePath);
	if (CleanFilename.IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Output file name is empty. Path=%s"),
		       debugOwner,
		       *SafePath);
		return false;
	}

	const FString extension = FPaths::GetExtension(SafePath, false).ToLower();
	bool bIsAllowedOutputExtension = false;
#if PLATFORM_ANDROID
	bIsAllowedOutputExtension = extension == TEXT("mp4");
#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_IOS
	bIsAllowedOutputExtension = extension == TEXT("mp4") || extension == TEXT("mov");
#else
	bIsAllowedOutputExtension = extension == TEXT("mp4");
#endif

	if (!bIsAllowedOutputExtension)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
		       TEXT("%s - Invalid output extension. Extension=%s Path=%s"),
		       debugOwner,
		       *extension,
		       *SafePath);
		return false;
	}

	static const TCHAR* InvalidChars = TEXT("<>:\"|?*");
	for (TCHAR Ch : CleanFilename)
	{
		if (FCString::Strchr(InvalidChars, Ch) != nullptr)
		{
			UE_LOG(LogVdjmRecorderCore, Error,
			       TEXT("%s - Output file name contains invalid character '%c'. File=%s"),
			       debugOwner,
			       Ch,
			       *CleanFilename);
			return false;
		}
	}

	outSafeFilePath = SafePath;
	return true;
}

bool VdjmRecordUtils::Validations::DbcValidateAudioConfig(const FVdjmEncoderInitRequestAudio& inAudioConfig,
	const TCHAR* debugOwner)
{
	if (!inAudioConfig.bEnableInternalAudioCapture)
	{
		return true;
	}

	if (inAudioConfig.AudioMimeType.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("%s - Audio mime type is empty."), debugOwner);
		return false;
	}

	constexpr int32 MinSampleRate = 8000;
	constexpr int32 MaxSampleRate = 192000;
	if (inAudioConfig.SampleRate < MinSampleRate || inAudioConfig.SampleRate > MaxSampleRate)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("%s - Audio sample rate out of range. SampleRate=%d Range=[%d,%d]"),
			debugOwner,
			inAudioConfig.SampleRate,
			MinSampleRate,
			MaxSampleRate);
		return false;
	}

	if (inAudioConfig.ChannelCount <= 0 || inAudioConfig.ChannelCount > 2)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("%s - Audio channel count out of range. ChannelCount=%d (supported 1~2)"),
			debugOwner,
			inAudioConfig.ChannelCount);
		return false;
	}

	int32 SafeBitrate = 0;
	if (!VdjmRecordUtils::Validations::DbcValidateBitrate(inAudioConfig.Bitrate, SafeBitrate, debugOwner))
	{
		return false;
	}

	if (inAudioConfig.AacProfile <= 0)
	{
		UE_LOG(LogVdjmRecorderCore, Error,
			TEXT("%s - Invalid AAC profile. AacProfile=%d"),
			debugOwner,
			inAudioConfig.AacProfile);
		return false;
	}

	if (inAudioConfig.SourceSubMixName.IsNone())
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("%s - Source submix name is empty."), debugOwner);
		return false;
	}

	return true;
}
