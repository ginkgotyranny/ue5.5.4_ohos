// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyOpenGLPrivate.h"

bool GOpenHarmonyGPUInfoReady = false;

FOpenHarmonyGPUInfo::FOpenHarmonyGPUInfo()
{
	// this is only valid in the game thread, make sure we are initialized there before being called on other threads!
	check(IsInGameThread())

	// make sure GL is started so we can get the supported formats
	OpenHarmonyEGL* EGL = OpenHarmonyEGL::GetInstance();

	if (!EGL->IsInitialized())
	{
		FOpenHarmonyAppEntry::PlatformInit();
	}

	// Do not create a window surface if the app is for Oculus Mobile (use small buffer)
	// ignore: jni function
	// bool bCreateSurface = !AndroidThunkCpp_IsOculusMobileApplication();
	bool bCreateSurface = true;
	FPlatformMisc::LowLevelOutputDebugString(TEXT("FOpenHarmonyGPUInfo"));
	EGL->InitSurface(bCreateSurface, bCreateSurface);
	EGL->SetCurrentSharedContext();

	// get extensions
	// Process the extension caps directly here, as FOpenGL might not yet be setup
	// Do not process extensions here, because extension pointers may not be setup
	const ANSICHAR* GlGetStringOutput = (const ANSICHAR*)glGetString(GL_EXTENSIONS);
	FString ExtensionsString = GlGetStringOutput;

	GPUFamily = (const ANSICHAR*)glGetString(GL_RENDERER);
	check(!GPUFamily.IsEmpty());

	GLVersion = (const ANSICHAR*)glGetString(GL_VERSION);

	UE_LOG(LogFOpenHarmonyGPUInfo, Warning, TEXT("OpenExtensionsString: %s"), *ExtensionsString);

	// highest priority is the per-texture version
	if (ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr")))
	{
		TargetPlatformNames.Add(TEXT("OpenHarmony_ASTC"));
	}
	if (ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")))
	{
		TargetPlatformNames.Add(TEXT("OpenHarmony_DXT"));
	}

	TargetPlatformNames.Add(TEXT("OpenHarmony_ETC2"));

	// finally, generic OpenHarmony
	TargetPlatformNames.Add(TEXT("OpenHarmony"));

	bSupportsFloatingPointRenderTargets =
		ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"))
		// According to https://www.khronos.org/registry/gles/extensions/EXT/EXT_color_buffer_float.txt
		|| (ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_float")));

	bSupportsFrameBufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch"))
		|| ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch ")); // has space at the end to exclude GL_ARM_shader_framebuffer_fetch_depth_stencil match

	GOpenHarmonyGPUInfoReady = true;
}
