// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyRuntimeSettings.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "HAL/IConsoleManager.h"
#include "Engine/RendererSettings.h"
#include "HAL/PlatformApplicationMisc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenHarmonyRuntimeSettings)

#if WITH_EDITOR
#include "IOpenHarmonyTargetPlatformModule.h"
#include "IOpenHarmonyTargetPlatformControlsModule.h"
#endif

DEFINE_LOG_CATEGORY(LogOpenHarmonyRuntimeSettings);

UOpenHarmonyRuntimeSettings::UOpenHarmonyRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Orientation(EOpenHarmonyScreenOrientation::Landscape)
	, MaxAspectRatio(2.1f)
	, bOpenHarmonyVoiceEnabled(false)
	, bPackageForMetaQuest(false)
	, bEnableGooglePlaySupport(false)
	, RequestCodeForPlayGamesActivities(80002)
	, bForceRefreshToken(false)
	, bSupportAdMob(true)
	, bBlockOpenHarmonyKeysOnControllers(false)
	, AudioSampleRate(44100)
	, AudioCallbackBufferFrameSize(1024)
	, AudioNumBuffersToEnqueue(4)
	, CacheSizeKB(65536)
	, MaxSampleRate(48000)
	, HighSampleRate(32000)
    , MedSampleRate(24000)
    , LowSampleRate(12000)
	, MinSampleRate(8000)
	, CompressionQualityModifier(1)
	, bMultiTargetFormat_ETC2(true)
	, bMultiTargetFormat_DXT(true)
	, bMultiTargetFormat_ASTC(true)
	, TextureFormatPriority_ETC2(0.2f)
	, TextureFormatPriority_DXT(0.6f)
	, TextureFormatPriority_ASTC(0.9f)
	, bStreamLandscapeMeshLODs(false)
{
	bBuildForES31 = bBuildForES31 || !bSupportsVulkan;
}

void UOpenHarmonyRuntimeSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);

#if PLATFORM_OPENHARMONY

	FPlatformApplicationMisc::SetGamepadsAllowed(bAllowControllers);

#endif //PLATFORM_OPENHARMONY
}

#if WITH_EDITOR

void UOpenHarmonyRuntimeSettings::HandlesRGBHWSupport()
{
	const bool SupportssRGB = bPackageForMetaQuest;
	URendererSettings* const Settings = GetMutableDefault<URendererSettings>();
	static auto* MobileUseHWsRGBEncodingCVAR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.UseHWsRGBEncoding"));

	if (SupportssRGB != Settings->bMobileUseHWsRGBEncoding)
	{
		Settings->bMobileUseHWsRGBEncoding = SupportssRGB;
		Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bMobileUseHWsRGBEncoding)), GetDefaultConfigFilename());
	}

	if (MobileUseHWsRGBEncodingCVAR && MobileUseHWsRGBEncodingCVAR->GetInt() != (int)SupportssRGB)
	{
		MobileUseHWsRGBEncodingCVAR->Set((int)SupportssRGB);
	}

}

