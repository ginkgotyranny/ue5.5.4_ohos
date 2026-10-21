// Copyright Epic Games, Inc. All Rights Reserved.OpenHarmonyDeviceProfileSelector

#include "OpenHarmonyDeviceProfileSelectorRuntimeModule.h"
#include "OpenHarmonyDeviceProfileSelectorRuntime.h"
#include "Templates/Casts.h"
#include "Internationalization/Regex.h"
#include "Modules/ModuleManager.h"
#include "OpenHarmonyDeviceProfileSelectorRuntime.h"
#include "OpenHarmonyDeviceProfileSelector.h"
#include "OpenHarmonyArkTSSurfaceViewDevices.h"
#include "IHeadMountedDisplayModule.h"

IMPLEMENT_MODULE(FOpenHarmonyDeviceProfileSelectorRuntimeModule, OpenHarmonyDeviceProfileSelectorRuntime);

void FOpenHarmonyDeviceProfileSelectorRuntimeModule::StartupModule()
{
}

void FOpenHarmonyDeviceProfileSelectorRuntimeModule::ShutdownModule()
{
}

// Build the selector params for the current device.
static const TMap<FName,FString>& GetDeviceSelectorParams()
{
	static bool bInitialized = false;
	static TMap<FName, FString> OpenHarmonyParams;
	if(!bInitialized)
	{
		bInitialized = true;
		auto GetParam = [](const FString& DefaultParam, const TCHAR* ConfRuleVarName)
		{
#if PLATFORM_OPENHARMONY
			if (FString* ConfRuleVarValue = FOpenHarmonyMisc::GetConfigRulesVariable(ConfRuleVarName))
			{
				return *ConfRuleVarValue;
			}
#endif
			return DefaultParam;
		};

#if !(PLATFORM_OPENHARMONY_X86 || PLATFORM_OPENHARMONY_X64)
		// Not running an Intel libUnreal.so with Houdini library present means we're emulated
		bool bUsingHoudini = (access("/system/lib/libhoudini.so", F_OK) != -1);
#else
		bool bUsingHoudini = false;
#endif

		FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
		// this is used in the same way as PlatformMemoryBucket
		// which on OpenHarmony has a different rounding algo. See GenericPlatformMemory::GetMemorySizeBucket.
		uint64 MemoryBucketRoundingAddition = 384;
#if PLATFORM_OPENHARMONY
		if (FString* MemoryBucketRoundingAdditionVar = FOpenHarmonyMisc::GetConfigRulesVariable(TEXT("MemoryBucketRoundingAddition")))
		{
			MemoryBucketRoundingAddition = FCString::Atoi64(**MemoryBucketRoundingAdditionVar);
		}
#endif
		uint32 TotalPhysicalGB = (uint32)((Stats.TotalPhysical + MemoryBucketRoundingAddition * 1024 * 1024 - 1) / 1024 / 1024 / 1024);

		FString HMDRequestedProfileName;
		if (IHeadMountedDisplayModule::IsAvailable())
		{
			HMDRequestedProfileName = IHeadMountedDisplayModule::Get().GetDeviceSystemName();
		}

		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_GPUFamily, GetParam(FOpenHarmonyMisc::GetGPUFamily(), TEXT("gpu")));
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_GLVersion, FOpenHarmonyMisc::GetGLVersion());
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_VulkanAvailable, FOpenHarmonyMisc::IsVulkanAvailable() ? TEXT("true") : TEXT("false"));
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_VulkanVersion, FOpenHarmonyMisc::GetVulkanVersion());
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_OpenHarmonyVersion, FOpenHarmonyMisc::GetOpenHarmonyVersion());
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceMake, FOpenHarmonyMisc::GetDeviceMake());
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceModel, FOpenHarmonyMisc::GetDeviceModel());
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceBuildNumber, FOpenHarmonyMisc::GetDeviceBuildNumber());
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_UsingHoudini, bUsingHoudini ? TEXT("true") : TEXT("false"));
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_Hardware, GetParam(FString(TEXT("unknown")), TEXT("hardware")));
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_Chipset, GetParam(FString(TEXT("unknown")), TEXT("chipset")));
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_HMDSystemName, HMDRequestedProfileName);
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_TotalPhysicalGB, FString::Printf(TEXT("%d"), TotalPhysicalGB));
		OpenHarmonyParams.Add(FOpenHarmonyProfileSelectorSourceProperties::SRC_SM5Available, FOpenHarmonyMisc::IsDesktopVulkanAvailable() ? TEXT("true") : TEXT("false"));

