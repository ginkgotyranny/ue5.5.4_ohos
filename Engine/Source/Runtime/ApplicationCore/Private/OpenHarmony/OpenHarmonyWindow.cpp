#include "OpenHarmony/OpenHarmonyWindow.h"
#include "OpenHarmony/OpenHarmonyWindowUtils.h"
#include "OpenHarmony/OpenHarmonyEventManager.h"
#include "HAL/OutputDevices.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformStackWalk.h"
#if USE_OPENHARMONY_NAPI
#include "aki/jsbind.h"
#endif
int GOpenHarmonyPropagateAlpha = 0; 
struct FCachedWindowRect
{
	FCachedWindowRect() : WindowWidth(-1), WindowHeight(-1), WindowInit(false), ContentScaleFactor(-1.0f), Window_EventThread(nullptr)
	{
	}

	int32 WindowWidth;
	int32 WindowHeight;
	bool WindowInit;
	float ContentScaleFactor;
	void* Window_EventThread;
};

// Cached calculated screen resolution
static FCachedWindowRect CachedWindowRect;
static FCachedWindowRect CachedWindowRect_EventThread;

static void ClearCachedWindowRects()
{
	CachedWindowRect.WindowInit = false;
	CachedWindowRect_EventThread.WindowInit = false;
}

static bool GOpenHarmonyIsPortrait = false;
static std::atomic<bool> GOpenHarmonySafezoneRequiresUpdate = false;
static FVector4 GOpenHarmonyPortraitSafezone = FVector4(-1.0f, -1.0f, -1.0f, -1.0f);
static FVector4 GOpenHarmonyLandscapeSafezone = FVector4(-1.0f, -1.0f, -1.0f, -1.0f);

bool FOpenHarmonyWindow::bAreCachedNativeDimensionsValid = false;
int32 FOpenHarmonyWindow::CachedNativeWindowWidth = -1;
int32 FOpenHarmonyWindow::CachedNativeWindowHeight = -1;

void* FOpenHarmonyWindow::NativeWindow = NULL;

FOpenHarmonyWindow::~FOpenHarmonyWindow()
{
}

TSharedRef<FOpenHarmonyWindow> FOpenHarmonyWindow::Make()
{
	return MakeShareable( new FOpenHarmonyWindow() );
}

FOpenHarmonyWindow::FOpenHarmonyWindow()
{
}

void FOpenHarmonyWindow::Initialize( class FOpenHarmonyApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FOpenHarmonyWindow >& InParent, const bool bShowImmediately )
{
	OwningApplication = Application;
	Definition = InDefinition;
}

bool FOpenHarmonyWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	FPlatformRect ScreenRect = GetScreenRect();

	X = ScreenRect.Left;
	Y = ScreenRect.Top;
	Width = ScreenRect.Right - ScreenRect.Left;
	Height = ScreenRect.Bottom - ScreenRect.Top;

	return true;
}

bool FOpenHarmonyWindow::IsPortraitOrientation()
{
	return GOpenHarmonyIsPortrait;
}

FVector4 FOpenHarmonyWindow::GetSafezone(bool bPortrait)
{
	return bPortrait ? GOpenHarmonyPortraitSafezone : GOpenHarmonyLandscapeSafezone;
}

void FOpenHarmonyWindow::InvalidateCachedScreenRect()
{
	ClearCachedWindowRects();
}

void FOpenHarmonyWindow::AcquireWindowRef(void* InWindow)
{

}

void FOpenHarmonyWindow::ReleaseWindowRef(void* InWindow)
{
	OH_NativeWindow_DestroyNativeWindow((OHNativeWindow*)InWindow);
}

void FOpenHarmonyWindow::SetHardwareWindow_EventThread(void* InWindow)
{
#if USE_OPENHARMONY_EVENTS
	check(IsInOpenHarmonyEventThread());
#endif
	NativeWindow = InWindow; //using raw native window handle for now. Could be changed to use AndroidWindow later if needed
}

void* FOpenHarmonyWindow::GetHardwareWindow_EventThread()
{
	return NativeWindow;
}

