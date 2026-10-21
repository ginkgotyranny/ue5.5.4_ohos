// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
		OpenHarmonySurvey.cpp: HardwareSurvey implementation
=============================================================================*/

#include "OpenHarmony/OpenHarmonyPlatformSurvey.h"
#include "OpenHarmony/OpenHarmonyWindow.h"

bool FOpenHarmonyPlatformSurvey::GetSurveyResults(FHardwareSurveyResults& OutResults, bool bWait)
{
#if PLATFORM_64BITS
	OutResults.OSBits = 64;
#else
	OutResults.OSBits = 32;
#endif
	FCString::Strcpy(OutResults.Platform, *(FOpenHarmonyMisc::GetDeviceMake() + TEXT("-") + FOpenHarmonyMisc::GetDeviceModel()));
	FCString::Strcpy(OutResults.OSVersion, *FOpenHarmonyMisc::GetOpenHarmonyVersion());
	FCString::Strcpy(OutResults.OSLanguage, *FPlatformMisc::GetDefaultLocale());
	// @todo vulkan: Get Vulkan version somehow
	FCString::Strcpy(OutResults.RenderingAPI, FOpenHarmonyMisc::ShouldUseVulkan() ? TEXT("Vulkan") : *FOpenHarmonyMisc::GetGLVersion());
	FCString::Strcpy(OutResults.RHIAdapter.AdapterName, *FOpenHarmonyMisc::GetGPUFamily());
	// display 0 is max size
	int32 ScreenWidth, ScreenHeight;
	FOpenHarmonyWindow::CalculateSurfaceSize(ScreenWidth, ScreenHeight);
	OutResults.Displays[0].CurrentModeWidth = ScreenWidth;
	OutResults.Displays[0].CurrentModeHeight = ScreenHeight;
	OutResults.CPUCount = FPlatformMisc::NumberOfCores();
	// display 1 is current size
	FPlatformRect ViewSize = FOpenHarmonyWindow::GetScreenRect();
	OutResults.Displays[1].CurrentModeWidth = ViewSize.Right - ViewSize.Left;
	OutResults.Displays[1].CurrentModeHeight = ViewSize.Bottom - ViewSize.Top;
	return true;
}
