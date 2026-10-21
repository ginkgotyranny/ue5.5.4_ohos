#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"

class FOpenHarmonyAffinity : public FGenericPlatformAffinity
{
	private:
	static uint64 GetLittleCoreMask();
};

typedef FOpenHarmonyAffinity FPlatformAffinity;
