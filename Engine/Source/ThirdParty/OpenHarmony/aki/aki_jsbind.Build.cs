// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class aki_jsbind : ModuleRules
{
	public aki_jsbind(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.OpenHarmony)
		{
			PublicDefinitions.Add("AKI_BUILDING_SHARED=0");
			
			PublicDefinitions.Add("AKI_ENABLE_NAPI=1");
			
			PublicDefinitions.Add("JSBIND_USING_NAPI=1");
			
			PublicDefinitions.Add("JSBIND_SUPPORT_DECLARATION=0");
			
			PublicDefinitions.Add("JSBIND_USING_TRACING=0");

			PublicDefinitions.Add("AKI_ENABLE_CXX_STANDARD_11=1");
			
			PublicIncludePaths.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "OpenHarmony", "aki", "include"));
		
			PublicAdditionalLibraries.Add(Path.Combine(Target.UEThirdPartySourceDirectory, "OpenHarmony", "aki", "lib", "libaki_jsbind.a"));
		}
	}
}
