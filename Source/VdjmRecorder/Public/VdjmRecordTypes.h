// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.generated.h"

class FVdjmVideoEncoderBase;
class UVdjmRecordUnit;
class UVdjmRecordResource;
class UVdjmRecordEnvCurrentInfo;
class AVdjmRecordBridgeActor;
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓			LOG Categories for Vdjm Recorder				↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
//	WITH_VDJM_WIN64_AVENCODER <- window 전용 매크로
DECLARE_LOG_CATEGORY_EXTERN(LogVdjmRecorderCore, Log, All);

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
				vdjm Result Macros and Common Result Codes
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/

/**	
	@brief Using int32 as the base type for VdjmResult to allow for a wide range of result codes while keeping it compact.  The convention is that non-negative values indicate success, while negative values indicate failure, similar to HRESULT in Windows programming.
 */
using VdjmResult = int32;

//	Result check macros
//	VdjmResult가 0 이상이면 성공, 0 미만이면 실패로 간주하는 규칙을 따릅니다. 이는 HRESULT와 유사한 방식입니다.
#define VDJM_SUCCEEDED(Result) ((VdjmResult)(Result) >= 0)
//	VdjmResult가 0 이상이면 성공, 0 미만이면 실패로 간주하는 규칙을 따릅니다. 이는 HRESULT와 유사한 방식입니다.
#define VDJM_FAILED(Result)    ((VdjmResult)(Result) < 0)


//	Common result codes
namespace VdjmResults
{
	// Success
	static constexpr VdjmResult Ok					= 0x00000000;
	static constexpr VdjmResult False				= 0x00000001;	// Success but false state

	// Generic failures
	static constexpr VdjmResult Fail				= (VdjmResult)0x80000000;
	static constexpr VdjmResult InvalidArg			= (VdjmResult)0x80000001;
	static constexpr VdjmResult InvalidState		= (VdjmResult)0x80000002;
	static constexpr VdjmResult NotInitialized		= (VdjmResult)0x80000003;
	static constexpr VdjmResult NotSupported		= (VdjmResult)0x80000004;

	// Encoder specific
	static constexpr VdjmResult BackendInitFailed	= (VdjmResult)0x80000100;
	static constexpr VdjmResult EncodeFailed		= (VdjmResult)0x80000101;
	static constexpr VdjmResult DrainFailed			= (VdjmResult)0x80000102;
	static constexpr VdjmResult SurfaceError		= (VdjmResult)0x80000103;

	// IO
	static constexpr VdjmResult FileOpenFailed		= (VdjmResult)0x80000200;
	static constexpr VdjmResult FileWriteFailed		= (VdjmResult)0x80000201;
}

/*
	Helpers
*/

FORCEINLINE constexpr bool VDJMRECORDER_API VdjmSucceeded(VdjmResult Result)
{
	return Result >= 0;
}

FORCEINLINE constexpr bool VDJMRECORDER_API VdjmFailed(VdjmResult Result)
{
	return Result < 0;
}

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
					Enumerations for Vdjm Recorder
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
/**
 * @brief EVdjmEncoderSteps는 인코더의 현재 단계 또는 상태를 나타내는 열거형입니다. 이 열거형은 인코딩 프로세스의 다양한 단계를 명확하게 구분하여 관리할 수 있도록 도와줍니다. 단 지금은 사용안함.
 */
UENUM(Blueprintable)
enum class EVdjmEncoderSteps : uint8
{
	EStarting,
	EEncoding,
	EStopping,
	EError,
	EIdle
};
//	TODO: 추후에 pause 도 만들어야하는데 그게 waiting 단계가 맞을까? 나중에 추가하자.
UENUM(Blueprintable)
enum class EVdjmResourceStatus : uint8
{
	ENone		= 0		UMETA(Hidden),
	ENew		= 1<<0	UMETA(ToolTip = "New / Uninitialized"),
	EReady		= 1<<1	UMETA(ToolTip = "Ready / Initialized"),
	ERunning 	= 1<<2	UMETA(ToolTip = "Running / Encoding"),
	EWaiting 	= 1<<3	UMETA(ToolTip = "Waiting / Waiting for Termination / Paused "),
	ETerminated = 1<<4	UMETA(ToolTip = "Terminated / Stopped"),
	EError		= 1<<5	UMETA(ToolTip = "Error"),
	EMax		= 1<<6	UMETA(Hidden)
};

