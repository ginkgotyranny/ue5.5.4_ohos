#include "OpenHarmonyTargetSettingsCustomization.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SBoxPanel.h"
#include "Engine/GameEngine.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "OpenHarmonyRuntimeSettings.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"

#include "SExternalImageReference.h"
#include "SHyperlinkLaunchURL.h"
#include "SPlatformSetupMessage.h"
#include "PlatformIconInfo.h"
#include "SourceControlHelpers.h"
#include "ManifestUpdateHelper.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/EngineBuildSettings.h"
#include "InstalledPlatformInfo.h"

#include "Interfaces/IMainFrameModule.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "OpenHarmonyRuntimeSettings"

//////////////////////////////////////////////////////////////////////////
// FOpenHarmonyTargetSettingsCustomization
namespace FOpenHarmonyTargetSettingsCustomizationConstants
{
	const FText DisabledTip = LOCTEXT("GitHubSourceRequiredToolTip", "This requires GitHub source.");
}

TSharedRef<IDetailCustomization> FOpenHarmonyTargetSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FOpenHarmonyTargetSettingsCustomization);
}

FOpenHarmonyTargetSettingsCustomization::FOpenHarmonyTargetSettingsCustomization()
	: LastLicenseChecktime(-1.0)
	, OpenHarmonyRelativePath(TEXT(""))
	, EngineOpenHarmonyPath(FPaths::EngineDir() + TEXT("Build/OpenHarmony"))
	, GameOpenHarmonyPath(FPaths::ProjectDir() + TEXT("Build/OpenHarmony"))
	, EngineProjectPropertiesPath(EngineOpenHarmonyPath / TEXT("project.properties"))
	, GameProjectPropertiesPath(GameOpenHarmonyPath / TEXT("project.properties"))
{
	IconNames.Emplace(TEXT("entry/src/main/resources/base/media/ability_icon.png"), LOCTEXT("SettingsIcon", "LabelIcon"), FText::GetEmpty(), 48, 48, FPlatformIconInfo::Required);
	IconNames.Emplace(TEXT("AppScope/resources/base/media/app_icon.png"), LOCTEXT("SettingsIcon", "AppIcon"), FText::GetEmpty(), 48, 48, FPlatformIconInfo::Required);

	//LaunchImageNames.Emplace(TEXT("res/drawable/downloadimagev.png"), LOCTEXT("SettingsIcon_DownloadImageV", "Download Background Vertical Image"), FText::GetEmpty(), 720, 1280, FPlatformIconInfo::Required);
	//LaunchImageNames.Emplace(TEXT("res/drawable/downloadimageh.png"), LOCTEXT("SettingsIcon_DownloadImageH", "Download Background Horizontal Image"), FText::GetEmpty(), 1280, 720, FPlatformIconInfo::Required);
	//LaunchImageNames.Emplace(TEXT("res/drawable/splashscreen_portrait.png"), LOCTEXT("LaunchImage_Portrait", "Launch Portrait"), FText::GetEmpty(), 360, 640, FPlatformIconInfo::Required);
	LaunchImageNames.Emplace(TEXT("entry/src/main/resources/base/media/splashscreen_landscape.png"), LOCTEXT("LaunchImage_Landscape", "Launch Landscape"), FText::GetEmpty(), 640, 360, FPlatformIconInfo::Required);
}

void FOpenHarmonyTargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	SavedLayoutBuilder = &DetailLayout;

	BuildIconSection(DetailLayout);
	BuildLaunchImageSection(DetailLayout);
}

static void OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
{
	const FString* URL = Metadata.Find(TEXT("href"));
	
	if(URL)
	{
		FPlatformProcess::LaunchURL(**URL, nullptr, nullptr);
	}
}

void FOpenHarmonyTargetSettingsCustomization::BuildIconSection(IDetailLayoutBuilder& DetailLayout)
{
	// Icon category
	IDetailCategoryBuilder& IconCategory = DetailLayout.EditCategory(TEXT("Icons"));

	for (const FPlatformIconInfo& Info : IconNames)
	{
		const FString AutomaticImagePath = EngineOpenHarmonyPath / Info.IconPath;
		const FString TargetImagePath = GameOpenHarmonyPath / Info.IconPath;

		IconCategory.AddCustomRow(Info.IconName)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding( FMargin( 0, 1, 0, 1 ) )
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(Info.IconName)
				.Font(DetailLayout.GetDetailFont())
			]
		]
		.ValueContent()
		.MaxDesiredWidth(400.0f)
		.MinDesiredWidth(100.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SExternalImageReference, AutomaticImagePath, TargetImagePath)
				.FileDescription(Info.IconDescription)
				.RequiredSize(Info.IconRequiredSize)
				.MaxDisplaySize(FVector2D(FMath::Min(96, Info.IconRequiredSize.X), FMath::Min(96, Info.IconRequiredSize.Y)))
			]
		];
	}
}

