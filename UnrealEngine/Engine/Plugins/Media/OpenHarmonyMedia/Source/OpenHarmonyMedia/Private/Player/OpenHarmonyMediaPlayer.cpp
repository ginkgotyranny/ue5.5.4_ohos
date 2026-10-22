// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyMediaPlayer.h"
#include "OpenHarmonyMediaPrivate.h"

#include "OpenHarmony/OpenHarmonyPlatformFile.h"
#include "OpenHarmonyMediaSettings.h"
#include "IMediaEventSink.h"
#include "MediaSamples.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIResources.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "ExternalTexture.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "IPlatformFilePak.h"

#include "OpenHarmonyMediaTextureSample.h"

#include <GLES3/gl31.h>
#include <rawfile/raw_dir.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>

#include <fcntl.h>

#define LOCTEXT_NAMESPACE "FOpenHarmonyMediaPlayer"

#define OPENHARMONYMEDIAPLAYER_USE_EXTERNALTEXTURE 0


/* FOpenHarmonyMediaPlayer structors
 *****************************************************************************/

FOpenHarmonyMediaPlayer::FOpenHarmonyMediaPlayer(IMediaEventSink& InEventSink)
        : CurrentState(EMediaState::Closed)
        , bLooping(false)
        , EventSink(InEventSink)
        , Samples(MakeShared<FMediaSamples, ESPMode::ThreadSafe>())
        , SelectedAudioTrack(0)
        , SelectedCaptionTrack(INDEX_NONE)
        , SelectedVideoTrack(0)
        , VideoSamplePool(new FOpenHarmonyMediaTextureSamplePool)
{
	AvPlayer.player = OH_AVPlayer_Create();

    OH_AVPlayerOnInfoCallback callbackInfo = [](OH_AVPlayer *player, AVPlayerOnInfoType type, OH_AVFormat *infoBody, void *userData){
        FOpenHarmonyMediaPlayer* OpenHarmonyMediaPlayer = (FOpenHarmonyMediaPlayer*)userData;
        if (type == AV_INFO_TYPE_INTERRUPT_EVENT) {
            //int32_t audio_interrupt_force, audio_interrupt_hint, audio_interrupt_type;
            //OH_AVFormat_GetIntValue(infoBody, OH_PLAYER_AUDIO_INTERRUPT_HINT, &audio_interrupt_hint);
            //if (audio_interrupt_hint == 1) {
            //	//收到恢复消息
            //	openHarmonyMediaPlayer->SetRate(1.0f);
            //}
            //else if (audio_interrupt_hint == 2) {
            //	//收到暂停消息
            //	openHarmonyMediaPlayer->SetRate(0.0f);
            //}
        }
        else if (type == AV_INFO_TYPE_EOS) {
            OpenHarmonyMediaPlayer->avComplete = true;
        }
        else if (type == AV_INFO_TYPE_STATE_CHANGE) {
            int32_t _state;
            OH_AVFormat_GetIntValue(infoBody, OH_PLAYER_STATE, &_state);
            AVPlayerState state  = (AVPlayerState) _state;
            if (state == AV_PLAYING && !OpenHarmonyMediaPlayer->entryBackground) {
                OpenHarmonyMediaPlayer->CurrentState = EMediaState::Playing;
                OpenHarmonyMediaPlayer->EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
            } else if (state == AV_PAUSED && !OpenHarmonyMediaPlayer->entryBackground) {
                OpenHarmonyMediaPlayer->CurrentState = EMediaState::Paused;
                OpenHarmonyMediaPlayer->EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
            }
        }
    };
    OH_AVPlayer_SetOnInfoCallback(AvPlayer.player, callbackInfo, this);

    OH_AVPlayerOnErrorCallback callbackError = [](OH_AVPlayer *player, int32_t errorCode, const char *errorMsg, void *userData){
        UE_LOG(LogTemp, Warning, TEXT("OH_AVPlayer_SetOnErrorCallback:%d,msg:%s"),errorCode,*FString(errorMsg));
    };
    OH_AVPlayer_SetOnErrorCallback(AvPlayer.player, callbackError, this);
}

FOpenHarmonyMediaPlayer::~FOpenHarmonyMediaPlayer()
{
    Close();

    if (GSupportsImageExternal && !FOpenHarmonyMisc::ShouldUseVulkan())
    {
        OH_AVPlayer_Release(AvPlayer.player);

        struct FReleaseVideoResourcesParams
        {
            FTextureRHIRef VideoTexture;
            FGuid PlayerGuid;
        };

        FReleaseVideoResourcesParams ReleaseVideoResourcesParams = { AvPlayer.videoTexture, PlayerGuid };

        ENQUEUE_RENDER_COMMAND(OpenHarmonyMediaPlayerWriteVideoSample)(
                [ReleaseVideoResourcesParams](FRHICommandListImmediate& RHICmdList)
                {
                    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("~FOpenHarmonyMediaPlayer: Unregister Guid: %s"), *ReleaseVideoResourcesParams.PlayerGuid.ToString());
                    FExternalTextureRegistry::Get().UnregisterExternalTexture(ReleaseVideoResourcesParams.PlayerGuid);
                    // @todo: this causes a crash  // Params.VideoTexture->Release();
                });
    }

    delete VideoSamplePool;
    VideoSamplePool = nullptr;

    if(FileHandle != -1)
    {
        close(FileHandle);
        FileHandle = -1;
    }
    if(videoFileDescriptor.fd != -1){
        close(videoFileDescriptor.fd);
        videoFileDescriptor.fd = -1;
    }
}


/* IMediaPlayer interface
 *****************************************************************************/

static void DumpState(EMediaState state)
{
    switch (state)
    {
        case EMediaState::Closed:
            FPlatformMisc::LowLevelOutputDebugString(TEXT("OpenHarmonyMediaPlayer: CurrentState = Closed"));
            break;
        case EMediaState::Error:
            FPlatformMisc::LowLevelOutputDebugString(TEXT("OpenHarmonyMediaPlayer: CurrentState = Error"));
            break;
        case EMediaState::Paused:
            FPlatformMisc::LowLevelOutputDebugString(TEXT("OpenHarmonyMediaPlayer: CurrentState = Paused"));
            break;
        case EMediaState::Playing:
            FPlatformMisc::LowLevelOutputDebugString(TEXT("OpenHarmonyMediaPlayer: CurrentState = Playing"));
            break;
        case EMediaState::Preparing:
            FPlatformMisc::LowLevelOutputDebugString(TEXT("OpenHarmonyMediaPlayer: CurrentState = Preparing"));
            break;
        case EMediaState::Stopped:
            FPlatformMisc::LowLevelOutputDebugString(TEXT("OpenHarmonyMediaPlayer: CurrentState = Stopped"));
            break;
    }
}


