// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyOpenGLFramePacer.h"

#if USE_OPENHARMONY_OPENGL

/*******************************************************************
 * FOpenHarmonyOpenGLFramePacer implementation
 *******************************************************************/

#include "OpenGLDrvPrivate.h"
#include "Math/UnrealMathUtility.h"
#if 0 //USE_OPENHARMONY_OPENGL_SWAPPY
// ignore jni
// #include "OpenHarmony/OpenHarmonyJNI.h"
#include "OpenHarmony/OpenHarmonyApplication.h"
// #include "swappyGL.h"
// #include "swappyGL_extra.h"
// #include "swappy_common.h"
// #include "HAL/Thread.h"
// #include "Misc/ScopeRWLock.h"
#include "EngineGlobals.h"

#endif

#include "OpenHarmonyEGL.h"
#include <EGL/egl.h>

#define USE_VSYNC 1

extern float GetNativeDisplayRefreshRate();

void FOpenHarmonyOpenGLFramePacer::Init()
{
	bSwappyInit = false;
#if 0 //USE_OPENHARMONY_OPENGL_SWAPPY
	// TODO: implement this in OpenHarmonyPlatformFramePacer
	// if (FOpenHarmonyPlatformRHIFramePacer::CVarUseSwappyForFramePacing.GetValueOnAnyThread() == 1)
	// {
	// 	// initialize now if set on startup
	// 	InitSwappy();
	// }
	// else
	// {
	// 	// initialize later if set by console
	// 	FOpenHarmonyPlatformRHIFramePacer::CVarUseSwappyForFramePacing.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* Variable)
	// 	{
	// 		if (Variable->GetInt() == 1)
	// 		{
	// 			InitSwappy();
	// 		}
	// 	}));
	// }
#endif
}

#if 0 //USE_OPENHARMONY_OPENGL_SWAPPY
void FOpenHarmonyOpenGLFramePacer::InitSwappy()
{
	// if (!bSwappyInit)
	// {
	// 	// initialize Swappy
	// TODO: implement this in OpenHarmonyApplication
	// 	JNIEnv* Env = FOpenHarmonyApplication::GetJavaEnv();
	// 	if (ensure(Env))
	// 	{
	// 		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Init Swappy"));
	// 		SwappyGL_init(Env, FJavaWrapper::GameActivityThis);
	// 	}
	// 	bSwappyInit = true;
	// }
}
#endif

FOpenHarmonyOpenGLFramePacer::~FOpenHarmonyOpenGLFramePacer()
{
#if 0 //USE_OPENHARMONY_OPENGL_SWAPPY
    // TODO: implement this in OpenHarmonyPlatformFramePacer
	// FOpenHarmonyPlatformRHIFramePacer::CVarUseSwappyForFramePacing.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate());
	// if (bSwappyInit)
	// {
	// 	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Shutdown Swappy"));
	// 	SwappyGL_destroy();
	// 	bSwappyInit = false;
	// }
#endif
}

static bool GGetTimeStampsSucceededThisFrame = true;
static uint32 GGetTimeStampsRetryCount = 0;

static bool CanUseGetFrameTimestamps()
{
	return FOpenHarmonyPlatformRHIFramePacer::CVarUseGetFrameTimestamps.GetValueOnAnyThread()
		&& eglGetFrameTimestampsOPENHARMONY_p
		&& eglGetNextFrameIdOPENHARMONY_p
		&& eglPresentationTimeOPENHARMONY_p
		&& (GGetTimeStampsRetryCount < FOpenHarmonyPlatformRHIFramePacer::CVarTimeStampErrorRetryCount.GetValueOnAnyThread());
}
static bool CanUseGetFrameTimestampsForThisFrame()
{
	return CanUseGetFrameTimestamps() && GGetTimeStampsSucceededThisFrame;
}

bool ShouldUseGPUFencesToLimitLatency()
{
	if (CanUseGetFrameTimestampsForThisFrame())
	{
		return true; // this method requires a GPU fence to give steady results
	}
	return FOpenHarmonyPlatformRHIFramePacer::CVarDisableOpenGLGPUSync.GetValueOnAnyThread() == 0; // otherwise just based on the FOpenHarmonyPlatformRHIFramePacer::CVar; thought to be bad to use GPU fences on PowerVR
}

