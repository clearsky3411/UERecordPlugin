#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "VdjmVcardDescriptorTypes.generated.h"

class UVcardDescriptorBase;

UENUM(BlueprintType)
enum class EVcardDescriptorApplyTiming : uint8
{
	EImmediate,
	ENextFrame,
	EStepPerFrame,
	EFlowDriven
};

UENUM(BlueprintType)
enum class EVcardDescriptorOpenPolicy : uint8
{
	EReplace,
	EAdd,
	EKeepIfSame,
	EHide
};

UENUM(BlueprintType)
enum class EVcardDescriptorRestorePolicy : uint8
{
	EUseDefault,
	EUsePreviousRuntimeState,
	EUseSavedState,
	EPreferPreviousThenDefault,
	EPreferSavedThenDefault
};

UENUM(BlueprintType)
enum class EVcardBackgroundItemType : uint8
{
	EColor,
	EImage,
	EVideo,
	EMaterial,
	ERuntimeUpload,
	ECustom
};

UENUM(BlueprintType)
enum class EVcardMotionItemType : uint8
{
	EPreset,
	EUploadedVideo,
	ECapturedRuntime,
	EExternal,
	ECustom
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardDescriptorApplyRequest
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	TObjectPtr<UUserWidget> HostWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	TObjectPtr<UObject> ContextObject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	TObjectPtr<UObject> PreviousStateObject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	TObjectPtr<UObject> SavedStateObject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FName InvocationSlotName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FName SourceSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	bool bAllowCreate = true;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardDescriptorApplyResult
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FName DescriptorId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	TArray<TObjectPtr<UUserWidget>> CreatedWidgets;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FString ErrorReason;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardWidgetAttachDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FName AttachId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FName TargetSlotName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	TObjectPtr<UObject> PayloadData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FSoftObjectPath PayloadPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	EVcardDescriptorOpenPolicy OpenPolicy = EVcardDescriptorOpenPolicy::EReplace;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	EVcardDescriptorRestorePolicy RestorePolicy = EVcardDescriptorRestorePolicy::EPreferPreviousThenDefault;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	EVcardDescriptorApplyTiming ApplyTiming = EVcardDescriptorApplyTiming::EImmediate;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	int32 DelayFrames = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	float DelaySeconds = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FName StartSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	FName FinishSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Descriptor")
	bool bAutoApplyPayload = true;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardOptionItemData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	TSoftObjectPtr<UObject> Thumbnail;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	TSoftObjectPtr<UObject> PayloadObject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	FSoftObjectPath PayloadPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	TArray<FName> Tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Option")
	bool bRuntimeGenerated = false;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardBackgroundItemData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	EVcardBackgroundItemType BackgroundType = EVcardBackgroundItemType::EColor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	TSoftObjectPtr<UObject> ImageAsset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	TSoftObjectPtr<UObject> MediaSource;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	TSoftObjectPtr<UObject> MaterialAsset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	FSoftObjectPath PayloadPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	TArray<FName> Tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Background")
	bool bRuntimeGenerated = false;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardMotionItemData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Motion")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Motion")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Motion")
	EVcardMotionItemType MotionType = EVcardMotionItemType::EPreset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Motion")
	TSoftObjectPtr<UObject> Thumbnail;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Motion")
	TSoftObjectPtr<UObject> MotionDataObject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Motion")
	FSoftObjectPath SourceVideoPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Motion")
	FSoftObjectPath AnalysisDataPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Motion")
	TArray<FName> Tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Motion")
	bool bRuntimeGenerated = false;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardTextFieldDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Text")
	FName FieldId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Text")
	FText Label;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Text")
	FText Placeholder;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Text")
	FText DefaultValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Text")
	bool bDefaultEnabled = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Text")
	int32 MaxLength = 128;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardToolDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	FName ToolId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	TSubclassOf<UUserWidget> ButtonWidgetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	TSoftObjectPtr<UObject> DefaultIcon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	TSoftObjectPtr<UObject> ActiveIcon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	FName CommandSignalTag = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	FName TargetDescriptorId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Tool")
	bool bLocksOtherTools = false;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardBottomSheetDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	FName DescriptorId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	TSubclassOf<UUserWidget> SheetWidgetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	TArray<FVcardWidgetAttachDescriptor> HeaderAttachments;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	FVcardWidgetAttachDescriptor ContentAttachment;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	EVcardDescriptorRestorePolicy RestorePolicy = EVcardDescriptorRestorePolicy::EPreferPreviousThenDefault;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	float InitialOpenRatio = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	float MinOpenRatio = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	float MaxOpenRatio = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	TArray<float> SnapRatios;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	bool bAllowDrag = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|BottomSheet")
	bool bAllowTapToggle = true;
};

USTRUCT(BlueprintType)
struct VDJMVCARD_API FVcardScreenDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Screen")
	FName ScreenId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Screen")
	TSubclassOf<UUserWidget> ScreenWidgetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Screen")
	TArray<FVcardWidgetAttachDescriptor> InitialAttachments;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Screen")
	TArray<FName> InitialSignalTags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vcard|Screen")
	EVcardDescriptorRestorePolicy RestorePolicy = EVcardDescriptorRestorePolicy::EPreferPreviousThenDefault;
};
