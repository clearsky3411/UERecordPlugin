#include "VdjmEvents/VdjmRecordEventFlowFragmentWrapper.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "VdjmEvents/VdjmRecordEventFlowDataAsset.h"
#include "VdjmEvents/VdjmRecordEventFlowFragment.h"
#include "VdjmEvents/VdjmRecordEventFlowRuntime.h"
#include "VdjmEvents/VdjmRecordEventManager.h"

namespace
{
	int32 GetWorldPriority(const UWorld* World)
	{
		if (World == nullptr)
		{
			return -1;
		}

		switch (World->WorldType)
		{
		case EWorldType::PIE:
			return 4;
		case EWorldType::Game:
			return 3;
		case EWorldType::Editor:
			return 2;
		case EWorldType::EditorPreview:
			return 1;
		default:
			return 0;
		}
	}
}

void UVdjmRecordEventFlowFragmentWrapper::BuildGeneratedJson()
{
	FVdjmRecordEventFlowFragment Fragment;
	FString BuildError;
	if (!BuildSourceFragment(Fragment, BuildError))
	{
		SetBuildError(BuildError);
		return;
	}

	UpdateGeneratedJson(Fragment, BuildError);
	if (!BuildError.IsEmpty())
	{
		SetBuildError(BuildError);
		return;
	}

	ClearBuildError();
	MarkPackageDirty();
}

void UVdjmRecordEventFlowFragmentWrapper::BuildPreviewFlowDataAsset()
{
	FVdjmRecordEventFlowFragment Fragment;
	FString BuildError;
	if (!BuildSourceFragment(Fragment, BuildError))
	{
		SetBuildError(BuildError);
		return;
	}

	UpdateGeneratedJson(Fragment, BuildError);
	if (!BuildError.IsEmpty())
	{
		SetBuildError(BuildError);
		return;
	}

	UVdjmRecordEventFlowDataAsset* NewPreviewAsset = nullptr;
	if (!Fragment.BuildFlowDataAsset(GetTransientPackage(), NewPreviewAsset, BuildError))
	{
		PreviewFlowDataAsset = nullptr;
		SetBuildError(BuildError);
		return;
	}

	PreviewFlowDataAsset = NewPreviewAsset;
	ClearBuildError();
	MarkPackageDirty();
}

void UVdjmRecordEventFlowFragmentWrapper::BuildPreviewRuntime()
{
	FVdjmRecordEventFlowFragment Fragment;
	FString BuildError;
	if (!BuildSourceFragment(Fragment, BuildError))
	{
		SetBuildError(BuildError);
		return;
	}

	UpdateGeneratedJson(Fragment, BuildError);
	if (!BuildError.IsEmpty())
	{
		SetBuildError(BuildError);
		return;
	}

	UVdjmRecordEventFlowRuntime* NewPreviewRuntime = nullptr;
	if (!Fragment.BuildRuntime(GetTransientPackage(), NewPreviewRuntime, BuildError))
	{
		PreviewFlowRuntime = nullptr;
		SetBuildError(BuildError);
		return;
	}

	PreviewFlowRuntime = NewPreviewRuntime;
	ClearBuildError();
	MarkPackageDirty();
}

void UVdjmRecordEventFlowFragmentWrapper::ExecuteGeneratedFlowInCurrentWorld()
{
	if (GeneratedFlowJson.IsEmpty())
	{
		BuildGeneratedJson();
		if (!LastBuildError.IsEmpty())
		{
			return;
		}
	}

	UObject* WorldContextObject = ResolveCurrentWorldContextObject();
	if (WorldContextObject == nullptr)
	{
		SetBuildError(TEXT("Could not resolve a current world context object."));
		return;
	}

	PreviewEventManager = UVdjmRecordEventManager::CreateEventManager(WorldContextObject);
	if (PreviewEventManager == nullptr)
	{
		SetBuildError(TEXT("Failed to create preview event manager."));
		return;
	}

	FString BuildError;
	if (!PreviewEventManager->StartEventFlowFromJsonString(GeneratedFlowJson, BuildError, true))
	{
		SetBuildError(BuildError);
		return;
	}

	ClearBuildError();
	MarkPackageDirty();
}