static uint32 NextFrameIDSlot = 0;
#define NUM_FRAMES_TO_MONITOR (4)
static EGLuint64KHR FrameIDs[NUM_FRAMES_TO_MONITOR] = { 0 };

static int32 RecordedFrameInterval[100];
static int32 NumRecordedFrameInterval = 0;

// ignore jni
// extern float OpenHarmonyThunkCpp_GetMetaDataFloat(const FString& Key);

bool FOpenHarmonyOpenGLFramePacer::SupportsFramePaceInternal(int32 QueryFramePace, int32& OutRefreshRate, int32& OutSyncInterval)
{
#if 0 //USE_OPENHARMONY_OPENGL_SWAPPY
	// TODO: implement this in OpenHarmonyPlaformFramePacer
	// if (FOpenHarmonyPlatformRHIFramePacer::CVarUseSwappyForFramePacing.GetValueOnAnyThread() == 1)
	// {
	// 	int NumRates = Swappy_getSupportedRefreshRates(nullptr, 0);
	// 	TArray<uint64> RefreshRatesNs;
	// 	RefreshRatesNs.AddZeroed(NumRates);
	// 	Swappy_getSupportedRefreshRates((uint64_t*)RefreshRatesNs.GetData(), NumRates);
	// 	TArray<int32> RefreshRates;
	// 	RefreshRates.Empty(NumRates);
	// 	FString DebugString = TEXT("Supported Refresh Rates:");
	// 	for (uint64 RateNs : RefreshRatesNs)
	// 	{
	// 		if (RateNs > 0)
	// 		{
	// 			int32 RefreshRate = FMath::DivideAndRoundNearest(1000000000ull, RateNs);
	// 			RefreshRates.Add(RefreshRate);
	// 			DebugString += FString::Printf(TEXT(" %d (%ld ns)"), RefreshRate, RateNs);
	// 		}
	// 	}
	// 	UE_LOG(LogRHI, Log, TEXT("%s"), *DebugString);

	// 	for (int32 Rate : RefreshRates)
	// 	{
	// 		if ((Rate % QueryFramePace) == 0)
	// 		{
	// 			UE_LOG(LogRHI, Log, TEXT("Supports %d using refresh rate %d and sync interval %d"), QueryFramePace, Rate, Rate / QueryFramePace);
	// 			OutRefreshRate = Rate;
	// 			OutSyncInterval = Rate / QueryFramePace;
	// 			return true;
	// 		}
	// 	}

	// 	// check if we want to use naive frame pacing at less than a multiple of supported refresh rate
	// 	if (FOpenHarmonyPlatformRHIFramePacer::CVarSupportNonVSyncMultipleFrameRates.GetValueOnAnyThread() == 1)
	// 	{
	// 		RefreshRates.Sort();
	// 		for (int32 Rate : RefreshRates)
	// 		{
	// 			if (Rate > QueryFramePace)
	// 			{
	// 				UE_LOG(LogRHI, Log, TEXT("Supports %d using refresh rate %d with naive frame pacing"), QueryFramePace, Rate);
	// 				OutRefreshRate = Rate;
	// 				OutSyncInterval = 0;
	// 				return true;
	// 			}
	// 		}
	// 	}
	// }
#endif
	OutRefreshRate = QueryFramePace;
	OutSyncInterval = 0;
	return FGenericPlatformRHIFramePacer::SupportsFramePace(QueryFramePace);
}

bool FOpenHarmonyOpenGLFramePacer::SupportsFramePace(int32 QueryFramePace)
{
	int32 TempRefreshRate, TempSyncInterval;
	return SupportsFramePaceInternal(QueryFramePace, TempRefreshRate, TempSyncInterval);
}

bool FOpenHarmonyOpenGLFramePacer::SwapBuffers(bool bLockToVsync)
{
#if !UE_BUILD_SHIPPING
	if (FOpenHarmonyPlatformRHIFramePacer::CVarStallSwap.GetValueOnAnyThread() > 0.0f)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Swap_Intentional_Stall);
		FPlatformProcess::Sleep(FOpenHarmonyPlatformRHIFramePacer::CVarStallSwap.GetValueOnRenderThread() / 1000.0f);
	}
