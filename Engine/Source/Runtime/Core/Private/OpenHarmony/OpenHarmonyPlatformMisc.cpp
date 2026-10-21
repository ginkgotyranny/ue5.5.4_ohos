#include "OpenHarmony/OpenHarmonyPlatformMisc.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include <hilog/log.h>
#include "OpenHarmony/OpenHarmonyJSMessageBox.h"

#include <sched.h>
#include "Modules/ModuleManager.h"
#include "FramePro/FrameProProfiler.h"
#include <hitrace/trace.h>
#include <sys/statvfs.h>
#include "Misc/FileHelper.h"
#include "OpenHarmony/OpenHarmonyArkTsEnv.h"
#include "OpenHarmony/OpenHarmonyPlatformCrashContext.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "Misc/CoreDelegates.h"


#define GOpenHarmonyPackageVersion	1 


static int32 GOpenHarmonyTraceMarkersEnabled = 0;
static FAutoConsoleVariableRef COpenHarmonyTraceMarkersEnabled(
	TEXT("openharmony.tracemarkers"),
	GOpenHarmonyTraceMarkersEnabled,
	TEXT("Enable outputting named events to OpenHarmony trace marker file.\n"),
	ECVF_Default
);

static int32 GOpenHarmonyLowPowerBatteryThreshold = 15;
static FAutoConsoleVariableRef COpenHarmonyLowPowerBatteryThreshold(
	TEXT("openharmony.LowPowerBatteryThreshold"),
	GOpenHarmonyLowPowerBatteryThreshold,
	TEXT("The battery level below which the device is considered in a low power state."),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarMaliMidgardIndexingBug(
	TEXT("r.OpenHarmony.MaliMidgardIndexingBug"),
	0,
	TEXT("For an indexed instance draw, the OpenGL ES driver does not handle attributes correctly. This issue only happens on Mali T8xx GPU when the difference between two adjacent index values are larger than 16.\n")
	TEXT("  0 = off\n")
	TEXT("  1 = on."),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<FString> CVarOpenHarmonyCPUThermalSensorFilePath(
	TEXT("OpenHarmony.CPUThermalSensorFilePath"),
	"",
	TEXT("Overrides CPU Thermal sensor file path")
);

static float GOpenHarmonyMemoryStateChangeThreshold = 0.1f;
static FAutoConsoleVariableRef COpenHarmonyMemoryStateChangeThreshold(
	TEXT("OpenHarmony.OpenHarmonyMemoryStateChangeThreshold"),
	GOpenHarmonyMemoryStateChangeThreshold,
	TEXT("The memory state change threshold after which memory state is reported to memory warning callback"),
	ECVF_Default
);
#if STATS || ENABLE_STATNAMEDEVENTS
int32 FOpenHarmonyMisc::TraceMarkerFileDescriptor = -1;
typedef void (*ATrace_beginSection_Type) (const char* sectionName);
typedef void (*ATrace_endSection_Type) (void);
typedef bool (*ATrace_isEnabled_Type) (void);

static ATrace_beginSection_Type ATrace_beginSection = NULL;
static ATrace_endSection_Type ATrace_endSection = NULL;
static ATrace_isEnabled_Type ATrace_isEnabled = NULL;
static bool bUseNativeSystrace = false;
#endif
FString FOpenHarmonyMisc::OpenHarmonyVersion; // version of Openharmony are running eg "4.0.4"
int32   FOpenHarmonyMisc::OpenHarmonyMajorVersion = 0; // integer major version of openharmony we are running, eg 12
int32   FOpenHarmonyMisc::TargetSDKVersion; // Target SDK version, eg 12.
FString FOpenHarmonyMisc::DeviceMake; // make of the device we are running on eg. "mate40"
FString FOpenHarmonyMisc::DeviceModel; // model of the device we are running on eg "CMS-100"
FString FOpenHarmonyMisc::DeviceBuildNumber; // platform image build number of device "R16NW.G960NKSU1ARD6"
FString FOpenHarmonyMisc::OSLanguage; // language code the device is set to eg "deu""
FString FOpenHarmonyMisc::ProductName; // version of openharmony we are running eg "API 12"

// Build/API level we are running.
int32 FOpenHarmonyMisc::OpenHarmonyBuildVersion = 0;

// Whether or not the system handles the volume buttons (event will still be generated either way)
bool FOpenHarmonyMisc::VolumeButtonsHandledBySystem = true;

// Whether an app restart is needed to free driver allocated memory after precompiling PSOs
bool FOpenHarmonyMisc::bNeedsRestartAfterPSOPrecompile = false;

// Key/Value pair variables from the optional configuration.txt
TMap<FString, FString> FOpenHarmonyMisc::ConfigRulesVariables;

static FCriticalSection OpenHarmonyThreadNamesLock;
static TMap<uint32, const char*, TFixedSetAllocator<16>> OpenHarmonyThreadNames;

EDeviceScreenOrientation FOpenHarmonyMisc::DeviceOrientation = EDeviceScreenOrientation::Unknown;

int FOpenHarmonyMisc::TmpDeviceOrientation = 0;

extern void OpenHarmonyThunkCpp_ForceQuit();
extern void OpenHarmonyThunkCpp_SetOrientation(int32 Value);
//TODO:
extern void OpenHarmonyThunkCpp_SetCellularPreference(int32 Value);
//TODO:
extern int32 OpenHarmonyThunkCpp_GetCellularPreference();


static char OpenHarmonyCpuThermalSensorFileBuf[256] = "";
//待验证
static void OverrideCpuThermalSensorFileFromCVar(IConsoleVariable* Var)
{
	FString Override = CVarOpenHarmonyCPUThermalSensorFilePath.GetValueOnAnyThread();
	const int32 Len = Override.Len();
	if (Len == 0)
	{
		return;
	}
	if (Len < UE_ARRAY_COUNT(OpenHarmonyCpuThermalSensorFileBuf))
	{
		FCStringAnsi::Strcpy(OpenHarmonyCpuThermalSensorFileBuf, TCHAR_TO_ANSI(*Override));
		UE_LOG(LogOpenHarmony, Display, TEXT("Thermal sensor's filepath was set to `%s`"), *Override);
		return;
	}
	UE_LOG(LogOpenHarmony, Display, TEXT("Thermal sensor's filepath is too long, max path is `%u`"), UE_ARRAY_COUNT(OpenHarmonyCpuThermalSensorFileBuf));
}

static void InitCpuThermalSensor()
{
	OverrideCpuThermalSensorFileFromCVar(nullptr);
	CVarOpenHarmonyCPUThermalSensorFilePath->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OverrideCpuThermalSensorFileFromCVar));

	uint32 Counter = 0;
	const uint32 INVALID_INDEX = -1;
	uint32 CPUSensorIndex = INVALID_INDEX;
	while (true)
	{
		char Buf[256] = "";
		sprintf(Buf, "/sys/devices/virtual/thermal/thermal_zone%u/type", Counter);
		if (FILE* File = fopen(Buf, "r"))
		{
			fgets(Buf, UE_ARRAY_COUNT(Buf), File);
			fclose(File);
			char* Ptr = Buf;
			while (!iscntrl(*Ptr))		// it appears that zone type string ends up with \n symbol
			{
				++Ptr;
			}
			*Ptr = 0;

			if (strstr(Buf, "cpu-") && CPUSensorIndex == INVALID_INDEX)
			{
				CPUSensorIndex = Counter;
				FCStringAnsi::Sprintf(OpenHarmonyCpuThermalSensorFileBuf, "/sys/devices/virtual/thermal/thermal_zone%u/temp", Counter);
			}

			UE_LOG(LogOpenHarmony, Display, TEXT("Detected thermal sensor `%s` at /sys/devices/virtual/thermal/thermal_zone%u/temp"), ANSI_TO_TCHAR(Buf), Counter);
			++Counter;
		}
		else
		{
			break;
		}
	}

	TArray<FString> SensorLocations;
	GConfig->GetArray(TEXT("ThermalSensors"), TEXT("SensorLocations"), SensorLocations, GEngineIni);

	for (uint32 i = 0; i < SensorLocations.Num(); ++i)
	{
		auto ConvertedStr = StringCast<ANSICHAR>(*SensorLocations[i]);
		const char* SensorFilePath = ConvertedStr.Get();
		if (FILE* File = fopen(SensorFilePath, "r"))
		{
			FCStringAnsi::Strcpy(OpenHarmonyCpuThermalSensorFileBuf, SensorFilePath);
			UE_LOG(LogOpenHarmony, Display, TEXT("Selecting thermal sensor located at `%s`"), *SensorLocations[i]);
			fclose(File);
			return;
		}
	}

	if (CPUSensorIndex != INVALID_INDEX)
	{
		UE_LOG(LogOpenHarmony, Display, TEXT("Selecting thermal sensor located at `%s`"), ANSI_TO_TCHAR(OpenHarmonyCpuThermalSensorFileBuf));
	}
	else
	{
		UE_LOG(LogOpenHarmony, Display, TEXT("No CPU thermal sensor was detected. To manually override the sensor path set OpenHarmony.CPUThermalSensorFilePath CVar."));
	}
}


