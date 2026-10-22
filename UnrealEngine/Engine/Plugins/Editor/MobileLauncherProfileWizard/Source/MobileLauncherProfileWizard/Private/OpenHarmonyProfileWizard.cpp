// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyProfileWizard.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
//#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "SProfileWizardUI.h"

#define LOCTEXT_NAMESPACE "MobileLauncherProfileWizard"

FText FOpenHarmonyProfileWizard::GetName() const
{
	return LOCTEXT("OpenHarmonyWizardName", "Minimal OpenHarmony APK + DLC...");
}

FText FOpenHarmonyProfileWizard::GetDescription() const
{
	return LOCTEXT("OpenHarmonyWizardDescription", "This wizard will help to create two launcher profiles, one for minimal OpenHarmony APK and another for downloadable content.");
}

namespace OpenHarmonyProfileConstants
{
	const FString AppPlatformName(TEXT("OpenHarmony_ETC2"));
	const FString AppReleaseName(TEXT("1.0"));
	const FString DLCName(TEXT("DLC1.0"));
}

static void SetupOpenHarmonyAppProfile(ILauncherProfileRef& AppProfile, const FProfileParameters& Params, const FString& ProjectPath)
{
	AppProfile->SetProjectSpecified(true);
	AppProfile->SetProjectPath(ProjectPath);

	AppProfile->SetBuildUAT(true);
	// App build configuration
	AppProfile->SetBuildMode(ELauncherProfileBuildModes::Auto);
	AppProfile->SetBuildConfiguration(Params.BuildConfiguration);
	
	//// Cooking
	AppProfile->SetCookMode(ELauncherProfileCookModes::ByTheBook);
	AppProfile->SetCookConfiguration(Params.BuildConfiguration);
	for (const FString& MapName : Params.AppMaps)
	{
		AppProfile->AddCookedMap(MapName);
	}
	AppProfile->AddCookedPlatform(OpenHarmonyProfileConstants::AppPlatformName);

	// Release settings
	AppProfile->SetCreateReleaseVersion(true);
	AppProfile->SetCreateReleaseVersionName(OpenHarmonyProfileConstants::AppReleaseName);
	AppProfile->SetIncrementalCooking(false);
	AppProfile->SetCompressed(false);
	AppProfile->SetDeployWithUnrealPak(true);

	// Packaging
	AppProfile->SetPackagingMode(ELauncherProfilePackagingModes::Locally);

	// Archive
	AppProfile->SetArchive(true);
	{
		FString AppDir = Params.ArchiveDirectory / TEXT("App/") / OpenHarmonyProfileConstants::AppReleaseName;
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		
		if (!PlatformFile.DirectoryExists(*AppDir))
		{
			PlatformFile.CreateDirectoryTree(*AppDir);
		}
		
		AppProfile->SetArchiveDirectory(AppDir);
	}
		
	// Deploy
	AppProfile->SetDeploymentMode(ELauncherProfileDeploymentModes::DoNotDeploy);

	// Launch
	AppProfile->SetLaunchMode(ELauncherProfileLaunchModes::DoNotLaunch);
}

