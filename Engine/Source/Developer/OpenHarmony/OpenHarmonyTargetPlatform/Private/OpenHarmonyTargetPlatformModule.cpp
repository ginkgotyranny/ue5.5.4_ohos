#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "OpenHarmony/OpenHarmonyPlatformProperties.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Common/TargetPlatformBase.h"
#include "Interfaces/IOpenHarmonyDeviceDetection.h"
#include "Interfaces/IOpenHarmonyDeviceDetectionModule.h"
#include "IOpenHarmonyTargetPlatformModule.h"

/**
 * Module for the OpenHarmony target platform.
 */
class FOpenHarmonyTargetPlatformModule : public ITargetPlatformModule
{
public:
	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms)
	{

	}

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms, TArray<ITargetPlatformSettings*> TargetPlatformSettings, TArray<ITargetPlatformControls*> TargetPlatformControls) override
	{
		for (ITargetPlatformControls* TargetPlatformControlsIt : TargetPlatformControls)
		{
			TargetPlatforms.Add(new FTargetPlatformMerged(TargetPlatformControlsIt->GetTargetPlatformSettings(), TargetPlatformControlsIt));
		}
	}
};

IMPLEMENT_MODULE( FOpenHarmonyTargetPlatformModule, OpenHarmonyTargetPlatform);
