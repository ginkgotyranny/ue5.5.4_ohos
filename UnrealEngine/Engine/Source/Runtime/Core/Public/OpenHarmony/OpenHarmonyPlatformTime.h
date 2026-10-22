// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformTime.h"
#include <sys/time.h>

/**
 * OpenHarmony implementation of the Time OS functions
 */
struct CORE_API FOpenHarmonyPlatformTime : public FGenericPlatformTime
{
	// openHarmony uses BSD time code from GenericPlatformTime
	static FORCEINLINE double Seconds()
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return ((double)ts.tv_sec) + (((double)ts.tv_nsec) / 1000000000.0);
	}

	static FORCEINLINE uint32 Cycles()
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (uint32)((((uint64)ts.tv_sec) * 1000000ULL) + (((uint64)ts.tv_nsec) / 1000ULL));
	}

	static FORCEINLINE uint64 Cycles64()
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return ((((uint64)ts.tv_sec) * 1000000ULL) + (((uint64)ts.tv_nsec) / 1000ULL));
	}
};

typedef FOpenHarmonyPlatformTime FPlatformTime;