// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyOpenGLPlatform.h"

#include "OpenHarmony/OpenHarmonyApplication.h"
#include "OpenHarmony/OpenHarmonyEGL.h"
#include "OpenHarmony/OpenHarmonyOpenGLPrivate.h"
#include "OpenHarmony/OpenHarmonyPlatformMisc.h"

FString FOpenHarmonyMisc::GetGPUFamily()
{
	return FOpenHarmonyGPUInfo::Get().GetGPUFamily();
}

FString FOpenHarmonyMisc::GetGLVersion()
{
	return FOpenHarmonyGPUInfo::Get().GLVersion;
}

bool FOpenHarmonyMisc::SupportsFloatingPointRenderTargets()
{
	return FOpenHarmonyGPUInfo::Get().bSupportsFloatingPointRenderTargets;
}

bool FOpenHarmonyMisc::SupportsShaderFramebufferFetch()
{
	return FOpenHarmonyGPUInfo::Get().bSupportsFrameBufferFetch;
}

bool FOpenHarmonyMisc::SupportsES30()
{
	return true;
}

void FOpenHarmonyMisc::GetValidTargetPlatforms(TArray<FString>& TargetPlatformNames)
{
	TargetPlatformNames = FOpenHarmonyGPUInfo::Get().TargetPlatformNames;
}

void FOpenHarmonyAppEntry::PlatformInit()
{
	// Try to create an ES3.2 EGL here for gpu queries and don't have to recreate the GL context.
	OpenHarmonyEGL::GetInstance()->Init(OpenHarmonyEGL::AV_OpenGLES, 3, 2, false);
}

void FOpenHarmonyAppEntry::ReleaseEGL()
{
	OpenHarmonyEGL* EGL = OpenHarmonyEGL::GetInstance();
	if (EGL->IsInitialized())
	{
		EGL->DestroyBackBuffer();
		EGL->Terminate();
	}
}
