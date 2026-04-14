// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmRecordTypes.generated.h"

class FVdjmVideoEncoderBase;
class UVdjmRecordUnit;
class UVdjmRecordResource;
class UVdjmRecordEnvCurrentInfo_deprecated;

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

UENUM(Blueprintable)
enum class EVdjmRecordQualityTiers : uint8
{
	EUndefined UMETA(DisplayName="Undefined",Hidden),
	EDefault UMETA(DisplayName="Default"),
	EUltra UMETA(DisplayName="Ultra"),
	EHigh UMETA(DisplayName="High"),
	EMediumHigh UMETA(DisplayName="MediumHigh"),
	EMedium UMETA(DisplayName="Medium"),
	EMdeiumLow UMETA(DisplayName="MediumLow"),
	ELow UMETA(DisplayName="Low"),
	ELowest UMETA(DisplayName="Lowest"),
	ECustom UMETA(DisplayName="Custom"),
	EResolvedTier UMETA(DisplayName="ResolvedTier",Hidden),
	EMax UMETA(Hidden)
};
enum class EVdjmRecordContentComplexity : uint8
{
	EUIStatic,
	EGameplay,
	EHighMotion
};
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

struct FVdjmEncoderInitRequestAudio;

namespace VdjmRecordUtils
{
	namespace Platforms
	{
		inline EVdjmRecordEnvPlatform GetTargetPlatform()
		{
#if PLATFORM_WINDOWS
			return EVdjmRecordEnvPlatform::EWindows;
#elif PLATFORM_ANDROID || defined(__RESHARPER__)
			return EVdjmRecordEnvPlatform::EAndroid;
#elif PLATFORM_IOS
			return EVdjmRecordEnvPlatform::EIos;
#else
			return EVdjmRecordEnvPlatform::EUnknown;
#endif
		}
	}
	
	namespace Validations
	{
		bool DbcValidateResolution(
			const FIntPoint& inResolution,
			FIntPoint& outSafeResolution,
			const TCHAR* debugOwner);

		bool DbcValidateBitrate(
			const int32 inBitrate,
			int32& outSafeBitrate,
			const TCHAR* debugOwner);

		bool DbcValidateOutputFilePath(
			const FString& inFilePath,
			FString& outSafeFilePath,
			const TCHAR* debugOwner);

