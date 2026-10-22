// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "AudioCompressionSettings.h"

#include "OpenHarmonyRuntimeSettings.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOpenHarmonyRuntimeSettings, Log, All);

UENUM()
namespace EOpenHarmonyScreenOrientation
{
	// IF THIS CHANGES, MAKE SURE TO UPDATE UEDeployOpenHarmony.cs, ConvertOrientationIniValue()!

	enum Type : int
	{
		/** Represents an undefined orientation mode, determined by the system.*/
		UNSPECIFIED,

		/** Portrait orientation (the display is taller than it is wide). */
		Portrait,

		/** Landscape orientation (the display is wider than it is tall). */
		Landscape,

		/** Represents reverse portrait display mode. */
		PORTRAIT_INVERTED,

		/** Represents reverse landscape display mode. */
		LANDSCAPE_INVERTED,

		/** Represents sensor auto-rotation mode. */
		AUTO_ROTATION,

		/** Represents sensor auto-rotation mode in portrait. */
		AUTO_ROTATION_PORTRAIT,

		/** Represents sensor auto-rotation mode in landscape. */
		AUTO_ROTATION_LANDSCAPE,

		/** Represents auto-rotation mode controlled by a switch. */
		AUTO_ROTATION_RESTRICTED,

		/** Represents auto-rotation mode in portrait, controlled by a switch. */
		AUTO_ROTATION_PORTRAIT_RESTRICTED,

		/** Represents auto-rotation mode in landscape, controlled by a switch. */
		AUTO_ROTATION_LANDSCAPE_RESTRICTED,

		/** Represents locked mode. */
		LOCKED
	};
}

/** Depth buffer precision preferences */
UENUM()
namespace EOpenHarmonyDepthBufferPreference
{
	// IF THIS CHANGES, MAKE SURE TO UPDATE UEDeployOpenHarmony.cs, ConvertDepthBufferIniValue()!

	enum Type : int
	{
		Default = 0 UMETA(DisplayName = "Default"),
		Bits16 = 16 UMETA(DisplayName = "16-bit"),
		Bits24 = 24 UMETA(DisplayName = "24-bit"),
		Bits32 = 32 UMETA(DisplayName = "32-bit"),
	};
}

/** The default install location for the application */
UENUM()
namespace EOpenHarmonyInstallLocation
{
	enum Type : int
	{
		/** Install your app only on internal device storage */
		InternalOnly,
		/** Install your app on external storage when available */
		PreferExternal,
		/** Internal storage is preferred over external, unless the interal storage is low on space */
		Auto
	};
}

UENUM()
namespace EOpenHarmonyAudio
{
	enum Type : int
	{
		Default = 0 UMETA(DisplayName = "Default", ToolTip = "This option selects the default encoder."),
		OGG = 1 UMETA(DisplayName = "Ogg Vorbis", ToolTip = "Selects Ogg Vorbis encoding."),
		ADPCM = 2 UMETA(DisplayName = "ADPCM", ToolTip = "This option selects ADPCM lossless encoding.")
	};
}

UENUM()
namespace EOpenHarmonyGraphicsDebugger
{
	enum Type : int
	{
		None = 0 UMETA(DisplayName = "None"),
		Mali = 1 UMETA(DisplayName = "Mali Graphics Debugger", ToolTip = "Configure for Mali Graphics Debugger."),
		Adreno = 2 UMETA(DisplayName = "Adreno Profiler", ToolTip = "Configure for Adreno Profiler."),
	};
}


/**
 * Implements the settings for the OpenHarmony runtime platform.
 */
