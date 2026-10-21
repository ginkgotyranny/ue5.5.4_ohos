#include "OpenHarmony/OpenHarmonyApplication.h"
#include "OpenHarmony/OpenHarmonyInputInterface.h"
#include "OpenHarmony/OpenHarmonyWindow.h"
#include "OpenHarmony/OpenHarmonyCursor.h"
#include "IInputDeviceModule.h"
#include "HAL/OutputDevices.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CoreDelegates.h"

extern int GetNativeDeviceOrientation();

DEFINE_LOG_CATEGORY_STATIC(LogOpenHarmonyApplication, Log, All);

bool FOpenHarmonyApplication::bWindowSizeChanged = false;

FOpenHarmonyApplication* FOpenHarmonyApplication::_application = nullptr;

FOpenHarmonyApplication* FOpenHarmonyApplication::CreateOpenHarmonyApplication()
{
	return new FOpenHarmonyApplication();
}

FOpenHarmonyApplication::FOpenHarmonyApplication()
	: GenericApplication(MakeShareable(new FOpenHarmonyCursor()))
	, InputInterface( FOpenHarmonyInputInterface::Create( MessageHandler, Cursor ) )
	, bHasLoadedInputPlugins(false)
{
	_application = this;
}

FOpenHarmonyApplication::FOpenHarmonyApplication(TSharedPtr<class FOpenHarmonyInputInterface> InInputInterface)
	: GenericApplication((InInputInterface.IsValid() && InInputInterface->GetCursor()) ? InInputInterface->GetCursor() : MakeShareable(new FOpenHarmonyCursor()))
	, InputInterface(InInputInterface)
	, bHasLoadedInputPlugins(false)
{
	_application = this;
}

void FOpenHarmonyApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}

void FOpenHarmonyApplication::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		InputInterface->AddExternalInputDevice(InputDevice);
	}
}

void FOpenHarmonyApplication::PollGameDeviceState( const float TimeDelta )
{
	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins && GIsRunning)
	{
		TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
		for (auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt)
		{
			TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
			AddExternalInputDevice(Device);
		}

		bHasLoadedInputPlugins = true;
	}

	// Poll game device state and send new events
	InputInterface->Tick( TimeDelta );
	InputInterface->SendControllerEvents();
	
	// Note: We're using window size values cached on GT. We dont wait for a valid HW window.
	if (bWindowSizeChanged && 
		Windows.Num() > 0)
	{
		int32 WindowX,WindowY, WindowWidth,WindowHeight;
		Windows[0]->GetFullScreenInfo(WindowX, WindowY, WindowWidth, WindowHeight);

		GenericApplication::GetMessageHandler()->OnSizeChanged(Windows[0],WindowWidth,WindowHeight, false);
		GenericApplication::GetMessageHandler()->OnResizingWindow(Windows[0]);
		
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
		BroadcastDisplayMetricsChanged(DisplayMetrics);

		// the cursor needs to compute the proper slate scaling factor each time the display metrics change
		TSharedPtr<FOpenHarmonyCursor> OpenHarmonyCursor = StaticCastSharedPtr<FOpenHarmonyCursor>(Cursor);
		if (OpenHarmonyCursor.IsValid())
		{
			OpenHarmonyCursor->ComputeUIScaleFactor();
		}

		bWindowSizeChanged = false;
	}
    
    HandleDeviceOrientation();
}

void FOpenHarmonyApplication::HandleDeviceOrientation()
{
#if USE_OPENHARMONY_NAPI
	int curOrientation = FOpenHarmonyMisc::TmpDeviceOrientation;
	const auto PreviousDeviceOrientation = DeviceOrientation;
	switch (curOrientation)
	{
		//在鸿蒙OS中，横屏是指充电口朝左，反向横屏是指充电口朝右；与安卓相反
	case 0: DeviceOrientation = EDeviceScreenOrientation::Portrait;             break;
	case 1: DeviceOrientation = EDeviceScreenOrientation::LandscapeRight;        break;
	case 2: DeviceOrientation = EDeviceScreenOrientation::PortraitUpsideDown;   break;
	case 3: DeviceOrientation = EDeviceScreenOrientation::LandscapeLeft;       break;
	}

	FOpenHarmonyMisc::SaveDeviceOrientation(DeviceOrientation);
	if (PreviousDeviceOrientation != DeviceOrientation)
	{
		FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.Broadcast((int32)DeviceOrientation);
	}
	if (FOpenHarmonyWindow::SafezoneUpdated())
	{
		//we also want to fire off the safe frame event
		FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
	}
#endif
}

FPlatformRect FOpenHarmonyApplication::GetWorkArea(const FPlatformRect& CurrentWindow) const
{
	return FOpenHarmonyWindow::GetScreenRect();
}