static void DumpAVPlayerState(AVPlayerState avState)
{
    FString state;
    switch (avState)
    {
        case AVPlayerState::AV_STOPPED:
            state = "stopped";
            break;
        case AVPlayerState::AV_COMPLETED:
            state = "completed";
            break;
        case AVPlayerState::AV_PLAYING:
            state = "playing";
            break;
        case AVPlayerState::AV_IDLE:
            state = "idle";
            break;
        case AVPlayerState::AV_INITIALIZED:
            state = "initialized";
            break;
        case AVPlayerState::AV_PREPARED:
            state = "prepared";
            break;
        case AVPlayerState::AV_PAUSED:
            state = "paused";
            break;
        case AVPlayerState::AV_RELEASED:
            state = "released";
            break;
        case AVPlayerState::AV_ERROR:
            state = "error";
            break;
        default:
            state = "none";
            break;
    }
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("TickInput, STATE: %s"), *state);
}



void FOpenHarmonyMediaPlayer::Close()
{
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMedia::Close() - %s"), *PlayerGuid.ToString());

    if (CurrentState == EMediaState::Closed)
    {
        return;
    }

    CurrentState = EMediaState::Closed;

    bLooping = false;

    OH_AVPlayer_SetLooping(AvPlayer.player, false);
    OH_AVPlayer_Pause(AvPlayer.player);
    OH_AVPlayer_Stop(AvPlayer.player);
    OH_AVPlayer_Reset(AvPlayer.player);

    VideoSamplePool->Reset();

    SelectedAudioTrack = INDEX_NONE;
    SelectedCaptionTrack = INDEX_NONE;
    SelectedVideoTrack = INDEX_NONE;

    AudioTracks.Empty();
    CaptionTracks.Empty();
    VideoTracks.Empty();

    Info.Empty();
    MediaUrl = FString();

    // notify listeners
    EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
    EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}

IMediaCache& FOpenHarmonyMediaPlayer::GetCache()
{
    return *this;
}

IMediaControls& FOpenHarmonyMediaPlayer::GetControls()
{
    return *this;
}

FString FOpenHarmonyMediaPlayer::GetInfo() const
{
    return Info;
}

FGuid FOpenHarmonyMediaPlayer::GetPlayerPluginGUID() const
{
    static FGuid PlayerPluginGUID(0xb741d759, 0xbeb14030, 0xa1f97e79, 0xb431f427);
    return PlayerPluginGUID;
}

IMediaSamples& FOpenHarmonyMediaPlayer::GetSamples()
{
    return *Samples.Get();
}

FString FOpenHarmonyMediaPlayer::GetStats() const
{
    return TEXT("OpenHarmonyMedia stats information not implemented yet");
}

IMediaTracks& FOpenHarmonyMediaPlayer::GetTracks()
{
    return *this;
}

FString FOpenHarmonyMediaPlayer::GetUrl() const
{
    return MediaUrl;
}

IMediaView& FOpenHarmonyMediaPlayer::GetView()
{
    return *this;
}

bool FOpenHarmonyMediaPlayer::Open(const FString& Url, const IMediaOptions* /*Options*/)
{
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMedia::Open(%s) - %s"), *Url, *PlayerGuid.ToString());
    avComplete = false;
    Close();

    if ((Url.IsEmpty()))
    {
        return false;
    }
    MediaUrl = Url;
    videoFileDescriptor  = {0, 0, 0};

    extern NativeResourceManager* GNativeResourceManager;
    // open the media
    if (Url.StartsWith(TEXT("file://")))
    {
        FString FilePath = Url.RightChop(7);
        FPaths::NormalizeFilename(FilePath);

        IOpenHarmonyPlatformFile& PlatformFile = static_cast<IOpenHarmonyPlatformFile&>(IOpenHarmonyPlatformFile::GetPlatformPhysical());

        if (!PlatformFile.FileExists(*FilePath))
        {
            // possible it is in a PAK file
            FPakPlatformFile* PakPlatformFile = (FPakPlatformFile*)(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));

            TRefCountPtr<FPakFile> PakFile;

            FPakEntry FileEntry;
            if (PakPlatformFile == nullptr || !PakPlatformFile->FindFileInPakFiles(*FilePath, &PakFile, &FileEntry))
            {
                UE_LOG(LogOpenHarmonyMedia, Warning, TEXT("File doesn't exist %s."), *FilePath);
                return false;
            }

            // is it a simple case (can just use file datasource)?
            if (FileEntry.CompressionMethodIndex == 0 && !FileEntry.IsEncrypted())
            {
                FString PakFilename = PakFile->GetFilename();
                int64 PakHeaderSize = FileEntry.GetSerializedSize(PakFile->GetInfo().Version);
                int64 OffsetInPak = FileEntry.Offset + PakHeaderSize;
                int64 FileSize = FileEntry.Size;

                int64 FileOffset = PlatformFile.FileStartOffset(*PakFilename) + OffsetInPak;
                FString FileRootPath = PlatformFile.FileRootPath(*PakFilename);
                FileHandle = open(TCHAR_TO_UTF8(*FileRootPath), O_RDONLY);

                // possible PAK file in rawfile
                if(FileHandle == -1)
                {
                    RawFile64 *Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_ANSI(*FileRootPath));
                    if (Rfile != nullptr)
                    {
                        OH_ResourceManager_GetRawFileDescriptor64(Rfile, &videoFileDescriptor);
                        FileHandle = videoFileDescriptor.fd;
                        // api11新增注意点: 需要再申请一个fd，与avplayer的fd分开，避免getVideoTrackInfo时，avplayer的fd被影响。
                        OH_ResourceManager_GetRawFileDescriptor64(Rfile, &videoFileDescriptor);
                        OH_ResourceManager_CloseRawFile64(Rfile);
                    }
                } else {
                    // 如果视频存放在pak中，且rawfile和沙箱路径下均有对应的pak的情况下，会优先读取沙箱路径下的文件，会导致videoFileDescriptor.fd为空，从而跳过了视频解析。
                    videoFileDescriptor.fd = open(TCHAR_TO_UTF8(*FileRootPath), O_RDONLY);
                }
                videoFileDescriptor.start = FileOffset;
                videoFileDescriptor.length = FileSize;

                int32_t ret = OH_AVPlayer_SetFDSource(AvPlayer.player, FileHandle, FileOffset, FileSize);
                if (ret != AV_ERR_OK)
                {
                    UE_LOG(LogOpenHarmonyMedia, Warning, TEXT("Failed to set data source for file in PAK %s"), *FilePath);
                    return false;
                }
            }
            else
            {
                // todo: 加密和压缩的情况, 鸿蒙 avplayer api 暂时不支持
                UE_LOG(LogOpenHarmonyMedia, Warning, TEXT("File cannot be played on this version of OpenHarmony due to PAK file with compression or encryption: %s."), *FilePath);
                return false;
            }
        }
        else
        {
            FString FileRootPath = PlatformFile.FileRootPath(*FilePath);
            if (PlatformFile.IsAsset(*FileRootPath))
            {
                RawFile64 *Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_ANSI(*FileRootPath));
                OH_ResourceManager_GetRawFileDescriptor64(Rfile, &videoFileDescriptor);
                FileHandle = videoFileDescriptor.fd;
                OH_ResourceManager_GetRawFileDescriptor64(Rfile, &videoFileDescriptor);
                OH_ResourceManager_CloseRawFile64(Rfile);
            }
            else
            {
                videoFileDescriptor.fd = open(TCHAR_TO_UTF8(*FileRootPath), O_RDONLY);
                FileHandle = videoFileDescriptor.fd;
                // api11新增注意点: 需要再申请一个fd，与avplayer的fd分开，避免getVideoTrackInfo时，avplayer的fd被影响。
                videoFileDescriptor.fd = open(TCHAR_TO_UTF8(*FileRootPath), O_RDONLY);
                videoFileDescriptor.start = 0;
                videoFileDescriptor.length = PlatformFile.FileSize(*FilePath);
            }
            int32_t ret = OH_AVPlayer_SetFDSource(AvPlayer.player,
                                                  FileHandle, videoFileDescriptor.start, videoFileDescriptor.length);
            if (ret != AV_ERR_OK)
            {
                UE_LOG(LogOpenHarmonyMedia, Error, TEXT("AvPlayer datasource failed, error code: %d"), ret);
                return false;
            }
        }
    }
    else
    {
        // open remote media
        UE_LOG(LogOpenHarmonyMedia, Warning, TEXT("Failed to set data source"));
        return false;
    }
    int ret = AV_ERR_OK;


    // 创建纹理
