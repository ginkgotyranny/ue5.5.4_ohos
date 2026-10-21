// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"
#include "DSP/Dsp.h"

#include "ohaudio/native_audiocapturer.h"
#include "ohaudio/native_audiorenderer.h"
#include "ohaudio/native_audiostreambuilder.h"
#include "ohaudio/native_audiostream_base.h"

// Any platform defines
namespace Audio
{
	class FMixerPlatformOpenHarmony : public IAudioMixerPlatformInterface
	{
	public:
		FMixerPlatformOpenHarmony();
		~FMixerPlatformOpenHarmony();

		//~ Begin IAudioMixerPlatformInterface
		virtual FString GetPlatformApi() const override { return TEXT("OHOS OHAudio");}
		virtual bool InitializeHardware() override;
		virtual bool TeardownHardware() override;
		virtual bool IsInitialized() const override;
		virtual bool GetNumOutputDevices(uint32& OutNumOutputDevices) override;
		virtual bool GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo) override;
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override;
		virtual bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) override;
		virtual bool CloseAudioStream() override;
		virtual bool StartAudioStream() override;
		virtual bool StopAudioStream() override;
		virtual FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override;
		virtual void SubmitBuffer(const uint8* Buffer) override;
		virtual FString GetDefaultDeviceName() override;
		virtual FAudioPlatformSettings GetPlatformSettings() const override;
		virtual void SuspendContext() override;
		virtual void ResumeContext() override;

		//~ End IAudioMixerPlatformInterface

		// Used for the oh_audio module
		void* tmpBuffer;
		int32 tmpBufferSize;
	private:
		const TCHAR* GetErrorString(OH_AudioStream_Result Result);

		int32 GetDeviceBufferSize(int32 RenderCallbackSize) const;

		OH_AudioStream_Result ret;
		OH_AudioCapturer_Callbacks callbacks;
		OH_AudioRenderer_Callbacks rendererCallbacks;
		OH_AudioCapturer *audioCapturer;
		OH_AudioRenderer *audioRenderer;
		OH_AudioStreamBuilder *builder;
		OH_AudioStreamBuilder *rendererBuilder;

		FCriticalSection SuspendedCriticalSection;

		bool bSuspended;
		bool bInitialized;
		bool bInCallback;
		
		// This buffer is pushed to and popped from in the SubmitBuffer callback. 
		// This is required for devices that require frame counts per callback that are not powers of two.
		Audio::TCircularAudioBuffer<int16> CircularOutputBuffer;

		// This is the buffer we pop CircularOutputBuffer into in SubmitBuffer.
		TArray<int16> DeviceBuffer;

		int32 NumSamplesPerRenderCallback;
		int32 NumSamplesPerDeviceCallback;
	};

}

