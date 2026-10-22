// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenHarmonyRuntimeSettings : ModuleRules
{
	public OpenHarmonyRuntimeSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "OpenHarmony";

        PublicDependencyModuleNames.Add("AudioPlatformConfiguration");

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "Engine"
            }
		);

        if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
			    {
                    "TargetPlatform"
			    }
            );

            PrivateIncludePathModuleNames.Add("OpenHarmonyTargetPlatform");
            PrivateIncludePathModuleNames.Add("OpenHarmonyTargetPlatformControls");
        }
	}
}
