// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmAndroidTypes.generated.h"

enum class EVdjmAndroidGraphicBackend : uint8
{
	EUnknown,
	EOpenGL,
	EVulkan
};

struct FVdjmAndroidEncoderConfigureVideo
{
	int32 VideoWidth = 0;
	int32 VideoHeight = 0;
	int32 VideoBitrate = 0;
	int32 VideoFPS = 0;
	int32 VideoIntervalSec = 0;
	FString MimeType = "video/avc";
	FString OutputFilePath = TEXT("");
	EVdjmAndroidGraphicBackend GraphicBackend = EVdjmAndroidGraphicBackend::EUnknown;
	
	FVdjmAndroidEncoderConfigureVideo() = default;
	FVdjmAndroidEncoderConfigureVideo(int32 width, int32 height, int32 bitrate, int32 fps, const FString& outputFilePath)
		: VideoWidth(width), VideoHeight(height), VideoBitrate(bitrate), VideoFPS(fps), OutputFilePath(outputFilePath), GraphicBackend(EVdjmAndroidGraphicBackend::EUnknown), MimeType("video/avc"), VideoIntervalSec(1)
	{}
	FVdjmAndroidEncoderConfigureVideo(const FVdjmAndroidEncoderConfigureVideo& other)
		: VideoWidth(other.VideoWidth), VideoHeight(other.VideoHeight), VideoBitrate(other.VideoBitrate), VideoFPS(other.VideoFPS), OutputFilePath(other.OutputFilePath), MimeType(other.MimeType), GraphicBackend(other.GraphicBackend), VideoIntervalSec(other.VideoIntervalSec)
	{}
	FVdjmAndroidEncoderConfigureVideo(FVdjmAndroidEncoderConfigureVideo&& other) noexcept
		: VideoWidth(other.VideoWidth), VideoHeight(other.VideoHeight), VideoBitrate(other.VideoBitrate), VideoFPS(other.VideoFPS), OutputFilePath(other.OutputFilePath), MimeType(other.MimeType), GraphicBackend(other.GraphicBackend), VideoIntervalSec(other.VideoIntervalSec)
	{}
	FVdjmAndroidEncoderConfigureVideo& operator=(const FVdjmAndroidEncoderConfigureVideo& other)
	{		this->VideoWidth = other.VideoWidth;
		this->VideoHeight = other.VideoHeight;
		this->VideoBitrate = other.VideoBitrate;
		this->VideoFPS = other.VideoFPS;
		this->OutputFilePath = other.OutputFilePath;
		this->MimeType = other.MimeType;
		this->GraphicBackend = other.GraphicBackend;
		this->VideoIntervalSec = other.VideoIntervalSec;
		return *this;
	}
	FVdjmAndroidEncoderConfigureVideo& operator=(FVdjmAndroidEncoderConfigureVideo&& other) noexcept
	{		this->VideoWidth = other.VideoWidth;
		this->VideoHeight = other.VideoHeight;
		this->VideoBitrate = other.VideoBitrate;
		this->VideoFPS = other.VideoFPS;
		this->OutputFilePath = other.OutputFilePath;
		this->MimeType = other.MimeType;
		this->GraphicBackend = other.GraphicBackend;
		this->VideoIntervalSec = other.VideoIntervalSec;
		return *this;
	}
	void Clear()
	{
		VideoWidth = 0;
		VideoHeight = 0;
		VideoBitrate = 0;
		VideoFPS = 0;
		OutputFilePath.Empty();
		MimeType = "video/avc";
		VideoIntervalSec = 1;
		GraphicBackend = EVdjmAndroidGraphicBackend::EUnknown;
	}
	bool IsValidateVideoEncoderArguments() const
	{		return VideoWidth > 0 && VideoHeight > 0 && VideoBitrate > 0 && VideoFPS > 0 && !OutputFilePath.IsEmpty() && !MimeType.IsEmpty() && GraphicBackend != EVdjmAndroidGraphicBackend::EUnknown && VideoIntervalSec >= 0;
	}
	FString ToString() const
	{
		return FString::Printf(TEXT("OutputFilePath: %s, MimeType: %s, Resolution: %dx%d, Bitrate: %d, FPS: %d, I-Frame Interval: %d, GraphicBackend: %d"),
			*OutputFilePath,
			*MimeType,
			VideoWidth,
			VideoHeight,
			VideoBitrate,
			VideoFPS,
			VideoIntervalSec,
			static_cast<int32>(GraphicBackend));
	}
};
struct FVdjmAndroidEncoderConfigureAudio
{
	bool bEnableAudio = false;
	int32 AudioSampleRate = 48000;	//	Hz
	int32 AudioChannelCount = 2;
	int32 AudioBitrate = 128000;
	
	int32 AudioAacProfile = 2;	//	AAC LC
	FString AudioMimeType = "audio/mp4a-latm";
	FString AudioSourceId;
	bool bAudioRequired = false;
	int32 AudioDriftToleranceMs = 20;
	int32 AudioFrameDurationMs = 20;	// AAC 인코딩 chunk 기준
	