IInputInterface* FOpenHarmonyApplication::GetInputInterface()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

void FOpenHarmonyApplication::Tick(const float TimeDelta)
{
	//generate event that will end up calling 'QueryCursor' in slate to support proper reporting of the cursor's type.
	MessageHandler->OnCursorSet();
}

bool FOpenHarmonyApplication::IsGamepadAttached() const
{
	FOpenHarmonyInputInterface* OpenHarmonyInputInterface = (FOpenHarmonyInputInterface*)InputInterface.Get();

	if (OpenHarmonyInputInterface)
	{
		return OpenHarmonyInputInterface->IsGamepadAttached();
	}

	return false;
}

bool FOpenHarmonyApplication::GetNativeWindowResolution(int32_t& OutWidth, int32_t& OutHeight) const
{
	if (Windows.IsEmpty()) return false;

	const TSharedPtr<FOpenHarmonyWindow>& Window = Windows[0];

	if (!Window.IsValid()) return false;

	return Window->GetNativeWindowResolution(OutWidth, OutHeight);
}

void FDisplayMetrics::RebuildDisplayMetrics( FDisplayMetrics& OutDisplayMetrics )
{
	// Get screen rect
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FOpenHarmonyWindow::GetScreenRect();
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;

	// Total screen size of the primary monitor
	OutDisplayMetrics.PrimaryDisplayWidth = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left;
	OutDisplayMetrics.PrimaryDisplayHeight = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top;

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();

	bool bIsPortrait = FOpenHarmonyWindow::IsPortraitOrientation();
	FVector4 SafeZoneRect = FOpenHarmonyWindow::GetSafezone(bIsPortrait);

	// ConfigRules values override values from device
	if (FString* SafeZoneVar = FOpenHarmonyMisc::GetConfigRulesVariable(bIsPortrait ? TEXT("SafeZone_Portrait") : TEXT("SafeZone_Landscape")))
	{
		TArray<FString> ZoneVector;
		ZoneVector.Reserve(4);

		int ZoneParseCount = SafeZoneVar->ParseIntoArray(ZoneVector, TEXT(","), true);
		ensureMsgf(ZoneParseCount == 4, TEXT("SafeZone variable not properly formatted."));

		if (ZoneParseCount == 4)
		{
			SafeZoneRect = FVector4(FCString::Atof(*ZoneVector[0]), FCString::Atof(*ZoneVector[1]), FCString::Atof(*ZoneVector[2]), FCString::Atof(*ZoneVector[3]));
		}
	}

	const double Inset_Left = SafeZoneRect.X > 0 ? SafeZoneRect.X * OutDisplayMetrics.PrimaryDisplayWidth : -1.0f;
	const double Inset_Top = SafeZoneRect.Y > 0 ? SafeZoneRect.Y * OutDisplayMetrics.PrimaryDisplayHeight : -1.0f;
	const double Inset_Right =  SafeZoneRect.Z > 0 ? SafeZoneRect.Z * OutDisplayMetrics.PrimaryDisplayWidth : -1.0f;
	const double Inset_Bottom = SafeZoneRect.W > 0 ? SafeZoneRect.W * OutDisplayMetrics.PrimaryDisplayHeight : -1.0f;

	OutDisplayMetrics.TitleSafePaddingSize.X = (Inset_Left >= 0.0f) ? Inset_Left : OutDisplayMetrics.TitleSafePaddingSize.X;
	OutDisplayMetrics.TitleSafePaddingSize.Y = (Inset_Top >= 0.0f) ? Inset_Top : OutDisplayMetrics.TitleSafePaddingSize.Y;
	OutDisplayMetrics.TitleSafePaddingSize.Z = (Inset_Right >= 0.0f) ? Inset_Right : OutDisplayMetrics.TitleSafePaddingSize.Z;
	OutDisplayMetrics.TitleSafePaddingSize.W = (Inset_Bottom >= 0.0f) ? Inset_Bottom : OutDisplayMetrics.TitleSafePaddingSize.W;
	OutDisplayMetrics.ActionSafePaddingSize = OutDisplayMetrics.TitleSafePaddingSize;
}

TSharedRef< FGenericWindow > FOpenHarmonyApplication::MakeWindow()
{
	return FOpenHarmonyWindow::Make();
}

void FOpenHarmonyApplication::InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately )
{
	const TSharedRef< FOpenHarmonyWindow > Window = StaticCastSharedRef< FOpenHarmonyWindow >( InWindow );
	const TSharedPtr< FOpenHarmonyWindow > ParentWindow = StaticCastSharedPtr< FOpenHarmonyWindow >( InParent );

	Windows.Add( Window );
	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
}

void FOpenHarmonyApplication::OnWindowSizeChanged()
{
	bWindowSizeChanged = true;
}
