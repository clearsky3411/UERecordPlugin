#include "VdjmVcardTileImageLoadBroker.h"

#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformFileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include "VdjmVcard.h"

namespace
{
	struct FVcardDecodedImageResult
	{
		TArray<uint8> ThumbnailPixels;
		TArray<uint8> SourcePixels;
		int32 ThumbnailWidth = 0;
		int32 ThumbnailHeight = 0;
		int32 SourceWidth = 0;
		int32 SourceHeight = 0;
		bool bSuccess = false;
		FString ErrorReason;
	};

	bool EnsureDirectoryExists(const FString& directoryPath, FString& outErrorReason)
	{
		if (directoryPath.IsEmpty())
		{
			outErrorReason = TEXT("Directory path is empty.");
			return false;
		}

		IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (platformFile.DirectoryExists(*directoryPath))
		{
			return true;
		}

		if (!platformFile.CreateDirectoryTree(*directoryPath))
		{
			outErrorReason = FString::Printf(TEXT("Failed to create directory '%s'."), *directoryPath);
			return false;
		}

		return true;
	}

	bool IsSupportedImageExtension(const FString& filePath)
	{
		const FString extension = FPaths::GetExtension(filePath, false).ToLower();
		return extension == TEXT("png")
			|| extension == TEXT("jpg")
			|| extension == TEXT("jpeg")
			|| extension == TEXT("bmp");
	}

	void ResizeBgraNearest(
		const TArray<uint8>& sourcePixels,
		int32 sourceWidth,
		int32 sourceHeight,
		int32 targetWidth,
		int32 targetHeight,
		TArray<uint8>& outPixels)
	{
		outPixels.Reset();
		if (sourcePixels.Num() <= 0 || sourceWidth <= 0 || sourceHeight <= 0 || targetWidth <= 0 || targetHeight <= 0)
		{
			return;
		}

		outPixels.SetNumZeroed(targetWidth * targetHeight * 4);
		for (int32 targetY = 0; targetY < targetHeight; ++targetY)
		{
			const int32 sourceY = FMath::Clamp(FMath::FloorToInt(static_cast<float>(targetY) * static_cast<float>(sourceHeight) / static_cast<float>(targetHeight)), 0, sourceHeight - 1);
			for (int32 targetX = 0; targetX < targetWidth; ++targetX)
			{
				const int32 sourceX = FMath::Clamp(FMath::FloorToInt(static_cast<float>(targetX) * static_cast<float>(sourceWidth) / static_cast<float>(targetWidth)), 0, sourceWidth - 1);
				const int32 sourceIndex = ((sourceY * sourceWidth) + sourceX) * 4;
				const int32 targetIndex = ((targetY * targetWidth) + targetX) * 4;
				outPixels[targetIndex] = sourcePixels[sourceIndex];
				outPixels[targetIndex + 1] = sourcePixels[sourceIndex + 1];
				outPixels[targetIndex + 2] = sourcePixels[sourceIndex + 2];
				outPixels[targetIndex + 3] = sourcePixels[sourceIndex + 3];
			}
		}
	}

	FIntPoint CalculateLimitedImageSize(int32 sourceWidth, int32 sourceHeight, int32 maxSize)
	{
		if (sourceWidth <= 0 || sourceHeight <= 0 || maxSize <= 0)
		{
			return FIntPoint(sourceWidth, sourceHeight);
		}

		const int32 largestAxis = FMath::Max(sourceWidth, sourceHeight);
		if (largestAxis <= maxSize)
		{
			return FIntPoint(sourceWidth, sourceHeight);
		}

		const float scale = static_cast<float>(maxSize) / static_cast<float>(largestAxis);
		return FIntPoint(
			FMath::Max(1, FMath::RoundToInt(static_cast<float>(sourceWidth) * scale)),
			FMath::Max(1, FMath::RoundToInt(static_cast<float>(sourceHeight) * scale)));
	}