	FVdjmAndroidEncoderConfigureAudio() = default;
	FVdjmAndroidEncoderConfigureAudio(const FVdjmAndroidEncoderConfigureAudio& other)
		: bEnableAudio(other.bEnableAudio), AudioSampleRate(other.AudioSampleRate), AudioChannelCount(other.AudioChannelCount), AudioBitrate(other.AudioBitrate), AudioAacProfile(other.AudioAacProfile), AudioMimeType(other.AudioMimeType), AudioSourceId(other.AudioSourceId), bAudioRequired(other.bAudioRequired), AudioDriftToleranceMs(other.AudioDriftToleranceMs), AudioFrameDurationMs(other.AudioFrameDurationMs)
	{}
	FVdjmAndroidEncoderConfigureAudio(FVdjmAndroidEncoderConfigureAudio&& other) noexcept
		: bEnableAudio(other.bEnableAudio), AudioSampleRate(other.AudioSampleRate), AudioChannelCount(other.AudioChannelCount), AudioBitrate(other.AudioBitrate), AudioAacProfile(other.AudioAacProfile), AudioMimeType(other.AudioMimeType), AudioSourceId(other.AudioSourceId), bAudioRequired(other.bAudioRequired), AudioDriftToleranceMs(other.AudioDriftToleranceMs), AudioFrameDurationMs(other.AudioFrameDurationMs)
	{}
	FVdjmAndroidEncoderConfigureAudio& operator=(const FVdjmAndroidEncoderConfigureAudio& other)
	{		this->bEnableAudio = other.bEnableAudio;
		this->AudioSampleRate = other.AudioSampleRate;
		this->AudioChannelCount = other.AudioChannelCount;
		this->AudioBitrate = other.AudioBitrate;
		this->AudioAacProfile = other.AudioAacProfile;
		this->AudioMimeType = other.AudioMimeType;
		this->AudioSourceId = other.AudioSourceId;
		this->bAudioRequired = other.bAudioRequired;
		this->AudioDriftToleranceMs = other.AudioDriftToleranceMs;
		this->AudioFrameDurationMs = other.AudioFrameDurationMs;
		return *this;
	}
	FVdjmAndroidEncoderConfigureAudio& operator=(FVdjmAndroidEncoderConfigureAudio&& other) noexcept
	{		this->bEnableAudio = other.bEnableAudio;
		this->AudioSampleRate = other.AudioSampleRate;
		this->AudioChannelCount = other.AudioChannelCount;
		this->AudioBitrate = other.AudioBitrate;
		this->AudioAacProfile = other.AudioAacProfile;
		this->AudioMimeType = other.AudioMimeType;
		this->AudioSourceId = other.AudioSourceId;
		this->bAudioRequired = other.bAudioRequired;
		this->AudioDriftToleranceMs = other.AudioDriftToleranceMs;
		this->AudioFrameDurationMs = other.AudioFrameDurationMs;
		return *this;
	}
	void Clear()	{
		bEnableAudio = false;
		AudioSampleRate = 48000;
		AudioChannelCount = 2;
		AudioBitrate = 128000;
		AudioAacProfile = 2;
		AudioMimeType = "audio/mp4a-latm";
		AudioSourceId.Empty();
		bAudioRequired = false;
		AudioDriftToleranceMs = 20;
		AudioFrameDurationMs = 20;
	}
	bool IsValidateAudioEncoderArguments() const
	{		return !bEnableAudio || (AudioSampleRate > 0 && AudioChannelCount > 0 && AudioBitrate > 0 && !AudioMimeType.IsEmpty() && AudioAacProfile > 0 && AudioDriftToleranceMs >= 0 && AudioFrameDurationMs > 0);
	}
	FString ToString() const
	{		return FString::Printf(TEXT("EnableAudio: %s, SampleRate: %d, ChannelCount: %d, Bitrate: %d, AACProfile: %d, MimeType: %s, AudioSourceId: %s, AudioRequired: %s, DriftToleranceMs: %d, FrameDurationMs: %d"),
			bEnableAudio ? TEXT("True") : TEXT("False"),
			AudioSampleRate,
			AudioChannelCount,
			AudioBitrate,
			AudioAacProfile,
			*AudioMimeType,
			*AudioSourceId,
			bAudioRequired ? TEXT("True") : TEXT("False"),
			AudioDriftToleranceMs,
			AudioFrameDurationMs);
	}
};


struct FVdjmAndroidEncoderConfigure
{
	
	FVdjmAndroidEncoderConfigureVideo VideoConfig;
	FVdjmAndroidEncoderConfigureAudio AudioConfig;
	
	FVdjmAndroidEncoderConfigure() = default;
	FVdjmAndroidEncoderConfigure(FVdjmAndroidEncoderConfigureVideo videoConfig, FVdjmAndroidEncoderConfigureAudio audioConfig)
		: VideoConfig(videoConfig), AudioConfig(audioConfig)
	{}
	FVdjmAndroidEncoderConfigure(const FVdjmAndroidEncoderConfigure& other)
		: VideoConfig(other.VideoConfig), AudioConfig(other.AudioConfig)
	{}
	FVdjmAndroidEncoderConfigure(FVdjmAndroidEncoderConfigure&& other) noexcept
		: VideoConfig(other.VideoConfig), AudioConfig(other.AudioConfig)
	{}
	FVdjmAndroidEncoderConfigure& operator=(const FVdjmAndroidEncoderConfigure& other)
	{
		this->VideoConfig = other.VideoConfig;
		this->AudioConfig = other.AudioConfig;
		return *this;
	}
	FVdjmAndroidEncoderConfigure& operator=(FVdjmAndroidEncoderConfigure&& other) noexcept
	{
		this->VideoConfig = other.VideoConfig;
		this->AudioConfig = other.AudioConfig;
		return *this;
	}
	
	bool IsValidateEncoderArguments() const;
	void Clear()
	{
		VideoConfig.Clear();
		AudioConfig.Clear();
	}
	
	FString ToString() const
	{
		return FString::Printf(TEXT("Encoder Configure - Video: [%s], Audio: [%s]"), *VideoConfig.ToString(), *AudioConfig.ToString());
	}
};

/**
 * 
 */
UCLASS()
class VDJMRECORDER_API UVdjmAndroidTypes : public UObject
{
	GENERATED_BODY()
};
