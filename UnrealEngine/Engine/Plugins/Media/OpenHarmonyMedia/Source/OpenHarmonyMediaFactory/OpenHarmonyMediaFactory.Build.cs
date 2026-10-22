// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenHarmonyMediaFactory : ModuleRules
	{
		public OpenHarmonyMediaFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MediaAssets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"OpenHarmonyMedia",
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"OpenHarmonyMediaFactory/Private",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
				});

			if (Target.Platform == UnrealTargetPlatform.OpenHarmony)
			{
				DynamicallyLoadedModuleNames.Add("OpenHarmonyMedia");
			}
		}
	}
}
