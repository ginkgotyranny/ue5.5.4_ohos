// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Interface to allow exposing public methods from the toolchain to other assemblies
	/// </summary>
	public interface IOpenHarmonyToolChain
	{

	}

	/// <summary>
	/// Interface to allow exposing public methods from the OpenHarmony deployment context to other assemblies
	/// </summary>
	public interface IOpenHarmonyDeploy
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Architectures"></param>
		/// <param name="inPluginExtraData"></param>
		void SetOpenHarmonyPluginData(UnrealArchitectures Architectures, List<string> inPluginExtraData);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="ProjectName"></param>
		/// <param name="ProjectDirectory"></param>
		/// <param name="ExecutablePath"></param>
		/// <param name="EngineDirectory"></param>
		/// <param name="CookFlavor"></param>
		/// <param name="Configuration"></param>
		/// <param name="bSkipGradleBuild"></param>
		/// <returns></returns>
		bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, DirectoryReference ProjectDirectory, string ExecutablePath, string EngineDirectory, string CookFlavor, UnrealTargetConfiguration Configuration, bool bSkipGradleBuild);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectName"></param>
		/// <param name="ProjectDirectoryFullName"></param>
		/// <param name="Type"></param>
		/// <param name="bIsEmbedded"></param>
		bool SavePackageInfo(string ProjectName, string ProjectDirectoryFullName, TargetType Type, bool bIsEmbedded);
	}

	/// <summary>
	/// Public OpenHarmony functions exposed to UAT
	/// </summary>
	public static class OpenHarmonyExports
	{
		
		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <returns></returns>
		public static IOpenHarmonyToolChain CreateToolChain(FileReference ProjectFile)
		{
			return new OpenHarmonyToolChain(ProjectFile, Log.Logger);
		}
		
		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="InForcePackageData"></param>
		/// <returns></returns>
		public static IOpenHarmonyDeploy CreateDeploymentHandler(FileReference ProjectFile, bool InForcePackageData)
		{
			return new UEDeployOpenHarmony(ProjectFile, InForcePackageData, Log.Logger);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="NDKArch"></param>
		/// <returns></returns>
		public static UnrealArch GetUnrealArch(string NDKArch)
		{
			return UEDeployOpenHarmony.GetUnrealArch(NDKArch);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="SourceFile"></param>
		/// <param name="TargetFile"></param>
		/// <param name="Logger">Logger for output</param>
		public static void StripSymbols(FileReference SourceFile, FileReference TargetFile, ILogger Logger)
		{
			OpenHarmonyToolChain ToolChain = new OpenHarmonyToolChain(null, Logger);
			ToolChain.StripSymbols(SourceFile, TargetFile, Logger);
		}
	}
}