		bool DbcValidateAudioConfig(
			const FVdjmEncoderInitRequestAudio& inAudioConfig,
			const TCHAR* debugOwner);
	}
	namespace FeaturePresets
	{
		inline FIntPoint GetPresetFeatureResolution_Window(uint32 tier)
		{
			const TArray<FIntPoint> resultResolution =
			{
				FIntPoint(7680, 4320),	// 8K, Enthusiast Monitor / TV
				FIntPoint(3840, 2160),	// UHD (4K), High-End PC Monitor
				FIntPoint(3440, 1440),	// UWQHD, 21:9 Ultrawide Monitor
				FIntPoint(2560, 1440),	// QHD (1440p), 2K Gaming Monitor
				FIntPoint(1920, 1200),	// WUXGA (1200p), 16:10 Standard Monitor
				FIntPoint(1920, 1080),	// FHD (1080p), Standard PC Monitor
				FIntPoint(1280, 720),	// HD (720p), Steam Deck (1280x800 for 16:10)
			};
			uint32 maxTierNum = resultResolution.Num();
			if (tier < maxTierNum)
			{
				return resultResolution[tier];
			}
			else
			{
				return resultResolution[tier % maxTierNum];
			}
		}
		inline FIntPoint GetPresetFeatureResolution_Android(uint32 tier)
		{
			const TArray<FIntPoint> resultResolution =
			{
				FIntPoint(2560, 1600),	// Tablet (Landscape default), Samsung Galaxy Tab S8/S9
				FIntPoint(1812, 2176),	// Foldable (Inner Screen), Samsung Galaxy Z Fold 5
				FIntPoint(1440, 3120),	// Premium Flagship, Samsung Galaxy S24 Ultra / Pixel 8 Pro
				FIntPoint(1080, 2400),	// Standard Flagship 2, Google Pixel 7/8
				FIntPoint(1080, 2340),	// Standard Flagship, Samsung Galaxy S22/S23 (SM-S901/S911)
				FIntPoint(720, 1600),	// Budget Tier (HD+), Samsung Galaxy A12 / Older phones
			};
			uint32 maxTierNum = resultResolution.Num();
			if (tier < maxTierNum)
			{
				return resultResolution[tier];
			}
			else
			{
				return resultResolution[tier % maxTierNum];
			}
		}
		inline FIntPoint GetPresetFeatureResolution_Ios(uint32 tier)
		{
			const TArray<FIntPoint> resultResolution =
			{
				FIntPoint(2048, 2732),	// iPad Pro 12.9-inch / 13-inch
				FIntPoint(1668, 2388),	// iPad Pro 11-inch
				FIntPoint(1290, 2796),	// iPhone 14/15/16 Pro Max
				FIntPoint(1284, 2778),	// iPhone 12/13/14 Pro Max & Plus
				FIntPoint(1179, 2556),	// iPhone 14 Pro / 15 Pro / 16 Pro
				FIntPoint(1170, 2532),	// iPhone 12 / 13 / 14 (Standard size)
				FIntPoint(750, 1334),	// iPhone SE (3rd Gen) / Older iPhones (8, 7)
			};
			uint32 maxTierNum = resultResolution.Num();
			if (tier < maxTierNum)
			{
				return resultResolution[tier];
			}
			else
			{
				return resultResolution[tier % maxTierNum];
			}
		}
		inline FIntPoint GetPresetFeatureResolution_Mac(uint32 tier)
		{
			const TArray<FIntPoint> resultResolution =
			{
				FIntPoint(6016, 3384),	// Pro Display XDR (6K)
				FIntPoint(5120, 2880),	// Apple Studio Display / 27" iMac (5K Retina)
				FIntPoint(3456, 2234),	// MacBook Pro 16" (M1/M2/M3)
				FIntPoint(3024, 1964),	// MacBook Pro 14" (M1/M2/M3)
				FIntPoint(2560, 1664),	// MacBook Air 13" (M2/M3) - Liquid Retina (Notch included)
				FIntPoint(2560, 1600),	// MacBook Air 13" (M1) / Older MacBook Pro
			};
			uint32 maxTierNum = resultResolution.Num();
			if (tier < maxTierNum)
			{
				return resultResolution[tier];
			}
			else
			{
				return resultResolution[tier % maxTierNum];
			}
		}
		inline FIntPoint GetPresetFeatureResolution_Linux(uint32 tier)
		{
			return GetPresetFeatureResolution_Window(tier);
		}
		inline FIntPoint GetPresetFeatureResolution(uint32 tier)
		{
			switch (VdjmRecordUtils::Platforms::GetTargetPlatform())
			{
			case EVdjmRecordEnvPlatform::EWindows:
				return GetPresetFeatureResolution_Window(tier);
				break;
			case EVdjmRecordEnvPlatform::EAndroid:
				return GetPresetFeatureResolution_Android(tier);
				break;
			case EVdjmRecordEnvPlatform::EIOS:
				return GetPresetFeatureResolution_Ios(tier);
				break;
			case EVdjmRecordEnvPlatform::EMac:
				return GetPresetFeatureResolution_Mac(tier);
				break;
			case EVdjmRecordEnvPlatform::ELinux:
				return GetPresetFeatureResolution_Linux(tier);
				break;
			case EVdjmRecordEnvPlatform::EDefault:
				return GetPresetFeatureResolution_Window(tier);
				break;
			default: return FIntPoint();
			}
		}
	}
	
