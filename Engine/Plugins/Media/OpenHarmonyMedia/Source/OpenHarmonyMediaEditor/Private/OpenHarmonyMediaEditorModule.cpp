// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


/**
 * Implements the OpenHarmonyMediaEditor module.
 */
class FOpenHarmonyMediaEditorModule
        : public IModuleInterface
{
public:

    //~ IModuleInterface interface

    virtual void StartupModule() override { }
    virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FOpenHarmonyMediaEditorModule, OpenHarmonyMediaEditor);