#if OPENHARMONYMEDIAPLAYER_USE_EXTERNALTEXTURE 
    if (!FOpenHarmonyMisc::ShouldUseVulkan())
    {

		struct FWriteVideoSampleParams
		{
			OHAvPlayer* AvPlayer;
		}Params = { &AvPlayer };

		ENQUEUE_RENDER_COMMAND(OpenHarmonyMediaPlayerWriteVideoSample)(
			[Params](FRHICommandListImmediate& RHICmdList)
			{
				//==============replace=================
				const FRHITextureCreateDesc Desc =
					FRHITextureCreateDesc::Create2D(TEXT("OpenHarmony_FRHIResourceCreateInfo"), 1, 1, PF_B8G8R8A8)
					.SetNumMips(1).SetNumSamples(1).SetFlags(ETextureCreateFlags::None)
					//                        .SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::SRGB | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
					.SetInitialState(ERHIAccess::SRVGraphics);
				Params.AvPlayer->videoTexture = RHICreateTexture(Desc);
				//==============replace================
			});

		AvPlayer.textureId = *reinterpret_cast<int32_t*>(AvPlayer.videoTexture->GetNativeResource());
    }
	else
#else
	{

        GLuint mTextureID;
        glGenTextures(1, &mTextureID);
        GLenum error = glGetError();
        if(error != GL_NO_ERROR){
            FPlatformMisc::LowLevelOutputDebugStringf(TEXT("glGenTextures failed"));
            return false;
        }
		AvPlayer.textureId = mTextureID;
    }
#endif

    if(AvPlayer.nativeImage == nullptr){
		AvPlayer.nativeImage = OH_NativeImage_Create(AvPlayer.textureId, GL_TEXTURE_EXTERNAL_OES);
        OH_NativeImage_SetOnFrameAvailableListener(AvPlayer.nativeImage, {
                &AvPlayer, [] (void *context) {
                    OHAvPlayer *player = static_cast<OHAvPlayer*>(context);
                    player->numFrameAvailable++;
                }
        });
    }

    if(AvPlayer.nativeWindow == nullptr){
		AvPlayer.nativeWindow = OH_NativeImage_AcquireNativeWindow(AvPlayer.nativeImage);
        ret = OH_AVPlayer_SetVideoSurface(AvPlayer.player, AvPlayer.nativeWindow);
        if (ret != AV_ERR_OK) {
            FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OH_AVPlayer_SetVideoSurface failed"));
            return false;
        }
    }

    ret = OH_AVPlayer_Prepare(AvPlayer.player);
    if (ret != AV_ERR_OK) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OH_AVPlayer_Prepare failed"));
        return false;
    }
	AvPlayer.textureValid = true;

    ret = OH_AVPlayer_SetAudioRendererInfo(AvPlayer.player, OH_AudioStream_Usage::AUDIOSTREAM_USAGE_GAME);
    if (ret != AV_ERR_OK) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OH_AVPlayer_SetAudioRendererInfo failed"));
        return false;
    }
    CurrentState = EMediaState::Preparing;
    return true;
}

bool FOpenHarmonyMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* /*Options*/)
{
    return false;
}

void FOpenHarmonyMediaPlayer::SetGuid(const FGuid& Guid)
{
    PlayerGuid = Guid;
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("IMediaPlayer SetGuid: %s"), *PlayerGuid.ToString());
}

// ==============================================================
// Used by TickFetch

//使用外部纹理
static void DoUpdateExternalMediaSampleExecute(OHAvPlayer * AvPlayer, FGuid PlayerGuid)
{
    // 更新外部纹理
    if(AvPlayer->numFrameAvailable > 0 && AvPlayer->nativeImage != nullptr)
    {
		AvPlayer->numFrameAvailable--;

        int ret = OH_NativeImage_UpdateSurfaceImage(AvPlayer->nativeImage);
        if (ret != 0) {
            FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OH_NativeImage_UpdateSurfaceImage failed"));
        }

        glBindTexture(0x8D65, 0);
    }
}