void UOpenHarmonyRuntimeSettings::HandleMetaQuestSupport()
{
	// PackageForOculusMobile doesn't get loaded since it's marked as deprecated, so it needs to be read directly from the config
	TArray<FString> PackageList;
	//GConfig->GetArray(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), *FString("+").Append(GET_MEMBER_NAME_STRING_CHECKED(UOpenHarmonyRuntimeSettings, PackageForOculusMobile)), PackageList, GetDefaultConfigFilename());

	FString SupportedDevicesTag("<meta-data openharmony:name=\"com.oculus.supportedDevices\"");
	if (PackageList.Num() > 0)
	{
		bPackageForMetaQuest = true;
		// Clean ExtraApplications metadata so that the updated list of supported devices will be added further down.
		RemoveExtraApplicationTag(SupportedDevicesTag);
		// Use TryUpdateDefaultConfigFile() instead of UpdateSinglePropertyInConfigFile() so that the PackageForOculusMobile will also get cleared
		TryUpdateDefaultConfigFile();
	}

	// Automatically disable x86_64, and Vulkan Desktop if building for Meta Quest devices and switch to appropriate alternatives
	if (bPackageForMetaQuest)
	{
		if (bBuildForX8664)
		{
			bBuildForX8664 = false;
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bBuildForX8664)), GetDefaultConfigFilename());
			UE_LOG(LogOpenHarmonyRuntimeSettings, Warning, TEXT("Support x86_64 has been changed to false.\n"));

		}
		if (!bBuildForArm64)
		{
			bBuildForArm64 = true;
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bBuildForArm64)), GetDefaultConfigFilename());
			UE_LOG(LogOpenHarmonyRuntimeSettings, Warning, TEXT("Support arm64 has been changed to true.\n"));
		}
		if (bSupportsVulkanSM5)
		{
			bSupportsVulkanSM5 = false;
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bSupportsVulkanSM5)), GetDefaultConfigFilename());
			UE_LOG(LogOpenHarmonyRuntimeSettings, Warning, TEXT("Support Vulkan Desktop has been changed to false.\n"));
			EnsureValidGPUArch();
		}
		if (bBuildForES31)
		{
			bBuildForES31 = false;
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bBuildForES31)), GetDefaultConfigFilename());
			UE_LOG(LogOpenHarmonyRuntimeSettings, Warning, TEXT("Support OpenGL ES3.2 has been changed to false.\n"));
			EnsureValidGPUArch();
		}

		UE_LOG(LogOpenHarmonyRuntimeSettings, Display, TEXT("Enabled Package for Meta Quest devices.\nThe following settings have been applied:\n"));
		UE_LOG(LogOpenHarmonyRuntimeSettings, Display, TEXT("Support arm64: %d.\n"), bBuildForArm64);
		UE_LOG(LogOpenHarmonyRuntimeSettings, Display, TEXT("Support Vulkan: %d.\n"), bSupportsVulkan);
		UE_LOG(LogOpenHarmonyRuntimeSettings, Display, TEXT("Support x86_64: %d.\n"), bBuildForX8664);
		UE_LOG(LogOpenHarmonyRuntimeSettings, Display, TEXT("Support Vulkan Desktop: %d.\n"), bSupportsVulkanSM5);
		UE_LOG(LogOpenHarmonyRuntimeSettings, Display, TEXT("Support OpenGL ES3.2: %d."), bBuildForES31);

		int32 SupportedDevicesTagIndex = ExtraApplicationSettings.Find("com.oculus.supportedDevices");
		FString SupportedDevicesValue("quest|quest2|questpro|quest3");
		int32 SupportedDevicesIndex = ExtraApplicationSettings.Find(SupportedDevicesValue);
		// The supported devices tag is present but not up to date and does not contain all the currently supported devices.
		bool bNeedtoUpdateDevices = (SupportedDevicesTagIndex != INDEX_NONE) && (SupportedDevicesIndex == INDEX_NONE);
		// Remove the current supported devices value so that it can be added again with the updated value further down.
		if (bNeedtoUpdateDevices)
		{
			RemoveExtraApplicationTag(SupportedDevicesTag);
		}

		if (SupportedDevicesTagIndex == INDEX_NONE || bNeedtoUpdateDevices)
		{
			ExtraApplicationSettings.Append("<meta-data openharmony:name=\"com.oculus.supportedDevices\" openharmony:value=\"" + SupportedDevicesValue + "\" />");
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, ExtraApplicationSettings)), GetDefaultConfigFilename());
		}
	}
	else 
	{
		// Clean up the supported devices metadata tag so it doesn't end up in the manifest when bPackageForMetaQuest is turned off.
		RemoveExtraApplicationTag(SupportedDevicesTag);
	}
}

void UOpenHarmonyRuntimeSettings::RemoveExtraApplicationTag(FString TagToRemove)
{
	int32 StartIndex = ExtraApplicationSettings.Find(TagToRemove);
	if (StartIndex != INDEX_NONE)
	{
		int32 EndIndex = ExtraApplicationSettings.Find(">", ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
		ExtraApplicationSettings.RemoveAt(StartIndex, EndIndex - StartIndex + 1);
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, ExtraApplicationSettings)), GetDefaultConfigFilename());
	}
}