//待验证
void FOpenHarmonyMisc::RequestExit( bool Force, const TCHAR* CallSite)
{
	if (Force)
	{
		OpenHarmonyThunkCpp_ForceQuit();
	}
	else
	{
		RequestEngineExit(TEXT("OpenHarmony RequestExit"));
	}
}


extern void OpenHarmonyThunkCpp_RestartApplication();
bool FOpenHarmonyMisc::RestartApplication()
{
#if USE_OPENHARMONY_NAPI 
	OpenHarmonyThunkCpp_RestartApplication();
	return true;
#else
	return FGenericPlatformMisc::RestartApplication();
#endif //USE_OPENHARMONY_NAPI
}
void FOpenHarmonyMisc::SetThreadName(const char* name)
{
	pthread_setname_np(pthread_self(), name);
}

void FOpenHarmonyMisc::LocalPrint(const TCHAR *Message)
{
	// Builds for distribution should not have logging in them:
	// http://developer.openharmony.com/tools/publishing/preparing.html#publishing-configure
#if !UE_BUILD_SHIPPING
	const int MAX_LOG_LENGTH = 4096;
	// not static since may be called by different threads
	wchar_t MessageBuffer[MAX_LOG_LENGTH];

	const TCHAR* SourcePtr = Message;
	while (*SourcePtr)
	{
		wchar_t* WritePtr = MessageBuffer;
		int32 RemainingSpace = MAX_LOG_LENGTH;
		while (*SourcePtr && --RemainingSpace > 0)
		{
			if (*SourcePtr == TEXT('\r'))
			{
				// If next character is newline, skip it
				if (*(++SourcePtr) == TEXT('\n'))
					++SourcePtr;
				break;
			}
			else if (*SourcePtr == TEXT('\n'))
			{
				++SourcePtr;
				break;
			}
			else {
				*WritePtr++ = static_cast<wchar_t>(*SourcePtr++);
			}
		}
		*WritePtr = '\0';
		OH_LOG_Print(LOG_APP, LOG_INFO, 0x0001, "UE5OH", "%{public}ls", MessageBuffer);
	}
#endif
}

// Test for device vulkan support.
static void EstablishVulkanDeviceSupport();
//待验证
namespace FOpenHarmonyAppEntry
{
	extern void PlatformInit();
}
//待验证
void FOpenHarmonyMisc::PlatformPreInit()
{
	FOpenHarmonyCrashContext::Initialize();
	FGenericPlatformMisc::PlatformPreInit();
	EstablishVulkanDeviceSupport();
	FOpenHarmonyAppEntry::PlatformInit();
}

static volatile bool HeadPhonesArePluggedIn = false; 

static FOpenHarmonyMisc::FBatteryState CurrentBatteryState; 

static FCriticalSection ReceiversLock; 

static struct
{
	int		Volume;
	double	TimeOfChange;
} CurrentVolume;
static FDelegateHandle OpenHarmonyOnBackgroundBinding;
static FDelegateHandle OpenHarmonyOnForegroundBinding;
#if (STATS || ENABLE_STATNAMEDEVENTS)

static void StartTraceMarkers()
{
	if (FOpenHarmonyMisc::TraceMarkerFileDescriptor != -1)
	{
		UE_LOG(LogOpenHarmony, Warning, TEXT("Systrace event logging already open."));
		return;
	}

	// Setup trace file descriptor
	FOpenHarmonyMisc::TraceMarkerFileDescriptor = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
	if (FOpenHarmonyMisc::TraceMarkerFileDescriptor == -1)
	{
		UE_LOG(LogOpenHarmony, Warning, TEXT("Trace Marker failed to open; systrace support disabled"));
	}
	else
	{
		UE_LOG(LogOpenHarmony, Display, TEXT("Started systrace events logging."));
	}
}

static void StopTraceMarkers()
{
	// Tear down trace file descriptor
	if (FOpenHarmonyMisc::TraceMarkerFileDescriptor != -1)
	{
		close(FOpenHarmonyMisc::TraceMarkerFileDescriptor);
		FOpenHarmonyMisc::TraceMarkerFileDescriptor = -1;
		UE_LOG(LogOpenHarmony, Display, TEXT("Stopped systrace events logging."));
	}
}

static void UpdateTraceMarkersEnable(IConsoleVariable* Var)
{
	if (!GOpenHarmonyTraceMarkersEnabled)
	{
		StopTraceMarkers();
	}
	else
	{
		StartTraceMarkers();
	}
}
#endif
static bool GEnableReceivers = false;
void EnableEventReceivers(bool bEnableReceivers)
{
	GEnableReceivers = bEnableReceivers;
}

void FOpenHarmonyMisc::PlatformInit()
{
	// Increase the maximum number of simultaneously open files
	// Display Timer resolution.
	// Get swap file info
	// Display memory info
	// Setup user specified thread affinity if any
	extern void OpenHarmonySetupDefaultThreadAffinity();
	OpenHarmonySetupDefaultThreadAffinity();

#if (STATS || ENABLE_STATNAMEDEVENTS)
	//鸿蒙侧只会使用UseNativeSystrace，把火焰图数据输出到OH_HiTrace中，对接smartperf,删除无用分支
	bUseNativeSystrace = true;
#endif
	OpenHarmonyOnBackgroundBinding = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddStatic(EnableEventReceivers, false);
	OpenHarmonyOnForegroundBinding = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddStatic(EnableEventReceivers, true);
}
extern void OpenHarmonyThunkCpp_DismissSplashScreen();
void FOpenHarmonyMisc::PlatformTearDown()
{
	UE_LOG(LogOpenHarmony, Display, TEXT("FOpenHarmonyMisc::PlatformTearDown"));
#if (STATS || ENABLE_STATNAMEDEVENTS)
	StopTraceMarkers();
#endif

	auto RemoveBinding = [](FCoreDelegates::FApplicationLifetimeDelegate& ApplicationLifetimeDelegate, FDelegateHandle& DelegateBinding)
	{
		if (DelegateBinding.IsValid())
		{
			ApplicationLifetimeDelegate.Remove(DelegateBinding);
			DelegateBinding.Reset();
		}
	};

	RemoveBinding(FCoreDelegates::ApplicationWillEnterBackgroundDelegate, OpenHarmonyOnBackgroundBinding);
	RemoveBinding(FCoreDelegates::ApplicationHasEnteredForegroundDelegate, OpenHarmonyOnForegroundBinding);
}
void FOpenHarmonyMisc::PlatformHandleSplashScreen(bool ShowSplashScreen)
{
#if USE_OPENHARMONY_NAPI
	if (!ShowSplashScreen)
	{
		OpenHarmonyThunkCpp_DismissSplashScreen();
	}
#endif
}

void FOpenHarmonyMisc::GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength)
{
	*Result = 0;
	// @todo OpenHarmony : get environment variable.
}

FString FOpenHarmonyMisc::GetEnvironmentVariable(const TCHAR* VariableName)
{
	// @todo OpenHarmony : get environment variable.
	return FString();
}

//待验证
const TCHAR* FOpenHarmonyMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	if (Error == 0)
	{
		Error = errno;
	}
	char ErrorBuffer[1024];
	if (strerror_r(Error, ErrorBuffer, 1024) == 0)
	{
		FCString::Strcpy(OutBuffer, BufferCount, UTF8_TO_TCHAR((const ANSICHAR*)ErrorBuffer));
	}
	else
	{
		*OutBuffer = TEXT('\0');
	}
	return OutBuffer;
}