void FOpenHarmonyTargetSettingsCustomization::BuildLaunchImageSection(IDetailLayoutBuilder& DetailLayout)
{
	// Add the launch images
	IDetailCategoryBuilder& LaunchImageCategory = DetailLayout.EditCategory(TEXT("LaunchImages"));
	LaunchImageCategory.AddCustomRow(LOCTEXT("LaunchImageInfo", "Launch Image Info"), false)
		.WholeRowWidget
		[
			SNew(SBorder)
			.Padding(1)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(10, 10, 10, 10))
				.FillWidth(1.0f)
				[
					SNew(SRichTextBlock)
					.Text(LOCTEXT("LaunchImageInfoMessage", "The <RichTextBlock.TextHighlight>Download Background</> image is used as the background for OBB downloading.  The <RichTextBlock.TextHighlight>Launch Portrait</> image is used as a splash screen for applications with Portrait, Reverse Portrait, Sensor Portrait, Sensor, or Full Sensor orientation.  The <RichTextBlock.TextHighlight>Launch Landscape</> image is used as a splash screen for applications with Landscape, Sensor Landscape, Reverse Landscape, Sensor, or Full Sensor orientation.\n\nThe launch images will be scaled to fit the device in the active orientation. Additional optional launch images may be provided as overrides for LDPI, MDPI, HDPI, and XHDPI by placing them in the project's corresponding Build/OpenHarmony/res/drawable-* directory."))
					.TextStyle(FAppStyle::Get(), "MessageLog")
					.DecoratorStyleSet(&FAppStyle::Get())
					.AutoWrapText(true)
					+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateStatic(&OnBrowserLinkClicked))
				]
			]
		];

	const FVector2D LaunchImageMaxSize(150.0f, 150.0f);

	for (const FPlatformIconInfo& Info : LaunchImageNames)
	{
		const FString AutomaticImagePath = EngineOpenHarmonyPath / Info.IconPath;
		const FString TargetImagePath = GameOpenHarmonyPath / Info.IconPath;

		LaunchImageCategory.AddCustomRow(Info.IconName)
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0, 1, 0, 1))
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(Info.IconName)
					.Font(DetailLayout.GetDetailFont())
				]
			]
			.ValueContent()
			.MaxDesiredWidth(400.0f)
			.MinDesiredWidth(100.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SExternalImageReference, AutomaticImagePath, TargetImagePath)
					.FileDescription(Info.IconDescription)
//					.RequiredSize(Info.IconRequiredSize)
					.MaxDisplaySize(LaunchImageMaxSize)
				]
			];
	}
}

void FOpenHarmonyTargetSettingsCustomization::CopySetupFilesIntoProject()
{
	FText ErrorMessage;
	
	// Now try to copy all of the icons, etc... (these can be ignored if the file already exists)
	for (const FPlatformIconInfo& Info : IconNames)
	{
		const FString EngineImagePath = EngineOpenHarmonyPath / Info.IconPath;
		const FString ProjectImagePath = GameOpenHarmonyPath / Info.IconPath;

		if (!FPaths::FileExists(ProjectImagePath))
		{
			SourceControlHelpers::CopyFileUnderSourceControl(ProjectImagePath, EngineImagePath, Info.IconName, /*out*/ ErrorMessage);
		}
	}

	// Now try to copy all of the launch images... (these can be ignored if the file already exists)
	for (const FPlatformIconInfo& Info : LaunchImageNames)
	{
		const FString EngineImagePath = EngineOpenHarmonyPath / Info.IconPath;
		const FString ProjectImagePath = GameOpenHarmonyPath / Info.IconPath;

		if (!FPaths::FileExists(ProjectImagePath))
		{
			SourceControlHelpers::CopyFileUnderSourceControl(ProjectImagePath, EngineImagePath, Info.IconName, /*out*/ ErrorMessage);
		}
	}

	// and copy the other files (aren't required)
	//SourceControlHelpers::CopyFileUnderSourceControl(GameProguardPath, EngineProguardPath, LOCTEXT("Proguard", "Proguard Settings"), /*out*/ ErrorMessage);

	SavedLayoutBuilder->ForceRefreshDetails();
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
