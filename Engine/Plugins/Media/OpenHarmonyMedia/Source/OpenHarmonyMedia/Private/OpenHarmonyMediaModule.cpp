// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyMediaPrivate.h"

#include "Modules/ModuleManager.h"

#include "IOpenHarmonyMediaModule.h"
#include "Player/OpenHarmonyMediaPlayer.h"


DEFINE_LOG_CATEGORY(LogOpenHarmonyMedia);


/**
 * Implements the OpenHarmonyMedia module.
 */
class FOpenHarmonyMediaModule
        : public IOpenHarmonyMediaModule
{
public:

    //~ IOpenHarmonyMediaModule interface

    virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
    {
        if (!IsSupported())
        {
            return nullptr;
        }

        return MakeShared<FOpenHarmonyMediaPlayer, ESPMode::ThreadSafe>(EventSink);
    }

public:

    //~ IModuleInterface interface

    virtual void StartupModule() override { }
    virtual void ShutdownModule() override { }

protected:

    /**
     * Check whether media is supported on the running device.
     *
     * @return true if media is supported, false otherwise.
     */
    bool IsSupported()
    {
        return true;//(FOpenHarmonyMisc::GetOpenHarmonyBuildVersion() >= 14);注释掉的方法是类安卓的版本判定。但是鸿蒙现在还没有版本判定
    }
};


IMPLEMENT_MODULE(FOpenHarmonyMediaModule, OpenHarmonyMedia)
