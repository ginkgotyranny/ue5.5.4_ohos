// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

///////////////////////////////////////////////////////////////////
// If you are looking for supported version numbers, look in the
// OpenHarmonyPlatformSDK.Versions.cs file next to this file
///////////////////////////////////////////////////////////////////

namespace UnrealBuildTool
{
	partial class OpenHarmonyPlatformSDK : UEBuildPlatformSDK
	{
		public OpenHarmonyPlatformSDK(ILogger Logger)
			: base(Logger)
		{
		}

		// TODO:获取OHNDKROOT
		public static string? GetNDKRoot()
		{
			string? NDKPath = Environment.GetEnvironmentVariable("OHNDKROOT");
			if (string.IsNullOrEmpty(NDKPath))
			{
				Log.WriteLine(LogEventType.Log, ">>> Failed to retrieve NDK from environment variables, using fixed NDK <<<");
			}
			Log.WriteLine(LogEventType.Log, " NDKPath：" + NDKPath);
			// don't register if we don't have an NDKROOT specified
			if (!String.IsNullOrEmpty(NDKPath))
			{
				return NDKPath.Replace("\"", "");
			}

			if (OperatingSystem.IsMacOS())
			{
				Dictionary<string, string> OpenHarmonyEnv = new Dictionary<string, string>();
				Dictionary<string, string> EnvVarNames = new Dictionary<string, string> {
														 {"OHSDKROOT", "SDKPath"},
														 {"OHNDKROOT", "NDKPath"},
														 {"DEVECO_PATH", "DevEcoPath"}
														 };

				string BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bash_profile");
				if (!File.Exists(BashProfilePath))
				{
					// Try .bashrc if didn't fine .bash_profile
					BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bashrc");
				}
				if (File.Exists(BashProfilePath))
				{
					string[] BashProfileContents = File.ReadAllLines(BashProfilePath);

					// Walk backwards so we keep the last export setting instead of the first
					for (int LineIndex = BashProfileContents.Length - 1; LineIndex >= 0; --LineIndex)
					{
						foreach (KeyValuePair<string, string> kvp in EnvVarNames)
						{
							if (OpenHarmonyEnv.ContainsKey(kvp.Key))
							{
								continue;
							}
							if (BashProfileContents[LineIndex].StartsWith("export " + kvp.Key + "="))
							{
								string PathVar = BashProfileContents[LineIndex].Split('=')[1].Replace("\"", "");
								OpenHarmonyEnv.Add(kvp.Key, PathVar);
							}
						}
					}
				}

				// Set for the process
				foreach (KeyValuePair<string, string> kvp in OpenHarmonyEnv)
				{
					Environment.SetEnvironmentVariable(kvp.Key, kvp.Value);
				}

				NDKPath = Environment.GetEnvironmentVariable("OHNDKROOT");
				if (!String.IsNullOrEmpty(NDKPath))
				{
					return NDKPath.Replace("\"", "");
				}
			}
			return null;
		}

		// TODO:获取当前已安装的SDK
		protected override string? GetInstalledSDKVersion()
		{
			Log.WriteLine(LogEventType.Log, " 获取SDK版本为：12 ");
			return "12";
			/*
			string? NDKPath = GetNDKRoot();

			// don't register if we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				return null;
			}
			NDKPath = NDKPath.Replace("\"", "");

			// figure out the NDK version
			string? NDKToolchainVersion = null;
			string SourcePropFilename = Path.Combine(NDKPath, "source.properties");
			if (File.Exists(SourcePropFilename))
			{
				string RevisionString = "";
				string[] PropertyContents = File.ReadAllLines(SourcePropFilename);
				foreach (string PropertyLine in PropertyContents)
				{
					if (PropertyLine.StartsWith("Pkg.Revision"))
					{
						RevisionString = PropertyLine;
						break;
					}
				}

				int EqualsIndex = RevisionString.IndexOf('=');
				if (EqualsIndex > 0)
				{
					string[] RevisionParts = RevisionString.Substring(EqualsIndex + 1).Trim().Split('.');
					int RevisionMinor = Int32.Parse(RevisionParts.Length > 1 ? RevisionParts[1] : "0");
					char RevisionLetter = Convert.ToChar('a' + RevisionMinor);
					NDKToolchainVersion = "r" + RevisionParts[0] + (RevisionMinor > 0 ? Char.ToString(RevisionLetter) : "");
				}
			}
			else
			{
				string ReleaseFilename = Path.Combine(NDKPath, "RELEASE.TXT");
				if (File.Exists(ReleaseFilename))
				{
					string[] PropertyContents = File.ReadAllLines(SourcePropFilename);
					NDKToolchainVersion = PropertyContents[0];
				}
			}

			// return something like r10e, or null if anything went wrong above
			return NDKToolchainVersion;
			*/
		}

		public override bool TryConvertVersionToInt(string? StringValue, out ulong OutValue, string? Hint)
		{
			if (!String.IsNullOrEmpty(StringValue))
			{
				return UInt64.TryParse(StringValue, out OutValue);
			}

			OutValue = 0;
			return false;
		}

		//public override SDKStatus PrintSDKInfoAndReturnValidity(LogEventType Verbosity, LogFormatOptions Options, LogEventType ErrorVerbosity, LogFormatOptions ErrorOptions)
		//{
		//	SDKStatus Validity = base.PrintSDKInfoAndReturnValidity(Verbosity, Options, ErrorVerbosity, ErrorOptions);

		//	if (GetInstalledVersion() != GetMainVersion())
		//	{
		//		Log.WriteLine(Verbosity, Options, "Note: OpenHarmony toolchain NDK {0} recommended ('{1}' was found)", GetMainVersion(), GetInstalledVersion());
		//	}

