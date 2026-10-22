// Copyright Epic Games, Inc. All Rights Reserved.
#include "OpenHarmony/OpenHarmonyAKI.h"
#include "OpenHarmony/OpenHarmonyPlatformMisc.h"
#include "OpenHarmony/OpenHarmonyPlatformProcess.h"
#include "OpenHarmony/OpenHarmonyWindow.h"

#if USE_OPENHARMONY_NAPI
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
#include "Async/TaskGraphInterfaces.h"

#include <cerrno>

#include "Misc/Paths.h"
#include <dirent.h>
#include <string>

#define USE_UTIME 0
#define LOG_OPENHARMONY_FILE 1
#define UNUSE_RAW_PATH 1

void OpenHarmonyThunkCpp_LaunchURL(const FString& URL)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OpenHarmonyThunkCpp_LaunchURL:%s\n"), *URL);
	std::string url(TCHAR_TO_UTF8(*URL));
	auto jsFunc = aki::JSBind::GetJSFunction("LaunchURL");
	jsFunc->Invoke<void>(url);
}

FString OpenHarmonyThunkCpp_GetOpenHarmonyId();
FString OpenHarmonyThunkCpp_GetAdvertisingId()
{
	FString adIdResult = OpenHarmonyThunkCpp_GetOpenHarmonyId();
	return adIdResult;
}

FString GetOpenHarmonyId()
{
	std::promise<std::string> promise;
	std::function<void(std::string)> getOpenHarmonyIdCallback = [&promise](std::string message) {
		promise.set_value(message);
		};

	auto jsFunc = aki::JSBind::GetJSFunction("requestOAIDTrackingConsentPermissions");
	jsFunc->Invoke<void>(getOpenHarmonyIdCallback);
	std::string Msg = promise.get_future().get(); 

	return FString(UTF8_TO_TCHAR(Msg.c_str()));
}

FString OpenHarmonyThunkCpp_GetOpenHarmonyId()
{
	/* check permissions from user */
	auto jsFunc = aki::JSBind::GetJSFunction("PermissionsController.checkPermissionsFromUser");
	bool hasPermission = jsFunc->Invoke<bool>(std::string("ohos.permission.APP_TRACKING_CONSENT"));
	if (hasPermission)
	{
		return GetOpenHarmonyId();	//get data
	}
	else {
		/* request permissions from user */
		std::vector<std::string> permissions{ "ohos.permission.APP_TRACKING_CONSENT" };
		std::promise<std::pair<std::string, std::string>> reqPromise;
		std::function<void(std::string, std::string)> reqCallback =
			[&reqPromise](std::string authResults, std::string dialogShownResults) {
			std::pair<std::string, std::string> combinedMessages(authResults, dialogShownResults);
			reqPromise.set_value(combinedMessages);
			};

		auto jsFunc = aki::JSBind::GetJSFunction("PermissionsController.reqPermissionsFromUser");
		jsFunc->Invoke<void>(permissions, reqCallback);
		std::pair<std::string, std::string> reqRes = reqPromise.get_future().get(); // 子线程阻塞，直到promise.set_value

		if (reqRes.first[0] == '0') {
			return GetOpenHarmonyId();	//get data
		}
		else if (reqRes.second[0] != '0') {
			/* show permission setting dialog */
			std::promise<std::string> settingPromise;
			std::function<void(std::string)> settingCallback =
				[&settingPromise](std::string GrantStatus) {
				settingPromise.set_value(GrantStatus);
				};
			auto jsFunc = aki::JSBind::GetJSFunction("PermissionsController.openPermissionsSetting");
			jsFunc->Invoke<void>(permissions, settingCallback);
			std::string settingRes = settingPromise.get_future().get(); // 子线程阻塞，直到promise.set_value
			if (settingRes[0] == '0') {
				return GetOpenHarmonyId();	//get data
			}
			else {
				return FString();
			}
		}
		else {
			return FString();
		}
	}
}

//TODO:
void OpenHarmonyThunkCpp_ShareURL(const FString& URL, const FText& Description, const FText& SharePrompt, int32 LocationHintX, int32 LocationHintY)
{
	
}
void OpenHarmonyThunkCpp_ForceQuit()
{
	auto jsFunc = aki::JSBind::GetJSFunction("ForceQuit");
	jsFunc->Invoke<void>();
}

