// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyMoviePlayer.h"
#include "OpenHarmonyMovieStreamer.h"

#include "Misc/CoreDelegates.h"
//防止引用头文件的其他文件重复编译
TSharedPtr<FOpenHarmonyMoviePlayerStreamer> OpenHarmonyMovieStreamer;

class FOpenHarmonyMoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FOpenHarmonyMoviePlayerStreamer* Streamer = new FOpenHarmonyMoviePlayerStreamer;
		OpenHarmonyMovieStreamer = MakeShareable(Streamer);
		
		FCoreDelegates::RegisterMovieStreamerDelegate.Broadcast(OpenHarmonyMovieStreamer); //注册托管
	}

	virtual void ShutdownModule() override
	{
        if (OpenHarmonyMovieStreamer.IsValid())
        {
            FCoreDelegates::UnRegisterMovieStreamerDelegate.Broadcast(OpenHarmonyMovieStreamer); //取消托管
        }

		OpenHarmonyMovieStreamer->Cleanup();

        OpenHarmonyMovieStreamer.Reset();
	}

private:

};

IMPLEMENT_MODULE( FOpenHarmonyMoviePlayerModule, OpenHarmonyMoviePlayer )
