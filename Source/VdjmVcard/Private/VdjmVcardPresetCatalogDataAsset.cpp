#include "VdjmVcardPresetCatalogDataAsset.h"

#include "Engine/Texture2D.h"

namespace
{
	bool IsTextureKind(EVcardPresetAssetKind assetKind)
	{
		return assetKind == EVcardPresetAssetKind::ETexture || assetKind == EVcardPresetAssetKind::EImage;
	}

	bool DoesGroupMatch(FName requestedGroupKey, FName candidateGroupKey)
	{
		return requestedGroupKey.IsNone() || requestedGroupKey == candidateGroupKey;
	}

	FName ResolveFallbackItemId(const FVcardPresetItemData& presetItemData, int32 itemIndex)
	{
		if (!presetItemData.ItemId.IsNone())
		{
			return presetItemData.ItemId;
		}

		if (!presetItemData.PrimaryAssetSlotKey.IsNone())
		{
			return presetItemData.PrimaryAssetSlotKey;
		}

		return FName(*FString::Printf(TEXT("preset_%d"), itemIndex));
	}
}

FSoftObjectPath FVcardPresetAssetRef::GetResolvedAssetPath() const
{
	const FSoftObjectPath objectPath = AssetObject.ToSoftObjectPath();
	return objectPath.IsValid() ? objectPath : AssetPath;
}

void UVcardPresetItemRuntimeData::InitializeFromPresetItemData(const FVcardPresetItemData& presetItemData)
{
	PresetItemData = presetItemData;
}

bool UVcardPresetItemRuntimeData::FindAssetBySlot(FName slotKey, FVcardPresetAssetRef& outAssetRef) const
{
	for (const FVcardPresetAssetRef& assetRef : PresetItemData.Assets)
	{
		if (assetRef.SlotKey == slotKey)
		{
			outAssetRef = assetRef;
			return true;
		}
	}

	return false;
}

bool UVcardPresetItemRuntimeData::FindFirstAssetByKind(EVcardPresetAssetKind assetKind, FVcardPresetAssetRef& outAssetRef) const
{
	for (const FVcardPresetAssetRef& assetRef : PresetItemData.Assets)
	{
		if (assetRef.AssetKind == assetKind)
		{
			outAssetRef = assetRef;
			return true;
		}
	}

	return false;
}

bool UVcardPresetItemRuntimeData::GetPrimaryAssetRef(FVcardPresetAssetRef& outAssetRef) const
{
	if (!PresetItemData.PrimaryAssetSlotKey.IsNone() && FindAssetBySlot(PresetItemData.PrimaryAssetSlotKey, outAssetRef))
	{
		return true;
	}

	if (PresetItemData.PrimaryKind != EVcardPresetAssetKind::ENone && FindFirstAssetByKind(PresetItemData.PrimaryKind, outAssetRef))
	{
		return true;
	}

	if (PresetItemData.Assets.Num() > 0)
	{
		outAssetRef = PresetItemData.Assets[0];
		return true;
	}

	return false;
}

TArray<FVcardPresetItemData> UVcardPresetCatalogDataAsset::GetPresetItems() const
{
	TArray<FVcardPresetItemData> allPresetItems = PresetItems;
	for (const FVcardPresetCatalogChunk& presetChunk : PresetChunks)
	{
		for (FVcardPresetItemData presetItemData : presetChunk.PresetItems)
		{
			if (presetItemData.GroupKey.IsNone())
			{
				presetItemData.GroupKey = presetChunk.GroupKey;
			}

			if (presetItemData.PrimaryKind == EVcardPresetAssetKind::ENone)
			{
				presetItemData.PrimaryKind = presetChunk.PrimaryKind;
			}

			if (presetItemData.PrimaryAssetSlotKey.IsNone())
			{
				presetItemData.PrimaryAssetSlotKey = presetChunk.PrimaryAssetSlotKey;
			}

			allPresetItems.Add(presetItemData);
		}
	}

	return allPresetItems;
}

TArray<FName> UVcardPresetCatalogDataAsset::GetGroupKeys() const
{
	TArray<FName> groupKeys;
	for (const FVcardPresetItemData& presetItemData : PresetItems)
	{
		if (!presetItemData.GroupKey.IsNone())
		{
			groupKeys.AddUnique(presetItemData.GroupKey);
		}
	}

	for (const FVcardPresetCatalogChunk& presetChunk : PresetChunks)
	{
		if (!presetChunk.GroupKey.IsNone())
		{
			groupKeys.AddUnique(presetChunk.GroupKey);
		}

		for (const FVcardPresetItemData& presetItemData : presetChunk.PresetItems)
		{
			if (!presetItemData.GroupKey.IsNone())
			{
				groupKeys.AddUnique(presetItemData.GroupKey);
			}
		}
	}

	return groupKeys;
}

