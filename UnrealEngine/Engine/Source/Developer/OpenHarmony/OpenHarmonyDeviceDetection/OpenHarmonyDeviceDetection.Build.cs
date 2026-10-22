using UnrealBuildTool;

public class OpenHarmonyDeviceDetection : ModuleRules
{
	public OpenHarmonyDeviceDetection( ReadOnlyTargetRules Target ) : base(Target)
	{
		BinariesSubFolder = "OpenHarmony";

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Json",
                "JsonUtilities",
                "PIEPreviewDeviceSpecification"
            }
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"TcpMessaging",
			}
		);

        if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PIEPreviewDeviceProfileSelector",
					"DesktopPlatform",
					"SlateCore",
					"Slate",
				}
			);
		}

		DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                "TcpMessaging"
            }
        );
    }
}
