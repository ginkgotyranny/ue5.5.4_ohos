#pragma once

#include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/GenericApplication.h"
#include "native_window/external_window.h"

/**
 * A platform specific implementation of FNativeWindow.
 * Native windows provide platform-specific backing for and are always owned by an SWindow.
 */
 
class APPLICATIONCORE_API FOpenHarmonyWindow : public FGenericWindow
{
public:
	~FOpenHarmonyWindow();

	/** Create a new FOpenHarmonyWindow.
	 *
	 * @param OwnerWindow		The SlateWindow for which we are crating a backing AndroidWindow
	 * @param InParent			Parent iOS window; usually NULL.
	 */
	static TSharedRef<FOpenHarmonyWindow> Make();
	virtual void* GetOSWindowHandle() const override { return nullptr; }

	void Initialize( class FOpenHarmonyApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FOpenHarmonyWindow >& InParent, const bool bShowImmediately );

	/** Returns the rectangle of the screen the window is associated with */
	virtual bool GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const override;

	static FPlatformRect GetScreenRect(bool bUseEventThreadWindow = false);
	static void InvalidateCachedScreenRect();

	static void CalculateSurfaceSize(int32_t& SurfaceWidth, int32_t& SurfaceHeight, bool bUseEventThreadWindow = false);
	static bool OnWindowOrientationChanged(bool bIsPortrait);
	static void UpdateWindowSizeWhenSurfaceReset(int32_t width, int32_t height);

	static void AcquireWindowRef(void* InWindow);
	static void ReleaseWindowRef(void* InWindow);

	static void SetHardwareWindow_EventThread(void* InWindow);
	static void SetWindowSize(int width,int height);
	static void* GetHardwareWindow_EventThread();

	/** Waits on the current thread for a hardware window and returns it.
	 *  May return nullptr if the application is shutting down.
	 */
	static void* WaitForHardwareWindow();

	static bool IsPortraitOrientation();
	static FVector4 GetSafezone(bool bPortrait);

	// called by the Android event thread to initially set the current window dimensions.
	static void SetWindowDimensions_EventThread(void* DimensionWindow);

	static void EventManagerUpdateWindowDimensions(int32 Width, int32 Height);

	static void SetSafezoneInfo(bool bIsPortrait, float left, float top, float right, float bottom);
	
	static bool SafezoneUpdated();
	static bool GetNativeWindowResolution(int32_t& OutWidth, int32_t& OutHeight);
	
protected:
	/** @return true if the native window is currently in fullscreen mode, false otherwise */
	virtual EWindowMode::Type GetWindowMode() const override { return EWindowMode::Fullscreen; }
	
private:
	/** called from GetScreenRect function */
	/** test cached values from the latest computations stored by CacheRect to decide their validity with the provided arguments */
	static bool IsCachedRectValid(bool bUseEventThreadWindow, const float RequestedContentScaleFactor);
	/** caches some values used to compute the size of the window by GetScreenRect function */
	static void CacheRect(bool bUseEventThreadWindow, const int32 Width, const int32 Height, const float RequestedContentScaleFactor);
	
	/**
	 * Protect the constructor; only TSharedRefs of this class can be made.
	 */
	FOpenHarmonyWindow();

	FOpenHarmonyApplication* OwningApplication;
    	
	static void* NativeWindow;

	// Waits for the event thread to report an initial window size.
	static bool WaitForWindowDimensions();

	static bool bAreCachedNativeDimensionsValid;
	static int32 CachedNativeWindowWidth;
	static int32 CachedNativeWindowHeight;
};
