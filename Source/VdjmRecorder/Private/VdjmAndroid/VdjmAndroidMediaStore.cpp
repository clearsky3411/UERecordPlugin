#include "VdjmAndroid/VdjmAndroidMediaStore.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "VdjmRecordTypes.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace
{
	FString NormalizeAndroidMediaSourcePath(const FString& sourceFilePath)
	{
		FString normalizedPath = sourceFilePath;
		normalizedPath.TrimStartAndEndInline();
		FPaths::NormalizeFilename(normalizedPath);
		VdjmRecordUtils::FilePaths::StripEmbeddedWindowsAbsolutePathInline(normalizedPath);

		if (FPaths::IsRelative(normalizedPath))
		{
			normalizedPath = FPaths::ConvertRelativePathToFull(normalizedPath);
			FPaths::NormalizeFilename(normalizedPath);
		}

		return normalizedPath;
	}

	FString ResolveAndroidMediaDisplayName(const FString& sourceFilePath, const FString& displayName)
	{
		FString resolvedDisplayName = displayName.TrimStartAndEnd();
		if (resolvedDisplayName.IsEmpty())
		{
			resolvedDisplayName = FPaths::GetCleanFilename(sourceFilePath);
		}

		resolvedDisplayName.ReplaceInline(TEXT("/"), TEXT("_"));
		resolvedDisplayName.ReplaceInline(TEXT("\\"), TEXT("_"));
		if (not resolvedDisplayName.EndsWith(TEXT(".mp4"), ESearchCase::IgnoreCase))
		{
			resolvedDisplayName += TEXT(".mp4");
		}

		return resolvedDisplayName;
	}

	FString ResolveAndroidMediaRelativePath(const FString& relativePath)
	{
		FString resolvedRelativePath = relativePath.TrimStartAndEnd();
		if (resolvedRelativePath.IsEmpty())
		{
			resolvedRelativePath = TEXT("Movies/VdjmRecorder");
		}

		resolvedRelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		resolvedRelativePath.RemoveFromStart(TEXT("/"));
		resolvedRelativePath.RemoveFromEnd(TEXT("/"));
		return resolvedRelativePath;
	}

#if PLATFORM_ANDROID
	bool CheckAndClearJavaException(JNIEnv* env, const TCHAR* debugContext, FString& outErrorReason)
	{
		if (env == nullptr || not env->ExceptionCheck())
		{
			return true;
		}

		env->ExceptionDescribe();
		env->ExceptionClear();
		outErrorReason = FString::Printf(TEXT("%s raised a Java exception."), debugContext);
		return false;
	}

	jstring NewJavaString(JNIEnv* env, const FString& value)
	{
		if (env == nullptr)
		{
			return nullptr;
		}

		return env->NewStringUTF(TCHAR_TO_UTF8(*value));
	}

	FString JavaStringToFString(JNIEnv* env, jstring javaString)
	{
		if (env == nullptr || javaString == nullptr)
		{
			return FString();
		}

		const char* rawString = env->GetStringUTFChars(javaString, nullptr);
		if (rawString == nullptr)
		{
			return FString();
		}

		const FString result = UTF8_TO_TCHAR(rawString);
		env->ReleaseStringUTFChars(javaString, rawString);
		return result;
	}

	int32 GetAndroidSdkInt(JNIEnv* env)
	{
		if (env == nullptr)
		{
			return 0;
		}

		jclass versionClass = env->FindClass("android/os/Build$VERSION");
		if (versionClass == nullptr)
		{
			env->ExceptionClear();
			return 0;
		}

		jfieldID sdkIntField = env->GetStaticFieldID(versionClass, "SDK_INT", "I");
		if (sdkIntField == nullptr)
		{
			env->DeleteLocalRef(versionClass);
			env->ExceptionClear();
			return 0;
		}

		const int32 sdkInt = static_cast<int32>(env->GetStaticIntField(versionClass, sdkIntField));
		env->DeleteLocalRef(versionClass);
		return sdkInt;
	}

	bool PutContentValueString(
		JNIEnv* env,
		jobject contentValues,
		jmethodID putStringMethod,
		const FString& key,
		const FString& value,
		FString& outErrorReason)
	{
		jstring keyString = NewJavaString(env, key);
		jstring valueString = NewJavaString(env, value);
		if (keyString == nullptr || valueString == nullptr)
		{
			outErrorReason = FString::Printf(TEXT("Failed to allocate ContentValues string. Key=%s"), *key);
			if (keyString != nullptr)
			{
				env->DeleteLocalRef(keyString);
			}
			if (valueString != nullptr)
			{
				env->DeleteLocalRef(valueString);
			}
			return false;
		}

		env->CallVoidMethod(contentValues, putStringMethod, keyString, valueString);
		env->DeleteLocalRef(keyString);
		env->DeleteLocalRef(valueString);
		return CheckAndClearJavaException(env, TEXT("ContentValues.put(String)"), outErrorReason);
	}

	bool PutContentValueInteger(
		JNIEnv* env,
		jobject contentValues,
		jmethodID putIntegerMethod,
		jmethodID integerValueOfMethod,
		const FString& key,
		int32 value,
		FString& outErrorReason)
	{
		jclass integerClass = env->FindClass("java/lang/Integer");
		if (integerClass == nullptr)
		{
			outErrorReason = TEXT("Failed to find java.lang.Integer.");
			CheckAndClearJavaException(env, TEXT("FindClass(java.lang.Integer)"), outErrorReason);
			return false;
		}

		jstring keyString = NewJavaString(env, key);
		jobject integerValue = env->CallStaticObjectMethod(integerClass, integerValueOfMethod, value);
		if (keyString == nullptr || integerValue == nullptr)
		{
			outErrorReason = FString::Printf(TEXT("Failed to allocate ContentValues integer. Key=%s"), *key);
			if (keyString != nullptr)
			{
				env->DeleteLocalRef(keyString);
			}
			if (integerValue != nullptr)
			{
				env->DeleteLocalRef(integerValue);
			}
			env->DeleteLocalRef(integerClass);
			CheckAndClearJavaException(env, TEXT("Integer.valueOf"), outErrorReason);
			return false;
		}

		env->CallVoidMethod(contentValues, putIntegerMethod, keyString, integerValue);
		env->DeleteLocalRef(keyString);
		env->DeleteLocalRef(integerValue);
		env->DeleteLocalRef(integerClass);
		return CheckAndClearJavaException(env, TEXT("ContentValues.put(Integer)"), outErrorReason);
	}

	bool WriteFileToJavaOutputStream(
		JNIEnv* env,
		const FString& sourceFilePath,
		jobject outputStream,
		jmethodID writeMethod,
		FString& outErrorReason)
	{
		TUniquePtr<FArchive> sourceReader(IFileManager::Get().CreateFileReader(*sourceFilePath));
		if (not sourceReader.IsValid())
		{
			outErrorReason = FString::Printf(TEXT("Failed to open media source file. Path=%s"), *sourceFilePath);
			return false;
		}

		constexpr int64 chunkCapacity = 256 * 1024;
		TArray<uint8> chunkBuffer;
		chunkBuffer.SetNumUninitialized(static_cast<int32>(chunkCapacity));

		jbyteArray javaChunk = env->NewByteArray(static_cast<jsize>(chunkCapacity));
		if (javaChunk == nullptr)
		{
			outErrorReason = TEXT("Failed to allocate Java media copy buffer.");
			CheckAndClearJavaException(env, TEXT("NewByteArray"), outErrorReason);
			return false;
		}

		int64 remainingBytes = sourceReader->TotalSize();
		while (remainingBytes > 0)
		{
			const int32 chunkSize = static_cast<int32>(FMath::Min<int64>(remainingBytes, chunkCapacity));
			sourceReader->Serialize(chunkBuffer.GetData(), chunkSize);
			if (sourceReader->IsError())
			{
				outErrorReason = FString::Printf(TEXT("Failed to read media source chunk. Path=%s"), *sourceFilePath);
				env->DeleteLocalRef(javaChunk);
				return false;
			}

			env->SetByteArrayRegion(
				javaChunk,
				0,
				static_cast<jsize>(chunkSize),
				reinterpret_cast<const jbyte*>(chunkBuffer.GetData()));
			if (not CheckAndClearJavaException(env, TEXT("SetByteArrayRegion"), outErrorReason))
			{
				env->DeleteLocalRef(javaChunk);
				return false;
			}

			env->CallVoidMethod(outputStream, writeMethod, javaChunk, 0, chunkSize);
			if (not CheckAndClearJavaException(env, TEXT("OutputStream.write"), outErrorReason))
			{
				env->DeleteLocalRef(javaChunk);
				return false;
			}

			remainingBytes -= chunkSize;
		}

		env->DeleteLocalRef(javaChunk);
		return true;
	}
#endif
}

