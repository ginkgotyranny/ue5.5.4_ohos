// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyOpenGL.h"

#if USE_OPENHARMONY_OPENGL

#include "OpenGLDrvPrivate.h"
#include "OpenGLES.h"
#include "OpenHarmony/OpenHarmonyWindow.h"
#include "OpenHarmonyOpenGLPrivate.h"
#include "OpenHarmony/OpenHarmonyPlatformMisc.h"
#include "OpenHarmony/OpenHarmonyPlatformFramePacer.h"
#include "Misc/ConfigCacheIni.h"
#include "String/Find.h"
#include "String/LexFromString.h"
#include "PSOMetrics.h"
#include <native_window/external_window.h>

int32 FOpenHarmonyOpenGL::GLMajorVerion = 0;
int32 FOpenHarmonyOpenGL::GLMinorVersion = 0;

bool FOpenHarmonyOpenGL::bSupportsImageExternal = false;
bool FOpenHarmonyOpenGL::bRequiresAdrenoTilingHint = false;

static TAutoConsoleVariable<int32> CVarEnableAdrenoTilingHint(
	TEXT("r.OpenHarmony.EnableAdrenoTilingHint"),
	1,
	TEXT("Whether Adreno-based OpenHarmony devices should hint to the driver to use tiling mode for the mobile base pass.\n")
	TEXT("  0 = hinting disabled\n")
	TEXT("  1 = hinting enabled for Adreno devices running Andorid 8 or earlier [default]\n")
	TEXT("  2 = hinting always enabled for Adreno devices\n"));