EAppReturnType::Type FOpenHarmonyMisc::MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption )
{
#if USE_OPENHARMONY_NAPI
	FJSOpenHarmonyMessageBox MessageBox;
	MessageBox.SetText(Text);
	MessageBox.SetCaption(Caption);
	EAppReturnType::Type * ResultValues = nullptr;
	static EAppReturnType::Type ResultsOk[] = {
		EAppReturnType::Ok };
	static EAppReturnType::Type ResultsYesNo[] = {
		EAppReturnType::Yes, EAppReturnType::No };
	static EAppReturnType::Type ResultsOkCancel[] = {
		EAppReturnType::Ok, EAppReturnType::Cancel };
	static EAppReturnType::Type ResultsYesNoCancel[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::Cancel };
	static EAppReturnType::Type ResultsCancelRetryContinue[] = {
		EAppReturnType::Cancel, EAppReturnType::Retry, EAppReturnType::Continue };
	static EAppReturnType::Type ResultsYesNoYesAllNoAll[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::YesAll,
		EAppReturnType::NoAll };
	static EAppReturnType::Type ResultsYesNoYesAllNoAllCancel[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::YesAll,
		EAppReturnType::NoAll, EAppReturnType::Cancel };
	static EAppReturnType::Type ResultsYesNoYesAll[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::YesAll };

	// TODO: Should we localize button text?

	switch (MsgType)
	{
	case EAppMsgType::Ok:
		MessageBox.AddButton(TEXT("Ok"));
		ResultValues = ResultsOk;
		break;
	case EAppMsgType::YesNo:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		ResultValues = ResultsYesNo;
		break;
	case EAppMsgType::OkCancel:
		MessageBox.AddButton(TEXT("Ok"));
		MessageBox.AddButton(TEXT("Cancel"));
		ResultValues = ResultsOkCancel;
		break;
	case EAppMsgType::YesNoCancel:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Cancel"));
		ResultValues = ResultsYesNoCancel;
		break;
	case EAppMsgType::CancelRetryContinue:
		MessageBox.AddButton(TEXT("Cancel"));
		MessageBox.AddButton(TEXT("Retry"));
		MessageBox.AddButton(TEXT("Continue"));
		ResultValues = ResultsCancelRetryContinue;
		break;
	case EAppMsgType::YesNoYesAllNoAll:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Yes To All"));
		MessageBox.AddButton(TEXT("No To All"));
		ResultValues = ResultsYesNoYesAllNoAll;
		break;
	case EAppMsgType::YesNoYesAllNoAllCancel:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Yes To All"));
		MessageBox.AddButton(TEXT("No To All"));
		MessageBox.AddButton(TEXT("Cancel"));
		ResultValues = ResultsYesNoYesAllNoAllCancel;
		break;
	case EAppMsgType::YesNoYesAll:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Yes To All"));
		ResultValues = ResultsYesNoYesAll;
		break;
	default:
		check(0);
	}
	int32 Choice = MessageBox.Show();
	if (Choice >= 0 && nullptr != ResultValues)
	{
		return ResultValues[Choice];
	}
#endif

	// Failed to show dialog, or failed to get a response,
	// return default cancel response instead.
	return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
}
bool FOpenHarmonyMisc::HasPlatformFeature(const TCHAR* FeatureName)  
{
	if (FCString::Stricmp(FeatureName, TEXT("Vulkan")) == 0)
	{
		return FOpenHarmonyMisc::ShouldUseVulkan();
	}

	return FGenericPlatformMisc::HasPlatformFeature(FeatureName);
}

bool FOpenHarmonyMisc::UseRenderThread()
{
	return true;
}

int32 FOpenHarmonyMisc::NumberOfCores()
{
	int32 NumberOfCores = 1;

	static int CalculatedNumberOfCores = 0;
	if (CalculatedNumberOfCores == 0)
	{
		pid_t ThreadId = gettid();
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		if (sched_getaffinity(ThreadId, sizeof(cpuset), &cpuset) != -1)
		{
			CalculatedNumberOfCores = CPU_COUNT(&cpuset);
		}

		UE_LOG(LogTemp, Log, TEXT("%d cores and %d assignable cores"), NumberOfCores, CalculatedNumberOfCores);
	}

	return !CalculatedNumberOfCores ? NumberOfCores : CalculatedNumberOfCores;
}

int32 FOpenHarmonyMisc::NumberOfCoresIncludingHyperthreads()
{
	return NumberOfCores();
}


static FOpenHarmonyMisc::FCPUState CurrentCPUState; 
//待验证 
FOpenHarmonyMisc::FCPUState& FOpenHarmonyMisc::GetCPUState() {
	uint64_t UserTime, NiceTime, SystemTime, SoftIRQTime, IRQTime, IdleTime, IOWaitTime;
	int32		Index = 0;
	ANSICHAR	Buffer[500];

	CurrentCPUState.CoreCount = FMath::Min(FOpenHarmonyMisc::NumberOfCores(), FOpenHarmonyMisc::FCPUState::MaxSupportedCores);
	FILE* FileHandle = fopen("/proc/cpuinfo", "r");
	if (FileHandle) {
		CurrentCPUState.ActivatedCoreCount = 0;
		for (size_t n = 0; n < CurrentCPUState.CoreCount; n++) {
			CurrentCPUState.Status[n] = 0;
			CurrentCPUState.PreviousUsage[n] = CurrentCPUState.CurrentUsage[n];
		}

		while (fgets(Buffer, 100, FileHandle)) {
#if PLATFORM_64BITS
			sscanf(Buffer, "%5s %8lu %8lu %8lu %8lu %8lu %8lu %8lu", CurrentCPUState.Name,
				&UserTime, &NiceTime, &SystemTime, &IdleTime, &IOWaitTime, &IRQTime,
				&SoftIRQTime);
#else
			sscanf(Buffer, "%5s %8llu %8llu %8llu %8llu %8llu %8llu %8llu", CurrentCPUState.Name,
				&UserTime, &NiceTime, &SystemTime, &IdleTime, &IOWaitTime, &IRQTime,
				&SoftIRQTime);
#endif

			if (0 == strncmp(CurrentCPUState.Name, "cpu", 3)) {
				Index = CurrentCPUState.Name[3] - '0';
				if (Index >= 0 && Index < CurrentCPUState.CoreCount)
				{
					if (CurrentCPUState.Name[5] != '\0')
					{
						Index = atol(&CurrentCPUState.Name[3]);
					}
					CurrentCPUState.CurrentUsage[Index].IdleTime = IdleTime;
					CurrentCPUState.CurrentUsage[Index].NiceTime = NiceTime;
					CurrentCPUState.CurrentUsage[Index].SystemTime = SystemTime;
					CurrentCPUState.CurrentUsage[Index].SoftIRQTime = SoftIRQTime;
					CurrentCPUState.CurrentUsage[Index].IRQTime = IRQTime;
					CurrentCPUState.CurrentUsage[Index].IOWaitTime = IOWaitTime;
					CurrentCPUState.CurrentUsage[Index].UserTime = UserTime;
					CurrentCPUState.CurrentUsage[Index].TotalTime = UserTime + NiceTime + SystemTime + SoftIRQTime + IRQTime + IdleTime + IOWaitTime;
					CurrentCPUState.Status[Index] = 1;
					CurrentCPUState.ActivatedCoreCount++;
				}
				if (Index == CurrentCPUState.CoreCount - 1)
					break;
			}
		}
		fclose(FileHandle);

		uint64_t WallTime;
		double CPULoad[CurrentCPUState.CoreCount];
		CurrentCPUState.AverageUtilization = 0.0;
		for (size_t n = 0; n < CurrentCPUState.CoreCount; n++) {
			if (CurrentCPUState.CurrentUsage[n].TotalTime <= CurrentCPUState.PreviousUsage[n].TotalTime) {
				CPULoad[n] = 0;
				continue;
			}

			WallTime = CurrentCPUState.CurrentUsage[n].TotalTime - CurrentCPUState.PreviousUsage[n].TotalTime;
			IdleTime = CurrentCPUState.CurrentUsage[n].IdleTime - CurrentCPUState.PreviousUsage[n].IdleTime;

			if (!WallTime || WallTime <= IdleTime) {
				CPULoad[n] = 0;
				continue;
			}
			CPULoad[n] = ((double)WallTime - (double)IdleTime) * 100.0 / (double)WallTime;
			CurrentCPUState.Utilization[n] = CPULoad[n];
			CurrentCPUState.AverageUtilization += CPULoad[n];
		}
		CurrentCPUState.AverageUtilization /= (double)CurrentCPUState.CoreCount;
	}
	else {
		FMemory::Memzero(CurrentCPUState);
	}
	return CurrentCPUState;
}

bool FOpenHarmonyMisc::SupportsLocalCaching()
{
	return true;
	/*if ( SupportsUTime() )
	{
		return true;
	}*/
}

static int SysGetRandomSupported = -1;

void DefaultCrashHandler(const FOpenHarmonyCrashContext& Context)
{
}
//
///** Global pointer to crash handler */
void (*GCrashHandlerPointer)(const FGenericCrashContext& Context) = NULL;
//