void OpenHarmonyThunkCpp_SetOrientation(int32 Value)
{
	auto jsFunc = aki::JSBind::GetJSFunction("Display.SetOrientation");
	jsFunc->Invoke<void>(Value);
}
//TODO:
void OpenHarmonyThunkCpp_SetCellularPreference(int32 Value)
{

}
//TODO:
int32 OpenHarmonyThunkCpp_GetCellularPreference()
{
	int32 value = 0;
	return value;
}

//restart
void OpenHarmonyThunkCpp_RestartApplication()
{
	auto jsFunc = aki::JSBind::GetJSFunction("Restart");
	jsFunc->Invoke<void>();
}
// call optional remote notification registration
void OpenHarmonyThunkCpp_RegisterForRemoteNotifications()
{

}

// call optional remote notification unregistration
void OpenHarmonyThunkCpp_UnregisterForRemoteNotifications()
{
	
}



bool OpenHarmonyThunkCpp_IsAllowedRemoteNotifications()
{
	bool Result = false;
	return Result;
}

void OpenHarmonyThunkCpp_AddNetworkListener()
{
	
}

void OpenHarmonyThunkCpp_RemoveNetworkListener()
{
	
}
//JS调用C++实现获取OpenHarmony版本号码、主版本号、sdk api版本、设备生产商、设备型号、设备构建版本、区域语言
void FOpenHarmonyNapiFunc::Napi_akiInit(const char* OpenHarmonyVersion, int MajorVersion, int SdkApiVersion, 
	const char* DeviceMake, const char* DeviceModel, const char* BuildVersion, const char* UEOSLanguage)
{
	FOpenHarmonyMisc::SetVersionInfo(FString(OpenHarmonyVersion), MajorVersion, SdkApiVersion, FString(DeviceMake), 
		FString(DeviceModel), FString(BuildVersion), FString(UEOSLanguage));

	// Wire up to core delegates, so core code can call out to ArkTS
	DECLARE_DELEGATE_OneParam(FOpenHarmonyLaunchURLDelegate, const FString&);
	extern CORE_API FOpenHarmonyLaunchURLDelegate OnOpenHarmonyLaunchURL;
	OnOpenHarmonyLaunchURL = FOpenHarmonyLaunchURLDelegate::CreateStatic(&OpenHarmonyThunkCpp_LaunchURL);
}

//JS调用C++传递大小核掩码
// void FOpenHarmonyNapiFunc::nativeSetAffinityInfo(bool bEnableAffinity, std::string bigCoreMask, std::string littleCoreMask)
// {
// 	//16进制转换
// 	int32 bigCoreMaskInt = stoi(bigCoreMask, 0 ,16);
// 	int32 littleCoreMaskInt = stoi(littleCoreMask, 0 ,16);
// 	FPlatformProcess::SetAffinityInfo(bEnableAffinity, bigCoreMaskInt, littleCoreMaskInt);
// }

// Pointer to target widget for virtual keyboard contents
static TWeakPtr<IVirtualKeyboardEntry> VirtualKeyboardWidget;
//This function is declared in the aki-defined class : "public native void Napi_nativeVirtualKeyboardChanged(String contents);"
void Napi_nativeVirtualKeyboardChanged(std::string contents)
{
	if (VirtualKeyboardWidget.IsValid())
	{
        FString Contents(UTF8_TO_TCHAR(contents.c_str()));
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Contents is : %s"), *Contents);

		// call to set the widget text on game thread
		if (FTaskGraphInterface::IsRunning())
		{
			FGraphEventRef SetWidgetText = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
			{
				TSharedPtr<IVirtualKeyboardEntry> LockedKeyboardWidget(VirtualKeyboardWidget.Pin());
				if (LockedKeyboardWidget.IsValid())
				{
					LockedKeyboardWidget->SetTextFromVirtualKeyboard(FText::FromString(Contents), ETextEntryType::TextEntryUpdated);
                    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LockedKeyboardWidget!"));
				}
			}, TStatId(), NULL, ENamedThreads::GameThread);
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(SetWidgetText);
		}
	}
}