bool UVcardPresetCatalogDataAsset::FindPresetItemById(FName itemId, FVcardPresetItemData& outPresetItemData) const
{
	if (itemId.IsNone())
	{
		return false;
	}

	for (const FVcardPresetItemData& presetItemData : PresetItems)
	{
		if (presetItemData.ItemId == itemId)
		{
			outPresetItemData = presetItemData;
			return true;
		}
	}

	for (const FVcardPresetCatalogChunk& presetChunk : PresetChunks)
	{
		for (const FVcardPresetItemData& presetItemData : presetChunk.PresetItems)
		{
			if (presetItemData.ItemId == itemId)
			{
				outPresetItemData = presetItemData;
				return true;
			}
		}
	}

	return false;
}

bool UVcardPresetCatalogDataAsset::FindChunkByKey(FName chunkKey, FVcardPresetCatalogChunk& outChunk) const
{
	const int32 chunkIndex = FindChunkIndexByKey(chunkKey);
	if (chunkIndex == INDEX_NONE)
	{
		return false;
	}

	outChunk = PresetChunks[chunkIndex];
	return true;
}

void UVcardPresetCatalogDataAsset::GetPresetItemsByGroup(FName groupKey, TArray<FVcardPresetItemData>& outPresetItems) const
{
	outPresetItems.Reset();
	for (const FVcardPresetItemData& presetItemData : PresetItems)
	{
		if (presetItemData.bEnabled && DoesGroupMatch(groupKey, presetItemData.GroupKey))
		{
			outPresetItems.Add(presetItemData);
		}
	}

	for (const FVcardPresetCatalogChunk& presetChunk : PresetChunks)
	{
		if (!presetChunk.bEnabled)
		{
			continue;
		}

		for (FVcardPresetItemData presetItemData : presetChunk.PresetItems)
		{
			if (!presetItemData.bEnabled)
			{
				continue;
			}

			if (presetItemData.GroupKey.IsNone())
			{
				presetItemData.GroupKey = presetChunk.GroupKey;
			}

			if (DoesGroupMatch(groupKey, presetItemData.GroupKey))
			{
				outPresetItems.Add(presetItemData);
			}
		}
	}
}

void UVcardPresetCatalogDataAsset::ReplaceChunkItems(
	FName chunkKey,
	FText displayName,
	FName groupKey,
	EVcardPresetAssetKind primaryKind,
	FName primaryAssetSlotKey,
	const TArray<FVcardPresetItemData>& presetItems)
{
	if (chunkKey.IsNone())
	{
		return;
	}

	const int32 chunkIndex = FindChunkIndexByKey(chunkKey);
	FVcardPresetCatalogChunk* presetChunk = nullptr;
	if (chunkIndex == INDEX_NONE)
	{
		presetChunk = &PresetChunks.AddDefaulted_GetRef();
		presetChunk->ChunkKey = chunkKey;
	}
	else
	{
		presetChunk = &PresetChunks[chunkIndex];
	}

	presetChunk->DisplayName = displayName;
	presetChunk->GroupKey = groupKey;
	presetChunk->PrimaryKind = primaryKind;
	presetChunk->PrimaryAssetSlotKey = primaryAssetSlotKey;
	presetChunk->PresetItems = presetItems;
	presetChunk->bEnabled = true;
}

void UVcardPresetCatalogDataAsset::AppendChunkItems(
	FName chunkKey,
	FText displayName,
	FName groupKey,
	EVcardPresetAssetKind primaryKind,
	FName primaryAssetSlotKey,
	const TArray<FVcardPresetItemData>& presetItems)
{
	if (chunkKey.IsNone())
	{
		return;
	}

	const int32 chunkIndex = FindChunkIndexByKey(chunkKey);
	FVcardPresetCatalogChunk* presetChunk = nullptr;
	if (chunkIndex == INDEX_NONE)
	{
		presetChunk = &PresetChunks.AddDefaulted_GetRef();
		presetChunk->ChunkKey = chunkKey;
	}
	else
	{
		presetChunk = &PresetChunks[chunkIndex];
	}

	presetChunk->DisplayName = displayName;
	presetChunk->GroupKey = groupKey;
	presetChunk->PrimaryKind = primaryKind;
	presetChunk->PrimaryAssetSlotKey = primaryAssetSlotKey;
	presetChunk->bEnabled = true;

	for (const FVcardPresetItemData& presetItemData : presetItems)
	{
		const bool bAlreadyExists = presetChunk->PresetItems.ContainsByPredicate([&presetItemData](const FVcardPresetItemData& candidate)
		{
			return !presetItemData.ItemId.IsNone() && candidate.ItemId == presetItemData.ItemId;
		});

		if (!bAlreadyExists)
		{
			presetChunk->PresetItems.Add(presetItemData);
		}
	}
}

