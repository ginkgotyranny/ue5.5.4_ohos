// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenHarmonyMedia : ModuleRules
	{
		public OpenHarmonyMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
					"ApplicationCore",
					"PakFile",
					"RHI",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"OpenHarmonyMediaFactory",
					"Core",
					"Engine",
					"MediaUtils",
					"RenderCore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"OpenHarmonyMedia/Private",
					"OpenHarmonyMedia/Private/Player",
				});
		}
	}
}
