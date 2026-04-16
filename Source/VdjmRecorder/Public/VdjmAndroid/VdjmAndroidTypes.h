// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VdjmAndroidTypes.h"


enum class EVdjmAndroidGraphicBackend : uint8
{
	EUnknown,
	EOpenGL,
	EVulkan
};

struct FVdjmAndroidEncoderVideoSnapshot
{
	int32 VideoWidth = 0;
	int32 VideoHeight = 0;
	int32 VideoBitrate = 0;
	int32 VideoFPS = 0;
	int32 VideoIntervalSec = 0;
	FString MimeType = "video/avc";
	FString OutputFilePath = TEXT("");
	EVdjmAndroidGraphicBackend GraphicBackend = EVdjmAndroidGraphicBackend::EUnknown;
	
	FVdjmAndroidEncoderVideoSnapshot() = default;
	FVdjmAndroidEncoderVideoSnapshot(int32 width, int32 height, int32 bitrate, int32 fps, const FString& outputFilePath)
		: VideoWidth(width), VideoHeight(height), VideoBitrate(bitrate), VideoFPS(fps), OutputFilePath(outputFilePath), GraphicBackend(EVdjmAndroidGraphicBackend::EUnknown), MimeType("video/avc"), VideoIntervalSec(1)
	{}
	FVdjmAndroidEncoderVideoSnapshot(const FVdjmAndroidEncoderVideoSnapshot& other)
		: VideoWidth(other.VideoWidth), VideoHeight(other.VideoHeight), VideoBitrate(other.VideoBitrate), VideoFPS(other.VideoFPS), OutputFilePath(other.OutputFilePath), MimeType(other.MimeType), GraphicBackend(other.GraphicBackend), VideoIntervalSec(other.VideoIntervalSec)
	{}
	FVdjmAndroidEncoderVideoSnapshot(FVdjmAndroidEncoderVideoSnapshot&& other) noexcept
		: VideoWidth(other.VideoWidth), VideoHeight(other.VideoHeight), VideoBitrate(other.VideoBitrate), VideoFPS(other.VideoFPS), OutputFilePath(other.OutputFilePath), MimeType(other.MimeType), GraphicBackend(other.GraphicBackend), VideoIntervalSec(other.VideoIntervalSec)
	{}
	FVdjmAndroidEncoderVideoSnapshot& operator=(const FVdjmAndroidEncoderVideoSnapshot& other)
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
	FVdjmAndroidEncoderVideoSnapshot& operator=(FVdjmAndroidEncoderVideoSnapshot&& other) noexcept
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
struct FVdjmAndroidEncoderAudioSnapshot
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
	
	FVdjmAndroidEncoderAudioSnapshot() = default;
	FVdjmAndroidEncoderAudioSnapshot(const FVdjmAndroidEncoderAudioSnapshot& other)
		: bEnableAudio(other.bEnableAudio), AudioSampleRate(other.AudioSampleRate), AudioChannelCount(other.AudioChannelCount), AudioBitrate(other.AudioBitrate), AudioAacProfile(other.AudioAacProfile), AudioMimeType(other.AudioMimeType), AudioSourceId(other.AudioSourceId), bAudioRequired(other.bAudioRequired), AudioDriftToleranceMs(other.AudioDriftToleranceMs), AudioFrameDurationMs(other.AudioFrameDurationMs)
	{}
	FVdjmAndroidEncoderAudioSnapshot(FVdjmAndroidEncoderAudioSnapshot&& other) noexcept
		: bEnableAudio(other.bEnableAudio), AudioSampleRate(other.AudioSampleRate), AudioChannelCount(other.AudioChannelCount), AudioBitrate(other.AudioBitrate), AudioAacProfile(other.AudioAacProfile), AudioMimeType(other.AudioMimeType), AudioSourceId(other.AudioSourceId), bAudioRequired(other.bAudioRequired), AudioDriftToleranceMs(other.AudioDriftToleranceMs), AudioFrameDurationMs(other.AudioFrameDurationMs)
	{}
	FVdjmAndroidEncoderAudioSnapshot& operator=(const FVdjmAndroidEncoderAudioSnapshot& other)
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
	FVdjmAndroidEncoderAudioSnapshot& operator=(FVdjmAndroidEncoderAudioSnapshot&& other) noexcept
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


struct FVdjmAndroidEncoderSnapshot
{
	FVdjmAndroidEncoderVideoSnapshot VideoConfig;
	FVdjmAndroidEncoderAudioSnapshot AudioConfig;
	
	
	
	FVdjmAndroidEncoderSnapshot() = default;
	FVdjmAndroidEncoderSnapshot(FVdjmAndroidEncoderVideoSnapshot videoConfig, FVdjmAndroidEncoderAudioSnapshot audioConfig)
		: VideoConfig(videoConfig), AudioConfig(audioConfig)
	{}
	FVdjmAndroidEncoderSnapshot(const FVdjmAndroidEncoderSnapshot& other)
		: VideoConfig(other.VideoConfig), AudioConfig(other.AudioConfig)
	{}
	FVdjmAndroidEncoderSnapshot(FVdjmAndroidEncoderSnapshot&& other) noexcept
		: VideoConfig(other.VideoConfig), AudioConfig(other.AudioConfig)
	{}
	FVdjmAndroidEncoderSnapshot& operator=(const FVdjmAndroidEncoderSnapshot& other)
	{
		this->VideoConfig = other.VideoConfig;
		this->AudioConfig = other.AudioConfig;
		return *this;
	}
	FVdjmAndroidEncoderSnapshot& operator=(FVdjmAndroidEncoderSnapshot&& other) noexcept
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