void UVdjmRecordEventFlowFragmentWrapper::ClearPreviewArtifacts()
{
	GeneratedFlowJson.Reset();
	LastBuildError.Reset();
	PreviewFlowDataAsset = nullptr;
	PreviewFlowRuntime = nullptr;
	PreviewEventManager = nullptr;
	MarkPackageDirty();
}

bool UVdjmRecordEventFlowFragmentWrapper::BuildSourceFragment(FVdjmRecordEventFlowFragment& OutFragment, FString& OutError) const
{
	OutError.Reset();
	OutFragment.Reset();

	switch (PresetType)
	{
	case EVdjmRecordEventBuiltInPresetType::ELogOnly:
		OutFragment = VdjmRecordEventFlowPresets::MakeLogOnlyFlowFragment(
			TestLogMessage,
			bTestLogAsWarning,
			TestLogEventTag);
		break;
	case EVdjmRecordEventBuiltInPresetType::ESetEnvOnly:
		OutFragment = VdjmRecordEventFlowPresets::MakeSetEnvOnlyFlowFragment(
			EnvDataAssetPath,
			bRequireLoadSuccess);
		break;
	case EVdjmRecordEventBuiltInPresetType::EBootstrapReuseBridge:
		OutFragment = VdjmRecordEventFlowPresets::MakeBootstrapReuseBridgeFlowFragment(
			EnvDataAssetPath,
			bRequireLoadSuccess);
		break;
	case EVdjmRecordEventBuiltInPresetType::EBootstrapSpawnBridge:
		OutFragment = VdjmRecordEventFlowPresets::MakeBootstrapSpawnBridgeFlowFragment(
			EnvDataAssetPath,
			BridgeActorClassPath,
			bRequireLoadSuccess);
		break;
	case EVdjmRecordEventBuiltInPresetType::EJumpToNextByTag:
		if (TargetTag.IsNone())
		{
			OutError = TEXT("TargetTag must be set for JumpToNextByTag preset.");
			return false;
		}
		OutFragment = VdjmRecordEventFlowPresets::MakeJumpToNextByTagFlowFragment(
			TargetTag,
			bAbortIfNotFound);
		break;
	default:
		OutError = TEXT("Unsupported preset type.");
		return false;
	}

	if (bAppendTestLogEvent && PresetType != EVdjmRecordEventBuiltInPresetType::ELogOnly)
	{
		FVdjmRecordEventNodeFragment LogNode = VdjmRecordEventFlowPresets::MakeLogNode(
			TestLogMessage,
			bTestLogAsWarning);
		if (!TestLogEventTag.IsNone())
		{
			LogNode.SetEventTag(TestLogEventTag);
		}
		OutFragment.AppendEvent(LogNode);
	}

	if (OutFragment.IsEmpty())
	{
		OutError = TEXT("Built fragment is empty.");
		return false;
	}

	return true;
}

UObject* UVdjmRecordEventFlowFragmentWrapper::ResolveCurrentWorldContextObject() const
{
	if (GEngine == nullptr)
	{
		return nullptr;
	}

	UWorld* BestWorld = nullptr;
	int32 BestPriority = -1;

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* CandidateWorld = WorldContext.World();
		const int32 Priority = GetWorldPriority(CandidateWorld);
		if (Priority > BestPriority)
		{
			BestWorld = CandidateWorld;
			BestPriority = Priority;
		}
	}

	return BestWorld;
}

void UVdjmRecordEventFlowFragmentWrapper::UpdateGeneratedJson(const FVdjmRecordEventFlowFragment& InFragment, FString& OutError)
{
	OutError.Reset();
	GeneratedFlowJson = InFragment.WriteJsonString(true);
	if (GeneratedFlowJson.IsEmpty())
	{
		OutError = TEXT("Generated flow json is empty.");
	}
}

void UVdjmRecordEventFlowFragmentWrapper::SetBuildError(const FString& InError)
{
	LastBuildError = InError;
	MarkPackageDirty();
}

void UVdjmRecordEventFlowFragmentWrapper::ClearBuildError()
{
	LastBuildError.Reset();
}
