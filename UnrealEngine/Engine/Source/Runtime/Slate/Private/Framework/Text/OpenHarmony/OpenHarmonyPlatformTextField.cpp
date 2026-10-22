// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/OpenHarmony/OpenHarmonyPlatformTextField.h"

#if USE_OPENHARMONY_NAPI
#include "HAL/IConsoleManager.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"

// OpenHarmony InputType class
#define INPUTTYPE_NORMAL		0
#define INPUTTYPE_NUMBER		1
#define INPUTTYPE_PHONENUMBER	2
#define INPUTTYPE_EMAIL			3
#define INPUTTYPE_PASSWORD		4

int32 GOpenHarmonyNewKeyboard = 0;
static FAutoConsoleVariableRef CVarOpenHarmonyNewKeyboard(
	TEXT("OpenHarmony.NewKeyboard"),
	GOpenHarmonyNewKeyboard,
	TEXT("Controls usage of experimental new keyboard input. 0 uses the checkbox setting, 1 forces new keyboard, 2 forces dialog. (Default: 0)"),
	ECVF_Default );

void FOpenHarmonyPlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget)
{
	// Set the EditBox inputType based on keyboard type
	int32 InputType;
	
	if (bShow)
	{
		switch (TextEntryWidget->GetVirtualKeyboardType())
		{
			case EKeyboardType::Keyboard_Number:
				InputType = INPUTTYPE_NUMBER;
				break;
			case EKeyboardType::Keyboard_Web:
				InputType = INPUTTYPE_NORMAL;
				break;
			case EKeyboardType::Keyboard_Email:
				InputType = INPUTTYPE_EMAIL;
				break;
			case EKeyboardType::Keyboard_Password:
				InputType = INPUTTYPE_PASSWORD;
				break;
			case EKeyboardType::Keyboard_AlphaNumeric:
			case EKeyboardType::Keyboard_Default:
			default:
				InputType = INPUTTYPE_NORMAL;
				break;
		}
	}

	// bool bIsUsingIntegratedKeyboard = EnableNewKeyboardConfig();
	// switch (GOpenHarmonyNewKeyboard)
	// {
	// case 1:
	// 	bIsUsingIntegratedKeyboard = true;
	// 	break;
	// case 2:
	// 	bIsUsingIntegratedKeyboard = false;
	// 	break;

	// default:
	// 	break;
	// }

	//Currently, OpenHarmony only implements dialogue mode keyboard input

	// if (bIsUsingIntegratedKeyboard)
	// {
	// 	if (bShow)
	// 	{
	// 		// Show alert for input
	// 		if (TextEntryWidget->IsMultilineEntry())
	// 			InputType |= TYPE_TEXT_FLAG_MULTI_LINE;
	// 		extern void OpenHarmonyThunkCpp_ShowVirtualKeyboardInput(TSharedPtr<IVirtualKeyboardEntry>, int32, const FString&, const FString&);
	// 		OpenHarmonyThunkCpp_ShowVirtualKeyboardInput(TextEntryWidget, InputType, TextEntryWidget->GetHintText().ToString(), TextEntryWidget->GetText().ToString());
	// 	}
	// 	else
	// 	{
	// 		extern void OpenHarmonyThunkCpp_HideVirtualKeyboardInput();
	// 		OpenHarmonyThunkCpp_HideVirtualKeyboardInput();
	// 	}
	// }
	// else
	// {
		if (bShow)
		{
			// Show alert for input
			extern void ShowVirtualKeyboardInputDialog(TSharedPtr<IVirtualKeyboardEntry>, int32, const FString&, const FString&);
			ShowVirtualKeyboardInputDialog(TextEntryWidget, InputType, TextEntryWidget->GetHintText().ToString(), TextEntryWidget->GetText().ToString());
		}
		else
		{
			extern void HideVirtualKeyboardInputDialog();
			HideVirtualKeyboardInputDialog();
		}
	// }
}

bool FOpenHarmonyPlatformTextField::AllowMoveCursor()
{
	return !EnableNewKeyboardConfig();
}

// bool FOpenHarmonyPlatformTextField::ShouldUseAutocorrect(TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) const
// {
// 	bool bUseAutocorrect = IPlatformTextField::ShouldUseVirtualKeyboardAutocorrect(TextEntryWidget);

// 	if (bUseAutocorrect)
// 	{
// 		static FString DeviceModel = FOpenHarmonyMisc::GetDeviceModel();

// 		TArray<FString> ExcludedDeviceModels;
// 		GConfig->GetArray(TEXT("/Script/Engine.InputSettings"), TEXT("ExcludedAutocorrectDeviceModels"), ExcludedDeviceModels, GInputIni);

// 		for (const FString& ExcludedModel : ExcludedDeviceModels)
// 		{
// 			if (DeviceModel.StartsWith(ExcludedModel))
// 			{
// 				bUseAutocorrect = false;
// 				break;
// 			}
// 		}
// 	}

// 	return bUseAutocorrect;
// }

bool FOpenHarmonyPlatformTextField::EnableNewKeyboardConfig() const
{
	// read the value from the config file
	static bool bEnableNewKeyboardConfig = false;
	GConfig->GetBool(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("bEnableNewKeyboard"), bEnableNewKeyboardConfig, GEngineIni);

	// use integrated keyboard if the runtime setting is set or the console variable is set to 1
	return bEnableNewKeyboardConfig;
}
#endif