#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "Containers/Map.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IOpenHarmonyDeviceDetection.h"
#include "Interfaces/IOpenHarmonyDeviceDetectionModule.h"
#include "ITcpMessagingModule.h"

#include "PIEPreviewDeviceSpecification.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Internationalization/Regex.h"

#if WITH_EDITOR
#include "PIEPreviewDeviceProfileSelectorModule.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#endif
#include "String/ParseLines.h"

#define LOCTEXT_NAMESPACE "FOpenHarmonyDeviceDetectionModule" 

DEFINE_LOG_CATEGORY_STATIC(OpenHarmonyDeviceDetectionLog, Log, All);

static int32 GOpenHarmonyDeviceDetectionPollInterval = 10;
static FAutoConsoleVariableRef CVarOpenHarmonyDeviceDetectionPollInterval(
	TEXT("OpenHarmony.DeviceDetectionPollInterval"),
	GOpenHarmonyDeviceDetectionPollInterval,
	TEXT("The number of seconds between polling for connected OpenHarmony devices.\n")
	TEXT("Default: 10"),
	ECVF_Default
);

class FOpenHarmonyDeviceDetectionRunnable : public FRunnable
{
public:
	FOpenHarmonyDeviceDetectionRunnable(TMap<FString,FOpenHarmonyDeviceInfo>& InDeviceMap, FCriticalSection* InDeviceMapLock, FCriticalSection* InHDCPathCheckLock) :
		StopTaskCounter(0),
		DeviceMap(InDeviceMap),
		DeviceMapLock(InDeviceMapLock),
		HDCPathCheckLock(InHDCPathCheckLock),
		HasHDCPath(false),
		ForceCheck(false)
	{
		TcpMessagingModule = FModuleManager::LoadModulePtr<ITcpMessagingModule>("TcpMessaging");
	}

public:

	// FRunnable interface.
	virtual bool Init(void) 
	{ 
		return true; 
	}

	virtual void Exit(void) 
	{
	}

	virtual void Stop(void)
	{
		StopTaskCounter.Increment();
	}

	virtual uint32 Run(void)
	{
		int LoopCount = 10;

		while (StopTaskCounter.GetValue() == 0)
		{
			// query when we have waited 'GOpenHarmonyDeviceDetectionPollInterval' seconds.
			if (LoopCount++ >= GOpenHarmonyDeviceDetectionPollInterval || ForceCheck)
			{
				// Make sure we have an HDC path before checking
				FScopeLock PathLock(HDCPathCheckLock);
				if (HasHDCPath)
					QueryConnectedDevices();

				LoopCount = 0;
				ForceCheck = false;
			}

			FPlatformProcess::Sleep(1.0f);
		}

		return 0;
	}

	void UpdateHDCPath(FString &InHDCPath, FString& InGetPropCommand, bool InbGetExtensionsViaSurfaceFlinger)
	{
		HDCPath = InHDCPath;
		GetPropCommand = InGetPropCommand;
		bGetExtensionsViaSurfaceFlinger = InbGetExtensionsViaSurfaceFlinger;

		HasHDCPath = !HDCPath.IsEmpty();
		// Force a check next time we go around otherwise it can take over 10sec to find devices
		ForceCheck = HasHDCPath;	

		// If we have no path then clean the existing devices out
		if (!HasHDCPath && DeviceMap.Num() > 0)
		{
			DeviceMap.Reset();
		}
	}

private:

