#include "VdjmEncoderFactory.h"
#include "VdjmRecoderEncoderImpl.h"

#if PLATFORM_WINDOWS
#include "VdjmWMF/VdjmRecorderWndEncoder.h"
#endif

#if PLATFORM_ANDROID || defined(__RESHARPER__)
#include "VdjmAndroid/VdjmRecoderAndroidEncoder.h"
#endif

TSharedPtr<FVdjmVideoEncoderBase> CreatePlatformVideoEncoder()
{
#if PLATFORM_WINDOWS
	return MakeShared<FVdjmWindowsEncoderImpl>();
#elif PLATFORM_ANDROID
	return MakeShared<FVdjmAndroidEncoderImpl>();
#else
	return nullptr;
#endif
}