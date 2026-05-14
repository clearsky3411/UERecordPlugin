#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VdjmAssetRegistryTypes.h"
#include "VdjmAssetRegistryBlueprintLibrary.generated.h"

UCLASS()
class VDJMASSETREGISTRY_API UVdjmAssetRegistryBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool GetDefaultRegistryFilePath(FString& outFilePath);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static FString MakeAssetKey(const FString& type, const FString& root, const FString& relativePath);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool LoadDefaultRegistry(FVdjmAssetRegistryDocument& outRegistry, TArray<FVdjmAssetRegistryMessage>& outMessages);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool SaveDefaultRegistry(const FVdjmAssetRegistryDocument& registry, TArray<FVdjmAssetRegistryMessage>& outMessages);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool ValidateRegistry(const FVdjmAssetRegistryDocument& registry, TArray<FVdjmAssetRegistryMessage>& outMessages);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool ResolveRegistryRootFullPath(
		const FVdjmAssetRegistryDocument& registry,
		const FString& rootKey,
		FString& outFullPath,
		TArray<FVdjmAssetRegistryMessage>& outMessages);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool ScanDefaultRegistry(
		const bool bRegisterDiscoveredAssets,
		const bool bSaveAfterScan,
		FVdjmAssetRegistryDocument& outRegistry,
		FVdjmAssetRegistryScanResult& outScanResult,
		TArray<FVdjmAssetRegistryMessage>& outMessages);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool ScanDefaultRegistryWithRequest(
		const FVdjmAssetRegistryScanRequest& scanRequest,
		FVdjmAssetRegistryDocument& outRegistry,
		FVdjmAssetRegistryScanResult& outScanResult,
		TArray<FVdjmAssetRegistryMessage>& outMessages);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool ScanRegistryWithRequest(
		const FVdjmAssetRegistryScanRequest& scanRequest,
		FVdjmAssetRegistryDocument& registry,
		FVdjmAssetRegistryScanResult& outScanResult,
		TArray<FVdjmAssetRegistryMessage>& outMessages);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool RemoveAssetFromDefaultRegistry(
		const FString& assetKey,
		const bool bSaveAfterRemove,
		FVdjmAssetRegistryDocument& outRegistry,
		TArray<FVdjmAssetRegistryMessage>& outMessages);

	UFUNCTION(BlueprintCallable, Category = "VdjmAssetRegistry")
	static bool GetRegistrySummary(const FVdjmAssetRegistryDocument& registry, FString& outSummary);
};
