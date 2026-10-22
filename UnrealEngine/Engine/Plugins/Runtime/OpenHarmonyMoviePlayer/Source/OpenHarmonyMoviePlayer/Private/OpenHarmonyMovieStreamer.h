// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoviePlayer.h"
#include "OpenHarmony/OpenHarmonyAvPlayer.h"
#include "Slate/SlateTextures.h"

#include "OpenHarmony/OpenHarmonyBitmapRenderer.h"
#include "OpenHarmony/OpenHarmonyPlatformFile.h"
#include <rawfile/raw_dir.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>

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

// The actual streamer class
class FOpenHarmonyMoviePlayerStreamer : public IMovieStreamer
{
public:
	FOpenHarmonyMoviePlayerStreamer();
	virtual ~FOpenHarmonyMoviePlayerStreamer();

	virtual bool Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> inPlaybackType) override;
	virtual void ForceCompletion() override;
	virtual bool Tick(float DeltaTime) override;
	virtual TSharedPtr<class ISlateViewport> GetViewportInterface() override;
	virtual float GetAspectRatio() const override;
	virtual void Cleanup() override;

	virtual FString GetMovieName() override;
	virtual bool IsLastMovieInPlaylist() override;

	FOnCurrentMovieClipFinished OnCurrentMovieClipFinishedDelegate;
	virtual FOnCurrentMovieClipFinished& OnCurrentMovieClipFinished() override { return OnCurrentMovieClipFinishedDelegate; }

private:
	/**
	A list of all the stored movie paths we have enqueued for playing
	*/
	FCriticalSection			MovieQueueCriticalSection;
	TArray<FString>				MovieQueue;

	/**
	Texture and view-port data for displaying to Slate
	*/
	TSharedPtr<FMovieViewport>		MovieViewport;
	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> Texture;

	/**
	List of textures pending deletion, need to keep this list because we can't immediately
	destroy them since they could be getting used on the main thread
	*/
	TArray<TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>> TexturesPendingDeletion;

	OHAvPlayer avPlayer;
	/**
	Java side interface for MediaPlayer utility.
	*/
	// TSharedPtr<FJavaOpenHarmonyMediaPlayer, ESPMode::ThreadSafe> JavaMediaPlayer;

	/**
	Some state for the playing movie.
	*/
	int32 CurrentPosition;

	/** Video track descriptors. */
	TArray<FVideoTrack> VideoTracks;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

	FOpenHarmonyBitmapRenderer *mBitmapRenderer = nullptr;

	bool playTrigger = false;

	bool StartNextMovie();
	void CloseMovie();

	/**
	 * Initialize the media player.
	 *
	 * @return true on success, false otherwise.
	 */
	bool InitializePlayer(RawFileDescriptor64& fd);

	bool GetVideoTracks(TArray<FVideoTrack>& VideoTracks, RawFileDescriptor64& videoFD);

	int GetCurrentPosition();
	void initBitmapRenderer();
	bool CreateBitmapRenderer();
};