#if PLATFORM_OPENHARMONY
		// allow ConfigRules to override cvars first
		const TMap<FString, FString>& ConfigRules = FOpenHarmonyMisc::GetConfigRulesTMap();
		for (const TPair<FString, FString>& Pair : ConfigRules)
		{
			const FString& VariableName = Pair.Key;
			const FString& VariableValue = Pair.Value;
			OpenHarmonyParams.Add(FName(FString::Printf(TEXT("SRC_ConfigRuleVar[%s]"), *VariableName)), *VariableValue);
		}
#endif
	}
	return OpenHarmonyParams;
}

FString const FOpenHarmonyDeviceProfileSelectorRuntimeModule::GetRuntimeDeviceProfileName()
{
	static FString ProfileName;

	if (ProfileName.IsEmpty())
	{
		// Fallback profiles in case we do not match any rules
		ProfileName = FPlatformMisc::GetDefaultDeviceProfileName();
		if (ProfileName.IsEmpty())
		{
			ProfileName = FPlatformProperties::PlatformName();
		}
		const TMap<FName, FString>& OpenHarmonyParams = GetDeviceSelectorParams();

		FOpenHarmonyDeviceProfileSelector::SetSelectorProperties(OpenHarmonyParams);

		UE_LOG(LogOpenHarmony, Log, TEXT("Checking %d rules from DeviceProfile ini file."), FOpenHarmonyDeviceProfileSelector::GetNumProfiles() );
		UE_LOG(LogOpenHarmony, Log, TEXT("  Default profile: %s"), * ProfileName);
		UE_LOG(LogOpenHarmony, Log, TEXT("  OpenHarmony selector params: "));
		for(auto& MapIt : OpenHarmonyParams)
		{
			UE_LOG(LogOpenHarmony, Log, TEXT("  %s: %s"), *MapIt.Key.ToString(), *MapIt.Value);
		}

		const FString& DeviceMake = OpenHarmonyParams.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceMake);
		const FString& DeviceModel = OpenHarmonyParams.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceModel);

		CheckForArkTSSurfaceViewWorkaround(DeviceMake, DeviceModel);

		// Use override from ConfigRules if set
		FString* ConfigProfile = FOpenHarmonyMisc::GetConfigRulesVariable(TEXT("Profile"));
		if (ConfigProfile != nullptr)
		{
			ProfileName = *ConfigProfile;
			UE_LOG(LogOpenHarmony, Log, TEXT("Using ConfigRules Profile: [%s]"), *ProfileName);
		}
		else
		{
			// Find a match with the DeviceProfiles matching rules
			ProfileName = FOpenHarmonyDeviceProfileSelector::FindMatchingProfile(ProfileName);
			UE_LOG(LogOpenHarmony, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
		}
	}

	return ProfileName;
}

bool FOpenHarmonyDeviceProfileSelectorRuntimeModule::GetSelectorPropertyValue(const FName& PropertyType, FString& PropertyValueOUT)
{
	if (const FString* Found = GetDeviceSelectorParams().Find(PropertyType))
	{
		PropertyValueOUT = *Found;
		return true;
	}
	// Special case for non-existent config rule variables
	// they should return true and a value of '[null]'
	// this prevents configrule issues from throwing errors.
	if (PropertyType.ToString().StartsWith(TEXT("SRC_ConfigRuleVar[")))
	{
		PropertyValueOUT = TEXT("[null]");
		return true;
	}

	return false;
}

void FOpenHarmonyDeviceProfileSelectorRuntimeModule::CheckForArkTSSurfaceViewWorkaround(const FString& DeviceMake, const FString& DeviceModel) const
{
/*
#if USE_OpenHarmony_JNI
	// We need to initialize the class early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
	extern UClass* Z_Construct_UClass_UOpenHarmonyArkTSSurfaceViewDevices();
	Z_Construct_UClass_UOpenHarmonyArkTSSurfaceViewDevices();

	const UOpenHarmonyArkTSSurfaceViewDevices *const SurfaceViewDevices = Cast<UOpenHarmonyArkTSSurfaceViewDevices>(UOpenHarmonyArkTSSurfaceViewDevices::StaticClass()->GetDefaultObject());
	check(SurfaceViewDevices);

	for(const FArkTSSurfaceViewDevice& Device : SurfaceViewDevices->SurfaceViewDevices)
	{
		if(Device.Manufacturer == DeviceMake && Device.Model == DeviceModel)
		{
			extern void OpenHarmonyThunkCpp_UseSurfaceViewWorkaround();
			OpenHarmonyThunkCpp_UseSurfaceViewWorkaround();
			return;
		}
	}
#endif
*/
}
