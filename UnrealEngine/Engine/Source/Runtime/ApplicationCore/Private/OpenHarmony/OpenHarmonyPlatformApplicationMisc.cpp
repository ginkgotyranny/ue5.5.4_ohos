// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmony/OpenHarmonyPlatformApplicationMisc.h"
#include "OpenHarmony/OpenHarmonyErrorOutputDevice.h"


#include "OpenHarmony/OpenHarmonyApplication.h"
/*
#include "OpenHarmony/OpenHarmonyInputInterface.h"
#include "OpenHarmony/OpenHarmonyWindow.h"
#include "OpenHarmony/OpenHarmonyCursor.h"
#include "IInputDeviceModule.h"
#include "HAL/OutputDevices.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CoreDelegates.h"

#include "HAL/PlatformMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Regex.h"
#include "Modules/ModuleManager.h"
*/


void FOpenHarmonyApplicationMisc::LoadPreInitModules()
{
    FModuleManager::Get().LoadModule(TEXT("OpenGLDrv"));
#if USE_OPENHARMONY_AUDIO
//    FModuleManager::Get().LoadModule(TEXT("OpenHarmonyAudio"));
	FModuleManager::Get().LoadModule(TEXT("AudioMixerOpenHarmony"));
#endif
}

GenericApplication* FOpenHarmonyApplicationMisc::CreateApplication()
{
	return FOpenHarmonyApplication::CreateOpenHarmonyApplication();
}

extern void OpenHarmonyThunkCpp_ClipboardCopy(const FString& Str);
void FOpenHarmonyApplicationMisc::ClipboardCopy(const TCHAR* Str)
{
#if USE_OPENHARMONY_NAPI
	OpenHarmonyThunkCpp_ClipboardCopy(Str);
#endif
}

extern FString OpenHarmonyThunkCpp_ClipboardPaste();
void FOpenHarmonyApplicationMisc::ClipboardPaste(class FString& Result)
{
#if USE_OPENHARMONY_NAPI
	Result = OpenHarmonyThunkCpp_ClipboardPaste();
#endif
}
