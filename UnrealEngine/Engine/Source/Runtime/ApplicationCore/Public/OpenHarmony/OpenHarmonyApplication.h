#pragma once

#include "GenericPlatform/GenericApplication.h"
#include "OpenHarmonyWindow.h"

namespace FOpenHarmonyAppEntry
{
	void PlatformInit();

	// if the native window handle has changed then the new handle is required.
	void ReInitWindow(void* NewNativeWindowHandle = nullptr);

	void ReleaseEGL();
	void OnPauseEvent();
}

struct FPlatformOpenGLContext;
namespace FOpenHarmonyEGL
{
	// Back door into more intimate Android OpenGL variables (a.k.a. a hack)
	FPlatformOpenGLContext*	GetRenderingContext();
	FPlatformOpenGLContext*	CreateContext();
	void					MakeCurrent(FPlatformOpenGLContext*);
	void					ReleaseContext(FPlatformOpenGLContext*);
	void					SwapBuffers(FPlatformOpenGLContext*);
	void					SetFlipsEnabled(bool Enabled);
	void					BindDisplayToContext(FPlatformOpenGLContext*);
}

//disable warnings from overriding the deprecated forcefeedback.  
//calls to the deprecated function will still generate warnings.
PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FOpenHarmonyApplication : public GenericApplication
{
public:

	static FOpenHarmonyApplication* CreateOpenHarmonyApplication();


	static FOpenHarmonyApplication* Get() { return _application; }

public:	
	
	virtual ~FOpenHarmonyApplication() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;

	virtual IInputInterface* GetInputInterface() override;

	virtual TSharedRef< FGenericWindow > MakeWindow() override;

	virtual void AddExternalInputDevice(TSharedPtr<class IInputDevice> InputDevice);
	
	void InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately );

	static void OnWindowSizeChanged();

	virtual void Tick(const float TimeDelta) override;

	virtual bool IsGamepadAttached() const override;

	bool GetNativeWindowResolution(int32_t& OutWidth, int32_t& OutHeight) const;

protected:

	FOpenHarmonyApplication();
	FOpenHarmonyApplication(TSharedPtr<class FOpenHarmonyInputInterface> InInputInterface);


private:

	TSharedPtr< class FOpenHarmonyInputInterface > InputInterface;
	bool bHasLoadedInputPlugins;

	TArray< TSharedRef< FOpenHarmonyWindow > > Windows;

	static bool bWindowSizeChanged;

	static FOpenHarmonyApplication* _application;

    EDeviceScreenOrientation DeviceOrientation;
    void HandleDeviceOrientation();
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