struct FRHICommandUpdateExternalMediaSample final : public FRHICommand<FRHICommandUpdateExternalMediaSample>
{
    OHAvPlayer * AvPlayer;
    FGuid PlayerGuid;

    FORCEINLINE_DEBUGGABLE FRHICommandUpdateExternalMediaSample(OHAvPlayer * AvPlayer, FGuid PlayerGuid)
            : AvPlayer(AvPlayer), PlayerGuid(PlayerGuid)
    {
    }
    void Execute(FRHICommandListBase& CmdList)
    {
        QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandUpdateExternalMediaSample_Execute);
        DoUpdateExternalMediaSampleExecute(AvPlayer, PlayerGuid);
    }
};

//不使用外部纹理
static void ConditionalInitializeVideoSample_RenderThread(TSharedRef<FOpenHarmonyMediaTextureSample, ESPMode::ThreadSafe> VideoSample)
{
	if (VideoSample->GetTexture() == nullptr || VideoSample->GetTexture()->GetSizeXY() != VideoSample->GetOutputDim())
	{
		VideoSample->InitializeTexture(FTimespan::Zero());
	}
}

static void DoUpdateTextureMediaSampleExecute(FOpenHarmonyMediaPlayer* OHMediaPlayer, TWeakPtr<FMediaSamples, ESPMode::ThreadSafe> SamplesPtr,
	TSharedRef<FOpenHarmonyMediaTextureSample, ESPMode::ThreadSafe> VideoSample)
{
	
	auto PinnedSamples = SamplesPtr.Pin();

	if (!PinnedSamples.IsValid())
	{
		return;
	}

	const int32 CurrentFramePosition = OHMediaPlayer->GetCurrentPosition();
	const FTimespan Time = FTimespan::FromMilliseconds(CurrentFramePosition);

	// write frame into texture
	FRHITexture2D* Texture = VideoSample->InitializeTexture(Time);

	if (Texture != nullptr)
	{
		int32 Resource = *reinterpret_cast<int32*>(Texture->GetNativeResource());
		bool ret = OHMediaPlayer->GetVideoLastFrame(Resource, &OHMediaPlayer->AvPlayer);
		if (!ret)
		{
			return;
		}
	}

	PinnedSamples->AddVideo(VideoSample);
}

struct FRHICommandUpdateTextureMediaSample final : public FRHICommand<FRHICommandUpdateTextureMediaSample>
{
	FOpenHarmonyMediaPlayer* OHMediaPlayer;
	TWeakPtr<FMediaSamples, ESPMode::ThreadSafe> SamplesPtr;
	TSharedRef<FOpenHarmonyMediaTextureSample, ESPMode::ThreadSafe> VideoSample;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateTextureMediaSample(FOpenHarmonyMediaPlayer* OHMediaPlayer, TWeakPtr<FMediaSamples, ESPMode::ThreadSafe> InSamplesPtr,
		TSharedRef<FOpenHarmonyMediaTextureSample, ESPMode::ThreadSafe> InVideoSample)
		: OHMediaPlayer(OHMediaPlayer)
		, SamplesPtr(InSamplesPtr)
		, VideoSample(InVideoSample)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandUpdateTextureMediaSample_Execute);
		DoUpdateTextureMediaSampleExecute(OHMediaPlayer, SamplesPtr, VideoSample);
	}
};

// ==============================================================

void FOpenHarmonyMediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan /*Timecode*/)
{

    if (CurrentState != EMediaState::Playing && CurrentState != EMediaState::Paused)
    {
        return;
    }

    if(FOpenHarmonyMisc::ShouldUseVulkan())
    {
        const FVideoTrack VideoTrack = VideoTracks[SelectedVideoTrack];
        auto VideoSample = VideoSamplePool->AcquireShared();
        if (!VideoSample->Initialize(VideoTrack.Dimensions, FTimespan::FromSeconds(1.0 / VideoTrack.FrameRate)))
        {
            return;
        }
        struct FWriteVideoSampleParams
        {
            FOpenHarmonyMediaPlayer *OHMediaPlayer;
            TWeakPtr<FMediaSamples, ESPMode::ThreadSafe> SamplesPtr;
            TSharedRef<FOpenHarmonyMediaTextureSample, ESPMode::ThreadSafe> VideoSample;
            int32 SampleCount;
        }
        Params = { this, Samples, VideoSample, (int32)(VideoTrack.Dimensions.X * VideoTrack.Dimensions.Y * sizeof(int32)) };

        ENQUEUE_RENDER_COMMAND(OpenHarmonyMediaPlayerWriteVideoSample)(
                [Params](FRHICommandListImmediate& RHICmdList)
                {
                    FOpenHarmonyMediaPlayer *OHMediaPlayer = Params.OHMediaPlayer;
                    auto PinnedSamples = Params.SamplesPtr.Pin();
                    if (!PinnedSamples.IsValid())
                    {
                        return;
                    }
                    const FTimespan Time = OHMediaPlayer->GetTime();
                    // write frame into buffer
                    void* Buffer = nullptr;
                    int64 BufferCount = 0;
                    if (!OHMediaPlayer->GetVideoLastFrameData(Buffer, BufferCount))
                    {
                        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMediaPlayer::Fetch: OHMediaPlayer->GetVideoLastFrameData return false !!!!!"));
                        return;
                    }
                    if (BufferCount != Params.SampleCount)
                    {
                        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMediaPlayer::Fetch: Sample count mismatch (Buffer=%d, Available=%d"), Params.SampleCount, BufferCount);
                    }
                    check(Params.SampleCount <= BufferCount);
                    Params.VideoSample->InitializeBuffer(Buffer, Time, true);
                    PinnedSamples->AddVideo(Params.VideoSample);
                });
    }
    else
#if OPENHARMONYMEDIAPLAYER_USE_EXTERNALTEXTURE
    {
        struct FWriteVideoSampleParams
        {
            OHAvPlayer * AvPlayer;
            FGuid PlayerGuid;
            TSharedRef<FOpenHarmonyMediaTextureSample, ESPMode::ThreadSafe> VideoSample;
            TWeakPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;
        };

        auto VideoSample = VideoSamplePool->AcquireShared();
        const FVideoTrack VideoTrack = VideoTracks[SelectedVideoTrack];
        if (!VideoSample->Initialize(VideoTrack.Dimensions, FTimespan::FromSeconds(1.0 / VideoTrack.FrameRate), FTimespan::FromMilliseconds(GetCurrentPosition())))
        {
            return;
        }

        FWriteVideoSampleParams Params = {&AvPlayer, PlayerGuid, VideoSample, Samples};
        ENQUEUE_RENDER_COMMAND(OpenHarmonyMediaPlayerWriteVideoSample)(
                [Params](FRHICommandListImmediate& RHICmdList)
                {
                    // 创建纹理 及 更新NativeImage
                    if (IsRunningRHIInSeparateThread())
                    {
                        new (RHICmdList.AllocCommand<FRHICommandUpdateExternalMediaSample>()) FRHICommandUpdateExternalMediaSample(Params.AvPlayer, Params.PlayerGuid);
                        // wait for DoUpdateExternalMediaSampleExecute to complete.
                        RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
                    }
                    else
                    {
                        DoUpdateExternalMediaSampleExecute(Params.AvPlayer, Params.PlayerGuid);
                    }
                    // 注册外部纹理
                    OHAvPlayer* AvPlayer_temp = Params.AvPlayer;
                    auto PinnedSamples = Params.Samples.Pin();
                    if(!PinnedSamples.IsValid())
                    {
                        return;
                    }

                    if(AvPlayer_temp->videoTexture != nullptr && !AvPlayer_temp->textureValid)
                    {
                        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Fetch RT: Register Guid: %s"), *Params.PlayerGuid.ToString());
                        FSamplerStateInitializerRHI SamplerStateInitializer(SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp);
                        FSamplerStateRHIRef SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
                        FExternalTextureRegistry::Get().RegisterExternalTexture(Params.PlayerGuid, AvPlayer_temp->videoTexture, SamplerStateRHI, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
						AvPlayer_temp->textureValid = true;
                    }

                    // Add a dummy video sample
                    // (actual data flows "on the side" via external texture mechanism)
                    PinnedSamples->AddVideo(Params.VideoSample);
                });
    }
#else 
	{
		const FVideoTrack VideoTrack = VideoTracks[SelectedVideoTrack];
		auto VideoSample = VideoSamplePool->AcquireShared();
		if (!VideoSample->Initialize(VideoTrack.Dimensions, FTimespan::FromSeconds(1.0 / VideoTrack.FrameRate)))
		{
			return;
		}
		struct FWriteVideoSampleParams
		{
			FOpenHarmonyMediaPlayer* OHMediaPlayer;
			TWeakPtr<FMediaSamples, ESPMode::ThreadSafe> SamplesPtr;
			TSharedRef<FOpenHarmonyMediaTextureSample, ESPMode::ThreadSafe> VideoSample;
			int32 SampleCount;
		}
		Params = { this, Samples, VideoSample, (int32)(VideoTrack.Dimensions.X * VideoTrack.Dimensions.Y * sizeof(int32)) };

		ENQUEUE_RENDER_COMMAND(OpenHarmonyMediaPlayerWriteVideoSample)(
			[Params](FRHICommandListImmediate& RHICmdList)
			{
				FOpenHarmonyMediaPlayer* OHMediaPlayer = Params.OHMediaPlayer;
				auto PinnedSamples = Params.SamplesPtr.Pin();
				if (!PinnedSamples.IsValid())
				{
					return;
				}
				const FTimespan Time = OHMediaPlayer->GetTime();
				// write frame into buffer
				void* Buffer = nullptr;
				int64 BufferCount = 0;
				if (!OHMediaPlayer->GetVideoLastFrameData(Buffer, BufferCount))
				{
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMediaPlayer::Fetch: OHMediaPlayer->GetVideoLastFrameData return false !!!!!"));
					return;
				}
				if (BufferCount != Params.SampleCount)
				{
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMediaPlayer::Fetch: Sample count mismatch (Buffer=%d, Available=%d"), Params.SampleCount, BufferCount);
				}
				check(Params.SampleCount <= BufferCount);
				Params.VideoSample->InitializeBuffer(Buffer, Time, true);
				PinnedSamples->AddVideo(Params.VideoSample);
			});
	}
#endif //OPENHARMONYMEDIAPLAYER_USE_EXTERNALTEXTURE
}

void FOpenHarmonyMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan /*Timecode*/)
{

    // AVPlayerState avState = GetAVPlayerState();
    // DumpAVPlayerState(avState);

    if (CurrentState != EMediaState::Playing)
    {
        // remove delegates if registered
        if (ResumeHandle.IsValid())
        {
            FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ResumeHandle);
            ResumeHandle.Reset();
        }

        if (PauseHandle.IsValid())
        {
            FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(PauseHandle);
            PauseHandle.Reset();
        }
        if (CurrentState == EMediaState::Preparing)
        {
            AVPlayerState avState = GetAVPlayerState();
            if(avState == AVPlayerState::AV_PREPARED)
            {
                InitializePlayer(videoFileDescriptor);
            }
        } else if (CurrentState == EMediaState::Stopped)
        {
            if (DidComplete())
            {
                EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
                FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMedia::Tick - PlaybackEndReached - stopped - %s"), *PlayerGuid.ToString());
            }
        }
        return;
    }

    // register delegate if not registered
    if (!ResumeHandle.IsValid())
    {
        ResumeHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FOpenHarmonyMediaPlayer::HandleApplicationWillEnterBackground);
    }
    if (!PauseHandle.IsValid())
    {
        PauseHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FOpenHarmonyMediaPlayer::HandleApplicationHasEnteredForeground);
    }

    // generate events
    if (GetAVPlayerState() != AVPlayerState::AV_PLAYING)
    {
        // might catch it restarting the loop so ignore if looping
        if (!bLooping)
        {
            CurrentState = EMediaState::Stopped;
            EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
            FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMedia::Tick - PlaybackSuspended - !playing - %s"), *PlayerGuid.ToString());
        }

        if (DidComplete())
        {
            EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
            FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMedia::Tick - PlaybackEndReached - !playing - %s"), *PlayerGuid.ToString());
        }
    }
    else if (DidComplete())
    {
        EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMedia::Tick - PlaybackEndReached - DidComplete true - %s"), *PlayerGuid.ToString());
    }
}

