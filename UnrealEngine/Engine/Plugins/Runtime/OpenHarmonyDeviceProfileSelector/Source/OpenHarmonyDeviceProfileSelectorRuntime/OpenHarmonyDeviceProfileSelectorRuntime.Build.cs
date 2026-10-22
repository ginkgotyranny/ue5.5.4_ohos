// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class OpenHarmonyDeviceProfileSelectorRuntime : ModuleRules
	{
        public OpenHarmonyDeviceProfileSelectorRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "OpenHarmonyDPSRT";

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
				    "Core",
				    "CoreUObject",
				    "Engine",
					"OpenHarmonyDeviceProfileSelector",
					"HeadMountedDisplay",
				}
				);
		}
	}
}