#define VDJM_STATUS_RES_CHANGE(prevStatus,currentStatus) (int32(prevStatus) << 8 | int32(currentStatus))
enum class EStatusResourceChangeType : int32
{
	ENewToReady =  VDJM_STATUS_RES_CHANGE(EVdjmResourceStatus::ENew,EVdjmResourceStatus::EReady),
	EReadyToRunning = VDJM_STATUS_RES_CHANGE(EVdjmResourceStatus::EReady,EVdjmResourceStatus::ERunning),
	ERunningToWaiting = VDJM_STATUS_RES_CHANGE(EVdjmResourceStatus::ERunning,EVdjmResourceStatus::EWaiting),
	EWaitingToReady = VDJM_STATUS_RES_CHANGE(EVdjmResourceStatus::EWaiting,EVdjmResourceStatus::EReady),
	EWaitingToTerminated = VDJM_STATUS_RES_CHANGE(EVdjmResourceStatus::EWaiting,EVdjmResourceStatus::ETerminated)
};
#define VDJM_STATUS_RES_CHANGE_TYPE(prevStatus,currentStatus) static_cast<EStatusResourceChangeType>(VDJM_STATUS_RES_CHANGE(prevStatus,currentStatus))
UENUM(Blueprintable)
enum class EVdjmRecordEnvPlatform : uint8
{
	EWindows     UMETA(DisplayName = "Windows"),
	EAndroid     UMETA(DisplayName = "Android"),
	EIOS         UMETA(DisplayName = "iOS"),
	EMac         UMETA(DisplayName = "Mac"),
	ELinux       UMETA(DisplayName = "Linux"),
	EDefault     UMETA(DisplayName = "Default / Fallback")
};

UENUM(BlueprintType)
enum class EVdjmRecordBridgeInitStep : uint8
{
	EInitErrorEnd,
	EInitError,
	EInitializeWorldParts,
	EInitializeCurrentEnvironment,
	ECreateRecordResource,
	EPostResourceInitResolve,
	ECreatePipelines,
	EFinalizeInitialization,
	EComplete,
};

USTRUCT(Blueprintable)
struct VDJMRECORDER_API FVdjmEncoderStatus
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	EVdjmEncoderSteps CurrentStep = EVdjmEncoderSteps::EIdle;
	UPROPERTY(BlueprintReadOnly)
	float CurrentDurationSec = 0.0f;
	UPROPERTY(BlueprintReadOnly)
	int32 TotalFramesRecorded = 0;

	FVdjmEncoderStatus() = default;
	
	FVdjmEncoderStatus(const FVdjmEncoderStatus& other)
	{
		CurrentStep = other.CurrentStep;
		CurrentDurationSec = other.CurrentDurationSec;
		TotalFramesRecorded = other.TotalFramesRecorded;
	}
	FVdjmEncoderStatus(FVdjmEncoderStatus&& other) noexcept
	{
		CurrentStep = other.CurrentStep;
		CurrentDurationSec = other.CurrentDurationSec;
		TotalFramesRecorded = other.TotalFramesRecorded;
	}

	FVdjmEncoderStatus& operator=(const FVdjmEncoderStatus& other)
	{
		CurrentStep = other.CurrentStep;
		CurrentDurationSec = other.CurrentDurationSec;
		TotalFramesRecorded = other.TotalFramesRecorded;
		return *this;
	}
	FVdjmEncoderStatus& operator=(FVdjmEncoderStatus&& other) noexcept
	{
		CurrentStep = other.CurrentStep;
		CurrentDurationSec = other.CurrentDurationSec;
		TotalFramesRecorded = other.TotalFramesRecorded;
		return *this;
	}
	
	FVdjmEncoderStatus& Clear()
	{
		CurrentStep = EVdjmEncoderSteps::EIdle;
		CurrentDurationSec = 0.0f;
		TotalFramesRecorded = 0;
		return *this;
	}
	FVdjmEncoderStatus& OnStarting()
	{
		Clear();
		CurrentStep = EVdjmEncoderSteps::EStarting;
		return *this;
	}
	FVdjmEncoderStatus& OnEncoding(float durationSec, int32 totalFrames)
	{
		CurrentStep = EVdjmEncoderSteps::EEncoding;
		CurrentDurationSec = durationSec;
		TotalFramesRecorded = totalFrames;
		return *this;
	}
	FVdjmEncoderStatus& OnStopping()
	{
		CurrentStep = EVdjmEncoderSteps::EStopping;
		return *this;
	}
	FVdjmEncoderStatus& OnError()
	{
		CurrentStep = EVdjmEncoderSteps::EError;
		return *this;
	}
	template <typename FuncType>
	static void DbcGameThreadTask(FuncType&& Task)
	{
		if (IsInGameThread())
		{
			Task();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [task = Forward<FuncType>(Task)]() {
				task();
			});
		}
	}
	template <typename LambdaType>
	static void DbcRenderThreadTask(LambdaType&& Task)
	{
		if (IsInRenderingThread())
		{
			Task();
		}
		else
		{
			ENQUEUE_RENDER_COMMAND(VdjmInitTexturePool)(
			[Task](FRHICommandListImmediate& RHICmdList)
			{
				Task();
			}
		);
		}
	}
	
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVdjmEncoderStatusEvent,  EVdjmResourceStatus, prevStatus, EVdjmResourceStatus, currentStatus);
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	Config DataAsset
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
USTRUCT()
struct VDJMRECORDER_API FVdjmFunctionLibraryHelper 
{
	GENERATED_BODY()
	