void FOpenHarmonyMisc::TriggerCrashHandler(ECrashContextType InType, const TCHAR* InErrorMessage, const TCHAR* OverrideCallstack)
{
} 
//待验证
bool FOpenHarmonyMisc::GetUseVirtualJoysticks()
{
	return true;
}
//待验证
bool FOpenHarmonyMisc::SupportsTouchInput()
{
	return true;
}
extern void OpenHarmonyThunkCpp_RegisterForRemoteNotifications(); //待实现 TODO：
extern void OpenHarmonyThunkCpp_UnregisterForRemoteNotifications();//待实现  TODO：
extern bool OpenHarmonyThunkCpp_IsAllowedRemoteNotifications();//待实现 默认返回false TODO：
extern FString GFontPathBase;

void FOpenHarmonyMisc::RegisterForRemoteNotifications()
{
#if USE_OPENHATRMONY_NAPI
	OpenHarmonyThunkCpp_RegisterForRemoteNotifications();
#endif
}

void FOpenHarmonyMisc::UnregisterForRemoteNotifications()
{
#if USE_OPENHATRMONY_NAPI
	OpenHarmonyThunkCpp_UnregisterForRemoteNotifications();
#endif
}

bool FOpenHarmonyMisc::IsAllowedRemoteNotifications()
{
#if USE_OPENHATRMONY_NAPI
	return OpenHarmonyThunkCpp_IsAllowedRemoteNotifications();
#else
	return false;
#endif
}

//待验证
TArray<uint8> FOpenHarmonyMisc::GetSystemFontBytes()
{
#if USE_OPENHARMONY_FILE
	TArray<uint8> FontBytes;
	static FString FullFontPath = GFontPathBase + FString(TEXT("HarmonyOS_Sans.ttf"));
	FFileHelper::LoadFileToArray(FontBytes, *FullFontPath);
	return FontBytes;
#else 
	return FGenericPlatformMisc::GetSystemFontBytes();
#endif
}


class IPlatformChunkInstall* FOpenHarmonyMisc::GetPlatformChunkInstall()
{
	static IPlatformChunkInstall* ChunkInstall = nullptr;
	static bool bIniChecked = false;
	if (!ChunkInstall || !bIniChecked)
	{
		FString ProviderName;
		IPlatformChunkInstallModule* PlatformChunkInstallModule = nullptr;
		if (!GEngineIni.IsEmpty())
		{
			FString InstallModule;
			GConfig->GetString(TEXT("StreamingInstall"), TEXT("DefaultProviderName"), InstallModule, GEngineIni);
			FModuleStatus Status;
			if (FModuleManager::Get().QueryModule(*InstallModule, Status))
			{
				PlatformChunkInstallModule = FModuleManager::LoadModulePtr<IPlatformChunkInstallModule>(*InstallModule);
				if (PlatformChunkInstallModule != nullptr)
				{
					// Attempt to grab the platform installer
					ChunkInstall = PlatformChunkInstallModule->GetPlatformChunkInstall();
				}
			}
			bIniChecked = true;
		}
		if (!ChunkInstall)
		{
			// Placeholder instance
			ChunkInstall = FGenericPlatformMisc::GetPlatformChunkInstall();
		}
	}

	return ChunkInstall;
}

void FOpenHarmonyMisc::PrepareMobileHaptics(EMobileHapticsType Type)
{
}

void FOpenHarmonyMisc::TriggerMobileHaptics()
{
#if USE_OPENHARMONY_NAPI
	extern void OpenHarmonyThunkCpp_Vibrate(int32 Duration);
	// directly play a small vibration one-shot
	// note: this will do nothing if device is already playing force feedback (non-zero intensity)
	// but will play and not be cancelled by force feedback since it only sends updates when not already above zero
	OpenHarmonyThunkCpp_Vibrate(10);
#endif
}


void FOpenHarmonyMisc::ReleaseMobileHaptics()
{
}
//TODO：
void FOpenHarmonyMisc::ShareURL(const FString& URL, const FText& Description, int32 LocationHintX, int32 LocationHintY)
{
#if USE_OPENHARMONY_NAPI
	extern void OpenHarmonyThunkCpp_ShareURL(const FString & URL, const FText & Description, const FText & SharePrompt, int32 LocationHintX, int32 LocationHintY);
	OpenHarmonyThunkCpp_ShareURL(URL, Description, NSLOCTEXT("OpenHarmonyMisc", "ShareURL", "Share URL"), LocationHintX, LocationHintY);
#endif
}
FString FOpenHarmonyMisc::LoadTextFileFromPlatformPackage(const FString& RelativePath)
{
	return FString();
}

bool FOpenHarmonyMisc::FileExistsInPlatformPackage(const FString& RelativePath)
{
#if USE_OPENHARMONY_NAPI
	
#endif
	return false;
}



void Napi_nativeSetConfigRulesVariables(napi_env env, napi_callback_info info)
{
	size_t argc = 1;
	napi_value args[1] = {nullptr};
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	napi_value &stringArray = args[0];
	
	uint32_t length = 0;
	napi_get_array_length(env, stringArray, &length);

	uint32_t index = 0;
	napi_value napiString;
	while (index < length)
	{
		napi_get_element(env, stringArray, index++, &napiString);
		auto arktsKey = FArkTsHelper::FStringFromLocalRef(env, napiString);

		napi_get_element(env, stringArray, index++, &napiString);
		auto arktsValue = FArkTsHelper::FStringFromLocalRef(env, napiString);

		FOpenHarmonyMisc::ConfigRulesVariables.Add(arktsKey, arktsValue);
	}
}

void FOpenHarmonyMisc::SetVersionInfo(FString InOpenHarmonyVersion, int32 InMajorVersion, int32 InSdkApiVersion, FString InDeviceMake, FString InDeviceModel, FString InBuildVersion, FString InOSLanguage)
{
	OpenHarmonyVersion = InOpenHarmonyVersion;
	OpenHarmonyMajorVersion = InMajorVersion;
	TargetSDKVersion = InSdkApiVersion;
	DeviceMake = InDeviceMake;
	DeviceModel = InDeviceModel;
	DeviceBuildNumber = InBuildVersion;
	OSLanguage = InOSLanguage;

	UE_LOG(LogOpenHarmony, Display, TEXT("OpenHarmonyVersion:%s,OpenHarmonyMajorVersion:%d,TargetSDKVersion:%d,DeviceMake:%s, DeviceModel:%s,BuildVersion:%s, OpenHarmony Language: %s"),
		*OpenHarmonyVersion, OpenHarmonyMajorVersion, TargetSDKVersion, *DeviceMake, *DeviceModel, *DeviceBuildNumber, *OSLanguage);
}

const FString FOpenHarmonyMisc::GetOpenHarmonyVersion()
{
	return OpenHarmonyVersion;
}

int32 FOpenHarmonyMisc::GetOpenHarmonyMajorVersion()
{
	return OpenHarmonyMajorVersion;
}

int32 FOpenHarmonyMisc::GetTargetSDKVersion()
{
	return TargetSDKVersion;
}

const FString FOpenHarmonyMisc::GetDeviceMake()
{
	return DeviceMake;
}

const FString FOpenHarmonyMisc::GetDeviceModel()
{
	return DeviceModel;
}

const FString FOpenHarmonyMisc::GetDeviceBuildNumber()
{
	return DeviceBuildNumber;
}

const FString FOpenHarmonyMisc::GetOSLanguage()
{
	return OSLanguage;
}

const FString FOpenHarmonyMisc::GetProductName()
{
	return ProductName;
}

const FString FOpenHarmonyMisc::GetProjectVersion() {
	return FString::FromInt(GOpenHarmonyPackageVersion);
}



FString FOpenHarmonyMisc::GetDefaultLocale()
{
	return OSLanguage;
}

bool FOpenHarmonyMisc::GetVolumeButtonsHandledBySystem()
{
	return VolumeButtonsHandledBySystem;
}

void FOpenHarmonyMisc::SetVolumeButtonsHandledBySystem(bool enabled)
{
	VolumeButtonsHandledBySystem = enabled;
}
static bool bForceUnsupported = false;

void FOpenHarmonyMisc::SetForceUnsupported(bool bInOverride)
{
	bForceUnsupported = bInOverride;
}

enum class EDeviceVulkanSupportStatus
{
	Uninitialized,
	NotSupported,
	Supported
};

static FString VulkanVersionString;
static EDeviceVulkanSupportStatus VulkanSupport = EDeviceVulkanSupportStatus::Uninitialized;

static EDeviceVulkanSupportStatus AttemptVulkanInit(void* VulkanLib)
{
	if (VulkanLib == nullptr)
	{
		return EDeviceVulkanSupportStatus::NotSupported;
	}

	return EDeviceVulkanSupportStatus::Supported;
}

bool FOpenHarmonyMisc::HasVulkanDriverSupport()
{
	return true;
}

