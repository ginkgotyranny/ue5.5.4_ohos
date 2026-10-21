// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformStackWalk.h"

/**
* OpenHarmony platform stack walking
*/
struct CORE_API FOpenHarmonyPlatformStackWalk : public FGenericPlatformStackWalk
{
	// default implementation for now
	typedef FGenericPlatformStackWalk Parent;

	static void ProgramCounterToSymbolInfo(uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo);
	static uint32 CaptureStackBackTrace(uint64* BackTrace, uint32 MaxDepth, void* Context = nullptr);
	static bool SymbolInfoToHumanReadableString(const FProgramCounterSymbolInfo& SymbolInfo, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize);
};

typedef FOpenHarmonyPlatformStackWalk FPlatformStackWalk;