	namespace VideoBitrate
	{
		inline double GetBaseVideoBpp(EVdjmRecordQualityTiers Tier)
		{
			switch (Tier)
			{
			case EVdjmRecordQualityTiers::ELowest:     return 0.045;
			case EVdjmRecordQualityTiers::ELow:        return 0.055;
			case EVdjmRecordQualityTiers::EMdeiumLow:  return 0.065;
			case EVdjmRecordQualityTiers::EMedium:     return 0.075;
			case EVdjmRecordQualityTiers::EMediumHigh: return 0.090;
			case EVdjmRecordQualityTiers::EHigh:       return 0.105;
			case EVdjmRecordQualityTiers::EUltra:      return 0.120;
			case EVdjmRecordQualityTiers::EDefault:
			default:                                   return 0.080;
			}
		}
		inline double GetComplexityMultiplier(EVdjmRecordContentComplexity InComplexity)
		{
			switch (InComplexity)
			{
			case EVdjmRecordContentComplexity::EUIStatic:   return 0.85;
			case EVdjmRecordContentComplexity::EHighMotion: return 1.20;
			case EVdjmRecordContentComplexity::EGameplay:
			default:                                        return 1.00;
			}
		}
	}
	namespace VideoResolution
	{
		inline FIntPoint FitResolutionWithin(const FIntPoint& Desired, const FIntPoint& MaxSize)
		{
			if (Desired.X <= 0 || Desired.Y <= 0)
			{
				return MaxSize;
			}
			if (MaxSize.X <= 0 || MaxSize.Y <= 0)
			{
				return Desired;
			}

			const double ScaleX = static_cast<double>(MaxSize.X) / static_cast<double>(Desired.X);
			const double ScaleY = static_cast<double>(MaxSize.Y) / static_cast<double>(Desired.Y);
			const double Scale = FMath::Min(1.0, FMath::Min(ScaleX, ScaleY));

			return FIntPoint(
				FMath::Max(2, FMath::FloorToInt(static_cast<double>(Desired.X) * Scale)),
				FMath::Max(2, FMath::FloorToInt(static_cast<double>(Desired.Y) * Scale))
			);
		}
	}
	
	namespace FilePaths
	{
		inline FString GetPlatformRecordBaseDir(EVdjmRecordEnvPlatform InPlatform)
		{
			switch (InPlatform)
			{
			case EVdjmRecordEnvPlatform::EAndroid:
				return FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());

			case EVdjmRecordEnvPlatform::EWindows:
			case EVdjmRecordEnvPlatform::EIOS:
			case EVdjmRecordEnvPlatform::EMac:
			case EVdjmRecordEnvPlatform::ELinux:
			case EVdjmRecordEnvPlatform::EDefault:
			default:
				return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
			}
		}

