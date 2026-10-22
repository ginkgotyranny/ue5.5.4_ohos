#include "CoreMinimal.h"
#include "OpenHarmonyTargetPlatformControls.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Interfaces/ITargetPlatformControlsModule.h"
#include "OpenHarmony/OpenHarmonyPlatformProperties.h"
#include "MultiPlatformTargetReceiptBuildWorkers.h"
#include "IOpenHarmonyTargetPlatformControlsModule.h"
#include "IOpenHarmonyTargetPlatformSettingsModule.h"

#define LOCTEXT_NAMESPACE "FOpenHarmonyTargetPlatformControlsModule"

/**
 * Module for the OpenHarmony target platform controls.
 */
class FOpenHarmonyTargetPlatformControlsModule
	: public IOpenHarmonyTargetPlatformControlsModule
{
public:
	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms)
	{

	}
	/** Destructor. */
	~FOpenHarmonyTargetPlatformControlsModule()
	{
	}
protected:
	virtual void GetTargetPlatformControls(TArray<ITargetPlatformControls*>& TargetPlatforms, FName& PlatformSettingsModuleName)
	{
		if (FOpenHarmonyTargetPlatformControls::IsUsable())
		{
			// ensure this is only called once
			check(SinglePlatforms.IsEmpty());

			TMap<EOpenHarmonyTextureFormatCategory, ITargetPlatformSettings*> OutMap;
			ITargetPlatformSettings* OutMulti = nullptr;
			IOpenHarmonyTargetPlatformSettingsModule* Module = FModuleManager::GetModulePtr<IOpenHarmonyTargetPlatformSettingsModule>(PlatformSettingsModuleName);
			if (Module != nullptr)
			{
				Module->GetSinglePlatformSettings(OutMap);
				OutMulti = Module->GetMultiPlatformSettings();
			}

			for (int32 Type = 0; Type < 2; Type++)
			{
				bool bIsClient = Type == 1;

				// flavorless must come first
				// "flavorless" (aka "vanilla") OpenHarmony is not well defined and not actually usable
				//  but Editor wants to see it in the platform list
				SinglePlatforms.Add(new FOpenHarmonyTargetPlatformControls(bIsClient, OutMap[EOpenHarmonyTextureFormatCategory::Count], nullptr));
				SinglePlatforms.Add(new FOpenHarmony_ASTCTargetPlatformControls(bIsClient, OutMap[EOpenHarmonyTextureFormatCategory::ASTC]));
				SinglePlatforms.Add(new FOpenHarmony_DXTTargetPlatformControls(bIsClient, OutMap[EOpenHarmonyTextureFormatCategory::DXT]));
				SinglePlatforms.Add(new FOpenHarmony_ETC2TargetPlatformControls(bIsClient, OutMap[EOpenHarmonyTextureFormatCategory::ETC2]));

				// these are used in NotifyMultiSelectedFormatsChanged, so track in another array
				MultiPlatforms.Add(new FOpenHarmony_MultiTargetPlatformControls(bIsClient, OutMulti));
			}

			// join the single and the multi into one
			TargetPlatforms.Append(SinglePlatforms);
			TargetPlatforms.Append(MultiPlatforms);

			// set up the multi platforms now that we have all the other platforms ready to go
			NotifyMultiSelectedFormatsChanged();
		}
	}

	virtual void NotifyMultiSelectedFormatsChanged() override
	{
		for (FOpenHarmony_MultiTargetPlatformControls* TP : MultiPlatforms)
		{
			TP->LoadFormats(SinglePlatforms);
		}
		// @todo multi needs to be passed this event!
	}

private:
	/** Holds the specific types of target platforms for NotifyMultiSelectedFormatsChanged */
	TArray<FOpenHarmonyTargetPlatformControls*> SinglePlatforms;
	TArray<FOpenHarmony_MultiTargetPlatformControls*> MultiPlatforms;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FOpenHarmonyTargetPlatformControlsModule, OpenHarmonyTargetPlatformControls);