	UTexture2D* CreateTransientTextureFromBgra(const TArray<uint8>& pixels, int32 width, int32 height, const FString& textureName)
	{
		if (pixels.Num() <= 0 || width <= 0 || height <= 0 || pixels.Num() < width * height * 4)
		{
			return nullptr;
		}

		UTexture2D* texture = UTexture2D::CreateTransient(width, height, PF_B8G8R8A8, FName(*textureName));
		if (!IsValid(texture) || texture->GetPlatformData() == nullptr || texture->GetPlatformData()->Mips.Num() <= 0)
		{
			return nullptr;
		}

		texture->SRGB = true;

		FTexture2DMipMap& mip = texture->GetPlatformData()->Mips[0];
		void* textureData = mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(textureData, pixels.GetData(), width * height * 4);
		mip.BulkData.Unlock();
		texture->UpdateResource();
		return texture;
	}

	FVcardDecodedImageResult DecodeLocalImageFile(
		const FString& filePath,
		EVcardTileImageLoadRequestType requestType,
		int32 thumbnailSize,
		int32 maxSourceTextureSize)
	{
		FVcardDecodedImageResult result;

		TArray<uint8> compressedData;
		if (!FFileHelper::LoadFileToArray(compressedData, *filePath))
		{
			result.ErrorReason = FString::Printf(TEXT("Failed to read image file '%s'."), *filePath);
			return result;
		}

		IImageWrapperModule& imageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const EImageFormat imageFormat = imageWrapperModule.DetectImageFormat(compressedData.GetData(), compressedData.Num());
		if (imageFormat == EImageFormat::Invalid)
		{
			result.ErrorReason = FString::Printf(TEXT("Unsupported image format '%s'."), *filePath);
			return result;
		}

		const TSharedPtr<IImageWrapper> imageWrapper = imageWrapperModule.CreateImageWrapper(imageFormat);
		if (!imageWrapper.IsValid() || !imageWrapper->SetCompressed(compressedData.GetData(), compressedData.Num()))
		{
			result.ErrorReason = FString::Printf(TEXT("Failed to decode image header '%s'."), *filePath);
			return result;
		}

		TArray64<uint8> rawPixels64;
		if (!imageWrapper->GetRaw(ERGBFormat::BGRA, 8, rawPixels64))
		{
			result.ErrorReason = FString::Printf(TEXT("Failed to decode image pixels '%s'."), *filePath);
			return result;
		}

		TArray<uint8> rawPixels;
		const int32 rawPixelByteCount = IntCastChecked<int32>(rawPixels64.Num());
		rawPixels.SetNumUninitialized(rawPixelByteCount);
		FMemory::Memcpy(rawPixels.GetData(), rawPixels64.GetData(), rawPixelByteCount);

		const int32 sourceWidth = imageWrapper->GetWidth();
		const int32 sourceHeight = imageWrapper->GetHeight();
		if (sourceWidth <= 0 || sourceHeight <= 0)
		{
			result.ErrorReason = FString::Printf(TEXT("Decoded image size is invalid '%s'."), *filePath);
			return result;
		}

		if (requestType == EVcardTileImageLoadRequestType::EThumbnailOnly || requestType == EVcardTileImageLoadRequestType::EThumbnailAndSource)
		{
			const int32 resolvedThumbnailSize = FMath::Max(1, thumbnailSize);
			result.ThumbnailWidth = resolvedThumbnailSize;
			result.ThumbnailHeight = resolvedThumbnailSize;
			ResizeBgraNearest(rawPixels, sourceWidth, sourceHeight, result.ThumbnailWidth, result.ThumbnailHeight, result.ThumbnailPixels);
		}

		if (requestType == EVcardTileImageLoadRequestType::ESourceOnly || requestType == EVcardTileImageLoadRequestType::EThumbnailAndSource)
		{
			const FIntPoint sourceSize = CalculateLimitedImageSize(sourceWidth, sourceHeight, maxSourceTextureSize);
			result.SourceWidth = sourceSize.X;
			result.SourceHeight = sourceSize.Y;
			if (result.SourceWidth == sourceWidth && result.SourceHeight == sourceHeight)
			{
				result.SourcePixels = MoveTemp(rawPixels);
			}
			else
			{
				ResizeBgraNearest(rawPixels, sourceWidth, sourceHeight, result.SourceWidth, result.SourceHeight, result.SourcePixels);
			}
		}

		result.bSuccess = true;
		return result;
	}

	FString ResolveStorageBasePath(EVcardTileImageStorageMode storageMode, const FString& customAbsolutePath)
	{
		switch (storageMode)
		{
		case EVcardTileImageStorageMode::EProjectSaved:
			return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		case EVcardTileImageStorageMode::ECustomAbsolute:
			return FPaths::ConvertRelativePathToFull(customAbsolutePath);
		case EVcardTileImageStorageMode::EPersistentDownload:
		default:
			return FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());
		}
	}
}

