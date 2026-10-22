#pragma  once 

/*------------------------------------------------------------------------------------
IOpenHarmony_MultiTargetPlatformModule interface 
------------------------------------------------------------------------------------*/

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatformControlsModule.h"

class IOpenHarmonyTargetPlatformControlsModule : public ITargetPlatformControlsModule
{
public:
	//
	// Called by OpenHarmonyRuntimeSettings to notify us when the user changes the selected texture formats for the Multi format
	//
	virtual void NotifyMultiSelectedFormatsChanged() = 0;
};