#endif

	VERIFY_EGL_SCOPE();

	EGLDisplay eglDisplay = OpenHarmonyEGL::GetInstance()->GetDisplay();
	EGLSurface eglSurface = OpenHarmonyEGL::GetInstance()->GetSurface();
	int32 SyncInterval = FOpenHarmonyPlatformRHIFramePacer::GetLegacySyncInterval();

	bool bPrintMethod = false;

#if 0 //USE_OPENHARMONY_OPENGL_SWAPPY
	/* int32 CurrentFramePace = 0;
	if (FAndroidPlatformRHIFramePacer::CVarUseSwappyForFramePacing.GetValueOnRenderThread() != 0 && (CurrentFramePace = FAndroidPlatformRHIFramePacer::GetFramePace()) != 0 && ensure(bSwappyInit))
	{
		ANativeWindow* CurrentNativeWindow = AndroidEGL::GetInstance()->GetNativeWindow();
		if (CurrentNativeWindow != CachedNativeWindow)
		{
			CachedNativeWindow = CurrentNativeWindow;
			UE_LOG(LogRHI, Verbose, TEXT("Swappy - setting native window %p"), CachedNativeWindow);
			SwappyGL_setWindow(CurrentNativeWindow);
		}
		
		// cache refresh rate and sync interval
		if (CurrentFramePace != CachedFramePace)
		{
			CachedFramePace = CurrentFramePace;
			SupportsFramePaceInternal(CurrentFramePace, CachedRefreshRate, CachedSyncInterval);
			SwappyGL_resetFramePacing();
		}

		SwappyGL_setAutoSwapInterval(false);
		if (CachedSyncInterval != 0)
		{
			// Multiple of sync interval, use swappy directly
			UE_LOG(LogRHI, Verbose, TEXT("Setting swappy to interval of %" PRId64 " (%d fps)"), (int64)(1000000000L) / (int64)CurrentFramePace, CurrentFramePace);
			SwappyGL_setSwapIntervalNS((1000000000L) / (int64)CurrentFramePace);
		}
		else
		{
			// Unsupported frame rate. Set to higher refresh rate
			SwappyGL_setSwapIntervalNS((1000000000L) / (int64)CachedRefreshRate);

			// use naive frame pacing to limit the frame rate
			float MinTimeBetweenFrames = (1.f / CurrentFramePace);
			float ThisTime = FPlatformTime::Seconds() - LastTimeEmulatedSync;
			if (ThisTime > 0 && ThisTime < MinTimeBetweenFrames)
			{
				FPlatformProcess::Sleep(MinTimeBetweenFrames - ThisTime);
			}
		}

		SwappyGL_swap(eglDisplay, eglSurface);
		LastTimeEmulatedSync = FPlatformTime::Seconds();
	}
	else */
