#include "OpenHarmony/OpenHarmonyPlatformProcess.h"
#include "OpenHarmony/OpenHarmonyPlatformAffinity.h"
#include "OpenHarmony/OpenHarmonyPlatformRunnableThread.h"
#include "Async/TaskGraphInterfaces.h"

#include <sys/syscall.h>
#include <pthread.h>
#include <dlfcn.h>

#include "Misc/CoreDelegates.h"


int64 FOpenHarmonyAffinity::GameThreadMask = FPlatformAffinity::GetNoAffinityMask();
int64 FOpenHarmonyAffinity::RenderingThreadMask = FPlatformAffinity::GetNoAffinityMask();

void* FOpenHarmonyPlatformProcess::GetDllHandle(const TCHAR* Filename)
{
	check(Filename);

	// Check if dylib is already loaded
	void* Handle = dlopen(TCHAR_TO_ANSI(Filename), RTLD_NOLOAD | RTLD_LAZY | RTLD_LOCAL);

	if (!Handle)
	{
		// Not loaded yet, so try to open it
		Handle = dlopen(TCHAR_TO_ANSI(Filename), RTLD_LAZY | RTLD_LOCAL);
	}
	if (!Handle)
	{
		UE_LOG(LogOpenHarmony, Warning, TEXT("dlopen failed: %s"), ANSI_TO_TCHAR(dlerror()));
	}
	return Handle;
}

void FOpenHarmonyPlatformProcess::FreeDllHandle(void* DllHandle)
{
	check(DllHandle);
	dlclose(DllHandle);
}

void* FOpenHarmonyPlatformProcess::GetDllExport(void* DllHandle, const TCHAR* ProcName)
{
	check(DllHandle);
	check(ProcName);
	return dlsym(DllHandle, TCHAR_TO_ANSI(ProcName));
}


const TCHAR* FOpenHarmonyPlatformProcess::ComputerName()
{
	static FString ComputerName;
	if (ComputerName.Len() == 0)
	{
		ComputerName = FOpenHarmonyMisc::GetDeviceModel();
	}

	return *ComputerName;
}

void FOpenHarmonyPlatformProcess::SetThreadAffinityMask(uint64 InAffinityMask)
{
	if (FPlatformAffinity::GetNoAffinityMask() != InAffinityMask)
	{
		pid_t ThreadId = gettid();
		int AffinityMask = (int)InAffinityMask;
		syscall(__NR_sched_setaffinity, ThreadId, sizeof(AffinityMask), &AffinityMask);
	}
}

uint32 FOpenHarmonyPlatformProcess::GetCurrentProcessId()
{
	return getpid();
}

uint32 FOpenHarmonyPlatformProcess::GetCurrentCoreNumber()
{
	unsigned CPU;
	int Err = syscall(__NR_getcpu, &CPU, nullptr, nullptr);
	return (!Err) ? CPU : 0;
}

const TCHAR* FOpenHarmonyPlatformProcess::BaseDir()
{
	return TEXT("");
}

// const TCHAR* FOpenHarmonyPlatformProcess::ExecutableName(bool bRemoveExtension)
// {
// 	return TEXT("");
	
// }
const TCHAR* FOpenHarmonyPlatformProcess::ExecutableName(bool bRemoveExtension)
{
#if USE_OPENHARMONY_FILE
	extern FString GOpenHarmonyProjectName;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("GOpenHarmonyProjectName is : %s"), *GOpenHarmonyProjectName);
	
	return *GOpenHarmonyProjectName;
#else
	UE_LOG(LogOpenHarmony, Fatal, TEXT("A sub-platform that doesn't use USE_OPENHARMONY_FILE must implement PlatformProcess::ExecutableName"));
	return TEXT("");
#endif
}

FRunnableThread* FOpenHarmonyPlatformProcess::CreateRunnableThread()
{
	return new FRunnableThreadOpenHarmony();
}

bool FOpenHarmonyPlatformProcess::CanLaunchURL(const TCHAR* URL)
{
	return URL != nullptr;
}

DECLARE_DELEGATE_OneParam(FOpenHarmonyLaunchURLDelegate, const FString&);

CORE_API FOpenHarmonyLaunchURLDelegate OnOpenHarmonyLaunchURL;

void FOpenHarmonyPlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	if (FCoreDelegates::ShouldLaunchUrl.IsBound() && !FCoreDelegates::ShouldLaunchUrl.Execute(URL))
	{
		if (Error)
		{
			*Error = TEXT("LaunchURL cancelled by delegate");
		}
		return;
	}

	check(URL);
	const FString URLWithParams = FString::Printf(TEXT("%s %s"), URL, Parms ? Parms : TEXT("")).TrimEnd();

	OnOpenHarmonyLaunchURL.ExecuteIfBound(URLWithParams);

	if (Error != nullptr)
	{
		*Error = TEXT("");
	}
}

FString FOpenHarmonyPlatformProcess::GetGameBundleId()
{
	return TEXT("");
}

#if !PLATFORM_LUMIN
static bool EnableLittleCoreAffinity = false;
static int32 BigCoreMask = 0;
static int32 LittleCoreMask = 0;

void FOpenHarmonyPlatformProcess::SetAffinityInfo(bool bEnableAffinity, int32 bigCoreMask, int32 littleCoreMask)
{
	EnableLittleCoreAffinity = bEnableAffinity;
	BigCoreMask = bigCoreMask;
	LittleCoreMask = littleCoreMask;	
}

uint64 FOpenHarmonyAffinity::GetLittleCoreMask()
{
	static int Mask = 0;
	if (Mask == 0)
	{
		Mask = FGenericPlatformAffinity::GetNoAffinityMask();
		if (EnableLittleCoreAffinity)
		{
			Mask = LittleCoreMask;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LittleCore Affinity applying mask: 0x%0x"), Mask);
		}
	}
	return Mask;
}
#endif

// Can be specified per device profile
// openharmony.DefaultThreadAffinity GT 0x01 RT 0x02
TAutoConsoleVariable<FString> CVarOpenHarmonyDefaultThreadAffinity(
	TEXT("openharmony.DefaultThreadAffinity"),
	TEXT(""),
	TEXT("Sets the thread affinity for OpenHarmony platform. Pairs of args [GT|RT] [Hex affinity], ex: openharmony.DefaultThreadAffinity GT 0x01 RT 0x02"));

static void OpenHarmonySetAffinityOnThread()
{
	if (IsInActualRenderingThread()) // If RenderingThread is not started yet, affinity will applied at RT creation time 
	{
		FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetRenderingThreadMask());
	}
	else if (IsInGameThread())
	{
		FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetMainGameMask());
	}
}

static void ApplyDefaultThreadAffinity(IConsoleVariable* Var)
{
	FString AffinityCmd = CVarOpenHarmonyDefaultThreadAffinity.GetValueOnAnyThread();

	TArray<FString> Args;
	if (AffinityCmd.ParseIntoArrayWS(Args) > 0)
	{
		for (int32 Index = 0; Index + 1 < Args.Num(); Index += 2)
		{
			uint64 Aff = FParse::HexNumber(*Args[Index + 1]);
			if (!Aff)
			{
				UE_LOG(LogOpenHarmony, Display, TEXT("Parsed 0 for affinity, using 0xFFFFFFFFFFFFFFFF instead"));
				Aff = 0xFFFFFFFFFFFFFFFF;
			}
			if (Args[Index] == TEXT("GT"))
			{
				FOpenHarmonyAffinity::GameThreadMask = Aff;
			}
			else if (Args[Index] == TEXT("RT"))
			{
				FOpenHarmonyAffinity::RenderingThreadMask = Aff;
			}
		}

		if (FTaskGraphInterface::IsRunning())
		{
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OpenHarmonySetAffinityOnThread),
				TStatId(), NULL, ENamedThreads::GetRenderThread());

			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OpenHarmonySetAffinityOnThread),
				TStatId(), NULL, ENamedThreads::GameThread);
		}
		else
		{
			OpenHarmonySetAffinityOnThread();
		}
	}
}

void OpenHarmonySetupDefaultThreadAffinity()
{
	ApplyDefaultThreadAffinity(nullptr);

	// Watch for CVar update
	CVarOpenHarmonyDefaultThreadAffinity->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&ApplyDefaultThreadAffinity));
}
