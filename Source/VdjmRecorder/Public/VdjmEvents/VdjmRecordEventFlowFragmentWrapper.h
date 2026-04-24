#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VdjmRecordEventFlowFragmentWrapper.generated.h"

class UVdjmRecordEventFlowDataAsset;
class UVdjmRecordEventFlowRuntime;
class UVdjmRecordEventManager;
struct FVdjmRecordEventFlowFragment;

UENUM(BlueprintType)
enum class EVdjmRecordEventBuiltInPresetType : uint8
{
	ELogOnly UMETA(DisplayName = "Log Only"),
	ESetEnvOnly UMETA(DisplayName = "Set Env Only"),
	EBootstrapReuseBridge UMETA(DisplayName = "Bootstrap Reuse Bridge"),
	EBootstrapSpawnBridge UMETA(DisplayName = "Bootstrap Spawn Bridge"),
	EJumpToNextByTag UMETA(DisplayName = "Jump To Next By Tag")
};

UCLASS(BlueprintType)
class VDJMRECORDER_API UVdjmRecordEventFlowFragmentWrapper : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, Category = "Recorder|EventFlow|Wrapper")
	void BuildGeneratedJson();

	UFUNCTION(CallInEditor, Category = "Recorder|EventFlow|Wrapper")
	void BuildPreviewFlowDataAsset();

	UFUNCTION(CallInEditor, Category = "Recorder|EventFlow|Wrapper")
	void BuildPreviewRuntime();

	UFUNCTION(CallInEditor, Category = "Recorder|EventFlow|Wrapper")
	void ExecuteGeneratedFlowInCurrentWorld();

	UFUNCTION(CallInEditor, Category = "Recorder|EventFlow|Wrapper")
	void ClearPreviewArtifacts();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	EVdjmRecordEventBuiltInPresetType PresetType = EVdjmRecordEventBuiltInPresetType::ELogOnly;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	FSoftObjectPath EnvDataAssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	FSoftClassPath BridgeActorClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	FName TargetTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	bool bRequireLoadSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	bool bAbortIfNotFound = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	bool bAppendTestLogEvent = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	FName TestLogEventTag = TEXT("TestLog");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper", meta = (MultiLine = "true"))
	FString TestLogMessage = TEXT("Vdjm event flow log node executed.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	bool bTestLogAsWarning = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper", meta = (MultiLine = "true"))
	FString GeneratedFlowJson;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper", meta = (MultiLine = "true"))
	FString LastBuildError;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	TObjectPtr<UVdjmRecordEventFlowDataAsset> PreviewFlowDataAsset;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	TObjectPtr<UVdjmRecordEventFlowRuntime> PreviewFlowRuntime;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Recorder|EventFlow|Wrapper")
	TObjectPtr<UVdjmRecordEventManager> PreviewEventManager;

private:
	bool BuildSourceFragment(FVdjmRecordEventFlowFragment& OutFragment, FString& OutError) const;
	UObject* ResolveCurrentWorldContextObject() const;
	void UpdateGeneratedJson(const FVdjmRecordEventFlowFragment& InFragment, FString& OutError);
	void SetBuildError(const FString& InError);
	void ClearBuildError();
};