		inline FString MakeSafeBaseName(const FString& InCustomFileName, const FString& InSessionId)
		{
			FString BaseName = !InCustomFileName.TrimStartAndEnd().IsEmpty()
				? InCustomFileName
				: (!InSessionId.TrimStartAndEnd().IsEmpty() ? InSessionId : TEXT("VdjmRecord"));

			BaseName = FPaths::MakeValidFileName(BaseName);
			if (BaseName.IsEmpty())
			{
				BaseName = TEXT("VdjmRecord");
			}
			return BaseName;
		}
	}
	
	namespace Resolvers
	{
		inline int32 ResolveVideoBitrateBps(
			const FIntPoint& InResolution,
			const int32 InFrameRate,
			const EVdjmRecordQualityTiers InTier,
			const EVdjmRecordContentComplexity InComplexity)
		{
			const double BaseBpp = VideoBitrate::GetBaseVideoBpp(InTier);
			const double ComplexityMul = VideoBitrate::GetComplexityMultiplier(InComplexity);

			const double RawBitrate =
				static_cast<double>(InResolution.X) *
				static_cast<double>(InResolution.Y) *
				static_cast<double>(InFrameRate) *
				BaseBpp *
				ComplexityMul;

			const int32 SafeBitrate = FMath::Clamp(
				FMath::RoundToInt(RawBitrate),
				500000,
				50000000);

			return SafeBitrate;
		}
		
		inline FIntPoint ResolveVideoResolution(
			const FIntPoint& InViewportSize,
			const FIntPoint& InPresetResolution,
			const bool bFitToDisplay,
			const FIntPoint& InPlatformMaxResolution)
		{
			FIntPoint Desired = bFitToDisplay ? InViewportSize : InPresetResolution;
			FIntPoint Fit =  VideoResolution::FitResolutionWithin(Desired, InPlatformMaxResolution);

			// 짝수 보정은 ValidateResolution 쪽에서 처리
			FIntPoint Safe = FIntPoint::ZeroValue;
			if (!Validations::DbcValidateResolution(
				Fit,
				Safe,
				TEXT("ResolveVideoResolution")))
			{
				return FIntPoint(1280, 720);
			}

			return Safe;
		}
		
		inline FString BuildResolvedOutputPath(
		EVdjmRecordEnvPlatform InPlatform,
		const FString& InRequestedPath,
		const FString& InCustomFileName,
		const FString& InSessionId,
		const bool bOverwriteExists,
		const TCHAR* DebugOwner)
		{
			const FString BaseDir = FilePaths::GetPlatformRecordBaseDir(InPlatform);

			FString CandidatePath = InRequestedPath;
			CandidatePath.TrimStartAndEndInline();

			if (CandidatePath.IsEmpty())
			{
				const FString BaseName = FilePaths::MakeSafeBaseName(InCustomFileName, InSessionId);
				CandidatePath = FPaths::Combine(
					BaseDir,
					FString::Printf(TEXT("%s_%lld.mp4"), *BaseName, FDateTime::Now().GetTicks()));
			}
			else
			{
				if (FPaths::IsRelative(CandidatePath))
				{
					CandidatePath = FPaths::Combine(BaseDir, CandidatePath);
				}

				const FString Dir = FPaths::GetPath(CandidatePath);
				const FString BaseName = FPaths::GetBaseFilename(CandidatePath, false);
				CandidatePath = FPaths::Combine(Dir, BaseName + TEXT(".mp4"));
			}

			FString SafePath;
			if (!Validations::DbcValidateOutputFilePath(CandidatePath, SafePath, DebugOwner))
			{
				return FString();
			}

			if (!bOverwriteExists && IFileManager::Get().FileExists(*SafePath))
			{
				const FString Dir = FPaths::GetPath(SafePath);
				const FString BaseName = FPaths::GetBaseFilename(SafePath, false);

				const FString UniquePath = FPaths::Combine(
					Dir,
					FString::Printf(TEXT("%s_%lld.mp4"), *BaseName, FDateTime::Now().GetTicks()));

				FString UniqueSafePath;
				if (!Validations::DbcValidateOutputFilePath(
					UniquePath,
					UniqueSafePath,
					DebugOwner))
				{
					return FString();
				}

				SafePath = MoveTemp(UniqueSafePath);
			}

			return SafePath;
		}
	}
}


UENUM(BlueprintType)
enum class EVdjmRecordBridgeInitStep : uint8
{
	EInitErrorEnd,
	EInitError,
	EInitializeStart,
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
§	↓	↓	↓	↓	↓	↓	↓
							UENUM s							
	Tier
*/


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

UENUM()
enum class EVdjmRecordEventSessionState
{
	EUndefined UMETA(DisplayName="Undefined"),
	EInitialized UMETA(DisplayName="Initialized"),
	EPrepare UMETA(DisplayName="Prepare"),
	EPrepared UMETA(DisplayName="Prepared"),
	ERunning UMETA(DisplayName="Running"),
	EStopping UMETA(DisplayName="Stopping"),
	EStopped UMETA(DisplayName="Stopped"),
	ETerminated UMETA(DisplayName="Terminated"),
	EError UMETA(DisplayName="Error"),
};
enum class EVdjmRecordEventSessionCallbackMask : uint8
{
	ENone					= 0x00,
	EStartControl			= 0x01,
	ERunningControl			= 0x02,
	EStopControl			= 0x04,
	ERunningObserve			= 0x08,
	EStateChangedObserve	= 0x10,
	EAll					= 0x1F
};
ENUM_CLASS_FLAGS(EVdjmRecordEventSessionCallbackMask);

DECLARE_DELEGATE_RetVal_OneParam(VdjmResult, FVdjmRecordEventSessionControlDelegate, UVdjmRecordEventSession* /* Session */);

// observe delegate
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FVdjmRecordEventSessionStateChangedDelegate,
	EVdjmRecordEventSessionState /* PrevState */,
	EVdjmRecordEventSessionState /* CurrentState */
);

