#pragma once

// HEADER_UNIT_SKIP - Not included directly

#include "Containers/UnrealString.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/ITargetDeviceOutput.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/ThreadSafeCounter.h"

class FOpenHarmonyTargetDevice;

class FOpenHarmonyDeviceOutputReaderRunnable : public FRunnable
{
public:
	FOpenHarmonyDeviceOutputReaderRunnable(const FString& HdcFilename, const FString& DeviceSerialNumber, FOutputDevice* Output);
	
	// FRunnable interface.
	virtual bool Init(void) override;
	virtual void Exit(void) override; 
	virtual void Stop(void) override;
	virtual uint32 Run(void) override;

private:
	bool StartLogcatProcess(void);

private:
	// > 0 if we've been asked to abort work in progress at the next opportunity
	FThreadSafeCounter	StopTaskCounter;
	
	FString				HdcFilename;
	FString				DeviceSerialNumber;
	FOutputDevice*		Output;
	
	void*				LogcatReadPipe;
	void*				LogcatWritePipe;
	FProcHandle			LogcatProcHandle;
};

/**
 * Implements a OpenHarmony target device.
 */
class FOpenHarmonyTargetDeviceOutput : public ITargetDeviceOutput
{
public:
	bool Init(const FOpenHarmonyTargetDevice& TargetDevice, FOutputDevice* Output);
	
private:
	TUniquePtr<FRunnableThread>						DeviceOutputThread;
	FString											DeviceSerialNumber;
	FString											DeviceName;
};

#include "OpenHarmonyTargetDeviceOutput.inl" // IWYU pragma: export
