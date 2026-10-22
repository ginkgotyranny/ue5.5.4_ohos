#pragma  once 

/*------------------------------------------------------------------------------------
IOpenHarmony_MultiTargetPlatformModule interface 
------------------------------------------------------------------------------------*/

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatformModule.h"

class IOpenHarmonyTargetPlatformModule : public ITargetPlatformModule
{
public:
	//
	// Called by OpenHarmonyRuntimeSettings to notify us when the user changes the selected texture formats for the Multi format
	//
	virtual void NotifyMultiSelectedFormatsChanged() = 0;
};