// RunningSession 관찰용.
// 현재 mSessionIntervalSeconds 기반으로 부르면 frame 이 아니라 running tick count 임.
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FVdjmRecordEventSessionRunningObservedDelegate,
	UVdjmRecordEventSession* /* Session */,
	float /* ElapsedSeconds */,
	int32 /* RunningTickCount */
);

UCLASS()
class VDJMRECORDER_API UVdjmRecordEventSession : public UObject
{
	GENERATED_BODY()
public:
	void InitializeSession(	AActor* InOwnerActor,EVdjmRecordEventSessionCallbackMask InCallbackMask =EVdjmRecordEventSessionCallbackMask::EAll, float InSessionIntervalSeconds = 0.5f);
	
	void StartSession();
	void RunningSession();
	void StopSession();
	
	EVdjmRecordEventSessionState GetCurrentSessionState() const { return mCurrentState; }
	EVdjmRecordEventSessionState GetPreviousSessionState() const { return mPrevState; }
	EVdjmRecordEventSessionState GetReservedNextSessionState() const { return mReservedNextState; }
	
	FVdjmRecordEventSessionControlDelegate OnStartSessionCallback;
	FVdjmRecordEventSessionControlDelegate OnRunningSessionCallback;
	FVdjmRecordEventSessionControlDelegate OnStopSessionCallback;
	
	FVdjmRecordEventSessionRunningObservedDelegate OnRunningSessionObservedCallback;
	FVdjmRecordEventSessionStateChangedDelegate OnSessionStateChangedCallback;
protected:
	bool TrySetSessionState(EVdjmRecordEventSessionState InNewState);
	bool IsCallbackEnabled(EVdjmRecordEventSessionCallbackMask InMask) const;
	VdjmResult ExecuteControlCallback(
		FVdjmRecordEventSessionControlDelegate& InDelegate,
		EVdjmRecordEventSessionCallbackMask InMask
	);
	void ClearSessionTimers();
	
	TWeakObjectPtr<AActor> mOwnerActor;
	TWeakObjectPtr<UWorld> mOwnerWorld;  
	
	float mSessionStartTime = 0.0f;
	int32 mSessionFrameCount = 0; // 현재 구조에서는 actual frame count가 아니라 RunningSession 호출 횟수에 가까움
	float mSessionEndTime = 0.0f; // 절대 시간 기준 종료 시각. 0 이하면 auto-stop 안함.

	float mSessionIntervalSeconds = 0.5f;

	FTimerHandle mSessionTimerHandle;
	FTimerHandle mSessionObserverTimerHandle; // 지금 구현에서는 미사용. 분리 observer cadence 가 필요 없으면 제거 권장.

	EVdjmRecordEventSessionCallbackMask mCallbackMask = EVdjmRecordEventSessionCallbackMask::EAll;

	EVdjmRecordEventSessionState mPrevState = EVdjmRecordEventSessionState::EUndefined;
	EVdjmRecordEventSessionState mCurrentState = EVdjmRecordEventSessionState::EUndefined;
	EVdjmRecordEventSessionState mReservedNextState = EVdjmRecordEventSessionState::EUndefined;
};

#define DECLARE_VDJM_ENCODER_BOILERPLATE(StructType) \
static TOptional<StructType> CreateDefaultForCurrentPlatform(); \
void ApplyOptional(const TOptional<StructType>& Optional) { if (Optional.IsSet()) { *this = Optional.GetValue(); } else { Clear(); } } \
void Clear() { ApplyOptional(CreateDefaultForCurrentPlatform()); } \
bool EvaluateValidation() const;

