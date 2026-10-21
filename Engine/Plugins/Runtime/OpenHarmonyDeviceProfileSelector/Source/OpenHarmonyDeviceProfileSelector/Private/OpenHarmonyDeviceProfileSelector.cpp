// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyDeviceProfileSelector.h"
#include "OpenHarmonyDeviceProfileMatchingRules.h"
#include "OpenHarmonyArkTSSurfaceViewDevices.h"
#include "Internationalization/Regex.h"
#include "Misc/CommandLine.h"

#if PLATFORM_OPENHARMONY
#include "OpenHarmony/OpenHarmonyPlatformMisc.h"
#endif

UOpenHarmonyDeviceProfileMatchingRules::UOpenHarmonyDeviceProfileMatchingRules(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UOpenHarmonyArkTSSurfaceViewDevices::UOpenHarmonyArkTSSurfaceViewDevices(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static UOpenHarmonyDeviceProfileMatchingRules* GetOpenHarmonyDeviceProfileMatchingRules()
{
	// We need to initialize the class early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
	extern UClass* Z_Construct_UClass_UOpenHarmonyDeviceProfileMatchingRules();
	CreatePackage(UOpenHarmonyDeviceProfileMatchingRules::StaticPackage());
	Z_Construct_UClass_UOpenHarmonyDeviceProfileMatchingRules();

	// Get the default object which will has the values from DeviceProfiles.ini
	UOpenHarmonyDeviceProfileMatchingRules* Rules = Cast<UOpenHarmonyDeviceProfileMatchingRules>(UOpenHarmonyDeviceProfileMatchingRules::StaticClass()->GetDefaultObject());
	check(Rules);
	return Rules;
}

TMap<FName, FString> FOpenHarmonyDeviceProfileSelector::SelectorProperties;

void FOpenHarmonyDeviceProfileSelector::VerifySelectorParams()
{
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_GPUFamily));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_GLVersion));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_OpenHarmonyVersion));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceMake));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceModel));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceBuildNumber));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_VulkanVersion));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_UsingHoudini));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_VulkanAvailable));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_Hardware));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_Chipset));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_HMDSystemName));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_TotalPhysicalGB));
	check(SelectorProperties.Find(FOpenHarmonyProfileSelectorSourceProperties::SRC_SM5Available));
}

