// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using System.IO;
using UnrealBuildTool;

public class OpenHarmonyDeviceProfileCommandlets : ModuleRules
{
	public OpenHarmonyDeviceProfileCommandlets(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Developer/OpenHarmony/OpenHarmonyDeviceDetection/Public");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"Json",
				"JsonUtilities",
				"OpenHarmonyDeviceProfileSelector",
				"PIEPreviewDeviceSpecification"
			}
			);
	}
}
