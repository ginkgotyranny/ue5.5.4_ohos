using UnrealBuildTool;

public class OpenHarmonyTargetPlatform : ModuleRules
{
	public OpenHarmonyTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;
		BinariesSubFolder = "OpenHarmony";

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"TargetPlatform",
                "DesktopPlatform",
				"OpenHarmonyDeviceDetection",
                "AudioPlatformConfiguration"
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