bool VdjmRecordAndroidMediaStore::PublishVideoFileToMediaStore(
	const FString& sourceFilePath,
	const FString& displayName,
	const FString& relativePath,
	FVdjmRecordAndroidMediaStorePublishResult& outResult)
{
	outResult = FVdjmRecordAndroidMediaStorePublishResult();
	outResult.SourceFilePath = NormalizeAndroidMediaSourcePath(sourceFilePath);
	outResult.DisplayName = ResolveAndroidMediaDisplayName(outResult.SourceFilePath, displayName);
	outResult.RelativePath = ResolveAndroidMediaRelativePath(relativePath);
	outResult.SourceFileSizeBytes = IFileManager::Get().FileSize(*outResult.SourceFilePath);

	if (outResult.SourceFilePath.IsEmpty())
	{
		outResult.ErrorReason = TEXT("MediaStore publish source path is empty.");
		return false;
	}

	if (outResult.SourceFileSizeBytes <= 0)
	{
		outResult.ErrorReason = FString::Printf(
			TEXT("MediaStore publish source file is missing or empty. Path=%s Size=%lld"),
			*outResult.SourceFilePath,
			outResult.SourceFileSizeBytes);
		return false;
	}

#if PLATFORM_ANDROID
	JNIEnv* env = FAndroidApplication::GetJavaEnv();
	if (env == nullptr)
	{
		outResult.ErrorReason = TEXT("MediaStore publish failed because JNI environment is null.");
		return false;
	}

	jobject activity = FAndroidApplication::GetGameActivityThis();
	if (activity == nullptr)
	{
		outResult.ErrorReason = TEXT("MediaStore publish failed because GameActivity is null.");
		return false;
	}

	jclass activityClass = env->GetObjectClass(activity);
	jclass contentResolverClass = env->FindClass("android/content/ContentResolver");
	jclass contentValuesClass = env->FindClass("android/content/ContentValues");
	jclass integerClass = env->FindClass("java/lang/Integer");
	jclass mediaStoreVideoMediaClass = env->FindClass("android/provider/MediaStore$Video$Media");
	jclass uriClass = env->FindClass("android/net/Uri");
	jclass outputStreamClass = env->FindClass("java/io/OutputStream");

	if (activityClass == nullptr || contentResolverClass == nullptr || contentValuesClass == nullptr ||
		integerClass == nullptr || mediaStoreVideoMediaClass == nullptr || uriClass == nullptr ||
		outputStreamClass == nullptr)
	{
		outResult.ErrorReason = TEXT("MediaStore publish failed to find required Java classes.");
		CheckAndClearJavaException(env, TEXT("Find required MediaStore classes"), outResult.ErrorReason);
		return false;
	}

	jmethodID getContentResolverMethod = env->GetMethodID(
		activityClass,
		"getContentResolver",
		"()Landroid/content/ContentResolver;");
	jmethodID contentValuesConstructor = env->GetMethodID(contentValuesClass, "<init>", "()V");
	jmethodID putStringMethod = env->GetMethodID(
		contentValuesClass,
		"put",
		"(Ljava/lang/String;Ljava/lang/String;)V");
	jmethodID putIntegerMethod = env->GetMethodID(
		contentValuesClass,
		"put",
		"(Ljava/lang/String;Ljava/lang/Integer;)V");
	jmethodID clearMethod = env->GetMethodID(contentValuesClass, "clear", "()V");
	jmethodID integerValueOfMethod = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
	jfieldID externalContentUriField = env->GetStaticFieldID(
		mediaStoreVideoMediaClass,
		"EXTERNAL_CONTENT_URI",
		"Landroid/net/Uri;");
	jmethodID insertMethod = env->GetMethodID(
		contentResolverClass,
		"insert",
		"(Landroid/net/Uri;Landroid/content/ContentValues;)Landroid/net/Uri;");
	jmethodID openOutputStreamMethod = env->GetMethodID(
		contentResolverClass,
		"openOutputStream",
		"(Landroid/net/Uri;)Ljava/io/OutputStream;");
	jmethodID updateMethod = env->GetMethodID(
		contentResolverClass,
		"update",
		"(Landroid/net/Uri;Landroid/content/ContentValues;Ljava/lang/String;[Ljava/lang/String;)I");
	jmethodID deleteMethod = env->GetMethodID(
		contentResolverClass,
		"delete",
		"(Landroid/net/Uri;Ljava/lang/String;[Ljava/lang/String;)I");
	jmethodID uriToStringMethod = env->GetMethodID(uriClass, "toString", "()Ljava/lang/String;");
	jmethodID writeMethod = env->GetMethodID(outputStreamClass, "write", "([BII)V");
	jmethodID flushMethod = env->GetMethodID(outputStreamClass, "flush", "()V");
	jmethodID closeMethod = env->GetMethodID(outputStreamClass, "close", "()V");

	if (getContentResolverMethod == nullptr || contentValuesConstructor == nullptr || putStringMethod == nullptr ||
		putIntegerMethod == nullptr || clearMethod == nullptr || integerValueOfMethod == nullptr ||
		externalContentUriField == nullptr || insertMethod == nullptr || openOutputStreamMethod == nullptr ||
		updateMethod == nullptr || deleteMethod == nullptr || uriToStringMethod == nullptr || writeMethod == nullptr ||
		flushMethod == nullptr || closeMethod == nullptr)
	{
		outResult.ErrorReason = TEXT("MediaStore publish failed to find required Java members.");
		CheckAndClearJavaException(env, TEXT("Find required MediaStore members"), outResult.ErrorReason);
		return false;
	}

	jobject contentResolver = env->CallObjectMethod(activity, getContentResolverMethod);
	jobject contentValues = env->NewObject(contentValuesClass, contentValuesConstructor);
	jobject collectionUri = env->GetStaticObjectField(mediaStoreVideoMediaClass, externalContentUriField);
	if (contentResolver == nullptr || contentValues == nullptr || collectionUri == nullptr)
	{
		outResult.ErrorReason = TEXT("MediaStore publish failed to create ContentResolver, ContentValues, or collection Uri.");
		CheckAndClearJavaException(env, TEXT("Create MediaStore objects"), outResult.ErrorReason);
		return false;
	}

	if (not PutContentValueString(env, contentValues, putStringMethod, TEXT("_display_name"), outResult.DisplayName, outResult.ErrorReason) ||
		not PutContentValueString(env, contentValues, putStringMethod, TEXT("mime_type"), TEXT("video/mp4"), outResult.ErrorReason))
	{
		return false;
	}

	const int32 sdkInt = GetAndroidSdkInt(env);
	if (sdkInt >= 29)
	{
		if (not PutContentValueString(env, contentValues, putStringMethod, TEXT("relative_path"), outResult.RelativePath, outResult.ErrorReason) ||
			not PutContentValueInteger(env, contentValues, putIntegerMethod, integerValueOfMethod, TEXT("is_pending"), 1, outResult.ErrorReason))
		{
			return false;
		}
	}

	jobject publishedUri = env->CallObjectMethod(contentResolver, insertMethod, collectionUri, contentValues);
	if (publishedUri == nullptr)
	{
		outResult.ErrorReason = TEXT("MediaStore publish failed because insert returned null Uri.");
		CheckAndClearJavaException(env, TEXT("ContentResolver.insert"), outResult.ErrorReason);
		return false;
	}

	jobject outputStream = env->CallObjectMethod(contentResolver, openOutputStreamMethod, publishedUri);
	if (outputStream == nullptr)
	{
		outResult.ErrorReason = TEXT("MediaStore publish failed because openOutputStream returned null.");
		CheckAndClearJavaException(env, TEXT("ContentResolver.openOutputStream"), outResult.ErrorReason);
		env->CallIntMethod(contentResolver, deleteMethod, publishedUri, nullptr, nullptr);
		return false;
	}

	bool bWriteSucceeded = WriteFileToJavaOutputStream(
		env,
		outResult.SourceFilePath,
		outputStream,
		writeMethod,
		outResult.ErrorReason);

	env->CallVoidMethod(outputStream, flushMethod);
	CheckAndClearJavaException(env, TEXT("OutputStream.flush"), outResult.ErrorReason);
	env->CallVoidMethod(outputStream, closeMethod);
	CheckAndClearJavaException(env, TEXT("OutputStream.close"), outResult.ErrorReason);

	if (not bWriteSucceeded)
	{
		env->CallIntMethod(contentResolver, deleteMethod, publishedUri, nullptr, nullptr);
		CheckAndClearJavaException(env, TEXT("ContentResolver.delete failed publish"), outResult.ErrorReason);
		return false;
	}

	if (sdkInt >= 29)
	{
		env->CallVoidMethod(contentValues, clearMethod);
		if (not CheckAndClearJavaException(env, TEXT("ContentValues.clear"), outResult.ErrorReason) ||
			not PutContentValueInteger(env, contentValues, putIntegerMethod, integerValueOfMethod, TEXT("is_pending"), 0, outResult.ErrorReason))
		{
			env->CallIntMethod(contentResolver, deleteMethod, publishedUri, nullptr, nullptr);
			return false;
		}

		env->CallIntMethod(contentResolver, updateMethod, publishedUri, contentValues, nullptr, nullptr);
		if (not CheckAndClearJavaException(env, TEXT("ContentResolver.update is_pending=0"), outResult.ErrorReason))
		{
			env->CallIntMethod(contentResolver, deleteMethod, publishedUri, nullptr, nullptr);
			return false;
		}
	}

	jstring publishedUriString = static_cast<jstring>(env->CallObjectMethod(publishedUri, uriToStringMethod));
	if (publishedUriString != nullptr)
	{
		outResult.PublishedContentUri = JavaStringToFString(env, publishedUriString);
		env->DeleteLocalRef(publishedUriString);
	}

	outResult.bSucceeded = not outResult.PublishedContentUri.IsEmpty();
	if (not outResult.bSucceeded)
	{
		outResult.ErrorReason = TEXT("MediaStore publish succeeded but content Uri string is empty.");
		return false;
	}

	env->DeleteLocalRef(outputStream);
	env->DeleteLocalRef(publishedUri);
	env->DeleteLocalRef(collectionUri);
	env->DeleteLocalRef(contentValues);
	env->DeleteLocalRef(contentResolver);
	env->DeleteLocalRef(activityClass);
	env->DeleteLocalRef(contentResolverClass);
	env->DeleteLocalRef(contentValuesClass);
	env->DeleteLocalRef(integerClass);
	env->DeleteLocalRef(mediaStoreVideoMediaClass);
	env->DeleteLocalRef(uriClass);
	env->DeleteLocalRef(outputStreamClass);

	UE_LOG(LogVdjmRecorderCore, Log,
		TEXT("MediaStore publish succeeded. Source=%s Uri=%s Size=%lld"),
		*outResult.SourceFilePath,
		*outResult.PublishedContentUri,
		outResult.SourceFileSizeBytes);
	return true;
#else
	outResult.ErrorReason = TEXT("MediaStore publish is only supported on Android.");
	return false;
#endif
}

