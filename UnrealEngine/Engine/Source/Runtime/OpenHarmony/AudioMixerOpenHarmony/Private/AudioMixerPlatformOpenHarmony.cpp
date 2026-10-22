// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerPlatformOpenHarmony.h"
#include "Modules/ModuleManager.h"
#include "AudioMixer.h"
#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "AudioDevice.h"

#if WITH_ENGINE
#include "AudioPluginUtilities.h"
#endif


DECLARE_LOG_CATEGORY_EXTERN(LogAudioMixerOpenHarmony, Log, All);
DEFINE_LOG_CATEGORY(LogAudioMixerOpenHarmony);

#define UNREAL_AUDIO_TEST_WHITE_NOISE 0

namespace Audio
{
	FMixerPlatformOpenHarmony::FMixerPlatformOpenHarmony()
		: bSuspended(false)
		, bInitialized(false)
		, bInCallback(false)
		, NumSamplesPerRenderCallback(0)
		, NumSamplesPerDeviceCallback(0)
	{

	}

	FMixerPlatformOpenHarmony::~FMixerPlatformOpenHarmony()
	{
		if (bInitialized)
		{
			TeardownHardware();
		}
	}

	const TCHAR* FMixerPlatformOpenHarmony::GetErrorString(OH_AudioStream_Result Result)
	{
		switch (Result)
		{
		case AUDIOSTREAM_ERROR_INVALID_PARAM:	return TEXT("AUDIOSTREAM_ERROR_INVALID_PARAM");
		case AUDIOSTREAM_ERROR_ILLEGAL_STATE:	return TEXT("AUDIOSTREAM_ERROR_ILLEGAL_STATE");

		default:
		case AUDIOSTREAM_ERROR_SYSTEM:			return TEXT("AUDIOSTREAM_ERROR_SYSTEM");
		}
	}

	int32 FMixerPlatformOpenHarmony::GetDeviceBufferSize(int32 RenderCallbackSize) const
	{
		return 960;
	}

	bool FMixerPlatformOpenHarmony::InitializeHardware()
	{
		if (bInitialized)
		{
			return false;
		}

		bInitialized = true;

		return true;
	}

	bool FMixerPlatformOpenHarmony::TeardownHardware()
	{
		if(!bInitialized)
		{
			return true;
		}

		bInitialized = false;

		return true;
	}

	bool FMixerPlatformOpenHarmony::IsInitialized() const
	{
		return bInitialized;
	}

	bool FMixerPlatformOpenHarmony::GetNumOutputDevices(uint32& OutNumOutputDevices)
	{
		OutNumOutputDevices = 1;
		return true;
	}