	template<typename T>
	static T* TryGetRecordConfigureDataAsset(const FSoftObjectPath& configObjPath)
	{
		T* result = nullptr;

		if (configObjPath.IsAsset() && configObjPath.IsValid())
		{
			UObject* configResolved = configObjPath.ResolveObject();
			if (configResolved == nullptr)
			{
				configResolved = configObjPath.TryLoad();
				if (configResolved == nullptr)
				{
					UE_LOG(LogVdjmRecorderCore,Warning,TEXT("Failed to load default VcardConfigDataAsset %s synchronously."),*configObjPath.ToString());
				}
			}
			result = Cast<T>(configResolved);
		}
		return result;
	}
	static int32 ConvertToBitrateValue(float inValue) 
	{
		constexpr float CheckValue = 1000.f;
		constexpr float Mpbs = 1000000.0f;
		if (inValue <= 0.0f)
		{
			return 5000000;
		}
		
		if (inValue <= CheckValue)
		{
			return FMath::RoundToInt(inValue * Mpbs);
		}
		else
		{
			return FMath::RoundToInt(inValue);
		}	
	}
};
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓						USTRUCT	s							↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
USTRUCT()
struct VDJMRECORDER_API FVdjmRecordGlobalRules
{
	GENERATED_BODY()
	
	UPROPERTY(Category = "Recorder|Rules",
		EditAnywhere)
	float MaxRecordDurationSeconds = 15.0f;
	UPROPERTY(EditAnywhere, Category ="Recorder|Rules")
	int32 MinFrameRate = 24; // 최소 1FPS
	
	UPROPERTY(EditAnywhere, Category ="Recorder|Rules", meta=(ClampMin="24", ClampMax="60"))
	int32 MaxFrameRate = 30; // 30FPS 제한 (렌더 스레드 과부하 방지)
	
	UPROPERTY(Category = "Recorder|Rules",
			EditAnywhere)
	FIntVector Numthreads = FIntVector(8,8,1);
};

/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
	↓							UENUM s							↓
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓
*/
UENUM(Blueprintable)
enum class EVdjmRecordBitrateType : uint8
{
	EDefault UMETA(DisplayName="Default"),
	EUltra UMETA(DisplayName="Ultra"),
	EHigh UMETA(DisplayName="High"),
	EMediumHigh UMETA(DisplayName="MediumHigh"),
	EMedium UMETA(DisplayName="Medium"),
	EMdeiumLow UMETA(DisplayName="MediumLow"),
	ELow UMETA(DisplayName="Low"),
	ELowest UMETA(DisplayName="Lowest"),
};
UENUM(Blueprintable)
enum class EVdjmRecordSavePathDirectoryType : uint8
{
	EDefault UMETA(DisplayName="Default"),
	EProjectDir UMETA(DisplayName="ProjectDir"),	//	FPathUtil::ProjectDir()
	EDocumentsDir UMETA(DisplayName="DocumentsDir"),	//	FPathUtil::GetDocumentsDir()
	EVideoDir UMETA(DisplayName="VideoDir"),	//	FPathUtil::GetVideoDir()
	ECustomDir UMETA(DisplayName="CustomDir"),	//	CustomSaveDirectory
	ECustomSaverClass UMETA(DisplayName="CustomSaverClass"),	//	CustomFileSaverClass
};
/*
§	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
EVdjmRecordPipelineStages 		
	↓	↓	↓	↓	↓	↓	↓	↓	↓	↓	
*/
UENUM(Blueprintable)
enum class EVdjmRecordPipelineStages : uint8
{
	EUndefined UMETA(DisplayName="Undefined"),
	EPrepare UMETA(DisplayName="Prepare"),
	EComputeShader UMETA(DisplayName="ComputeShader"),
	EPostComputeShader UMETA(DisplayName="PostComputeShader"),
	EEncode UMETA(DisplayName="Encode"),
	EPostEncode UMETA(DisplayName="PostEncode"),
	EEncodeAndWrite UMETA(DisplayName="EncodeAndWrite"),
	EWriteToDisk UMETA(DisplayName="WriteToDisk"),
	ECustomStage UMETA(DisplayName="CustomStage"),
	ETest UMETA(DisplayName="Test"),
	EAllInOne UMETA(DisplayName="AllInOne"),
	ESurface UMETA(DisplayName="Surface"),
	ESurfaceEncode UMETA(DisplayName="SurfaceEncode"),
	ESurfaceEncodeAndWrite UMETA(DisplayName="SurfaceEncodeAndWrite"),
	EMax UMETA(Hidden),
	EPipelineBegin UMETA(DisplayName="PipelineBegin"),
	EPipelineEnd = EPipelineBegin + 7 UMETA(DisplayName="PipelineEnd"),
	EPipelineChain UMETA(DisplayName="PipelineChain"),
	ELimit = 255 UMETA(Hidden) 
};


/**
 * 
 */
UCLASS()
class VDJMRECORDER_API UVdjmRecordTypes : public UObject
{
	GENERATED_BODY()
};
