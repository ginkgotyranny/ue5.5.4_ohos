#pragma once
#include "CoreTypes.h"
#include "OpenHarmony/OpenHarmonySystemIncludes.h"
#include "GenericPlatform/GenericPlatformMisc.h"

template <typename FuncType>
class TFunction;

#if PLATFORM_OPENHARMONY_ARM64
	#define PLATFORM_BREAK()	__asm__(".inst 0xd4200000")
#elif PLATFORM_OPENHARMONY_ARM
	#define PLATFORM_BREAK()	__asm__("trap")
#else
	#define PLATFORM_BREAK()	__asm__("int $3")
#endif

#define UE_DEBUG_BREAK_IMPL()	PLATFORM_BREAK()
enum class ECrashContextType; 
/**
 * OpenHarmony implementation of the misc OS functions
 */
struct FOpenHarmonyMisc : public FGenericPlatformMisc
{
	static CORE_API void RequestExit( bool Force, const TCHAR* CallSite = nullptr);
	static CORE_API bool RestartApplication();  
	static CORE_API void LocalPrint(const TCHAR* Message);

	static bool IsLocalPrintThreadSafe() { return true; }
	static CORE_API void SetThreadName(const char* name);
	static CORE_API void PlatformPreInit();
	static CORE_API void PlatformInit();
	static CORE_API void PlatformTearDown();
	static CORE_API void PlatformHandleSplashScreen(bool ShowSplashScreen);
	static EDeviceScreenOrientation GetDeviceOrientation() { return DeviceOrientation; }
	UE_DEPRECATED(5.1, "SetDeviceOrientation is deprecated. Use SetAllowedDeviceOrientation instead.")
	static CORE_API void SetDeviceOrientation(EDeviceScreenOrientation NewDeviceOrentation);
	static CORE_API void SetAllowedDeviceOrientation(EDeviceScreenOrientation NewAllowedDeviceOrientation);

	// Change this to an Enum with Always allow, allow and deny
	static CORE_API void SetCellularPreference(int32 Value);
	static CORE_API int32 GetCellularPreference();

	FORCEINLINE static int32 GetMaxPathLength()
	{
		return OPENHARMONY_MAX_PATH;
	}

	UE_DEPRECATED(4.21, "void FPlatformMisc::GetEnvironmentVariable(Name, Result, Length) is deprecated. Use FString FPlatformMisc::GetEnvironmentVariable(Name) instead.")
	static CORE_API void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength);

	static CORE_API FString GetEnvironmentVariable(const TCHAR* VariableName);
	static CORE_API const TCHAR* GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error);
	static CORE_API EAppReturnType::Type MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption);
	static CORE_API bool UseRenderThread();
	static CORE_API bool HasPlatformFeature(const TCHAR* FeatureName);
	static CORE_API bool SupportsES30();
public:

	static CORE_API bool AllowThreadHeartBeat();

	struct FCPUStatTime {
		uint64_t			TotalTime;
		uint64_t			UserTime;
		uint64_t			NiceTime;
		uint64_t			SystemTime;
		uint64_t			SoftIRQTime;
		uint64_t			IRQTime;
		uint64_t			IdleTime;
		uint64_t			IOWaitTime;
	};

	struct FCPUState
	{
		const static int32			    MaxSupportedCores = 16; //Core count 16 is maximum for now
		int32						    CoreCount;
		int32						    ActivatedCoreCount;
		ANSICHAR					    Name[6];
		FOpenHarmonyMisc::FCPUStatTime	CurrentUsage[MaxSupportedCores];
		FOpenHarmonyMisc::FCPUStatTime	PreviousUsage[MaxSupportedCores];
		int32						    Status[MaxSupportedCores];
		double						    Utilization[MaxSupportedCores];
		double						    AverageUtilization;

	};

	static CORE_API FCPUState& GetCPUState();
	static CORE_API int32 NumberOfCores();
	static CORE_API int32 NumberOfCoresIncludingHyperthreads();
	static CORE_API bool SupportsLocalCaching();
	//static CORE_API void CreateGuid(struct FGuid& Result);
	static CORE_API void SetCrashHandler(void (*CrashHandler)(const FGenericCrashContext& Context));
	// NOTE: THIS FUNCTION IS DEFINED IN OPENHARMONYOPENGL.CPP
	static CORE_API void GetValidTargetPlatforms(class TArray<class FString>& TargetPlatformNames);
	static CORE_API bool GetUseVirtualJoysticks();
	static CORE_API bool SupportsTouchInput();
	static const TCHAR* GetDefaultDeviceProfileName() { return TEXT("OpenHarmony_Default"); }
	static CORE_API bool GetVolumeButtonsHandledBySystem();
	static CORE_API void SetVolumeButtonsHandledBySystem(bool enabled);
	// Returns current volume, 0-15
	static CORE_API int GetVolumeState(double* OutTimeOfChangeInSec = nullptr);

	static CORE_API int32 GetDeviceVolume();


