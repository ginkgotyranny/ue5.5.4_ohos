// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDeviceProfileSelectorModule.h"

/**
 * Implements the OpenHarmony Device Profile Selector module.
 */
class FOpenHarmonyDeviceProfileSelectorRuntimeModule
	: public IDeviceProfileSelectorModule
{
public:

	//~ Begin IDeviceProfileSelectorModule Interface
	virtual const FString GetRuntimeDeviceProfileName() override;
	virtual bool GetSelectorPropertyValue(const FName& PropertyType, FString& PropertyValueOUT) override;
	//~ End IDeviceProfileSelectorModule Interface


	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface

	
	/**
	 * Virtual destructor.
	 */
	virtual ~FOpenHarmonyDeviceProfileSelectorRuntimeModule()
	{
	}

private:

	/**
	 * Make sure we use the ArkTS workaround for view scaling on devices that require this
	 */
	void CheckForArkTSSurfaceViewWorkaround(const FString& DeviceMake, const FString& DeviceModel) const;
};