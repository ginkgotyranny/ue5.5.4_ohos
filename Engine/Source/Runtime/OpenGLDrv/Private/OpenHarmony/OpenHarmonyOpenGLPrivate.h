// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenHarmonyOpenGLPrivate.h: Code shared betweeen OpenHarmonyOpenGL and OpenHarmonyESDeferredOpenGL (Removed)
=============================================================================*/
#pragma once

#include "OpenHarmonyEGL.h"
#include "OpenHarmony/OpenHarmonyApplication.h"

extern bool GOpenHarmonyGPUInfoReady;

// call out to JNI to see if the application was packaged for Oculus Mobile
// ignore JNI
// extern bool AndroidThunkCpp_IsOculusMobileApplication();
extern bool ShouldUseGPUFencesToLimitLatency();

DEFINE_LOG_CATEGORY_STATIC(LogFOpenHarmonyGPUInfo, Log, All);
class FOpenHarmonyGPUInfo
{
public:
	static FOpenHarmonyGPUInfo& Get()
	{
		static FOpenHarmonyGPUInfo This;
		return This;
	}

	FString GLVersion;
	FString VendorName;
	bool bSupportsFloatingPointRenderTargets;
	bool bSupportsFrameBufferFetch;
	TArray<FString> TargetPlatformNames;

	void RemoveTargetPlatform(FString PlatformName)
	{
		TargetPlatformNames.Remove(PlatformName);
	}

	// computing GPU family needs regex access, which might not be available early in init
	FString& GetGPUFamily()
	{
		return GPUFamily;
	}

private:
	FString GPUFamily;

	FOpenHarmonyGPUInfo();
};
