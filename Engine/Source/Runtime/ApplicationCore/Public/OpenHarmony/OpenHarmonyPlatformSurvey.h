// Copyright Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	OpenHarmonySurvey.h: Google OpenHarmony platform hardware-survey classes
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSurvey.h"

/**
 * OpenHarmony implementation of FGenericPlatformSurvey
 **/
struct FOpenHarmonyPlatformSurvey : public FGenericPlatformSurvey
{
	static bool GetSurveyResults(FHardwareSurveyResults& OutResults, bool bWait=false);
};

typedef FOpenHarmonyPlatformSurvey FPlatformSurvey;