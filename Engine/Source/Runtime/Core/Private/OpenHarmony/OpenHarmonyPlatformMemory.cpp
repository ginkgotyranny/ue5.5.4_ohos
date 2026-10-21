#include "OpenHarmony/OpenHarmonyPlatformMemory.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/MallocBinned.h"
#include "HAL/MallocBinned2.h"
#include "HAL/MallocBinned3.h"
#include "HAL/MallocAnsi.h"
#include "Misc/ScopeLock.h"
#include "unistd.h"
#include <sys/sysinfo.h>
#include <sys/mman.h>

#include "OpenHarmony/OpenHarmonyPlatformCrashContext.h"
#include "Async/TaskGraphInterfaces.h"
#include "Async/Async.h"

static int32 GOpenHarmonyAddSwapToTotalPhysical = 1;
static FAutoConsoleVariableRef CVarAddSwapToTotalPhysical(
	TEXT("openharmony.AddSwapToTotalPhysical"),
	GOpenHarmonyAddSwapToTotalPhysical,
	TEXT("When non zero, Total physical memory reporting will also include swap memory. (default)"),
	ECVF_Default
);

static int64 GetNativeHeapAllocatedSize()
{
	int64 AllocatedSize = 0;
	return AllocatedSize;
}

/** Controls growth of pools - see PooledVirtualMemoryAllocator.cpp */
extern float GVMAPoolScale;     //come from PooledVirtualMemoryAllocator.cpp line27

void FOpenHarmonyPlatformMemory::Init()
{
	// Only allow this method to be called once
	{
		static bool bInitDone = false;
		if (bInitDone)
			return;
		bInitDone = true;
	}

	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	FPlatformMemoryStats MemoryStats = GetStats();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.2fMB (%dGB approx) Available=%.2fMB PageSize=%.1fKB"), 
		float((double)MemoryConstants.TotalPhysical/1024.0/1024.0),
		MemoryConstants.TotalPhysicalGB, 
		float((double)MemoryStats.AvailablePhysical/1024.0/1024.0),
		float((double)MemoryConstants.PageSize/1024.0)
		);
	UE_LOG(LogInit, Log, TEXT(" - VirtualMemoryAllocator pools will grow at scale %g"), GVMAPoolScale);

}

extern void (*GMemoryWarningHandler)(const FGenericMemoryWarningContext& Context);

namespace OpenHarmonyPlatformMemory
{
	/**
	* @brief Returns value in bytes from a status line
	* @param Line in format "Blah:  10000 kB" - needs to be writable as it will modify it
	* @return value in bytes (10240000, i.e. 10000 * 1024 for the above example)
	*/
	uint64 GetBytesFromStatusLine(char * Line)
	{
		check(Line);
		int Len = strlen(Line);

		// Len should be long enough to hold at least " kB\n"
		const int kSuffixLength = 4;	// " kB\n"
		if (Len <= kSuffixLength)
		{
			return 0;
		}

		// let's check that this is indeed "kB"
		char * Suffix = &Line[Len - kSuffixLength];
		if (strcmp(Suffix, " kB\n") != 0)
		{
			// Linux the kernel changed the format, huh?
			return 0;
		}

		// kill the kB
		*Suffix = 0;

		// find the beginning of the number
		for (const char * NumberBegin = Suffix; NumberBegin >= Line; --NumberBegin)
		{
			if (*NumberBegin == ' ')
			{
				return static_cast< uint64 >(atol(NumberBegin + 1)) * 1024ULL;
			}
		}

		// we were unable to find whitespace in front of the number
		return 0;
	}

}

// useful for debugging, this triggers a traversal of smaps and can take 100s of ms.
static bool GetPss(uint64& PSSOUT, uint64& SwapPSSOUT)
{
	PSSOUT = 0;
	SwapPSSOUT = 0;
	int FieldsSetSuccessfully = 0;
	// again /proc "API" :/
	if (FILE* SmapsRollup = fopen("/proc/self/smaps_rollup", "r"))
	{
		do
		{
			char LineBuffer[256] = { 0 };
			char* Line = fgets(LineBuffer, UE_ARRAY_COUNT(LineBuffer), SmapsRollup);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}
			if (strstr(Line, "SwapPss:") == Line)
			{
				SwapPSSOUT = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "Pss:") == Line)
			{
				PSSOUT = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
		} while (FieldsSetSuccessfully < 2);
		fclose(SmapsRollup);
	}
	return FieldsSetSuccessfully == 2;
}