static void EstablishVulkanDeviceSupport()
{
	// just do this check once
	if (VulkanSupport == EDeviceVulkanSupportStatus::Uninitialized)
	{
		// this call will initialize VulkanSupport
		FOpenHarmonyMisc::HasVulkanDriverSupport();
	}
}
bool FOpenHarmonyMisc::IsDesktopVulkanAvailable()
{
	static int CachedDesktopVulkanAvailable = -1;

	if (CachedDesktopVulkanAvailable == -1)
	{
		CachedDesktopVulkanAvailable = 0;

		bool bSupportsVulkanSM5 = false;

		GConfig->GetBool(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("bSupportsVulkanSM5"), bSupportsVulkanSM5, GEngineIni);

		if (bSupportsVulkanSM5)
		{
			CachedDesktopVulkanAvailable = 1;
		}
	}

	return CachedDesktopVulkanAvailable == 1;
}

bool FOpenHarmonyMisc::IsVulkanAvailable()
{
	//check(VulkanSupport != EDeviceVulkanSupportStatus::Uninitialized);

	static int CachedVulkanAvailable = -1;
	if (CachedVulkanAvailable == -1)
	{
		CachedVulkanAvailable = 0;
		if (true/*VulkanSupport == EDeviceVulkanSupportStatus::Supported*/)//暂时不检测设备
		{
			bool bSupportsVulkan = false;
			GConfig->GetBool(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("bSupportsVulkan"), bSupportsVulkan, GEngineIni);

			// whether to detect Vulkan by default or require the -detectvulkan command line parameter
			bool bDetectVulkanByDefault = true;
			GConfig->GetBool(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("bDetectVulkanByDefault"), bDetectVulkanByDefault, GEngineIni);
			const bool bDetectVulkanCmdLine = FParse::Param(FCommandLine::Get(), TEXT("detectvulkan"));

			// @todo Lumin: Double check all this stuff after merging general OpenHarmony Vulkan SM5 from main
			const bool bSupportsVulkanSM5 = IsDesktopVulkanAvailable();

			const bool bVulkanDisabledCmdLine = FParse::Param(FCommandLine::Get(), TEXT("GL")) || FParse::Param(FCommandLine::Get(), TEXT("OpenGL"));

			if (!FModuleManager::Get().ModuleExists(TEXT("VulkanRHI")))
			{
				UE_LOG(LogTemp, Log, TEXT("Vulkan not available as VulkanRHI not present."));
			}
			else if (!(bSupportsVulkan || bSupportsVulkanSM5))
			{
				UE_LOG(LogTemp, Log, TEXT("Vulkan not available as project packaged without bSupportsVulkan or bSupportsVulkanSM5."));
			}
			else if (bVulkanDisabledCmdLine)
			{
				UE_LOG(LogTemp, Log, TEXT("Vulkan API detection is disabled by a command line option."));
			}
			else if (false/*!bDetectVulkanByDefault && !bDetectVulkanCmdLine*/)//暂时不detect
			{
				UE_LOG(LogTemp, Log, TEXT("Vulkan available but detection disabled by bDetectVulkanByDefault=False in OpenHarmonyRuntimeSettings. Use -detectvulkan to override."));
			}
			else
			{
				CachedVulkanAvailable = 1;
			}
		}
	}

	return CachedVulkanAvailable == 1;
}