bool FOpenHarmonyMediaPlayer::InitializePlayer(RawFileDescriptor64& videoFileDescriptor_)
{
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOpenHarmonyMedia::InitializePlayer %s"), *PlayerGuid.ToString());

    if (ResumeHandle.IsValid())
    {
        FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ResumeHandle);
        ResumeHandle.Reset();
    }

    if (PauseHandle.IsValid())
    {
        FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(PauseHandle);
        PauseHandle.Reset();
    }
    if (videoFileDescriptor_.fd != 0)
    {
        GetVideoTracks(VideoTracks, videoFileDescriptor_);
    }
    for (FVideoTrack Track : VideoTracks)
    {
        Info += FString::Printf(TEXT("Stream %i\n"), Track.Index);
        Info += TEXT("    Type: Video\n");
        //		Info += FString::Printf(TEXT("    Duration: %s\n"), *FTimespan::FromMilliseconds(Track->StreamInfo.duration).ToString());
        Info += FString::Printf(TEXT("    MimeType: %s\n"), *Track.MimeType);
        Info += FString::Printf(TEXT("    Language: %s\n"), *Track.Language);
        Info += FString::Printf(TEXT("    FrameRate: %0.1f\n"), Track.FrameRate);
        Info += FString::Printf(TEXT("    Dimensions: %i x %i\n"), Track.Dimensions.X, Track.Dimensions.Y);
        Info += TEXT("\n");
    }
    FPlatformMisc::LowLevelOutputDebugStringf( TEXT("%s"), *Info);

    // Select first audio and video track by default (have to force update first time by setting to none)
    if (AudioTracks.Num() > 0)
    {
        SelectedAudioTrack = 0;
    }
    else
    {
        SelectedAudioTrack = INDEX_NONE;
    }

    if (VideoTracks.Num() > 0)
    {
        SelectedVideoTrack = 0;
    }
    else
    {
        SelectedVideoTrack = INDEX_NONE;
    }

    CurrentState = EMediaState::Stopped;

    // notify listeners
    EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
    EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
    return true;
}


/* IMediaTracks interface
 *****************************************************************************/

bool FOpenHarmonyMediaPlayer::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
    if ((FormatIndex != 0) || !AudioTracks.IsValidIndex(TrackIndex))
    {
        return false;
    }

    const FAudioTrack& Track = AudioTracks[TrackIndex];

    OutFormat.BitsPerSample = 16;
    OutFormat.NumChannels = Track.Channels;
    OutFormat.SampleRate = Track.SampleRate;
    OutFormat.TypeName = TEXT("Native");

    return true;
}

int32 FOpenHarmonyMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
    switch (TrackType)
    {
        case EMediaTrackType::Audio:
            return AudioTracks.Num();

        case EMediaTrackType::Caption:
            return CaptionTracks.Num();

        case EMediaTrackType::Video:
            return VideoTracks.Num();

        default:
            return 0;
    }
}

int32 FOpenHarmonyMediaPlayer::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
    return ((TrackIndex >= 0) && (TrackIndex < GetNumTracks(TrackType))) ? 1 : 0;
}

int32 FOpenHarmonyMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
    switch (TrackType)
    {
        case EMediaTrackType::Audio:
            return SelectedAudioTrack;

        case EMediaTrackType::Caption:
            return SelectedCaptionTrack;

        case EMediaTrackType::Video:
            return SelectedVideoTrack;

        default:
            return INDEX_NONE;
    }
}

FText FOpenHarmonyMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
    switch (TrackType)
    {
        case EMediaTrackType::Audio:
            if (AudioTracks.IsValidIndex(TrackIndex))
            {
                return FText::FromString(AudioTracks[TrackIndex].DisplayName);
            }
            break;

        case EMediaTrackType::Caption:
            if (CaptionTracks.IsValidIndex(TrackIndex))
            {
                return FText::FromString(CaptionTracks[TrackIndex].DisplayName);
            }
            break;

        case EMediaTrackType::Video:
            if (VideoTracks.IsValidIndex(TrackIndex))
            {
                return FText::FromString(VideoTracks[TrackIndex].DisplayName);
            }

        default:
            break;
    }

    return FText::GetEmpty();
}

int32 FOpenHarmonyMediaPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
    return (GetSelectedTrack(TrackType) != INDEX_NONE) ? 0 : INDEX_NONE;
}

FString FOpenHarmonyMediaPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
    switch (TrackType)
    {
        case EMediaTrackType::Audio:
            if (AudioTracks.IsValidIndex(TrackIndex))
            {
                return AudioTracks[TrackIndex].Language;
            }
            break;

        case EMediaTrackType::Caption:
            if (CaptionTracks.IsValidIndex(TrackIndex))
            {
                return CaptionTracks[TrackIndex].Language;
            }
            break;

        case EMediaTrackType::Video:
            if (VideoTracks.IsValidIndex(TrackIndex))
            {
                return VideoTracks[TrackIndex].Language;
            }

        default:
            break;
    }

    return FString();
}

FString FOpenHarmonyMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
    switch (TrackType)
    {
        case EMediaTrackType::Audio:
            if (AudioTracks.IsValidIndex(TrackIndex))
            {
                return AudioTracks[TrackIndex].Name;
            }
            break;

        case EMediaTrackType::Caption:
            if (CaptionTracks.IsValidIndex(TrackIndex))
            {
                return CaptionTracks[TrackIndex].Name;
            }
            break;

        case EMediaTrackType::Video:
            if (VideoTracks.IsValidIndex(TrackIndex))
            {
                return VideoTracks[TrackIndex].Name;
            }

        default:
            break;
    }

    return FString();
}

bool FOpenHarmonyMediaPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
    if ((FormatIndex != 0) || !VideoTracks.IsValidIndex(TrackIndex))
    {
        return false;
    }

    const FVideoTrack Track = VideoTracks[TrackIndex];

    OutFormat.Dim = Track.Dimensions;
    OutFormat.FrameRate = Track.FrameRate;
    OutFormat.FrameRates = TRange<float>(Track.FrameRate);
    OutFormat.TypeName = TEXT("BGRA");

    return true;
}

