// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct  FOpenHarmonyApplicationMisc : public FGenericPlatformApplicationMisc
{
	// default implementation for now
    static APPLICATIONCORE_API void LoadPreInitModules();
	static APPLICATIONCORE_API class GenericApplication* CreateApplication();
	static APPLICATIONCORE_API void ClipboardCopy(const TCHAR* Str);
	static APPLICATIONCORE_API void ClipboardPaste(class FString& Dest);
};

typedef FOpenHarmonyApplicationMisc FPlatformApplicationMisc;
