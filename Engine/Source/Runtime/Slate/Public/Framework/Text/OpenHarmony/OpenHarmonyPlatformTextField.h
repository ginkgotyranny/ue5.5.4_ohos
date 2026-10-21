// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// some OpenHarmony platforms disable this but not defined otherwise if OpenHarmonyPlatform.h not included first
#ifndef USE_OPENHARMONY_NAPI
	#define USE_OPENHARMONY_NAPI    1
#endif

#if USE_OPENHARMONY_NAPI

#include "CoreMinimal.h"
#include "Framework/Application/IPlatformTextField.h"

class IVirtualKeyboardEntry;

class FOpenHarmonyPlatformTextField : public IPlatformTextField
{
public:
	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override;
	virtual bool AllowMoveCursor() override;
private:
	bool ShouldUseAutocorrect(TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) const;
	bool EnableNewKeyboardConfig() const;
	//	SlateTextField* TextField;
};

typedef FOpenHarmonyPlatformTextField FPlatformTextField;

#else

#include "Framework/Text/GenericPlatformTextField.h"

#endif
