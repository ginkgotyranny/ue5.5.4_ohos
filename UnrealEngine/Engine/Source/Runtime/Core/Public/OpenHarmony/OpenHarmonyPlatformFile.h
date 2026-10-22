// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	OpenHarmonyPlatformFile.h: OpenHarmony platform File functions
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "GenericPlatform/GenericPlatformFile.h"

#include <napi/native_api.h>
#include <rawfile/raw_dir.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>

#define GET_AND_THROW_LAST_ERROR(env)                                                                                  \
    do {                                                                                                               \
        const napi_extended_error_info* errorInfo = nullptr;                                                           \
        napi_get_last_error_info((env), &errorInfo);                                                                   \
        bool isPending = false;                                                                                        \
        napi_is_exception_pending((env), &isPending);                                                                  \
        if (!isPending && errorInfo != nullptr) {                                                                      \
            const char* errorMessage =                                                                                 \
                errorInfo->error_message != nullptr ? errorInfo->error_message : "empty error message";                \
            napi_throw_error((env), nullptr, errorMessage);                                                            \
        }                                                                                                              \
    } while (0)

#define NAPI_CALL_BASE(env, theCall, retVal)                                                                           \
    do {                                                                                                               \
        if ((theCall) != napi_ok) {                                                                                    \
            GET_AND_THROW_LAST_ERROR((env));                                                                           \
            return retVal;                                                                                             \
        }                                                                                                              \
    } while (0)

#define NAPI_CALL(env, theCall) NAPI_CALL_BASE(env, theCall, nullptr)

class IFileHandle;
template <typename FuncType> class TFunctionRef;


class CORE_API IOpenHarmonyPlatformFile : public IPhysicalPlatformFile
{
public:
#if USE_OPENHARMONY_NAPI
	virtual NativeResourceManager *GetAssetManager() = 0;
#endif
	
	static IOpenHarmonyPlatformFile& GetPlatformPhysical();

//#if USE_ANDROID_FILE
//	/**
//	 * Get the directory path to write log files to.
//	 * This is /temp0 in shipping, or a path inside /data for other configs.
//	 */
	static const FString* GetOverrideLogDirectory();
//#endif

//#if USE_ANDROID_JNI
//	// Get the android.content.res.AssetManager that Java code
//	// should use to open APK assets.
//	virtual jobject GetAssetManager() = 0;
//#endif

	// Get detailed information for a file that
	// we can hand to other Android media classes for access.

	// Is file embedded as an asset in the APK?
	virtual bool IsAsset(const FString& AssetPath) = 0;

	// Offset within file or asset where its data starts.
	// Note, offsets for assets is relative to complete APK file
	// and matches what is returned by AssetFileDescriptor.getStartOffset().
	virtual int64 FileStartOffset(const TCHAR* Filename) = 0;

	// Get the root, i.e. underlying, path for the file. This
	// can be any of: a resolved file path, an OBB path, an
	// asset path.
	virtual FString FileRootPath(const TCHAR* Filename) = 0;
};