bool UVcardPresetCatalogDataAsset::ClearChunk(FName chunkKey)
{
	const int32 chunkIndex = FindChunkIndexByKey(chunkKey);
	if (chunkIndex == INDEX_NONE)
	{
		return false;
	}

	PresetChunks.RemoveAt(chunkIndex);
	return true;
}

bool UVcardPresetCatalogDataAsset::CreateTileItemDataStates(
	UObject* outer,
	TArray<UVcardTileItemDataState*>& outItemDataStates,
	FString& outErrorReason) const
{
	return CreateTileItemDataStatesByGroup(NAME_None, outer, outItemDataStates, outErrorReason);
}

bool UVcardPresetCatalogDataAsset::CreateTileItemDataStatesByGroup(
	FName groupKey,
	UObject* outer,
	TArray<UVcardTileItemDataState*>& outItemDataStates,
	FString& outErrorReason) const
{
	outItemDataStates.Reset();
	outErrorReason.Reset();

	UObject* resolvedOuter = IsValid(outer) ? outer : const_cast<UVcardPresetCatalogDataAsset*>(this);
	int32 itemIndex = 0;
	for (const FVcardPresetItemData& presetItemData : PresetItems)
	{
		if (!presetItemData.bEnabled || !DoesGroupMatch(groupKey, presetItemData.GroupKey))
		{
			++itemIndex;
			continue;
		}

		UVcardTileItemDataState* itemDataState = CreateTileItemDataState(resolvedOuter, presetItemData, itemIndex);
		if (IsValid(itemDataState))
		{
			outItemDataStates.Add(itemDataState);
		}

		++itemIndex;
	}

	for (const FVcardPresetCatalogChunk& presetChunk : PresetChunks)
	{
		if (!presetChunk.bEnabled)
		{
			continue;
		}

		for (FVcardPresetItemData presetItemData : presetChunk.PresetItems)
		{
			if (!presetItemData.bEnabled)
			{
				++itemIndex;
				continue;
			}

			if (presetItemData.GroupKey.IsNone())
			{
				presetItemData.GroupKey = presetChunk.GroupKey;
			}

			if (presetItemData.PrimaryKind == EVcardPresetAssetKind::ENone)
			{
				presetItemData.PrimaryKind = presetChunk.PrimaryKind;
			}

			if (presetItemData.PrimaryAssetSlotKey.IsNone())
			{
				presetItemData.PrimaryAssetSlotKey = presetChunk.PrimaryAssetSlotKey;
			}

			if (!DoesGroupMatch(groupKey, presetItemData.GroupKey))
			{
				++itemIndex;
				continue;
			}

			UVcardTileItemDataState* itemDataState = CreateTileItemDataState(resolvedOuter, presetItemData, itemIndex);
			if (IsValid(itemDataState))
			{
				outItemDataStates.Add(itemDataState);
			}

			++itemIndex;
		}
	}

	if (outItemDataStates.Num() <= 0)
	{
		outErrorReason = groupKey.IsNone()
			? TEXT("No enabled preset items were found.")
			: FString::Printf(TEXT("No enabled preset items were found in group '%s'."), *groupKey.ToString());
		return false;
	}

	return true;
}

int32 UVcardPresetCatalogDataAsset::FindChunkIndexByKey(FName chunkKey) const
{
	if (chunkKey.IsNone())
	{
		return INDEX_NONE;
	}

	return PresetChunks.IndexOfByPredicate([chunkKey](const FVcardPresetCatalogChunk& presetChunk)
	{
		return presetChunk.ChunkKey == chunkKey;
	});
}