	bool FMixerPlatformOpenHarmony::GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OHOS_WAYNE FMixerPlatformOpenHarmony::GetOutputDeviceInfo"));
		OutInfo.Name = TEXT("OpenHarmony Audio Device");
		OutInfo.DeviceId = 0;
		OutInfo.bIsSystemDefault = true;
		OutInfo.SampleRate = 48000;
		OutInfo.NumChannels = 2; 
		OutInfo.Format = EAudioMixerStreamDataFormat::Int16;
		OutInfo.OutputChannelArray.SetNum(2);
		OutInfo.OutputChannelArray[0] = EAudioMixerChannel::FrontLeft;
		OutInfo.OutputChannelArray[1] = EAudioMixerChannel::FrontRight;
		return true;
	}

	bool FMixerPlatformOpenHarmony::GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const
	{
		OutDefaultDeviceIndex = 0;
		return true;
	}

	int32_t AudioRendererOnWriteData(OH_AudioRenderer *renderer, void *userData, void *buffer, int32_t bufferLen)
	{
		FMixerPlatformOpenHarmony* MixerPlatformOpenHarmony = (FMixerPlatformOpenHarmony*)userData;
		if (MixerPlatformOpenHarmony != nullptr)
		{
			MixerPlatformOpenHarmony->ReadNextBuffer();
		}

		memcpy(buffer, MixerPlatformOpenHarmony->tmpBuffer, MixerPlatformOpenHarmony->tmpBufferSize);

		return 0;
	}

	int32_t AudioRendererOnInterrupt(OH_AudioRenderer* renderer, void* userData, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint) {
		return 0;
	}
	
	bool FMixerPlatformOpenHarmony::OpenAudioStream(const FAudioMixerOpenStreamParams& Params)
	{
		if (!bInitialized || AudioStreamInfo.StreamState != EAudioOutputStreamState::Closed)
		{
			return false;
		}

		OpenStreamParams = Params;

		AudioStreamInfo.Reset();

		AudioStreamInfo.OutputDeviceIndex = 0;
		AudioStreamInfo.NumOutputFrames = OpenStreamParams.NumFrames;
		AudioStreamInfo.NumBuffers = FMath::Max(OpenStreamParams.NumBuffers, 4);
		AudioStreamInfo.AudioMixer = OpenStreamParams.AudioMixer;

		if (!GetOutputDeviceInfo(AudioStreamInfo.OutputDeviceIndex, AudioStreamInfo.DeviceInfo))
		{	
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OHOS_WAYNE FMixerPlatformOpenHarmony::OpenAudioStream GetOutputDeviceInfo failure"));
			return false;
		}

		AudioStreamInfo.DeviceInfo.SampleRate = OpenStreamParams.SampleRate;

		FAudioPlatformSettings PlatformSettings = GetPlatformSettings();

		// Set up circular buffer between our rendering buffer size and the device's buffer size.
		// Since we are only using this circular buffer on a single thread, we do not need to add extra slack.
		NumSamplesPerRenderCallback = OpenStreamParams.NumFrames * AudioStreamInfo.DeviceInfo.NumChannels;
		NumSamplesPerDeviceCallback = PlatformSettings.CallbackBufferFrameSize * AudioStreamInfo.DeviceInfo.NumChannels;
		const int32 MaxCircularBufferCapacity = FMath::Max<int32>(NumSamplesPerRenderCallback, NumSamplesPerDeviceCallback) * 2;
		CircularOutputBuffer.SetCapacity(MaxCircularBufferCapacity);

		DeviceBuffer.Reset();
		DeviceBuffer.AddUninitialized(NumSamplesPerDeviceCallback);

		tmpBufferSize = NumSamplesPerRenderCallback * sizeof(int16);
		tmpBuffer = malloc(NumSamplesPerRenderCallback * sizeof(int16));

		OH_AudioStream_Type type = AUDIOSTREAM_TYPE_RENDERER;

		if (OH_AudioStreamBuilder_Create(&rendererBuilder, type) != AUDIOSTREAM_SUCCESS)
		{
			 UE_LOG(LogAudioMixerOpenHarmony, Warning, TEXT("OH_AudioStreamBuilder_Create failed"));
			return false;
		};

		// set params and callbacks
    
		if(OH_AudioStreamBuilder_SetSamplingRate(rendererBuilder, AudioStreamInfo.DeviceInfo.SampleRate) != AUDIOSTREAM_SUCCESS)
		{
			UE_LOG(LogAudioMixerOpenHarmony, Warning, TEXT("OH_AudioStreamBuilder_SetSamplingRate failed"));
			return false;
		};
		if(OH_AudioStreamBuilder_SetChannelCount(rendererBuilder, AudioStreamInfo.DeviceInfo.NumChannels) != AUDIOSTREAM_SUCCESS)
		{
			UE_LOG(LogAudioMixerOpenHarmony, Warning, TEXT("OH_AudioStreamBuilder_SetChannelCount failed"));
			return false;
		};
		if(OH_AudioStreamBuilder_SetSampleFormat(rendererBuilder, AUDIOSTREAM_SAMPLE_S16LE) != AUDIOSTREAM_SUCCESS)
		{
			UE_LOG(LogAudioMixerOpenHarmony, Warning, TEXT("OH_AudioStreamBuilder_SetSampleFormat failed"));
			return false;
		}
		if(OH_AudioStreamBuilder_SetLatencyMode(rendererBuilder, AUDIOSTREAM_LATENCY_MODE_NORMAL) != AUDIOSTREAM_SUCCESS)
		{
			UE_LOG(LogAudioMixerOpenHarmony, Warning, TEXT("OH_AudioStreamBuilder_SetLatencyMode failed"));
			return false;
		}
		if(OH_AudioStreamBuilder_SetFrameSizeInCallback(rendererBuilder, 20) != AUDIOSTREAM_SUCCESS)
		{
			UE_LOG(LogAudioMixerOpenHarmony, Warning, TEXT("OH_AudioStreamBuilder_SetLatencyMode failed"));
			return false;
		}

		OH_AudioStreamBuilder_SetRendererInfo(rendererBuilder, AUDIOSTREAM_USAGE_GAME);

		rendererCallbacks.OH_AudioRenderer_OnWriteData = AudioRendererOnWriteData;

		rendererCallbacks.OH_AudioRenderer_OnInterruptEvent = AudioRendererOnInterrupt;
		
		if(OH_AudioStreamBuilder_SetRendererCallback(rendererBuilder, rendererCallbacks, (void*)this) != AUDIOSTREAM_SUCCESS)
		{
			UE_LOG(LogAudioMixerOpenHarmony, Warning, TEXT("OH_AudioStreamBuilder_SetRendererCallback failed"));
			return false;
		}

		// create OH_AudioRenderer
		if(OH_AudioStreamBuilder_GenerateRenderer(rendererBuilder, &audioRenderer) != AUDIOSTREAM_SUCCESS)
		{
			UE_LOG(LogAudioMixerOpenHarmony, Warning, TEXT("OH_AudioStreamBuilder_GenerateRenderer failed"));
			return false;
		}

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Open;

		return true;
	}

	bool FMixerPlatformOpenHarmony::CloseAudioStream()
	{
		if (!bInitialized || (AudioStreamInfo.StreamState != EAudioOutputStreamState::Open && AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped))
		{
			return false;
		}

		if( audioRenderer )
		{
			OH_AudioRenderer_Release(audioRenderer);
			OH_AudioStreamBuilder_Destroy(rendererBuilder);
		}

		free(tmpBuffer);

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;

		return true;
	}

	bool FMixerPlatformOpenHarmony::StartAudioStream()
	{
		BeginGeneratingAudio();

		// set the player's state to playing
		OH_AudioStream_Result result = OH_AudioRenderer_Start(audioRenderer);
		check(AUDIOSTREAM_SUCCESS == result);

		return true;
	}

	bool FMixerPlatformOpenHarmony::StopAudioStream()
	{
		if(!bInitialized || AudioStreamInfo.StreamState != EAudioOutputStreamState::Running)
		{
			return false;
		}

		if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped)
		{
			// set the player's state to stopped
			OH_AudioStream_Result result = OH_AudioRenderer_Stop(audioRenderer);
			check(AUDIOSTREAM_SUCCESS == result);

			if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Running)
			{
				StopGeneratingAudio();
			}

			check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);
		}

		return true;
	}

	FAudioPlatformDeviceInfo FMixerPlatformOpenHarmony::GetPlatformDeviceInfo() const
	{
		return AudioStreamInfo.DeviceInfo;
	}

 	FAudioPlatformSettings FMixerPlatformOpenHarmony::GetPlatformSettings() const
 	{
#if WITH_ENGINE
		FAudioPlatformSettings PlatformSettings = FAudioPlatformSettings::GetPlatformSettings(FPlatformProperties::GetRuntimeSettingsClassName());
#else
		FAudioPlatformSettings PlatformSettings = FAudioPlatformSettings();
#endif // WITH_ENGINE

		PlatformSettings.CallbackBufferFrameSize = GetDeviceBufferSize(PlatformSettings.CallbackBufferFrameSize);
		return PlatformSettings;
	}

	void FMixerPlatformOpenHarmony::SuspendContext()
	{
		FScopeLock ScopeLock(&SuspendedCriticalSection);

		if (!bSuspended)
		{
			UE_LOG(LogAudioMixerOpenHarmony, Display, TEXT("Suspending openharmony audio renderer"));

			// set the player's state to paused
			OH_AudioStream_Result result = OH_AudioRenderer_Pause(audioRenderer);
			check(AUDIOSTREAM_SUCCESS == result);

			bSuspended = true;
		}
	}

	void FMixerPlatformOpenHarmony::ResumeContext()
	{
		FScopeLock ScopeLock(&SuspendedCriticalSection);

		// set the player's state to paused
		if (bSuspended)
		{
			UE_LOG(LogAudioMixerOpenHarmony, Display, TEXT("Resuming android audio renderer"));

			OH_AudioStream_Result result = OH_AudioRenderer_Start(audioRenderer);
			check(AUDIOSTREAM_SUCCESS == result);

			bSuspended = false;
		}
	}

	void FMixerPlatformOpenHarmony::SubmitBuffer(const uint8* Buffer)
	{
		memcpy(tmpBuffer, Buffer, NumSamplesPerRenderCallback * sizeof(int16));
	}

	FString FMixerPlatformOpenHarmony::GetDefaultDeviceName()
	{
		return FString();
	}
}