USTRUCT(Blueprintable)
struct VDJMRECORDER_API FVdjmEncoderInitRequestVideo
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="VideoConfig")
	bool bResolutionFitToDisplay = true;
	UPROPERTY(EditAnywhere, Category="VideoConfig")
	int32 Width = 1920;
	UPROPERTY(EditAnywhere, Category="VideoConfig")
	int32 Height = 1080;
	UPROPERTY(EditAnywhere, Category="VideoConfig")
	int32 FrameRate = 30;
	UPROPERTY(EditAnywhere, Category="VideoConfig")
	int32 Bitrate = 5000000;
	UPROPERTY(EditAnywhere, Category="VideoConfig")
	int32 KeyframeInterval = 2;
	UPROPERTY(EditAnywhere, Category="VideoConfig")
	FString MimeType = TEXT("video/mp4");
	
	UPROPERTY(EditAnywhere, Category="VideoConfig")
	TEnumAsByte<EPixelFormat> PixelFormat = PF_B8G8R8A8; // Unreal Engine에서 일반적으로 사용되는 픽셀 포맷. 플랫폼과 인코더에 따라 지원 여부가 다를 수 있습니다.
	
	DECLARE_VDJM_ENCODER_BOILERPLATE(FVdjmEncoderInitRequestVideo);
	
	FString ToString() const
	{
		return FString::Printf(TEXT("{ VideoInitRequest: FitToDisplay=%s, Resolution=%dx%d, FrameRate=%d, Bitrate=%d, KeyframeInterval=%d, MimeType=%s }"),
			bResolutionFitToDisplay ? TEXT("True") : TEXT("False"),
			Width, Height,
			FrameRate,
			Bitrate,
			KeyframeInterval,
			*MimeType);
	}
	
};

