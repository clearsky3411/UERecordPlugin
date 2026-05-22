#pragma once

#include "CoreMinimal.h"
#include "VdjmEvents/VdjmRecordEventNode.h"
#include "VdjmVcardTileImageLoadBroker.h"
#include "VdjmVcardTileImageEventNodes.generated.h"

/**
 * Ensures the tile image load broker exists and configures its queue/storage settings.
 *
 * Responsibility:
 * - Resolve or spawn a world-level broker actor.
 * - Configure user image storage and bounded timer queue settings.
 * - Store the broker in flow runtime/context lookup when requested.
 *
 * Must not:
 * - Load tile images by itself. Use the broker queue after this node.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ToolTip = "Flow에서 Vcard tile image load broker를 찾거나 생성하고 저장 위치/큐 설정을 적용합니다."))
class VDJMVCARD_API UVcardEventEnsureTileImageLoadBrokerNode : public UVdjmRecordEventBase
{
	GENERATED_BODY()

public:
	virtual FVdjmRecordEventResult ExecuteEvent_Implementation(UVdjmRecordEventManager* EventManager, AVdjmRecordBridgeActor* BridgeActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Lookup", meta = (ToolTip = "현재 flow session에 broker를 저장할 로컬 key입니다. None이면 runtime slot 저장을 건너뜁니다."))
	FName RuntimeSlotKey = FName(TEXT("l.tile.image.broker"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Lookup", meta = (ToolTip = "WorldContextSubsystem에 broker를 등록할 글로벌 key입니다. None이면 context 등록을 건너뜁니다."))
	FName ContextKey = FName(TEXT("g.tile.image.broker"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Lookup", meta = (ToolTip = "true면 기존 broker를 runtime/context/world에서 찾아 재사용합니다. false면 새 broker를 스폰합니다."))
	bool bReuseExistingBroker = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Storage", meta = (ToolTip = "유저 이미지 파일을 저장할 기본 위치입니다. PersistentDownload가 모바일 기본값으로 적합합니다."))
	EVcardTileImageStorageMode StorageMode = EVcardTileImageStorageMode::EPersistentDownload;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Storage", meta = (ToolTip = "StorageMode로 계산된 base path 아래에 붙일 상대 폴더입니다."))
	FString RelativeFolder = TEXT("Vcard/UserImages");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Storage", meta = (ToolTip = "StorageMode가 CustomAbsolute일 때 사용할 절대 경로입니다."))
	FString CustomAbsolutePath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Storage", meta = (ToolTip = "true면 저장 폴더가 없을 때 생성합니다."))
	bool bCreateIfMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Queue", meta = (ClampMin = "1", ToolTip = "동시에 처리할 최대 로딩 작업 수입니다."))
	int32 MaxActiveJobs = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Queue", meta = (ClampMin = "1", ToolTip = "타이머 한 step에서 새로 시작할 최대 작업 수입니다."))
	int32 MaxJobsPerStep = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Queue", meta = (ClampMin = "0.001", ToolTip = "작업이 있을 때만 도는 broker timer 간격입니다."))
	float StepIntervalSeconds = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Image", meta = (ClampMin = "1", ToolTip = "로컬 이미지 파일에서 생성할 메모리 thumbnail 크기입니다."))
	int32 ThumbnailSize = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Image", meta = (ClampMin = "1", ToolTip = "로컬 원본 이미지를 로딩할 때 긴 축을 제한할 최대 크기입니다."))
	int32 MaxSourceTextureSize = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Behavior", meta = (ToolTip = "true면 broker를 WorldContextSubsystem에 weak context로 등록합니다."))
	bool bRegisterContext = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Behavior", meta = (ToolTip = "true면 broker를 현재 flow runtime slot에 저장합니다."))
	bool bStoreRuntimeSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Behavior", meta = (ToolTip = "true면 broker 생성/설정 실패 시에도 flow를 성공으로 진행합니다."))
	bool bSucceedIfMissing = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vcard|EventNode|TileImage|Debug", meta = (ToolTip = "디버그용입니다. true면 broker 보장 결과를 LogVdjmVcard에 남깁니다."))
	bool bLogResult = true;
};