UVcardTileItemDataState* UVcardPresetCatalogDataAsset::CreateTileItemDataState(
	UObject* outer,
	const FVcardPresetItemData& presetItemData,
	int32 itemIndex) const
{
	if (!presetItemData.bEnabled)
	{
		return nullptr;
	}

	UObject* resolvedOuter = IsValid(outer) ? outer : const_cast<UVcardPresetCatalogDataAsset*>(this);
	UVcardTileItemDataState* itemDataState = NewObject<UVcardTileItemDataState>(resolvedOuter);
	if (!IsValid(itemDataState))
	{
		return nullptr;
	}

	itemDataState->ItemId = ResolveFallbackItemId(presetItemData, itemIndex);
	itemDataState->DisplayName = presetItemData.DisplayName;
	itemDataState->Thumbnail = TSoftObjectPtr<UObject>(presetItemData.ThumbnailTexture.ToSoftObjectPath());
	itemDataState->Icon = itemDataState->Thumbnail;
	itemDataState->PayloadPath = ResolvePrimaryAssetPath(presetItemData);
	itemDataState->ActionDescriptorKey = presetItemData.ActionDescriptorKey;
	itemDataState->SelectSignalTag = presetItemData.SelectSignalTag;
	itemDataState->HoverSignalTag = presetItemData.HoverSignalTag;
	itemDataState->HoverBorderMaterial = presetItemData.HoverBorderMaterial;
	itemDataState->SelectedBorderMaterial = presetItemData.SelectedBorderMaterial;
	itemDataState->NormalTint = presetItemData.NormalTint;
	itemDataState->HoverTint = presetItemData.HoverTint;
	itemDataState->SelectedTint = presetItemData.SelectedTint;

	UVcardPresetItemRuntimeData* runtimeData = NewObject<UVcardPresetItemRuntimeData>(itemDataState);
	if (IsValid(runtimeData))
	{
		runtimeData->InitializeFromPresetItemData(presetItemData);
		itemDataState->PayloadData = runtimeData;
	}

	FString localImagePath;
	if (ResolveLocalImagePath(presetItemData, localImagePath))
	{
		itemDataState->SetLocalImageFileSource(localImagePath);
	}
	else if (presetItemData.ThumbnailTexture.ToSoftObjectPath().IsValid())
	{
		itemDataState->SetAssetTextureSource(ResolveTileSourceTexture(presetItemData), presetItemData.ThumbnailTexture);
	}

	return itemDataState;
}

TSoftObjectPtr<UTexture2D> UVcardPresetCatalogDataAsset::ResolveTileSourceTexture(const FVcardPresetItemData& presetItemData) const
{
	FVcardPresetAssetRef candidateAssetRef;
	if (!presetItemData.PrimaryAssetSlotKey.IsNone())
	{
		for (const FVcardPresetAssetRef& assetRef : presetItemData.Assets)
		{
			if (assetRef.SlotKey == presetItemData.PrimaryAssetSlotKey && IsTextureKind(assetRef.AssetKind))
			{
				return TSoftObjectPtr<UTexture2D>(assetRef.GetResolvedAssetPath());
			}
		}
	}

	for (const FVcardPresetAssetRef& assetRef : presetItemData.Assets)
	{
		if (IsTextureKind(assetRef.AssetKind))
		{
			return TSoftObjectPtr<UTexture2D>(assetRef.GetResolvedAssetPath());
		}
	}

	return TSoftObjectPtr<UTexture2D>();
}

bool UVcardPresetCatalogDataAsset::ResolveLocalImagePath(const FVcardPresetItemData& presetItemData, FString& outLocalImagePath) const
{
	outLocalImagePath.Reset();

	if (!presetItemData.PrimaryAssetSlotKey.IsNone())
	{
		for (const FVcardPresetAssetRef& assetRef : presetItemData.Assets)
		{
			if (assetRef.SlotKey == presetItemData.PrimaryAssetSlotKey &&
				assetRef.AssetKind == EVcardPresetAssetKind::EExternalFile &&
				!assetRef.ExternalPath.IsEmpty())
			{
				outLocalImagePath = assetRef.ExternalPath;
				return true;
			}
		}
	}

	for (const FVcardPresetAssetRef& assetRef : presetItemData.Assets)
	{
		if (assetRef.AssetKind == EVcardPresetAssetKind::EExternalFile && !assetRef.ExternalPath.IsEmpty())
		{
			outLocalImagePath = assetRef.ExternalPath;
			return true;
		}
	}

	return false;
}

FSoftObjectPath UVcardPresetCatalogDataAsset::ResolvePrimaryAssetPath(const FVcardPresetItemData& presetItemData) const
{
	if (!presetItemData.PrimaryAssetSlotKey.IsNone())
	{
		for (const FVcardPresetAssetRef& assetRef : presetItemData.Assets)
		{
			if (assetRef.SlotKey == presetItemData.PrimaryAssetSlotKey)
			{
				return assetRef.GetResolvedAssetPath();
			}
		}
	}

	if (presetItemData.PrimaryKind != EVcardPresetAssetKind::ENone)
	{
		for (const FVcardPresetAssetRef& assetRef : presetItemData.Assets)
		{
			if (assetRef.AssetKind == presetItemData.PrimaryKind)
			{
				return assetRef.GetResolvedAssetPath();
			}
		}
	}

	return presetItemData.Assets.Num() > 0 ? presetItemData.Assets[0].GetResolvedAssetPath() : FSoftObjectPath();
}
