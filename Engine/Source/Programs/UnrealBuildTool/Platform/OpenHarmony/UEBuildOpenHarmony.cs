// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// OpenHarmony-specific target settings
	/// </summary>
	public partial class OpenHarmonyTargetRules
	{
		/// <summary>
		/// Enables address sanitizer (ASan)
		/// </summary>
		[CommandLine("-EnableASan")]
		public bool bEnableAddressSanitizer = false;

		/// <summary>
		/// Enables HW address sanitizer (HWASan)
		/// </summary>
		[CommandLine("-EnableHWASan")]
		public bool bEnableHWAddressSanitizer = false;

		/// <summary>
		/// Enables thread sanitizer (TSan)
		/// </summary>
		//[CommandLine("-EnableTSan")]
		public bool bEnableThreadSanitizer = false;

		/// <summary>
		/// Enables undefined behavior sanitizer (UBSan)
		/// </summary>
		[CommandLine("-EnableUBSan")]
		public bool bEnableUndefinedBehaviorSanitizer = false;

		/// <summary>
		/// Enables minimal undefined behavior sanitizer (UBSan)
		/// </summary>
		[CommandLine("-EnableMinUBSan")]
		public bool bEnableMinimalUndefinedBehaviorSanitizer = false;

		/// <summary>
		/// Enables runtime ray tracing support.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenHarmonyTargetPlatform.OpenHarmonyTargetSettings")]
		public bool bEnableRayTracing = false;

		/// <summary>
		/// Enables ASIS plugin and STANDALONE support.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/OpenHarmonySingleInstanceServiceEditor.OpenHarmonySingleInstanceServiceRuntimeSettings")]
		public bool bEnableASISPlugin = false;
	}

	/// <summary>
	/// Read-only wrapper for OpenHarmony-specific target settings
	/// </summary>
	public partial class ReadOnlyOpenHarmonyTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private OpenHarmonyTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyOpenHarmonyTargetRules(OpenHarmonyTargetRules Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>
		#region Read-only accessor properties 
#pragma warning disable CS1591

		public bool bEnableAddressSanitizer => Inner.bEnableAddressSanitizer;

		public bool bEnableHWAddressSanitizer => Inner.bEnableHWAddressSanitizer;

		public bool bEnableThreadSanitizer => Inner.bEnableThreadSanitizer;

		public bool bEnableUndefinedBehaviorSanitizer => Inner.bEnableUndefinedBehaviorSanitizer;

		public bool bEnableMinimalUndefinedBehaviorSanitizer => Inner.bEnableMinimalUndefinedBehaviorSanitizer;

		public bool bEnableRayTracing => Inner.bEnableRayTracing;

		public bool bEnableASISPlugin => Inner.bEnableASISPlugin;

		public OpenHarmonyTargetRules TargetRules => Inner;

#pragma warning restore CS1591
		#endregion
	}

	class OpenHarmonyArchitectureConfig : UnrealArchitectureConfig
	{
		public OpenHarmonyArchitectureConfig()
			: base(UnrealArchitectureMode.SingleTargetLinkSeparately, new[] { UnrealArch.Arm64, UnrealArch.X64 })
		{

		}

		public override UnrealArchitectures ActiveArchitectures(FileReference? ProjectFile, string? TargetName) => GetProjectArchitectures(ProjectFile, false);

		public override string GetFolderNameForArchitecture(UnrealArch Architecture)
		{
			return Architecture == UnrealArch.Arm64 ? "a" : "x";
		}

		private static UnrealArchitectures? CachedActiveArchitectures = null;
		private static FileReference? CachedActiveArchesProject = null;
		private UnrealArchitectures GetProjectArchitectures(FileReference? ProjectFile, bool bGetAllSupported)
		{
			if (CachedActiveArchitectures == null || ProjectFile != CachedActiveArchesProject)
			{
				List<string> ActiveArches = new();
				CachedActiveArchesProject = ProjectFile;

				// look in ini settings for what platforms to compile for
				ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
				bool bBuild;
				bool bUnsupportedBinaryBuildArch = false;

				if (Ini.GetBool("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "bBuildForArm64", out bBuild) && bBuild)
				{
					ActiveArches.Add("arm64");
				}
				if (Ini.GetBool("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "bBuildForx8664", out bBuild) && bBuild)
				{
					ActiveArches.Add("x64");
				}

				// we expect one to be specified
				if (ActiveArches.Count == 0)
				{
					if (bUnsupportedBinaryBuildArch)
					{
						throw new BuildException("Only architectures unsupported by binary-only engine selected.");
					}
					else
					{ 
						throw new BuildException("At least one architecture must be specified in OpenHarmony project settings.");
					}
				}

				CachedActiveArchitectures = new UnrealArchitectures(ActiveArches);
			}
			return CachedActiveArchitectures;
		}
	}

	class OpenHarmonyPlatform : UEBuildPlatform
	{
		UEBuildPlatformSDK SDK;

		public OpenHarmonyPlatform(UnrealTargetPlatform InTargetPlatform, UEBuildPlatformSDK InSDK, ILogger InLogger)
			: base(InTargetPlatform, InSDK, new OpenHarmonyArchitectureConfig(), InLogger)
		{
			SDK = InSDK;
		}

		public OpenHarmonyPlatform(OpenHarmonyPlatformSDK InSDK, ILogger InLogger) : this(UnrealTargetPlatform.OpenHarmony, InSDK, InLogger)
		{
		}

		public override void ResetTarget(TargetRules Target)
		{
			ValidateTarget(Target);

			Target.bDeployAfterCompile = true;
		}

		public override void ValidateTarget(TargetRules Target)
		{
			if (!String.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE")))
			{
				Target.StaticAnalyzer = StaticAnalyzer.Default;
				Target.StaticAnalyzerOutputType = (Environment.GetEnvironmentVariable("CLANG_ANALYZER_OUTPUT")?.Contains("html", StringComparison.OrdinalIgnoreCase) == true) ? StaticAnalyzerOutputType.Html : StaticAnalyzerOutputType.Text;
				Target.StaticAnalyzerMode = String.Equals(Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE"), "shallow", StringComparison.OrdinalIgnoreCase) ? StaticAnalyzerMode.Shallow : StaticAnalyzerMode.Deep;
			}
			else if (Target.StaticAnalyzer == StaticAnalyzer.Clang)
			{
				Target.StaticAnalyzer = StaticAnalyzer.Default;
			}

			// Disable linking and ignore build outputs if we're using a static analyzer
			if (Target.StaticAnalyzer == StaticAnalyzer.Default)
			{
				Target.bDisableLinking = true;
				Target.bIgnoreBuildOutputs = true;

				// Clang static analysis requires non unity builds
				Target.bUseUnityBuild = false;
			}

			Target.bCompileRecast = true;
			Target.bCompileISPC = false;

			// disable plugins by architecture (if we are compiling for multiple architectures, we still need to disable the plugin for all architectures)
			if (Target.Architectures.Contains(UnrealArch.Arm64) && Target.Name != "UnrealHeaderTool")
			{
				Target.DisablePlugins.AddRange(new string[]
				{

				});
			}
		}

		public override bool CanUseXGE()
		{
			// Disable when building on Linux
			return BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux;
		}

		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return IsBuildProductWithArch(FileName, NamePrefixes, NameSuffixes, ".so")
				|| IsBuildProductWithArch(FileName, NamePrefixes, NameSuffixes, ".hap")
				|| IsBuildProductWithArch(FileName, NamePrefixes, NameSuffixes, ".a");
		}

		static bool IsBuildProductWithArch(string Name, string[] NamePrefixes, string[] NameSuffixes, string Extension)
		{
			// Strip off the extension, then a CPU suffix, before testing whether it matches a build product name.
			if (Name.EndsWith(Extension, StringComparison.InvariantCultureIgnoreCase))
			{
				int ExtensionEndIdx = Name.Length - Extension.Length;
				foreach (string CpuSuffix in OpenHarmonyToolChain.AllCpuSuffixes.Values)
				{
					int CpuIdx = ExtensionEndIdx - CpuSuffix.Length;
					if (CpuIdx > 0 && String.Compare(Name, CpuIdx, CpuSuffix, 0, CpuSuffix.Length, StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						return IsBuildProductName(Name, 0, CpuIdx, NamePrefixes, NameSuffixes);
					}
				}
			}
			return false;
		}

		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".so";
				case UEBuildBinaryType.Executable:
					return ".so";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		public override string[] GetDebugInfoExtensions(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			return new string[] { };
		}

		public override void FindAdditionalBuildProductsToClean(ReadOnlyTargetRules Target, List<FileReference> FilesToDelete, List<DirectoryReference> DirectoriesToDelete)
		{
			base.FindAdditionalBuildProductsToClean(Target, FilesToDelete, DirectoriesToDelete);

			if (Target.ProjectFile != null)
			{
				DirectoriesToDelete.Add(DirectoryReference.Combine(DirectoryReference.FromFile(Target.ProjectFile), "Intermediate", "OpenHarmony"));
			}
		}

		public virtual bool HasSpecificDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectPath, ILogger Logger)
		{
			string[] BoolKeys = new string[] {
				"bBuildForArm64", "bBuildForX8664",
				"bBuildForES31", "bBuildWithHiddenSymbolVisibility", "bSaveSymbols"
			};
			string[] StringKeys = new string[] {
				"NDKAPILevelOverride"
			};

			// look up OpenHarmony specific settings
			if (!DoProjectSettingsMatchDefault(Platform, ProjectPath, "/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings",
				BoolKeys, null, StringKeys, Logger))
			{
				return false;
			}
			return true;
		}

		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectPath)
		{
			// @todo Lumin: This is kinda messy - better way?
			if (HasSpecificDefaultBuildConfig(Platform, ProjectPath, Logger) == false)
			{
				return false;
			}

			// any shared-between-all-OpenHarmonys would be here

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectPath);
		}

		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			// This platform currently always compiles monolithic
			return true;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			// don't do any target platform stuff if not available for host and opted in
			// do not require SDK to build it since we don't necessarily need it for editor building
			if (!UEBuildPlatform.IsPlatformAvailableForTarget(Platform, Target, bIgnoreSDKCheck: true))
			{
				return;
			}

			if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac) || (Target.Platform == UnrealTargetPlatform.Linux))
			{
				bool bBuildShaderFormats = Target.bForceBuildShaderFormats;
				if (!Target.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (Target.bBuildDeveloperTools)
						{
							Rules.DynamicallyLoadedModuleNames.Add("OpenHarmonyTargetPlatform");
							Rules.DynamicallyLoadedModuleNames.Add("OpenHarmonyTargetPlatformSettings");
							Rules.DynamicallyLoadedModuleNames.Add("OpenHarmonyTargetPlatformControls");
						}
					}
					else if (ModuleName == "TargetPlatform")
					{
						bBuildShaderFormats = true;
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatETC2");  // ETC2 
						if (Target.bBuildDeveloperTools)
						{
							//Rules.DynamicallyLoadedModuleNames.Add("AudioFormatADPCM");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (Target.bForceBuildTargetPlatforms)
					{
						Rules.DynamicallyLoadedModuleNames.Add("OpenHarmonyTargetPlatform");
						Rules.DynamicallyLoadedModuleNames.Add("OpenHarmonyTargetPlatformSettings");
						Rules.DynamicallyLoadedModuleNames.Add("OpenHarmonyTargetPlatformControls");
					}

					if (bBuildShaderFormats)
					{
						//Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenHarmony");		//@todo OpenHarmony: ShaderFormatOpenHarmony
					}
				}

				if (ModuleName == "UnrealEd")
				{
					Rules.DynamicallyLoadedModuleNames.Add("OpenHarmonyPlatformEditor");
				}
			}
		}

		public override List<FileReference> FinalizeBinaryPaths(FileReference BinaryName, FileReference? ProjectFile, ReadOnlyTargetRules Target)
		{
			// make multiple output binaries
			List<FileReference> AllBinaries = new List<FileReference>();
			foreach (UnrealArch Architecture in Target.Architectures.Architectures)
			{
				string BinaryPath;
				if (Target.bShouldCompileAsDLL)
				{
					BinaryPath = Path.Combine(BinaryName.Directory.FullName, Target.Configuration.ToString(), "libUnreal.so");
				}
				else
				{
					BinaryPath = OpenHarmonyToolChain.InlineArchName(BinaryName.FullName, Architecture);
				}

				AllBinaries.Add(new FileReference(BinaryPath));
			}

			return AllBinaries;
		}

		public override void AddExtraModules(ReadOnlyTargetRules Target, List<string> PlatformExtraModules)
		{
			if (Target.Type != TargetType.Program)
			{
				PlatformExtraModules.Add("VulkanRHI");
			}
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
		}

		public static bool IsMakeAAREnabled(ReadOnlyTargetRules Target)
		{
			return Target.OpenHarmonyPlatform.bEnableASISPlugin;
		}

		public virtual void SetUpSpecificEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment, ILogger Logger)
		{
			string NDKPath = Environment.GetEnvironmentVariable("OHNDKROOT")!;
			// string NDKPath = "D:\\develops\\deveco\\DevEco Studio\\sdk\\default\\openharmony\\native";
			Environment.SetEnvironmentVariable("OHNDKROOT", NDKPath);
			NDKPath = NDKPath.Replace("\"", "");

			OpenHarmonyToolChain ToolChain = new OpenHarmonyToolChain(Target.ProjectFile, Logger);

			CompileEnvironment.Definitions.Add("PLATFORM_DESKTOP=0");
			CompileEnvironment.Definitions.Add("PLATFORM_CAN_SUPPORT_EDITORONLY_DATA=0");

			CompileEnvironment.Definitions.Add("UNICODE");
			CompileEnvironment.Definitions.Add("_UNICODE");

			CompileEnvironment.Definitions.Add("PLATFORM_OPENHARMONY=1");
			CompileEnvironment.Definitions.Add("OPENHARMONY=1");

			CompileEnvironment.Definitions.Add("WITH_EDITOR=0");
			CompileEnvironment.Definitions.Add("USE_NULL_RHI=0");


			DirectoryReference NdkDir = new DirectoryReference(NDKPath);

			CompileEnvironment.SystemIncludePaths.Add(DirectoryReference.Combine(NdkDir, "llvm/include"));

			// the toolchain will actually filter these out
			LinkEnvironment.SystemLibraryPaths.Add(DirectoryReference.Combine(NdkDir, "llvm/lib/aarch64-linux-ohos/c++"));
			//LinkEnvironment.SystemLibraryPaths.Add(DirectoryReference.Combine(NdkDir, "llvm/lib/x86_64-linux-ohos/c++"));
			LinkEnvironment.SystemLibraryPaths.Add(DirectoryReference.Combine(NdkDir, "llvm/lib/aarch64-linux-ohos"));

			if (!UseTegraGraphicsDebugger(Target))
			{
				LinkEnvironment.SystemLibraries.Add("GLESv3");
				LinkEnvironment.SystemLibraries.Add("EGL");
			}
		}

		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{

			CompileEnvironment.Definitions.Add("PLATFORM_DESKTOP=0");
			CompileEnvironment.Definitions.Add("PLATFORM_CAN_SUPPORT_EDITORONLY_DATA=0");

			// TODO 暂不需要OGGVORBIS		
			//CompileEnvironment.Definitions.Add("WITH_OGGVORBIS=1");

			CompileEnvironment.Definitions.Add("UNICODE");
			CompileEnvironment.Definitions.Add("_UNICODE");

			CompileEnvironment.Definitions.Add("PLATFORM_OPENHARMONY=1");
			CompileEnvironment.Definitions.Add("OPENHARMONY=1");

			CompileEnvironment.Definitions.Add("WITH_EDITOR=0");
			CompileEnvironment.Definitions.Add("USE_NULL_RHI=0");
			
			if (Target.OpenHarmonyPlatform.bEnableRayTracing)
			{
				Logger.LogInformation("Compiling with ray tracing enabled");
				CompileEnvironment.Definitions.Add("RHI_RAYTRACING=1");
			}

			if (Target.OpenHarmonyPlatform.bEnableASISPlugin)
			{
				Logger.LogInformation("Compiling with USE_OPENHARMONY_STANDALONE");
				CompileEnvironment.Definitions.Add("USE_OPENHARMONY_STANDALONE=1");
			}

			if (Target.bPGOOptimize || Target.bPGOProfile)
			{
				Logger.LogInformation("PGO {PgoType} build", Target.bPGOOptimize ? "optimize" : "profile");
				if (Target.bPGOOptimize)
				{
					CompileEnvironment.PGODirectory = DirectoryReference.Combine(Target.ProjectFile?.Directory ?? Unreal.WritableEngineDirectory, "Platforms", "OpenHarmony", "Build", "PGO").FullName;
					CompileEnvironment.PGOFilenamePrefix = String.Format("{0}-OpenHarmony", Target.Name);

					LinkEnvironment.PGODirectory = CompileEnvironment.PGODirectory;
					LinkEnvironment.PGOFilenamePrefix = CompileEnvironment.PGOFilenamePrefix;

					Logger.LogInformation("PGO Dir: {PgoDir}", CompileEnvironment.PGODirectory);
					Logger.LogInformation("PGO Prefix: {PgoPrefix}", CompileEnvironment.PGOFilenamePrefix);
				}
			}

			CompileEnvironment.Definitions.Add("INT64_T_TYPES_NOT_LONG_LONG=1");

			SetUpSpecificEnvironment(Target, CompileEnvironment, LinkEnvironment, Logger);

			// deliberately not linking stl or stdc++ here (c++_shared is default)
			LinkEnvironment.SystemLibraries.Add("c");
			LinkEnvironment.SystemLibraries.Add("dl");
			LinkEnvironment.SystemLibraries.Add("m");
			LinkEnvironment.SystemLibraries.Add("z");
			LinkEnvironment.SystemLibraries.Add("uv");
			LinkEnvironment.SystemLibraries.Add("hilog_ndk.z");
			LinkEnvironment.SystemLibraries.Add("hitrace_ndk.z");
			LinkEnvironment.SystemLibraries.Add("ace_napi.z");
			LinkEnvironment.SystemLibraries.Add("rawfile.z");
			LinkEnvironment.SystemLibraries.Add("native_window");
			LinkEnvironment.SystemLibraries.Add("ace_ndk.z");
			LinkEnvironment.SystemLibraries.Add("unwind");
			LinkEnvironment.SystemLibraries.Add("ohaudio");

			//mediaplayer
			LinkEnvironment.SystemLibraries.Add("avplayer");
			LinkEnvironment.SystemLibraries.Add("native_image");
			LinkEnvironment.SystemLibraries.Add("native_window");
			LinkEnvironment.SystemLibraries.Add("native_buffer");
			LinkEnvironment.SystemLibraries.Add("native_media_avsource");
			LinkEnvironment.SystemLibraries.Add("native_media_core");
			LinkEnvironment.SystemLibraries.Add("native_media_codecbase");

			//vsync
			LinkEnvironment.SystemLibraries.Add("native_vsync");

			//sles
			LinkEnvironment.SystemLibraries.Add("OpenSLES");

		}

		private bool UseTegraGraphicsDebugger(ReadOnlyTargetRules Target)
		{
			// Disable for now
			return false;
		}

		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Shipping:
				case UnrealTargetConfiguration.Test:
				case UnrealTargetConfiguration.Debug:
				default:
					return true;
			};
		}

		public static ClangToolChainOptions CreateToolChainOptions(OpenHarmonyTargetRules TargetRules)
		{
			ClangToolChainOptions Options = ClangToolChainOptions.None;
			if (TargetRules.bEnableAddressSanitizer)
			{
				Options |= ClangToolChainOptions.EnableAddressSanitizer;
			}
			else if (TargetRules.bEnableHWAddressSanitizer)
			{
				Options |= ClangToolChainOptions.EnableHWAddressSanitizer;
			}
			if (TargetRules.bEnableThreadSanitizer)
			{
				Options |= ClangToolChainOptions.EnableThreadSanitizer;
			}
			if (TargetRules.bEnableUndefinedBehaviorSanitizer)
			{
				Options |= ClangToolChainOptions.EnableUndefinedBehaviorSanitizer;
			}
			else if (TargetRules.bEnableMinimalUndefinedBehaviorSanitizer)
			{
				Options |= ClangToolChainOptions.EnableMinimalUndefinedBehaviorSanitizer;
			}

			return Options;
		}

		public override UEToolChain CreateToolChain(ReadOnlyTargetRules Target)
		{
			ClangToolChainOptions Options = CreateToolChainOptions(Target.OpenHarmonyPlatform.TargetRules);
			if (Target.bAllowLTCG && Target.bPreferThinLTO)
			{
				Options |= ClangToolChainOptions.EnableThinLTO;
			}
			return new OpenHarmonyToolChain(Target.ProjectFile, Options, Logger);
		}
		public virtual UEToolChain CreateTempToolChainForProject(FileReference? ProjectFile)
		{
			OpenHarmonyTargetRules TargetRules = new OpenHarmonyTargetRules();
			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), TargetRules, Logger);
			ClangToolChainOptions Options = CreateToolChainOptions(TargetRules);
			return new OpenHarmonyToolChain(ProjectFile, Options, Logger);
		}

		/// <inheritdoc/>
		public override void Deploy(TargetReceipt Receipt)
		{
			if (Receipt.ProjectFile == null)
			{
				throw new BuildException("Cannot deploy OpenHarmony project without a project file.");
			}
			// do not package data if building via UBT
			new UEDeployOpenHarmony(Receipt.ProjectFile, false, Logger).PrepTargetForDeployment(Receipt);
		}
	}

	class OpenHarmonyPlatformFactory : UEBuildPlatformFactory
	{
		public override UnrealTargetPlatform TargetPlatform => UnrealTargetPlatform.OpenHarmony;

		public override void RegisterBuildPlatforms(ILogger Logger)
		{
			OpenHarmonyPlatformSDK SDK = new OpenHarmonyPlatformSDK(Logger);
			// Register this build platform
			UEBuildPlatform.RegisterBuildPlatform(new OpenHarmonyPlatform(SDK, Logger), Logger);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.OpenHarmony, UnrealPlatformGroup.OpenHarmony);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.OpenHarmony, UnrealPlatformGroup.ThirtyHz);
		}
	}
}