static TAutoConsoleVariable<int32> CVarDisableEarlyFragmentTests(
	TEXT("r.OpenHarmony.DisableEarlyFragmentTests"),
	0,
	TEXT("Whether to disable early_fragment_tests if any \n"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarDisableFBFNonCoherent(
	TEXT("r.OpenHarmony.DisableFBFNonCoherent"),
	0,
	TEXT("Whether to disable usage of QCOM_shader_framebuffer_fetch_noncoherent extension\n"),
	ECVF_ReadOnly);

struct FPlatformOpenGLDevice
{
	bool TargetDirty;

	void SetCurrentSharedContext();
	void SetCurrentRenderingContext();
	void SetupCurrentContext();
	void SetCurrentNULLContext();

	FPlatformOpenGLDevice();
	~FPlatformOpenGLDevice();
	void Init();
	void LoadEXT();
	void Terminate();
	void ReInit();
};


FPlatformOpenGLDevice::~FPlatformOpenGLDevice()
{
	// FPlatformRHIFramePacer::Destroy();
	
	// TODO: declare this in OpenHarmonyApplication.h
	// FOPenHarmonyAppEntry::ReleaseEGL();
}

FPlatformOpenGLDevice::FPlatformOpenGLDevice()
{
}

// call out to JNI to see if the application was packaged for Oculus Mobile
// ignore: jni function
//extern bool AndroidThunkCpp_IsOculusMobileApplication();


// RenderDoc
#define GL_DEBUG_TOOL_EXT	0x6789
static bool bRunningUnderRenderDoc = false;

void FPlatformOpenGLDevice::Init()
{
	// Initialize frame pacer
	FPlatformRHIFramePacer::Init(new FOpenHarmonyOpenGLFramePacer());

	extern void InitDebugContext();

	bRunningUnderRenderDoc = glIsEnabled(GL_DEBUG_TOOL_EXT) != GL_FALSE;

	FPlatformMisc::LowLevelOutputDebugString(TEXT("FPlatformOpenGLDevice:Init"));
	// ignore: jni function
	//bool bCreateSurface = !AndroidThunkCpp_IsOculusMobileApplication();
	bool bCreateSurface = true;
	OpenHarmonyEGL::GetInstance()->InitSurface(false, bCreateSurface);

	LoadEXT();
	PlatformRenderingContextSetup(this);

	InitDefaultGLContextState();
	InitDebugContext();

	PlatformSharedContextSetup(this);
	InitDefaultGLContextState();
	InitDebugContext();

	OpenHarmonyEGL::GetInstance()->InitBackBuffer(); //can be done only after context is made current.

#if 0 // USE_OPENHARMONY_OPENGL_SWAPPY
//	bool bIsSwappyEnabled = FAndroidPlatformRHIFramePacer::CVarUseSwappyForFramePacing.GetValueOnAnyThread() == 1;
	
//	// don't even initialize this if swappy is not enabled
//	if (bIsSwappyEnabled)
//	{
//		IConsoleVariable* CVarAndroidSupportsTimestampQueries = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Android.SupportsTimestampQueries"));
//		IConsoleVariable* CVarAndroidSupportsDynamicResolution = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Android.SupportsDynamicResolution"));

//		bool bSupportsTimestampQueries = CVarAndroidSupportsTimestampQueries != nullptr && CVarAndroidSupportsTimestampQueries->GetBool();
//		bool bSupportsDynamicResolution = CVarAndroidSupportsDynamicResolution != nullptr && CVarAndroidSupportsDynamicResolution->GetBool();

//		GRHISupportsDynamicResolution = bSupportsDynamicResolution;
//		GSupportsTimestampRenderQueries = bSupportsTimestampQueries;
//		GRHISupportsGPUTimestampBubblesRemoval = true;
//	}
#endif
}

FPlatformOpenGLDevice* PlatformCreateOpenGLDevice()
{
	FPlatformOpenGLDevice* Device = new FPlatformOpenGLDevice();
	Device->Init();
	return Device;
}

bool PlatformCanEnableGPUCapture()
{
	return bRunningUnderRenderDoc;
}

void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
}

void* PlatformGetWindow(FPlatformOpenGLContext* Context, void** AddParam)
{
	check(Context);

	return (void*)&Context->eglContext;
}

bool PlatformBlitToViewport(IRHICommandContext& RHICmdContext, FPlatformOpenGLDevice* Device, const FOpenGLViewport& Viewport, uint32 BackbufferSizeX, uint32 BackbufferSizeY, bool bPresent,bool bLockToVsync )
{
	SCOPED_NAMED_EVENT(STAT_PlatformBlitToViewportTime, FColor::Red)
	
	FPlatformOpenGLContext* const Context = Viewport.GetGLContext();

	if (bPresent && OpenHarmonyEGL::GetInstance()->IsOfflineSurfaceRequired())
	{
		if (Device->TargetDirty)
		{
			VERIFY_GL_SCOPE();
			glBindFramebuffer(GL_FRAMEBUFFER, Context->ViewportFramebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, Context->BackBufferTarget, Context->BackBufferResource, 0);

			Device->TargetDirty = false;
		}

		{
			VERIFY_GL_SCOPE();
// aln 机型暂不支持 GL_FRAMEBUFFER_SRGB, 下一代芯片会支持
#if !PLATFORM_OPENHARMONY
			glDisable(GL_FRAMEBUFFER_SRGB);
#endif
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			FOpenGL::DrawBuffer(GL_BACK);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, Context->ViewportFramebuffer);
			FOpenGL::ReadBuffer(GL_COLOR_ATTACHMENT0);

			FOpenGL::BlitFramebuffer(
				0, 0, BackbufferSizeX, BackbufferSizeY,
				0, BackbufferSizeY, BackbufferSizeX, 0,
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST
			);
#if !PLATFORM_OPENHARMONY
			glEnable(GL_FRAMEBUFFER_SRGB);
#endif
			// Bind viewport FBO so driver knows we don't need backbuffer image anymore
			glBindFramebuffer(GL_FRAMEBUFFER, Context->ViewportFramebuffer);
		}
	}

	if (bPresent && Viewport.GetCustomPresent())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FOpenHarmonyOpenGL_PlatformBlitToViewport_CustomPresent);
		int32 SyncInterval = FOpenHarmonyPlatformRHIFramePacer::GetLegacySyncInterval();
		bPresent = Viewport.GetCustomPresent()->Present(RHICmdContext, SyncInterval);
	}
	if (bPresent)
	{
		OpenHarmonyEGL::GetInstance()->UpdateBuffersTransform();
		FOpenHarmonyPlatformRHIFramePacer::SwapBuffers(bLockToVsync);
	}
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("a.UseFrameTimeStampsForPacing"));
	const bool bForceGPUFence = CVar ? CVar->GetInt() != 0 : false;

	return bPresent && ShouldUseGPUFencesToLimitLatency();
}