		//	return Validity;
		//}

		protected override bool PlatformSupportsAutoSDKs()
		{
			return true;
		}

		// prefer auto sdk on openharmony as correct 'manual' sdk detection isn't great at the moment.
		protected override bool PreferAutoSDK()
		{
			return true;
		}

		private static bool ExtractPath(string Source, out string Path)
		{
			int start = Source.IndexOf('"');
			int end = Source.LastIndexOf('"');
			if (start != 1 && end != -1 && start < end)
			{
				++start;
				Path = Source.Substring(start, end - start);
				return true;
			}
			else
			{
				Path = "";
			}

			return false;
		}

		public static bool GetPath(ConfigHierarchy Ini, string SectionName, string Key, out string Value)
		{
			string? temp;
			if (Ini.TryGetValue(SectionName, Key, out temp))
			{
				return ExtractPath(temp, out Value);
			}
			else
			{
				Value = "";
			}

			return false;
		}

		/// <summary>
		/// checks if the sdk is installed or has been synced
		/// </summary>
		/// <returns></returns>
		protected virtual bool HasAnySDK()
		{
			// The OpenHarmony SDK is not required to build OpenHarmonyTargetPlatform. So for installed builds where its expected another machine will have
			// the SDK setup we can force this to be on to build OpenHarmonyTargetPlatform.
			string? ForceOpenHarmonySDK = Environment.GetEnvironmentVariable("FORCE_OPENHARMONY_SDK_ENABLED");

			// OPENHARMONY_SDK_HOME defined messes up newer OpenHarmony Gradle plugin finding of .openharmony so clear it
			Environment.SetEnvironmentVariable("OPENHARMONY_SDK_HOME", null);

			if (!String.IsNullOrEmpty(ForceOpenHarmonySDK))
			{
				return true;
			}

			string? NDKPath = Environment.GetEnvironmentVariable("OHNDKROOT");
			string? DevEcoPath = Environment.GetEnvironmentVariable("DEVECO_PATH");
			// string? NDKPath = "D:/develops/deveco/DevEco Studio/sdk/default/openharmony/native";;
			// string? DevEcoPath = "D:/develops/deveco/DevEco Studio";
			{
				ConfigHierarchy configCacheIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, null, BuildHostPlatform.Current.Platform);
				Dictionary<string, string> OpenHarmonyEnv = new Dictionary<string, string>();

				Dictionary<string, string> EnvVarNames = new Dictionary<string, string> {
														 {"OHSDKROOT", "SDKPath"},
														 {"OHNDKROOT", "NDKPath"},
														 {"DEVECO_PATH", "DevEcoPath"}
														 };

				string path;
				foreach (KeyValuePair<string, string> kvp in EnvVarNames)
				{
					if (GetPath(configCacheIni, "/Script/OpenHarmonyPlatformEditor.OpenHarmonySDKSettings", kvp.Value, out path) && !String.IsNullOrEmpty(path))
					{
						OpenHarmonyEnv.Add(kvp.Key, path);
					}
					else
					{
						string? envValue = Environment.GetEnvironmentVariable(kvp.Key);
						if (!String.IsNullOrEmpty(envValue))
						{
							OpenHarmonyEnv.Add(kvp.Key, envValue);
						}
					}
				}

				// If we are not running on Windows and we are still missing a key then go and find it from the .bash_profile
				if (!RuntimePlatform.IsWindows && !EnvVarNames.All(s => OpenHarmonyEnv.ContainsKey(s.Key)))
				{
					string BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bash_profile");
					if (!File.Exists(BashProfilePath))
					{
						// Try .bashrc if didn't fine .bash_profile
						BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bashrc");
					}
					if (File.Exists(BashProfilePath))
					{
						string[] BashProfileContents = File.ReadAllLines(BashProfilePath);

						// Walk backwards so we keep the last export setting instead of the first
						for (int LineIndex = BashProfileContents.Length - 1; LineIndex >= 0; --LineIndex)
						{
							foreach (KeyValuePair<string, string> kvp in EnvVarNames)
							{
								if (OpenHarmonyEnv.ContainsKey(kvp.Key))
								{
									continue;
								}

								if (BashProfileContents[LineIndex].StartsWith("export " + kvp.Key + "="))
								{
									string PathVar = BashProfileContents[LineIndex].Split('=')[1].Replace("\"", "");
									OpenHarmonyEnv.Add(kvp.Key, PathVar);
								}
							}
						}
					}
				}

				// Set for the process
				foreach (KeyValuePair<string, string> kvp in OpenHarmonyEnv)
				{
					Environment.SetEnvironmentVariable(kvp.Key, kvp.Value);
				}

				// See if we have an NDK path now...
				OpenHarmonyEnv.TryGetValue("OHNDKROOT", out NDKPath);
				OpenHarmonyEnv.TryGetValue("DEVECO_PATH", out DevEcoPath);
			}

			// we don't have an OHNDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				Logger.LogInformation("OHNDKROOT not set");
				return false;
			}

			NDKPath = NDKPath.Replace("\"", "");

			// need a supported llvm
			if (!Directory.Exists(Path.Combine(NDKPath, @"llvm")))
			{
				Logger.LogInformation("OHNDKROOT llvm missing");
				return false;
			}

			// check DevEcoPath is valid
			if (String.IsNullOrEmpty(DevEcoPath))
			{
				Logger.LogInformation("DevEcoPath not set");
				return false;
			}
			DevEcoPath = DevEcoPath.Replace("\"", "");

			return true;
		}
	}
}