//void Napi_nativeVirtualKeyboardSendKey(JNIEnv* jenv, jobject thiz, jint keyCode)
void Napi_nativeVirtualKeyboardSendKey(int32 keyCode)
{
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("keyCode is : %d"), keyCode);

	FDeferredOpenHarmonyMessage Message;

	Message.messageType = MessageType_KeyDown;
	Message.KeyEventData.keyId = keyCode;
	FOpenHarmonyInputInterface::DeferMessage(Message);
}

void OpenHarmonyThunkCpp_PushSensorEvents(){
	auto jsFunc = aki::JSBind::GetJSFunction("Sensor.PushSensorEvents");
	jsFunc->Invoke<void>();
}

void Napi_nativeHandleSensorEvents(std::vector<float> tiltFloatValues, std::vector<float> rotationRateFloatValues, std::vector<float> gravityFloatValues, std::vector<float> accelerationFloatValues)
{
	FVector current_tilt(tiltFloatValues[0], tiltFloatValues[1], tiltFloatValues[2]);
	FVector current_rotation_rate(rotationRateFloatValues[0], rotationRateFloatValues[1], rotationRateFloatValues[2]);
	FVector current_gravity(gravityFloatValues[0], gravityFloatValues[1], gravityFloatValues[2]);
	FVector current_acceleration(accelerationFloatValues[0], accelerationFloatValues[1], accelerationFloatValues[2]);

	FOpenHarmonyInputInterface::QueueMotionData(current_tilt, current_rotation_rate, current_gravity, current_acceleration);

	// FPlatformMisc::LowLevelOutputDebugStringf(TEXT("SensorLog current_tilt:[%f, %f, %f]\n"), current_tilt[0], current_tilt[1], current_tilt[2]);
	// FPlatformMisc::LowLevelOutputDebugStringf(TEXT("SensorLog current_rotation_rate:[%f, %f, %f]\n"), current_rotation_rate[0], current_rotation_rate[1], current_rotation_rate[2]);
	// FPlatformMisc::LowLevelOutputDebugStringf(TEXT("SensorLog current_gravity:[%f, %f, %f]\n"), current_gravity[0], current_gravity[1], current_gravity[2]);
	// FPlatformMisc::LowLevelOutputDebugStringf(TEXT("SensorLog current_acceleration:[%f, %f, %f]\n"), current_acceleration[0], current_acceleration[1], current_acceleration[2]);
}

//设置安全区
void Napi_nativeSetSafezoneInfo(bool bIsPortrait, float left, float top, float right, float bottom)
{
	FOpenHarmonyWindow::SetSafezoneInfo(bIsPortrait,left,top,right,bottom);
}

void Napi_nativeSetDeviceOrientation(int curOrientation) {
	FOpenHarmonyMisc::TmpDeviceOrientation = curOrientation;
}

//JS调用C++，方法绑定
JSBIND_GLOBAL() { 
    JSBIND_FUNCTION(FOpenHarmonyNapiFunc::Napi_akiInit, "Napi_akiInit");    //这里用了映射，JS里面调用的名称为akiSetVersionInfo
	JSBIND_FUNCTION(Napi_nativeVirtualKeyboardChanged);  //这里没用映射，JS里面调用的名称要和C++名称NapiNativeVirtualKeyboardChangedd一致
	JSBIND_FUNCTION(Napi_nativeVirtualKeyboardSendKey);
	JSBIND_FUNCTION(Napi_nativeHandleSensorEvents);
	JSBIND_FUNCTION(Napi_nativeSetSafezoneInfo);
	//JSBIND_FUNCTION(FOpenHarmonyNapiFunc::nativeSetAffinityInfo, "nativeSetAffinityInfo");
	JSBIND_FUNCTION(Napi_nativeSetDeviceOrientation);
}





