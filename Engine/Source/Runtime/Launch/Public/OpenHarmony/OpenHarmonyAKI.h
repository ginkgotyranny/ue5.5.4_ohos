#pragma once

#include "CoreMinimal.h"

#if USE_OPENHARMONY_NAPI

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "GenericPlatform/GenericPlatformFile.h"

#include <napi/native_api.h>
#include <rawfile/raw_dir.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include "aki/jsbind.h"

napi_value InitNapiEnv(napi_env env, napi_value exports)
{
	exports = aki::JSBind::BindSymbols(env, exports);
	return exports;
}

class CORE_API FOpenHarmonyNapiFunc
{
public:
	static void Napi_akiInit(const char* OpenHarmonyVersion, int MajorVersion, int SdkApiVersion,
		const char* DeviceMake, const char* DeviceModel, const char* BuildVersion, const char* UEOSLanguage);
};

#endif