bool FOpenHarmonyMisc::ShouldUseVulkan()
{
	//check(VulkanSupport != EDeviceVulkanSupportStatus::Uninitialized);
	static int CachedShouldUseVulkan = -1;

	if (CachedShouldUseVulkan == -1)
	{
		CachedShouldUseVulkan = 0;

		//static const auto CVarDisableVulkan = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OpenHarmony.DisableVulkanSupport"));

		const bool bVulkanAvailable = IsVulkanAvailable();
		UE_LOG(LogTemp, Warning, TEXT("hmsbvulkanavailable = %d"), IsVulkanAvailable());
		//const bool bVulkanDisabledCVar = CVarDisableVulkan->GetValueOnAnyThread() == 1;

		if (bVulkanAvailable /* && !bVulkanDisabledCVar */)
		{
			CachedShouldUseVulkan = 1;
			UE_LOG(LogTemp, Log, TEXT("VulkanRHI will be used!"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("VulkanRHI will NOT be used:"));
			if (!bVulkanAvailable)
			{
				UE_LOG(LogTemp, Log, TEXT(" ** Vulkan support is not available (Driver, RHI or shaders are missing, or disabled by cmdline, see above logging for details)"));
			}
			/*
			if (bVulkanDisabledCVar)
			{
				UE_LOG(LogTemp, Log, TEXT(" ** Vulkan is disabled via console variable."));
			}*/
			UE_LOG(LogTemp, Log, TEXT("OpenGL ES will be used."));
		}
	}

	return CachedShouldUseVulkan == 1;
}

bool FOpenHarmonyMisc::ShouldUseDesktopVulkan()
{
	static int CachedShouldUseDesktopVulkan = -1;

	if (CachedShouldUseDesktopVulkan == -1)
	{
		CachedShouldUseDesktopVulkan = 0;

		const bool bVulkanSM5Enabled = IsDesktopVulkanAvailable();

		//static const auto CVarDisableVulkanSM5 = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OpenHarmony.DisableVulkanSM5Support"));
		//const bool bVulkanSM5Disabled = CVarDisableVulkanSM5->GetValueOnAnyThread() == 1;

		if (bVulkanSM5Enabled /* && !bVulkanSM5Disabled*/)
		{
			CachedShouldUseDesktopVulkan = 1;
			UE_LOG(LogTemp, Log, TEXT("Vulkan SM5 RHI will be used!"));
		}
		/*
		else if (bVulkanSM5Disabled)
		{
			UE_LOG(LogTemp, Log, TEXT("Vulkan SM5 is available but disabled for this device."));
		}*/
		else if (!bVulkanSM5Enabled)
		{
			UE_LOG(LogTemp, Log, TEXT("** Vulkan SM5 support is not available (Driver, RHI or shaders are missing, or disabled by cmdline, see above logging for details)"));
		}
	}

	return CachedShouldUseDesktopVulkan;
}


FString FOpenHarmonyMisc::GetVulkanVersion()
{
	//check(VulkanSupport != EDeviceVulkanSupportStatus::Uninitialized);
	return VulkanVersionString;
}

const TMap<FString, FString>& FOpenHarmonyMisc::GetConfigRulesTMap()
{
	return ConfigRulesVariables;
}

FString* FOpenHarmonyMisc::GetConfigRulesVariable(const FString& Key)
{
	return ConfigRulesVariables.Find(Key);
}

bool FOpenHarmonyMisc::AllowThreadHeartBeat()
{
	static uint32 AllowThreadHeartBeatOnce = -1;
	if (AllowThreadHeartBeatOnce == -1)
	{
		const FString* AllowThreadHeartBeatConfigVar = GetConfigRulesVariable(TEXT("EnableThreadHeartBeat"));
		if (AllowThreadHeartBeatConfigVar)
		{
			AllowThreadHeartBeatOnce = (uint32)AllowThreadHeartBeatConfigVar->Equals("true", ESearchCase::IgnoreCase);
		}
		else
		{
			AllowThreadHeartBeatOnce = 0;
		}
	}

	return AllowThreadHeartBeatOnce == 1;
}
//TODO
//extern bool OpenHarmonyThunkCpp_HasMetaDataKey(const FString& Key);
static bool bDetectedDebugger = false;

// JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeSetOpenHarmonyStartupState(JNIEnv* jenv, jobject thiz, jboolean bDebuggerAttached)
// {
// 	// if Java debugger attached, mark detected (but don't lose previous trigger state)
// 	if (bDebuggerAttached)
// 	{
// 		bDetectedDebugger = true;
// 	}
// }
#if !UE_BUILD_SHIPPING
bool FOpenHarmonyMisc::IsDebuggerPresent()
{
	extern CORE_API bool GIgnoreDebugger;
	if (GIgnoreDebugger)
	{
		return false;
	}
	if (bDetectedDebugger)
	{
		return true;
	}
	// If a process is tracing this one then TracerPid in /proc/self/status will
	// be the id of the tracing process. Use SignalHandler safe functions
	int StatusFile = open("/proc/self/status", O_RDONLY);
	if (StatusFile == -1)
	{
		// Failed - unknown debugger status.
		return false;
	}
	char Buffer[256];
	ssize_t Length = read(StatusFile, Buffer, sizeof(Buffer));
	bool bDebugging = false;
	const char* TracerString = "TracerPid:\t";
	const ssize_t LenTracerString = strlen(TracerString);
	int i = 0;
	while ((Length - i) > LenTracerString)
	{
		// TracerPid is found
		if (strncmp(&Buffer[i], TracerString, LenTracerString) == 0)
		{
			// 0 if no process is tracing.
			bDebugging = Buffer[i + LenTracerString] != '0';
			break;
		}
		++i;
	}
	close(StatusFile);
	// remember if we detected debugger so we can skip check next time
	if (bDebugging)
	{
		bDetectedDebugger = true;
	}
	return bDebugging;
}
#endif
#if STATS || ENABLE_STATNAMEDEVENTS
void FOpenHarmonyMisc::BeginNamedEventFrame()
{
#if FRAMEPRO_ENABLED
	FFrameProProfiler::FrameStart();
#endif // FRAMEPRO_ENABLED
}

static void WriteTraceMarkerEvent(const ANSICHAR* Text, int32 TraceMarkerFileDescriptor)
{
	if (bUseNativeSystrace)
	{
		OH_HiTrace_StartTrace(Text);
	}
	else
	{
		const int MAX_TRACE_EVENT_LENGTH = 256;
		ANSICHAR EventBuffer[MAX_TRACE_EVENT_LENGTH];
		int EventLength = snprintf(EventBuffer, MAX_TRACE_EVENT_LENGTH, "B|%d|%s", getpid(), Text);

		write(TraceMarkerFileDescriptor, EventBuffer, EventLength);
	}
}

void FOpenHarmonyMisc::BeginNamedEvent(const struct FColor& Color, const TCHAR* Text)
{
#if FRAMEPRO_ENABLED
	FFrameProProfiler::PushEvent(Text);
#endif // FRAMEPRO_ENABLED
	if (bUseNativeSystrace ? false : TraceMarkerFileDescriptor == -1)
	{
		return;
	}

	const int MAX_TRACE_MESSAGE_LENGTH = 256;

	// not static since may be called by different threads
	ANSICHAR TextBuffer[MAX_TRACE_MESSAGE_LENGTH];

	const TCHAR* SourcePtr = Text;
	ANSICHAR* WritePtr = TextBuffer;
	int32 RemainingSpace = MAX_TRACE_MESSAGE_LENGTH;
	while (*SourcePtr && --RemainingSpace > 0)
	{
		*WritePtr++ = static_cast<ANSICHAR>(*SourcePtr++);
	}
	*WritePtr = '\0';

	WriteTraceMarkerEvent(TextBuffer, TraceMarkerFileDescriptor);
}

void FOpenHarmonyMisc::BeginNamedEvent(const struct FColor& Color, const ANSICHAR* Text)
{
#if FRAMEPRO_ENABLED
	FFrameProProfiler::PushEvent(Text);
#endif // FRAMEPRO_ENABLED
	if (bUseNativeSystrace ? false : TraceMarkerFileDescriptor == -1)
	{
		return;
	}

	WriteTraceMarkerEvent(Text, TraceMarkerFileDescriptor);
}

void FOpenHarmonyMisc::EndNamedEvent()
{
#if FRAMEPRO_ENABLED
	FFrameProProfiler::PopEvent();
#endif // FRAMEPRO_ENABLED
	if (bUseNativeSystrace ? false : TraceMarkerFileDescriptor == -1)
	{
		return;
	}

	if (bUseNativeSystrace)
	{
		OH_HiTrace_FinishTrace();
	}
	else
	{
		const ANSICHAR EventTerminatorChar = 'E';
		write(TraceMarkerFileDescriptor, &EventTerminatorChar, 1);
	}
}

void FOpenHarmonyMisc::CustomNamedStat(const TCHAR* Text, float Value, const TCHAR* Graph, const TCHAR* Unit)
{
	FRAMEPRO_DYNAMIC_CUSTOM_STAT(TCHAR_TO_WCHAR(Text), Value, TCHAR_TO_WCHAR(Graph), TCHAR_TO_WCHAR(Unit), FRAMEPRO_COLOUR(255,255,255) );
}

void FOpenHarmonyMisc::CustomNamedStat(const ANSICHAR* Text, float Value, const ANSICHAR* Graph, const ANSICHAR* Unit)
{
	FRAMEPRO_DYNAMIC_CUSTOM_STAT(Text, Value, Graph, Unit, FRAMEPRO_COLOUR(255,255,255));
}
#endif // STATS || ENABLE_STATNAMEDEVENTS
 
int FOpenHarmonyMisc::GetVolumeState(double* OutTimeOfChangeInSec)
{
	int v;
	ReceiversLock.Lock();
	v = CurrentVolume.Volume;
	if (OutTimeOfChangeInSec)
	{
		*OutTimeOfChangeInSec = CurrentVolume.TimeOfChange;
	}
	ReceiversLock.Unlock();
	return v;
}

int32 FOpenHarmonyMisc::GetDeviceVolume()
{
	//FOpenHarmonyMisc::GetVolumeState returns 0-15, scale to 0-100
	int32 BaseVolume = FOpenHarmonyMisc::GetVolumeState();
	int32 ScaledVolume = (BaseVolume * 100) / 15;
	return ScaledVolume;
}

#if USE_OPENHARMONY_FILE
const TCHAR* FOpenHarmonyMisc::GamePersistentDownloadDir()
{
	extern FString GFilePathBase;
	return *GFilePathBase;
}
//write by weicheng
FString FOpenHarmonyMisc::GetLoginId()   // 获取UUID
{
	static FString LoginId = TEXT("");

	// Return already loaded or generated Id
	if (!LoginId.IsEmpty())
	{
		return LoginId;
	}

	// Check for existing identifier file
	extern FString GInternalFilePath;
	extern FString GExternalFilePath;
	FString InternalLoginIdFilename = GInternalFilePath / TEXT("login-identifier.txt");
	if (FPaths::FileExists(InternalLoginIdFilename))
	{
		if (FFileHelper::LoadFileToString(LoginId, *InternalLoginIdFilename))
		{
			return LoginId;
		}
	}
	FString LoginIdFilename = GExternalFilePath / TEXT("login-identifier.txt");
	if (FPaths::FileExists(LoginIdFilename))
	{
		if (FFileHelper::LoadFileToString(LoginId, *LoginIdFilename))
		{
			FFileHelper::SaveStringToFile(LoginId, *InternalLoginIdFilename);
			return LoginId;
		}
	}

	// Generate a new one and write to file
	FGuid DeviceGuid;
	FPlatformMisc::CreateGuid(DeviceGuid);
	LoginId = DeviceGuid.ToString();
	FFileHelper::SaveStringToFile(LoginId, *InternalLoginIdFilename);

	return LoginId;
}
#endif
//get device ID
FString FOpenHarmonyMisc::GetDeviceId()
{
#if GET_DEVICE_ID_UNAVAILABLE
	return FString();
#else
	extern FString OpenHarmonyThunkCpp_GetOpenHarmonyId();
	static FString DeviceId = OpenHarmonyThunkCpp_GetOpenHarmonyId();

	// note: this can be empty or NOT unique depending on the OEM implementation!
	return DeviceId;
#endif
}
//GetUniqueAdvertisingId
FString FOpenHarmonyMisc::GetUniqueAdvertisingId()
{
	extern FString OpenHarmonyThunkCpp_GetAdvertisingId();
	static FString AdvertisingId = OpenHarmonyThunkCpp_GetAdvertisingId();

	// note: this can be empty if Google Play not installed, or user is blocking it!
	return AdvertisingId;
}
extern float GetBatteryTemperature();
extern int GetBatteryChargeState();
extern int GetBatterySoC();

FOpenHarmonyMisc::FBatteryState FOpenHarmonyMisc::GetBatteryState()
{
	if (GEnableReceivers == true) {
		CurrentBatteryState.Temperature = GetBatteryTemperature();
		UE_LOG(LogOpenHarmony, Display, TEXT("CurrentBatteryState.Temperature is %f"),CurrentBatteryState.Temperature);

		CurrentBatteryState.State = (FOpenHarmonyMisc::EBatteryState)GetBatteryChargeState();
		UE_LOG(LogOpenHarmony, Display, TEXT("CurrentBatteryState.State is %d"), CurrentBatteryState.State);

		CurrentBatteryState.Level = GetBatterySoC();
		UE_LOG(LogOpenHarmony, Display, TEXT("CurrentBatteryState.Level is %d"), CurrentBatteryState.Level);
	}

	return CurrentBatteryState;
}

int FOpenHarmonyMisc::GetBatteryLevel()
{
	FBatteryState BatteryState = GetBatteryState();
	return BatteryState.Level;
}

bool FOpenHarmonyMisc::IsRunningOnBattery()
{
	FBatteryState BatteryState = GetBatteryState();
	return BatteryState.State == BATTERY_STATE_DISABLE;
}

bool FOpenHarmonyMisc::IsInLowPowerMode()
{
	FBatteryState BatteryState = GetBatteryState();
	return BatteryState.Level <= GOpenHarmonyLowPowerBatteryThreshold;
}

float FOpenHarmonyMisc::GetDeviceTemperatureLevel()
{
	return GetBatteryState().Temperature;
}

#define OPENHARMONY_CONNECTION_TYPE_NONE -1
#define OPENHARMONY_CONNECTION_TYPE_BEARER_CELLULAR 0
#define OPENHARMONY_CONNECTION_TYPE_BEARER_WIFI 1
#define OPENHARMONY_CONNECTION_TYPE_BEARER_ETHERNET 3

static bool bLastConnectionTypeValid = false;

ENetworkConnectionType FOpenHarmonyMisc::GetNetworkConnectionType()
{
	extern int OpenHarmonyThunkCpp_GetNetworkConnectionType();

	switch (OpenHarmonyThunkCpp_GetNetworkConnectionType())
	{
	case OPENHARMONY_CONNECTION_TYPE_NONE:				return ENetworkConnectionType::None;
	case OPENHARMONY_CONNECTION_TYPE_BEARER_ETHERNET:	return ENetworkConnectionType::Ethernet;
	case OPENHARMONY_CONNECTION_TYPE_BEARER_CELLULAR:	return ENetworkConnectionType::Cell;
	case OPENHARMONY_CONNECTION_TYPE_BEARER_WIFI:		return ENetworkConnectionType::WiFi;
	}

	return ENetworkConnectionType::Unknown;
}


bool FOpenHarmonyMisc::HasActiveWiFiConnection()
{
	ENetworkConnectionType ConnectionType = GetNetworkConnectionType();
	return ConnectionType == ENetworkConnectionType::WiFi;
}
static FOpenHarmonyMisc::ReInitWindowCallbackType OnReInitWindowCallback;

FOpenHarmonyMisc::ReInitWindowCallbackType FOpenHarmonyMisc::GetOnReInitWindowCallback()
{
	return OnReInitWindowCallback;
}

void FOpenHarmonyMisc::SetOnReInitWindowCallback(FOpenHarmonyMisc::ReInitWindowCallbackType InOnReInitWindowCallback)
{
	OnReInitWindowCallback = InOnReInitWindowCallback;
}

static FOpenHarmonyMisc::ReleaseWindowCallbackType OnReleaseWindowCallback;

FOpenHarmonyMisc::ReleaseWindowCallbackType FOpenHarmonyMisc::GetOnReleaseWindowCallback()
{
	return OnReleaseWindowCallback;
}

void FOpenHarmonyMisc::SetOnReleaseWindowCallback(FOpenHarmonyMisc::ReleaseWindowCallbackType InOnReleaseWindowCallback)
{
	OnReleaseWindowCallback = InOnReleaseWindowCallback;
}

static FOpenHarmonyMisc::OnPauseCallBackType OnPauseCallback;
FOpenHarmonyMisc::OnPauseCallBackType FOpenHarmonyMisc::GetOnPauseCallback()
{
	return OnPauseCallback;
}
void FOpenHarmonyMisc::SetOnPauseCallback(FOpenHarmonyMisc::OnPauseCallBackType InOnPauseCallback)
{
	OnPauseCallback = InOnPauseCallback;
}
FString FOpenHarmonyMisc::GetCPUVendor() // 获取CPU供应商
{
	return DeviceMake;
}

FString FOpenHarmonyMisc::GetCPUBrand()  // 获取CPU型号
{
	return DeviceModel;
}

FString FOpenHarmonyMisc::GetCPUChipset()
{
	static FString* Chipset = FOpenHarmonyMisc::GetConfigRulesVariable(TEXT("hardware"));
	return (Chipset == NULL) ? FGenericPlatformMisc::GetCPUChipset() : *Chipset;
}

FString FOpenHarmonyMisc::GetPrimaryGPUBrand()
{
	return FOpenHarmonyMisc::GetGPUFamily();
}

void FOpenHarmonyMisc::GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel) // 获取OS版本
{
	out_OSVersionLabel = TEXT("OpenHarmony");
	out_OSSubVersionLabel = OpenHarmonyVersion;
}

