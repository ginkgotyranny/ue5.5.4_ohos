// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OpenHarmonyDeviceProfileMatchingRules.generated.h"

UENUM()
enum EOHSourceType : int
{
	OHSRC_PreviousRegexMatch,
	OHSRC_GpuFamily,
	OHSRC_GlVersion,
	OHSRC_OpenHarmonyVersion,
	OHSRC_DeviceMake,
	OHSRC_DeviceModel,
	OHSRC_DeviceBuildNumber,
	OHSRC_VulkanVersion,
	OHSRC_UsingHoudini,
	OHSRC_VulkanAvailable,
	OHSRC_CommandLine,
	OHSRC_Hardware,
	OHSRC_Chipset,
	OHSRC_ConfigRuleVar,
	OHSRC_HMDSystemName,
	OHSRC_SM5Available,
	OHSRC_MAX,
};

UENUM()
enum EOHCompareType : int
{
	OHCMP_Equal,
	OHCMP_Less,
	OHCMP_LessEqual,
	OHCMP_Greater,
	OHCMP_GreaterEqual,
	OHCMP_NotEqual,
	OHCMP_Regex,
	OHCMP_EqualIgnore,
	OHCMP_LessIgnore,
	OHCMP_LessEqualIgnore,
	OHCMP_GreaterIgnore,
	OHCMP_GreaterEqualIgnore,
	OHCMP_NotEqualIgnore,
	OHCMP_Hash,
	OHCMP_MAX,
};

USTRUCT()
struct FOHProfileMatchItem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TEnumAsByte<EOHSourceType> SourceType=EOHSourceType::OHSRC_MAX;

	UPROPERTY()
	TEnumAsByte<EOHCompareType> CompareType=EOHCompareType::OHCMP_MAX;

	UPROPERTY()
	FString MatchString;
};

USTRUCT()
struct FOHProfileMatch
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Profile;
	
	UPROPERTY()
	TArray<FOHProfileMatchItem> Match;
};

UCLASS(config = DeviceProfiles)
class UOpenHarmonyDeviceProfileMatchingRules : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// Use OpenHarmony .ini files in the editor
	virtual const TCHAR* GetConfigOverridePlatform() const override
	{
		return TEXT("OpenHarmony");
	}

	/** Array of rules to match */
	UPROPERTY(EditAnywhere, config, Category = "Matching Rules")
	TArray<FOHProfileMatch> MatchProfile;
};
