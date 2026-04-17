// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmAndroid/VdjmAndroidTypes.h"


bool FVdjmAndroidEncoderSnapshot::IsValidateEncoderArguments() const
{
		// 1. 출력 경로
	if (VideoConfig.OutputFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - OutputFilePath is empty."));
		return false;
	}

	const FString NormalizedPath = FPaths::ConvertRelativePathToFull(VideoConfig.OutputFilePath);
	const FString DirectoryPath = FPaths::GetPath(NormalizedPath);
	const FString Extension = FPaths::GetExtension(NormalizedPath, false);

	if (DirectoryPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Directory path is empty. Path: %s"), *NormalizedPath);
		return false;
	}

	if (!IFileManager::Get().DirectoryExists(*DirectoryPath))
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Directory does not exist. Directory: %s"), *DirectoryPath);
		return false;
	}

	if (Extension.IsEmpty() || !Extension.Equals(TEXT("mp4"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Output file extension must be mp4. Path: %s"), *NormalizedPath);
		return false;
	}

	// 2. MIME
	// video/mp4 는 컨테이너 성격 값으로 들어오는 경우가 있어 호환 처리 허용.
	// 실제 코덱 설정 직전에는 video/avc 로 정규화한다.
	if (VideoConfig.MimeType.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - MimeType is empty."));
		return false;
	}
	const FString mimeTypeLower = VideoConfig.MimeType.ToLower();
	if (mimeTypeLower != TEXT("video/avc") && mimeTypeLower != TEXT("video/mp4"))
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Unsupported MimeType: %s"), *VideoConfig.MimeType);
		return false;
	}

	// 3. 해상도
	if (VideoConfig.VideoWidth <= 0 || VideoConfig.VideoHeight <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Invalid resolution. Width=%d Height=%d"), VideoConfig.VideoWidth, VideoConfig.VideoHeight);
		return false;
	}

	// H.264 / Surface 인코더 호환성 관점에서 짝수 강제 권장
	if ((VideoConfig.VideoWidth % 2) != 0 || (VideoConfig.VideoHeight % 2) != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Width and Height must be even. Width=%d Height=%d"), VideoConfig.VideoWidth, VideoConfig.VideoHeight);
		return false;
	}

	// 너무 작은 값 방지
	if (VideoConfig.VideoWidth < 16 || VideoConfig.VideoHeight < 16)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Resolution is too small. Width=%d Height=%d"), VideoConfig.VideoWidth, VideoConfig.VideoHeight);
		return false;
	}

	// 너무 큰 값 방지
	// 오늘 안에 끝내는 목적이면 보수적으로 8K 정도 상한선
	if (VideoConfig.VideoWidth > 7680 || VideoConfig.VideoHeight > 4320)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Resolution is too large. Width=%d Height=%d"), VideoConfig.VideoWidth, VideoConfig.VideoHeight);
		return false;
	}

	// 4. 비트레이트
	if (VideoConfig.VideoBitrate <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Bitrate must be > 0. Bitrate=%d"), VideoConfig.VideoBitrate);
		return false;
	}

	// 너무 비정상적인 값 방지
	if (VideoConfig.VideoBitrate < 100000)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Bitrate looks too low. Bitrate=%d"), VideoConfig.VideoBitrate);
	}

	if (VideoConfig.VideoBitrate > 100000000)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - Bitrate is too high. Bitrate=%d"), VideoConfig.VideoBitrate);
		return false;
	}

	// 5. FPS
	if (VideoConfig.VideoFPS <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - FrameRate must be > 0. FrameRate=%d"), VideoConfig.VideoFPS);
		return false;
	}

	if (VideoConfig.VideoFPS > 120)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - FrameRate is too high. FrameRate=%d"), VideoConfig.VideoFPS);
		return false;
	}

	// 6. I-Frame interval
	if (VideoConfig.VideoIntervalSec < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - VideoIntervalSec must be >= 0. VideoIntervalSec=%d"), VideoConfig.VideoIntervalSec);
		return false;
	}

	// 0은 허용할 수는 있지만 보통 1이 더 무난함
	if (VideoConfig.VideoIntervalSec == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - VideoIntervalSec is 0. This may create too many keyframes depending on codec behavior."));
	}

	if (VideoConfig.VideoIntervalSec > 10)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - VideoIntervalSec is quite high. This may create very long GOPs depending on codec behavior."));
		return false;
	}
	
	if (VideoConfig.GraphicBackend == EVdjmAndroidGraphicBackend::EUnknown)
	{
		UE_LOG(LogTemp, Warning, TEXT("FVdjmAndroidEncoderImpl::ValidateEncoderArguments - GraphicBackend is unknown. Make sure to set it correctly for optimal performance."));
		
		return false;
	}
	
	return true;
}
