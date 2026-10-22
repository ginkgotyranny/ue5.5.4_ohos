#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IOpenHarmonyDeviceDetectionModule.h"
#include "ISettingsModule.h"
#include "OpenHarmonyRuntimeSettings.h"
#include "OpenHarmonySDKSettings.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "ISettingsModule.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "PropertyEditorModule.h"
#include "OpenHarmonyTargetSettingsCustomization.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
//#include "OpenHarmonySDKSettingsCustomization.h"
#include "ISettingsModule.h"
#include "MaterialShaderQualitySettingsCustomization.h"
#include "MaterialShaderQualitySettings.h"
#include "ComponentRecreateRenderStateContext.h"
#include "RenderingThread.h"
#include "ShaderPlatformQualitySettings.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FOpenHarmonyPlatformEditorModule"


/**
 * Module for OpenHarmony platform editor utilities
 */
class FOpenHarmonyPlatformEditorModule
	: public IModuleInterface
{
	virtual void StartupModule() override
	{
		// register settings detail panel customization
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			UOpenHarmonyRuntimeSettings::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FOpenHarmonyTargetSettingsCustomization::MakeInstance)
		);

		/*
		PropertyModule.RegisterCustomClassLayout(
			UOpenHarmonySDKSettings::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FOpenHarmonySDKSettingsCustomization::MakeInstance)
			);
		*/

		FOnUpdateMaterialShaderQuality UpdateMaterials = FOnUpdateMaterialShaderQuality::CreateLambda([]()
		{
			FGlobalComponentRecreateRenderStateContext Recreate;
			FlushRenderingCommands();
			UMaterial::AllMaterialsCacheResourceShadersForRendering();
			UMaterialInstance::AllMaterialsCacheResourceShadersForRendering();
		});

		PropertyModule.RegisterCustomClassLayout(
			UShaderPlatformQualitySettings::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FMaterialShaderQualitySettingsCustomization::MakeInstance, UpdateMaterials)
			);

		PropertyModule.NotifyCustomizationModuleChanged();

		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "OpenHarmony",
				LOCTEXT("RuntimeSettingsName", "OpenHarmony"),
				LOCTEXT("RuntimeSettingsDescription", "Project settings for OpenHarmony apps"),
				GetMutableDefault<UOpenHarmonyRuntimeSettings>()
			);

 			SettingsModule->RegisterSettings("Project", "Platforms", "OpenHarmonySDK",
 				LOCTEXT("SDKSettingsName", "OpenHarmony SDK"),
 				LOCTEXT("SDKSettingsDescription", "Settings for OpenHarmony SDK (for all projects)"),
 				GetMutableDefault<UOpenHarmonySDKSettings>()
			);

			{
				static FName NAME_GLSL_ES3_1_OPENHARMONY(TEXT("GLSL_ES3_1_OPENHARMONY"));
				UShaderPlatformQualitySettings* OpenHarmonyMaterialQualitySettings = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(NAME_GLSL_ES3_1_OPENHARMONY);
				SettingsModule->RegisterSettings("Project", "Platforms", "OpenHarmonyOpenGLMobileQuality",
					LOCTEXT("OpenHarmonyOpenGLMobileQualitySettingsName", "OpenHarmony Material Quality - OpenGL Mobile"),
					LOCTEXT("OpenHarmonyOpenGLMobileQualitySettingsDescription", "Settings for OpenHarmony OpenGL Mobile material quality"),
					OpenHarmonyMaterialQualitySettings
				);
			}
			{
				static FName NAME_SF_VULKAN_ES31_OPENHARMONY(TEXT("SF_VULKAN_ES31_OPENHARMONY"));
				UShaderPlatformQualitySettings* OpenHarmonyMaterialQualitySettings = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(NAME_SF_VULKAN_ES31_OPENHARMONY);
				SettingsModule->RegisterSettings("Project", "Platforms", "OpenHarmonyVulkanQuality",
					LOCTEXT("OpenHarmonyVulkanQualitySettingsName", "OpenHarmony Material Quality - Vulkan"),
					LOCTEXT("OpenHarmonyVulkanQualitySettingsDescription", "Settings for OpenHarmony Vulkan material quality"),
					OpenHarmonyMaterialQualitySettings
				);
			}
			{
				static FName NAME_SF_VULKAN_SM5_OPENHARMONY(TEXT("SF_VULKAN_SM5_OPENHARMONY"));
				UShaderPlatformQualitySettings* OpenHarmonyMaterialQualitySettings = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(NAME_SF_VULKAN_SM5_OPENHARMONY);
				SettingsModule->RegisterSettings("Project", "Platforms", "OpenHarmonyVulkanSM5Quality",
					LOCTEXT("OpenHarmonyVulkanSM5QualitySettingsName", "OpenHarmony SM5 Material Quality - Vulkan"),
					LOCTEXT("OpenHarmonyVulkanSM5QualitySettingsDescription", "Settings for OpenHarmony Vulkan SM5 material quality"),
					OpenHarmonyMaterialQualitySettings
				);
			}

		}


		auto OpenHarmonyRuntimeSettings = GetMutableDefault<UOpenHarmonyRuntimeSettings>();
		if (OpenHarmonyRuntimeSettings)
		{
			OpenHarmonyRuntimeSettings->OnPropertyChanged.AddLambda([this, OpenHarmonyRuntimeSettings](struct FPropertyChangedEvent& PropertyChangedEvent)
				{
					if (PropertyChangedEvent.Property != nullptr)
					{
						if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bBuildForES31) &&
							OpenHarmonyRuntimeSettings->bBuildForES31 == false)
						{
							FCoreDelegates::OnFeatureLevelDisabled.Broadcast(ERHIFeatureLevel::ES3_1, FName());
							return;
						}

						if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UOpenHarmonyRuntimeSettings, bSupportsVulkan) &&
							OpenHarmonyRuntimeSettings->bSupportsVulkan == false)
						{
							FCoreDelegates::OnFeatureLevelDisabled.Broadcast(ERHIFeatureLevel::ES3_1, LegacyShaderPlatformToShaderFormat(SP_VULKAN_ES3_1_OPENHARMONY));
							return;
						}
					}

				});
		}

		// Force the SDK settings into a sane state initially so we can make use of them
		auto &TargetPlatformManagerModule = FModuleManager::LoadModuleChecked<ITargetPlatformManagerModule>("TargetPlatform");
		UOpenHarmonySDKSettings * settings = GetMutableDefault<UOpenHarmonySDKSettings>();
		settings->SetTargetModule(&TargetPlatformManagerModule);
		auto &OpenHarmonyDeviceDetection = FModuleManager::LoadModuleChecked<IOpenHarmonyDeviceDetectionModule>("OpenHarmonyDeviceDetection");
		settings->SetDeviceDetection(OpenHarmonyDeviceDetection.GetOpenHarmonyDeviceDetection());
		settings->UpdateTargetModulePaths();
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "OpenHarmony");
			SettingsModule->UnregisterSettings("Project", "Platforms", "OpenHarmonySDK");
			SettingsModule->UnregisterSettings("Project", "Platforms", "OpenHarmonyES31Quality");
			SettingsModule->UnregisterSettings("Project", "Platforms", "OpenHarmonyVulkanQuality");
		}
	}
};


IMPLEMENT_MODULE(FOpenHarmonyPlatformEditorModule, OpenHarmonyPlatformEditor);

#undef LOCTEXT_NAMESPACE