bool VdjmRecordAndroidMediaStore::ScanAndroidMediaFile(const FString& sourceFilePath, FString& outErrorReason)
{
	outErrorReason.Reset();
	const FString normalizedPath = NormalizeAndroidMediaSourcePath(sourceFilePath);
	if (normalizedPath.IsEmpty())
	{
		outErrorReason = TEXT("Media scan source path is empty.");
		return false;
	}

#if PLATFORM_ANDROID
	JNIEnv* env = FAndroidApplication::GetJavaEnv();
	if (env == nullptr)
	{
		outErrorReason = FString::Printf(TEXT("Failed to get JNI environment. Path=%s"), *normalizedPath);
		return false;
	}

	jobject activity = FAndroidApplication::GetGameActivityThis();
	if (activity == nullptr)
	{
		outErrorReason = FString::Printf(TEXT("GameActivity is null. Path=%s"), *normalizedPath);
		return false;
	}

	jclass stringClass = env->FindClass("java/lang/String");
	jclass mediaScannerClass = env->FindClass("android/media/MediaScannerConnection");
	if (stringClass == nullptr || mediaScannerClass == nullptr)
	{
		outErrorReason = FString::Printf(TEXT("Failed to find media scanner classes. Path=%s"), *normalizedPath);
		CheckAndClearJavaException(env, TEXT("Find media scanner classes"), outErrorReason);
		return false;
	}

	jmethodID scanFileMethod = env->GetStaticMethodID(
		mediaScannerClass,
		"scanFile",
		"(Landroid/content/Context;[Ljava/lang/String;[Ljava/lang/String;Landroid/media/MediaScannerConnection$OnScanCompletedListener;)V");
	if (scanFileMethod == nullptr)
	{
		outErrorReason = FString::Printf(TEXT("Failed to find MediaScannerConnection.scanFile. Path=%s"), *normalizedPath);
		CheckAndClearJavaException(env, TEXT("Find MediaScannerConnection.scanFile"), outErrorReason);
		env->DeleteLocalRef(stringClass);
		env->DeleteLocalRef(mediaScannerClass);
		return false;
	}

	jobjectArray paths = env->NewObjectArray(1, stringClass, nullptr);
	jobjectArray mimeTypes = env->NewObjectArray(1, stringClass, nullptr);
	jstring pathString = NewJavaString(env, normalizedPath);
	jstring mimeTypeString = NewJavaString(env, TEXT("video/mp4"));
	if (paths == nullptr || mimeTypes == nullptr || pathString == nullptr || mimeTypeString == nullptr)
	{
		outErrorReason = FString::Printf(TEXT("Failed to allocate media scan arrays. Path=%s"), *normalizedPath);
		CheckAndClearJavaException(env, TEXT("Allocate media scan arrays"), outErrorReason);
		return false;
	}

	env->SetObjectArrayElement(paths, 0, pathString);
	env->SetObjectArrayElement(mimeTypes, 0, mimeTypeString);
	env->CallStaticVoidMethod(mediaScannerClass, scanFileMethod, activity, paths, mimeTypes, nullptr);
	if (not CheckAndClearJavaException(env, TEXT("MediaScannerConnection.scanFile"), outErrorReason))
	{
		return false;
	}

	env->DeleteLocalRef(pathString);
	env->DeleteLocalRef(mimeTypeString);
	env->DeleteLocalRef(paths);
	env->DeleteLocalRef(mimeTypes);
	env->DeleteLocalRef(stringClass);
	env->DeleteLocalRef(mediaScannerClass);
	return true;
#else
	outErrorReason = TEXT("Media scan is only supported on Android.");
	return false;
#endif
}