#if USE_OPENHARMONY_FILE
	static const TCHAR* GamePersistentDownloadDir();
	static FString GetLoginId();
#endif
//#if USE_ANDROID_JNI
	static CORE_API FString GetDeviceId();
	static CORE_API FString GetUniqueAdvertisingId();
//#end if
	static CORE_API FString GetCPUVendor();
	static CORE_API FString GetCPUBrand();
	static CORE_API FString GetCPUChipset();
	static CORE_API FString GetPrimaryGPUBrand();
	static CORE_API void GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel);
	static CORE_API bool GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes);
	enum EBatteryState
	{
		BATTERY_STATE_NONE = 0,
		BATTERY_STATE_ENABLE,
		BATTERY_STATE_DISABLE,
		BATTERY_STATE_FULL
	};
	enum EBatteryCapacityLevel
	{
		BATTERY_STATE_LEVEL_FULL = 1,
		BATTERY_STATE_LEVEL_HIGH,
		BATTERY_STATE_LEVEL_NORMAL,
		BATTERY_STATE_LEVEL_LOW,
		BATTERY_STATE_LEVEL_WARNING,
		BATTERY_STATE_LEVEL_CRITICAL,
		BATTERY_STATE_LEVEL_SHUTDOWN
	};
	struct FBatteryState
	{
		FOpenHarmonyMisc::EBatteryState	State;
		int								Level;          // in range [1, 100]
		//EBatteryCapacityLevel			Level;          // in range [1,7]
		float						    Temperature;    // in degrees of Celsius
	};

	static CORE_API FBatteryState GetBatteryState();
	static CORE_API int GetBatteryLevel();
	static CORE_API bool IsRunningOnBattery();
	static CORE_API bool IsInLowPowerMode();
	static CORE_API float GetDeviceTemperatureLevel();
	static CORE_API bool AreHeadPhonesPluggedIn();
	static CORE_API ENetworkConnectionType GetNetworkConnectionType();
	static CORE_API bool HasActiveWiFiConnection();

	static CORE_API void RegisterForRemoteNotifications();
	static CORE_API void UnregisterForRemoteNotifications();

	static CORE_API bool IsAllowedRemoteNotifications();

	/** @return Memory representing a true type or open type font provided by the platform as a default font for unreal to consume; empty array if the default font failed to load. */
	static CORE_API TArray<uint8> GetSystemFontBytes();

	static CORE_API IPlatformChunkInstall* GetPlatformChunkInstall();

	static CORE_API void PrepareMobileHaptics(EMobileHapticsType Type);
	static CORE_API void TriggerMobileHaptics();
	static CORE_API void ReleaseMobileHaptics();
	static CORE_API void ShareURL(const FString& URL, const FText& Description, int32 LocationHintX, int32 LocationHintY);

	static CORE_API FString LoadTextFileFromPlatformPackage(const FString& RelativePath);
	static CORE_API bool FileExistsInPlatformPackage(const FString& RelativePath);


	static CORE_API void SetVersionInfo(FString InOpenHarmonyVersion, int32 InMajorVersion, int32 InSdkApiVersion, FString InDeviceMake, FString InDeviceModel, FString InBuildVersion, FString InOSLanguage);
	static CORE_API const FString GetOpenHarmonyVersion();
	static CORE_API int32 GetOpenHarmonyMajorVersion();
	static CORE_API int32 GetTargetSDKVersion();
    static CORE_API const FString GetDeviceMake();
	static CORE_API const FString GetDeviceModel();
    static CORE_API const FString GetOSLanguage();
	static CORE_API const FString GetProductName();
	static CORE_API const FString GetDeviceBuildNumber();
	static CORE_API const FString GetProjectVersion();
	static CORE_API FString GetDefaultLocale();
	static CORE_API FString GetGPUFamily();
	static CORE_API FString GetGLVersion();
	static CORE_API bool SupportsFloatingPointRenderTargets();
	static CORE_API bool SupportsShaderFramebufferFetch();
	static CORE_API bool SupportsShaderIOBlocks();
	static CORE_API bool IsSupportedOpenHarmonyDevice();
	static CORE_API void SetForceUnsupported(bool bInOverride);
	static CORE_API const TMap<FString, FString>& GetConfigRulesTMap();
	static CORE_API FString* GetConfigRulesVariable(const FString& Key);

	/* HasVulkanDriverSupport
	 * @return true if this Android device supports a Vulkan API Unreal could use
	 */
	static CORE_API bool HasVulkanDriverSupport();

	/* IsVulkanAvailable
	 * @return	true if there is driver support, we have an RHI, we are packaged with Vulkan support,
	 *			and not we are not forcing GLES with a command line switch
	 */
	static CORE_API bool IsVulkanAvailable();
	static CORE_API bool IsDesktopVulkanAvailable();
	/* ShouldUseVulkan
	 * @return true if Vulkan is available, and not disabled by device profile cvar
	 */
	static CORE_API bool ShouldUseVulkan();
	static CORE_API bool ShouldUseDesktopVulkan();
	static CORE_API FString GetVulkanVersion();
	typedef TFunction<void(void* NewNativeHandle)> ReInitWindowCallbackType;
	static CORE_API ReInitWindowCallbackType GetOnReInitWindowCallback();
	static CORE_API void SetOnReInitWindowCallback(ReInitWindowCallbackType InOnReInitWindowCallback);
	typedef TFunction<void()> ReleaseWindowCallbackType;
	static CORE_API ReleaseWindowCallbackType GetOnReleaseWindowCallback();
	static CORE_API void SetOnReleaseWindowCallback(ReleaseWindowCallbackType InOnReleaseWindowCallback);

	static CORE_API FString GetOSVersion(); 
	static bool GetOverrideResolution(int32& ResX, int32& ResY) { return false; } 

	typedef TFunction<void()> OnPauseCallBackType;
	static CORE_API OnPauseCallBackType GetOnPauseCallback();
	static CORE_API void SetOnPauseCallback(OnPauseCallBackType InOnPauseCallback);
	static CORE_API void TriggerCrashHandler(ECrashContextType InType, const TCHAR* InErrorMessage, const TCHAR* OverrideCallstack = nullptr);

	//static CORE_API FString GetFatalSignalMessage(int Signal, siginfo* Info);
	static CORE_API void OverrideFatalSignalHandler(void (*FatalSignalHandlerOverrideFunc)(int Signal, struct siginfo* Info, void* Context, uint32 CrashingThreadId));

	static CORE_API bool IsInSignalHandler();