void PlatformRenderingContextSetup(FPlatformOpenGLDevice* Device)
{
	Device->SetCurrentRenderingContext();
	Device->SetupCurrentContext();
}

void PlatformFlushIfNeeded()
{
}

void PlatformSharedContextSetup(FPlatformOpenGLDevice* Device)
{
	Device->SetCurrentSharedContext();
	Device->SetupCurrentContext();
}

void PlatformNULLContextSetup()
{
	OpenHarmonyEGL::GetInstance()->ReleaseContextOwnership();
}

EOpenGLCurrentContext PlatformOpenGLCurrentContext(FPlatformOpenGLDevice* Device)
{
	return (EOpenGLCurrentContext)OpenHarmonyEGL::GetInstance()->GetCurrentContextType();
}

void PlatformRestoreDesktopDisplayMode()
{
}

bool PlatformInitOpenGL()
{
	check(!FOpenHarmonyMisc::ShouldUseVulkan());

	{
		// determine ES version. PlatformInitOpenGL happens before ProcessExtensions and therefore FOpenHarmonyOpenGL::bES31Support.
		FString FullVersionString, VersionString, SubVersionString;
		FOpenHarmonyGPUInfo::Get().GLVersion.Split(TEXT("OpenGL ES "), nullptr, &FullVersionString);
		FullVersionString.Split(TEXT(" "), &FullVersionString, nullptr);
		FullVersionString.Split(TEXT("."), &VersionString, &SubVersionString);
		FOpenHarmonyOpenGL::GLMajorVerion = FCString::Atoi(*VersionString);
		FOpenHarmonyOpenGL::GLMinorVersion = FCString::Atoi(*SubVersionString);

		bool bES32Supported = FOpenHarmonyOpenGL::GLMajorVerion == 3 && FOpenHarmonyOpenGL::GLMinorVersion >= 2; 
		static const auto CVarDisableES31 = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OpenHarmony.DisableOpenGLES31Support"));

		bool bBuildForES31 = false;
		GConfig->GetBool(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("bBuildForES31"), bBuildForES31, GEngineIni);

		// TODO: implement this in OpenHarmonyPlatformMisc
		// const bool bSupportsFloatingPointRTs = FAndroidMisc::SupportsFloatingPointRenderTargets();

		if (bBuildForES31 && bES32Supported)
		{
			FOpenGLES::CurrentFeatureLevelSupport = FOpenHarmonyOpenGL::GLMinorVersion >= 2 ? FOpenGLES::EFeatureLevelSupport::ES32 : FOpenGLES::EFeatureLevelSupport::ES31;
			UE_LOG(LogRHI, Log, TEXT("App is packaged for OpenGL ES 3.1 and an ES %d.%d-capable device was detected."), FOpenHarmonyOpenGL::GLMajorVerion, FOpenHarmonyOpenGL::GLMinorVersion);
		}
		else
		{
			FString Message = TEXT("");

			if (bES32Supported)
			{
				Message.Append(TEXT("This device does not support Vulkan but the app was not packaged with ES 3.1 support."));
				// TODO: implement this in OpenHarmonyPlatformMisc
				// if (FAndroidMisc::GetAndroidBuildVersion() < 26)
				// {
				// 	Message.Append(TEXT(" Updating to a newer Android version may resolve this issue."));
				// }
				FPlatformMisc::LowLevelOutputDebugString(*Message);
				// TODO: implement this in OpenHarmonyPlatformMisc
				// FAndroidMisc::MessageBoxExt(EAppMsgType::Ok, *Message, TEXT("Unable to run on this device!"));
			}
			else
			{
				Message.Append(TEXT("This device only supports OpenGL ES 2/3 which is not supported, only supports ES 3.1+ "));
				FPlatformMisc::LowLevelOutputDebugString(*Message);
				// TODO: implement this in OpenHarmonyPlatformMisc
				// FAndroidMisc::MessageBoxExt(EAppMsgType::Ok, *Message, TEXT("Unable to run on this device!"));
			}
		}
	}
	return true;
}