FString FOpenHarmonyMisc::GetOSVersion()  // 获取OS版本
{
	return OpenHarmonyVersion;
}

bool FOpenHarmonyMisc::GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes)
{
#if USE_OPENHARMONY_FILE
	extern FString GExternalFilePath;
	struct statvfs diskInfo;
	FTCHARToUTF8 Converter(*GExternalFilePath);
	int ret = statvfs((ANSICHAR*)Converter.Get(), &diskInfo);
	if (ret == 0)
	{
		NumberOfFreeBytes = static_cast<uint64>(diskInfo.f_bsize) * static_cast<uint64>(diskInfo.f_bfree);
		TotalNumberOfBytes = static_cast<uint64>(diskInfo.f_bsize) * static_cast<uint64>(diskInfo.f_blocks);
	}
	else
	{
		UE_LOG(LogOpenHarmony, Warning, TEXT("Unable to statfs(): errno=%d (%s)"), ret, UTF8_TO_TCHAR(strerror(ret)));
	}
	return (ret == 0);
#else
	return false;
#endif
}



uint32 FOpenHarmonyMisc::GetCoreFrequency(int32 CoreIndex, ECoreFrequencyProperty CoreFrequencyProperty)  // 获取CPU核心频率
{
	uint32 ReturnFrequency = 0;
	char QueryFile[256];
	const char* FreqProperty = nullptr;
	static const char* CurrentFrequencyString = "scaling_cur_freq";
	static const char* MaxFrequencyString = "cpuinfo_max_freq";
	static const char* MinFrequencyString = "cpuinfo_min_freq";
	switch (CoreFrequencyProperty)
	{
	case ECoreFrequencyProperty::MaxFrequency:
		FreqProperty = MaxFrequencyString;
		break;
	case ECoreFrequencyProperty::MinFrequency:
		FreqProperty = MinFrequencyString;
		break;
	default:
	case ECoreFrequencyProperty::CurrentFrequency:
		FreqProperty = CurrentFrequencyString;
		break;
	}
	sprintf(QueryFile, "/sys/devices/system/cpu/cpu%d/cpufreq/%s", CoreIndex, FreqProperty);

	if (FILE* CoreFreqStateFile = fopen(QueryFile, "r"))
	{
		char CurrCoreFreq[32] = { 0 };
		if( fgets(CurrCoreFreq, UE_ARRAY_COUNT(CurrCoreFreq), CoreFreqStateFile) != nullptr)
		{
			ReturnFrequency = atol(CurrCoreFreq);
		}
		fclose(CoreFreqStateFile);
	}
	return ReturnFrequency;
}

float FOpenHarmonyMisc::GetCPUTemperature()
{
	float Temp = 0.0f;
	if (*OpenHarmonyCpuThermalSensorFileBuf == 0)
	{
		return Temp;
	}
	if (FILE* Thermals = fopen(OpenHarmonyCpuThermalSensorFileBuf, "r"))
	{
		char Buf[256];
		if (fgets(Buf, UE_ARRAY_COUNT(Buf), Thermals))
		{
			// sensor temp file can contain whitespace symbols at the end of the line, count length only for digit symbols
			char* p = Buf;
			uint32 Len = 0;
			while (isdigit(*p))
			{
				++Len;
				++p;
			}
			// Temperature is reported by different sensors in different ways, some report it as XXX, some - as XXXXX. Reduce it to standard XX.X
			const uint32 StandardLen = 2;
			const float Divider = pow(10.0f, (float)(Len - StandardLen));
			Temp = (float)atol(Buf) / Divider;
		}
		fclose(Thermals);
	}
	return Temp;
}
void FOpenHarmonyMisc::UpdateDeviceOrientation()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FOpenHarmonyMisc_UpdateDeviceOrientation);
	//TODO:
}
bool FOpenHarmonyMisc::Expand16BitIndicesTo32BitOnLoad()
{
	return  (CVarMaliMidgardIndexingBug.GetValueOnAnyThread() > 0);
}

int FOpenHarmonyMisc::GetMobilePropagateAlphaSetting()
{
	extern int GOpenHarmonyPropagateAlpha; // added into openharmonywindow.h
	return GOpenHarmonyPropagateAlpha;
}

TArray<int32> FOpenHarmonyMisc::GetSupportedNativeDisplayRefreshRates()
{
	TArray<int32> Result;
	//#if USE_OpenHarmony_NAPI
	//	extern TArray<int32> OpenHarmonyThunkCpp_GetSupportedNativeDisplayRefreshRates();
	//	Result = OpenHarmonyThunkCpp_GetSupportedNativeDisplayRefreshRates();
	//#else
	//	Result.Add(60);
	//#endif
	int32 CurrentRefreshRate = FOpenHarmonyMisc::GetNativeDisplayRefreshRate(); // TODO: Real supported refresh rate
	Result.Add(CurrentRefreshRate);
	if (CurrentRefreshRate != 60)
	{
		Result.Add(60);
	}

	return Result;
}

