// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	OpenHarmonySplash.h: OpenHarmony platform splash screen...
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSplash.h"

/**
 * OpenHarmony splash implementation
 */
struct APPLICATIONCORE_API FOpenHarmonySplash : public FGenericPlatformSplash
{
	// default implementation for now
};


typedef FOpenHarmonySplash FPlatformSplash;