static void SetupOpenHarmonyDLCProfile(ILauncherProfileRef& DLCProfile, const FProfileParameters& Params, const FString& ProjectPath)
{
	DLCProfile->SetProjectSpecified(true);
	DLCProfile->SetProjectPath(ProjectPath);

	DLCProfile->SetBuildUAT(true);
	// App build configuration
	DLCProfile->SetBuildMode(ELauncherProfileBuildModes::DoNotBuild);
	DLCProfile->SetBuildConfiguration(Params.BuildConfiguration);
		
	//// Cooking
	DLCProfile->SetCookMode(ELauncherProfileCookModes::ByTheBook);
	DLCProfile->SetCookConfiguration(Params.BuildConfiguration);
	for (const FString& MapName : Params.DLCMaps)
	{
		DLCProfile->AddCookedMap(MapName);
	}
	
	for (const FString& CookFlavor : Params.DLCCookFlavors)
	{
		DLCProfile->AddCookedPlatform(CookFlavor);
	}
	
	// Release settings
	DLCProfile->SetCreateReleaseVersion(false);
	DLCProfile->SetBasedOnReleaseVersionName(OpenHarmonyProfileConstants::AppReleaseName);
	DLCProfile->SetCreateDLC(true);
	DLCProfile->SetDLCName(OpenHarmonyProfileConstants::DLCName);
	DLCProfile->SetDLCIncludeEngineContent(true);
		
	DLCProfile->SetIncrementalCooking(false);
	DLCProfile->SetCompressed(false);
	DLCProfile->SetDeployWithUnrealPak(true);

	// HTTP chunk data
	DLCProfile->SetGenerateHttpChunkData(true);
	DLCProfile->SetHttpChunkDataReleaseName(OpenHarmonyProfileConstants::DLCName);
	{
		FString CloudDir = Params.ArchiveDirectory / TEXT("HTTPchunks/") / OpenHarmonyProfileConstants::DLCName;
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		
		if (!PlatformFile.DirectoryExists(*CloudDir))
		{
			PlatformFile.CreateDirectoryTree(*CloudDir);
		}
		
		DLCProfile->SetHttpChunkDataDirectory(CloudDir);
	}

	// Packaging
	DLCProfile->SetPackagingMode(ELauncherProfilePackagingModes::DoNotPackage);
	
	// Deploy
	DLCProfile->SetDeploymentMode(ELauncherProfileDeploymentModes::DoNotDeploy);

	// Launch
	DLCProfile->SetLaunchMode(ELauncherProfileLaunchModes::DoNotLaunch);
}

static void CreateOpenHarmonyProfiles(const FProfileParameters& Params, FString ProjectPath, ILauncherProfileManagerRef ProfileManager)
{
	const FString ProjectName = FPaths::GetBaseFilename(ProjectPath);
	const FString AppProfileName = ProjectName + TEXT(" - OpenHarmony APK");
	const FString DLCProfileName = ProjectName + TEXT(" - OpenHarmony DLC");

	// Make profile names unique
	FString AppProfileNameUnique = AppProfileName;
	FString DLCProfileNameUnique = DLCProfileName;
	int32 UniqueCounter = 1;
	while (ProfileManager->FindProfile(AppProfileNameUnique).IsValid() 
			|| ProfileManager->FindProfile(DLCProfileNameUnique).IsValid())
	{
		AppProfileNameUnique = AppProfileName + FString::FromInt(UniqueCounter);
		DLCProfileNameUnique = DLCProfileName + FString::FromInt(UniqueCounter);
		UniqueCounter++;
	}


	// Create archive directory
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Params.ArchiveDirectory))
	{
		PlatformFile.CreateDirectoryTree(*Params.ArchiveDirectory);
	}
	
	// Add App profile
	ILauncherProfileRef AppProfile = ProfileManager->AddNewProfile();
	SetupOpenHarmonyAppProfile(AppProfile, Params, ProjectPath);
	ProfileManager->ChangeProfileName(AppProfile, AppProfileNameUnique);
	ProfileManager->SaveJSONProfile(AppProfile);

	// Add DLC profile
	ILauncherProfileRef DLCProfile = ProfileManager->AddNewProfile();
	SetupOpenHarmonyDLCProfile(DLCProfile, Params, ProjectPath);
	ProfileManager->ChangeProfileName(DLCProfile, DLCProfileNameUnique);
	ProfileManager->SaveJSONProfile(DLCProfile);
}

void FOpenHarmonyProfileWizard::HandleCreateLauncherProfile(const ILauncherProfileManagerRef& ProfileManager)
{
	const FVector2D WindowSize = FVector2D(940, 540);

	FText WindowTitle = LOCTEXT("CreateOpenHarmonyProfileWizardTitle", "OpenHarmony APK + DLC");
	FString ProjectPath = ProfileManager->GetProjectPath();

	TSharedRef<SWindow> AddProfileWindow =
		SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(WindowSize)
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMinimize(false).SupportsMaximize(false);

	TSharedRef<SProfileWizardUI> ProfilesDialog =
		SNew(SProfileWizardUI)
		.ProfilePlatform(EProfilePlatform::OpenHarmony)
		.ProjectPath(ProjectPath)
		.OnCreateProfileEvent(FCreateProfileEvent::CreateStatic(&CreateOpenHarmonyProfiles, ProjectPath, ProfileManager));

	AddProfileWindow->SetContent(ProfilesDialog);
	FSlateApplication::Get().AddWindow(AddProfileWindow);
}

#undef LOCTEXT_NAMESPACE
