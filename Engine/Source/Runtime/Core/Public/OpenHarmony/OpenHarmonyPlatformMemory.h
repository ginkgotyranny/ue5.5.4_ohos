#pragma once
#include "GenericPlatform/GenericPlatformMemory.h"

/**
 *	 OpenHarmony implementation of the FGenericPlatformMemoryStats.
 */
struct FPlatformMemoryStats : public FGenericPlatformMemoryStats
{
	uint64 VMSwap = 0;
	uint64 VMRss = 0;
};

/**
* OpenHarmony implementation of the memory OS functions
**/
struct FOpenHarmonyPlatformMemory : public FGenericPlatformMemory
{
	//~ Begin FGenericPlatformMemory Interface
	static CORE_API void Init();
	static CORE_API FPlatformMemoryStats GetStats();
	static CORE_API uint64 GetMemoryUsedFast();
	static CORE_API const FPlatformMemoryConstants& GetConstants();
	static CORE_API EPlatformMemorySizeBucket GetMemorySizeBucket();
	static CORE_API FMalloc* BaseAllocator();
	static CORE_API void* BinnedAllocFromOS(SIZE_T Size);
	static CORE_API void BinnedFreeToOS(void* Ptr, SIZE_T Size);

	CORE_API struct FMemAdviceStats
	{
		bool IsValid() const { return MemFree > 0 && MemUsed > 0; };
		int64 TotalMem = 0; // total memory available to the device
		int64 MemFree = 0;
		int64 MemUsed = 0;  // mem used across the device, (simply returns total - free)
	};

	class FPlatformVirtualMemoryBlock : public FBasicVirtualMemoryBlock
	{
	public:

		FPlatformVirtualMemoryBlock()
		{
		}

		FPlatformVirtualMemoryBlock(void *InPtr, uint32 InVMSizeDivVirtualSizeAlignment)
			: FBasicVirtualMemoryBlock(InPtr, InVMSizeDivVirtualSizeAlignment)
		{
		}
		FPlatformVirtualMemoryBlock(const FPlatformVirtualMemoryBlock& Other) = default;
		FPlatformVirtualMemoryBlock& operator=(const FPlatformVirtualMemoryBlock& Other) = default;

		void Commit(size_t InOffset, size_t InSize);        // Map to physical memory
		void Decommit(size_t InOffset, size_t InSize);
		void FreeVirtual();

		FORCEINLINE void CommitByPtr(void *InPtr, size_t InSize)
		{
			Commit(size_t(((uint8*)InPtr) - ((uint8*)Ptr)), InSize);
		}

		FORCEINLINE void DecommitByPtr(void *InPtr, size_t InSize)
		{
			Decommit(size_t(((uint8*)InPtr) - ((uint8*)Ptr)), InSize);
		}

		FORCEINLINE void Commit()
		{
			Commit(0, GetActualSize());
		}

		FORCEINLINE void Decommit()
		{
			Decommit(0, GetActualSize());
		}

		FORCEINLINE size_t GetActualSize() const
		{
			return VMSizeDivVirtualSizeAlignment * GetVirtualSizeAlignment();
		}

		static FPlatformVirtualMemoryBlock AllocateVirtual(size_t Size, size_t InAlignment = FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment());
		static size_t GetCommitAlignment();
		static size_t GetVirtualSizeAlignment();
	};

	static CORE_API bool GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment);
	//~ End FGenericPlatformMemory Interface
	
};

typedef FOpenHarmonyPlatformMemory FPlatformMemory;



