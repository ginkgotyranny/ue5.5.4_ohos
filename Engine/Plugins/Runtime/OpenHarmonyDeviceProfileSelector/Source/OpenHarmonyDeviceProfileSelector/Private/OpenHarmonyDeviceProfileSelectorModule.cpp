// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyDeviceProfileSelectorModule.h"
#include "OpenHarmonyDeviceProfileSelector.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FOpenHarmonyDeviceProfileSelectorModule, OpenHarmonyDeviceProfileSelector);

DEFINE_LOG_CATEGORY_STATIC(LogOpenHarmonyDPSelector, Log, All)

void FOpenHarmonyDeviceProfileSelectorModule::StartupModule()
{
}

void FOpenHarmonyDeviceProfileSelectorModule::ShutdownModule()
{
}

const FString FOpenHarmonyDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	// We are not expecting this module to have GetRuntimeDeviceProfileName called directly.
	// OpenHarmony ProfileSelectorModule runtime is now in FOpenHarmonyDeviceProfileSelectorRuntimeModule.
	// Use GetDeviceProfileName.
	checkNoEntry();
	return FString();
}

const FString FOpenHarmonyDeviceProfileSelectorModule::GetDeviceProfileName()
{
	FString ProfileName; 

	// ensure SelectorProperties does actually contain our parameters
	check(FOpenHarmonyDeviceProfileSelector::GetSelectorProperties().Num() > 0);

	UE_LOG(LogOpenHarmonyDPSelector, Log, TEXT("Checking %d rules from DeviceProfile ini file."), FOpenHarmonyDeviceProfileSelector::GetNumProfiles() );
	UE_LOG(LogOpenHarmonyDPSelector, Log, TEXT("  Default profile: %s"), *ProfileName);
	for (const TTuple<FName,FString>& MapIt : FOpenHarmonyDeviceProfileSelector::GetSelectorProperties())
	{
		UE_LOG(LogOpenHarmonyDPSelector, Log, TEXT("  %s: %s"), *MapIt.Key.ToString(), *MapIt.Value);
	}

	ProfileName = FOpenHarmonyDeviceProfileSelector::FindMatchingProfile(ProfileName);

	UE_LOG(LogOpenHarmonyDPSelector, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);

	return ProfileName;
}

bool FOpenHarmonyDeviceProfileSelectorModule::GetSelectorPropertyValue(const FName& PropertyType, FString& PropertyValueOUT)
{
	if (const FString* Found = FOpenHarmonyDeviceProfileSelector::GetSelectorProperties().Find(PropertyType))
	{
		PropertyValueOUT = *Found;
		return true;
	}
	// Special case for non-existent config rule variables
	// they should return true and a value of '[null]'
	// this prevents configrule issues from throwing errors.
	if (PropertyType.ToString().StartsWith(TEXT("OHSRC_ConfigRuleVar[")))
	{
		PropertyValueOUT = TEXT("[null]");
		return true;
	}

	return false;
}

void FOpenHarmonyDeviceProfileSelectorModule::SetSelectorProperties(const TMap<FName, FString>& SelectorPropertiesIn)
{
	FOpenHarmonyDeviceProfileSelector::SetSelectorProperties(SelectorPropertiesIn);
}
