#include "CoreMinimal.h"
#include "OpenHarmonyTargetPlatformSettings.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Interfaces/ITargetPlatformSettingsModule.h"
#include "Interfaces/ITargetPlatformControlsModule.h"
#include "OpenHarmony/OpenHarmonyPlatformProperties.h"
#include "MultiPlatformTargetReceiptBuildWorkers.h"
#include "IOpenHarmonyTargetPlatformSettingsModule.h"

/**
 * Module for the OpenHarmonys target platform settings.
 */
class FOpenHarmonyTargetPlatformSettingsModule
	: public IOpenHarmonyTargetPlatformSettingsModule
{
public:

	/** Destructor. */
	~FOpenHarmonyTargetPlatformSettingsModule() 
	{
	}

public:
	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms)
	{

	}
	void GetSinglePlatformSettings(TMap<EOpenHarmonyTextureFormatCategory, ITargetPlatformSettings*>& OutMap)
	{
		OutMap = SinglePlatformSettings;
	}

	ITargetPlatformSettings* GetMultiPlatformSettings()
	{
		return MultiTargetPlatformSettings;
	}

protected:
	virtual void GetTargetPlatformSettings(TArray<ITargetPlatformSettings*>& TargetPlatforms)
	{
		ITargetPlatformSettings* VanillaTP = new FOpenHarmonyTargetPlatformSettings();
		TargetPlatforms.Add(VanillaTP);
		SinglePlatformSettings.Add(EOpenHarmonyTextureFormatCategory::Count, VanillaTP);

		ITargetPlatformSettings* DXTTP = new FOpenHarmony_DXTTargetPlatformSettings();
		TargetPlatforms.Add(DXTTP);
		SinglePlatformSettings.Add(EOpenHarmonyTextureFormatCategory::DXT, DXTTP);

		ITargetPlatformSettings* ETC2TP = new FOpenHarmony_ETC2TargetPlatformSettings();
		TargetPlatforms.Add(ETC2TP);
		SinglePlatformSettings.Add(EOpenHarmonyTextureFormatCategory::ETC2, ETC2TP);

		ITargetPlatformSettings*ASTCTP = new FOpenHarmony_ASTCTargetPlatformSettings();
		TargetPlatforms.Add(ASTCTP);
		SinglePlatformSettings.Add(EOpenHarmonyTextureFormatCategory::ASTC, ASTCTP);

		ITargetPlatformSettings* MultiTP = new FOpenHarmony_MultiTargetPlatformSettings();
		TargetPlatforms.Add(MultiTP);
		MultiTargetPlatformSettings = MultiTP;
	}
private:
	ITargetPlatformSettings* MultiTargetPlatformSettings;
	TMap<EOpenHarmonyTextureFormatCategory, ITargetPlatformSettings*> SinglePlatformSettings;
};

IMPLEMENT_MODULE(FOpenHarmonyTargetPlatformSettingsModule, OpenHarmonyTargetPlatformSettings);