FPlatformMemoryStats FOpenHarmonyPlatformMemory::GetStats()
{
	FPlatformMemoryStats MemoryStats;	// will init from constants

										// open to all kind of overflows, thanks to Linux approach of exposing system stats via /proc and lack of proper C API
										// And no, sysinfo() isn't useful for this (cannot get the same value for MemAvailable through it for example).

	if (FILE* FileGlobalMemStats = fopen("/proc/meminfo", "r"))
	{
		int FieldsSetSuccessfully = 0;
		uint64 MemFree = 0, Cached = 0;
		do
		{
			char LineBuffer[256] = { 0 };
			char *Line = fgets(LineBuffer, UE_ARRAY_COUNT(LineBuffer), FileGlobalMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}

			// if we have MemAvailable, favor that (see http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=34e431b0ae398fc54ea69ff85ec700722c9da773)
			if (strstr(Line, "MemAvailable:") == Line)
			{
				MemoryStats.AvailablePhysical = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "SwapFree:") == Line)
			{
				MemoryStats.AvailableVirtual = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "MemFree:") == Line)
			{
				MemFree = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "Cached:") == Line)
			{
				Cached = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
		} while (FieldsSetSuccessfully < 4);

		// if we didn't have MemAvailable (kernels < 3.14 or CentOS 6.x), use free + cached as a (bad) approximation
		if (MemoryStats.AvailablePhysical == 0)
		{
			MemoryStats.AvailablePhysical = FMath::Min(MemFree + Cached, MemoryStats.TotalPhysical);
		}

		fclose(FileGlobalMemStats);
	}

	// again /proc "API" :/
	if (FILE* ProcMemStats = fopen("/proc/self/status", "r"))
	{
		int FieldsSetSuccessfully = 0;
		do
		{
			char LineBuffer[256] = { 0 };
			char *Line = fgets(LineBuffer, UE_ARRAY_COUNT(LineBuffer), ProcMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}

			if (strstr(Line, "VmPeak:") == Line)
			{
				MemoryStats.PeakUsedVirtual = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmSize:") == Line)
			{
				MemoryStats.UsedVirtual = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmHWM:") == Line)
			{
				MemoryStats.PeakUsedPhysical = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmRSS:") == Line)
			{
				MemoryStats.UsedPhysical = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				MemoryStats.VMRss = MemoryStats.UsedPhysical;
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmSwap:") == Line)
			{
				MemoryStats.VMSwap = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
		} while (FieldsSetSuccessfully < 5);

		fclose(ProcMemStats);
	}

	if (GOpenHarmonyAddSwapToTotalPhysical)
	{
		MemoryStats.UsedPhysical += MemoryStats.VMSwap;
	}


	// sanitize stats as sometimes peak < used for some reason
	MemoryStats.PeakUsedVirtual = FMath::Max(MemoryStats.PeakUsedVirtual, MemoryStats.UsedVirtual);
	MemoryStats.PeakUsedPhysical = FMath::Max(MemoryStats.PeakUsedPhysical, MemoryStats.UsedPhysical);



	return MemoryStats;
}

uint64 FOpenHarmonyPlatformMemory::GetMemoryUsedFast()
{


	uint64 VMRSS = 0;
	uint64 VMSwap = 0;
	// minimal code to get Used memory
	if (FILE* ProcMemStats = fopen("/proc/self/status", "r"))
	{
		int LinesToFind = GOpenHarmonyAddSwapToTotalPhysical ? 2 : 1;
		while (1)
		{
			char LineBuffer[256] = { 0 };
			char *Line = fgets(LineBuffer, UE_ARRAY_COUNT(LineBuffer), ProcMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}
			else if (strstr(Line, "VmRSS:") == Line)
			{
				VMRSS = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				LinesToFind--;
			}
			else if (GOpenHarmonyAddSwapToTotalPhysical && strstr(Line, "VmSwap:") == Line)
			{
				VMSwap = OpenHarmonyPlatformMemory::GetBytesFromStatusLine(Line);
				LinesToFind--;
			}

			if (LinesToFind == 0)
			{
				break;
			}
		} 
		fclose(ProcMemStats);
	}

	return VMRSS + VMSwap;
}


const FPlatformMemoryConstants& FOpenHarmonyPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if (MemoryConstants.TotalPhysical == 0)
	{
		// Gather platform memory stats.
		struct sysinfo SysInfo;
		unsigned long long MaxPhysicalRAMBytes = 0;
		unsigned long long MaxVirtualRAMBytes = 0;

		if (0 == sysinfo(&SysInfo))
		{
			MaxPhysicalRAMBytes = static_cast< unsigned long long >(SysInfo.mem_unit) * static_cast< unsigned long long >(SysInfo.totalram);
			MaxVirtualRAMBytes = static_cast< unsigned long long >(SysInfo.mem_unit) * static_cast< unsigned long long >(SysInfo.totalswap);
		}

		MemoryConstants.TotalPhysical = MaxPhysicalRAMBytes;
		MemoryConstants.TotalVirtual = MaxVirtualRAMBytes;
		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
		MemoryConstants.PageSize = sysconf(_SC_PAGESIZE);
		MemoryConstants.BinnedPageSize = FMath::Max((SIZE_T)65536, MemoryConstants.PageSize);
		MemoryConstants.BinnedAllocationGranularity = MemoryConstants.PageSize;
		MemoryConstants.OsAllocationGranularity = MemoryConstants.PageSize;
#if PLATFORM_32BITS
		MemoryConstants.AddressLimit = DECLARE_UINT64(4) * 1024 * 1024 * 1024;
#else
		MemoryConstants.AddressLimit = FPlatformMath::RoundUpToPowerOfTwo64(MemoryConstants.TotalPhysical);
#endif
	}

	return MemoryConstants;
}

EPlatformMemorySizeBucket FOpenHarmonyPlatformMemory::GetMemorySizeBucket()
{
	// @todo openharmony - if running without the extensions for texture streaming, we will load all of the textures
	// so we better look like a low memory device
	return FGenericPlatformMemory::GetMemorySizeBucket();
}

// Set rather to use BinnedMalloc2 for binned malloc, can be overridden below
#define USE_MALLOC_BINNED2 PLATFORM_OPENHARMONY_ARM64
#if !defined(USE_MALLOC_BINNED3)
	#define USE_MALLOC_BINNED3 (0)
#endif

FMalloc* FOpenHarmonyPlatformMemory::BaseAllocator()
{
	static FMalloc* Instance = nullptr;
	if (Instance != nullptr)
	{
		return Instance;
	}

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	// make sure LLM is using UsedPhysical for program size, instead of Available-Free
	FPlatformMemoryStats Stats = FOpenHarmonyPlatformMemory::GetStats();
	FLowLevelMemTracker::Get().SetProgramSize(Stats.UsedPhysical);
#endif

#if FORCE_ANSI_ALLOCATOR
	return (Instance = new FMallocAnsi());
#endif

#if USE_MALLOC_BINNED3 && PLATFORM_OPENHARMONY_ARM64
	return (Instance = new FMallocBinned3());
#elif USE_MALLOC_BINNED2
	return (Instance = new FMallocBinned2());
#else
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	// 1 << FMath::CeilLogTwo(MemoryConstants.TotalPhysical) should really be FMath::RoundUpToPowerOfTwo,
	// but that overflows to 0 when MemoryConstants.TotalPhysical is close to 4GB, since CeilLogTwo returns 32
	// this then causes the MemoryLimit to be 0 and crashing the app
	uint64 MemoryLimit = FMath::Min<uint64>(uint64(1) << FMath::CeilLogTwo(MemoryConstants.TotalPhysical), 0x100000000);

	// todo: Verify MallocBinned2 on 32bit
	// [RCL] 2017-03-06 FIXME: perhaps BinnedPageSize should be used here, but leaving this change to the openharmony platform owner.
	return (Instance = new FMallocBinned(MemoryConstants.PageSize, MemoryLimit));
#endif
}


void* FOpenHarmonyPlatformMemory::BinnedAllocFromOS(SIZE_T Size)
{
	void* Ptr;
	if (USE_MALLOC_BINNED2)
	{
		static FCriticalSection CriticalSection;
		FScopeLock Lock(&CriticalSection);

		const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();

		static uint8* Base = nullptr;
		static uint8* End = nullptr;

		static const SIZE_T MinAllocSize = 4 * 1024 * 1024; // we will allocate chunks of 4MB, that means the amount we will need to unmap, assuming a lot of 64k blocks, will be small.

		if (End - Base < Size)
		{
			if (Base)
			{
				if (Base < End)
				{
					if (munmap(Base, End - Base) != 0)
					{
						const int ErrNo = errno;
						UE_LOG(LogHAL, Fatal, TEXT("munmap (for trim) (addr=%p, len=%llu) failed with errno = %d (%s)"), Base, End - Base,
							ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
					}
				}
				Base = nullptr;
				End = nullptr;
			}

			SIZE_T SizeToAlloc = FMath::Max<SIZE_T>(MinAllocSize, Align(Size, MemoryConstants.PageSize) + MemoryConstants.BinnedPageSize);
			uint8* UnalignedBase = (uint8*)mmap(nullptr, SizeToAlloc, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
			End = UnalignedBase + SizeToAlloc;
			Base = Align(UnalignedBase, MemoryConstants.BinnedPageSize);

			if (Base > UnalignedBase)
			{
				if (munmap(UnalignedBase, Base - UnalignedBase) != 0)
				{
					const int ErrNo = errno;
					UE_LOG(LogHAL, Fatal, TEXT("munmap (for align) (addr=%p, len=%llu) failed with errno = %d (%s)"), UnalignedBase, Base - UnalignedBase,
						ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
				}
			}


		}
		Ptr = Base;
		uint8* UnalignedBase = Align(Base + Size, MemoryConstants.PageSize);
		Base = Align(UnalignedBase, MemoryConstants.BinnedPageSize);

		if (Base > End)
		{
			Base = End;
		}

		if (Base > UnalignedBase)
		{
			if (munmap(UnalignedBase, Base - UnalignedBase) != 0)
			{
				const int ErrNo = errno;
				UE_LOG(LogHAL, Fatal, TEXT("munmap (for tail align) (addr=%p, len=%llu) failed with errno = %d (%s)"), UnalignedBase, Base - UnalignedBase,
					ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
			}
		}
	}
	else
	{
		Ptr = mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	}

	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Ptr, Size));
	return Ptr;
}

void FOpenHarmonyPlatformMemory::BinnedFreeToOS(void* Ptr, SIZE_T Size)
{
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));
	if (munmap(Ptr, Size) != 0)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Ptr, Size,
			ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
	}
}

size_t FOpenHarmonyPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment()
{
	static SIZE_T OSPageSize = FPlatformMemory::GetConstants().PageSize;
	return OSPageSize;
}

size_t FOpenHarmonyPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment()
{
	static SIZE_T OSPageSize = FPlatformMemory::GetConstants().PageSize;
	return OSPageSize;
}

FOpenHarmonyPlatformMemory::FPlatformVirtualMemoryBlock FOpenHarmonyPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(size_t InSize, size_t InAlignment)
{
	FPlatformVirtualMemoryBlock Result;
	InSize = Align(InSize, GetVirtualSizeAlignment());
	Result.VMSizeDivVirtualSizeAlignment = InSize / GetVirtualSizeAlignment();

	size_t Alignment = FMath::Max(InAlignment, GetVirtualSizeAlignment());
	check(Alignment <= GetVirtualSizeAlignment());

	Result.Ptr = mmap(nullptr, Result.GetActualSize(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (!LIKELY(Result.Ptr != MAP_FAILED))
	{
		FPlatformMemory::OnOutOfMemory(Result.GetActualSize(), InAlignment);
	}
	check(Result.Ptr && IsAligned(Result.Ptr, Alignment));
	return Result;
}



void FOpenHarmonyPlatformMemory::FPlatformVirtualMemoryBlock::FreeVirtual()
{
	if (Ptr)
	{
		check(VMSizeDivVirtualSizeAlignment > 0);
		if (munmap(Ptr, GetActualSize()) != 0)
		{
			// we can ran out of VMAs here
			FPlatformMemory::OnOutOfMemory(GetActualSize(), 0);
			// unreachable
		}
		Ptr = nullptr;
		VMSizeDivVirtualSizeAlignment = 0;
	}
}

void FOpenHarmonyPlatformMemory::FPlatformVirtualMemoryBlock::Commit(size_t InOffset, size_t InSize)
{
	check(IsAligned(InOffset, GetCommitAlignment()) && IsAligned(InSize, GetCommitAlignment()));
	check(InOffset >= 0 && InSize >= 0 && InOffset + InSize <= GetActualSize() && Ptr);
}

void FOpenHarmonyPlatformMemory::FPlatformVirtualMemoryBlock::Decommit(size_t InOffset, size_t InSize)
{
	check(IsAligned(InOffset, GetCommitAlignment()) && IsAligned(InSize, GetCommitAlignment()));
	check(InOffset >= 0 && InSize >= 0 && InOffset + InSize <= GetActualSize() && Ptr);
	if (madvise(((uint8*)Ptr) + InOffset, InSize, MADV_DONTNEED) != 0)
	{
		// we can ran out of VMAs here too!
		FPlatformMemory::OnOutOfMemory(InSize, 0);
	}
}


/**
* LLM uses these low level functions (LLMAlloc and LLMFree) to allocate memory. It grabs
* the function pointers by calling FPlatformMemory::GetLLMAllocFunctions. If these functions
* are not implemented GetLLMAllocFunctions should return false and LLM will be disabled.
*/

#if ENABLE_LOW_LEVEL_MEM_TRACKER

int64 LLMMallocTotal = 0;

void* LLMAlloc(size_t Size)
{
	void* Ptr = mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

	LLMMallocTotal += Size;

	return Ptr;
}

void LLMFree(void* Addr, size_t Size)
{
	LLMMallocTotal -= Size;
	if (Addr != nullptr && munmap(Addr, Size) != 0)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Addr, Size,
			ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
	}
}

#endif

bool FOpenHarmonyPlatformMemory::GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment)
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	OutAllocFunction = LLMAlloc;
	OutFreeFunction = LLMFree;
	OutAlignment = sysconf(_SC_PAGESIZE);
	return true;
#else
	return false;
#endif
}