//C++调JS实现键盘拉起和关闭
void ShowVirtualKeyboardInputDialog(TSharedPtr<IVirtualKeyboardEntry> TextWidget, int32 InputType, const FString& Label, const FString& Contents)
{
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("VirtualKeyboardInputType is : %d"), InputType);
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("VirtualKeyboardLabel is : %s"), *Label);
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("VirtualKeyboardContents is : %s"), *Contents);
    // remember target widget for contents
	VirtualKeyboardWidget = TextWidget;

    const FString& MyString= Contents;
    std::string Msg(TCHAR_TO_UTF8(*MyString));
	
	// C++调用JS方法
	auto jsFunc = aki::JSBind::GetJSFunction("OpenHarmonyShowVirtualKeyboardInputDialog");
    jsFunc -> Invoke<void>(InputType, Msg);
	
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OpenHarmonyShowVirtualKeyboardInputDialog start!"));
}

void HideVirtualKeyboardInputDialog()
{
    // Make sure virtual keyboard currently open
	if (!VirtualKeyboardWidget.IsValid())
	{
		return;
	}

    // ignore anything it might return
	VirtualKeyboardWidget.Reset();

     // call the OpenHarmony side
    auto jsFunc = aki::JSBind::GetJSFunction("OpenHarmonyHideVirtualKeyboardInputDialog");
	jsFunc -> Invoke<void>();

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OpenHarmonyHideVirtualKeyboardInputDialog start!"));
}

//C++调JS拉起ConsoleWindow
void OpenHarmonyThunkCpp_ShowConsoleWindow()
{
	// figure out all the possible texture formats that are allowed
	TArray<FString> PossibleTargetPlatforms;
	FPlatformMisc::GetValidTargetPlatforms(PossibleTargetPlatforms);

	// separate the format suffixes with commas
	FString ConsoleText;
	for (int32 FormatIndex = 0; FormatIndex < PossibleTargetPlatforms.Num(); FormatIndex++)
	{
		const FString& Format = PossibleTargetPlatforms[FormatIndex];
		int32 UnderscoreIndex;
		if (Format.FindLastChar('_', UnderscoreIndex))
		{
			if (ConsoleText != TEXT(""))
			{
					ConsoleText += ", ";
			}

			ConsoleText += Format.Mid(UnderscoreIndex + 1);
		}
	}
	
    std::string Msg(TCHAR_TO_UTF8(*ConsoleText));

	// call the OpenHarmony side
    auto jsFunc = aki::JSBind::GetJSFunction("ShowConsoleWindow");
	jsFunc -> Invoke<void>(Msg);
}

void OpenHarmonyThunkCpp_Vibrate(int32_t Duration)
{
	auto jsFunc = aki::JSBind::GetJSFunction("Vibrator.Vibrate");
	jsFunc->Invoke<void>(Duration);
}

// 从TS侧获取设备屏幕刷新率，给FOpenHarmonyOpenGLFramePacer使用
float GetNativeDisplayRefreshRate()
{
	auto jsFunc = aki::JSBind::GetJSFunction("Display.GetRefreshRate");
	float rate = jsFunc->Invoke<float>();
	return rate < 15.0f ? 60.0f : rate;
}

//TODO:
int GetNativeDeviceOrientation() {
	
}
float GetBatteryTemperature()
{
	auto jsFunc = aki::JSBind::GetJSFunction("BatteryInfo.GetBatteryTemperature");
	return jsFunc->Invoke<float>();
}

int GetBatteryChargeState()
{
	auto jsFunc = aki::JSBind::GetJSFunction("BatteryInfo.GetBatteryChargeState");
	return jsFunc->Invoke<int>();
}

int GetBatterySoC()
{
	auto jsFunc = aki::JSBind::GetJSFunction("BatteryInfo.GetBatterySoC");
	return jsFunc->Invoke<int>();
}

void OpenHarmonyThunkCpp_DismissSplashScreen()
{
	auto jsFunc = aki::JSBind::GetJSFunction("CCDialogUtils.closeSplashDialog");
	jsFunc->Invoke<void>();
}

int OpenHarmonyThunkCpp_GetNetworkConnectionType()
{
	auto jsFunc = aki::JSBind::GetJSFunction("Network.GetNetworkConnectionType");
	return jsFunc->Invoke<int>();
}

