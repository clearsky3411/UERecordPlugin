#pragma once

#include "CoreMinimal.h"

struct FVdjmRecordAndroidMediaStorePublishResult
{
	FString SourceFilePath;
	FString PublishedContentUri;
	FString DisplayName;
	FString RelativePath;
	FString ErrorReason;
	int64 SourceFileSizeBytes = -1;
	bool bSucceeded = false;
};

namespace VdjmRecordAndroidMediaStore
{
	VDJMRECORDER_API bool PublishVideoFileToMediaStore(
		const FString& sourceFilePath,
		const FString& displayName,
		const FString& relativePath,
		FVdjmRecordAndroidMediaStorePublishResult& outResult);

	VDJMRECORDER_API bool ScanAndroidMediaFile(const FString& sourceFilePath, FString& outErrorReason);
}
