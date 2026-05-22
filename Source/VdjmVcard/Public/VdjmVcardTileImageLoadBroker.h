#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "VdjmVcardTileViewWidgets.h"
#include "VdjmVcardTileImageLoadBroker.generated.h"

struct FStreamableHandle;

UENUM(BlueprintType)
enum class EVcardTileImageStorageMode : uint8
{
	EPersistentDownload UMETA(DisplayName = "Persistent Download"),
	EProjectSaved UMETA(DisplayName = "Project Saved"),
	ECustomAbsolute UMETA(DisplayName = "Custom Absolute")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVcardTileImageBrokerItemDelegate, UVcardTileItemDataState*, ItemDataState, EVcardTileImageLoadRequestType, RequestType, bool, bSuccess);

USTRUCT()
struct FVcardTileImageLoadQueueEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UVcardTileItemDataState> ItemDataState = nullptr;

	UPROPERTY(Transient)
	EVcardTileImageLoadRequestType RequestType = EVcardTileImageLoadRequestType::EThumbnailOnly;
};

/**
 * Timer driven image load broker for V-card tile item data.
 *
 * Responsibility:
 * - Keep thumbnail/source loading independent from TileView widgets.
 * - Process queued load requests with a bounded timer loop.
 * - Copy user image files into the app storage folder.
 *
 * Must not:
 * - Decide how a loaded image is applied to world actors.
 * - Scan arbitrary project folders at runtime; use baked descriptors for packaged builds.
 */
UCLASS(BlueprintType, Blueprintable)
class VDJMVCARD_API AVcardTileImageLoadBroker : public AInfo
{
	GENERATED_BODY()

public:
	AVcardTileImageLoadBroker();

	UFUNCTION(BlueprintCallable, Category = "Vcard|TileImage")
	bool ConfigureStoragePath(EVcardTileImageStorageMode storageMode, const FString& relativeFolder, const FString& customAbsolutePath, bool bCreateIfMissing, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileImage")
	bool SetStorageRootPath(const FString& storageRootPath, bool bCreateIfMissing, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileImage")
	bool CopyImageFileToStore(const FString& sourceFilePath, FString& outStoredFilePath, FString& outErrorReason) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileImage")
	UVcardTileItemDataState* CreateLocalImageTileItem(UObject* outer, const FString& localImageFilePath, FName itemId, FText displayName, FString& outErrorReason) const;
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileImage")
	bool QueueTileItemLoad(UVcardTileItemDataState* itemDataState, EVcardTileImageLoadRequestType requestType, FString& outErrorReason);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileImage")
	int32 QueueTileItemsLoad(const TArray<UVcardTileItemDataState*>& itemDataStates, EVcardTileImageLoadRequestType requestType);
	UFUNCTION(BlueprintCallable, Category = "Vcard|TileImage")
	void CancelAllLoads();

	UFUNCTION(BlueprintPure, Category = "Vcard|TileImage")
	FString GetStorageRootPath() const { return mStorageRootPath; }
	UFUNCTION(BlueprintPure, Category = "Vcard|TileImage")
	bool IsProcessingQueue() const { return mbProcessingQueue; }
	UFUNCTION(BlueprintPure, Category = "Vcard|TileImage")
	int32 GetPendingRequestCount() const { return mPendingRequests.Num(); }
	UFUNCTION(BlueprintPure, Category = "Vcard|TileImage")
	int32 GetActiveRequestCount() const { return mActiveJobCount; }

	UPROPERTY(BlueprintAssignable, Category = "Vcard|TileImage")
	FVcardTileImageBrokerItemDelegate OnItemLoadCompleted;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|TileImage|Queue")
	int32 MaxActiveJobs = 2;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|TileImage|Queue")
	int32 MaxJobsPerStep = 2;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|TileImage|Queue")
	float StepIntervalSeconds = 0.03f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|TileImage|Image")
	int32 ThumbnailSize = 64;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Vcard|TileImage|Image")
	int32 MaxSourceTextureSize = 2048;

protected:
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

private:
	void StartProcessingQueue();
	void StopProcessingQueue();
	void ProcessQueueStep();
	bool StartLoadRequest(const FVcardTileImageLoadQueueEntry& queueEntry);
	bool StartAssetTextureLoad(UVcardTileItemDataState* itemDataState, EVcardTileImageLoadRequestType requestType);
	bool StartLocalImageFileLoad(UVcardTileItemDataState* itemDataState, EVcardTileImageLoadRequestType requestType);
	void CompleteLoadRequest(UVcardTileItemDataState* itemDataState, EVcardTileImageLoadRequestType requestType, bool bSuccess, const FString& errorReason);

	UPROPERTY(Transient)
	TArray<FVcardTileImageLoadQueueEntry> mPendingRequests;

	TArray<TSharedPtr<FStreamableHandle>> mActiveStreamableHandles;

	UPROPERTY(Transient)
	FString mStorageRootPath;

	FTimerHandle ProcessTimerHandle;

	int32 mActiveJobCount = 0;

	bool mbProcessingQueue = false;
};