AVcardTileImageLoadBroker::AVcardTileImageLoadBroker()
{
	PrimaryActorTick.bCanEverTick = false;

	FString errorReason;
	ConfigureStoragePath(
		EVcardTileImageStorageMode::EPersistentDownload,
		TEXT("Vcard/UserImages"),
		FString(),
		false,
		errorReason);
}

bool AVcardTileImageLoadBroker::ConfigureStoragePath(
	EVcardTileImageStorageMode storageMode,
	const FString& relativeFolder,
	const FString& customAbsolutePath,
	bool bCreateIfMissing,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	FString basePath = ResolveStorageBasePath(storageMode, customAbsolutePath);
	if (basePath.IsEmpty())
	{
		outErrorReason = TEXT("Storage base path is empty.");
		return false;
	}

	FString resolvedPath = basePath;
	if (!relativeFolder.IsEmpty())
	{
		resolvedPath = FPaths::Combine(basePath, relativeFolder);
	}

	return SetStorageRootPath(resolvedPath, bCreateIfMissing, outErrorReason);
}

bool AVcardTileImageLoadBroker::SetStorageRootPath(const FString& storageRootPath, bool bCreateIfMissing, FString& outErrorReason)
{
	outErrorReason.Reset();

	if (storageRootPath.IsEmpty())
	{
		outErrorReason = TEXT("Storage root path is empty.");
		return false;
	}

	mStorageRootPath = FPaths::ConvertRelativePathToFull(storageRootPath);
	FPaths::NormalizeDirectoryName(mStorageRootPath);

	if (bCreateIfMissing)
	{
		return EnsureDirectoryExists(mStorageRootPath, outErrorReason);
	}

	return true;
}

bool AVcardTileImageLoadBroker::CopyImageFileToStore(
	const FString& sourceFilePath,
	FString& outStoredFilePath,
	FString& outErrorReason) const
{
	outStoredFilePath.Reset();
	outErrorReason.Reset();

	if (!FPaths::FileExists(sourceFilePath))
	{
		outErrorReason = FString::Printf(TEXT("Source image file does not exist '%s'."), *sourceFilePath);
		return false;
	}

	if (!IsSupportedImageExtension(sourceFilePath))
	{
		outErrorReason = FString::Printf(TEXT("Unsupported image extension '%s'."), *sourceFilePath);
		return false;
	}

	if (mStorageRootPath.IsEmpty())
	{
		outErrorReason = TEXT("Storage root path is empty.");
		return false;
	}

	FString directoryErrorReason;
	if (!EnsureDirectoryExists(mStorageRootPath, directoryErrorReason))
	{
		outErrorReason = directoryErrorReason;
		return false;
	}

	const FString extension = FPaths::GetExtension(sourceFilePath, true).ToLower();
	const FString baseName = FPaths::GetBaseFilename(sourceFilePath);
	const FString uniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString storedFileName = FString::Printf(TEXT("%s_%s%s"), *baseName, *uniqueSuffix, *extension);
	const FString storedFilePath = FPaths::Combine(mStorageRootPath, storedFileName);

	IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!platformFile.CopyFile(*storedFilePath, *sourceFilePath))
	{
		outErrorReason = FString::Printf(TEXT("Failed to copy image file to '%s'."), *storedFilePath);
		return false;
	}

	outStoredFilePath = storedFilePath;
	return true;
}

UVcardTileItemDataState* AVcardTileImageLoadBroker::CreateLocalImageTileItem(
	UObject* outer,
	const FString& localImageFilePath,
	FName itemId,
	FText displayName,
	FString& outErrorReason) const
{
	outErrorReason.Reset();

	if (!FPaths::FileExists(localImageFilePath))
	{
		outErrorReason = FString::Printf(TEXT("Local image file does not exist '%s'."), *localImageFilePath);
		return nullptr;
	}

	UObject* resolvedOuter = IsValid(outer) ? outer : const_cast<AVcardTileImageLoadBroker*>(this);
	UVcardTileItemDataState* itemDataState = NewObject<UVcardTileItemDataState>(resolvedOuter);
	if (!IsValid(itemDataState))
	{
		outErrorReason = TEXT("Failed to create tile item data state.");
		return nullptr;
	}

	itemDataState->ItemId = !itemId.IsNone() ? itemId : FName(*FPaths::GetBaseFilename(localImageFilePath));
	itemDataState->DisplayName = displayName.IsEmpty() ? FText::FromString(FPaths::GetBaseFilename(localImageFilePath)) : displayName;
	itemDataState->SetLocalImageFileSource(localImageFilePath);
	return itemDataState;
}

