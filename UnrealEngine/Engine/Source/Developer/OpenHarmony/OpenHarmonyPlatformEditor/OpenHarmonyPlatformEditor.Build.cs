using UnrealBuildTool;

public class OpenHarmonyPlatformEditor : ModuleRules
{
	public OpenHarmonyPlatformEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error; 
		BinariesSubFolder = "OpenHarmony";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",				
                "EditorWidgets",
                "DesktopWidgets",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"SourceControl",
				"OpenHarmonyRuntimeSettings",
                "OpenHarmonyDeviceDetection",
                "DesktopPlatform",
                "RenderCore",
                "RHI",
                "MaterialShaderQualitySettings",
				"MainFrame",
                "AudioSettingsEditor"
            }
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);
	}
}