FString FOpenHarmonyDeviceProfileSelector::FindMatchingProfile(const FString& FallbackProfileName)
{
	FString OutProfileName = FallbackProfileName;
	FString CommandLine = FCommandLine::Get();
	
	FString GPUFamily			= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_GPUFamily);
	FString GLVersion			= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_GLVersion);
	FString OpenHarmonyVersion		= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_OpenHarmonyVersion);
	FString DeviceMake			= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceMake);
	FString DeviceModel			= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceModel);
	FString DeviceBuildNumber	= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_DeviceBuildNumber);
	FString VulkanVersion		= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_VulkanVersion);
	FString UsingHoudini		= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_UsingHoudini);
	FString VulkanAvailable		= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_VulkanAvailable);
	FString Hardware			= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_Hardware);
	FString Chipset				= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_Chipset);
	FString TotalPhysicalGB		= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_TotalPhysicalGB);
	FString HMDSystemName		= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_HMDSystemName);
	FString SM5Available		= SelectorProperties.FindChecked(FOpenHarmonyProfileSelectorSourceProperties::SRC_SM5Available);

	for (const FOHProfileMatch& Profile : GetOpenHarmonyDeviceProfileMatchingRules()->MatchProfile)
	{
		FString PreviousRegexMatch;
		bool bFoundMatch = true;
		for (const FOHProfileMatchItem& Item : Profile.Match)
		{
			FString ConfigRuleString;
			const FString* SourceString = nullptr;
			FString MatchString = Item.MatchString;
			switch (Item.SourceType)
			{
			case EOHSourceType::OHSRC_PreviousRegexMatch:
				SourceString = &PreviousRegexMatch;
				break;
			case EOHSourceType::OHSRC_GpuFamily:
				SourceString = &GPUFamily;
				break;
			case EOHSourceType::OHSRC_GlVersion:
				SourceString = &GLVersion;
				break;
			case EOHSourceType::OHSRC_OpenHarmonyVersion:
				SourceString = &OpenHarmonyVersion;
				break;
			case EOHSourceType::OHSRC_DeviceMake:
				SourceString = &DeviceMake;
				break;
			case EOHSourceType::OHSRC_DeviceModel:
				SourceString = &DeviceModel;
				break;
			case EOHSourceType::OHSRC_DeviceBuildNumber:
				SourceString = &DeviceBuildNumber;
				break;
			case EOHSourceType::OHSRC_VulkanVersion:
				SourceString = &VulkanVersion;
				break;
			case EOHSourceType::OHSRC_UsingHoudini:
				SourceString = &UsingHoudini;
				break;
			case EOHSourceType::OHSRC_VulkanAvailable:
				SourceString = &VulkanAvailable;
				break;
			case EOHSourceType::OHSRC_CommandLine:
				SourceString = &CommandLine;
				break;
			case EOHSourceType::OHSRC_Hardware:
				SourceString = &Hardware;
				break;
			case EOHSourceType::OHSRC_Chipset:
				SourceString = &Chipset;
				break;
			case EOHSourceType::OHSRC_HMDSystemName:
				SourceString = &HMDSystemName;
				break;
			case EOHSourceType::OHSRC_SM5Available:
				SourceString = &SM5Available;
				break;
			case EOHSourceType::OHSRC_ConfigRuleVar:
				{
					// expected Matchstring contents for configrulevar is "configrule_varname|matchstring"
					// sourcestring is set to the configrule variable content. 
					// sourcestring will be an empty string if configrule_varname is not found.
					FString VariableValueMatchString;
					FString ConfigRuleVarName;
					SourceString = &ConfigRuleString;
					if (MatchString.Split(TEXT("|"), &ConfigRuleVarName, &VariableValueMatchString))
					{
						MatchString = VariableValueMatchString;
#if PLATFORM_OPENHARMONY
						// TODO: Do this when running device selection with editor builds.
						if (FString* ConfigRuleVar = FOpenHarmonyMisc::GetConfigRulesVariable(ConfigRuleVarName))
						{
							ConfigRuleString = *ConfigRuleVar;
						}
#endif
					}					
					break;
				}
			default:
				continue;
			}

			const bool bNumericOperands = SourceString->IsNumeric() && MatchString.IsNumeric();

			switch (Item.CompareType)
			{
			case OHCMP_Equal:
				if (Item.SourceType == OHSRC_CommandLine) 
				{
					if (!FParse::Param(*CommandLine, *MatchString))
					{
						bFoundMatch = false;
					}
				}
				else
				{
					if (*SourceString != MatchString)
					{
						bFoundMatch = false;
					}
				}
				break;
			case OHCMP_Less:
				if ((bNumericOperands && FCString::Atof(**SourceString) >= FCString::Atof(*MatchString)) || (!bNumericOperands && *SourceString >= MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_LessEqual:
				if ((bNumericOperands && FCString::Atof(**SourceString) > FCString::Atof(*MatchString)) || (!bNumericOperands && *SourceString > MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_Greater:
				if ((bNumericOperands && FCString::Atof(**SourceString) <= FCString::Atof(*MatchString)) || (!bNumericOperands && *SourceString <= MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_GreaterEqual:
				if ((bNumericOperands && FCString::Atof(**SourceString) < FCString::Atof(*MatchString)) || (!bNumericOperands && *SourceString < MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_NotEqual:
				if (Item.SourceType == OHSRC_CommandLine)
				{
					if (FParse::Param(*CommandLine, *MatchString))
					{
						bFoundMatch = false;
					}
				}
				else
				{
					if (*SourceString == MatchString)
					{
						bFoundMatch = false;
					}
				}
				break;
			case OHCMP_EqualIgnore:
				if (SourceString->ToLower() != MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_LessIgnore:
				if (SourceString->ToLower() >= MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_LessEqualIgnore:
				if (SourceString->ToLower() > MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_GreaterIgnore:
				if (SourceString->ToLower() <= MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_GreaterEqualIgnore:
				if (SourceString->ToLower() < MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_NotEqualIgnore:
				if (SourceString->ToLower() == MatchString.ToLower())
				{
					bFoundMatch = false;
				}
				break;
			case OHCMP_Regex:
				{
					const FRegexPattern RegexPattern(MatchString);
					FRegexMatcher RegexMatcher(RegexPattern, *SourceString);
					if (RegexMatcher.FindNext())
					{
						PreviousRegexMatch = RegexMatcher.GetCaptureGroup(1);
					}
					else
					{
						bFoundMatch = false;
					}
				}
			break;
				case OHCMP_Hash:
				{
					// Salt string is concatenated onto the end of the input text.
					// For example the input string "PhoneModel" with salt "Salt" and pepper "Pepper" can be computed with
					// % printf "PhoneModelSaltPepper" | openssl dgst -sha1 -hex
					// resulting in d9e5cbd6b0e4dba00edd9de92cf64ee4c3f3a2db and would be stored in the matching rules as 
					// "Salt|d9e5cbd6b0e4dba00edd9de92cf64ee4c3f3a2db". Salt is optional.
					FString MatchHashString;
					FString SaltString;
					if (!MatchString.Split(TEXT("|"), &SaltString, &MatchHashString))
					{
						MatchHashString = MatchString;
					}
					FString HashInputString = *SourceString + SaltString
#ifdef HASH_PEPPER_SECRET_GUID
						+ HASH_PEPPER_SECRET_GUID.ToString()
#endif
						;

					FSHAHash SourceHash;
					FSHA1::HashBuffer(TCHAR_TO_ANSI(*HashInputString), HashInputString.Len(), SourceHash.Hash);
					if (SourceHash.ToString() != MatchHashString.ToUpper())
					{
						bFoundMatch = false;
					}
				}
				break;
			default:
				bFoundMatch = false;
			}

			if (!bFoundMatch)
			{
				break;
			}
		}

		if (bFoundMatch)
		{
			OutProfileName = Profile.Profile;
			break;
		}
	}
	return OutProfileName;
}

int32 FOpenHarmonyDeviceProfileSelector::GetNumProfiles()
{
	return GetOpenHarmonyDeviceProfileMatchingRules()->MatchProfile.Num();
}