bool FOpenHarmonyMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
    switch (TrackType)
    {
        case EMediaTrackType::Audio:
            if (TrackIndex != SelectedAudioTrack)
            {
                UE_LOG(LogOpenHarmonyMedia, Verbose, TEXT("Player %p: Selecting audio track %i instead of %i (%i tracks)"), this, TrackIndex, SelectedVideoTrack, AudioTracks.Num());

                if (TrackIndex == INDEX_NONE)
                {
                    UE_LOG(LogOpenHarmonyMedia, VeryVerbose, TEXT("Player %p: Disabling audio"), this, TrackIndex);
                }
                else
                {
                    if (!AudioTracks.IsValidIndex(TrackIndex))
                    {
                        return false;
                    }

                    UE_LOG(LogOpenHarmonyMedia, VeryVerbose, TEXT("Player %p: Enabling audio"), this, TrackIndex);
                }

                SelectedAudioTrack = TrackIndex;
            }
            break;

        case EMediaTrackType::Caption:
            if (TrackIndex != SelectedCaptionTrack)
            {
                UE_LOG(LogOpenHarmonyMedia, Verbose, TEXT("Player %p: Selecting caption track %i instead of %i (%i tracks)"), this, TrackIndex, SelectedCaptionTrack, CaptionTracks.Num());

                if (TrackIndex == INDEX_NONE)
                {
                    UE_LOG(LogOpenHarmonyMedia, VeryVerbose, TEXT("Player %p: Disabling captions"), this, TrackIndex);
                }
                else
                {
                    if (!CaptionTracks.IsValidIndex(TrackIndex))
                    {
                        return false;
                    }

                    UE_LOG(LogOpenHarmonyMedia, VeryVerbose, TEXT("Player %p: Enabling captions"), this, TrackIndex);
                }

                SelectedCaptionTrack = TrackIndex;
            }
            break;

        case EMediaTrackType::Video:
            if (TrackIndex != SelectedVideoTrack)
            {
                UE_LOG(LogOpenHarmonyMedia, Verbose, TEXT("Player %p: Selecting video track %i instead of %i (%i tracks)."), this, TrackIndex, SelectedVideoTrack, VideoTracks.Num());

                if (TrackIndex == INDEX_NONE)
                {
                    UE_LOG(LogOpenHarmonyMedia, VeryVerbose, TEXT("Player %p: Disabling video"), this, TrackIndex);
                }
                else
                {
                    if (!VideoTracks.IsValidIndex(TrackIndex))
                    {
                        return false;
                    }

                    UE_LOG(LogOpenHarmonyMedia, VeryVerbose, TEXT("Player %p: Enabling video"), this, TrackIndex);
                }

                SelectedVideoTrack = TrackIndex;
            }
            break;

        default:
            return false;
    }
    return true;
}

bool FOpenHarmonyMediaPlayer::GetVideoTracks(TArray<FVideoTrack>& VideoTracks_, RawFileDescriptor64& videoFD)
{
    VideoTracks_.Empty();
    int32_t trackCount = 0;
    int videoTrackCount = 0;
    OH_AVSource *source = OH_AVSource_CreateWithFD(videoFD.fd, videoFD.start, videoFD.length);
    OH_AVFormat *format = OH_AVSource_GetSourceFormat(source);
    int32_t trackType;
    OH_AVFormat_GetIntValue(format, OH_MD_KEY_TRACK_COUNT, &trackCount);
    for (uint32_t index = 0; index < (static_cast<uint32_t>(trackCount)); index++) {
        OH_AVFormat *trackFormat = OH_AVSource_GetTrackFormat(source, index);
        OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_TRACK_TYPE, &trackType);
        if (static_cast<OH_MediaType>(trackType) == OH_MediaType::MEDIA_TYPE_VID) {

            int32 VideoTrackIndex = VideoTracks_.AddDefaulted();
            FVideoTrack& VideoTrack = VideoTracks_[VideoTrackIndex];

            VideoTrack.Index = index;

            const char *mimeType;
            OH_AVFormat_GetStringValue(trackFormat, OH_MD_KEY_CODEC_MIME, &mimeType);
            VideoTrack.MimeType = FString(UTF8_TO_TCHAR(mimeType));

            VideoTrack.DisplayName = FString::Printf(TEXT("Video Track %d (Stream  %d)"), videoTrackCount, index);

            const char *language;
            OH_AVFormat_GetStringValue(trackFormat, OH_MD_KEY_LANGUAGE, &language);
            VideoTrack.Language = FString(UTF8_TO_TCHAR(language));
            int64_t bitRate;
            OH_AVFormat_GetLongValue(trackFormat, OH_MD_KEY_BITRATE, &bitRate);
            VideoTrack.BitRate = bitRate;
            int32_t Width = 0;
            int32_t Height = 0;
            OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_WIDTH, &Width);
            OH_AVFormat_GetIntValue(trackFormat, OH_MD_KEY_HEIGHT, &Height);
            VideoTrack.Dimensions = FIntPoint(Width, Height);

            double frameRate;
            OH_AVFormat_GetDoubleValue(trackFormat, OH_MD_KEY_FRAME_RATE, &frameRate);
            VideoTrack.FrameRate = frameRate;

            videoTrackCount++;
        }
    }
    return true;
}
bool FOpenHarmonyMediaPlayer::GetCaptionTracks(TArray<FCaptionTrack>& CaptionTracks_)
{
    // todo
    return false;
}
bool FOpenHarmonyMediaPlayer::GetAudioTracks(TArray<FAudioTrack>& AudioTracks_)
{
    // todo
    return false;
}
int FOpenHarmonyMediaPlayer::GetCurrentPosition()
{
    // todo: 判断prepare
    int position = 0;
    OH_AVPlayer_GetCurrentTime(AvPlayer.player, &position);
    return position;
}
bool FOpenHarmonyMediaPlayer::GetVideoLastFrameData(void* &outPixels, int64 &outCount)
{
    initBitmapRenderer();
    if (mBitmapRenderer != nullptr)
    {
        return mBitmapRenderer->updateFrameData(outPixels, outCount, &AvPlayer);
    }
    return false;
}
bool FOpenHarmonyMediaPlayer::GetVideoLastFrame(int32 DestTexture , OHAvPlayer* AvPlayer)
{
	initBitmapRenderer();
	if (mBitmapRenderer != nullptr)
	{
		return mBitmapRenderer->updateFrameData(DestTexture, AvPlayer);
		// =================================================================
	// --------- 在第600帧输出纹理为ppm图片, 验证外部纹理是否被正常更新 ---------
		
	}
	return false;
} //-_-
void FOpenHarmonyMediaPlayer::initBitmapRenderer()
{
    // if not already allocated.
    // Create bitmap renderer's gl resources in the renderer thread.
    if (mBitmapRenderer == nullptr)
    {
        CreateBitmapRenderer();
    }
}
bool FOpenHarmonyMediaPlayer::CreateBitmapRenderer()
{
    // todo:
    // releaseBitmapRenderer();

    mBitmapRenderer = new FOpenHarmonyBitmapRenderer(true, true, &AvPlayer);
    if (!mBitmapRenderer->isValid())
    {
        mBitmapRenderer = nullptr;
        return false;
    }
    // todo: OH_AVPlayer_GetVideoWidth 接口存在bug, 有时候返回值为0。暂时从videoTrack中获取
    // int32_t Width = 0;
    // int32_t Height = 0;
    // OH_AVPlayer_GetVideoWidth(AvPlayer.player, &Width);
    // OH_AVPlayer_GetVideoHeight(AvPlayer.player, &Height);
    // mBitmapRenderer->setSize(Width, Height);
    const FVideoTrack VideoTrack = VideoTracks[SelectedVideoTrack];
    mBitmapRenderer->setSize(VideoTrack.Dimensions.X, VideoTrack.Dimensions.Y);

    return true;
}