#endif
	{
#if USE_VSYNC
		if (DesiredSyncIntervalRelativeTo60Hz != SyncInterval)
		{
			GGetTimeStampsRetryCount = 0;

			bPrintMethod = true;
			DesiredSyncIntervalRelativeTo60Hz = SyncInterval;
			DriverRefreshRate = 60.0f;
			DriverRefreshNanos = 16666666;


			EGLnsecsOPENHARMONY EGL_COMPOSITE_DEADLINE_OPENHARMONY_Value = -1;
			EGLnsecsOPENHARMONY EGL_COMPOSITE_INTERVAL_OPENHARMONY_Value = -1;
			EGLnsecsOPENHARMONY EGL_COMPOSITE_TO_PRESENT_LATENCY_OPENHARMONY_Value = -1;

			if (eglGetCompositorTimingOPENHARMONY_p)
			{
				{
					EGLint Item = EGL_COMPOSITE_DEADLINE_OPENHARMONY;
					if (!eglGetCompositorTimingOPENHARMONY_p(eglDisplay, eglSurface, 1, &Item, &EGL_COMPOSITE_DEADLINE_OPENHARMONY_Value))
					{
						EGL_COMPOSITE_DEADLINE_OPENHARMONY_Value = -1;
					}
				}
				{
					EGLint Item = EGL_COMPOSITE_INTERVAL_OPENHARMONY;
					if (!eglGetCompositorTimingOPENHARMONY_p(eglDisplay, eglSurface, 1, &Item, &EGL_COMPOSITE_INTERVAL_OPENHARMONY_Value))
					{
						EGL_COMPOSITE_INTERVAL_OPENHARMONY_Value = -1;
					}
				}
				{
					EGLint Item = EGL_COMPOSITE_TO_PRESENT_LATENCY_OPENHARMONY;
					if (!eglGetCompositorTimingOPENHARMONY_p(eglDisplay, eglSurface, 1, &Item, &EGL_COMPOSITE_TO_PRESENT_LATENCY_OPENHARMONY_Value))
					{
						EGL_COMPOSITE_TO_PRESENT_LATENCY_OPENHARMONY_Value = -1;
					}
				}
				UE_LOG(LogRHI, Log, TEXT("OpenHarmonyEGL:SwapBuffers eglGetCompositorTimingOPENHARMONY EGL_COMPOSITE_DEADLINE_OPENHARMONY=%lld, EGL_COMPOSITE_INTERVAL_OPENHARMONY=%lld, EGL_COMPOSITE_TO_PRESENT_LATENCY_OPENHARMONY=%lld"),
					EGL_COMPOSITE_DEADLINE_OPENHARMONY_Value,
					EGL_COMPOSITE_INTERVAL_OPENHARMONY_Value,
					EGL_COMPOSITE_TO_PRESENT_LATENCY_OPENHARMONY_Value
				);
			}

			// ignore jni
			// float RefreshRate = AndroidThunkCpp_GetMetaDataFloat(TEXT("unreal.display.getRefreshRate"));
			float RefreshRate = GetNativeDisplayRefreshRate();

			UE_LOG(LogRHI, Log, TEXT("NAPI Display getRefreshRate=%f"),
				RefreshRate
			);

			if (EGL_COMPOSITE_INTERVAL_OPENHARMONY_Value >= 4000000 && EGL_COMPOSITE_INTERVAL_OPENHARMONY_Value <= 41666666)
			{
				DriverRefreshRate = float(1000000000.0 / double(EGL_COMPOSITE_INTERVAL_OPENHARMONY_Value));
				DriverRefreshNanos = EGL_COMPOSITE_INTERVAL_OPENHARMONY_Value;
			}
			else if (RefreshRate >= 24.0f && RefreshRate <= 250.0f)
			{
				DriverRefreshRate = RefreshRate;
				DriverRefreshNanos = int64(0.5 + 1000000000.0 / double(RefreshRate));
			}

			UE_LOG(LogRHI, Log, TEXT("Final display timing metrics: DriverRefreshRate=%7.4f  DriverRefreshNanos=%lld"),
				DriverRefreshRate,
				DriverRefreshNanos
			);

			// make sure requested interval is in supported range
			EGLint MinSwapInterval, MaxSwapInterval;
			OpenHarmonyEGL::GetInstance()->GetSwapIntervalRange(MinSwapInterval, MaxSwapInterval);

			int64 SyncIntervalNanos = (30 + 1000000000l * int64(SyncInterval)) / 60;

			int32 UnderDriverInterval = int32(SyncIntervalNanos / DriverRefreshNanos);
			int32 OverDriverInterval = UnderDriverInterval + 1;

			int64 UnderNanos = int64(UnderDriverInterval) * DriverRefreshNanos;
			int64 OverNanos = int64(OverDriverInterval) * DriverRefreshNanos;

			DesiredSyncIntervalRelativeToDevice = (FMath::Abs(SyncIntervalNanos - UnderNanos) < FMath::Abs(SyncIntervalNanos - OverNanos)) ?
				UnderDriverInterval : OverDriverInterval;

			int32 DesiredDriverSyncInterval = FMath::Clamp<int32>(DesiredSyncIntervalRelativeToDevice, MinSwapInterval, MaxSwapInterval);

			UE_LOG(LogRHI, Log, TEXT("OpenHarmonyEGL:SwapBuffers Min=%d, Max=%d, Request=%d, ClosestDriver=%d, SetDriver=%d"), MinSwapInterval, MaxSwapInterval, DesiredSyncIntervalRelativeTo60Hz, DesiredSyncIntervalRelativeToDevice, DesiredDriverSyncInterval);

			if (DesiredDriverSyncInterval != DriverSyncIntervalRelativeToDevice)
			{
				DriverSyncIntervalRelativeToDevice = DesiredDriverSyncInterval;
				UE_LOG(LogRHI, Log, TEXT("Called eglSwapInterval %d"), DesiredDriverSyncInterval);
				eglSwapInterval(eglDisplay, DriverSyncIntervalRelativeToDevice);
			}
		}

		if (DesiredSyncIntervalRelativeToDevice > DriverSyncIntervalRelativeToDevice)
		{
			{
				UE_CLOG(bPrintMethod, LogRHI, Display, TEXT("Using niave method for frame pacing (possible with timestamps method)"));
				if (LastTimeEmulatedSync > 0.0)
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_StallForEmulatedSyncInterval);
					float MinTimeBetweenFrames = (float(DesiredSyncIntervalRelativeToDevice) / DriverRefreshRate);

					for (;;)
					{
						float ThisTime = FPlatformTime::Seconds() - LastTimeEmulatedSync;
						// sleep only when there is substantial time left to a next sync interval
						// for a small duration rely on eglSwapBuffers
						if (ThisTime > 0.001f && ThisTime < MinTimeBetweenFrames)
						{
							// do not sleep for too long, poll occlussion queries from time to time as RT might be waiting for them
							float SleepDuration = FMath::Min(MinTimeBetweenFrames - ThisTime, 0.003f);
							FPlatformProcess::Sleep(SleepDuration);
						}
						else
						{
							break;
						}
					}
				}
			}
		}
		if (CanUseGetFrameTimestamps())
		{
			UE_CLOG(bPrintMethod, LogRHI, Display, TEXT("Using eglGetFrameTimestampsOPENHARMONY method for frame pacing"));

			//static bool bPrintOnce = true;
			if (FrameIDs[(int32(NextFrameIDSlot) - 1) % NUM_FRAMES_TO_MONITOR])
				// not supported   && eglGetFrameTimestampsSupportedOPENHARMONY_p && eglGetFrameTimestampsSupportedOPENHARMONY_p(eglDisplay, eglSurface, EGL_FIRST_COMPOSITION_START_TIME_OPENHARMONY))
			{
				//UE_CLOG(bPrintOnce, LogRHI, Log, TEXT("eglGetFrameTimestampsSupportedOPENHARMONY retured true for EGL_FIRST_COMPOSITION_START_TIME_OPENHARMONY"));
				EGLint TimestampList = EGL_FIRST_COMPOSITION_START_TIME_OPENHARMONY;
				//EGLint TimestampList = EGL_COMPOSITION_LATCH_TIME_OPENHARMONY;
				//EGLint TimestampList = EGL_LAST_COMPOSITION_START_TIME_OPENHARMONY;
				//EGLint TimestampList = EGL_DISPLAY_PRESENT_TIME_OPENHARMONY;
				EGLnsecsOPENHARMONY Result = 0;
				int32 DeltaFrameIndex = 1;
				for (int32 Index = int32(NextFrameIDSlot) - 1; Index >= int32(NextFrameIDSlot) - NUM_FRAMES_TO_MONITOR && Index >= 0; Index--)
				{
					Result = 0;
					if (FrameIDs[Index % NUM_FRAMES_TO_MONITOR])
					{
						eglGetFrameTimestampsOPENHARMONY_p(eglDisplay, eglSurface, FrameIDs[Index % NUM_FRAMES_TO_MONITOR], 1, &TimestampList, &Result);
					}
					if (Result > 0)
					{
						break;
					}
					DeltaFrameIndex++;
				}

				GGetTimeStampsSucceededThisFrame = Result > 0;
				if (GGetTimeStampsSucceededThisFrame)
				{
					EGLnsecsOPENHARMONY FudgeFactor = 0; //  8333 * 1000;
					EGLnsecsOPENHARMONY DeltaNanos = EGLnsecsOPENHARMONY(DesiredSyncIntervalRelativeToDevice) * EGLnsecsOPENHARMONY(DeltaFrameIndex) * DriverRefreshNanos;
					EGLnsecsOPENHARMONY PresentationTime = Result + DeltaNanos + FudgeFactor;
					eglPresentationTimeOPENHARMONY_p(eglDisplay, eglSurface, PresentationTime);
					GGetTimeStampsRetryCount = 0;
				}
				else
				{
					GGetTimeStampsRetryCount++;
					if (GGetTimeStampsRetryCount == FOpenHarmonyPlatformRHIFramePacer::CVarTimeStampErrorRetryCount.GetValueOnAnyThread())
					{
						UE_LOG(LogRHI, Log, TEXT("eglGetFrameTimestampsOPENHARMONY_p failed for %d consecutive frames, reverting to naive frame pacer."), GGetTimeStampsRetryCount);
					}
				}
			}
			else
			{
				//UE_CLOG(bPrintOnce, LogRHI, Log, TEXT("eglGetFrameTimestampsSupportedOPENHARMONY doesn't exist or retured false for EGL_FIRST_COMPOSITION_START_TIME_OPENHARMONY, discarding eglGetNextFrameIdOPENHARMONY_p and eglGetFrameTimestampsOPENHARMONY_p"));
			}
			//bPrintOnce = false;
		}

		LastTimeEmulatedSync = FPlatformTime::Seconds();

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_eglSwapBuffers);

			FrameIDs[(NextFrameIDSlot) % NUM_FRAMES_TO_MONITOR] = 0;
			if (eglGetNextFrameIdOPENHARMONY_p && (CanUseGetFrameTimestamps() || FOpenHarmonyPlatformRHIFramePacer::CVarSpewGetFrameTimestamps.GetValueOnAnyThread()))
			{
				eglGetNextFrameIdOPENHARMONY_p(eglDisplay, eglSurface, &FrameIDs[(NextFrameIDSlot) % NUM_FRAMES_TO_MONITOR]);
			}
			NextFrameIDSlot++;
