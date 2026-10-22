using UnrealBuildTool;

public class OpenHarmonyNativeGlue : ModuleRules
{
	public OpenHarmonyNativeGlue(ReadOnlyTargetRules Target) : base(Target)
	{

		if (Target.Platform == UnrealTargetPlatform.OpenHarmony)
		{
			BinariesSubFolder = "OpenHarmony";

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Launch"
				}
			);

			CircularlyReferencedDependentModules.Add("Launch");
		}
	}
}