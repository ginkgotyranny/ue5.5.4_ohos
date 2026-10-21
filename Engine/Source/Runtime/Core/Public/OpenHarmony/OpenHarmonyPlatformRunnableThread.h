// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Runtime/Core/Private/HAL/PThreadRunnableThread.h"
#include "OpenHarmony/OpenHarmonyPlatformMisc.h"

/**
* OpenHarmony implementation of the pthread functions
**/
class FRunnableThreadOpenHarmony : public FRunnableThreadPThread
{
	enum
	{
		OpenHarmonyThreadNameLimit = 15
	};

public:
    FRunnableThreadOpenHarmony() : FRunnableThreadPThread()
    {
    }

	~FRunnableThreadOpenHarmony()
	{
		// Call the parent destructor body before the parent does it - see comment on that function for explanation why.
		FRunnableThreadPThread_DestructorBody();
	}

private:

	/**
	 * Allows a platform subclass to setup anything needed on the thread before running the Run function
	 */
	virtual void PreRun()
	{
		FString SizeLimitedThreadName = ThreadName;

		if (SizeLimitedThreadName.Len() > OpenHarmonyThreadNameLimit)
		{
			// first, attempt to cut out common and meaningless substrings
			SizeLimitedThreadName = SizeLimitedThreadName.Replace(TEXT("Thread"), TEXT(""));
			SizeLimitedThreadName = SizeLimitedThreadName.Replace(TEXT("Runnable"), TEXT(""));

			// if still larger
			if (SizeLimitedThreadName.Len() > OpenHarmonyThreadNameLimit)
			{
				FString Temp = SizeLimitedThreadName;

				// cut out the middle and replace with a substitute
				const TCHAR Dash[] = TEXT("-");
				const int32 DashLen = UE_ARRAY_COUNT(Dash) - 1;
				int NumToLeave = (OpenHarmonyThreadNameLimit - DashLen) / 2;

				SizeLimitedThreadName = Temp.Left(OpenHarmonyThreadNameLimit - (NumToLeave + DashLen));
				SizeLimitedThreadName += Dash;
				SizeLimitedThreadName += Temp.Right(NumToLeave);

				check(SizeLimitedThreadName.Len() <= OpenHarmonyThreadNameLimit);
			}
		}

		FOpenHarmonyMisc::SetThreadName(TCHAR_TO_ANSI(*SizeLimitedThreadName));
	}

	/**
	 * Allows platforms to adjust stack size
	 */
	virtual uint32 AdjustStackSize(uint32 InStackSize)
	{
		InStackSize = FRunnableThreadPThread::AdjustStackSize(InStackSize);

		// If it's set, make sure it's at least 128 KB or stack allocations (e.g. in Logf) may fail
		if (InStackSize && InStackSize < 128 * 1024)
		{
			InStackSize = 128 * 1024;
		}

		return InStackSize;
	}
};