bool PlatformOpenGLContextValid()
{
	return OpenHarmonyEGL::GetInstance()->IsCurrentContextValid();
}

// =============================================================

void PlatformGetNewOcclusionQuery( GLuint* OutQuery, uint64* OutQueryContext )
{
}

bool PlatformContextIsCurrent( uint64 QueryContext )
{
	return true;
}

void FPlatformOpenGLDevice::LoadEXT()
{
	eglGetSystemTimeNV_p = (PFNEGLGETSYSTEMTIMENVPROC)((void*)eglGetProcAddress("eglGetSystemTimeNV"));
	eglCreateSyncKHR_p = (PFNEGLCREATESYNCKHRPROC)((void*)eglGetProcAddress("eglCreateSyncKHR"));
	eglDestroySyncKHR_p = (PFNEGLDESTROYSYNCKHRPROC)((void*)eglGetProcAddress("eglDestroySyncKHR"));
	eglClientWaitSyncKHR_p = (PFNEGLCLIENTWAITSYNCKHRPROC)((void*)eglGetProcAddress("eglClientWaitSyncKHR"));
	eglGetSyncAttribKHR_p = (PFNEGLGETSYNCATTRIBKHRPROC)((void*)eglGetProcAddress("eglGetSyncAttribKHR"));

	eglPresentationTimeOPENHARMONY_p = (PFNeglPresentationTimeOPENHARMONY)((void*)eglGetProcAddress("eglPresentationTimeOPENHARMONY"));
	eglGetNextFrameIdOPENHARMONY_p = (PFNeglGetNextFrameIdOPENHARMONY)((void*)eglGetProcAddress("eglGetNextFrameIdOPENHARMONY"));
	eglGetCompositorTimingOPENHARMONY_p = (PFNeglGetCompositorTimingOPENHARMONY)((void*)eglGetProcAddress("eglGetCompositorTimingOPENHARMONY"));
	eglGetFrameTimestampsOPENHARMONY_p = (PFNeglGetFrameTimestampsOPENHARMONY)((void*)eglGetProcAddress("eglGetFrameTimestampsOPENHARMONY"));
	eglQueryTimestampSupportedOPENHARMONY_p = (PFNeglQueryTimestampSupportedOPENHARMONY)((void*)eglGetProcAddress("eglQueryTimestampSupportedOPENHARMONY"));
	eglGetCompositorTimingSupportedOPENHARMONY_p = (PFNeglQueryTimestampSupportedOPENHARMONY)((void*)eglGetProcAddress("eglGetCompositorTimingSupportedOPENHARMONY"));
	eglGetFrameTimestampsSupportedOPENHARMONY_p = (PFNeglQueryTimestampSupportedOPENHARMONY)((void*)eglGetProcAddress("eglGetFrameTimestampsSupportedOPENHARMONY"));

	const TCHAR* NotAvailable = TEXT("NOT Available");
	const TCHAR* Present = TEXT("Present");

	UE_LOG(LogRHI, Log, TEXT("Extension %s %s"), TEXT("eglPresentationTimeOPENHARMONY"), eglPresentationTimeOPENHARMONY_p  ? Present : NotAvailable);
	UE_LOG(LogRHI, Log, TEXT("Extension %s %s"), TEXT("eglGetNextFrameIdOPENHARMONY"), eglGetNextFrameIdOPENHARMONY_p ? Present : NotAvailable);
	UE_LOG(LogRHI, Log, TEXT("Extension %s %s"), TEXT("eglGetCompositorTimingOPENHARMONY"), eglGetCompositorTimingOPENHARMONY_p  ? Present : NotAvailable);
	UE_LOG(LogRHI, Log, TEXT("Extension %s %s"), TEXT("eglGetFrameTimestampsOPENHARMONY"), eglGetFrameTimestampsOPENHARMONY_p  ? Present : NotAvailable);
	UE_LOG(LogRHI, Log, TEXT("Extension %s %s"), TEXT("eglQueryTimestampSupportedOPENHARMONY"), eglQueryTimestampSupportedOPENHARMONY_p ? Present : NotAvailable);
	UE_LOG(LogRHI, Log, TEXT("Extension %s %s"), TEXT("eglGetCompositorTimingSupportedOPENHARMONY"), eglGetCompositorTimingSupportedOPENHARMONY_p ? Present : NotAvailable);
	UE_LOG(LogRHI, Log, TEXT("Extension %s %s"), TEXT("eglGetFrameTimestampsSupportedOPENHARMONY"), eglGetFrameTimestampsSupportedOPENHARMONY_p ? Present : NotAvailable);

	glDebugMessageControlKHR = (PFNGLDEBUGMESSAGECONTROLKHRPROC)((void*)eglGetProcAddress("glDebugMessageControlKHR"));

	// Some PowerVR drivers (Rogue Han and Intel-based devices) are crashing using glDebugMessageControlKHR (causes signal 11 crash)
	// TODO: declare this in OpenHarmonyPlatformMisc
	// if (glDebugMessageControlKHR != NULL && FAndroidMisc::GetGPUFamily().Contains(TEXT("PowerVR")))
	// {
	// 	glDebugMessageControlKHR = NULL;
	// }

	glDebugMessageInsertKHR = (PFNGLDEBUGMESSAGEINSERTKHRPROC)((void*)eglGetProcAddress("glDebugMessageInsertKHR"));
	glDebugMessageCallbackKHR = (PFNGLDEBUGMESSAGECALLBACKKHRPROC)((void*)eglGetProcAddress("glDebugMessageCallbackKHR"));
	glDebugMessageLogKHR = (PFNGLGETDEBUGMESSAGELOGKHRPROC)((void*)eglGetProcAddress("glDebugMessageLogKHR"));
	glGetPointervKHR = (PFNGLGETPOINTERVKHRPROC)((void*)eglGetProcAddress("glGetPointervKHR"));
	glPushDebugGroupKHR = (PFNGLPUSHDEBUGGROUPKHRPROC)((void*)eglGetProcAddress("glPushDebugGroupKHR"));
	glPopDebugGroupKHR = (PFNGLPOPDEBUGGROUPKHRPROC)((void*)eglGetProcAddress("glPopDebugGroupKHR"));
	glObjectLabelKHR = (PFNGLOBJECTLABELKHRPROC)((void*)eglGetProcAddress("glObjectLabelKHR"));
	glGetObjectLabelKHR = (PFNGLGETOBJECTLABELKHRPROC)((void*)eglGetProcAddress("glGetObjectLabelKHR"));
	glObjectPtrLabelKHR = (PFNGLOBJECTPTRLABELKHRPROC)((void*)eglGetProcAddress("glObjectPtrLabelKHR"));
	glGetObjectPtrLabelKHR = (PFNGLGETOBJECTPTRLABELKHRPROC)((void*)eglGetProcAddress("glGetObjectPtrLabelKHR"));
}

