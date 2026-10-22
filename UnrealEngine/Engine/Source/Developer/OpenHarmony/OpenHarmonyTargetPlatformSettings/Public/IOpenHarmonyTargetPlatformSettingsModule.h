#pragma  once 

/*------------------------------------------------------------------------------------
IOpenHarmonyTargetPlatformSettingsModule interface
------------------------------------------------------------------------------------*/

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatformSettingsModule.h"
#include "OpenHarmonyTargetPlatformSettings.h"

class IOpenHarmonyTargetPlatformSettingsModule : public ITargetPlatformSettingsModule
{
public:
	virtual void GetSinglePlatformSettings(TMap<EOpenHarmonyTextureFormatCategory, ITargetPlatformSettings*>& OutMap) = 0;
	virtual ITargetPlatformSettings* GetMultiPlatformSettings() = 0;
};