bool AVcardTileImageLoadBroker::QueueTileItemLoad(
	UVcardTileItemDataState* itemDataState,
	EVcardTileImageLoadRequestType requestType,
	FString& outErrorReason)
{
	outErrorReason.Reset();

	if (!IsValid(itemDataState))
	{
		outErrorReason = TEXT("Tile item data state is invalid.");
		return false;
	}

	FVcardTileImageLoadQueueEntry queueEntry;
	queueEntry.ItemDataState = itemDataState;
	queueEntry.RequestType = requestType;
	mPendingRequests.Add(queueEntry);

	itemDataState->SetImageLoadState(requestType, EVcardTileImageLoadState::EQueued, FString());
	StartProcessingQueue();
	return true;
}

int32 AVcardTileImageLoadBroker::QueueTileItemsLoad(
	const TArray<UVcardTileItemDataState*>& itemDataStates,
	EVcardTileImageLoadRequestType requestType)
{
	int32 queuedCount = 0;
	for (UVcardTileItemDataState* itemDataState : itemDataStates)
	{
		FString errorReason;
		if (QueueTileItemLoad(itemDataState, requestType, errorReason))
		{
			++queuedCount;
		}
	}

	return queuedCount;
}

void AVcardTileImageLoadBroker::CancelAllLoads()
{
	mPendingRequests.Reset();
	for (const TSharedPtr<FStreamableHandle>& streamableHandle : mActiveStreamableHandles)
	{
		if (streamableHandle.IsValid())
		{
			streamableHandle->CancelHandle();
		}
	}

	mActiveStreamableHandles.Reset();
	mActiveJobCount = 0;
	StopProcessingQueue();
}

void AVcardTileImageLoadBroker::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	CancelAllLoads();
	Super::EndPlay(endPlayReason);
}

void AVcardTileImageLoadBroker::StartProcessingQueue()
{
	if (mbProcessingQueue)
	{
		return;
	}

	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return;
	}

	mbProcessingQueue = true;
	world->GetTimerManager().SetTimer(
		ProcessTimerHandle,
		this,
		&AVcardTileImageLoadBroker::ProcessQueueStep,
		FMath::Max(0.001f, StepIntervalSeconds),
		true);
}

void AVcardTileImageLoadBroker::StopProcessingQueue()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(ProcessTimerHandle);
	}

	mbProcessingQueue = false;
}

void AVcardTileImageLoadBroker::ProcessQueueStep()
{
	if (mPendingRequests.Num() <= 0 && mActiveJobCount <= 0)
	{
		StopProcessingQueue();
		return;
	}

	const int32 resolvedMaxActiveJobs = FMath::Max(1, MaxActiveJobs);
	const int32 resolvedMaxJobsPerStep = FMath::Max(1, MaxJobsPerStep);
	int32 startedCount = 0;

	while (mPendingRequests.Num() > 0 && mActiveJobCount < resolvedMaxActiveJobs && startedCount < resolvedMaxJobsPerStep)
	{
		const FVcardTileImageLoadQueueEntry queueEntry = mPendingRequests[0];
		mPendingRequests.RemoveAt(0);

		StartLoadRequest(queueEntry);
		++startedCount;
	}
}

bool AVcardTileImageLoadBroker::StartLoadRequest(const FVcardTileImageLoadQueueEntry& queueEntry)
{
	UVcardTileItemDataState* itemDataState = queueEntry.ItemDataState;
	if (!IsValid(itemDataState))
	{
		return false;
	}

	itemDataState->SetImageLoadState(queueEntry.RequestType, EVcardTileImageLoadState::ELoading, FString());

	switch (itemDataState->GetImageSourceType())
	{
	case EVcardTileImageSourceType::EAssetTexture:
		return StartAssetTextureLoad(itemDataState, queueEntry.RequestType);
	case EVcardTileImageSourceType::ELocalImageFile:
		return StartLocalImageFileLoad(itemDataState, queueEntry.RequestType);
	case EVcardTileImageSourceType::ENone:
	default:
		CompleteLoadRequest(itemDataState, queueEntry.RequestType, false, TEXT("Tile item image source is none."));
		return false;
	}
}

