// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenHarmonyMoviePlayer : ModuleRules
	{
        public OpenHarmonyMoviePlayer(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"ApplicationCore",
				});

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"OpenHarmonyMoviePlayer/Private",
					// "OpenHarmonyMoviePlayer/Private/Player",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Media",
					"MediaUtils",
					"Engine",
					// "OpenGL"
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
				    "Engine",
                    "MoviePlayer",
                    "RenderCore",
					// "OpenGLDrv",
                    // "RHI",
                    // "Slate"
				}
				);
		}
	}
}