	bool ExecuteHdcCommand( const FString& CommandLine, FString* OutStdOut, FString* OutStdErr ) const
	{
		// execute the command
		int32 ReturnCode;
		FString DefaultError;

		// make sure there's a place for error output to go if the caller specified nullptr
		if (!OutStdErr)
		{
			OutStdErr = &DefaultError;
		}

		if (FPaths::FileExists(HDCPath))
		{
			FPlatformProcess::ExecProcess(*HDCPath, *CommandLine, &ReturnCode, OutStdOut, OutStdErr);

			if (ReturnCode != 0)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("The OpenHarmony SDK command '%s' failed to run. Return code: %d, Error: %s\n"), *CommandLine, ReturnCode, **OutStdErr);

				return false;
			}
		}

		return true;
	}

	// searches for 'DPIString' and 
	int32 ExtractDPI(const FString& SurfaceFlingerOutput, const FString& DPIString)
	{
		int32 FoundDpi = INDEX_NONE;

		int32 DpiIndex = SurfaceFlingerOutput.Find(DPIString);
		if (DpiIndex != INDEX_NONE)
		{
			int32 StartIndex = INDEX_NONE;
			for (int32 i = DpiIndex; i < SurfaceFlingerOutput.Len(); ++i)
			{
				// if we somehow hit a line break character something went wrong and no digits were found on this line
				// we don't want to search the SurfaceFlinger feed so exit now
				if (FChar::IsLinebreak(SurfaceFlingerOutput[i]))
				{
					break;
				}

				// search for the first digit aka the beginning of the DPI value
				if (StartIndex == INDEX_NONE && FChar::IsDigit(SurfaceFlingerOutput[i]))
				{
					StartIndex = i;
				}
				// if we hit some non-numeric character extract the number and exit
				else if (StartIndex != INDEX_NONE && !FChar::IsDigit(SurfaceFlingerOutput[i]))
				{
					FString str = SurfaceFlingerOutput.Mid(StartIndex, i - StartIndex);
					FoundDpi = FCString::Atoi(*str);
					break;
				}
			}
		}

		return FoundDpi;
	}
	// retrieve the string between 'InOutStartIndex' and the start position of the next 'Token' substring
	// the white spaces of the resulting string are trimmed out at both ends 
	FString ExtractNextToken(int32& InOutStartIndex, const FString& SurfaceFlingerOutput, const FString& Token)
	{
		FString OutString;
		int32 StartIndex = InOutStartIndex;

		int32 EndIndex = SurfaceFlingerOutput.Find(Token, ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);

		if (EndIndex != INDEX_NONE)
		{
			InOutStartIndex = EndIndex + 1;
			// the index should point to the position before the token start
			--EndIndex;

			for (int32 i = StartIndex; i < EndIndex; ++i)
			{
				if (!FChar::IsWhitespace(SurfaceFlingerOutput[i]))
				{
					StartIndex = i;
					break;
				}
			}

			for (int32 i = EndIndex; i > StartIndex; --i)
			{
				if (!FChar::IsWhitespace(SurfaceFlingerOutput[i]))
				{
					EndIndex = i;
					break;
				}
			}

			OutString = SurfaceFlingerOutput.Mid(StartIndex, FMath::Max(0, EndIndex - StartIndex + 1));
		}

		return OutString;
	}

	void ExtractGPUInfo(FString& outGLVersion, FString& outGPUFamily, const FString& SurfaceFlingerOutput)
	{
		int32 FoundDpi = INDEX_NONE;

		int32 LineIndex = SurfaceFlingerOutput.Find(TEXT("GLES:"));
		if (LineIndex != INDEX_NONE)
		{
			int32 StartIndex = SurfaceFlingerOutput.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromStart, LineIndex);
			if (StartIndex != INDEX_NONE)
			{
				++StartIndex;

				FString GPUVendorString = ExtractNextToken(StartIndex, SurfaceFlingerOutput, TEXT(","));
				outGPUFamily = ExtractNextToken(StartIndex, SurfaceFlingerOutput, TEXT(","));
				outGLVersion = ExtractNextToken(StartIndex, SurfaceFlingerOutput, TEXT("\n"));
			}
		}
	}

	void QueryConnectedDevices()
	{
		// grab the list of devices via hdc
		FString StdOut;
		if (!ExecuteHdcCommand(TEXT("list targets"), &StdOut, nullptr))
		{
			return;
		}

		// separate out each line
		TArray<FString> DeviceStrings;
		// StdOut = StdOut.Replace(TEXT("\r"), TEXT("\n"));
		StdOut.ParseIntoArray(DeviceStrings, TEXT("\n"), true);

		// list of any existing port forwardings, filled in when we find a device we need to add.
		TArray<FString> PortForwardings;

		// a list containing all devices found this time, so we can remove anything not in this list
		TArray<FString> CurrentlyConnectedDevices;

		for (int32 StringIndex = 0; StringIndex < DeviceStrings.Num(); ++StringIndex)
		{
			const FString& DeviceString = DeviceStrings[StringIndex];

			// skip over non-device lines
			if (DeviceString.StartsWith("[Empty]"))
			{
				continue;
			}

			/*
			// grab the device serial number
			int32 TabIndex;

			// use either tab or space as separator
			if (!DeviceString.FindChar(TCHAR('\t'), TabIndex))
			{
				if (!DeviceString.FindChar(TCHAR(' '), TabIndex))
				{
					continue;
				}
			}
			*/

			FOpenHarmonyDeviceInfo NewDeviceInfo;

			NewDeviceInfo.SerialNumber = DeviceString;
			/*
			const FString DeviceState = DeviceString.Mid(TabIndex + 1).TrimStart();

			NewDeviceInfo.bAuthorizedDevice = DeviceState != TEXT("unauthorized");
			*/

			// add it to our list of currently connected devices
			CurrentlyConnectedDevices.Add(NewDeviceInfo.SerialNumber);

			// move on to next device if this one is already a known device that has either already been authorized or the authorization
			// status has not changed
			if (DeviceMap.Contains(NewDeviceInfo.SerialNumber) && 
				(DeviceMap[NewDeviceInfo.SerialNumber].bAuthorizedDevice == NewDeviceInfo.bAuthorizedDevice))
			{
				continue;
			}

			if (!NewDeviceInfo.bAuthorizedDevice)
			{
				//note: OpenHarmonyTargetDevice::GetName() does not fetch this value, do not rely on this
				NewDeviceInfo.DeviceName = TEXT("Unauthorized - enable USB debugging");
			}
			else
			{
				// grab the OpenHarmony version
				const FString OpenHarmonyVersionCommand = FString::Printf(TEXT("-t %s %s const.product.software.version"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
				if (!ExecuteHdcCommand(*OpenHarmonyVersionCommand, &NewDeviceInfo.HumanOpenHarmonyVersion, nullptr))
				{
					continue;
				}
				NewDeviceInfo.HumanOpenHarmonyVersion = NewDeviceInfo.HumanOpenHarmonyVersion.Replace(TEXT("\r"), TEXT("")).Replace(TEXT("\n"), TEXT(""));
				NewDeviceInfo.HumanOpenHarmonyVersion.TrimStartAndEndInline();

				// grab the OpenHarmony SDK version
				const FString SDKVersionCommand = FString::Printf(TEXT("-t %s %s const.ohos.apiversion"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
				FString SDKVersionString;
				if (!ExecuteHdcCommand(*SDKVersionCommand, &SDKVersionString, nullptr))
				{
					continue;
				}
				NewDeviceInfo.SDKVersion = FCString::Atoi(*SDKVersionString);
				if (NewDeviceInfo.SDKVersion <= 0)
				{
					NewDeviceInfo.SDKVersion = INDEX_NONE;
				}

				if (bGetExtensionsViaSurfaceFlinger)
				{
					// get the GL extensions string (and a bunch of other stuff)
					const FString ExtensionsCommand = FString::Printf(TEXT("-s %s shell dumpsys SurfaceFlinger"), *NewDeviceInfo.SerialNumber);
					if (!ExecuteHdcCommand(*ExtensionsCommand, &NewDeviceInfo.GLESExtensions, nullptr))
					{
						continue;
					}

					// extract DPI information
					int32 XDpi = ExtractDPI(NewDeviceInfo.GLESExtensions, TEXT("x-dpi"));
					int32 YDpi = ExtractDPI(NewDeviceInfo.GLESExtensions, TEXT("y-dpi"));

					if (XDpi != INDEX_NONE && YDpi != INDEX_NONE)
					{
						NewDeviceInfo.DeviceDPI = (XDpi + YDpi) / 2;
					}

					// extract OpenGL version and GPU family name
					ExtractGPUInfo(NewDeviceInfo.OpenGLVersionString, NewDeviceInfo.GPUFamilyString, NewDeviceInfo.GLESExtensions);
				}

				// grab device brand
				{
					FString ExecCommand = FString::Printf(TEXT("-t %s %s const.product.brand"), *NewDeviceInfo.SerialNumber, *GetPropCommand);

					FString RoProductBrand;
					ExecuteHdcCommand(*ExecCommand, &RoProductBrand, nullptr);
					const TCHAR* Ptr = *RoProductBrand;
					FParse::Line(&Ptr, NewDeviceInfo.DeviceBrand);
				}

				// grab screen resolution
				{
					FString ResolutionString;
					const FString ExecCommand = FString::Printf(TEXT("-s %s shell wm size"), *NewDeviceInfo.SerialNumber);
					if (ExecuteHdcCommand(*ExecCommand, &ResolutionString, nullptr))
					{
						bool bFoundResX = false;
						int32 StartIndex = INDEX_NONE;
						for (int32 Index = 0; Index < ResolutionString.Len(); ++Index)
						{
							if (StartIndex == INDEX_NONE && FChar::IsDigit(ResolutionString[Index]))
							{
								StartIndex = Index;
							}
							else if (StartIndex != INDEX_NONE && !FChar::IsDigit(ResolutionString[Index]))
							{
								FString str = ResolutionString.Mid(StartIndex, Index - StartIndex);

								if (bFoundResX)
								{
									NewDeviceInfo.ResolutionY = FCString::Atoi(*str);
									break;
								}
								else
								{
									NewDeviceInfo.ResolutionX = FCString::Atoi(*str);
									bFoundResX = true;
									StartIndex = INDEX_NONE;
								}
							}
						}
					}
				}

				// grab the GL ES version
				FString GLESVersionString;
				const FString GLVersionCommand = FString::Printf(TEXT("-s %s %s ro.opengles.version"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
				if (!ExecuteHdcCommand(*GLVersionCommand, &GLESVersionString, nullptr))
				{
					continue;
				}
				NewDeviceInfo.GLESVersion = FCString::Atoi(*GLESVersionString);

				// Find the device model
				// FParse::Value(*DeviceString, TEXT("model:"), NewDeviceInfo.Model);
				// find the product model (this must match openharmony.os.build.model)
				FString ModelCommand = FString::Printf(TEXT("-t %s %s const.product.model"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
				FString RoProductModel;
				if( ExecuteHdcCommand(*ModelCommand, &RoProductModel, nullptr) )
				{
					if(!RoProductModel.IsEmpty())
					{
						NewDeviceInfo.Model = RoProductModel.TrimStartAndEnd();
					}
				}
				
				// Find the build ID
				FString BuildNumberString;
				const FString BuildNumberCommand = FString::Printf(TEXT("-s %s %s ro.build.display.id"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
				if (ExecuteHdcCommand(*BuildNumberCommand, &BuildNumberString, nullptr))
				{
					NewDeviceInfo.BuildNumber = BuildNumberString.TrimStartAndEnd();
				}

				// Scan lines looking for ContainsTerm
				auto FindLineContaining = [](const FString& SourceString, const FString& ContainsTerm)
				{
					FString result;
					UE::String::ParseLines(SourceString,
						[&result, &ContainsTerm](const FStringView& Line)
						{
							if (result.IsEmpty() && Line.Contains(ContainsTerm))
							{
								result = Line;
							}
						});

					return result;
				};

				// Parse vulkan version:
				auto MajorVK = [](uint32 Version) { return (((uint32_t)(Version) >> 22) & 0x7FU); };
				auto MinorVK = [](uint32 Version) { return (((uint32_t)(Version) >> 12) & 0x3FFU); };
				auto PatchVK = [](uint32 Version) { return ((uint32_t)(Version) & 0xFFFU); };
				FString FeaturesString;
				const FString FeaturesStringCommand = FString::Printf(TEXT("-s %s shell pm list features"), *NewDeviceInfo.SerialNumber);
				if (ExecuteHdcCommand(*FeaturesStringCommand, &FeaturesString, nullptr))
				{
					FString VulkanVersionLine = FindLineContaining(FeaturesString, TEXT("openharmony.hardware.vulkan.version"));
					const FRegexPattern RegexPattern(TEXT("openharmony\\.hardware\\.vulkan\\.version=(\\d*)"));
					FRegexMatcher RegexMatcher(RegexPattern, *VulkanVersionLine);
					if (RegexMatcher.FindNext())
					{
						uint32 PackedVersion = (uint32)FCString::Atoi64(*RegexMatcher.GetCaptureGroup(1));
						NewDeviceInfo.VulkanVersion = FString::Printf(TEXT("%d.%d.%d"), MajorVK(PackedVersion), MinorVK(PackedVersion), PatchVK(PackedVersion));
					}
				}

				// try vkjson:
				FString VKJsonString;
				const FString VKJsonStringCommand = FString::Printf(TEXT("-s %s shell cmd gpu vkjson"), *NewDeviceInfo.SerialNumber);
				if (ExecuteHdcCommand(*VKJsonStringCommand, &VKJsonString, nullptr))
				{
					FString VulkanVersionLine = FindLineContaining(VKJsonString, TEXT("apiVersion"));

					const FRegexPattern RegexPattern(TEXT("\"apiVersion\"\\s*:\\s*(\\d*)"));
 					FRegexMatcher RegexMatcher(RegexPattern, *VulkanVersionLine);
 					if (RegexMatcher.FindNext())
					{
						FString VulkanVersion = RegexMatcher.GetCaptureGroup(1);
						uint32 PackedVersion = (uint32)FCString::Atoi64(*VulkanVersion);
						if(PackedVersion>0)
						{
							NewDeviceInfo.VulkanVersion = FString::Printf(TEXT("%d.%d.%d"), MajorVK(PackedVersion), MinorVK(PackedVersion), PatchVK(PackedVersion));
						}
					}
				}
				
				if(NewDeviceInfo.VulkanVersion.IsEmpty())
				{
					NewDeviceInfo.VulkanVersion = TEXT("0.0.0");
				}

				// create the hardware field
				FString HardwareCommand = FString::Printf(TEXT("-s %s %s ro.hardware"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
				FString RoHardware;
				{
					ExecuteHdcCommand(*HardwareCommand, &RoHardware, nullptr);
					const TCHAR* Ptr = *RoHardware;
					FParse::Line(&Ptr, NewDeviceInfo.Hardware);
				}
				if (RoHardware.Contains(TEXT("qcom")))
				{
					HardwareCommand = FString::Printf(TEXT("-s %s %s ro.hardware.chipname"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
					ExecuteHdcCommand(*HardwareCommand, &RoHardware, nullptr);
					const TCHAR* Ptr = *RoHardware;
					FParse::Line(&Ptr, NewDeviceInfo.Hardware);
				}
				{
					HardwareCommand = FString::Printf(TEXT("-s %s %s ro.soc.model"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
					FString RoSOCModelIn;
					FString RoSOCModelOut;
					ExecuteHdcCommand(*HardwareCommand, &RoSOCModelIn, nullptr);
					const TCHAR* Ptr = *RoSOCModelIn;
					FParse::Line(&Ptr, RoSOCModelOut);
					if (!RoSOCModelOut.IsEmpty())
					{
						NewDeviceInfo.Hardware = RoSOCModelOut;
					}
				}

				// Read hardware from cpuinfo:
				FString CPUInfoString;
				const FString CPUInfoCommand = FString::Printf(TEXT("-s %s shell cat /proc/cpuinfo"), *NewDeviceInfo.SerialNumber);
				if (ExecuteHdcCommand(*CPUInfoCommand, &CPUInfoString, nullptr))
				{
					FString HardwareLine = FindLineContaining(CPUInfoString, TEXT("Hardware"));

					const FRegexPattern RegexPattern(TEXT("Hardware\\s*:\\s*(.*)"));
					FRegexMatcher RegexMatcher(RegexPattern, *HardwareLine);
					if (RegexMatcher.FindNext())
					{
						NewDeviceInfo.Hardware = RegexMatcher.GetCaptureGroup(1);
					}
				}

				// Total physical mem:
				FString MemTotalString;
				const FString MemTotalCommand = FString::Printf(TEXT("-s %s shell cat /proc/meminfo"), *NewDeviceInfo.SerialNumber);
				if (ExecuteHdcCommand(*MemTotalCommand, &MemTotalString, nullptr))
				{
					FString MemTotalLine = FindLineContaining(MemTotalString, TEXT("MemTotal"));

					const FRegexPattern RegexPattern(TEXT("MemTotal:\\s*(\\d*)"));
					FRegexMatcher RegexMatcher(RegexPattern, *MemTotalLine);
					if (RegexMatcher.FindNext())
					{
						NewDeviceInfo.TotalPhysicalKB = (uint32)FCString::Atoi64(*RegexMatcher.GetCaptureGroup(1));
					}
				}

				// parse the device name
				// FParse::Value(*DeviceString, TEXT("device:"), NewDeviceInfo.DeviceName);
				if (NewDeviceInfo.DeviceName.IsEmpty())
				{
					FString DeviceCommand = FString::Printf(TEXT("-t %s %s const.product.name"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
					FString RoProductDevice;
					ExecuteHdcCommand(*DeviceCommand, &RoProductDevice, nullptr);
					const TCHAR* Ptr = *RoProductDevice;
					FParse::Line(&Ptr, NewDeviceInfo.DeviceName);
				}

				// establish port forwarding if we're doing messaging
				if (TcpMessagingModule != nullptr)
				{
					// fill in the port forwarding array if needed
					if (PortForwardings.Num() == 0)
					{
						FString ForwardList;
						if (ExecuteHdcCommand(TEXT("fport ls"), &ForwardList, nullptr))
						{
							ForwardList = ForwardList.Replace(TEXT("\r"), TEXT("\n"));
							ForwardList.ParseIntoArray(PortForwardings, TEXT("\n"), true);
						}
					}

					// check if this device already has port forwarding enabled for message bus, eg from another editor session
					for (FString& FwdString : PortForwardings)
					{
						const TCHAR* Ptr = *FwdString;
						FString FwdSerialNumber, FwdHostPortString, FwdDevicePortString;
						uint16 FwdHostPort, FwdDevicePort;
						if (FParse::Token(Ptr, FwdSerialNumber, false) && FwdSerialNumber == NewDeviceInfo.SerialNumber &&
							FParse::Token(Ptr, FwdHostPortString, false) && FParse::Value(*FwdHostPortString, TEXT("tcp:"), FwdHostPort) &&
							FParse::Token(Ptr, FwdDevicePortString, false) && FParse::Value(*FwdDevicePortString, TEXT("tcp:"), FwdDevicePort) && FwdDevicePort == 7777)
						{
							NewDeviceInfo.HostMessageBusPort = FwdHostPort;
							break;
						}
					}

					// if not, setup TCP port forwarding for message bus on first available TCP port above 7777
					if (NewDeviceInfo.HostMessageBusPort == 0)
					{
						uint16 HostMessageBusPort = 7777;
						bool bFoundPort;
						do
						{
							bFoundPort = true;
							for (auto It = DeviceMap.CreateConstIterator(); It; ++It)
							{
								if (HostMessageBusPort == It.Value().HostMessageBusPort)
								{
									bFoundPort = false;
									HostMessageBusPort++;
									break;
								}
							}
						} while (!bFoundPort);

						FString DeviceCommand = FString::Printf(TEXT("-s %s forward tcp:%d tcp:7777"), *NewDeviceInfo.SerialNumber, HostMessageBusPort);
						ExecuteHdcCommand(*DeviceCommand, nullptr, nullptr);
						NewDeviceInfo.HostMessageBusPort = HostMessageBusPort;
					}

					TcpMessagingModule->AddOutgoingConnection(FString::Printf(TEXT("127.0.0.1:%d"), NewDeviceInfo.HostMessageBusPort));
				}

				// Add reverse port forwarding
				uint16 ReversePortMappings[] = {
					41899,	// Network file server, DEFAULT_TCP_FILE_SERVING_PORT in NetworkMessage.h
					1981,	// Unreal Insights data collection, TraceInsightsModule.cpp
					0		// end of list
					};

				for (int32 Idx=0; ReversePortMappings[Idx] > 0; Idx++)
				{
					FString DeviceCommand = FString::Printf(TEXT("-s %s reverse tcp:%d tcp:%d"), *NewDeviceInfo.SerialNumber, ReversePortMappings[Idx], ReversePortMappings[Idx]);
					// It doesn't really matter if a mapping already exists. There is no listening local port so no contention between multiple editor instances
					ExecuteHdcCommand(*DeviceCommand, nullptr, nullptr);
				}
			}

			// add the device to the map
			{
				FScopeLock ScopeLock(DeviceMapLock);

				FOpenHarmonyDeviceInfo& SavedDeviceInfo = DeviceMap.Add(NewDeviceInfo.SerialNumber);
				SavedDeviceInfo = NewDeviceInfo;
			}
		}

		// loop through the previously connected devices list and remove any that aren't still connected from the updated DeviceMap
		TArray<FString> DevicesToRemove;

		for (auto It = DeviceMap.CreateConstIterator(); It; ++It)
		{
			if (!CurrentlyConnectedDevices.Contains(It.Key()))
			{
				if (TcpMessagingModule && It.Value().HostMessageBusPort != 0)
				{
					TcpMessagingModule->RemoveOutgoingConnection(FString::Printf(TEXT("127.0.0.1:%d"), It.Value().HostMessageBusPort));
				}

				DevicesToRemove.Add(It.Key());
			}
		}

		{
			// enter the critical section and remove the devices from the map
			FScopeLock ScopeLock(DeviceMapLock);

			for (auto It = DevicesToRemove.CreateConstIterator(); It; ++It)
			{
				DeviceMap.Remove(*It);
			}
		}
	}

private:

	// path to the hdc command
	FString HDCPath;
	FString GetPropCommand;
	bool bGetExtensionsViaSurfaceFlinger;

	// > 0 if we've been asked to abort work in progress at the next opportunity
	FThreadSafeCounter StopTaskCounter;

	TMap<FString,FOpenHarmonyDeviceInfo>& DeviceMap;
	FCriticalSection* DeviceMapLock;

	FCriticalSection* HDCPathCheckLock;
	bool HasHDCPath;
	bool ForceCheck;

	ITcpMessagingModule* TcpMessagingModule;
};

class FOpenHarmonyDeviceDetection : public IOpenHarmonyDeviceDetection
{
public:

	FOpenHarmonyDeviceDetection() 
		: DetectionThread(nullptr)
		, DetectionThreadRunnable(nullptr)
	{
		// create and fire off our device detection thread
		DetectionThreadRunnable = new FOpenHarmonyDeviceDetectionRunnable(DeviceMap, &DeviceMapLock, &HDCPathCheckLock);
		DetectionThread = FRunnableThread::Create(DetectionThreadRunnable, TEXT("FOpenHarmonyDeviceDetectionRunnable"));

#if WITH_EDITOR
		// add some menu options just for OpenHarmony
		FPIEPreviewDeviceModule* PIEPreviewDeviceModule = FModuleManager::LoadModulePtr<FPIEPreviewDeviceModule>(TEXT("PIEPreviewDeviceProfileSelector"));
		PIEPreviewDeviceModule->AddToDevicePreviewMenuDelegates.AddLambda([this](const FText& CategoryName, class FMenuBuilder& MenuBuilder)
			{
				if (CategoryName.CompareToCaseIgnored(FText::FromString(TEXT("OpenHarmony"))) == 0)
				{
					CreatePIEPreviewMenu(MenuBuilder);
				}
			});
#endif
	}

	virtual ~FOpenHarmonyDeviceDetection()
	{
#if WITH_EDITOR
		FPIEPreviewDeviceModule* PIEPreviewDeviceModule = FModuleManager::GetModulePtr<FPIEPreviewDeviceModule>(TEXT("PIEPreviewDeviceProfileSelector"));
		if (PIEPreviewDeviceModule != nullptr)
		{
			PIEPreviewDeviceModule->AddToDevicePreviewMenuDelegates.Remove(DelegateHandle);
		}
#endif

		if (DetectionThreadRunnable && DetectionThread)
		{
			DetectionThreadRunnable->Stop();
			DetectionThread->WaitForCompletion();
		}
	}

	virtual void Initialize(const TCHAR* InSDKDirectoryEnvVar, const TCHAR* InSDKRelativeExePath, const TCHAR* InGetPropCommand, bool InbGetExtensionsViaSurfaceFlinger) override
	{
		SDKDirEnvVar = InSDKDirectoryEnvVar;
		SDKRelativeExePath = InSDKRelativeExePath;
		GetPropCommand = InGetPropCommand;
		bGetExtensionsViaSurfaceFlinger = InbGetExtensionsViaSurfaceFlinger;
		UpdateHDCPath();
	}

	virtual const TMap<FString,FOpenHarmonyDeviceInfo>& GetDeviceMap() override
	{
		return DeviceMap;
	}

	virtual FCriticalSection* GetDeviceMapLock() override
	{
		return &DeviceMapLock;
	}

	virtual FString GetHDCPath() override
	{
		FScopeLock PathUpdateLock(&HDCPathCheckLock);
		return HDCPath;
	}

	virtual void UpdateHDCPath() override
	{
		FScopeLock PathUpdateLock(&HDCPathCheckLock);
		FString OpenHarmonyDirectory = FPlatformMisc::GetEnvironmentVariable(*SDKDirEnvVar);

		HDCPath.Empty();
		
#if PLATFORM_MAC || PLATFORM_LINUX
		if (OpenHarmonyDirectory.Len() == 0)
		{
#if PLATFORM_LINUX
			// didn't find OPENHARMONY_HOME, so parse the .bashrc file on Linux
			FArchive* FileReader = IFileManager::Get().CreateFileReader(*FString("~/.bashrc"));
#else
			// didn't find OPENHARMONY_HOME, so parse the .bash_profile file on MAC
			FArchive* FileReader = IFileManager::Get().CreateFileReader(*FString([@"~/.bash_profile" stringByExpandingTildeInPath]));
#endif
			if (FileReader)
			{
				const int64 FileSize = FileReader->TotalSize();
				ANSICHAR* AnsiContents = (ANSICHAR*)FMemory::Malloc(FileSize + 1);
				FileReader->Serialize(AnsiContents, FileSize);
				FileReader->Close();
				delete FileReader;

				AnsiContents[FileSize] = 0;
				TArray<FString> Lines;
				FString(ANSI_TO_TCHAR(AnsiContents)).ParseIntoArrayLines(Lines);
				FMemory::Free(AnsiContents);

				for (int32 Index = Lines.Num()-1; Index >=0; Index--)
				{
					if (OpenHarmonyDirectory.Len() == 0 && Lines[Index].StartsWith(FString::Printf(TEXT("export %s="), *SDKDirEnvVar)))
					{
						FString Directory;
						Lines[Index].Split(TEXT("="), NULL, &Directory);
						Directory = Directory.Replace(TEXT("\""), TEXT(""));
						OpenHarmonyDirectory = Directory;
						setenv(TCHAR_TO_ANSI(*SDKDirEnvVar), TCHAR_TO_ANSI(*OpenHarmonyDirectory), 1);
					}
				}
			}
		}
#endif

		if (OpenHarmonyDirectory.Len() > 0)
		{
			HDCPath = FPaths::Combine(*OpenHarmonyDirectory, SDKRelativeExePath);

			// if it doesn't exist then just clear the path as we might set it later
			if (!FPaths::FileExists(HDCPath))
			{
				HDCPath.Empty();
			}
		}
		DetectionThreadRunnable->UpdateHDCPath(HDCPath, GetPropCommand, bGetExtensionsViaSurfaceFlinger);
	}

	virtual void ExportDeviceProfile(const FString& OutPath, const FString& DeviceName) override
	{
		// instantiate an FPIEPreviewDeviceSpecifications instance and its values
		FPIEPreviewDeviceSpecifications DeviceSpecs;

		bool bOpenGL3x = false;
		{
			FScopeLock ExportLock(GetDeviceMapLock());

			const FOpenHarmonyDeviceInfo* DeviceInfo = GetDeviceMap().Find(DeviceName);
			if (DeviceInfo == nullptr)
			{
				FMessageDialog::Open(EAppMsgType::Ok, EAppReturnType::Ok, LOCTEXT("loc_ExportError_Message", "Device disconnected!"), LOCTEXT("loc_ExportError_Title", "File export error."));
				return;
			}

			// generic values
			DeviceSpecs.DevicePlatform = EPIEPreviewDeviceType::OpenHarmony;
			DeviceSpecs.ResolutionX = DeviceInfo->ResolutionX;
			DeviceSpecs.ResolutionY = DeviceInfo->ResolutionY;
			DeviceSpecs.ResolutionYImmersiveMode = 0;
			DeviceSpecs.PPI = DeviceInfo->DeviceDPI;
			DeviceSpecs.ScaleFactors = { 0.25f, 0.5f, 0.75f, 1.0f };

			// OpenHarmony specific values
			DeviceSpecs.OpenHarmonyProperties.OpenHarmonyVersion = DeviceInfo->HumanOpenHarmonyVersion;
			DeviceSpecs.OpenHarmonyProperties.DeviceModel = DeviceInfo->Model;
			DeviceSpecs.OpenHarmonyProperties.DeviceMake = DeviceInfo->DeviceBrand;
			DeviceSpecs.OpenHarmonyProperties.GLVersion = DeviceInfo->OpenGLVersionString;
			DeviceSpecs.OpenHarmonyProperties.GPUFamily = DeviceInfo->GPUFamilyString;
			DeviceSpecs.OpenHarmonyProperties.VulkanVersion = DeviceInfo->VulkanVersion;
			DeviceSpecs.OpenHarmonyProperties.Hardware = DeviceInfo->Hardware;
			DeviceSpecs.OpenHarmonyProperties.DeviceBuildNumber = DeviceInfo->BuildNumber;
			// this is used in the same way as PlatformMemoryBucket..
			// to establish the nearest GB OpenHarmony has a different rounding algo (hence 384 used here). See GenericPlatformMemory::GetMemorySizeBucket.
			DeviceSpecs.OpenHarmonyProperties.TotalPhysicalGB = FString::Printf(TEXT("%d"),(((uint64)DeviceInfo->TotalPhysicalKB + 384 * 1024 - 1) / 1024 / 1024));
			
			DeviceSpecs.OpenHarmonyProperties.UsingHoudini = false;
			DeviceSpecs.OpenHarmonyProperties.VulkanAvailable = !(DeviceInfo->VulkanVersion.IsEmpty() || DeviceInfo->VulkanVersion.Contains(TEXT("0.0.0")));

			// OpenGL ES 3.x
			bOpenGL3x = DeviceInfo->OpenGLVersionString.Contains(TEXT("OpenGL ES 3"));
			if (bOpenGL3x)
			{
				DeviceSpecs.OpenHarmonyProperties.GLES31RHIState.MaxTextureDimensions = 4096;
				DeviceSpecs.OpenHarmonyProperties.GLES31RHIState.MaxShadowDepthBufferSizeX = 2048;
				DeviceSpecs.OpenHarmonyProperties.GLES31RHIState.MaxShadowDepthBufferSizeY = 2048;
				DeviceSpecs.OpenHarmonyProperties.GLES31RHIState.MaxCubeTextureDimensions = 2048;
				DeviceSpecs.OpenHarmonyProperties.GLES31RHIState.SupportsRenderTargetFormat_PF_G8 = true;
				DeviceSpecs.OpenHarmonyProperties.GLES31RHIState.SupportsRenderTargetFormat_PF_FloatRGBA = DeviceInfo->GLESExtensions.Contains(TEXT("GL_EXT_color_buffer_half_float"));
				DeviceSpecs.OpenHarmonyProperties.GLES31RHIState.SupportsMultipleRenderTargets = true;
			}

			// OpenGL ES 2.0 devices are no longer supported.
			if (!bOpenGL3x)
			{
				UE_LOG(LogCore, Warning, TEXT("Cannot export device info, a minimum of OpenGL ES 3 is required."));
				return;
			}
		} // FScopeLock ExportLock released

		// create a JSon object from the above structure
		TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject<FPIEPreviewDeviceSpecifications>(DeviceSpecs);

		// remove IOS and switch fields
		JsonObject->RemoveField(TEXT("IOSProperties"));
		JsonObject->RemoveField(TEXT("switchProperties"));

		// serialize the JSon object to string
		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

		// export file to disk
		FFileHelper::SaveStringToFile(OutputString, *OutPath);
	} // end of virtual void ExportDeviceProfile(...)

private:

	// path to the hdc command (local)
	FString HDCPath;

	FString SDKDirEnvVar;
	FString SDKRelativeExePath;
	FString GetPropCommand;
	bool bGetExtensionsViaSurfaceFlinger;

	FRunnableThread* DetectionThread;
	FOpenHarmonyDeviceDetectionRunnable* DetectionThreadRunnable;

	TMap<FString,FOpenHarmonyDeviceInfo> DeviceMap;
	FCriticalSection DeviceMapLock;
	FCriticalSection HDCPathCheckLock;


#if WITH_EDITOR
	FDelegateHandle DelegateHandle;

	// function will enumerate available OpenHarmony devices that can export their profile to a json file
	// called (below) from AddOpenHarmonyConfigExportMenu()
	void AddOpenHarmonyConfigExportSubMenus(FMenuBuilder& InMenuBuilder)
	{
		TMap<FString, FOpenHarmonyDeviceInfo> OpenHarmonyDeviceMap;

		// lock device map and copy its contents
		{
			FCriticalSection* DeviceLock = GetDeviceMapLock();
			FScopeLock Lock(DeviceLock);
			OpenHarmonyDeviceMap = GetDeviceMap();
		}

		for (auto& Pair : OpenHarmonyDeviceMap)
		{
			FOpenHarmonyDeviceInfo& DeviceInfo = Pair.Value;

			FString ModelName = DeviceInfo.Model + TEXT("[") + DeviceInfo.DeviceBrand + TEXT("]");

			// lambda function called to open the save dialog and trigger device export
			auto LambdaSaveConfigFile = [DeviceName = Pair.Key, DefaultFileName = ModelName, this]()
			{
				TArray<FString> OutputFileName;
				FString DefaultFolder = FPaths::EngineContentDir() + TEXT("Editor/PIEPreviewDeviceSpecs/OpenHarmony/");

				bool bResult = FDesktopPlatformModule::Get()->SaveFileDialog(
					FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
					LOCTEXT("PackagePluginDialogTitle", "Save platform configuration...").ToString(),
					DefaultFolder,
					DefaultFileName,
					TEXT("Json config file (*.json)|*.json"),
					0,
					OutputFileName);

				if (bResult && OutputFileName.Num())
				{
					ExportDeviceProfile(OutputFileName[0], DeviceName);
				}
			};

			InMenuBuilder.AddMenuEntry(
				FText::FromString(ModelName),
				FText(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "AssetEditor.SaveAsset"),
				FUIAction(FExecuteAction::CreateLambda(LambdaSaveConfigFile))
			);
		}
	}

	// function adds a sub-menu that will enumerate OpenHarmony devices whose profiles can be exported json files
	void AddOpenHarmonyConfigExportMenu(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.AddMenuSeparator();

		MenuBuilder.AddSubMenu(
			LOCTEXT("loc_AddOpenHarmonyConfigExportMenu", "Export device settings"),
			LOCTEXT("loc_tip_AddOpenHarmonyConfigExportMenu", "Export device settings to a Json file."),
			FNewMenuDelegate::CreateLambda([this](FMenuBuilder& Builder) { AddOpenHarmonyConfigExportSubMenus(Builder); }),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.SaveAll")
		);
	}

	// OpenHarmony devices can export their profile to a json file which then can be used for PIE device simulations
	void CreatePIEPreviewMenu(FMenuBuilder& MenuBuilder)
	{
		// check to see if we have any connected devices
		bool bHasOpenHarmonyDevices = false;
		{
			FCriticalSection* DeviceLock = GetDeviceMapLock();
			FScopeLock Lock(DeviceLock);
			bHasOpenHarmonyDevices = GetDeviceMap().Num() > 0;
		}

		// add the config. export menu
		if (bHasOpenHarmonyDevices)
		{
			AddOpenHarmonyConfigExportMenu(MenuBuilder);
		}
	}
#endif
};


/**
 * Holds the target platform singleton.
 */
static TMap<FString, FOpenHarmonyDeviceDetection*> OpenHarmonyDeviceDetectionSingletons;


/**
 * Module for detecting openharmony devices.
 */
class FOpenHarmonyDeviceDetectionModule : public IOpenHarmonyDeviceDetectionModule
{
public:
	/**
	 * Destructor.
	 */
	~FOpenHarmonyDeviceDetectionModule( )
	{
		for (auto It : OpenHarmonyDeviceDetectionSingletons)
		{
			delete It.Value;
		}
		OpenHarmonyDeviceDetectionSingletons.Empty();
	}

	virtual IOpenHarmonyDeviceDetection* GetOpenHarmonyDeviceDetection(const TCHAR* OverridePlatformName) override
	{
		FString Key(OverridePlatformName);
		FOpenHarmonyDeviceDetection* Value = OpenHarmonyDeviceDetectionSingletons.FindRef(Key);
		if (Value == nullptr)
		{
			Value = OpenHarmonyDeviceDetectionSingletons.Add(Key, new FOpenHarmonyDeviceDetection());
		}
		return Value;
	}
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FOpenHarmonyDeviceDetectionModule, OpenHarmonyDeviceDetection);
