// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OpenHarmony/OpenHarmonyErrorOutputDevice.h"

#include "CoreGlobals.h"
#include "Misc/OutputDeviceHelper.h"
#include "HAL/PlatformMisc.h"
#include "Misc/OutputDeviceRedirector.h"
#include "CoreGlobals.h"
#include "Misc/CoreDelegates.h"

FOpenHarmonyErrorOutputDevice::FOpenHarmonyErrorOutputDevice()
{
}

void FOpenHarmonyErrorOutputDevice::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::LowLevelOutputDebugString(*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Msg, GPrintLogTimes));

	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if(GIsCriticalError == 0 && NewCallCount == 1)
	{
		// First appError.
		GIsCriticalError = 1;

		FCString::Strncpy(GErrorExceptionDescription, Msg, UE_ARRAY_COUNT(GErrorExceptionDescription));
	}
	else
	{
		UE_LOG(LogOpenHarmony, Error, TEXT("Error reentered: %s"), Msg);
	}

	if (GIsGuarded)
	{
		UE_DEBUG_BREAK();
	}
	else
	{
		HandleError();
		FPlatformMisc::RequestExit(true, TEXT("FOpenHarmonyErrorOutputDevice::Serialize.!GIsGuarded"));
	}
}

void FOpenHarmonyErrorOutputDevice::HandleError()
{
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);

	if (NewCallCount != 1)
	{
		UE_LOG(LogOpenHarmony, Error, TEXT("HandleError re-entered."));
		return;
	}
	
	GIsGuarded = 0;
	GIsRunning = 0;
	GIsCriticalError = 1;
	GLogConsole = NULL;
	GErrorHist[UE_ARRAY_COUNT(GErrorHist) - 1] = 0;

	// Dump the error and flush the log.
#if !NO_LOGGING
	FDebug::LogFormattedMessageWithCallstack(LogOpenHarmony.GetCategoryName(), __FILE__, __LINE__, TEXT("=== Critical error: ==="), GErrorHist, ELogVerbosity::Error);
#endif
	
	GLog->Panic();

	FCoreDelegates::OnHandleSystemError.Broadcast();
	FCoreDelegates::OnShutdownAfterError.Broadcast();
}