bool FOpenHarmonyMisc::SetNativeDisplayRefreshRate(int32 RefreshRate)
{
	//#if USE_OpenHarmony_JNI
	//	extern bool OpenHarmonyThunkCpp_SetNativeDisplayRefreshRate(int32 RefreshRate);
	//	return OpenHarmonyThunkCpp_SetNativeDisplayRefreshRate(RefreshRate);
	//#else
	//	return RefreshRate == 60;
	//#endif
	return RefreshRate == 60;
}

int32 FOpenHarmonyMisc::GetNativeDisplayRefreshRate()
{
	#if USE_OPENHARMONY_NAPI
		extern float GetNativeDisplayRefreshRate();
		return int32(GetNativeDisplayRefreshRate());
	#else
		return 60;
	#endif
	//return 60;
}


double FOpenHarmonyMisc::GetDevicePhysicalSizeInInch()
{
#if USE_OPENHARMONY_NAPI
	extern double OpenHarmonyThunkCpp_GetDevicePhysicalSizeInInch();
	return OpenHarmonyThunkCpp_GetDevicePhysicalSizeInInch();
#else
	return 0.0;
#endif
}

FORCEINLINE bool ValueOutsideThreshold(float Value, float BaseLine, float Threshold)
{
	return Value > BaseLine * (1.0f + Threshold)
		|| Value < BaseLine * (1.0f - Threshold);
}

void (*GMemoryWarningHandler)(const FGenericMemoryWarningContext& Context) = NULL;

void FOpenHarmonyMisc::SetMemoryWarningHandler(void (*InHandler)(const FGenericMemoryWarningContext& Context))
{
	/*check(IsInGameThread());
	GMemoryWarningHandler = InHandler;*/
}

bool FOpenHarmonyMisc::HasMemoryWarningHandler()
{
	return false;
	//check(IsInGameThread());
	//return GMemoryWarningHandler != nullptr;
}

bool FOpenHarmonyMisc::SupportsBackbufferSampling()
{
	static int32 CachedOpenHarmonyOpenGLSupportsBackbufferSampling = -1;
	
	if (CachedOpenHarmonyOpenGLSupportsBackbufferSampling == -1)
	{
		bool bOpenHarmonyOpenGLSupportsBackbufferSampling = false;
		GConfig->GetBool(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("bOpenHarmonyOpenGLSupportsBackbufferSampling"), bOpenHarmonyOpenGLSupportsBackbufferSampling, GEngineIni);

		CachedOpenHarmonyOpenGLSupportsBackbufferSampling = (bOpenHarmonyOpenGLSupportsBackbufferSampling || FOpenHarmonyMisc::ShouldUseVulkan()) ? 1 : 0;
	}

	return CachedOpenHarmonyOpenGLSupportsBackbufferSampling == 1;

}

void FOpenHarmonyMisc::NonReentrantRequestExit()
{
#if UE_SET_REQUEST_EXIT_ON_TICK_ONLY
	// Cheating here to grab access to this. This function should only be used in extreme cases in which non-reentrant functions are needed (ie. crash handling/signal handler)
	extern bool GShouldRequestExit;
	GShouldRequestExit = true;
#else
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		GIsRequestingExit = true;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // UE_SET_REQUEST_EXIT_ON_TICK_ONLY
}

void FOpenHarmonyMisc::RegisterThreadName(const char* Name, uint32 ThreadId)
{
	FScopeLock Lock(&OpenHarmonyThreadNamesLock);
	if (!OpenHarmonyThreadNames.Contains(ThreadId))
	{
		OpenHarmonyThreadNames.Add(ThreadId, Name);
	}
}

const char* FOpenHarmonyMisc::GetThreadName(uint32 ThreadId)
{
	FScopeLock Lock(&OpenHarmonyThreadNamesLock);
	const char** ThreadName = OpenHarmonyThreadNames.Find(ThreadId);
	return ThreadName ? *ThreadName : nullptr;
}

void FOpenHarmonyMisc::SetDeviceOrientation(EDeviceScreenOrientation NewDeviceOrentation)
{
	SetAllowedDeviceOrientation(NewDeviceOrentation);
}


void FOpenHarmonyMisc::SetCellularPreference(int32 Value)
{
#if USE_OPENHARMONY_NAPI
	// todo l00845087
	OpenHarmonyThunkCpp_SetCellularPreference(Value); //待实现
#endif // USE_OPENHARMONY_NAPI
}

int32 FOpenHarmonyMisc::GetCellularPreference()
{
	int32 value = 0;
#if USE_OPENHARMONY_NAPI
	// todo l00845087
	value = OpenHarmonyThunkCpp_GetCellularPreference();//待实现
#endif // USE_OPENHARMONY_NAPI
	return value;
}


void FOpenHarmonyMisc::SetAllowedDeviceOrientation(EDeviceScreenOrientation NewAllowedDeviceOrientation)
{
	AllowedDeviceOrientation = NewAllowedDeviceOrientation;

#if USE_OPENHARMONY_NAPI
	OpenHarmonyThunkCpp_SetOrientation(GetOpenHarmonyScreenOrientation(NewAllowedDeviceOrientation));
#endif // USE_OPENHARMONY_NAPI
}

#if USE_OPENHARMONY_NAPI
int32 FOpenHarmonyMisc::GetOpenHarmonyScreenOrientation(EDeviceScreenOrientation ScreenOrientation)
{
	EOpenharmonyScreenOrientation OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_UNSPECIFIED;
	switch (ScreenOrientation)
	{
	case EDeviceScreenOrientation::Unknown:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_UNSPECIFIED;
		break;
	case EDeviceScreenOrientation::Portrait:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_PORTRAIT;
		break;
	case EDeviceScreenOrientation::PortraitUpsideDown:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_PORTRAIT_INVERTED;
		break;
	case EDeviceScreenOrientation::LandscapeLeft:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_LANDSCAPE;
		break;
	case EDeviceScreenOrientation::LandscapeRight:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_LANDSCAPE_INVERTED;
		break;
	case EDeviceScreenOrientation::FaceUp:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_UNSPECIFIED;
		break;
	case EDeviceScreenOrientation::FaceDown:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_UNSPECIFIED;
		break;
	case EDeviceScreenOrientation::PortraitSensor:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_AUTO_ROTATION_PORTRAIT;
		break;
	case EDeviceScreenOrientation::LandscapeSensor:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_AUTO_ROTATION_LANDSCAPE;
		break;
	case EDeviceScreenOrientation::FullSensor:
		OpenharmonyScreenOrientation = EOpenharmonyScreenOrientation::SCREEN_ORIENTATION_AUTO_ROTATION;
		break;
	}

	return static_cast<int32>(OpenharmonyScreenOrientation);
}

extern void OpenHarmonyThunkCpp_ShowConsoleWindow();

void FOpenHarmonyMisc::ShowConsoleWindow()
{
#if !UE_BUILD_SHIPPING && USE_OPENHARMONY_NAPI 
	OpenHarmonyThunkCpp_ShowConsoleWindow();
#endif // !UE_BUILD_SHIPPING && USE_ANDROID_JNI
}

FDelegateHandle FOpenHarmonyMisc::AddNetworkListener(FCoreDelegates::FOnNetworkConnectionChanged::FDelegate&& InNewDelegate)
{
	// not really necessary since PlatformInit calls AddNetworkListener but doesn't hurt anything
	if (!FCoreDelegates::OnNetworkConnectionChanged.IsBound())
	{
#if USE_OPENHARMONY_NAPI 
		extern void OpenHarmonyThunkCpp_AddNetworkListener(); //待实现
		OpenHarmonyThunkCpp_AddNetworkListener();
#endif
	}
	return FCoreDelegates::OnNetworkConnectionChanged.Add(MoveTemp(InNewDelegate));
}

bool FOpenHarmonyMisc::RemoveNetworkListener(FDelegateHandle Handle)
{
	bool bSuccess = FCoreDelegates::OnNetworkConnectionChanged.Remove(Handle);

	// we don't really want to remove listener since we're using it for GetNetworkConnectionType
	if (!FCoreDelegates::OnNetworkConnectionChanged.IsBound())
	{
#if USE_OPENHARMONY_NAPI
		extern void OpenHarmonyThunkCpp_RemoveNetworkListener(); //待实现
		OpenHarmonyThunkCpp_RemoveNetworkListener();//待实现
#endif
	}

	return bSuccess;
}
#endif // USE_OPENHARMONY_NAPI && standalone?