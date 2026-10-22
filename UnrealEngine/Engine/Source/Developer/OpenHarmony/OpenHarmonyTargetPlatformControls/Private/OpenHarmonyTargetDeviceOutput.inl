#include "CoreTypes.h"
#include "CoreFwd.h"

class FOpenHarmonyDeviceOutputReaderRunnable;
class FOpenHarmonyTargetDevice;
class FOpenHarmonyTargetDeviceOutput;

inline FOpenHarmonyDeviceOutputReaderRunnable::FOpenHarmonyDeviceOutputReaderRunnable(const FString& InHdcFilename, const FString& InDeviceSerialNumber, FOutputDevice* InOutput) 
	: StopTaskCounter(0)
	, HdcFilename(InHdcFilename)
	, DeviceSerialNumber(InDeviceSerialNumber)
	, Output(InOutput)
	, LogcatReadPipe(nullptr)
	, LogcatWritePipe(nullptr)
{
}

inline bool FOpenHarmonyDeviceOutputReaderRunnable::StartLogcatProcess(void)
{
	FString Params = FString::Printf(TEXT(" -t %s shell hilog -T UEOH"), *DeviceSerialNumber);
	LogcatProcHandle = FPlatformProcess::CreateProc(*HdcFilename, *Params, true, false, false, NULL, 0, NULL, LogcatWritePipe);
	return LogcatProcHandle.IsValid();
}

inline bool FOpenHarmonyDeviceOutputReaderRunnable::Init(void) 
{ 
	FPlatformProcess::CreatePipe(LogcatReadPipe, LogcatWritePipe);
	return StartLogcatProcess();
}

inline void FOpenHarmonyDeviceOutputReaderRunnable::Exit(void) 
{
	if (LogcatProcHandle.IsValid())
	{
		FPlatformProcess::CloseProc(LogcatProcHandle);
	}
	FPlatformProcess::ClosePipe(LogcatReadPipe, LogcatWritePipe);
}

inline void FOpenHarmonyDeviceOutputReaderRunnable::Stop(void)
{
	StopTaskCounter.Increment();
}

inline uint32 FOpenHarmonyDeviceOutputReaderRunnable::Run(void)
{
	FString LogcatOutput;
	
	while (StopTaskCounter.GetValue() == 0 && LogcatProcHandle.IsValid())
	{
		if (!FPlatformProcess::IsProcRunning(LogcatProcHandle))
		{
			// When user plugs out USB cable hdc process stops
			// Keep trying to restore hdc connection until code that uses this object will not kill us
			Output->Serialize(TEXT("Trying to restore connection to device..."), ELogVerbosity::Log, NAME_None);
			FPlatformProcess::CloseProc(LogcatProcHandle);
			if (StartLogcatProcess())
			{
				FPlatformProcess::Sleep(1.0f);
			}
			else
			{
				Output->Serialize(TEXT("Failed to start hdc proccess"), ELogVerbosity::Log, NAME_None);
				Stop();
			}
		}
		else
		{
			LogcatOutput.Append(FPlatformProcess::ReadPipe(LogcatReadPipe));

			if (LogcatOutput.Len() > 0)
			{
				TArray<FString> OutputLines;
				LogcatOutput.ParseIntoArray(OutputLines, TEXT("\n"), false);

				if (!LogcatOutput.EndsWith(TEXT("\n")))
				{
					// partial line at the end, do not serialize it until we receive remainder
					LogcatOutput = OutputLines.Last();
					OutputLines.RemoveAt(OutputLines.Num() - 1);
				}
				else
				{
					LogcatOutput.Reset();
				}

				for (int32 i = 0; i < OutputLines.Num(); ++i)
				{
					Output->Serialize(*OutputLines[i], ELogVerbosity::Log, NAME_None);
				}
			}
			
			FPlatformProcess::Sleep(0.1f);
		}
	}

	return 0;
}

inline bool FOpenHarmonyTargetDeviceOutput::Init(const FOpenHarmonyTargetDevice& TargetDevice, FOutputDevice* Output)
{
	check(Output);
	// Output will be produced by background thread
	check(Output->CanBeUsedOnAnyThread());
	
	DeviceSerialNumber = TargetDevice.GetSerialNumber();
	DeviceName = TargetDevice.GetName();
		
	FString HdcFilename;
	if (FOpenHarmonyTargetDevice::GetHdcFullFilename(HdcFilename))
	{
		auto* Runnable = new FOpenHarmonyDeviceOutputReaderRunnable(HdcFilename, DeviceSerialNumber, Output);
		DeviceOutputThread = TUniquePtr<FRunnableThread>(FRunnableThread::Create(Runnable, TEXT("FOpenHarmonyDeviceOutputReaderRunnable")));
		return true;
	}
		
	return false;
}
	
