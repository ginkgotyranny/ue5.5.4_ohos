// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenHarmonyLocalNotification : ModuleRules
{
	public OpenHarmonyLocalNotification(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "OpenHarmony";

		PublicIncludePathModuleNames.AddRange(new string[]
		{
			"OpenHarmonyLocalNotification",
			"Engine",
		});

        PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
            "Launch"
		});
	}
}