UCLASS(config=Engine, defaultconfig)
class OPENHARMONYRUNTIMESETTINGS_API UOpenHarmonyRuntimeSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	// The official name of the product (same as the name you use on the Play Store web site). Note: Must have at least 2 sections separated by a period and be unique!
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "OpenHarmony Package Name ('com.Company.Project', [PROJECT] is replaced with project name)"))
	FString PackageName;

	// The version number used to indicate newer versions in the Store
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Store Version (1-2147483647)", ClampMin="1", ClampMax="2147483647"))
	int32 StoreVersion;

	// Offset to add to store version for HAPs generated for arm64
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", meta = (DisplayName = "Store Version offset (arm64)"))
	int32 StoreVersionOffsetArm64;

	// Offset to add to store version for HAPs generated for x86_64
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", meta = (DisplayName = "Store Version offset (x86_64)"))
	int32 StoreVersionOffsetX8664;

	// The visual application name displayed for end users
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Application Display Name (app_name), project name if blank"))
	FString AppName;

	// The visual version displayed for end users
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Version Display Name (usually x.y)"))
	FString VersionDisplayName;

	// What OS version the app is allowed to be installed on (do not set this lower than 26)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Minimum SDK Version (26=8.0.0, 27=8.1.0, 28=9, 29=10, 30=11, 31=12)"))
	int32 MinSDKVersion;
	
	// What OS version the app is expected to run on (do not set this lower than 26)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Target SDK Version (26=8.0.0, 27=8.1.0, 28=9, 29=10, 30=11, 31=12)"))
	int32 TargetSDKVersion;

	// Preferred install location for the application
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging")
	TEnumAsByte<EOpenHarmonyInstallLocation::Type> InstallLocation;

	// Enable -Xlint:unchecked and -Xlint:depreciation for Java compiling (Gradle only)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Enable Lint depreciation checks"))
	bool bEnableLint;

	// Should the data be placed into the .HAP file instead of a separate .obb file. Amazon requires this to be enabled, but Google Play Store will not allow .HAP files larger than 100MB, so only small games will work with this enabled.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Package game data inside .HAP?"))
	bool bPackageDataInsideHAP;

	// If checked, both batch (.bat) files and shell script (.command) files will be generated, otherwise only done for the current system (default)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Generate install files for all platforms"))
	bool bCreateAllPlatformsInstall;

	// Disable the verification of an OBB file when it is downloaded or on first start when in a distribution build. 
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Disable verify OBB on first start/update."))
	bool bDisableVerifyOBBOnStartUp;

	// If checked, OBB is limited to 1 GiB.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Force small OBB files."))
	bool bForceSmallOBBFiles;

	// If checked, OBB is not limited to 2 GiB allowed by Google Play Store (still limited to 4 GiB ZIP limit). 
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Allow large OBB files."))
	bool bAllowLargeOBBFiles;

	// If checked, a patch OBB is generated for files not fitting in the main OBB (requires using multiple PAK files so split up content by chunk id).
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Allow patch OBB file."))
	bool bAllowPatchOBBFile;

	// If checked, up to two additional overflow OBB files are generated for files not fitting in the patch OBB (requires using multiple PAK files so split up content by chunk id).
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Allow overflow OBB files."))
	bool bAllowOverflowOBBFiles;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Don't bundle libraries into .HAP for quicker iteration [Experimental]"))
	bool bDontBundleLibrariesInHAP;

	// If checked, UnrealGame files will be placed in ExternalFilesDir which is removed on uninstall.
	// You should also check this if you need to save you game progress without requesting runtime WRITE_EXTERNAL_STORAGE permission in openharmony api 23+
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Use ExternalFilesDir for UnrealGame files?"))
	bool bUseExternalFilesDir;

	// If checked, log files will always be placed in a publicly available directory (either /sdcard/OpenHarmony or /sdcard/UnrealGame).
	// You may require WRITE_EXTERNAL_STORAGE permission if you do not use ExternalFilesDir checkbox in openharmony api 23+
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Make log files always publicly accessible?"))
	bool bPublicLogFiles;

	// The permitted orientation of the application on the device
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging")
	TEnumAsByte<EOpenHarmonyScreenOrientation::Type> Orientation;

	// Maximum supported aspect ratio (width / height). OpenHarmony will automatically letterbox application on devices with bigger aspect ratio
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Maximum supported aspect ratio."))
	float MaxAspectRatio;

	// Enables use of the display cutout area on OpenHarmony 9+
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Use display cutout region?"))
	bool bUseDisplayCutout;

	// Allow resizing of the window on OpenHarmony devices with splitscreen
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Allow splitscreen resizing?"))
	bool bAllowResizing;

	// Allow support for size change when foldable and flip devices change screen or layout on OpenHarmony 10+
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Allow fold/flip size changes?"))
	bool bSupportSizeChanges;

	// Should we restore scheduled local notifications on reboot? This will add a receiver for boot complete and a permission to the manifest.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Restore scheduled notifications on reboot"))
	bool bRestoreNotificationsOnReboot;

	// Should the software navigation buttons be hidden or not
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Enable FullScreen Immersive on KitKat and above devices."))
	bool bFullScreen;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = ( DisplayName = "Enable improved virtual keyboard"))
	bool bEnableNewKeyboard;
	
	// The preferred depth buffer bitcount for OpenHarmony
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Preferred Depth Buffer format"))
	TEnumAsByte<EOpenHarmonyDepthBufferPreference::Type> DepthBufferPreference;

	// Verifies the device supports at least one of the cooked texture formats at runtime
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Validate texture formats"))
	bool bValidateTextureFormats;

	// When building for MinSDKVersion >= 23 gradle will leave native libs uncompressed in the HAP. This flag might be helpful for builds that are not intended to be distributed via Google Play
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Force Gradle to compress native libs irregardless of MinSDKVersion setting"))
	bool bForceCompressNativeLibs;

	// Generates OpenHarmony binary with RELR and APS2 relocation tables when building for MinSDKVersion >= 28 or just APS2 when building for MinSDKVersion >= 23
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "HAP Packaging", Meta = (DisplayName = "Enable compression of relocation tables (and more). Depends on MinSDKVersion setting"))
	bool bEnableAdvancedBinaryCompression;

	// Enables generating AAB bundle
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "App Bundles", Meta = (DisplayName = "Generate bundle (AAB)"))
	bool bEnableBundle;

	// Enables generating universal HAP from bundle
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "App Bundles", Meta = (DisplayName = "Generate universal HAP from bundle", EditCondition = "bEnableBundle"))
	bool bEnableUniversalHAP;

	// Separate ABIs into their own HAP in bundle
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "App Bundles", Meta = (DisplayName = "Enable ABI split", EditCondition = "bEnableBundle"))
	bool bBundleABISplit;

	// Separate resources by language into their own HAP in bundle
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "App Bundles", Meta = (DisplayName = "Enable language split", EditCondition = "bEnableBundle"))
	bool bBundleLanguageSplit;

	// Separate resources by density into their own HAP in bundle
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "App Bundles", Meta = (DisplayName = "Enable density split", EditCondition = "bEnableBundle"))
	bool bBundleDensitySplit;

	// Any extra tags for the <manifest> node
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Extra Tags for <manifest> node"))
	TArray<FString> ExtraManifestNodeTags;

	// Any extra tags for the <application> node
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Extra Tags for <application> node"))
	TArray<FString> ExtraApplicationNodeTags;

	// Any extra tags for the com.epicgames.unreal.GameActivity <activity> node
	// Any extra settings for the <application> section (an optional file <Project>/Build/OpenHarmony/ManifestApplicationAdditions.txt will also be included)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Extra Settings for <application> section (\\n to separate lines)"))
	FString ExtraApplicationSettings;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Extra Tags for com.epicgames.unreal.GameActivity <activity> node"))
	TArray<FString> ExtraActivityNodeTags;

	// Any extra settings for the main <activity> section (an optional file <Project>/Build/OpenHarmony/ManifestApplicationActivtyAdditions.txt will also be included)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Extra Settings for <activity> section (\\n to separate lines)"))
	FString ExtraActivitySettings;

	// Any extra permissions your app needs (an optional file <Project>/Build/OpenHarmony/ManifestRequirementsAdditions.txt will also be included,
	// or an optional file <Project>/Build/OpenHarmony/ManifestRequirementsOverride.txt will replace the entire <!-- Requirements --> section)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Extra Permissions (e.g. 'openharmony.permission.INTERNET')"))
	TArray<FString> ExtraPermissions;

	// Add required permission to support Voice chat
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Add permissions to support Voice chat (RECORD_AUDIO)"))
	bool bOpenHarmonyVoiceEnabled;

	// Request permission at startup (SplashActivity) for Voice chat (RECORD_AUDIO)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Request permission at startup for Voice chat (RECORD_AUDIO)", EditCondition = "bOpenHarmonyVoiceEnabled"))
	bool bRecordPermissionAtStartupEnabled;

	// Add required permission and support to allow multicast/broadcast Wi-Fi traffic through network interface
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Add support for multicast Wi-Fi traffic (CHANGE_WIFI_MULTICAST_STATE)"))
	bool bEnableMulticastSupport;

	// Package for Oculus Mobile devices. When enabled, it will enable build support for arm64 and vulkan, and disable build support for x86_64, Vulkan Desktop, and OpenGL.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Package for Meta Quest devices."))
	bool bPackageForMetaQuest;

	// Removes Oculus Signature Files (osig) from HAP if Quest/Go HAP signed for distribution and enables entitlement checker
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Advanced HAP Packaging", Meta = (DisplayName = "Remove Oculus Signature Files from Distribution HAP"))
	bool bRemoveOSIG;

	// This is the file that keytool outputs, specified with the -keystore parameter (file should be in <Project>/Build/OpenHarmony)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DistributionSigning, Meta = (DisplayName = "Key Store (output of keytool, placed in <Project>/Build/OpenHarmony)"))
	FString KeyStore;
	
	// This is the name of the key that you specified with the -alias parameter to keytool
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DistributionSigning, Meta = (DisplayName = "Key Alias (-alias parameter to keytool)"))
	FString KeyAlias;
	
	// This is the password that you specified FOR THE KEYSTORE NOT THE KEY, when running keytool (either with -storepass or by typing it in).
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DistributionSigning, Meta = (DisplayName = "Key Store Password (-storepass parameter to keytool)"))
	FString KeyStorePassword;
	
	// This is the password for the key that you may have specified with keytool, if it's different from the keystore password. Leave blank to use same as Keystore
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DistributionSigning, Meta = (DisplayName = "Key Password (leave blank to use Key Store Password)"))
	FString KeyPassword;

	// Enable Arm64 support?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support arm64 [aka arm64-v8a]"))
	bool bBuildForArm64;

	// Enable x86-64 support? [CURRENTLY FOR FULL SOURCE GAMES ONLY]
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support x86_64 [aka x64]", EditCondition = "!bPackageForMetaQuest"))
	bool bBuildForX8664;

	// Include shaders for devices supporting OpenGL ES 3.2 and above (default)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support OpenGL ES3.2", EditCondition = "!bPackageForMetaQuest"))
	bool bBuildForES31;

	// Support the Vulkan RHI and include Vulkan shaders
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support Vulkan"))
	bool bSupportsVulkan;

	// Enable Vulkan SM5 rendering support
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support Vulkan Desktop [Experimental]", EditCondition = "!bPackageForMetaQuest"))
	bool bSupportsVulkanSM5;

	/** Directory for Debug Vulkan Layers to package */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Debug Vulkan Layer Directory"))
	FDirectoryPath DebugVulkanLayerDirectory;

	/** Debug Vulkan Device Layers to enable */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Debug Vulkan Device Layers"))
	TArray<FString> DebugVulkanDeviceLayers;

	/** Debug Vulkan Instance Layers to enable */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Debug Vulkan Instance Layers"))
	TArray<FString> DebugVulkanInstanceLayers;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support Backbuffer Sampling on OpenGL",
	ToolTip = "Whether to render to an offscreen surface instead of render to backbuffer directly on openharmony opengl platform. Enable it if you'd like to support UMG background blur on openharmony opengl."))
	bool bOpenHarmonyOpenGLSupportsBackbufferSampling;

	// Whether to detect Vulkan device support by default, if the project is packaged with Vulkan support. If unchecked, the -detectvulkan commandline will enable Vulkan detection.
	UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Build, meta = (DisplayName = "Detect Vulkan device support", EditCondition = "bSupportsVulkan"))
	bool bDetectVulkanByDefault;

	// Build the shipping config with hidden visibility by default. Results in smaller .so file but will also removes symbols used to display callstack dumps.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = AdvancedBuild, meta = (DisplayName = "Build with hidden symbol visibility in shipping config."))
	bool bBuildWithHiddenSymbolVisibility;

	// Disables extra checks for buffer overflows, comes with perf improvement, but might make tracing stack corruptions in production harder. Note that _FORTIFY_SOURCE=2 is still enabled by the toolchain providing lightweight stack checks
	UPROPERTY(GlobalConfig, EditAnywhere, Category = AdvancedBuild, meta = (DisplayName = "Disable extra checks for buffer overflows"))
	bool bDisableStackProtector;

	// Disable libc++_shared dependency validation in all .so files linked with libUnreal.so
	UPROPERTY(GlobalConfig, EditAnywhere, Category = AdvancedBuild, meta = (DisplayName = "Disable libc++_shared dependency validation in all dependencies"))
	bool bDisableLibCppSharedDependencyValidation;

	// Always save .so file with symbols allowing use of addr2line on raw callstack addresses.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = AdvancedBuild, meta = (DisplayName = "Always save a copy of the libUnreal.so with symbols. [Experimental]"))
	bool bSaveSymbols;

	// Strip shader reflection information under OpenHarmony to avoid issues on older drivers
	UPROPERTY(config, EditAnywhere, Category = AdvancedBuild, meta = (
	DisplayName = "Strip shader reflection information",
	ToolTip = "If true, strip shader reflection information under OpenHarmony",
	ConfigRestartRequired = true))
	bool bStripShaderReflection;

	// If selected, the checked architectures will be split into separate .HAP files [CURRENTLY FOR FULL SOURCE GAMES ONLY]
	// @todo openharmony fat binary: Currently, there isn't much utility in merging multiple .so's into a single .HAP except for debugging,
	// but we can't properly handle multiple GPU architectures in a single .HAP, so we are disabling the feature for now
	// The user will need to manually select the HAP to run in their Visual Studio debugger settings (see Override HAP in TADP, for instance)
