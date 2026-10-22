// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using System.IO;
using UnrealBuildTool;

public class OpenHarmonyDeviceProfileSelector : ModuleRules
{
	public OpenHarmonyDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "OpenHarmonyDPS";

		// get a value from .ini (ConfigFile attribute doesn't work in Build.cs files it seems)
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, Target.ProjectFile != null ? Target.ProjectFile.Directory : null, Target.Platform);
		string SecretGuid;
		Ini.GetString("OpenHarmonyDPSBuildSettings", "SecretGuid", out SecretGuid);
		if (!string.IsNullOrEmpty(SecretGuid))
        {
			PrivateDefinitions.Add("HASH_PEPPER_SECRET_GUID="+ SecretGuid);
		}

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
			}
			);
	}
}
