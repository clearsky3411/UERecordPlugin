#pragma once

#include "CoreMinimal.h"
#include "VdjmAssetRegistryTypes.generated.h"

UENUM(BlueprintType)
enum class EVdjmAssetRegistryMessageSeverity : uint8
{
	Info,
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct VDJMASSETREGISTRY_API FVdjmAssetRegistryMessage
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	EVdjmAssetRegistryMessageSeverity Severity = EVdjmAssetRegistryMessageSeverity::Info;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Code;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Path;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Message;
};

USTRUCT(BlueprintType)
struct VDJMASSETREGISTRY_API FVdjmAssetRegistryPathRoot
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Key;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString DefaultPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString WinPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString AndroidPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString IosPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	bool bScan = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TMap<FString, FString> Meta;
};

USTRUCT(BlueprintType)
struct VDJMASSETREGISTRY_API FVdjmAssetRegistryDefinedRoot
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Key;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Root;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString RelativePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	bool bScan = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TMap<FString, FString> Meta;
};

USTRUCT(BlueprintType)
struct VDJMASSETREGISTRY_API FVdjmAssetRegistryVirtualFolder
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString VirtualPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString ParentPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Key;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Label;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TMap<FString, FString> Meta;
};

USTRUCT(BlueprintType)
struct VDJMASSETREGISTRY_API FVdjmAssetRegistryAssetType
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Key;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Kind;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString DefaultClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FString> Extensions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TMap<FString, FString> Meta;
};

USTRUCT(BlueprintType)
struct VDJMASSETREGISTRY_API FVdjmAssetRegistryAssetEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString AssetKey;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Type;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Root;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString RelativePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString VirtualPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Importance = TEXT("optional");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Class;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FString> Tags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TMap<FString, FString> Meta;
};

USTRUCT(BlueprintType)
struct VDJMASSETREGISTRY_API FVdjmAssetRegistryRequirement
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString VirtualPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Mode = TEXT("all");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Type;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	FString Importance = TEXT("required");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TMap<FString, FString> Meta;
};

USTRUCT(BlueprintType)
struct VDJMASSETREGISTRY_API FVdjmAssetRegistryDocument
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	int32 SchemaVersion = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TMap<FString, FString> Meta;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TMap<FString, FString> Defines;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FVdjmAssetRegistryPathRoot> Roots;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FVdjmAssetRegistryPathRoot> ExternalPaths;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FVdjmAssetRegistryDefinedRoot> DefinedRoots;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FVdjmAssetRegistryVirtualFolder> VirtualFolders;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FVdjmAssetRegistryAssetType> AssetTypes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FVdjmAssetRegistryAssetEntry> Assets;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FVdjmAssetRegistryRequirement> Requirements;
};

USTRUCT(BlueprintType)
struct VDJMASSETREGISTRY_API FVdjmAssetRegistryScanResult
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	int32 ScannedFileCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	int32 AddedAssetCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	int32 UpdatedAssetCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	int32 UnchangedAssetCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	int32 MissingRegisteredAssetCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FString> AddedAssetKeys;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FString> UpdatedAssetKeys;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VdjmAssetRegistry")
	TArray<FString> MissingAssetKeys;
};