#if !UE_BUILD_SHIPPING
	static CORE_API bool IsDebuggerPresent();
#endif

	FORCEINLINE static void MemoryBarrier()
	{
		__sync_synchronize();
	}

#if STATS || ENABLE_STATNAMEDEVENTS
	static CORE_API void BeginNamedEventFrame();
	static CORE_API void BeginNamedEvent(const struct FColor& Color, const TCHAR* Text);
	static CORE_API void BeginNamedEvent(const struct FColor& Color, const ANSICHAR* Text);
	static CORE_API void EndNamedEvent();
	static CORE_API void CustomNamedStat(const TCHAR* Text, float Value, const TCHAR* Graph, const TCHAR* Unit);
	static CORE_API void CustomNamedStat(const ANSICHAR* Text, float Value, const ANSICHAR* Graph, const ANSICHAR* Unit);
#endif

#if (STATS || ENABLE_STATNAMEDEVENTS)
	static CORE_API int32 TraceMarkerFileDescriptor;
#endif

	// run time compatibility information
	static CORE_API FString OpenHarmonyVersion; // version of openharmony we are running eg "4.1.0.31"

	static CORE_API int32   OpenHarmonyMajorVersion; // integer major version of openharmony we are running, eg 4
	static CORE_API int32   TargetSDKVersion; // Target SDK version, eg 29.
	static CORE_API FString DeviceMake; // make of the device we are running on eg. "mate40"
    static CORE_API FString DeviceModel; // model of the device we are running on eg "CMS-AL100"

	static CORE_API FString DeviceBuildNumber; // platform image build number of device "R16NW.G960NKSU1ARD6"
	static CORE_API FString OSLanguage; // language code the device is set to
	static CORE_API FString ProductName; // Product name, if available. e.g. 'Galaxy Tab S8' or empty string.
	static CORE_API int32 OpenHarmonyBuildVersion;
	// Key/Value pair variables from the optional configuration.txt
	static CORE_API TMap<FString, FString> ConfigRulesVariables;

	static CORE_API bool VolumeButtonsHandledBySystem;
    static CORE_API bool bNeedsRestartAfterPSOPrecompile;
	enum class ECoreFrequencyProperty
	{
		CurrentFrequency,
		MaxFrequency,
		MinFrequency,
	};

	static CORE_API uint32 GetCoreFrequency(int32 CoreIndex, ECoreFrequencyProperty CoreFrequencyProperty);
	static CORE_API float GetCPUTemperature();
	static CORE_API void UpdateDeviceOrientation();
	static void SaveDeviceOrientation(EDeviceScreenOrientation NewDeviceOrentation) { DeviceOrientation = NewDeviceOrentation; }
	// Window access is locked by the game thread before preinit and unlocked here after RHIInit (PlatformCreateDynamicRHI). 
	static CORE_API void UnlockOpenHarmonyWindow();
	static CORE_API TArray<int32> GetSupportedNativeDisplayRefreshRates();
	static CORE_API bool SetNativeDisplayRefreshRate(int32 RefreshRate);
	static CORE_API int32 GetNativeDisplayRefreshRate();




	/**
	 * Returns whether or not a 16 bit index buffer should be promoted to 32 bit on load, needed for some OpenHarmony devices
	 */
	static CORE_API bool Expand16BitIndicesTo32BitOnLoad();
	/**
    * Will return true if we wish to propagate the alpha to the backbuffer
     */
	static CORE_API int GetMobilePropagateAlphaSetting();
	static CORE_API bool SupportsBackbufferSampling();

	static CORE_API void SetMemoryWarningHandler(void (*Handler)(const FGenericMemoryWarningContext& Context));
	static CORE_API bool HasMemoryWarningHandler();

	// Android specific requesting of exit, *ONLY* use this function in signal handling code. Otherwise normal RequestExit functions
	static CORE_API void NonReentrantRequestExit();

	// Register/Get thread names for Android specific threads
	static CORE_API void RegisterThreadName(const char* Name, uint32 ThreadId);
	static CORE_API const char* GetThreadName(uint32 ThreadId);

	static CORE_API void ShowConsoleWindow();

	static CORE_API FDelegateHandle AddNetworkListener(FOnNetworkConnectionChangedDelegate&& InNewDelegate);
	static CORE_API bool RemoveNetworkListener(FDelegateHandle Handle);
	//存储临时的设备朝向，由js侧异步修改
	static CORE_API int TmpDeviceOrientation;

	static CORE_API double GetDevicePhysicalSizeInInch();