#endif
			if (eglSurface == NULL || !eglSwapBuffers(eglDisplay, eglSurface))
			{
				// shutdown if swapbuffering goes down
				if (SwapBufferFailureCount > 10)
				{
					//Process.killProcess(Process.myPid());		//@todo android
				}
				SwapBufferFailureCount++;

				// basic reporting
				if (eglSurface == NULL)
				{
					return false;
				}
				else
				{
					if (eglGetError() == EGL_CONTEXT_LOST)
					{
						//Logger.LogOut("swapBuffers: EGL11.EGL_CONTEXT_LOST err: " + eglGetError());					
						//Process.killProcess(Process.myPid());		//@todo android
					}
				}

				return false;
			}
		}
#if USE_VSYNC
		if (DesiredSyncIntervalRelativeToDevice > 0 && eglGetFrameTimestampsOPENHARMONY_p && FOpenHarmonyPlatformRHIFramePacer::CVarSpewGetFrameTimestamps.GetValueOnAnyThread())
		{
			static EGLint TimestampList[9] =
			{
				EGL_REQUESTED_PRESENT_TIME_OPENHARMONY,
				EGL_RENDERING_COMPLETE_TIME_OPENHARMONY,
				EGL_COMPOSITION_LATCH_TIME_OPENHARMONY,
				EGL_FIRST_COMPOSITION_START_TIME_OPENHARMONY,
				EGL_LAST_COMPOSITION_START_TIME_OPENHARMONY,
				EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_OPENHARMONY,
				EGL_DISPLAY_PRESENT_TIME_OPENHARMONY,
				EGL_DEQUEUE_READY_TIME_OPENHARMONY,
				EGL_READS_DONE_TIME_OPENHARMONY
			};

			static const TCHAR* TimestampStrings[9] =
			{
				TEXT("EGL_REQUESTED_PRESENT_TIME_OPENHARMONY"),
				TEXT("EGL_RENDERING_COMPLETE_TIME_OPENHARMONY"),
				TEXT("EGL_COMPOSITION_LATCH_TIME_OPENHARMONY"),
				TEXT("EGL_FIRST_COMPOSITION_START_TIME_OPENHARMONY"),
				TEXT("EGL_LAST_COMPOSITION_START_TIME_OPENHARMONY"),
				TEXT("EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_OPENHARMONY"),
				TEXT("EGL_DISPLAY_PRESENT_TIME_OPENHARMONY"),
				TEXT("EGL_DEQUEUE_READY_TIME_OPENHARMONY"),
				TEXT("EGL_READS_DONE_TIME_OPENHARMONY")
			};


			EGLnsecsOPENHARMONY Results[NUM_FRAMES_TO_MONITOR][9] = { {0} };
			EGLnsecsOPENHARMONY FirstRealValue = 0;
			for (int32 Index = int32(NextFrameIDSlot) - NUM_FRAMES_TO_MONITOR; Index < int32(NextFrameIDSlot); Index++)
			{
				eglGetFrameTimestampsOPENHARMONY_p(eglDisplay, eglSurface, FrameIDs[Index % NUM_FRAMES_TO_MONITOR], 9, TimestampList, Results[Index % NUM_FRAMES_TO_MONITOR]);
				for (int32 IndexInner = 0; IndexInner < 9; IndexInner++)
				{
					if (!FirstRealValue || (Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner] > 1 && Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner] < FirstRealValue))
					{
						FirstRealValue = Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner];
					}
				}
			}
			UE_CLOG(FOpenHarmonyPlatformRHIFramePacer::CVarSpewGetFrameTimestamps.GetValueOnAnyThread() > 1, LogRHI, Log, TEXT("************************************  frame %d   base time is %lld"), NextFrameIDSlot - 1, FirstRealValue);

			for (int32 Index = int32(NextFrameIDSlot) - NUM_FRAMES_TO_MONITOR; Index < int32(NextFrameIDSlot); Index++)
			{

				UE_CLOG(FOpenHarmonyPlatformRHIFramePacer::CVarSpewGetFrameTimestamps.GetValueOnAnyThread() > 1, LogRHI, Log, TEXT("eglGetFrameTimestampsOPENHARMONY_p  frame %d"), Index);
				for (int32 IndexInner = 0; IndexInner < 9; IndexInner++)
				{
					int32 MsVal = (Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner] > 1) ? int32((Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner] - FirstRealValue) / 1000000) : int32(Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner]);

					UE_CLOG(FOpenHarmonyPlatformRHIFramePacer::CVarSpewGetFrameTimestamps.GetValueOnAnyThread() > 1, LogRHI, Log, TEXT("     %8d    %s"), MsVal, TimestampStrings[IndexInner]);
				}
			}

			int32 IndexLast = int32(NextFrameIDSlot) - NUM_FRAMES_TO_MONITOR;
			int32 IndexLastNext = IndexLast + 1;

			if (Results[IndexLast % NUM_FRAMES_TO_MONITOR][3] > 1 && Results[IndexLastNext % NUM_FRAMES_TO_MONITOR][3] > 1)
			{
				int32 MsVal = int32((Results[IndexLastNext % NUM_FRAMES_TO_MONITOR][3] - Results[IndexLast % NUM_FRAMES_TO_MONITOR][3]) / 1000000);

				RecordedFrameInterval[NumRecordedFrameInterval++] = MsVal;
				if (NumRecordedFrameInterval == 100)
				{
					FString All;
					int32 NumOnTarget = 0;
					int32 NumBelowTarget = 0;
					int32 NumAboveTarget = 0;
					for (int32 Index = 0; Index < 100; Index++)
					{
						if (Index)
						{
							All += TCHAR(' ');
						}
						All += FString::Printf(TEXT("%d"), RecordedFrameInterval[Index]);

						if (RecordedFrameInterval[Index] > DesiredSyncIntervalRelativeTo60Hz * 16 - 8 && RecordedFrameInterval[Index] < DesiredSyncIntervalRelativeTo60Hz * 16 + 8)
						{
							NumOnTarget++;
						}
						else if (RecordedFrameInterval[Index] < DesiredSyncIntervalRelativeTo60Hz * 16)
						{
							NumBelowTarget++;
						}
						else
						{
							NumAboveTarget++;
						}
					}
					UE_LOG(LogRHI, Log, TEXT("%3d fast  %3d ok  %3d slow   %s"), NumBelowTarget, NumOnTarget, NumAboveTarget, *All);
					NumRecordedFrameInterval = 0;
				}
			}
		}
	}
#endif
	return true;
}

#endif
