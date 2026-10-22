#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/Optional.h"
#include "HAL/PlatformProcess.h"
#include "OpenHarmonyTargetDeviceOutput.h"

class FOpenHarmonyTargetDevice;
struct FTargetDeviceProcessInfo;
enum class ETargetDeviceFeatures;

inline FString FOpenHarmonyTargetDevice::GetOperatingSystemName()
{
	if (!OpenHarmonyVersionString.IsEmpty())
	{
		return FString::Printf(TEXT("OpenHarmony %s, API level %d"), *OpenHarmonyVersionString, OpenHarmonySDKVersion);
	}
	else
	{
		return TEXT("OpenHarmony");
	}
}

inline int32 FOpenHarmonyTargetDevice::GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) 
{
	return 0;
}


inline bool FOpenHarmonyTargetDevice::Reboot( bool bReconnect )
{
	if (!ExecuteHdcCommand(TEXT("reboot"), NULL, NULL))
	{
		return false;
	}

	return true;
}

inline bool FOpenHarmonyTargetDevice::PowerOff( bool Force )
{
	if (!ExecuteHdcCommand(TEXT("reboot --poweroff"), NULL, NULL))
	{
		return false;
	}

	return true;
}

inline FString FOpenHarmonyTargetDevice::GetAllDevicesName() const
{
	return FString::Printf(TEXT("All_%s_On_%s"), *(GetPlatformSettings().IniPlatformName()), FPlatformProcess::ComputerName());
}

// cancel the running application
inline bool FOpenHarmonyTargetDevice::TerminateLaunchedProcess(const FString& ProcessIdentifier)
{
	FString HdcCommand = FString::Printf(TEXT("shell am force-stop '%s' -s %s"), *ProcessIdentifier, *SerialNumber);
	return ExecuteHdcCommand(HdcCommand, nullptr, nullptr);
}

inline bool FOpenHarmonyTargetDevice::SupportsFeature( ETargetDeviceFeatures Feature ) const
{
	switch (Feature)
	{
	case ETargetDeviceFeatures::PowerOff:
		return true;

	case ETargetDeviceFeatures::PowerOn:
		return false;

	case ETargetDeviceFeatures::Reboot:
		return true;

	default:
		return false;
	}
}


inline bool FOpenHarmonyTargetDevice::TerminateProcess( const int64 ProcessId )
{
	return false;
}


inline void FOpenHarmonyTargetDevice::SetUserCredentials( const FString& UserName, const FString& UserPassword )
{
}


inline bool FOpenHarmonyTargetDevice::GetUserCredentials( FString& OutUserName, FString& OutUserPassword )
{
	return false;
}

inline void FOpenHarmonyTargetDevice::ExecuteConsoleCommand(const FString& ExecCommand) const
{
	FString HdcCommand = FString::Printf(TEXT("shell \"am broadcast -a openharmony.intent.action.RUN -e cmd '%s'\""), *ExecCommand);
	ExecuteHdcCommand(HdcCommand, nullptr, nullptr);
}

inline ITargetDeviceOutputPtr FOpenHarmonyTargetDevice::CreateDeviceOutputRouter(FOutputDevice* Output) const
{
	FOpenHarmonyTargetDeviceOutputPtr DeviceOutputPtr = MakeShareable(new FOpenHarmonyTargetDeviceOutput());
	if (DeviceOutputPtr->Init(*this, Output))
	{
		return DeviceOutputPtr;
	}

	return nullptr;
}

/* FOpenHarmonyTargetDevice implementation
 *****************************************************************************/

inline bool FOpenHarmonyTargetDevice::GetHdcFullFilename(FString& OutFilename)
{
	TOptional<FString> ResultPath;

	// get the SDK binaries folder
	FString OpenHarmonyDirectory = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENHARMONY_HOME"));
	if (OpenHarmonyDirectory.Len() == 0)
	{
		return false;
	}

#if PLATFORM_WINDOWS
	OutFilename = FString::Printf(TEXT("%s\\base\\toolchains\\hdc.exe"), *OpenHarmonyDirectory);
#else
	OutFilename = FString::Printf(TEXT("%s/base/toolchains/hdc"), *OpenHarmonyDirectory);
#endif

	return true;
}

inline bool FOpenHarmonyTargetDevice::ExecuteHdcCommand( const FString& CommandLine, FString* OutStdOut, FString* OutStdErr ) const
{
	FString Filename;
	if (!GetHdcFullFilename(Filename))
	{
		return false;
	}
	
	// execute the command
	int32 ReturnCode;
	FString DefaultError;

	// make sure there's a place for error output to go if the caller specified NULL
	if (!OutStdErr)
	{
		OutStdErr = &DefaultError;
	}

	FString CommandLineWithDevice;
	// the devices command should never include a specific device
	if (CommandLine == TEXT("devices"))
	{
		CommandLineWithDevice = CommandLine;
	}
	else
	{
		CommandLineWithDevice = FString::Printf(TEXT("-t %s %s"), *SerialNumber, *CommandLine);
	}

	FPlatformProcess::ExecProcess(*Filename, *CommandLineWithDevice, &ReturnCode, OutStdOut, OutStdErr);

	if (ReturnCode != 0)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("The OpenHarmony SDK command '%s' failed to run. Return code: %d, Error: %s\n"), *CommandLineWithDevice, ReturnCode, **OutStdErr);

		return false;
	}

	return true;
}