private:
	static CORE_API const ANSICHAR* CodeToString(int Signal, int si_code);
	static CORE_API EDeviceScreenOrientation DeviceOrientation;

#if USE_OPENHARMONY_NAPI
	enum class EOpenharmonyScreenOrientation
	{
		SCREEN_ORIENTATION_UNSPECIFIED = 0,
		SCREEN_ORIENTATION_PORTRAIT = 1,
		SCREEN_ORIENTATION_LANDSCAPE = 2,
		SCREEN_ORIENTATION_PORTRAIT_INVERTED = 3,
		SCREEN_ORIENTATION_LANDSCAPE_INVERTED = 4,
		SCREEN_ORIENTATION_AUTO_ROTATION = 5,
		SCREEN_ORIENTATION_AUTO_ROTATION_PORTRAIT = 6,
		SCREEN_ORIENTATION_AUTO_ROTATION_LANDSCAPE = 7,
		SCREEN_ORIENTATION_AUTO_ROTATION_RESTRICTED = 8,
		SCREEN_ORIENTATION_AUTO_ROTATION_PORTRAIT_RESTRICTED = 9,
		SCREEN_ORIENTATION_AUTO_ROTATION_LANDSCAPE_RESTRICTED =10,
		SCREEN_ORIENTATION_LOCKED = 11,
		SCREEN_ORIENTATION_AUTO_ROTATION_UNSPECIFIED = 12,
		SCREEN_ORIENTATION_USER_ROTATION_PORTRAIT = 13,
		SCREEN_ORIENTATION_USER_ROTATION_LANDSCAPE = 14,
		SCREEN_ORIENTATION_USER_ROTATION_PORTRAIT_INVERTED = 15,
		SCREEN_ORIENTATION_USER_ROTATION_LANDSCAPE_INVERTED = 16,
		SCREEN_ORIENTATION_FOLLOW_DESKTOP = 17,
	};
	
	static CORE_API int32 GetOpenHarmonyScreenOrientation(EDeviceScreenOrientation ScreenOrientation);
#endif // USE_OPENHARMONY_NAPI
};

typedef FOpenHarmonyMisc FPlatformMisc;