FPlatformOpenGLContext* PlatformCreateOpenGLContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
	//Assumes Device is already initialized and context already created.
	return OpenHarmonyEGL::GetInstance()->GetRenderingContext();
}

void PlatformDestroyOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
}

FOpenGLTexture* PlatformCreateBuiltinBackBuffer(FOpenGLDynamicRHI* OpenGLRHI, uint32 SizeX, uint32 SizeY)
{
	check(IsInRenderingThread());
	// Create the built-in back buffer if we disable backbuffer sampling.
	// Otherwise return null and we will create an off-screen surface afterward.
	if(!OpenHarmonyEGL::GetInstance()->IsOfflineSurfaceRequired())
	{
		const FOpenGLTextureCreateDesc CreateDesc =
			FRHITextureCreateDesc::Create2D(TEXT("PlatformCreateBuiltinBackBuffer"), SizeX, SizeY, PF_B8G8R8A8)
			.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::Presentable | ETextureCreateFlags::ResolveTargetable)
			.DetermineInititialState();

		return new FOpenGLTexture(FRHICommandListImmediate::Get(), CreateDesc);
	}
	else
	{
		return nullptr;
	}
}

void PlatformResizeGLContext( FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context, uint32 SizeX, uint32 SizeY, bool bFullscreen, bool bWasFullscreen, GLenum BackBufferTarget, GLuint BackBufferResource)
{
	check(Context);
	VERIFY_GL_SCOPE();

	Context->BackBufferResource = BackBufferResource;
	Context->BackBufferTarget = BackBufferTarget;

	if (OpenHarmonyEGL::GetInstance()->IsOfflineSurfaceRequired())
	{
		Device->TargetDirty = true;
		glBindFramebuffer(GL_FRAMEBUFFER, Context->ViewportFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, BackBufferTarget, BackBufferResource, 0);
	}

	glViewport(0, 0, SizeX, SizeY);
	VERIFY_GL(glViewport);
}