bool FOpenHarmonyWindow::WaitForWindowDimensions()
{
	while (!bAreCachedNativeDimensionsValid)
	{
		if (IsEngineExitRequested()
#if USE_OPENHARMONY_EVENTS
		|| FAppEventManager::GetInstance()->WaitForEventInQueue(EAppEventState::APP_EVENT_STATE_ON_DESTROY, 0.0f)
#endif
			)
		{
			// Application is shutting down
			return false;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	return true;
}

// To be called during initialization from the event thread.
// once set the dimensions are 'valid' and further changes are updated via FAppEventManager::Tick 
void FOpenHarmonyWindow::SetWindowDimensions_EventThread(void* DimensionWindow)
{
	if(bAreCachedNativeDimensionsValid == false)
	{
		OHNativeWindow* nativeWindow = static_cast<OHNativeWindow*>(DimensionWindow);
		OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, GET_BUFFER_GEOMETRY, &CachedNativeWindowHeight, &CachedNativeWindowWidth);
		
		FPlatformMisc::MemoryBarrier();
		bAreCachedNativeDimensionsValid = true;
	}
}

void FOpenHarmonyWindow::EventManagerUpdateWindowDimensions(int32 Width, int32 Height)
{
	check(bAreCachedNativeDimensionsValid);
	check(Width >= 0 && Height >= 0);

	bool bChanged = CachedNativeWindowWidth != Width || CachedNativeWindowHeight != Height;

	CachedNativeWindowWidth = Width;
	CachedNativeWindowHeight = Height;

	if (bChanged)
	{
		InvalidateCachedScreenRect();
	}
}

bool FOpenHarmonyWindow::GetNativeWindowResolution(int32_t& OutWidth, int32_t& OutHeight)
{
	if (NativeWindow == nullptr) return false;

	OHNativeWindow* nativeWindow = static_cast<OHNativeWindow*>(NativeWindow);
	OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, GET_BUFFER_GEOMETRY, &OutWidth, &OutHeight);

	return true;
}

void* FOpenHarmonyWindow::WaitForHardwareWindow()
{
	// Sleep if the hardware window isn't currently available.
	// The Window may not exist if the activity is pausing/resuming, in which case we make this thread wait
	// This case will come up frequently as a result of the DON flow in Gvr.
	// Until the app is fully resumed. It would be nicer if this code respected the lifecycle events
	// of an android app instead, but all of those events are handled on a separate thread and it would require
	// significant re-architecturing to do.

	// Before sleeping, we peek into the event manager queue to see if it contains an ON_DESTROY event, 
	// in which case, we exit the loop to allow the application to exit before a window has been created.
	// For instance when the user aborts the "Place your phone into thr Daydream headset." screen.
	// It is not sufficient to check the IsEngineExitRequested() global function, as the handler reacting to the APP_EVENT_STATE_ON_DESTROY
	// may be running in the same thread as this method and therefore lead to a deadlock.

	void* WindowEventThread = GetHardwareWindow_EventThread();
	while (WindowEventThread == nullptr)
	{
#if USE_OPENHARMONY_EVENTS
		if (IsEngineExitRequested() || FAppEventManager::GetInstance()->WaitForEventInQueue(EAppEventState::APP_EVENT_STATE_ON_DESTROY, 0.0f))
		{
			// Application is shutting down soon, abort the wait and return nullptr
			return nullptr;
		}
#endif
		FPlatformProcess::Sleep(0.001f);
		WindowEventThread = GetHardwareWindow_EventThread();
	}
	return WindowEventThread;
}

void FOpenHarmonyWindow::SetWindowSize(int width,int height)
{
	CachedNativeWindowWidth = width;
	CachedNativeWindowHeight = height;
}

bool FOpenHarmonyWindow::IsCachedRectValid(bool bUseEventThreadWindow, const float RequestedContentScaleFactor)
{
	const FCachedWindowRect& CachedRect = CachedWindowRect;

	if (!CachedRect.WindowInit)
	{
		return false;
	}

	bool bValidCache = true;

	if (CachedRect.ContentScaleFactor != RequestedContentScaleFactor )
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** RequestedContentScaleFactor different %f != %f, not using res cache (%d)"), RequestedContentScaleFactor, CachedWindowRect.ContentScaleFactor, (int32)bUseEventThreadWindow);
		bValidCache = false;
	}

	if (CachedRect.WindowWidth <= 8)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** WindowWidth is %d, not using res cache (%d)"), CachedRect.WindowWidth, (int32)bUseEventThreadWindow);
		bValidCache = false;
	}

	return bValidCache;
}