bool AVcardTileImageLoadBroker::StartAssetTextureLoad(UVcardTileItemDataState* itemDataState, EVcardTileImageLoadRequestType requestType)
{
	TArray<FSoftObjectPath> loadPaths;

	const FSoftObjectPath sourcePath = itemDataState->GetSourceTexture().ToSoftObjectPath();
	const FSoftObjectPath thumbnailPath = itemDataState->GetThumbnailTexture().ToSoftObjectPath();
	const bool bNeedsThumbnail = requestType == EVcardTileImageLoadRequestType::EThumbnailOnly || requestType == EVcardTileImageLoadRequestType::EThumbnailAndSource;
	const bool bNeedsSource = requestType == EVcardTileImageLoadRequestType::ESourceOnly || requestType == EVcardTileImageLoadRequestType::EThumbnailAndSource;

	if (bNeedsThumbnail)
	{
		loadPaths.AddUnique(thumbnailPath.IsValid() ? thumbnailPath : sourcePath);
	}

	if (bNeedsSource)
	{
		loadPaths.AddUnique(sourcePath);
	}

	loadPaths.RemoveAll([](const FSoftObjectPath& path)
	{
		return !path.IsValid();
	});

	if (loadPaths.Num() <= 0)
	{
		CompleteLoadRequest(itemDataState, requestType, false, TEXT("No valid asset texture path to load."));
		return false;
	}

	++mActiveJobCount;
	TWeakObjectPtr<AVcardTileImageLoadBroker> weakThis(this);
	TWeakObjectPtr<UVcardTileItemDataState> weakItem(itemDataState);
	TSharedPtr<FStreamableHandle> streamableHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		loadPaths,
		FStreamableDelegate::CreateLambda([weakThis, weakItem, requestType, sourcePath, thumbnailPath]()
		{
			AVcardTileImageLoadBroker* broker = weakThis.Get();
			if (!IsValid(broker))
			{
				return;
			}

			UVcardTileItemDataState* resolvedItem = weakItem.Get();
			if (!IsValid(resolvedItem))
			{
				broker->CompleteLoadRequest(nullptr, requestType, false, TEXT("Tile item was destroyed before asset texture load completed."));
				return;
			}

			const bool bNeedsThumbnail = requestType == EVcardTileImageLoadRequestType::EThumbnailOnly || requestType == EVcardTileImageLoadRequestType::EThumbnailAndSource;
			const bool bNeedsSource = requestType == EVcardTileImageLoadRequestType::ESourceOnly || requestType == EVcardTileImageLoadRequestType::EThumbnailAndSource;

			bool bSuccess = true;
			if (bNeedsThumbnail)
			{
				const FSoftObjectPath resolvedThumbnailPath = thumbnailPath.IsValid() ? thumbnailPath : sourcePath;
				UTexture2D* loadedThumbnail = Cast<UTexture2D>(resolvedThumbnailPath.ResolveObject());
				resolvedItem->SetLoadedThumbnail(loadedThumbnail);
				bSuccess = bSuccess && IsValid(loadedThumbnail);
			}

			if (bNeedsSource)
			{
				UTexture2D* loadedSourceImage = Cast<UTexture2D>(sourcePath.ResolveObject());
				resolvedItem->SetLoadedSourceImage(loadedSourceImage);
				bSuccess = bSuccess && IsValid(loadedSourceImage);
			}

			broker->CompleteLoadRequest(
				resolvedItem,
				requestType,
				bSuccess,
				bSuccess ? FString() : TEXT("Failed to resolve loaded asset texture."));
		}));

	if (streamableHandle.IsValid())
	{
		mActiveStreamableHandles.Add(streamableHandle);
		return true;
	}

	CompleteLoadRequest(itemDataState, requestType, false, TEXT("Failed to start asset texture load."));
	return false;
}