void PlatformGetSupportedResolution(uint32 &Width, uint32 &Height)
{
}

bool PlatformGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	return true;
}

int32 PlatformGlGetError()
{
	return glGetError();
}

// =============================================================

void PlatformReleaseOcclusionQuery( GLuint Query, uint64 QueryContext )
{
}

void FPlatformOpenGLDevice::SetCurrentSharedContext()
{
	OpenHarmonyEGL::GetInstance()->SetCurrentSharedContext();
}

void PlatformDestroyOpenGLDevice(FPlatformOpenGLDevice* Device)
{
	delete Device;
}

void FPlatformOpenGLDevice::SetCurrentRenderingContext()
{
	OpenHarmonyEGL::GetInstance()->AcquireCurrentRenderingContext();
}

void FPlatformOpenGLDevice::SetupCurrentContext()
{
	GLuint* DefaultVao = nullptr;
	EOpenGLCurrentContext ContextType = (EOpenGLCurrentContext)OpenHarmonyEGL::GetInstance()->GetCurrentContextType();

	if (ContextType == CONTEXT_Rendering)
	{
		DefaultVao = &OpenHarmonyEGL::GetInstance()->GetRenderingContext()->DefaultVertexArrayObject;
	}
	else if (ContextType == CONTEXT_Shared)
	{
		DefaultVao = &OpenHarmonyEGL::GetInstance()->GetSharedContext()->DefaultVertexArrayObject;
	}
	else
	{
		//Invalid or Other return
		return;
	}
	
	if (*DefaultVao == 0)
	{
		glGenVertexArrays(1, DefaultVao);
		glBindVertexArray(*DefaultVao);
	}
}

//--------------------------------

bool FOpenHarmonyOpenGL::SupportsFramebufferSRGBEnable()
{	
	static auto* MobileUseHWsRGBEncodingCVAR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.UseHWsRGBEncoding"));
	const bool bMobileUseHWsRGBEncoding = (MobileUseHWsRGBEncodingCVAR && MobileUseHWsRGBEncodingCVAR->GetValueOnAnyThread() == 1);
	return bMobileUseHWsRGBEncoding;
}

FOpenHarmonyOpenGL::EImageExternalType FOpenHarmonyOpenGL::ImageExternalType = FOpenHarmonyOpenGL::EImageExternalType::None;

// ignore: jni function
// extern bool AndroidThunkCpp_GetMetaDataBoolean(const FString& Key);
// extern FString AndroidThunkCpp_GetMetaDataString(const FString& Key);