void FOpenHarmonyWindow::CacheRect(bool bUseEventThreadWindow, const int32 Width, const int32 Height, const float RequestedContentScaleFactor)
{
	FCachedWindowRect& CachedRect = CachedWindowRect;

	CachedRect.WindowWidth = Width;
	CachedRect.WindowHeight = Height;
	CachedRect.WindowInit = true;
	CachedRect.ContentScaleFactor = RequestedContentScaleFactor;
}
#if USE_OPENHARMONY_NAPI
void Napi_nativeSetWindowInfo(bool bIsPortrait)
{
	ClearCachedWindowRects();
	GOpenHarmonyIsPortrait = bIsPortrait;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("App is running in %s\n"), GOpenHarmonyIsPortrait ? TEXT("Portrait") : TEXT("Landscape"));
}

JSBIND_GLOBAL() {
	JSBIND_FUNCTION(Napi_nativeSetWindowInfo);
}
#endif

FPlatformRect FOpenHarmonyWindow::GetScreenRect([[maybe_unused]]bool bUseEventThreadWindow)
{
	// CSF is a multiplier to 1280x720
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
	// If the app is for Oculus Mobile then always use 0 as ScaleFactor (to match window size).
	float RequestedContentScaleFactor = CVar->GetFloat();

	FString CmdLineCSF;
	if (FParse::Value(FCommandLine::Get(), TEXT("mcsf="), CmdLineCSF, false))
	{
		RequestedContentScaleFactor = FCString::Atof(*CmdLineCSF);
	}

	bool bComputeRect = !IsCachedRectValid(bUseEventThreadWindow, RequestedContentScaleFactor);
	if (bComputeRect)
	{
		// get the aspect ratio of the physical screen
		int32 ScreenWidth, ScreenHeight;
		CalculateSurfaceSize(ScreenWidth, ScreenHeight, bUseEventThreadWindow);
	
		OpenHarmonyWindowUtils::ApplyContentScaleFactor(ScreenWidth, ScreenHeight);

		CacheRect(bUseEventThreadWindow, ScreenWidth, ScreenHeight, RequestedContentScaleFactor);
	}

	const FCachedWindowRect& CachedRect = CachedWindowRect;
	
	FPlatformRect ScreenRect;
	ScreenRect.Left = 0;
	ScreenRect.Top = 0;
	ScreenRect.Right = CachedRect.WindowWidth;
	ScreenRect.Bottom = CachedRect.WindowHeight;

	return ScreenRect;
}

void FOpenHarmonyWindow::CalculateSurfaceSize(int32_t& SurfaceWidth, int32_t& SurfaceHeight, bool bUseEventThreadWindow)
{
	FOpenHarmonyWindow::WaitForWindowDimensions();

	SurfaceWidth = CachedNativeWindowWidth;
	SurfaceHeight = CachedNativeWindowHeight;
}

//设置安全区域
void FOpenHarmonyWindow::SetSafezoneInfo(bool bIsPortrait, float left, float top, float right, float bottom)
{
	if (bIsPortrait)
	{
		GOpenHarmonyPortraitSafezone.X = left;
		GOpenHarmonyPortraitSafezone.Y = top;
		GOpenHarmonyPortraitSafezone.Z = right;
		GOpenHarmonyPortraitSafezone.W = bottom;
	}
	else
	{
		GOpenHarmonyLandscapeSafezone.X = left;
		GOpenHarmonyLandscapeSafezone.Y = top;
		GOpenHarmonyLandscapeSafezone.Z = right;
		GOpenHarmonyLandscapeSafezone.W = bottom;
	}
	GOpenHarmonySafezoneRequiresUpdate = true;
}

bool FOpenHarmonyWindow::OnWindowOrientationChanged(bool bIsPortrait)
{
	if (GOpenHarmonyIsPortrait != bIsPortrait)
	{
		GOpenHarmonyIsPortrait = bIsPortrait;
		return true;
	}
	return false;
}

// 每次旋转或折叠/展开时，Surface的size会重置为分辨率大小，绘制的内容在RHI线程更新size之前都无法填满屏幕，出现闪烁现象。
// 所以每次旋转后手动更新一下Size，保证绘制内容可填满屏幕不出现闪烁。
void FOpenHarmonyWindow::UpdateWindowSizeWhenSurfaceReset(int32_t width, int32_t height)
{
	if (!NativeWindow) {
		return;
	}
	OpenHarmonyWindowUtils::ApplyContentScaleFactor(width, height);
	OH_NativeWindow_NativeWindowHandleOpt((OHNativeWindow*)NativeWindow, SET_BUFFER_GEOMETRY, width, height);
}

bool FOpenHarmonyWindow::SafezoneUpdated()
{
	bool bRequiresUpdate = true;
	return GOpenHarmonySafezoneRequiresUpdate.compare_exchange_weak(bRequiresUpdate, false);
}