USTRUCT()
struct VDJMRECORDER_API FVdjmEncoderInitRequestAudio
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="AudioConfig")
	bool bEnableInternalAudioCapture = true;
	UPROPERTY(EditAnywhere, Category="AudioConfig")
	FString AudioMimeType = TEXT("audio/mp4a-latm");
	UPROPERTY(EditAnywhere, Category="AudioConfig")
	int32 SampleRate = 44100;
	UPROPERTY(EditAnywhere, Category="AudioConfig")
	int32 ChannelCount = 2;
	UPROPERTY(EditAnywhere, Category="AudioConfig")
	int32 Bitrate = 128000;
	UPROPERTY(EditAnywhere, Category="AudioConfig")
	int32 AacProfile = 2; // LC-AAC	
	UPROPERTY(EditAnywhere, Category="AudioConfig")
	FName SourceSubMixName = TEXT("Master"); // Unreal Engine의 오디오 믹싱 시스템에서 캡처할 서브믹스 이름. 예: "Master", "Music", "SFX" 등.
	
	DECLARE_VDJM_ENCODER_BOILERPLATE(FVdjmEncoderInitRequestAudio);
	
	FString ToString() const
	{
		return FString::Printf(TEXT("{ AudioInitRequest: EnableInternalAudioCapture=%s, AudioMimeType=%s, SampleRate=%d, ChannelCount=%d, Bitrate=%d, AacProfile=%d, SourceSubMixName=%s }"),
			bEnableInternalAudioCapture ? TEXT("True") : TEXT("False"),
			*AudioMimeType,
			SampleRate,
			ChannelCount,
			Bitrate,
			AacProfile,
			*SourceSubMixName.ToString());
	}
};
USTRUCT()
struct VDJMRECORDER_API FVdjmEncoderInitRequestOutput
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="OutputConfig")
	FString OutputFilePath = TEXT("RecordingOutput.mp4");
	UPROPERTY(EditAnywhere, Category="OutputConfig")
	FString SessionId = TEXT("DefaultSession");
	UPROPERTY(EditAnywhere, Category="OutputConfig")
	bool bOverwriteExists = false;
	
	DECLARE_VDJM_ENCODER_BOILERPLATE(FVdjmEncoderInitRequestOutput);
	
	FString ToString() const
	{
		return FString::Printf(TEXT("{ OutputInitRequest: OutputFilePath=%s, SessionId=%s, OverwriteExists=%s }"),
			*OutputFilePath,
			*SessionId,
			bOverwriteExists ? TEXT("True") : TEXT("False"));
	}
	
};
USTRUCT()
struct VDJMRECORDER_API FVdjmEncoderInitRequestRuntimePolicy
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="RuntimPolicyConfig")
	bool bRequireAVSync  = true;
	UPROPERTY(EditAnywhere, Category="RuntimPolicyConfig")
	int32 AllowedDriftMs = 20;
	UPROPERTY(EditAnywhere, Category="RuntimPolicyConfig")
	bool bStartMuxerWhenBothTracksReady = true;
	
	DECLARE_VDJM_ENCODER_BOILERPLATE(FVdjmEncoderInitRequestRuntimePolicy);
	
	FString ToString() const
	{
		return FString::Printf(TEXT("{ RuntimePolicyInitRequest: RequireAVSync=%s, AllowedDriftMs=%d, StartMuxerWhenBothTracksReady=%s }"),
			bRequireAVSync ? TEXT("True") : TEXT("False"),
			AllowedDriftMs,
			bStartMuxerWhenBothTracksReady ? TEXT("True") : TEXT("False"));
	}
	
};
USTRUCT()
struct VDJMRECORDER_API FVdjmEncoderInitRequestPlatformExtension
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="PlatformExtensionConfig") 
	TMap<FName,FString> PlatformSpecificParameters;
	
	DECLARE_VDJM_ENCODER_BOILERPLATE(FVdjmEncoderInitRequestPlatformExtension)
	
	FString ToString() const
	{
		FString ParamsString;
		for (const auto& Pair : PlatformSpecificParameters)
		{			ParamsString += FString::Printf(TEXT("%s=%s; "), *Pair.Key.ToString(), *Pair.Value);
		}
		return FString::Printf(TEXT("{ PlatformExtensionInitRequest: %d platform-specific parameters: %s }"),
			PlatformSpecificParameters.Num(),
			*ParamsString);
	}

};
USTRUCT()
struct VDJMRECORDER_API FVdjmEncoderInitRequest
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="EncoderInitRequest|Video")
	FVdjmEncoderInitRequestVideo VideoConfig;
	UPROPERTY(EditAnywhere, Category="EncoderInitRequest|Audio")
	FVdjmEncoderInitRequestAudio AudioConfig;
	UPROPERTY(EditAnywhere, Category="EncoderInitRequest|Output")
	FVdjmEncoderInitRequestOutput OutputConfig;
	UPROPERTY(EditAnywhere, Category="EncoderInitRequest|RuntimPolicy")
	FVdjmEncoderInitRequestRuntimePolicy RuntimePolicyConfig;
	UPROPERTY(EditAnywhere, Category="EncoderInitRequest|PlatformExtension")
	FVdjmEncoderInitRequestPlatformExtension PlatformExtensionConfig;
	
	void Clear()
	{
		VideoConfig.Clear();
		AudioConfig.Clear();
		OutputConfig.Clear();
		RuntimePolicyConfig.Clear();
		PlatformExtensionConfig.Clear();
	}
	FString ToString() const
	{
		return FString::Printf(TEXT("{ EncoderInitRequest: \n%s\n%s\n%s\n%s\n%s\n}"),
			*VideoConfig.ToString(),
			*AudioConfig.ToString(),
			*OutputConfig.ToString(),
			*RuntimePolicyConfig.ToString(),
			*PlatformExtensionConfig.ToString());
	}
	bool EvaluateValidation() const;
	static TOptional<FVdjmEncoderInitRequest> CreateDefaultForCurrentPlatform();
};


/**
 * 
 */
UCLASS()
class VDJMRECORDER_API UVdjmRecordTypes : public UObject
{
	GENERATED_BODY()
};