void OpenHarmonyThunkCpp_ClipboardCopy(const FString& Str)
{
    std::string Msg(TCHAR_TO_UTF8(*Str));

	auto jsFunc = aki::JSBind::GetJSFunction("OpenHarmonySetPasteboardData");
    jsFunc -> Invoke<void>(Msg);
}

FString GetPasteboardData()
{
	std::promise<std::string> promise;
	std::function<void(std::string)> pasteCallback = [&promise](std::string message) {
		promise.set_value(message);// 回调在JS线程被执行
	};
	
	auto jsFunc = aki::JSBind::GetJSFunction("OpenHarmonyGetPasteboardData");
	jsFunc -> Invoke<void>(pasteCallback);
	std::string Msg = promise.get_future().get(); // 子线程阻塞，直到promise.set_value
		
	return FString(UTF8_TO_TCHAR(Msg.c_str()));
}

FString OpenHarmonyThunkCpp_ClipboardPaste()
{
	/* check permissions from user */
	auto jsFunc = aki::JSBind::GetJSFunction("PermissionsController.checkPermissionsFromUser");
	bool hasPermission = jsFunc->Invoke<bool>(std::string("ohos.permission.READ_PASTEBOARD"));	
	if (hasPermission)
	{
		return GetPasteboardData();	//get data
	} else {		
		/* request permissions from user */
		std::vector<std::string> permissions{"ohos.permission.READ_PASTEBOARD"};
		std::promise<std::pair<std::string, std::string>> reqPromise;
		std::function<void(std::string, std::string)> reqCallback =
			[&reqPromise](std::string authResults, std::string dialogShownResults) {
				std::pair<std::string, std::string> combinedMessages(authResults, dialogShownResults);
				reqPromise.set_value(combinedMessages);
			};

		auto jsFunc = aki::JSBind::GetJSFunction("PermissionsController.reqPermissionsFromUser");
		jsFunc->Invoke<void>(permissions, reqCallback);
		std::pair<std::string, std::string> reqRes = reqPromise.get_future().get(); // 子线程阻塞，直到promise.set_value
		
		if (reqRes.first[0] == '0') {
			return GetPasteboardData();	//get data
		} else if (reqRes.second[0] != '0') {
			/* show permission setting dialog */
			std::promise<std::string> settingPromise;
			std::function<void(std::string)> settingCallback =
				[&settingPromise](std::string GrantStatus) {
					settingPromise.set_value(GrantStatus);
				};
			
			auto jsFunc = aki::JSBind::GetJSFunction("PermissionsController.openPermissionsSetting");
			jsFunc->Invoke<void>(permissions, settingCallback);
			std::string settingRes = settingPromise.get_future().get(); // 子线程阻塞，直到promise.set_value
			if (settingRes[0] == '0') {
				return GetPasteboardData();	//get data
			} else {
				return FString();
			}
		}else{
			return FString();
		}
	}
}

//C++调JS实现HMS Login、Pay功能
std::string SDKManagerMsghandlePullSDK(std::string SDKtype)
{
	//Login SDKtype有loginInit、authorization、getPlayerInfo、reportPlayerInfo
	//Pay SDKtype有checkPayEnv、queryConsumProdts、buy、consume、queryNoConsumableGoods、purchaseOfNonConsumableGoods、disposableGoodsOrderQuery、nonConsumableProductsRecords、consumableProductsRecords
	FString SDKTYPE(UTF8_TO_TCHAR(SDKtype.c_str()));
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("SDKtype is : %s"), *SDKTYPE);

	auto jsFunc = aki::JSBind::GetJSFunction("SDKManagerMsg.handlePullSDK");
	return jsFunc->Invoke<std::string>(SDKtype);
}

std::string OpenHarmonyThunkCpp_GetMetaDataString(std::string Key)
{
	auto jsFunc = aki::JSBind::GetJSFunction("EtsBundleManager.getMetaDataString");
	return jsFunc->Invoke<std::string>(Key);
}

double OpenHarmonyThunkCpp_GetDevicePhysicalSizeInInch()
{
	auto jsFunc = aki::JSBind::GetJSFunction("Display.GetDevicePhysicalSizeInInch");
	return jsFunc->Invoke<double>();
}
#endif