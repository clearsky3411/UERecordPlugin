#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "VdjmRecordAppState.generated.h"

class UVdjmRecordMetadataStore;

/**
 * @brief 앱 전체 복원/네트워크/미디어 TOC 상태를 한 JSON으로 저장하기 위한 사용자 섹션이다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordAppStateUserSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|User")
	FString UserId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|User")
	FString AuthorityRole = TEXT("developer");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|User")
	FString DeveloperId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|User")
	FString SessionTokenId;
};

/**
 * @brief 로컬 테스트 서버/GCP 업로드 서버 같은 원격 미디어 저장소 접속 정보를 담는다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordAppStateRemoteSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Remote")
	FString Provider = TEXT("local");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Remote")
	FString BaseUrl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Remote")
	FString HealthEndpointPath = TEXT("/health");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Remote")
	FString UploadRequestEndpointPath = TEXT("/upload/request");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Remote")
	bool bPreferDirectUpload = true;
};

/**
 * @brief 시작 시 다시 찾아야 하는 주요 DataAsset/Widget 경로를 한곳에 모은다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordAppStateAssetSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Assets")
	FSoftObjectPath EnvDataAssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Assets")
	FSoftObjectPath EventFlowDataAssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Assets")
	FSoftObjectPath LoadingWidgetClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Assets")
	FSoftObjectPath MainWidgetClassPath;
};

/**
 * @brief 개별 manifest 원본 대신 앱 시작/갤러리 구성을 위한 가벼운 목차 항목만 저장한다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordAppStateRecordTocEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString RecordId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString MetadataFilePath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString OutputFilePath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString PlaybackLocatorType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString PlaybackLocator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString PublishedContentUri;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString RemoteObjectKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString ThumbnailFilePath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString PreviewClipFilePath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString PreviewClipMimeType = TEXT("video/mp4");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString RegistryStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString LocalFileStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString MediaPublishStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString RemoteUploadStatus = TEXT("not_started");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString PreviewStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	FString LastErrorReason;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	int64 CreatedUnixTime = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	int64 LastSeenUnixTime = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	int64 FileSizeBytes = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	int32 VideoWidth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	int32 VideoHeight = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	bool bOutputFileExists = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	bool bMetadataFileExists = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|AppState|Records")
	bool bIsDeleted = false;
};

/**
 * @brief 앱 재진입 시 마지막 선택/복원 후보 같은 휘발성에 가까운 런타임 정보를 저장한다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordAppStateRuntimeSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Runtime")
	FString LastWorldName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Runtime")
	FString LastSelectedRecordId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Runtime")
	FString LastFlowSessionName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Runtime")
	FString RestoreStateJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Runtime")
	bool bRestoreLastSelection = true;
};

/**
 * @brief 미디어 프리뷰/캐러셀 UI가 마지막 위치를 복원할 수 있게 하는 섹션이다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordAppStatePreviewSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Preview")
	FString SelectedRecordId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Preview")
	int32 CarouselCenterIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Preview")
	int32 SlotWindowSize = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Preview")
	int32 ActiveSlotIndex = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Preview")
	bool bAutoPlayCenterPreview = true;
};

/**
 * @brief EventFlow 카탈로그/서브그래프를 이후 확장하기 위한 현재 실행 힌트 섹션이다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordAppStateEventFlowSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|EventFlow")
	FString MainFlowAssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|EventFlow")
	FString LastStartedSessionName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|EventFlow")
	FString LastSubgraphName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|EventFlow")
	FString ExternalFlowJsonPath;
};

/**
 * @brief Controller 옵션 UI/요청을 나중에 복원하기 위한 문자열 기반 스냅샷 섹션이다.
 */
USTRUCT(BlueprintType)
struct VDJMRECORDER_API FVdjmRecordAppStateControllerSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Controller")
	FString LastOptionRequestJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recorder|AppState|Controller")
	FString LastControllerStateJson;
};

/**
 * @brief Vdjm 앱 전체 상태 JSON의 실제 데이터 객체다.
 */
UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordAppStateManifest : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState")
	void Clear();

	UFUNCTION(BlueprintPure, Category = "Recorder|AppState")
	FString ToString() const;

	UFUNCTION(BlueprintPure, Category = "Recorder|AppState")
	FString ToJsonString() const;

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState")
	bool LoadFromJsonString(const FString& appStateJsonString, FString& outErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState")
	bool SaveToFile(const FString& appStateFilePath, FString& outErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState")
	bool LoadFromFile(const FString& appStateFilePath, FString& outErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState")
	void SetAppStateFilePath(const FString& appStateFilePath);

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState|Records")
	void SetRecordTocEntries(const TArray<FVdjmRecordAppStateRecordTocEntry>& recordTocEntries);

	UFUNCTION(BlueprintPure, Category = "Recorder|AppState")
	FString GetAppStateFilePath() const { return mAppStateFilePath; }
	UFUNCTION(BlueprintPure, Category = "Recorder|AppState")
	int32 GetSchemaVersion() const { return mSchemaVersion; }
	UFUNCTION(BlueprintPure, Category = "Recorder|AppState")
	int64 GetGeneratedUnixTime() const { return mGeneratedUnixTime; }
	UFUNCTION(BlueprintPure, Category = "Recorder|AppState|Records")
	TArray<FVdjmRecordAppStateRecordTocEntry> GetRecordTocEntries() const { return mRecordTocEntries; }
	UFUNCTION(BlueprintPure, Category = "Recorder|AppState|Records")
	int32 GetRecordTocEntryCount() const { return mRecordTocEntries.Num(); }

private:
	UPROPERTY(VisibleAnywhere, Category = "Recorder|AppState")
	int32 mSchemaVersion = 1;

	UPROPERTY(VisibleAnywhere, Category = "Recorder|AppState")
	int64 mGeneratedUnixTime = 0;

	UPROPERTY(VisibleAnywhere, Category = "Recorder|AppState")
	FString mAppStateFilePath;

	UPROPERTY(EditAnywhere, Category = "Recorder|AppState")
	FVdjmRecordAppStateUserSection mUser;

	UPROPERTY(EditAnywhere, Category = "Recorder|AppState")
	FVdjmRecordAppStateRemoteSection mRemote;

	UPROPERTY(EditAnywhere, Category = "Recorder|AppState")
	FVdjmRecordAppStateAssetSection mAssets;

	UPROPERTY(EditAnywhere, Category = "Recorder|AppState")
	FVdjmRecordAppStateRuntimeSection mRuntime;

	UPROPERTY(EditAnywhere, Category = "Recorder|AppState")
	FVdjmRecordAppStatePreviewSection mPreview;

	UPROPERTY(EditAnywhere, Category = "Recorder|AppState")
	FVdjmRecordAppStateEventFlowSection mEventFlow;

	UPROPERTY(EditAnywhere, Category = "Recorder|AppState")
	FVdjmRecordAppStateControllerSection mController;

	UPROPERTY(VisibleAnywhere, Category = "Recorder|AppState")
	TArray<FVdjmRecordAppStateRecordTocEntry> mRecordTocEntries;
};

/**
 * @brief AppStateManifest를 생성/로드/저장하고 WorldContext에 등록해주는 런타임 저장소다.
 */
UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordAppStateStore : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecordAppStateStore* FindAppStateStore(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState", meta = (WorldContext = "worldContextObject"))
	static UVdjmRecordAppStateStore* FindOrCreateAppStateStore(UObject* worldContextObject);

	bool InitializeStore(UObject* worldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState")
	void Clear();

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState")
	bool LoadAppState(FString& outErrorReason, bool bCreateIfMissing = true);

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState")
	bool SaveAppState(FString& outErrorReason);

	UFUNCTION(BlueprintCallable, Category = "Recorder|AppState")
	bool RefreshRecordsTocFromMetadataStore(UVdjmRecordMetadataStore* metadataStore, FString& outErrorReason);

	UFUNCTION(BlueprintPure, Category = "Recorder|AppState")
	UVdjmRecordAppStateManifest* GetAppStateManifest() const { return mAppStateManifest; }

	UFUNCTION(BlueprintPure, Category = "Recorder|AppState")
	FString GetAppStateFilePath() const { return mAppStateFilePath; }

	UFUNCTION(BlueprintPure, Category = "Recorder|AppState")
	FString GetLastErrorReason() const { return mLastErrorReason; }

	UFUNCTION(BlueprintPure, Category = "Recorder|AppState")
	FString ToString() const;

private:
	UVdjmRecordAppStateManifest* EnsureManifest();
	FString ResolveDefaultAppStateFilePath() const;
	bool RegisterStoreContext(UObject* worldContextObject);

	TWeakObjectPtr<UWorld> mCachedWorld;

	UPROPERTY(Transient)
	TObjectPtr<UVdjmRecordAppStateManifest> mAppStateManifest;

	FString mAppStateFilePath;
	FString mLastErrorReason;
};