static void InvalidateAllOpenHarmonyPlatforms()
{
	ITargetPlatformModule* Module = FModuleManager::GetModulePtr<IOpenHarmonyTargetPlatformModule>("OpenHarmonyTargetPlatform");

	// call the delegate for each TP object
	for (ITargetPlatform* TargetPlatform : Module->GetTargetPlatforms())
	{
		FCoreDelegates::OnTargetPlatformChangedSupportedFormats.Broadcast(TargetPlatform);
	}
}

void UOpenHarmonyRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Ensure that at least one architecture is supported
	if (!bBuildForX8664 && !bBuildForArm64)
	{
		bBuildForArm64 = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bBuildForArm64)), GetDefaultConfigFilename());
	}

	if (PropertyChangedEvent.Property != nullptr)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bSupportsVulkan) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bBuildForES31))
		{
			// Supported shader formats changed so invalidate cache
			InvalidateAllOpenHarmonyPlatforms();

			OnPropertyChanged.Broadcast(PropertyChangedEvent);
		}
	}

	EnsureValidGPUArch();

	if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetName().StartsWith(TEXT("bMultiTargetFormat")))
	{
		UpdateSinglePropertyInConfigFile(PropertyChangedEvent.Property, GetDefaultConfigFilename());

		// Ensure we have at least one format for OpenHarmony_Multi
		if (!bMultiTargetFormat_ETC2 && !bMultiTargetFormat_DXT && !bMultiTargetFormat_ASTC)
		{
			bMultiTargetFormat_ETC2 = true;
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bMultiTargetFormat_ETC2)), GetDefaultConfigFilename());
		}

		// Notify the OpenHarmonyTargetPlatform module if it's loaded
		IOpenHarmonyTargetPlatformControlsModule* Module = FModuleManager::GetModulePtr<IOpenHarmonyTargetPlatformControlsModule>("OpenHarmonyTargetPlatformControls");
		if (Module)
		{
			Module->NotifyMultiSelectedFormatsChanged();
		}
	}

	if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetName().StartsWith(TEXT("TextureFormatPriority")))
	{
		UpdateSinglePropertyInConfigFile(PropertyChangedEvent.Property, GetDefaultConfigFilename());

		// Notify the OpenHarmonyTargetPlatform module if it's loaded
		IOpenHarmonyTargetPlatformControlsModule* Module = FModuleManager::GetModulePtr<IOpenHarmonyTargetPlatformControlsModule>("OpenHarmonyTargetPlatformControls");
		if (Module)
		{
			Module->NotifyMultiSelectedFormatsChanged();
		}
	}

	if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetName().StartsWith(TEXT("bPackageForMetaQuest")))
	{
		HandleMetaQuestSupport();
	}

	HandlesRGBHWSupport();
}

void UOpenHarmonyRuntimeSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// If the config has an AdMobAdUnitID then we migrate it on load and clear the value
	if (!AdMobAdUnitID.IsEmpty())
	{
		AdMobAdUnitIDs.Add(AdMobAdUnitID);
		AdMobAdUnitID.Empty();
		TryUpdateDefaultConfigFile();
	}

	EnsureValidGPUArch();
	HandlesRGBHWSupport();
	HandleMetaQuestSupport();
}

void UOpenHarmonyRuntimeSettings::EnsureValidGPUArch()
{
	// Ensure that at least one GPU architecture is supported
	if (!bSupportsVulkan && !bBuildForES31 && !bSupportsVulkanSM5)
	{
		UE_LOG(LogOpenHarmonyRuntimeSettings, Warning, TEXT("No GPU architecture is selected.\n"));
		// Default to Vulkan for Meta Quest devices
		if (bPackageForMetaQuest)
		{
			bSupportsVulkan = true;
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bSupportsVulkan)), GetDefaultConfigFilename());
			UE_LOG(LogOpenHarmonyRuntimeSettings, Warning, TEXT("Support Vulkan has been changed to true.\n"));
		}
		else
		{
			bBuildForES31 = true;
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bBuildForES31)), GetDefaultConfigFilename());
			UE_LOG(LogOpenHarmonyRuntimeSettings, Warning, TEXT("Support OpenGL ES3.2 has been changed to true.\n"));
		}

		// Supported shader formats changed so invalidate cache
		InvalidateAllOpenHarmonyPlatforms();
	}
}
#endif

