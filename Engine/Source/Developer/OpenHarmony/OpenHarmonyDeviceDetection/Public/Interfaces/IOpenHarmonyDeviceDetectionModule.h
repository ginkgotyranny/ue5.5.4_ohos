#pragma once

#include "Modules/ModuleInterface.h"

class IOpenHarmonyDeviceDetection;

/**
 * Interface for OpenHarmonyDeviceDetection module.
 */
class IOpenHarmonyDeviceDetectionModule
	: public IModuleInterface
{
public:
	/**
	 * Returns the openharmony device detection singleton.
	 * @param AlternamePlatformName If a platform needs a separate detection instance, pass in an identifier here to create a new one
	 */
	virtual IOpenHarmonyDeviceDetection* GetOpenHarmonyDeviceDetection(const TCHAR* AlternamePlatformName=TEXT("")) = 0;

protected:

	/**
	 * Virtual destructor
	 */
	virtual ~IOpenHarmonyDeviceDetectionModule( ) { }
};
