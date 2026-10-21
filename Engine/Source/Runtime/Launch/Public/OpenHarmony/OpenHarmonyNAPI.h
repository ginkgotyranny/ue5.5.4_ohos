#pragma once

#if USE_OPENHARMONY_NAPI
#include "CoreMinimal.h"

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

/**
 * OpenHarmony File I/O implementation
**/

// class CORE_API OpenHarmonyNAPI {
// public:
//     static napi_value initCCArkJsDialog(napi_env env, napi_value exports);
//     static napi_value textFieldTTFOnChangeCB(napi_env env, napi_callback_info info);
//     static void showTextInputDialog(TSharedPtr<IVirtualKeyboardEntry> TextWidget, int32 InputType, const FString& Label, const FString& Contents);
//     static void hideTextInputDialog();
    
//     static napi_value getEngineFunc(const char* funcName);
    
// private:
//     static napi_env _env;
// };
#endif