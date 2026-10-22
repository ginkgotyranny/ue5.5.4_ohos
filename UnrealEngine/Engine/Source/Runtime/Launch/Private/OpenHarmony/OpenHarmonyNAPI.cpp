#include "OpenHarmony/OpenHarmonyNAPI.h"

#if OPENHARMONY
#include "HAL/ExceptionHandling.h"
#include "OpenHarmony/OpenHarmonyPlatformCrashContext.h"
#include "Runtime/Core/Public/Misc/DateTime.h"
#include "HAL/PlatformStackWalk.h"
#include "OpenHarmony/OpenHarmonyApplication.h"
#include "OpenHarmony/OpenHarmonyInputInterface.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "UnrealEngine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Math/Vector.h"
#include "Misc/EmbeddedCommunication.h"
#include "Async/TaskGraphInterfaces.h"

#include <cerrno>

#include "Misc/Paths.h"
#include <dirent.h>

#define USE_UTIME 0
#define LOG_OPENHARMONY_FILE 1
#define UNUSE_RAW_PATH 1

// Pointer to target widget for virtual keyboard contents
napi_value getEngineFunc(const char* funcName);


static napi_env _env = nullptr;
napi_value initCCArkJsDialog(napi_env env, napi_value exports)
{
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("initCCArkJsDialog start!"));
    _env = env;
    return 0;
}

//下面是NAPI调用键盘的部分函数示例，当前使用AKI实现了，这部分屏蔽。对于性能有要求的需要使用NAPI实现。
//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeVirtualKeyboardChanged(String contents);"
// napi_value textFieldTTFOnChangeCB(napi_env Env, napi_callback_info Info)
// {
// 	size_t Argc = 1;
// 	napi_value Args[1];
// 	NAPI_CALL(Env, napi_get_cb_info(Env, Info, &Argc, Args, nullptr, nullptr));
// 	size_t TypeLen = 0;
// 	napi_get_value_string_utf8(Env, Args[0], nullptr, 0, &TypeLen);
// 	char* ContentsBuffer = new char[TypeLen + 1];
// 	napi_get_value_string_utf8(Env, Args[0], ContentsBuffer, TypeLen + 1, &TypeLen);

// 	if (VirtualKeyboardWidget.IsValid())
// 	{
// 		FString Contents(UTF8_TO_TCHAR(ContentsBuffer));
// 		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Contents is : %s"), *Contents);

// 		// call to set the widget text on game thread
// 		if (FTaskGraphInterface::IsRunning())
// 		{
// 			FGraphEventRef SetWidgetText = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
// 			{
// 				TSharedPtr<IVirtualKeyboardEntry> LockedKeyboardWidget(VirtualKeyboardWidget.Pin());
// 				if (LockedKeyboardWidget.IsValid())
// 				{
// 					LockedKeyboardWidget->SetTextFromVirtualKeyboard(FText::FromString(Contents), ETextEntryType::TextEntryUpdated);
//                     FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LockedKeyboardWidget!"));
// 				}
// 			}, TStatId(), NULL, ENamedThreads::GameThread);
// 			FTaskGraphInterface::Get().WaitUntilTaskCompletes(SetWidgetText);
// 		}
// 	}

// 	delete[] ContentsBuffer;
// 	return nullptr;
// }


// void showTextInputDialog(TSharedPtr<IVirtualKeyboardEntry> TextWidget, int32 InputType, const FString& Label, const FString& Contents)    //中间两个参数未使用
// {
//     FPlatformMisc::LowLevelOutputDebugStringf(TEXT("VirtualKeyboardInputType is : %d"), InputType);
//     FPlatformMisc::LowLevelOutputDebugStringf(TEXT("VirtualKeyboardLabel is : %s"), *Label);
//     FPlatformMisc::LowLevelOutputDebugStringf(TEXT("VirtualKeyboardContents is : %s"), *Contents);
//     // remember target widget for contents
// 	VirtualKeyboardWidget = TextWidget;

//     // call the OpenHarmony side
//     FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OpenHarmonyNAPI showTextInputDialog start!"));
//     napi_value global;
//     napi_status status = napi_get_global(_env, &global);
//     if (status != napi_ok) {
//         FPlatformMisc::LowLevelOutputDebugStringf(TEXT("showTextInputDialog get global failed! status:%d"), status);
//         return;
//     }
//     napi_value showTextInputDialog = getEngineFunc("showTextInputDialog");
//     if (showTextInputDialog == nullptr) {
//         return;
//     }
//     //transfer InputType to OpenHarmony
//     std::vector<napi_value> args;
//     napi_value argMsg;
//     napi_create_int32(_env, InputType, &argMsg);
//     args.push_back(argMsg);


//     //transfer Contents to OpenHarmony
//     const FString& MyString= Contents;
//     std::string Msg(TCHAR_TO_UTF8(*MyString));

//     napi_create_string_utf8(_env, Msg.data(), Msg.length(), &argMsg);
//     args.push_back(argMsg);


//     napi_value return_val;
//     status = napi_call_function(_env, global, showTextInputDialog, args.size(), args.data(), &return_val);
//     if (status != napi_ok) {
//         FPlatformMisc::LowLevelOutputDebugStringf(TEXT("CCArkJsDialog::showTextInputDialog napi_call_function failed! status:%d"), status);
//         return;
//     }
// }

napi_value getEngineFunc(const char* funcName)
{
    // Get the function named "AddTwo" on the global object
    napi_value global;
    napi_status status = napi_get_global(_env, &global);
    if (status != napi_ok) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s get global failed!"), funcName);
        return nullptr;
    }
    
    napi_value cocosNs;
    status = napi_get_named_property(_env, global, "ccDialogUtils", &cocosNs);
    if (status != napi_ok) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s get UE_NS failed!"), funcName);
        return nullptr;
    }        
    
    napi_value func;
    status = napi_get_named_property(_env, cocosNs, funcName, &func);
    if (status != napi_ok) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s get func failed!"), funcName);
        return nullptr;
    }
    
    napi_valuetype functype;
    napi_typeof(_env, func, &functype);
    if (functype != napi_function) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s get func but is not a function"), funcName);
        return nullptr;
    }    
    return func;
}

#endif