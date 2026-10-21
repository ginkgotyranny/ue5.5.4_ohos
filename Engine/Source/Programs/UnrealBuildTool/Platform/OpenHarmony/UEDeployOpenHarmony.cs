// Copyright Epic Games, Inc. All Rights Reserved.
// #include ../../../../../Binaries/ThirdParty/Newtonsoft
using System;
using System.Collections.Generic;
using System.Linq;
//using System.Security.Cryptography;
using System.Diagnostics;
using System.IO;
//using System.IO.Compression;
//using Tools.DotNETCommon;
//using System.Text;
//using Newtonsoft;
using Newtonsoft.Json.Linq;
//using System.Xml.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class UEDeployOpenHarmony : UEBuildDeploy, IOpenHarmonyDeploy
	{
		protected FileReference ProjectFile;

		public UEDeployOpenHarmony(FileReference InProjectFile, bool InForcePackageData, ILogger InLogger)
			: base(InLogger)
		{
			ProjectFile = InProjectFile;

			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), this, Logger);
		}

		private ConfigHierarchy GetConfigCacheIni(ConfigHierarchyType Type)
		{
			return ConfigCache.ReadHierarchy(Type, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
		}
		
		private void CopyGfxDebugger(string LibPath, string NDKArch)
		{
			string OpenHarmonyGraphicsDebugger;
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "OpenHarmonyGraphicsDebugger", out OpenHarmonyGraphicsDebugger);

			switch (OpenHarmonyGraphicsDebugger.ToLower())
			{
				case "mali":
					{
						string MaliGraphicsDebuggerPath;
						OpenHarmonyPlatformSDK.GetPath(Ini, "/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "MaliGraphicsDebuggerPath", out MaliGraphicsDebuggerPath);
						if (Directory.Exists(MaliGraphicsDebuggerPath))
						{
							Directory.CreateDirectory(Path.Combine(LibPath, "libs", NDKArch));
							string MaliLibSrcPath = Path.Combine(MaliGraphicsDebuggerPath, "target", "openharmony-non-root", "arm", NDKArch, "libMGD.so");
							if (!File.Exists(MaliLibSrcPath))
							{
								// in v4.3.0 library location was changed
								MaliLibSrcPath = Path.Combine(MaliGraphicsDebuggerPath, "target", "openharmony", "arm", "unrooted", NDKArch, "libMGD.so");
							}
							string MaliLibDstPath = Path.Combine(LibPath, "libs", NDKArch, "libMGD.so");

							Logger.LogInformation("Copying {MaliLibSrcPath} to {MaliLibDstPath}", MaliLibSrcPath, MaliLibDstPath);
							File.Copy(MaliLibSrcPath, MaliLibDstPath, true);
							File.SetLastWriteTimeUtc(MaliLibDstPath, File.GetLastWriteTimeUtc(MaliLibSrcPath));

							string MaliVkLayerLibSrcPath = Path.Combine(MaliGraphicsDebuggerPath, "target", "openharmony", "arm", "rooted", NDKArch, "libGLES_aga.so");
							if (File.Exists(MaliVkLayerLibSrcPath))
							{
								string MaliVkLayerLibDstPath = Path.Combine(LibPath, "libs", NDKArch, "libVkLayerAGA.so");
								Logger.LogInformation("Copying {MaliVkLayerLibSrcPath} to {MaliVkLayerLibDstPath}", MaliVkLayerLibSrcPath, MaliVkLayerLibDstPath);
								File.Copy(MaliVkLayerLibSrcPath, MaliVkLayerLibDstPath, true);
								File.SetLastWriteTimeUtc(MaliVkLayerLibDstPath, File.GetLastWriteTimeUtc(MaliVkLayerLibSrcPath));
							}
						}
					}
					break;

				// @TODO: Add NVIDIA Gfx Debugger
				/*
				case "nvidia":
					{
						Directory.CreateDirectory(UnrealBuildPath + "/libs/" + NDKArch);
						File.Copy("F:/NVPACK/openharmony-kk-egl-t124-a32/Stripped_libNvPmApi.Core.so", UnrealBuildPath + "/libs/" + NDKArch + "/libNvPmApi.Core.so", true);
						File.Copy("F:/NVPACK/openharmony-kk-egl-t124-a32/Stripped_libNvidia_gfx_debugger.so", UnrealBuildPath + "/libs/" + NDKArch + "/libNvidia_gfx_debugger.so", true);
					}
					break;
				*/
				default:
					break;
			}
		}
		
		private UnrealPluginLanguage? UPL = null;
		private string ActiveUPLFiles = "";
		private string? UPLHashCode = null;
		private UnrealArchitectures? Architectures = null;
		
		public void SetOpenHarmonyPluginData(UnrealArchitectures Architectures, List<string> inPluginExtraData)
		{
			this.Architectures = Architectures;
			List<string> NDKArches = Architectures.Architectures.Select(x => GetNDKArch(x)).ToList();
			
			// check if certain plugins are enabled
			ActiveUPLFiles = "";
			foreach (string Plugin in inPluginExtraData)
			{
				ActiveUPLFiles += Plugin + "\n";
			}
			UPL = new UnrealPluginLanguage(ProjectFile, inPluginExtraData, NDKArches, null, UnrealTargetPlatform.OpenHarmony, Logger);
			UPLHashCode = UPL.GetUPLHash();
//			APL.SetTrace();
		}

		private static bool SafeDeleteFile(string Filename, bool bCheckExists = true)
		{
			if (!bCheckExists || File.Exists(Filename))
			{
				try
				{
					File.SetAttributes(Filename, FileAttributes.Normal);
					File.Delete(Filename);
					return true;
				}
				catch (System.UnauthorizedAccessException)
				{
					throw new BuildException("File '{0}' is in use; unable to modify it.", Filename);
				}
				catch (System.Exception)
				{
					return false;
				}
			}
			return true;
		}

		private static void CopyFileDirectory(string SourceDir, string DestDir, string[]? Excludes = null)
		{
			if (!Directory.Exists(SourceDir))
			{
				return;
			}

			string[] Files = Directory.GetFiles(SourceDir, "*.*", SearchOption.AllDirectories);
			foreach (string Filename in Files)
			{
				if (Excludes != null)
				{
					// skip files in excluded directories
					string DirectoryName = Path.GetFileName(Path.GetDirectoryName(Filename))!;
					bool bExclude = false;
					foreach (string Exclude in Excludes)
					{
						if (DirectoryName == Exclude)
						{
							bExclude = true;
							break;
						}
					}
					if (bExclude)
					{
						continue;
					}
				}
				
				// skip template files
				if (Path.GetExtension(Filename) == ".template")
				{
					continue;
				}

				// make the dst filename with the same structure as it was in SourceDir
				string DestFilename = Path.Combine(DestDir, Utils.MakePathRelativeTo(Filename, SourceDir)).Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);

				// make the subdirectory if needed
				string DestSubdir = Path.GetDirectoryName(DestFilename)!;
				if (!Directory.Exists(DestSubdir))
				{
					Directory.CreateDirectory(DestSubdir);
				}
				SafeDeleteFile(DestFilename);
				File.Copy(Filename, DestFilename);

				// preserve timestamp and clear read-only flags
				FileInfo DestFileInfo = new FileInfo(DestFilename);
				DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
				File.SetLastWriteTimeUtc(DestFilename, File.GetLastWriteTimeUtc(Filename));
			}
		}

		private static void DeleteDirectory(string InPath, ILogger Logger, string SubDirectoryToKeep = "")
		{
			// skip the dir we want to
			if (String.Compare(Path.GetFileName(InPath), SubDirectoryToKeep, true) == 0)
			{
				return;
			}

			// delete all files in here
			string[] Files;
			try
			{
				Files = Directory.GetFiles(InPath);
			}
			catch (Exception)
			{
				// directory doesn't exist so all is good
				return;
			}
			foreach (string Filename in Files)
			{
				try
				{
					// remove any read only flags
					FileInfo FileInfo = new FileInfo(Filename);
					FileInfo.Attributes = FileInfo.Attributes & ~FileAttributes.ReadOnly;
					FileInfo.Delete();
				}
				catch (Exception)
				{
					Logger.LogInformation("Failed to delete all files in directory {InPath}. Continuing on...", InPath);
				}
			}

			string[] Dirs = Directory.GetDirectories(InPath, "*.*", SearchOption.TopDirectoryOnly);
			foreach (string Dir in Dirs)
			{
				DeleteDirectory(Dir, Logger, SubDirectoryToKeep);
				// try to delete the directory, but allow it to fail (due to SubDirectoryToKeep still existing)
				try
				{
					Directory.Delete(Dir);
				}
				catch (Exception)
				{
					// do nothing
				}
			}
		}

		private void CleanCopyDirectory(string SourceDir, string DestDir, string[]? Excludes = null)
		{
			if (!Directory.Exists(SourceDir))
			{
				return;
			}
			if (!Directory.Exists(DestDir))
			{
				CopyFileDirectory(SourceDir, DestDir, Excludes);
				return;
			}

			// copy files that are different and make a list of ones to keep
			string[] StartingSourceFiles = Directory.GetFiles(SourceDir, "*.*", SearchOption.AllDirectories);
			List<string> FilesToKeep = new List<string>();
			foreach (string Filename in StartingSourceFiles)
			{
				if (Excludes != null)
				{
					// skip files in excluded directories
					string DirectoryName = Path.GetFileName(Path.GetDirectoryName(Filename))!;
					bool bExclude = false;
					foreach (string Exclude in Excludes)
					{
						if (DirectoryName == Exclude)
						{
							bExclude = true;
							break;
						}
					}
					if (bExclude)
					{
						continue;
					}
				}

				// make the dest filename with the same structure as it was in SourceDir
				string DestFilename = Path.Combine(DestDir, Utils.MakePathRelativeTo(Filename, SourceDir));

				// remember this file to keep
				FilesToKeep.Add(DestFilename);

				// only copy files that are new or different
				if (FilesAreDifferent(Filename, DestFilename))
				{
					if (File.Exists(DestFilename))
					{
						// delete it so can copy over it
						SafeDeleteFile(DestFilename);
					}

					// make the subdirectory if needed
					string DestSubdir = Path.GetDirectoryName(DestFilename)!;
					if (!Directory.Exists(DestSubdir))
					{
						Directory.CreateDirectory(DestSubdir);
					}

					// copy it
					File.Copy(Filename, DestFilename);

					// preserve timestamp and clear read-only flags
					FileInfo DestFileInfo = new FileInfo(DestFilename);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
					File.SetLastWriteTimeUtc(DestFilename, File.GetLastWriteTimeUtc(Filename));

					Logger.LogInformation("Copied file {DestFilename}.", DestFilename);
				}
			}

			// delete any files not in the keep list
			string[] StartingDestFiles = Directory.GetFiles(DestDir, "*.*", SearchOption.AllDirectories);
			foreach (string Filename in StartingDestFiles)
			{
				if (!FilesToKeep.Contains(Filename))
				{
					Logger.LogInformation("Deleting unneeded file {Filename}.", Filename);
					SafeDeleteFile(Filename);
				}
			}

			// delete any empty directories
			try
			{
				IEnumerable<string> BaseDirectories = Directory.EnumerateDirectories(DestDir, "*", SearchOption.AllDirectories).OrderByDescending(x => x);
				foreach (string directory in BaseDirectories)
				{
					if (Directory.Exists(directory) && Directory.GetFiles(directory, "*.*", SearchOption.AllDirectories).Count() == 0)
					{
						Logger.LogInformation("Cleaning Directory {directory} as empty.", directory);
						Directory.Delete(directory, true);
					}
				}
			}
			catch (Exception)
			{
				// likely System.IO.DirectoryNotFoundException, ignore it
			}
		}

		private bool FilesAreDifferent(string SourceFilename, string DestFilename)
		{
			// source must exist
			FileInfo SourceInfo = new FileInfo(SourceFilename);
			if (!SourceInfo.Exists)
			{
				throw new BuildException("Can't make an APK without file [{0}]", SourceFilename);
			}

			// different if destination doesn't exist
			FileInfo DestInfo = new FileInfo(DestFilename);
			if (!DestInfo.Exists)
			{
				return true;
			}

			// file lengths differ?
			if (SourceInfo.Length != DestInfo.Length)
			{
				return true;
			}

			// validate timestamps
			TimeSpan Diff = DestInfo.LastWriteTimeUtc - SourceInfo.LastWriteTimeUtc;
			if (Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
			{
				return true;
			}

			// could check actual bytes just to be sure, but good enough
			return false;
		}

		private void MakeDirectoryIfRequired(string DestFilename)
		{
			string DestSubdir = Path.GetDirectoryName(DestFilename)!;
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}
		}

		private static void CleanCopyFile(string SourceFileName, string FinalFileName)
		{
			if (!File.Exists(SourceFileName))
			{
				return;
			}
			bool bFileExists = File.Exists(FinalFileName);
			TimeSpan Diff = File.GetLastWriteTimeUtc(FinalFileName) - File.GetLastWriteTimeUtc(SourceFileName);
			if (!bFileExists || Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
			{
				SafeDeleteFile(FinalFileName);
				Directory.CreateDirectory(Path.GetDirectoryName(FinalFileName)!);
				File.Copy(SourceFileName, FinalFileName, true);

				// make sure it's writable if the source was readonly (e.g. autosdks)
				new FileInfo(FinalFileName).IsReadOnly = false;
				File.SetLastWriteTimeUtc(FinalFileName, File.GetLastWriteTimeUtc(SourceFileName));
			}
		}

		private static string GetNDKArch(UnrealArch UnrealArch)
		{
			if (UnrealArch == UnrealArch.Arm64)
			{
				return "arm64-v8a";
			}
			else if (UnrealArch == UnrealArch.X64)
			{
				return "x86_64";
			}

			throw new BuildException("Unknown Unreal architecture '{0}'", UnrealArch);

		}

		public static string GetNDKLibArch(UnrealArch UnrealArch)
		{
			switch (GetNDKArch(UnrealArch))
			{
				case "armeabi-v7a": return "arm-linux-ohos";
				case "arm64-v8a": return "aarch64-linux-ohos";
				case "x86":
				case "x86_64":
				case "x64": return "x86_64-linux-ohos";
				default: return "aarch64-linux-ohos";
			}
		}

		private static void CopySTL(string LibPath, UnrealArch UnrealArch)
		{
			string NDKPath = Environment.GetEnvironmentVariable("OHNDKROOT")!;

			// NDKArch需替换为aarch64-linux-ohos
			string SourceSTLSOName = Path.Combine(NDKPath, "llvm", "lib", GetNDKLibArch(UnrealArch), "c++", "libc++_shared.so");
			string FinalSTLSOName = Path.Combine(LibPath, "libc++_shared.so");


			// check to see if libc++_shared.so is newer than last time we copied
			CleanCopyFile(SourceSTLSOName, FinalSTLSOName);
		}

		static void FilterStdOutErr(object sender, DataReceivedEventArgs e, ILogger Logger)
		{
			if (e.Data != null)
			{
				switch (e.Data) 
				{
					case var data when data.Contains("WARNING: The specified OpenHarmony SDK Build Tools version"):
						Logger.LogInformation("{0}", data.Replace("WARNING: ", ">> "));
						return;
					case var data when data.Contains("Warning: Resigning with jarsigner."):
						Logger.LogInformation("{0}", data.Replace("Warning: ", ">> "));
						return;
					case var data when data.Contains("Unable to strip library"):
						Logger.LogInformation("{0}", e.Data.Replace("due to error", ""));
						return;
					case var data when data.Contains("To suppress this warning,"):
						Logger.LogInformation("{0}", e.Data.Replace(" warning,", ","));
						break;
					default:
						Logger.LogInformation("{0}", e.Data);
						break;
				}
				Logger.LogInformation("{0}", e.Data);
			}
		}

		private static void RunCommandLineProgramWithExceptionAndFiltering(string WorkingDirectory, string Command, string Params, ILogger Logger, string? OverrideDesc = null, bool bUseShellExecute = false)
		{
			// Process Arguments follow windows conventions in .NET Core
			// Which means single quotes ' are not considered quotes.
			// see https://github.com/dotnet/runtime/issues/29857
			// also see UE-102580
			// for rules see https://docs.microsoft.com/en-us/cpp/cpp/main-function-command-line-args
			if (Params.Contains('\''))
			{
				Params = Params.Replace("\"", "\\\"");
				Params = Params.Replace('\'', '\"');
			}

			if (OverrideDesc == null)
			{
				Logger.LogInformation("\nRunning: {Command} {Params}", Command, Params);
			}
			else if (!String.IsNullOrEmpty(OverrideDesc))
			{
				Logger.LogInformation("{Message}", OverrideDesc);
				Logger.LogDebug("\nRunning: {Command} {Params}", Command, Params);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDirectory;
			StartInfo.FileName = Command;
			StartInfo.Arguments = Params;
			StartInfo.UseShellExecute = bUseShellExecute;
			StartInfo.WindowStyle = ProcessWindowStyle.Minimized;
			StartInfo.RedirectStandardInput = true;
			StartInfo.RedirectStandardOutput = true;
			StartInfo.RedirectStandardError = true;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			Proc.OutputDataReceived += (s, e) => FilterStdOutErr(s, e, Logger);
			Proc.ErrorDataReceived += (s, e) => FilterStdOutErr(s, e, Logger);
			Proc.Start();
			Proc.BeginOutputReadLine();
			Proc.BeginErrorReadLine();
			
			StreamWriter StreamIn = Proc.StandardInput;
			StreamIn.WriteLine("yes");
			StreamIn.Close();

			Proc.WaitForExit();

			// openHarmony bat failure
			if (Proc.ExitCode != 0)
			{
				throw new BuildException("{0} failed with args {1}", Command, Params);
			}
		}

		private bool bHaveReadEngineVersion = false;
		private string EngineMajorVersion = "4";
		private string EngineMinorVersion = "0";
		private string EnginePatchVersion = "0";
		private string EngineChangelist = "0";
		private string? EngineBranch = "UE5";

		private string ReadEngineVersion()
		{
			if (!bHaveReadEngineVersion)
			{
				ReadOnlyBuildVersion Version = ReadOnlyBuildVersion.Current;

				EngineMajorVersion = Version.MajorVersion.ToString();
				EngineMinorVersion = Version.MinorVersion.ToString();
				EnginePatchVersion = Version.PatchVersion.ToString();
				EngineChangelist = Version.Changelist.ToString();
				EngineBranch = Version.BranchName;

				bHaveReadEngineVersion = true;
			}

			return EngineMajorVersion + "." + EngineMinorVersion + "." + EnginePatchVersion;
		}

		private int CachedStoreVersion = -1;
		private int CachedStoreVersionOffsetArm64 = 0;
		private int CachedStoreVersionOffsetX8664 = 0;

		public int GetStoreVersion(UnrealArch? Architecture)
		{
			if (CachedStoreVersion < 1)
			{
				ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
				int StoreVersion = 1;
				Ini.GetInt32("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "StoreVersion", out StoreVersion);

				bool bUseChangeListAsStoreVersion = false;
				Ini.GetBool("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "bUseChangeListAsStoreVersion", out bUseChangeListAsStoreVersion);

				bool IsBuildMachine = Unreal.IsBuildMachine();
				// override store version with changelist if enabled and is build machine
				if (bUseChangeListAsStoreVersion && IsBuildMachine)
				{
					// make sure changelist is cached (clear unused warning)
					string EngineVersion = ReadEngineVersion();
					if (EngineVersion == null)
					{
						throw new BuildException("No engine version!");
					}

					int Changelist = 0;
					if (Int32.TryParse(EngineChangelist, out Changelist))
					{
						if (Changelist != 0)
						{
							StoreVersion = Changelist;
						}
					}
				}

				Logger.LogInformation("GotStoreVersion found v{StoreVersion}. (bUseChangeListAsStoreVersion={bUseChangeListAsStoreVersion} IsBuildMachine={IsBuildMachine} EngineChangeList={EngineChangelist})", StoreVersion, bUseChangeListAsStoreVersion, IsBuildMachine, EngineChangelist);

				CachedStoreVersion = StoreVersion;

				Ini.GetInt32("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "StoreVersionOffsetArm64", out CachedStoreVersionOffsetArm64);
				Ini.GetInt32("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "StoreVersionOffsetX8664", out CachedStoreVersionOffsetX8664);
			}
			
			if (Architecture == UnrealArch.Arm64)
			{
				return CachedStoreVersion + CachedStoreVersionOffsetArm64;
			}
			else if (Architecture == UnrealArch.X64)
			{
				return CachedStoreVersion + CachedStoreVersionOffsetX8664;
			}

			return CachedStoreVersion;
		}

		private string? CachedVersionDisplayName = null;

		public string GetVersionDisplayName(bool bIsEmbedded)
		{
			if (string.IsNullOrEmpty(CachedVersionDisplayName))
			{
				ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
				string VersionDisplayName = "";
				Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "VersionDisplayName", out VersionDisplayName);

				if (Unreal.IsBuildMachine())
				{
					bool bAppendChangeListToVersionDisplayName = false;
					Ini.GetBool("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "bAppendChangeListToVersionDisplayName", out bAppendChangeListToVersionDisplayName);
					if (bAppendChangeListToVersionDisplayName)
					{
						VersionDisplayName = string.Format("{0}-{1}", VersionDisplayName, EngineChangelist);
					}

					bool bAppendPlatformToVersionDisplayName = false;
					Ini.GetBool("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "bAppendPlatformToVersionDisplayName", out bAppendPlatformToVersionDisplayName);
					if (bAppendPlatformToVersionDisplayName)
					{
						VersionDisplayName = string.Format("{0}-OpenHarmony", VersionDisplayName);
					}

					// append optional text to version name if embedded build
					if (bIsEmbedded)
					{
						string EmbeddedAppendDisplayName = "";
						if (Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "EmbeddedAppendDisplayName", out EmbeddedAppendDisplayName))
						{
							VersionDisplayName = VersionDisplayName + EmbeddedAppendDisplayName;
						}
					}
				}

				CachedVersionDisplayName = VersionDisplayName;
			}

			return CachedVersionDisplayName;
		}

		// Store generated package name in a text file for builds that do not generate an apk file 
		public bool SavePackageInfo(string TargetName, string ProjectDirectory, TargetType InTargetType, bool bIsEmbedded)
		{
			string PackageName = GetPackageName(TargetName);
			string DestPackageNameFileName = Path.Combine(ProjectDirectory, "Binaries", "OpenHarmony", "packageInfo.txt");

			string[] PackageInfoSource = new string[4];
			PackageInfoSource[0] = PackageName;
			PackageInfoSource[1] = GetStoreVersion(null).ToString();
			PackageInfoSource[2] = GetVersionDisplayName(bIsEmbedded);
			PackageInfoSource[3] = string.Format("name='com.epicgames.unreal.GameActivity.AppType' value='{0}'", InTargetType == TargetType.Game ? "" : InTargetType.ToString());

			Logger.LogInformation("Writing packageInfo pkgName:{PkgName} storeVersion:{StoreVer} versionDisplayName:{Version} to {DestFile}", PackageInfoSource[0], PackageInfoSource[1], PackageInfoSource[2], DestPackageNameFileName);

			string DestDirectory = Path.GetDirectoryName(DestPackageNameFileName)!;
			if (!Directory.Exists(DestDirectory))
			{
				Directory.CreateDirectory(DestDirectory);
			}

			File.WriteAllLines(DestPackageNameFileName, PackageInfoSource);

			return true;
		}
		
		public bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, DirectoryReference ProjectDirectory, string ExecutablePath, string EngineDirectory, string CookFlavor, UnrealTargetConfiguration Configuration, bool bSkipGradleBuild)
		{
			//Logger.LogInformation("$$$$$$$$$$$$$$ PrepForUATPackageOrDeploy $$$$$$$$$$$$$$$$$");

			TargetType Type = TargetType.Game;
			if (CookFlavor.EndsWith("Client"))
			{
				Type = TargetType.Client;			
			}
			else if (CookFlavor.EndsWith("Server"))
			{
				Type = TargetType.Server;
			}

			// note that we cannot allow the data packaged into the HAP if we are doing something like Launch On that will not make an obb
			// file and instead pushes files directly via deploy
			OpenHarmonyTargetRules TargetRules = new OpenHarmonyTargetRules();
			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), TargetRules, Logger);
			ClangToolChainOptions Options = OpenHarmonyPlatform.CreateToolChainOptions(TargetRules);
			OpenHarmonyToolChain ToolChain = new OpenHarmonyToolChain(ProjectFile, Options, Logger);

			SavePackageInfo(ProjectName, ProjectDirectory.FullName, Type, bSkipGradleBuild);

			MakeHap(ToolChain, ProjectName, ProjectDirectory.FullName, EngineDirectory, ExecutablePath, CookFlavor);
			return true;
		}

		private List<string> CollectPluginDataPaths(TargetReceipt Receipt)
		{
			List<string> PluginExtras = new List<string>();
			if (Receipt == null)
			{
				Logger.LogInformation("Receipt is NULL");
				return PluginExtras;
			}

			// collect plugin extra data paths from target receipt
			IEnumerable<ReceiptProperty> Results = Receipt.AdditionalProperties.Where(x => x.Name == "OpenHarmonyPlugin");
			foreach (ReceiptProperty Property in Results)
			{
				// Keep only unique paths
				string PluginPath = Property.Value;
				if (PluginExtras.FirstOrDefault(x => x == PluginPath) == null)
				{
					PluginExtras.Add(PluginPath);
					Logger.LogInformation("OpenHarmonyPlugin: {PluginPath}", PluginPath);
				}
			}
			return PluginExtras;
		}


		public override bool PrepTargetForDeployment(TargetReceipt Receipt)
		{
			Logger.LogInformation("$$$$$$$$$$$$$$ PrepTargetForDeployment $$$$$$$$$$$$$$$$$");
			DirectoryReference ProjectDirectory = DirectoryReference.FromFile(Receipt.ProjectFile) ?? Unreal.EngineDirectory;
			string TargetName = (Receipt.ProjectFile == null ? Receipt.TargetName : Receipt.ProjectFile.GetFileNameWithoutAnyExtensions());
			
			OpenHarmonyToolChain ToolChain = (OpenHarmonyToolChain)((OpenHarmonyPlatform)UEBuildPlatform.GetBuildPlatform(Receipt.Platform)).CreateTempToolChainForProject(Receipt.ProjectFile);
			
			// get the receipt
			SetOpenHarmonyPluginData(Receipt.Architectures, CollectPluginDataPaths(Receipt));
			bool bShouldCompileAsDll = Receipt.HasValueForAdditionalProperty("CompileAsDll", "true");
			SavePackageInfo(TargetName, ProjectDirectory.FullName, Receipt.TargetType, bShouldCompileAsDll);
			
			// Get the output paths
			BuildProductType ProductType = bShouldCompileAsDll ? BuildProductType.DynamicLibrary : BuildProductType.Executable;
			List<FileReference> OutputPaths = Receipt.BuildProducts.Where(x => x.Type == ProductType).Select(x => x.Path).ToList();
			if (OutputPaths.Count < 1)
			{
				throw new BuildException("Target file does not contain either executable or dynamic library .so");
			}

			// we need to strip architecture from any of the output paths
			string BaseSoName = ToolChain.RemoveArchName(OutputPaths[0].FullName);

			MakeHap(ToolChain, TargetName,  ProjectDirectory.FullName, Unreal.EngineDirectory.FullName, BaseSoName, CookFlavor: "");

			return true;
		}
		
		private bool IsLetter(char Input)
		{
			return (Input >= 'A' && Input <= 'Z') || (Input >= 'a' && Input <= 'z');
		}

		private bool IsDigit(char Input)
		{
			return (Input >= '0' && Input <= '9');
		}

		private string GetPackageName(string ProjectName)
		{
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
			string PackageName;
			Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "PackageName", out PackageName);

			if (PackageName.Contains("[PROJECT]"))
			{
				// project name must start with a letter
				if (!IsLetter(ProjectName[0]))
				{
					throw new BuildException("Package name segments must all start with a letter. Please replace [PROJECT] with a valid name");
				}

				// hyphens not allowed so change them to underscores in project name
				if (ProjectName.Contains('-'))
				{
					Trace.TraceWarning("Project name contained hyphens, converted to underscore");
					ProjectName = ProjectName.Replace("-", "_");
				}

				// check for special characters
				for (int Index = 0; Index < ProjectName.Length; Index++)
				{
					char c = ProjectName[Index];
					if (c != '.' && c != '_' && !IsDigit(c) && !IsLetter(c))
					{
						throw new BuildException("Project name contains illegal characters (only letters, numbers, and underscore allowed); please replace [PROJECT] with a valid name");
					}
				}

				PackageName = PackageName.Replace("[PROJECT]", ProjectName);
			}

			// verify minimum number of segments
			string[] PackageParts = PackageName.Split('.');
			int SectionCount = PackageParts.Length;
			if (SectionCount < 2)
			{
				throw new BuildException("Package name must have at least 2 segments separated by periods (ex. com.projectname, not projectname); please change in OpenHarmony Project Settings. Currently set to '" + PackageName + "'");
			}

			// hyphens not allowed
			if (PackageName.Contains('-'))
			{
				throw new BuildException("Package names may not contain hyphens; please change in OpenHarmony Project Settings. Currently set to '" + PackageName + "'");
			}

			// do not allow special characters
			for (int Index = 0; Index < PackageName.Length; Index++)
			{
				char c = PackageName[Index];
				if (c != '.' && c != '_' && !IsDigit(c) && !IsLetter(c))
				{
					throw new BuildException("Package name contains illegal characters (only letters, numbers, and underscore allowed); please change in OpenHarmony Project Settings. Currently set to '" + PackageName + "'");
				}
			}

			// validate each segment
			for (int Index = 0; Index < SectionCount; Index++)
			{
				if (PackageParts[Index].Length < 1)
				{
					throw new BuildException("Package name segments must have at least one letter; please change in OpenHarmony Project Settings. Currently set to '" + PackageName + "'");
				}

				if (!IsLetter(PackageParts[Index][0]))
				{
					throw new BuildException("Package name segments must start with a letter; please change in OpenHarmony Project Settings. Currently set to '" + PackageName + "'");
				}
			}

			Logger.LogInformation("Using package name: '{PackageName}'", PackageName);
			return PackageName;
		}

		private void MakeHap(OpenHarmonyToolChain ToolChain, string ProjectName, string ProjectDirectory, string EngineDirectory, string BaseSoName, string CookFlavor)
		{
			Logger.LogInformation("");
			Logger.LogInformation("===={Time}====PREPARING TO MAKE HAP=====", DateTime.Now.ToString());

			if (Architectures == null)
			{
				throw new BuildException("Called MakeHap without first calling SetAndroidPluginData");
			}

			// set up some directory info
			string IntermediateOpenHarmonyPath = Path.Combine(ProjectDirectory, "Intermediate", "OpenHarmony");
			string OpenHarmonyTemplatePath = Path.Combine(EngineDirectory, "Build", "OpenHarmony");
			string BuildFilePath = Path.Combine(ProjectDirectory, "Build", "OpenHarmony");
			
			UPL!.Init(new List<string>() {"arm64-v8a"}, false, EngineDirectory, IntermediateOpenHarmonyPath, ProjectDirectory, "", true);

			DeleteDirectory(IntermediateOpenHarmonyPath, Logger);

			//Copy template files to the intermediate folder
			CopyFileDirectory(OpenHarmonyTemplatePath, IntermediateOpenHarmonyPath);

			UPL.ProcessPluginNode("arm64-v8a", "customRequestPermissions", "");
			
			UPL.ReplaceAppJson(GetPackageName(ProjectName), IntermediateOpenHarmonyPath);
			UPL.ReplaceModuleJson(IntermediateOpenHarmonyPath);
			UPL.ReplaceElementJson(ProjectName, IntermediateOpenHarmonyPath);
			UPL.ReplaceBuildProfile(IntermediateOpenHarmonyPath);
			UPL.ReplaceProjectOHPackageJson(IntermediateOpenHarmonyPath);
			UPL.ReplaceOhpmrc(IntermediateOpenHarmonyPath);
			
			// copy postbuild plugin files
			bool bUseFileCopy = UPL.ProcessPluginNode("arm64-v8a", "resourceCopies", "").Contains("copyFile");
			if(!bUseFileCopy)
			{
				UPL.ReplaceEntryOHPackageJson(IntermediateOpenHarmonyPath);
			}

			foreach (UnrealArch Arch in Architectures.Architectures)
			{
				string NDKArch = GetNDKArch(Arch);
				string SourceSoName = OpenHarmonyToolChain.InlineArchName(BaseSoName, Arch);
				if (!File.Exists(SourceSoName))
				{
					throw new BuildException("Can't make Hap without the compiled .so [{0}]", SourceSoName);
				}

				string LibsDir = Path.Combine(IntermediateOpenHarmonyPath, "entry/libs/") + GetNDKArch(Arch);
				MakeDirectoryIfRequired(LibsDir);

				string FinalSoName = Path.Combine(LibsDir, "libUnreal.so");
				string NDKPath = Environment.GetEnvironmentVariable("OHNDKROOT")!;
				string ShellExecutable = RuntimePlatform.IsWindows ? "cmd.exe" : "/bin/sh";
				string ShellParametersBegin = RuntimePlatform.IsWindows ? "/c " : "-c '";
				string ShellParametersEnd = RuntimePlatform.IsWindows ? "" : "'";

				// check to see if libUnreal.so needs to be copied
				if (FilesAreDifferent(SourceSoName, FinalSoName))
				{
					Logger.LogInformation("\nCopying new .so {SourceSoName} file to libs folder...", SourceSoName);
					Directory.CreateDirectory(LibsDir); 
					// copy the binary to the standard .so location
					File.Copy(SourceSoName, FinalSoName, true);
					
					File.SetLastWriteTimeUtc(FinalSoName, File.GetLastWriteTimeUtc(SourceSoName));
				}

				// copy so STL
				CopySTL(LibsDir, Arch);
				CopyGfxDebugger(LibsDir, GetNDKArch(Arch));
				//CopyVulkanValidationLayers(LibsDir, Arch, Configuration.ToString());
				
				// copy postbuild plugin files
				// UPL.ProcessPluginNode(NDKArch, "resourceCopies", "");

				// copy "Content" directory to rawfile
				// *.pak文件在Content/Paks目录下; 除此之外，其他一些资源文件，比如视频文件也会被放在Content目录下，
				// 安卓使用OBB打包这些资源，OpenHarmony则放在rawfile目录下访问
				string RawFilePath = Path.Combine(IntermediateOpenHarmonyPath, "entry", "src", "main", "resources", "rawfile");
				string ContentDesPath = Path.Combine(RawFilePath, ProjectName, "Content/");
				Directory.CreateDirectory(ContentDesPath);
				string ContentSrcPath = Path.Combine(ProjectDirectory, "Saved", "StagedBuilds", "OpenHarmony" + CookFlavor, ProjectName, "Content");
				if (Directory.Exists(ContentSrcPath))
				{
					CopyFileDirectory(ContentSrcPath, ContentDesPath);
					//可以参考如下方式：ContentDesPath下除了chunk0外的chunk全部删除，不打入hap包内，后续通过http下载
					// string PakFileLocation = ContentDesPath + "Paks";
					// if (Directory.Exists(PakFileLocation))
					// {
					// 	System.Console.WriteLine("MakeHap: PakFileLocation={0}", PakFileLocation);
					// 	IEnumerable<string> PakFiles = Directory.EnumerateFiles(PakFileLocation, "*.pak", SearchOption.TopDirectoryOnly);
					// 	foreach (string PakFile in PakFiles)
					// 	{
					// 		bool bNeedCopy = true;
					// 		bool bChunk = PakFile.Contains("chunk");
					// 		if (true == bChunk)
					// 		{
					// 			bNeedCopy = PakFile.Contains("chunk0");
					// 			System.Console.WriteLine("MakeHap: bChunk={0}, PakFile={1}", "true", PakFile);
					// 		}
					// 		else
					// 		{
					// 			System.Console.WriteLine("MakeHap: bChunk={0}, PakFile={1}", "false", PakFile);
					// 		}
					// 		//string PakFileDesPath = Path.Combine(RawFilePath, ProjectName) + "/Content/Paks/";
					// 		//string PakFileDst = Path.Combine(PakFileDesPath, PakFile.Substring(PakFile.LastIndexOf(Path.DirectorySeparatorChar) + 1));
					// 		//MakeDirectoryIfRequired(PakFileDesPath);
					// 		if (true == bNeedCopy)
					// 		{
					// 			System.Console.WriteLine("MakeHap:true == bNeedCopy: PakFile={0}", PakFile);
					// 		}
					// 		else 
					// 		{
					// 			System.Console.WriteLine("MakeHap:true != bNeedCopy: PakFile={0} Delete......", PakFile);
					// 			File.Delete(PakFile);
					// 		}
					// 	}
					// }
				}
				
				// copy intermediate files to the res folder
				CopyFileDirectory(Path.Combine(ProjectDirectory, "Intermediate", "Build", "OpenHarmony", "Hars"), Path.Combine(IntermediateOpenHarmonyPath, "entry", "hars"));
				CopyFileDirectory(BuildFilePath, IntermediateOpenHarmonyPath);
				
				UPL.UpdateEntryAbility("arm64-v8a", EngineDirectory, IntermediateOpenHarmonyPath);
				UPL.UpdateAkiJSBind("arm64-v8a", EngineDirectory, IntermediateOpenHarmonyPath);

				// use hivigor to build the .hap file
				string devEcoPath = Environment.GetEnvironmentVariable("DEVECO_PATH")!;
				if (String.IsNullOrEmpty(devEcoPath))
				{
					throw new BuildException("devEcoPath not set");
				}
				string JbrPath = Path.Combine(devEcoPath, "jbr", "bin");
				string HvigorPath = Path.Combine(devEcoPath, "tools", "hvigor", "bin");
				string SdkPath = Path.Combine(devEcoPath, "sdk");
				string NodeHome = Path.Combine(devEcoPath, "tools", "node");
				string PathVar = Environment.GetEnvironmentVariable("PATH")!;
				Environment.SetEnvironmentVariable("PATH", NodeHome + ";" + JbrPath + ";" + HvigorPath + ";" + PathVar);
				Environment.SetEnvironmentVariable("DEVECO_SDK_HOME", SdkPath);
				
				// hivigor命令执行前创建.npmrc
				string HomePath = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
				string NpmrcPath = Path.Combine(HomePath, ".npmrc");
				if (!File.Exists(NpmrcPath))
				{ 
					File.WriteAllText(NpmrcPath, "");
				}

				// 加载entry/hars目录下的所有的 .har
				string OHPMPath = Path.Combine(devEcoPath, "tools", "ohpm", "bin");
				Environment.SetEnvironmentVariable("PATH", OHPMPath + ";" + Environment.GetEnvironmentVariable("PATH"));
				string EntryPath = Path.Combine(IntermediateOpenHarmonyPath, "entry");
				string HarDirectory = Path.Combine(EntryPath, "hars");

				if (Directory.Exists(HarDirectory))
				{
					string[] HarNames = Directory.GetFiles(HarDirectory, "*.har", SearchOption.AllDirectories);
					foreach (string name in HarNames)
					{
						RunCommandLineProgramWithExceptionAndFiltering(EntryPath, ShellExecutable, ShellParametersBegin + " ohpm install " + name + ShellParametersEnd, Logger, "ohpm install " + name);
					}
				}
				UPL.ReplaceEntryBuildProfileJson(IntermediateOpenHarmonyPath);
				// hvigorw需要可执行
				if (RuntimePlatform.IsWindows)
				{
					RunCommandLineProgramWithExceptionAndFiltering(IntermediateOpenHarmonyPath, ShellExecutable, "-c 'chmod 777 hvigorw'", Logger, "chmod hvigorw...");
				}
				
				ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
				bool bBuildAPP = false;
				Ini.GetBool("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "bBuildAPP", out bBuildAPP);
				if (!bBuildAPP)
				{
					RunCommandLineProgramWithExceptionAndFiltering(IntermediateOpenHarmonyPath, ShellExecutable, ShellParametersBegin + "hvigorw clean --no-daemon" + ShellParametersEnd, Logger, "hivigor clean...");
					RunCommandLineProgramWithExceptionAndFiltering(IntermediateOpenHarmonyPath, ShellExecutable, ShellParametersBegin + "hvigorw --mode module -p module=entry@default -p product=default assembleHap --info --no-daemon" + ShellParametersEnd, Logger, "Making hap with hivigor...");
					string BuildOutputsPath = Path.Combine(IntermediateOpenHarmonyPath, "entry", "build", "default", "outputs", "default/");
					string SourceUnSignedHapName = BuildOutputsPath + "entry-default-unsigned.hap";
					string SourceSignedHapName = BuildOutputsPath + "entry-default-signed.hap";
					string HapFileName = ProjectName + Arch + ".hap";
					string DestHapPath = Path.Combine(ProjectDirectory, "Binaries", "OpenHarmony/");
					string DestHapName = Path.Combine(DestHapPath, HapFileName);
					MakeDirectoryIfRequired(DestHapPath);

					// 调用自动化签名工具
					string PackageName = GetPackageName(ProjectName);
					string SignToolPath = Path.Combine(OpenHarmonyTemplatePath, "temp", "tools");
					string InSignfile = Path.Combine(SignToolPath, "UnsgnedReleasedProfileTemplate.json");
					string InSignfileTemp = Path.Combine(SignToolPath, "Temp.json");

					//Modify the json file so for auto signature
					string jsonString = File.ReadAllText(InSignfile);
					// Convert the JSON string to a JObject:
					JObject? jObject = Newtonsoft.Json.JsonConvert.DeserializeObject(jsonString) as JObject;
					// Select a nested property using a single string:
					JToken jToken = jObject!.SelectToken("bundle-info.bundle-name")!;
					// Update the value of the property: 
					jToken.Replace(PackageName);
					// Convert the JObject back to a string:
					string updatedJsonString = jObject!.ToString();
					File.WriteAllText(InSignfileTemp, updatedJsonString);

					//Get the path of signing tools
					string SignTool = Path.Combine(SignToolPath, "hap-sign-tool.jar");
					string CertPath;
					string StorePassword;
					string KeyAlias;
					string KeyPassword;
					string Profile;
					string StoreFile;
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "CertPath", out CertPath);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "StorePassword", out StorePassword);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "KeyAlias", out KeyAlias);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "KeyPassword", out KeyPassword);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "Profile", out Profile);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "StoreFile", out StoreFile);
					CertPath = Path.Combine(IntermediateOpenHarmonyPath, CertPath);
					Profile = Path.Combine(IntermediateOpenHarmonyPath, Profile);
					StoreFile = Path.Combine(IntermediateOpenHarmonyPath, StoreFile);
					//三个证书缺少任意一个都需要进行自动签名
					if (!CertPath.Contains(".cer") || !Profile.Contains(".p7b") || !StoreFile.Contains(".p12"))
					{
						StoreFile = Path.Combine(SignToolPath, "OpenHarmony.p12");
						CertPath = Path.Combine(SignToolPath, "OpenHarmonyApplication.pem");
						string ProfileCertFile = Path.Combine(SignToolPath, "OpenHarmonyProfileRelease.pem");
						Profile = Path.Combine(SignToolPath, "myApplication_ohos_Provision.p7b");
						KeyAlias = "OpenHarmony Application Release";
						StorePassword = "123456";
						KeyPassword = "123456";

						string SignProfileCmd = "java -jar \"" + SignTool +
											"\" sign-profile -mode \"localSign\" -keyAlias \"OpenHarmony Application Profile Release\" -keyPwd \"123456\" -inFile \""
											+ InSignfileTemp + "\" -outFile \"" + Profile + "\" -keystoreFile \""
											+ StoreFile + "\" -keystorePwd \"123456\"  -signAlg \"SHA256withECDSA\" " + "-profileCertFile \"" + ProfileCertFile + "\"";
						RunCommandLineProgramWithExceptionAndFiltering(IntermediateOpenHarmonyPath, ShellExecutable, ShellParametersBegin + SignProfileCmd + ShellParametersEnd, Logger);
					}

					string SignAppCmd = "java -jar \"" + SignTool + "\" sign-app -mode \"localSign\" -keyAlias \"" + KeyAlias + "\" -inFile \"" +
										SourceUnSignedHapName + "\" -outFile \"" + SourceSignedHapName +
										"\" -keystoreFile \"" + StoreFile +
										"\" -keystorePwd " + StorePassword + " -keyPwd " + KeyPassword + " -signAlg SHA256withECDSA -profileFile \""
										+ Profile + "\" -appCertFile \"" + CertPath + "\"";
					RunCommandLineProgramWithExceptionAndFiltering(IntermediateOpenHarmonyPath, ShellExecutable, ShellParametersBegin + SignAppCmd + ShellParametersEnd, Logger);

					File.Copy(SourceSignedHapName, DestHapName, true);
					File.Delete(InSignfileTemp);
					
					// 生成安装脚本
					string HapInstallScript = Path.Combine(DestHapPath, "Install_" + ProjectName + Arch + ".bat");
					string SDKPath = Environment.GetEnvironmentVariable("OHSDKROOT")!;
					string HDCPath = Path.Combine(SDKPath, "openharmony", "toolchains", "hdc");
					File.WriteAllLines(HapInstallScript, new []
					{
						"\"" + HDCPath + "\" " + " install " + DestHapName,
						"@pause"  
					});
				}
				else 
				{
					RunCommandLineProgramWithExceptionAndFiltering(IntermediateOpenHarmonyPath, ShellExecutable, ShellParametersBegin + "hvigorw clean --no-daemon" + ShellParametersEnd, Logger, "hivigor clean...");
					RunCommandLineProgramWithExceptionAndFiltering(IntermediateOpenHarmonyPath, ShellExecutable, ShellParametersBegin + "hvigorw --mode project -p product=default assembleApp --info --no-daemon" + ShellParametersEnd, Logger, "Making app with hivigor...");
					string BuildAppOutputsPath = Path.Combine(IntermediateOpenHarmonyPath, "build", "outputs", "default/");
					string SourceUnSignedAppName = BuildAppOutputsPath + "OpenHarmony-default-unsigned.app";
					string SourceSignedAppName = BuildAppOutputsPath + "OpenHarmony-default-signed.app";
					string AppFileName = ProjectName + Arch + ".app";
					string DestAppPath = Path.Combine(ProjectDirectory, "Binaries", "OpenHarmony/");
					string DestAppName = Path.Combine(DestAppPath, AppFileName);
					MakeDirectoryIfRequired(DestAppPath);

					// 调用自动化签名工具
					string PackageName = GetPackageName(ProjectName);
					string SignToolPath = Path.Combine(OpenHarmonyTemplatePath, "temp", "tools");
					string InSignfile = Path.Combine(SignToolPath, "UnsgnedReleasedProfileTemplate.json");
					string InSignfileTemp = Path.Combine(SignToolPath, "Temp.json");

					//Modify the json file so for auto signature
					string jsonString = File.ReadAllText(InSignfile);
					// Convert the JSON string to a JObject:
					JObject jObject = (Newtonsoft.Json.JsonConvert.DeserializeObject(jsonString) as JObject)!;
					// Select a nested property using a single string:
					JToken jToken = jObject.SelectToken("bundle-info.bundle-name")!;
					// Update the value of the property: 
					jToken.Replace(PackageName);
					// Convert the JObject back to a string:
					string updatedJsonString = jObject.ToString();
					File.WriteAllText(InSignfileTemp, updatedJsonString);

					//Get the path of signing tools
					string SignTool = Path.Combine(SignToolPath, "hap-sign-tool.jar");
					string CertPath;
					string StorePassword;
					string KeyAlias;
					string KeyPassword;
					string Profile;
					string StoreFile;
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "CertPath", out CertPath);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "StorePassword", out StorePassword);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "KeyAlias", out KeyAlias);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "KeyPassword", out KeyPassword);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "Profile", out Profile);
					Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "StoreFile", out StoreFile);
					CertPath = Path.Combine(IntermediateOpenHarmonyPath, CertPath);
					Profile = Path.Combine(IntermediateOpenHarmonyPath, Profile);
					StoreFile = Path.Combine(IntermediateOpenHarmonyPath, StoreFile);
					//三个证书缺少任意一个都需要进行自动签名
					if (!CertPath.Contains(".cer") || !Profile.Contains(".p7b") || !StoreFile.Contains(".p12"))
					{
						StoreFile = Path.Combine(SignToolPath, "OpenHarmony.p12");
						CertPath = Path.Combine(SignToolPath, "OpenHarmonyApplication.pem");
						string ProfileCertFile = Path.Combine(SignToolPath, "OpenHarmonyProfileRelease.pem");
						Profile = Path.Combine(SignToolPath, "myApplication_ohos_Provision.p7b");
						KeyAlias = "OpenHarmony Application Release";
						StorePassword = "123456";
						KeyPassword = "123456";

						string SignProfileCmd = "java -jar \"" + SignTool +
											"\" sign-profile -mode \"localSign\" -keyAlias \"OpenHarmony Application Profile Release\" -keyPwd \"123456\" -inFile \""
											+ InSignfileTemp + "\" -outFile \"" + Profile + "\" -keystoreFile \""
											+ StoreFile + "\" -keystorePwd \"123456\"  -signAlg \"SHA256withECDSA\" " + "-profileCertFile \"" + ProfileCertFile + "\"";
						RunCommandLineProgramWithExceptionAndFiltering(IntermediateOpenHarmonyPath, ShellExecutable, ShellParametersBegin + SignProfileCmd + ShellParametersEnd, Logger);
					}

					string SignApplicationCmd = "java -jar \"" + SignTool + "\" sign-app -mode \"localSign\" -keyAlias \"" + KeyAlias + "\" -inFile \"" +
										SourceUnSignedAppName + "\" -outFile \"" + SourceSignedAppName +
										"\" -keystoreFile \"" + StoreFile +
										"\" -keystorePwd " + StorePassword + " -keyPwd " + KeyPassword + " -signAlg SHA256withECDSA -profileFile \""
										+ Profile + "\" -appCertFile \"" + CertPath + "\"";
					RunCommandLineProgramWithExceptionAndFiltering(IntermediateOpenHarmonyPath, ShellExecutable, ShellParametersBegin + SignApplicationCmd + ShellParametersEnd, Logger);
					
					File.Copy(SourceSignedAppName, DestAppName, true);
					File.Delete(InSignfileTemp);
					
					// 生成安装脚本
					string HapInstallScript = Path.Combine(DestAppPath, "Install_" + ProjectName + Arch + ".bat");
					string SDKPath = Environment.GetEnvironmentVariable("OHSDKROOT")!;
					string HDCPath = Path.Combine(SDKPath, "openharmony", "toolchains", "hdc");
					File.WriteAllLines(HapInstallScript, new []
					{
						"\"" + HDCPath + "\" " + " install " + DestAppName,
						"@pause"  
					});
				}
			}
		}

		private void ReplaceBuildProfile(string IntermediateOpenHarmonyPath)
		{
			string BuildProfile = Path.Combine(IntermediateOpenHarmonyPath, "build-profile.json5");
			using (JsonWriter JsonWriter = new JsonWriter(BuildProfile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteObjectStart("app");
				JsonWriter.WriteArrayStart("signingConfigs");
				
				JsonWriter.WriteArrayEnd();

				JsonWriter.WriteArrayStart("products");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "default");
				JsonWriter.WriteValue("signingConfig", "default");
				JsonWriter.WriteValue("compatibleSdkVersion", "5.0.0(12)");
				JsonWriter.WriteValue("runtimeOS", "HarmonyOS");
				JsonWriter.WriteObjectStart("buildOption");
				JsonWriter.WriteObjectStart("strictMode");
				JsonWriter.WriteValue("useNormalizedOHMUrl", true);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();

				JsonWriter.WriteArrayStart("modules");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "entry");
				JsonWriter.WriteValue("srcPath", "./entry");
				JsonWriter.WriteArrayStart("targets");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "default");
				JsonWriter.WriteArrayStart("applyToProducts");
				JsonWriter.WriteValue("default");
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();

				JsonWriter.WriteObjectEnd();
			}
		}
		
		private void ReplaceEntryBuildProfileJson(string IntermediateOpenHarmonyPath)
		{
			string BuildProfile = Path.Combine(IntermediateOpenHarmonyPath, "entry", "build-profile.json5");
			using (JsonWriter JsonWriter = new JsonWriter(BuildProfile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("apiType", "stageMode");
				JsonWriter.WriteObjectStart("buildOption");
				JsonWriter.WriteObjectStart("arkOptions");
				JsonWriter.WriteObjectStart("runtimeOnly");
				JsonWriter.WriteArrayStart("packages");
				//entry目录下的build-profile.json5添加Package依赖
				HashSet<string> HarPackageNames = new HashSet<string>(){""};
				string[] addHarPackageNames = UPL!.ProcessPluginNode("arm64-v8a", "EntryBuildProfileJsonAddHarPackageNames", "").Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
				foreach (var PackageName in addHarPackageNames)
				{
					HarPackageNames.Add(PackageName.Trim());
				}
				foreach (var PackageName in HarPackageNames)
				{
					if (PackageName.Length > 0)
					{
						JsonWriter.WriteValue(PackageName.ToLower());
					}
				}
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
				
				JsonWriter.WriteObjectStart("externalNativeOptions");
				JsonWriter.WriteValue("path", "./src/main/cpp/CMakeLists.txt");
				JsonWriter.WriteValue("arguments", "");
				JsonWriter.WriteValue("cppFlags", "");
				JsonWriter.WriteObjectEnd();
				
				JsonWriter.WriteObjectStart("nativeLib");
				JsonWriter.WriteObjectStart("debugSymbol");
				JsonWriter.WriteValue("strip", true);
				JsonWriter.WriteArrayStart("exclude");
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart("sourceOption");
				JsonWriter.WriteArrayStart("workers");
				JsonWriter.WriteValue("./src/main/ets/workers/UeWorker.ets");
				JsonWriter.WriteValue("./src/main/ets/workers/AkiWorker.ets");
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
				
				JsonWriter.WriteArrayStart("targets");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "default");
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "ohosTest");
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
			}
		}
		
		private void ReplaceProjectOHPackageJson(string IntermediateOpenHarmonyPath)
		{
			string BuildProfile = Path.Combine(IntermediateOpenHarmonyPath, "oh-package.json5");
			string AppName;
			ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppName", out AppName);
			string HarDirectory = Path.Combine(".", "entry", "hars");
			using (JsonWriter JsonWriter = new JsonWriter(BuildProfile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("modelVersion", "5.0.0");
				JsonWriter.WriteValue("description", "Please describe the basic information.");
				JsonWriter.WriteObjectStart("dependencies");
				//工程的依赖写在这里
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart("overrides");
				//工程依赖替换为另一个版本的写在这里
				HashSet<string> HarPackageNames = new HashSet<string>(){""};
				string[] addHarPackageNames = UPL!.ProcessPluginNode("arm64-v8a", "ProjectOHPackageJsonOverridesAddHarPackageNames", "").Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
				foreach (var PackageName in addHarPackageNames)
				{
					HarPackageNames.Add(PackageName.Trim());
				}
				foreach (var PackageName in HarPackageNames)
				{
					//当前har包路径写死的，./entry/hars
					if (PackageName.Length > 0)
					{
						string HarPackageName = PackageName + ".har";
						JsonWriter.WriteValue(PackageName.ToLower(), Path.Combine(HarDirectory, HarPackageName));
					}
				}
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart("devDependencies");
				JsonWriter.WriteValue("@ohos/hypium", "1.0.18");
				JsonWriter.WriteValue("@ohos/hamock", "1.0.0");
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
			}
		}

		private void ReplaceEntryOHPackageJson(string IntermediateOpenHarmonyPath){
			string BuildProfile = Path.Combine(IntermediateOpenHarmonyPath, "entry", "oh-package.json5");
			using (JsonWriter JsonWriter = new JsonWriter(BuildProfile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "entry");
				JsonWriter.WriteValue("version", "1.0.0");
				JsonWriter.WriteValue("description", "Please describe the basic information.");
				JsonWriter.WriteValue("main", "");
				JsonWriter.WriteValue("author", "");
				JsonWriter.WriteValue("license", "");
				JsonWriter.WriteObjectStart("dependencies");
				HashSet<string> Dependencies = new HashSet<string>(){""};
				string[] addDependencies = UPL!.ProcessPluginNode("arm64-v8a", "addDependenciesInEntryOHPackage", "").Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
				foreach (var dependency in addDependencies)
				{
					Dependencies.Add(dependency.Trim());
				}
				foreach (var dependency in Dependencies)
				{
					if (dependency.Length > 0 && dependency.Contains("="))
					{
						string[] dependencyPair = dependency.Replace(" ", "").Split('=');
						string key = dependencyPair[0], value = dependencyPair[1];
						JsonWriter.WriteValue(key, value);
					}
				}
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart("devDependencies");
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart("dynamicDependencies");
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
			}
		}
		
		private string ConvertOrientationIniValue(string IniValue)
		{
			switch (IniValue.ToLower())
			{
				case "unspecified":
					return "unspecified";
				case "portrait":
					return "portrait";
				case "portrait_inverted":
					return "portrait_inverted";
				case "landscape_inverted":
					return "landscape_inverted";
				case "auto_rotation":
					return "auto_rotation";
				case "auto_rotation_portrait":
					return "auto_rotation_portrait";
				case "auto_rotation_landscape":
					return "auto_rotation_landscape";
				case "auto_rotation_restricted":
					return "auto_rotation_restricted";
				case "auto_rotation_portrait_restricted":
					return "auto_rotation_portrait_restricted";
				case "auto_rotation_landscape_restricted":
					return "auto_rotation_landscape_restricted";
				case "locked":
					return "locked";
				default:
					return "landscape";
			}
		}
		
		private string GetOrientation()
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string Orientation;
			Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "Orientation",
				out Orientation);
			// check for UPL override
			string OrientationOverride = UPL!.ProcessPluginNode("arm64-v8a", "orientationOverride", "");
			if (!String.IsNullOrEmpty(OrientationOverride))
			{
				Orientation = OrientationOverride;
			}
			
			return ConvertOrientationIniValue(Orientation);
		}
		
		private void ReplaceAppJson(string ProjectName, string IntermediateOpenHarmonyPath)
		{
			string PackageName = GetPackageName(ProjectName);
			ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
			ConfigHierarchy GameIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
			string Vendor = "";
			int VersionCode = 1;
			string VersionName = "1.0.0.0";
			GameIni.GetString("/Script/EngineSettings.GeneralProjectSettings", "CompanyName", out Vendor);
			EngineIni.GetInt32("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "VersionCode", out VersionCode);
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "VersionDisplayName", out VersionName);

			string appJsonFile = IntermediateOpenHarmonyPath + "/AppScope/app.json5";
			using (JsonWriter JsonWriter = new JsonWriter(appJsonFile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteObjectStart("app");
				JsonWriter.WriteValue("bundleName", PackageName);
				JsonWriter.WriteValue("vendor", Vendor);
				JsonWriter.WriteValue("versionCode", VersionCode);
				JsonWriter.WriteValue("versionName", VersionName);
				JsonWriter.WriteValue("icon", "$media:app_icon");
				JsonWriter.WriteValue("label", "$string:app_name");
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
			}

			string appElementFile = Path.Combine(IntermediateOpenHarmonyPath, "AppScope", "resources", "base", "element", "string.json");
			string AppName;
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppName", out AppName);
			if (string.IsNullOrEmpty(AppName))
			{
				AppName = "ueGame";
			}
			
			using (JsonWriter JsonWriter = new JsonWriter(appElementFile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteArrayStart("string");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "app_name");
				JsonWriter.WriteValue("value", AppName);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
			}

			string DesIconFile = Path.Combine(IntermediateOpenHarmonyPath, "AppScope", "resources", "base", "media", "app_icon.png");
			string SourceIconFile;
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppIcon", out SourceIconFile);
			CleanCopyFile(SourceIconFile, DesIconFile);	
		}
		
		private void ReplaceModuleJson(string IntermediateOpenHarmonyPath)
		{
			
		}
		
		private void ReplaceElementJson(string ProjectName, string IntermediateOpenHarmonyPath)
		{
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
			string ModuleDesc;
			string AbilityDesc;
			string AbilityLabel;
			string AbilityReason;
			Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "ModuleDesc", out ModuleDesc);
			Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AbilityDesc", out AbilityDesc);
			Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppName", out AbilityLabel);
			Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AbilityReason", out AbilityReason);
			if (string.IsNullOrEmpty(AbilityLabel))
			{
				AbilityLabel = "ueGame";
			}
			bool bShowLaunchImage = false;
			Ini.GetBool("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "bShowLaunchImage", out bShowLaunchImage);

			string ModuleStringJsonFile = IntermediateOpenHarmonyPath + "/entry/src/main/resources/base/element/string.json";
			using (JsonWriter JsonWriter = new JsonWriter(ModuleStringJsonFile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteArrayStart("string");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "module_desc");
				JsonWriter.WriteValue("value", string.IsNullOrEmpty(ModuleDesc) ? "description" : ModuleDesc);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "EntryAbility_desc");
				JsonWriter.WriteValue("value", string.IsNullOrEmpty(AbilityDesc) ? "description" : AbilityDesc);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "EntryAbility_label");
				JsonWriter.WriteValue("value", string.IsNullOrEmpty(AbilityLabel) ? "label" : AbilityLabel);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "reason");
				JsonWriter.WriteValue("value", string.IsNullOrEmpty(AbilityReason) ? "reason" : AbilityReason);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
			}
			
			string ModuleBooleanJsonFile = Path.Combine(IntermediateOpenHarmonyPath, "entry", "src", "main", "resources", "base", "element", "boolean.json");
			using (JsonWriter JsonWriter = new JsonWriter(ModuleBooleanJsonFile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteArrayStart("boolean");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "bUseSplash");
				JsonWriter.WriteValue("value", bShowLaunchImage);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
			}

			string DesIconFile = Path.Combine(IntermediateOpenHarmonyPath, "entry", "src", "main", "resources", "base", "media", "ability_icon.png");
			string SourceIconFile;
			Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppIcon", out SourceIconFile);
			CleanCopyFile(SourceIconFile, DesIconFile);
		}

		public static UnrealArch GetUnrealArch(string NDKArch)
		{
			switch (NDKArch)
			{
				case "arm64-v8a":
				case "arm64": return UnrealArch.Arm64;
				case "x86_64":
				case "x64": return UnrealArch.X64;
				//				default: throw new BuildException("Unknown NDK architecture '{0}'", NDKArch);
				// future-proof by returning arm64 for unknown
				default: return UnrealArch.Arm64;

			}
		}

		private void UpdateEntryAbility(string NDKArch, string EngineDir, string UnrealBuildPath)
		{
			string SourceFilename = Path.Combine(EngineDir, "Build", "OpenHarmony", "entry", "src", "main", "ets", "entryability", "EntryAbility.ets.template");
			string DestFilename = Path.Combine(UnrealBuildPath, "entry", "src", "main", "ets", "entryability", "EntryAbility.ets");
			// check for EntryAbility.ets.template override
			if (UPL != null)
			{
				SourceFilename = UPL.ProcessPluginNode(NDKArch, "EntryAbilityReplacement", SourceFilename);
			}
			Dictionary<string, string> Replacements = new Dictionary<string, string>{
				{ "//$${entryAbilityImportAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityImportAdditions", "") ?? ""},
				{ "//$${entryAbilityStaticInitializationAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityStaticInitializationAdditions", "") ?? ""},
				{ "//$${entryAbilityOnCreateAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityOnCreateAdditions", "") ?? ""},
				{ "//$${entryAbilityOnNewWantAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityOnNewWantAdditions", "") ?? ""},
				{ "//$${entryAbilityWindowStageActiveAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityWindowStageActiveAdditions", "") ?? ""},
				{ "//$${entryAbilityWindowStageInactiveAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityWindowStageInactiveAdditions", "") ?? ""},
				{ "//$${entryAbilityClassAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityClassAdditions", "") ?? ""},
				{ "//$${entryAbilityExternalClassAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityExternalClassAdditions", "") ?? ""},
				{ "//$${entryAbilityonDestroyAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityonDestroyAdditions", "") ?? ""},
				{ "//$${entryAbilityonWindowStageCreateAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityonWindowStageCreateAdditions", "") ?? ""},
				{ "//$${entryAbilityonWindowStageDestroyAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityonWindowStageDestroyAdditions", "") ?? ""},
				{ "//$${entryAbilityonForegroundAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityonForegroundAdditions", "") ?? ""},
				{ "//$${entryAbilityonBackgroundAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityonBackgroundAdditions", "") ?? ""},
				{ "//$${entryAbilityonConfigurationUpdateAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityonConfigurationUpdateAdditions", "") ?? ""},
				{ "//$${entryAbilityWindowStageloadContentAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "entryAbilityWindowStageloadContentAdditions", "") ?? ""},
			};

			string[] TemplateSrc = File.ReadAllLines(SourceFilename);
			string[] TemplateDest = File.Exists(DestFilename) ? File.ReadAllLines(DestFilename) : [];

			bool TemplateChanged = false;
			for (int LineIndex = 0; LineIndex < TemplateSrc.Length; ++LineIndex)
			{
				string SrcLine = TemplateSrc[LineIndex];
				bool Changed = false;
				foreach (KeyValuePair<string, string> KVP in Replacements)
				{
					if(SrcLine.Contains(KVP.Key))
					{
						SrcLine = SrcLine.Replace(KVP.Key, KVP.Value);
						Changed = true;
					}
				}
				if (Changed)
				{
					TemplateSrc[LineIndex] = SrcLine;
					TemplateChanged = true;
				}
			}
			if (TemplateChanged)
			{
				// deal with insertions of newlines
				TemplateSrc = string.Join("\n", TemplateSrc).Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);
			}
			if (TemplateDest == null || TemplateSrc.Length != TemplateDest.Length || !TemplateSrc.SequenceEqual(TemplateDest))
			{
				Logger.LogInformation("\n==== Writing new EntryAbility.ets file to {0} ====", DestFilename);
				MakeDirectoryIfRequired(DestFilename);
				File.WriteAllLines(DestFilename, TemplateSrc);
			}
		}

		private void UpdateAkiJSBind(string NDKArch, string EngineDir, string UnrealBuildPath)
		{
			string SourceFilename = Path.Combine(EngineDir, "Build", "OpenHarmony", "entry", "src", "main", "ets", "common", "AkiJSBind.ets.template");
			string DestFilename = Path.Combine(UnrealBuildPath, "entry", "src", "main", "ets", "common", "AkiJSBind.ets");
			// check for AkiJSBind.ets.templat override
			if (UPL != null)
			{
				SourceFilename = UPL.ProcessPluginNode(NDKArch, "gameActivityReplacement", SourceFilename);
			}
			Dictionary<string, string> Replacements = new Dictionary<string, string>{
				{ "//$${AkiJSBindImportAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "AkiJSBindImportAdditions", "") ?? ""},
				{ "//$${AkiJSBindMainThreadAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "AkiJSBindMainThreadAdditions", "") ?? ""},
				{ "//$${AkiJSBindUeWorkerAdditions}$$", UPL?.ProcessPluginNode(NDKArch, "AkiJSBindUeWorkerAdditions", "") ?? ""}
			};
			string[] TemplateSrc = File.ReadAllLines(SourceFilename);
			string[] TemplateDest = File.Exists(DestFilename) ? File.ReadAllLines(DestFilename) : [];
			bool TemplateChanged = false;
			for (int LineIndex = 0; LineIndex < TemplateSrc.Length; ++LineIndex)
			{
				string SrcLine = TemplateSrc[LineIndex];
				bool Changed = false;
				foreach (KeyValuePair<string, string> KVP in Replacements)
				{
					if(SrcLine.Contains(KVP.Key))
					{
						SrcLine = SrcLine.Replace(KVP.Key, KVP.Value);
						Changed = true;
					}
				}
				if (Changed)
				{
					TemplateSrc[LineIndex] = SrcLine;
					TemplateChanged = true;
				}
			}
			if (TemplateChanged)
			{
				// deal with insertions of newlines
				TemplateSrc = string.Join("\n", TemplateSrc).Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);
			}
			if (TemplateDest == null || TemplateSrc.Length != TemplateDest.Length || !TemplateSrc.SequenceEqual(TemplateDest))
			{
				Logger.LogInformation("\n==== Writing new AkiJSBind.ets file to {0} ====", DestFilename);
				MakeDirectoryIfRequired(DestFilename);
				File.WriteAllLines(DestFilename, TemplateSrc);
			}
		}
	}
}
