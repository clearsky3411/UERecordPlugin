// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VdjmRecordTypes.h"

// Android encoder code historically used these names. Keep aliases so the
// shared snapshot can move upward without forcing a broad Android refactor.
using EVdjmAndroidGraphicBackend = EVdjmRecordGraphicBackend;
using FVdjmAndroidEncoderVideoSnapshot = FVdjmRecordEncoderVideoSnapshot;
using FVdjmAndroidEncoderAudioSnapshot = FVdjmRecordEncoderAudioSnapshot;
using FVdjmAndroidEncoderSnapshot = FVdjmRecordEncoderSnapshot;