void FOpenHarmonyOpenGL::SetupDefaultGLContextState(const FString& ExtensionsString)
{
	// Enable QCOM non-coherent framebuffer fetch if supported
	if (CVarDisableFBFNonCoherent.GetValueOnAnyThread() == 0 &&
		ExtensionsString.Contains(TEXT("GL_QCOM_shader_framebuffer_fetch_noncoherent")) && 
		ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")))
	{
		bDefaultStateNonCoherentFramebufferFetchEnabled = true;
		glEnable(GL_FRAMEBUFFER_FETCH_NONCOHERENT_QCOM);
	}
}

bool FOpenHarmonyOpenGL::RequiresAdrenoTilingModeHint()
{
	return bRequiresAdrenoTilingHint;
}

void FOpenHarmonyOpenGL::EnableAdrenoTilingModeHint(bool bEnable)
{
	if(bEnable && CVarEnableAdrenoTilingHint.GetValueOnAnyThread() != 0)
	{
		glEnable(GL_BINNING_CONTROL_HINT_QCOM);
		glHint(GL_BINNING_CONTROL_HINT_QCOM, GL_GPU_OPTIMIZED_QCOM);
	}
	else
	{
		glDisable(GL_BINNING_CONTROL_HINT_QCOM);
	}
}

bool FOpenHarmonyOpenGL::ResetNonCoherentFramebufferFetch()
{
	if (bDefaultStateNonCoherentFramebufferFetchEnabled)
	{
		glEnable(GL_FRAMEBUFFER_FETCH_NONCOHERENT_QCOM);
		return true;
	}
	return false;
}

void FOpenHarmonyOpenGL::DisableNonCoherentFramebufferFetch()
{
	if (bDefaultStateNonCoherentFramebufferFetchEnabled)
	{
		glDisable(GL_FRAMEBUFFER_FETCH_NONCOHERENT_QCOM);
	}
}

bool FOpenHarmonyOpenGL::bDefaultStateNonCoherentFramebufferFetchEnabled = false;

void FOpenHarmonyOpenGL::ProcessExtensions(const FString& ExtensionsString)
{
	FString VersionString = FString(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VERSION)));
	FString SubVersionString;

	FOpenGLES::ProcessExtensions(ExtensionsString);

	FString RendererString = FString(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_RENDERER)));

	// Common GPU types
	const bool bIsNvidiaBased = RendererString.Contains(TEXT("NVIDIA"));
	const bool bIsPoverVRBased = RendererString.Contains(TEXT("PowerVR"));
	const bool bIsAdrenoBased = RendererString.Contains(TEXT("Adreno"));
	const bool bIsMaliBased = RendererString.Contains(TEXT("Mali"));
	const bool bIsSamsungBased = RendererString.Contains(TEXT("Xclipse"));

	if (bIsPoverVRBased)
	{
		bHasHardwareHiddenSurfaceRemoval = true;
		UE_LOG(LogRHI, Log, TEXT("Enabling support for Hidden Surface Removal on PowerVR"));
	}

	if (bIsAdrenoBased)
	{
		uint32 AdrenoDriverMajorVersion = 0;
		FStringView VersionStringView = MakeStringView(VersionString);
		FStringView AdrenoDriverVersionPrefix = TEXT("V@");
		int32 VersionStart = UE::String::FindFirst(VersionStringView, AdrenoDriverVersionPrefix, ESearchCase::CaseSensitive);
		if (VersionStart != INDEX_NONE)
		{
			VersionStart += AdrenoDriverVersionPrefix.Len();
			const int32 VersionCharCount = UE::String::FindFirst(VersionStringView.Mid(VersionStart), TEXT("."), ESearchCase::CaseSensitive);
			if (VersionCharCount != INDEX_NONE)
			{
				LexFromString(AdrenoDriverMajorVersion, VersionStringView.SubStr(VersionStart, VersionCharCount));
			}
		}

		GRHIMaximumInFlightQueries = 510;
		// This is to avoid a bug in Adreno drivers that define GL_ARM_shader_framebuffer_fetch_depth_stencil even when device does not support this extension
		// OpenGL ES 3.1 V@127.0 (GIT@I1af360237c)
		bRequiresARMShaderFramebufferFetchDepthStencilUndef = !bSupportsShaderDepthStencilFetch;

		if (AdrenoDriverMajorVersion > 0 && AdrenoDriverMajorVersion < 331)
		{
			// Shader compiler causes a freeze on older drivers
			// version 331 is known to work, 313 known not to work
			bSupportsShaderFramebufferFetchProgrammableBlending = false;
		}

		// FORT-221329's broken adreno driver not common on Android 9 and above. TODO: check adreno driver version instead.
		UE_CLOG(bRequiresAdrenoTilingHint, LogRHI, Log, TEXT("Enabling Adreno tiling hint."));
	}

	// Disable ASTC if requested by device profile
	// TODO: check if this exists
	//static const auto CVarDisableASTC = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OpenHarmony.DisableASTCSupport"));
	//if (bSupportsASTC && CVarDisableASTC->GetValueOnAnyThread())
	//{
	//	bSupportsASTC = false;
	//	FOpenHarmonyGPUInfo::Get().RemoveTargetPlatform(TEXT("OpenHarmony_ASTC"));
	//	UE_LOG(LogRHI, Log, TEXT("ASTC was disabled via r.OpenGL.DisableASTCSupport"));
	//}

	// Check for external image support for different ES versions
	ImageExternalType = EImageExternalType::None;

	static const auto CVarOverrideExternalTextureSupport = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OpenHarmony.OverrideExternalTextureSupport"));
	const int32 OverrideExternalTextureSupport = 0; // CVarOverrideExternalTextureSupport->GetValueOnAnyThread();
	switch (OverrideExternalTextureSupport)
	{
	case 1:
		ImageExternalType = EImageExternalType::None;
		break;

	case 2:
		ImageExternalType = EImageExternalType::ImageExternal100;
		break;

	case 3:
		ImageExternalType = EImageExternalType::ImageExternal300;
		break;

	case 4:
		ImageExternalType = EImageExternalType::ImageExternalESSL300;
		break;

	case 0:
	default:
		// auto-detect by extensions (default)
		bool bHasImageExternal = ExtensionsString.Contains(TEXT("GL_OES_EGL_image_external ")) || ExtensionsString.EndsWith(TEXT("GL_OES_EGL_image_external"));
		bool bHasImageExternalESSL3 = ExtensionsString.Contains(TEXT("OES_EGL_image_external_essl3"));
		if (bHasImageExternal || bHasImageExternalESSL3)
		{
			ImageExternalType = EImageExternalType::ImageExternal100;
			if (bHasImageExternalESSL3)
			{
				ImageExternalType = EImageExternalType::ImageExternalESSL300;
			}
			else
			{
				// Adreno 5xx can do essl3 even without extension in list
				if (bIsAdrenoBased && RendererString.Contains(TEXT("(TM) 5")))
				{
					ImageExternalType = EImageExternalType::ImageExternalESSL300;
				}
			}
			
			if (bIsNvidiaBased)
			{
				// Nvidia needs version 100 even though it supports ES3
				ImageExternalType = EImageExternalType::ImageExternal100;
			}
		}
		break;
	}
	switch (ImageExternalType)
	{
	case EImageExternalType::None:
		UE_LOG(LogRHI, Log, TEXT("Image external disabled"));
		break;

	case EImageExternalType::ImageExternal100:
		UE_LOG(LogRHI, Log, TEXT("Image external enabled: ImageExternal100"));
		break;

	case EImageExternalType::ImageExternal300:
		UE_LOG(LogRHI, Log, TEXT("Image external enabled: ImageExternal300"));
		break;

	case EImageExternalType::ImageExternalESSL300:
		UE_LOG(LogRHI, Log, TEXT("Image external enabled: ImageExternalESSL300"));
		break;

	default:
		ImageExternalType = EImageExternalType::None;
		UE_LOG(LogRHI, Log, TEXT("Image external disabled; unknown type"));
	}
	bSupportsImageExternal = ImageExternalType != EImageExternalType::None;

	// check for supported texture formats if enabled
	bool bCookOnTheFly = false;
#if !UE_BUILD_SHIPPING
	FString FileHostIP;
	bCookOnTheFly = FParse::Value(FCommandLine::Get(), TEXT("filehostip"), FileHostIP);
#endif
}

#endif
