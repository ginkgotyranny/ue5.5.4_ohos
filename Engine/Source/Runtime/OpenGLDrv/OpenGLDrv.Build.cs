// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Linux", "Android", "LinuxArm64","OpenHarmony")]
[SupportedPlatformGroups("Windows")]
public class OpenGLDrv : ModuleRules
{
	public OpenGLDrv(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"RHI",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"ApplicationCore",
			"Engine",
			"RHICore",
			"RenderCore",
			"PreLoadScreen"
		});

		PrivateIncludePathModuleNames.Add("ImageWrapper");
		DynamicallyLoadedModuleNames.Add("ImageWrapper");

		PublicIncludePathModuleNames.Add("OpenGL");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}

		if ((Target.Platform == UnrealTargetPlatform.Android) || (Target.Platform == UnrealTargetPlatform.OpenHarmony))
		{
			PrivateDependencyModuleNames.Add("detex");
			PrivateDependencyModuleNames.Add("ArmlibGPUInfo");
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
        {
			// for Swappy
			PrivateDefinitions.Add("USE_ANDROID_OPENGL_SWAPPY=1");

            PrivateDependencyModuleNames.Add("GoogleGameSDK");
			PrivateIncludePathModuleNames.Add("Launch");
		}
		
		if (Target.Platform == UnrealTargetPlatform.OpenHarmony)
		{
			// Swappy 是一个由 Google 提供的库，用于优化 Android 游戏的帧率和性能。
			// PrivateDefinitions.Add("USE_OPENHARMONY_OPENGL_SWAPPY=0");

			// PrivateDependencyModuleNames.Add("GoogleGameSDK");
			PrivateIncludePathModuleNames.Add("Launch");
		}

        if (!Target.IsInPlatformGroup(UnrealPlatformGroup.Windows)
			&& !Target.IsInPlatformGroup(UnrealPlatformGroup.IOS)
			&& Target.Platform != UnrealTargetPlatform.Android
			&& Target.Platform != UnrealTargetPlatform.OpenHarmony
			&& !Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}

		PrivateDefinitions.Add(Target.Platform == UnrealTargetPlatform.Android ? "USE_ANDROID_OPENGL=1" : "USE_ANDROID_OPENGL=0");
		PrivateDefinitions.Add(Target.Platform == UnrealTargetPlatform.OpenHarmony ? "USE_OPENHARMONY_OPENGL=1" : "USE_OPENHARMONY_OPENGL=0");
	}
}
