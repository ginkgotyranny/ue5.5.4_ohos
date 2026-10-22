// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonySDKSettingsCustomization.h"
#include "Modules/ModuleManager.h"
#include "Layout/Visibility.h"
#include "UnrealClient.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "OpenHarmonySDKSettings.h"
#include "PropertyHandle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"

#define LOCTEXT_NAMESPACE "OpenHarmonySDKSettings"

//////////////////////////////////////////////////////////////////////////
// FOpenHarmonySDKSettingsCustomization

TSharedRef<IDetailCustomization> FOpenHarmonySDKSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FOpenHarmonySDKSettingsCustomization);
}

FOpenHarmonySDKSettingsCustomization::FOpenHarmonySDKSettingsCustomization()
{
	TargetPlatformManagerModule = &FModuleManager::LoadModuleChecked<ITargetPlatformManagerModule>("TargetPlatform");
}

void FOpenHarmonySDKSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	SavedLayoutBuilder = &DetailLayout;
	BuildSDKPathSection(DetailLayout);
}


void FOpenHarmonySDKSettingsCustomization::BuildSDKPathSection(IDetailLayoutBuilder& DetailLayout)
{
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