bool FOpenHarmonyMediaPlayer::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("hgy - SetTrackFormat"));
    return true;
}


/* IMediaControls interface
 *****************************************************************************/

bool FOpenHarmonyMediaPlayer::CanControl(EMediaControl Control) const
{
    if (Control == EMediaControl::Pause)
    {
        return (CurrentState == EMediaState::Playing);
    }

    if (Control == EMediaControl::Resume)
    {
        return ((CurrentState == EMediaState::Playing) || (CurrentState == EMediaState::Stopped));
    }

    if (Control == EMediaControl::Seek)
    {
        return ((CurrentState != EMediaState::Closed) && (CurrentState != EMediaState::Error));
    }

    return false;
}

FTimespan FOpenHarmonyMediaPlayer::GetDuration() const
{
    if (CurrentState == EMediaState::Error)
    {
        return FTimespan::Zero();
    }

    int duration = 0;
    OH_AVPlayer_GetDuration(AvPlayer.player, &duration);
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("GetDuration: %d"), duration);
    return FTimespan::FromMilliseconds(duration);
}

float FOpenHarmonyMediaPlayer::GetRate() const
{
    return (CurrentState == EMediaState::Playing) ? 1.0f : 0.0f;
}

EMediaState FOpenHarmonyMediaPlayer::GetState() const
{
    return CurrentState;
}

EMediaStatus FOpenHarmonyMediaPlayer::GetStatus() const
{
    return EMediaStatus::None;
}

TRangeSet<float> FOpenHarmonyMediaPlayer::GetSupportedRates(EMediaRateThinning /*Thinning*/) const
{
    TRangeSet<float> Result;

    Result.Add(TRange<float>(0.0f));
    Result.Add(TRange<float>(1.0f));

    return Result;
}

FTimespan FOpenHarmonyMediaPlayer::GetTime() const
{
    if ((CurrentState == EMediaState::Closed) || (CurrentState == EMediaState::Error))
    {
        return FTimespan::Zero();
    }

    int position = 0;
    OH_AVPlayer_GetCurrentTime(AvPlayer.player, &position);
    return FTimespan::FromMilliseconds(position);
}

bool FOpenHarmonyMediaPlayer::IsLooping() const
{
    return bLooping;
}

bool FOpenHarmonyMediaPlayer::Seek(const FTimespan& Time)
{
    UE_LOG(LogOpenHarmonyMedia, Verbose, TEXT("Player %p: Seeking to %s"), this, *Time.ToString());
    avComplete = false;
    if ((CurrentState == EMediaState::Closed) ||
        (CurrentState == EMediaState::Error) ||
        (CurrentState == EMediaState::Preparing))
    {
        UE_LOG(LogOpenHarmonyMedia, Warning, TEXT("Cannot seek while closed, preparing, or in error state"));
        return false;
    }

    OH_AVPlayer_Seek(AvPlayer.player, static_cast<int32>(Time.GetTotalMilliseconds()), AVPlayerSeekMode::AV_SEEK_NEXT_SYNC);
    EventSink.ReceiveMediaEvent(EMediaEvent::SeekCompleted);

    return true;
}

bool FOpenHarmonyMediaPlayer::SetLooping(bool Looping)
{
    OH_AVPlayer_SetLooping(AvPlayer.player, Looping);
    bLooping = Looping;
    return true;
}

bool FOpenHarmonyMediaPlayer::SetRate(float Rate)
{
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("SetRate, %f"), Rate);
    avComplete = false;
    if ((CurrentState == EMediaState::Closed) ||
        (CurrentState == EMediaState::Error) ||
        (CurrentState == EMediaState::Preparing))
    {
        UE_LOG(LogOpenHarmonyMedia, Warning, TEXT("Cannot set rate while closed, preparing, or in error state"));
        return false;
    }

    if (Rate == GetRate())
    {
        return true; // rate already set
    }

    UE_LOG(LogOpenHarmonyMedia, Verbose, TEXT("Player %p: Setting rate from to %f to %f"), this, GetRate(), Rate);

    if (Rate == 0.0f)
    {
        OH_AVPlayer_Pause(AvPlayer.player);
    }
    else if (Rate == 1.0f)
    {
        OH_AVPlayer_Play(AvPlayer.player);
    }
    else
    {
        UE_LOG(LogOpenHarmonyMedia, Warning, TEXT("The rate %f is not supported"), Rate);
        return false;
    }
    return true;
}

bool FOpenHarmonyMediaPlayer::SetNativeVolume(float Volume)
{
    Volume = Volume < 0.0f ? 0.0f : (Volume < 1.0f ? Volume : 1.0f);
    OH_AVPlayer_SetVolume(AvPlayer.player, Volume, Volume);
    return true;
}


/* FOpenHarmonyMediaPlayer callbacks
 *****************************************************************************/

void FOpenHarmonyMediaPlayer::HandleApplicationHasEnteredForeground()
{
    if (CurrentState == EMediaState::Playing)
    {
        entryBackground = true;
        OH_AVPlayer_Pause(AvPlayer.player);
    }
}

void FOpenHarmonyMediaPlayer::HandleApplicationWillEnterBackground()
{
    // check state in case changed before ticked
    if (CurrentState == EMediaState::Playing)
    {
        entryBackground = false;
        OH_AVPlayer_Play(AvPlayer.player);
    }
}

AVPlayerState FOpenHarmonyMediaPlayer::GetAVPlayerState()
{
    AVPlayerState result;
    OH_AVErrCode code = OH_AVPlayer_GetState(AvPlayer.player, &result);
    if(code != AV_ERR_OK)
    {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("GetAVPlayerState failed"));
    }
    return result;
}

bool FOpenHarmonyMediaPlayer::DidComplete()
{
    bool result = avComplete;
    avComplete = false;
    return result;
}

#undef LOCTEXT_NAMESPACE
