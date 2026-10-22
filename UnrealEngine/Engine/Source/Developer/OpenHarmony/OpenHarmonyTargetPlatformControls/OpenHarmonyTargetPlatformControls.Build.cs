using UnrealBuildTool;

public class OpenHarmonyTargetPlatformControls : ModuleRules
{
	public OpenHarmonyTargetPlatformControls(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;
		BinariesSubFolder = "OpenHarmony";
		// We need a short name here since this can run afoul very easily of the `MAX_PATH` limit when
		// combined with building other targets that make use of this.
		ShortName = "OHTPCon";

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
				"OpenHarmonyDeviceDetection",
				"AudioPlatformConfiguration",
				"OpenHarmonyTargetPlatformSettings"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"OpenHarmonyTargetPlatformSettings"
			}
			);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");     //@todo openharmony: OpenHarmonyTargetPlatform.Build
		}

		PublicDefinitions.Add("WITH_OGGVORBIS=1");
	}
}