// 	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build)
// 	bool bSplitIntoSeparateHAPs;

	// Should Google Play support be enabled?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	bool bEnableGooglePlaySupport;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices, meta = (DisplayName = "Request code for showing GooglePlay Games activities"), meta = (ToolTip = "This request code is reported in onActivityResult when Achievements or Leaderboards UIs are closed"))
	int RequestCodeForPlayGamesActivities;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices, meta = (DisplayName = "GooglePlay Games Oauth Client ID registered for game server"), meta = (ToolTip = "No auth code will be requested on login if empty. Setting up Server-side API Access (see https://developers.google.com/games/services/openharmony/offline-access)"))
	FString PlayGamesClientId;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices, meta = (DisplayName = "Enable GooglePlay Games offline access"), meta = (ToolTip = "Include refresh token when Auth code is exchanged. Setting up Server-side API Access (see https://developers.google.com/games/services/openharmony/offline-access)"))
	bool bForceRefreshToken;

	// The app id obtained from the Google Play Developer Console
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	FString GamesAppID;

	// Enabling this includes the AdMob SDK and will be detected by Google Play Console on upload of HAP.  Disable if you do not need ads to remove this warning.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices, meta = (DisplayName = "Include AdMob support for ads"))
	bool bSupportAdMob;

	// AdMob Application ID
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices, meta = (DisplayName = "AdMob App ID"), meta = (EditCondition = "bSupportAdMob"), meta = (ToolTip = "AdMob Application ID (see https://support.google.com/admob/answer/7356431)"))
	FString AdMobAppID;

	// The unique identifier for the ad obtained from AdMob.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	FString AdMobAdUnitID;

	// Identifiers for ads obtained from AdMob
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	TArray<FString> AdMobAdUnitIDs;

	// The unique identifier for this application (needed for IAP)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices)
	FString GooglePlayLicenseKey;

	// The sender id obtained from Firebase Console, leave blank to disable (associate this with your app in Google Player Developer Console).
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GooglePlayServices, meta = (DisplayName = "Google Cloud Messaging Sender ID"))
	FString GCMClientSenderID;

	/** Show the launch image as a startup slash screen */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = LaunchImages, meta = (DisplayName = "Show launch image"))
	bool bShowLaunchImage;

	/** Allows accelerometer, magnetometer, and gyroscope event handling, disabling may improve performance. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Input, meta = (DisplayName = "Allow IMU Sampling"))
	bool bAllowIMU;

	// If checked, Bluetooth connected controllers will send input
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Input, meta = (DisplayName = "Allow Bluetooth controllers"))
	bool bAllowControllers;

	// If checked, controllers will not send OpenHarmony_Back and OpenHarmony_Menu events that might cause unnecce
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Input, meta = (DisplayName = "Block OpenHarmony system keys being sent from controllers"))
	bool bBlockOpenHarmonyKeysOnControllers;

	// Block force feedback on the device when controllers are attached.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Input, meta = (DisplayName = "Block force feedback on the device when controllers are attached"))
	bool bControllersBlockDeviceFeedback;

	/** OpenHarmony encoding options. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Audio, meta = (DisplayName = "Encoding Format"))
	TEnumAsByte<EOpenHarmonyAudio::Type> OpenHarmonyAudio;

	/** Sample rate to run the audio mixer with. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", Meta = (DisplayName = "Audio Mixer Sample Rate"))
	int32 AudioSampleRate;

	/** The amount of audio to compute each callback block. Lower values decrease latency but may increase CPU cost. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "512", ClampMax = "4096", DisplayName = "Callback Buffer Size"))
	int32 AudioCallbackBufferFrameSize;

	/** The number of buffers to keep enqueued. More buffers increases latency, but can compensate for variable compute availability in audio callbacks on some platforms. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "2", UIMin = "2", DisplayName = "Number of Buffers To Enqueue"))
	int32 AudioNumBuffersToEnqueue;

	/** The max number of channels (voices) to limit for this platform. The max channels used will be the minimum of this value and the global audio quality settings. A value of 0 will not apply a platform channel count max. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0", UIMin = "0", DisplayName = "Max Channels"))
	int32 AudioMaxChannels;

	/** The number of workers to use to compute source audio. Will only use up to the max number of sources. Will evenly divide sources to each source worker. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0", UIMin = "0", DisplayName = "Number of Source Workers"))
	int32 AudioNumSourceWorkers;

	/** Which of the currently enabled spatialization plugins to use. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SpatializationPlugin;

	/** Which of the currently enabled source data override plugins to use. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SourceDataOverridePlugin;

	/** Which of the currently enabled reverb plugins to use. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString ReverbPlugin;

	/** Which of the currently enabled occlusion plugins to use. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString OcclusionPlugin;

	/** Various overrides for how this platform should handle compression and decompression */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FPlatformRuntimeAudioCompressionOverrides CompressionOverrides;

	/** This determines the max amount of memory that should be used for the cache at any given time. If set low (<= 8 MB), it lowers the size of individual chunks of audio during cook. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides|Stream Caching", meta = (DisplayName = "Max Cache Size (KB)"))
	int32 CacheSizeKB;

	/** This overrides the default max chunk size used when chunking audio for stream caching (ignored if < 0) */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides|Stream Caching", meta = (DisplayName = "Max Chunk Size Override (KB)"))
	int32 MaxChunkSizeOverrideKB;

	UPROPERTY(config, EditAnywhere, Category = "Audio|CookOverrides")
	bool bResampleForDevice;

	/** Quality Level to COOK SoundCues at (if set, all other levels will be stripped by the cooker). */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides", meta = (DisplayName = "Sound Cue Cook Quality"))
	int32 SoundCueCookQualityIndex = INDEX_NONE;

	// Mapping of which sample rates are used for each sample rate quality for a specific platform.

	UPROPERTY(config, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "Max"))
	float MaxSampleRate;

	UPROPERTY(config, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "High"))
	float HighSampleRate;

	UPROPERTY(config, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "Medium"))
	float MedSampleRate;

	UPROPERTY(config, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "Low"))
	float LowSampleRate;

	UPROPERTY(config, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "Min"))
	float MinSampleRate;

	// Scales all compression qualities when cooking to this platform. For example, 0.5 will halve all compression qualities, and 1.0 will leave them unchanged.
	UPROPERTY(config, EditAnywhere, Category = "Audio|CookOverrides")
	float CompressionQualityModifier;

	// When set to anything beyond 0, this will ensure any SoundWaves longer than this value, in seconds, to stream directly off of the disk.
	UPROPERTY(GlobalConfig)
	float AutoStreamingThreshold;

	// Several OpenHarmony graphics debuggers require configuration changes to be made to your application in order to operate. Choosing an option from this menu will configure your project to work with that graphics debugger. 
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GraphicsDebugger)
	TEnumAsByte<EOpenHarmonyGraphicsDebugger::Type> OpenHarmonyGraphicsDebugger;

	/** The path to your Mali Graphics Debugger installation (eg C:/Program Files/ARM/Mali Developer Tools/Mali Graphics Debugger v4.2.0) */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GraphicsDebugger)
	FDirectoryPath MaliGraphicsDebuggerPath;

	// If checked, this will disable the security.perf_harden flag on the OpenHarmony device when launching or installing your app via the generated batch file.
	// Disabling security.perf_harden is necessary for HWCPipe to be able to report performance counters on Mali devices.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = GraphicsDebugger)
	bool bEnableMaliPerfCounters;

	/** Include ETC2 textures when packaging with the OpenHarmony (Multi) variant. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = MultiTextureFormats, meta = (DisplayName = "Include ETC2 textures"))
	bool bMultiTargetFormat_ETC2;

	/** Include DXT textures when packaging with the OpenHarmony (Multi) variant. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = MultiTextureFormats, meta = (DisplayName = "Include DXT textures"))
	bool bMultiTargetFormat_DXT;

	/** Include ASTC textures when packaging with the OpenHarmony (Multi) variant. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = MultiTextureFormats, meta = (DisplayName = "Include ASTC textures"))
	bool bMultiTargetFormat_ASTC;

	/** Priority for the ETC2 texture format when launching on device or packaging using OpenHarmony_Multi. The highest priority format supported by the device will be used. Default value is 0.2. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = TextureFormatPriorities, meta = (DisplayName = "ETC2 texture format priority"))
	float TextureFormatPriority_ETC2;

	/** Priority for the DXT texture format when launching on device or packaging using OpenHarmony_Multi. The highest priority format supported by the device will be used. Default value is 0.6. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = TextureFormatPriorities, meta = (DisplayName = "DXT texture format priority"))
	float TextureFormatPriority_DXT;

	/** Priority for the ASTC texture format when launching on device or packaging using OpenHarmony_Multi. The highest priority format supported by the device will be used. Default value is 0.9. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = TextureFormatPriorities, meta = (DisplayName = "ASTC texture format priority"))
	float TextureFormatPriority_ASTC;

	// Which SDK to package and compile Java with (a specific version or (without quotes) 'latest' for latest version on disk, or 'matchndk' to match the NDK API Level)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Project SDK Override", Meta = (DisplayName = "SDK API Level (specific version, 'latest', or 'matchndk' - see tooltip)"))
	FString SDKAPILevelOverride;

	// Which NDK to compile with (a specific version or (without quotes) 'latest' for latest version on disk). Note that choosing openharmony-21 or later won't run on pre-5.0 devices.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Project SDK Override", Meta = (DisplayName = "NDK API Level (specific version or 'latest' - see tooltip)"))
	FString NDKAPILevelOverride;

	// Which build-tools to package with (a specific version or (without quotes) 'latest' for latest version on disk).
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Project SDK Override", Meta = (DisplayName = "Build-Tools Version (specific version or 'latest')"))
	FString BuildToolsOverride;

	/** Whether to enable LOD streaming for landscape visual meshes. Only supported on feature level Mobile or above. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Misc", Meta = (DisplayName = "Stream landscape visual mesh LODs"))
	bool bStreamLandscapeMeshLODs;

	// Enables WebViews to use DOM storage API
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Misc", Meta = (DisplayName = "Enable DOM storage for WebViews"))
	bool bEnableDomStorage;

	virtual void PostReloadConfig(class FProperty* PropertyThatWasLoaded) override;


#if WITH_EDITOR
	/** Called whenever a registered OpenHarmony property changes. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FPropertyChanged, struct FPropertyChangedEvent&);
	FPropertyChanged OnPropertyChanged;

private:
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;
	void HandlesRGBHWSupport();
	void HandleMetaQuestSupport();
	void RemoveExtraApplicationTag(FString TagToRemove);
	
	// End of UObject interface
	void EnsureValidGPUArch();
#endif
};
