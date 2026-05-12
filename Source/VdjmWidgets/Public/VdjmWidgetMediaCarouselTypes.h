#pragma once

#include "CoreMinimal.h"
#include "VdjmRecorderCore.h"
#include "VdjmWidgetMediaCarouselTypes.generated.h"

UENUM(BlueprintType)
enum class EVdjmWidgetMediaCardState : uint8
{
	EEmpty UMETA(DisplayName = "Empty"),
	EWaiting UMETA(DisplayName = "Waiting"),
	EVisible UMETA(DisplayName = "Visible"),
	EActive UMETA(DisplayName = "Active"),
	EHidden UMETA(DisplayName = "Hidden"),
	EError UMETA(DisplayName = "Error")
};

UENUM(BlueprintType)
enum class EVdjmWidgetMediaCarouselLayoutState : uint8
{
	EEmpty UMETA(DisplayName = "Empty"),
	ESingle UMETA(DisplayName = "Single"),
	EPair UMETA(DisplayName = "Pair"),
	EWindow UMETA(DisplayName = "Window"),
	EOverflow UMETA(DisplayName = "Overflow")
};

UENUM(BlueprintType)
enum class EVdjmWidgetMediaCarouselInputAction : uint8
{
	ENone UMETA(DisplayName = "None"),
	EPressed UMETA(DisplayName = "Pressed"),
	EMoved UMETA(DisplayName = "Moved"),
	EReleased UMETA(DisplayName = "Released"),
	EClicked UMETA(DisplayName = "Clicked"),
	ESwipe UMETA(DisplayName = "Swipe")
};

UENUM(BlueprintType)
enum class EVdjmWidgetMediaCarouselActiveAfterRefreshPolicy : uint8
{
	EKeepRecordId UMETA(DisplayName = "Keep Record Id"),
	EKeepIndex UMETA(DisplayName = "Keep Index"),
	EFirst UMETA(DisplayName = "First"),
	ELatest UMETA(DisplayName = "Latest")
};

UENUM(BlueprintType)
enum class EVdjmWidgetMediaSourceKind : uint8
{
	ENone UMETA(DisplayName = "None"),
	EThumbnailFile UMETA(DisplayName = "Thumbnail File"),
	EPreviewClipFile UMETA(DisplayName = "Preview Clip File"),
	EOutputFile UMETA(DisplayName = "Output File"),
	EPlaybackLocator UMETA(DisplayName = "Playback Locator"),
	EPublishedContentUri UMETA(DisplayName = "Published Content Uri")
};

USTRUCT(BlueprintType)
struct VDJMWIDGETS_API FVdjmWidgetMediaCardSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media")
	int32 SourceRegistryIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media")
	FVdjmRecordMediaRegistryEntry RegistryEntry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media")
	bool bValid = false;
};

USTRUCT(BlueprintType)
struct VDJMWIDGETS_API FVdjmWidgetMediaCarouselLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel", meta = (ClampMin = "1", ClampMax = "32"))
	int32 VisibleCardCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel", meta = (ClampMin = "-1", ClampMax = "31"))
	int32 ActiveSlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NormalizedSpacing = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	FVector2D ProgressDirection = FVector2D(1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float ActiveScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float FarScale = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActiveOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FarOpacity = 0.55f;
};

USTRUCT(BlueprintType)
struct VDJMWIDGETS_API FVdjmWidgetMediaCarouselSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VdjmWidgets|Media|Carousel")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "VdjmWidgets|Media|Carousel")
	int32 SourceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "VdjmWidgets|Media|Carousel")
	float SlotOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VdjmWidgets|Media|Carousel")
	EVdjmWidgetMediaCardState TargetCardState = EVdjmWidgetMediaCardState::EEmpty;

	UPROPERTY(BlueprintReadOnly, Category = "VdjmWidgets|Media|Carousel")
	FVector2D RenderTranslation = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VdjmWidgets|Media|Carousel")
	FVector2D RenderScale = FVector2D(1.0f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "VdjmWidgets|Media|Carousel")
	float RenderOpacity = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VdjmWidgets|Media|Carousel")
	int32 ZOrder = 0;
};

USTRUCT(BlueprintType)
struct VDJMWIDGETS_API FVdjmWidgetMediaCarouselInputPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	EVdjmWidgetMediaCarouselInputAction Action = EVdjmWidgetMediaCarouselInputAction::ENone;

	UPROPERTY(BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	int32 CardIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	int32 SourceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	EVdjmWidgetMediaCardState CardState = EVdjmWidgetMediaCardState::EEmpty;

	UPROPERTY(BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	FVector2D ScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	FVector2D Delta = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "VdjmWidgets|Media|Carousel")
	bool bHasSource = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVdjmWidgetMediaCarouselRefreshDelegate, bool, bSuccess, const FString&, ErrorReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVdjmWidgetMediaCarouselActiveSourceChangedDelegate, int32, PreviousSourceIndex, int32, NewSourceIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVdjmWidgetMediaCarouselInputDelegate, const FVdjmWidgetMediaCarouselInputPayload&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVdjmWidgetMediaCardStateDelegate, EVdjmWidgetMediaCardState, PreviousState, EVdjmWidgetMediaCardState, NewState);
