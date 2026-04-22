#pragma once

#include "CoreMinimal.h"
#include "VdjmRecorderSessionTypes.generated.h"

UENUM(BlueprintType)
enum class EVdjmRecorderSessionState : uint8
{
	ENew UMETA(DisplayName = "New"),
	EPreparing UMETA(DisplayName = "Preparing"),
	EReady UMETA(DisplayName = "Ready"),
	ERecording UMETA(DisplayName = "Recording"),
	EFinalizing UMETA(DisplayName = "Finalizing"),
	ETerminated UMETA(DisplayName = "Terminated"),
	EFailed UMETA(DisplayName = "Failed")
};
