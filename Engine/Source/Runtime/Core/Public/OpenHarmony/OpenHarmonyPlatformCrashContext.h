#pragma once

#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Containers/Map.h"

struct CORE_API FOpenHarmonyCrashContext : public FGenericCrashContext
{
	FOpenHarmonyCrashContext(ECrashContextType InType, const TCHAR* InErrorMessage);

	~FOpenHarmonyCrashContext()
	{
	}
};

typedef FOpenHarmonyCrashContext FPlatformCrashContext;