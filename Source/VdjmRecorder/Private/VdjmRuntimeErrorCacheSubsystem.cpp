#include "VdjmRuntimeErrorCacheSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

UVdjmRuntimeErrorCacheSubsystem* UVdjmRuntimeErrorCacheSubsystem::Get(UObject* worldContextObject)
{
	if (worldContextObject == nullptr || GEngine == nullptr)
	{
		return nullptr;
	}

	UWorld* world = GEngine->GetWorldFromContextObject(worldContextObject, EGetWorldErrorMode::ReturnNull);
	if (world == nullptr)
	{
		return nullptr;
	}

	UGameInstance* gameInstance = world->GetGameInstance();
	return gameInstance != nullptr ? gameInstance->GetSubsystem<UVdjmRuntimeErrorCacheSubsystem>() : nullptr;
}

bool UVdjmRuntimeErrorCacheSubsystem::PushRuntimeErrorLog(
	FName sourceTag,
	FName operationTag,
	const FString& message,
	const FString& details,
	UObject* sourceObject)
{
	CachedRuntimeErrorLog.bHasCachedError = true;
	CachedRuntimeErrorLog.ErrorId = FGuid::NewGuid();
	CachedRuntimeErrorLog.TimestampUtc = FDateTime::UtcNow();
	CachedRuntimeErrorLog.SourceTag = sourceTag;
	CachedRuntimeErrorLog.OperationTag = operationTag;
	CachedRuntimeErrorLog.Message = message;
	CachedRuntimeErrorLog.Details = details;
	CachedRuntimeErrorLog.SourceObjectName = IsValid(sourceObject) ? sourceObject->GetName() : FString();
	CachedRuntimeErrorLog.SourceObjectClassName = IsValid(sourceObject) && sourceObject->GetClass() != nullptr
		? sourceObject->GetClass()->GetName()
		: FString();

	return true;
}

bool UVdjmRuntimeErrorCacheBlueprintLibrary::PushRuntimeErrorLog(
	UObject* worldContextObject,
	FName sourceTag,
	FName operationTag,
	const FString& message,
	const FString& details,
	UObject* sourceObject)
{
	UVdjmRuntimeErrorCacheSubsystem* errorCacheSubsystem = UVdjmRuntimeErrorCacheSubsystem::Get(worldContextObject);
	return errorCacheSubsystem != nullptr
		? errorCacheSubsystem->PushRuntimeErrorLog(sourceTag, operationTag, message, details, sourceObject)
		: false;
}
