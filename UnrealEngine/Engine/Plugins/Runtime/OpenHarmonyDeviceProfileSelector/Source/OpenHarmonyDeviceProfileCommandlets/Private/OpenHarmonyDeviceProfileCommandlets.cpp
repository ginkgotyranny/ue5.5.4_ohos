// Copyright Epic Games, Inc. All Rights Reserved.
#include "OpenHarmonyDeviceProfileCommandlets.h"
#include "Modules/ModuleManager.h"


class FOpenHarmonyDeviceProfileCommandlets : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE( FOpenHarmonyDeviceProfileCommandlets, OpenHarmonyDeviceProfileCommandlets )
