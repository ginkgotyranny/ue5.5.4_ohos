#pragma once
#include "GenericPlatform/GenericPlatformProcess.h"

/** Dummy process handle for platforms that use generic implementation. */
struct FProcHandle : public TProcHandle<void*, nullptr>
{
public:
	/** Default constructor. */
	FORCEINLINE FProcHandle()
		: TProcHandle()
	{}

	/** Initialization constructor. */
	FORCEINLINE explicit FProcHandle(HandleType Other)
		: TProcHandle(Other)
	{}
};

/**
 * OpenHarmony implementation of the Process OS functions
 **/
struct CORE_API FOpenHarmonyPlatformProcess : public FGenericPlatformProcess
{
	static void* GetDllHandle(const TCHAR* Filename);
	static void FreeDllHandle(void* DllHandle);
	static void* GetDllExport(void* DllHandle, const TCHAR* ProcName);
	static const TCHAR* ComputerName();
	static void SetThreadAffinityMask(uint64 AffinityMask);
	static uint32 GetCurrentProcessId();
	static uint32 GetCurrentCoreNumber();
	static const TCHAR* BaseDir();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static class FRunnableThread* CreateRunnableThread();
	static bool CanLaunchURL(const TCHAR* URL);
	static void LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error);
	static void SetAffinityInfo(bool bEnableAffinity, int32 bigCoreMask, int32 littleCoreMask);
	static FString GetGameBundleId();
};

typedef FOpenHarmonyPlatformProcess FPlatformProcess;

