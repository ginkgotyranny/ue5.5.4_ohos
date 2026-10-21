// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "OpenHarmony/OpenHarmonyAvPlayer.h"

#include <napi/native_api.h>
#include <multimedia/player_framework/avplayer.h>
#include <multimedia/player_framework/avplayer_base.h>
#include <multimedia/player_framework/native_avsource.h>

#include <native_image/native_image.h>
#include <native_window/external_window.h>
#include <native_buffer/native_buffer.h>

#include "OpenHarmony/OpenHarmonyPlatformFile.h"
#include <rawfile/raw_dir.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include "OpenHarmony/OpenHarmonyBitmapRenderer.h"

class FOpenHarmonyMediaTextureSamplePool;
class FMediaSamples;
class IMediaEventSink;





struct FAudioTrack
{
    int32 Index;
    FString MimeType;
    FString DisplayName;
    FString Language;
    FString Name;
    uint32 Channels;
    uint32 SampleRate;
};

struct FCaptionTrack
{
    int32 Index;
    FString MimeType;
    FString DisplayName;
    FString Language;
    FString Name;
};

struct FVideoTrack
{
    int32 Index;
    FString MimeType;
    FString DisplayName;
    FString Language;
    FString Name;
    uint32 BitRate;
    FIntPoint Dimensions;
    float FrameRate;
};


/*
 * Implements media playback using the OpenHarmony MediaPlayer interface.
 */
class FOpenHarmonyMediaPlayer
        : public IMediaPlayer
                , protected IMediaCache
                , protected IMediaControls
                , protected IMediaTracks
                , protected IMediaView
{
public:

    /**
     * Create and initialize a new instance.
     *
     * @param InEventSink The object that receives media events from this player.
     */
    FOpenHarmonyMediaPlayer(IMediaEventSink& InEventSink);

    /** Virtual destructor. */
    virtual ~FOpenHarmonyMediaPlayer();

public:

    //~ IMediaPlayer interface

    virtual void Close() override;
    virtual IMediaCache& GetCache() override;
    virtual IMediaControls& GetControls() override;
    virtual FString GetInfo() const override;
    virtual FGuid GetPlayerPluginGUID() const override;
    virtual IMediaSamples& GetSamples() override;
    virtual FString GetStats() const override;
    virtual IMediaTracks& GetTracks() override;
    virtual FString GetUrl() const override;
    virtual IMediaView& GetView() override;
    virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
    virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;
    virtual void SetGuid(const FGuid& Guid) override;
    virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
    virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;

	bool GetVideoLastFrame(GLint Resource, OHAvPlayer* AvPlayer);
	int GetCurrentPosition();
protected:

    /**
     * Initialize the media player.
     *
     * @return true on success, false otherwise.
     */
    bool InitializePlayer(RawFileDescriptor64& fd);

protected:

    //~ IMediaControls interface

    virtual bool CanControl(EMediaControl Control) const override;
    virtual FTimespan GetDuration() const override;
    virtual float GetRate() const override;
    virtual EMediaState GetState() const override;
    virtual EMediaStatus GetStatus() const override;
    virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
    virtual FTimespan GetTime() const override;
    virtual bool IsLooping() const override;
    virtual bool Seek(const FTimespan& Time) override;
    virtual bool SetLooping(bool Looping) override;
    virtual bool SetRate(float Rate) override;
    virtual bool SetNativeVolume(float Volume) override;

protected:

    //~ IMediaTracks interface

    virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
    virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
    virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
    virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
    virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
    virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
    virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;

private:

    /** Callback for when the application resumed in the foreground. */
    void HandleApplicationHasEnteredForeground();

    /** Callback for when the application is being paused in the background. */
    void HandleApplicationWillEnterBackground();

    AVPlayerState GetAVPlayerState();
    bool DidComplete();
    bool GetVideoTracks(TArray<FVideoTrack>& VideoTracks, RawFileDescriptor64& fd);
    bool GetCaptionTracks(TArray<FCaptionTrack>& CaptionTracks);
    bool GetAudioTracks(TArray<FAudioTrack>& AudioTracks);

  

    bool GetVideoLastFrameData(void* & outPixels, int64 & outCount);
	
    void initBitmapRenderer();
    bool CreateBitmapRenderer();



public:

    // ---
    OHAvPlayer AvPlayer;
    FOpenHarmonyBitmapRenderer *mBitmapRenderer = nullptr;

    // ---
    /** Audio track descriptors. */
    TArray<FAudioTrack> AudioTracks;

    /** Caption track descriptors. */
    TArray<FCaptionTrack> CaptionTracks;

    /** Video track descriptors. */
    TArray<FVideoTrack> VideoTracks;

    bool avComplete = false;

    bool entryBackground = false;

private:

    /** Current player state. */
    EMediaState CurrentState;

    /** Current state of looping. */
    bool bLooping;

    /** The media event handler. */
    IMediaEventSink& EventSink;

    /** Media information string. */
    FString Info;

    /** Currently opened media. */
    FString MediaUrl;

    /** Media player Guid */
    FGuid PlayerGuid;

    /** Foreground/background delegate for pause. */
    FDelegateHandle PauseHandle;

    /** Foreground/background delegate for resume. */
    FDelegateHandle ResumeHandle;

    /** The media sample queue. */
    TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;

    /** Index of the selected audio track. */
    int32 SelectedAudioTrack;

    /** Index of the selected audio track. */
    int32 SelectedCaptionTrack;

    /** Index of the selected video track. */
    int32 SelectedVideoTrack;

    /** Index of the file handle. */
    int32 FileHandle;

    /** Video sample object pool. */
    FOpenHarmonyMediaTextureSamplePool* VideoSamplePool;

    RawFileDescriptor64 videoFileDescriptor;
};