bool AVcardTileImageLoadBroker::StartLocalImageFileLoad(UVcardTileItemDataState* itemDataState, EVcardTileImageLoadRequestType requestType)
{
	const FString localImageFilePath = itemDataState->GetLocalSourceImagePath();
	if (!FPaths::FileExists(localImageFilePath))
	{
		CompleteLoadRequest(itemDataState, requestType, false, FString::Printf(TEXT("Local image file does not exist '%s'."), *localImageFilePath));
		return false;
	}

	++mActiveJobCount;

	TWeakObjectPtr<AVcardTileImageLoadBroker> weakThis(this);
	TWeakObjectPtr<UVcardTileItemDataState> weakItem(itemDataState);
	const int32 resolvedThumbnailSize = FMath::Max(1, ThumbnailSize);
	const int32 resolvedMaxSourceTextureSize = FMath::Max(1, MaxSourceTextureSize);
	FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	Async(EAsyncExecution::ThreadPool, [weakThis, weakItem, localImageFilePath, requestType, resolvedThumbnailSize, resolvedMaxSourceTextureSize]()
	{
		FVcardDecodedImageResult decodeResult = DecodeLocalImageFile(
			localImageFilePath,
			requestType,
			resolvedThumbnailSize,
			resolvedMaxSourceTextureSize);

		AsyncTask(ENamedThreads::GameThread, [weakThis, weakItem, requestType, decodeResult = MoveTemp(decodeResult)]() mutable
		{
			AVcardTileImageLoadBroker* broker = weakThis.Get();
			if (!IsValid(broker))
			{
				return;
			}

			UVcardTileItemDataState* itemDataState = weakItem.Get();
			if (!IsValid(itemDataState))
			{
				broker->CompleteLoadRequest(nullptr, requestType, false, TEXT("Tile item was destroyed before local image load completed."));
				return;
			}

			if (!decodeResult.bSuccess)
			{
				broker->CompleteLoadRequest(itemDataState, requestType, false, decodeResult.ErrorReason);
				return;
			}

			bool bCreatedAllRequestedTextures = true;

			if (decodeResult.ThumbnailPixels.Num() > 0)
			{
				UTexture2D* thumbnailTexture = CreateTransientTextureFromBgra(
					decodeResult.ThumbnailPixels,
					decodeResult.ThumbnailWidth,
					decodeResult.ThumbnailHeight,
					FString::Printf(TEXT("VcardTileThumb_%s"), *itemDataState->GetItemId().ToString()));
				itemDataState->SetLoadedThumbnail(thumbnailTexture);
				bCreatedAllRequestedTextures = bCreatedAllRequestedTextures && IsValid(thumbnailTexture);
			}

			if (decodeResult.SourcePixels.Num() > 0)
			{
				UTexture2D* sourceTexture = CreateTransientTextureFromBgra(
					decodeResult.SourcePixels,
					decodeResult.SourceWidth,
					decodeResult.SourceHeight,
					FString::Printf(TEXT("VcardTileSource_%s"), *itemDataState->GetItemId().ToString()));
				itemDataState->SetLoadedSourceImage(sourceTexture);
				bCreatedAllRequestedTextures = bCreatedAllRequestedTextures && IsValid(sourceTexture);
			}

			broker->CompleteLoadRequest(
				itemDataState,
				requestType,
				bCreatedAllRequestedTextures,
				bCreatedAllRequestedTextures ? FString() : TEXT("Failed to create transient texture."));
		});
	});

	return true;
}

void AVcardTileImageLoadBroker::CompleteLoadRequest(
	UVcardTileItemDataState* itemDataState,
	EVcardTileImageLoadRequestType requestType,
	bool bSuccess,
	const FString& errorReason)
{
	mActiveJobCount = FMath::Max(0, mActiveJobCount - 1);

	if (IsValid(itemDataState))
	{
		if (!bSuccess)
		{
			itemDataState->SetImageLoadState(requestType, EVcardTileImageLoadState::EFailed, errorReason);
		}
		OnItemLoadCompleted.Broadcast(itemDataState, requestType, bSuccess);
	}

	mActiveStreamableHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& streamableHandle)
	{
		return !streamableHandle.IsValid() || streamableHandle->HasLoadCompleted();
	});

	if (mPendingRequests.Num() <= 0 && mActiveJobCount <= 0)
	{
		StopProcessingQueue();
	}
}
