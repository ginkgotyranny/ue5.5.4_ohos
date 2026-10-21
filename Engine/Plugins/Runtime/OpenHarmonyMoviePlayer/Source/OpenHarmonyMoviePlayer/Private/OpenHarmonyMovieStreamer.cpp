// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyMovieStreamer.h"
#include "OpenHarmony/OpenHarmonyApplication.h"
#include "OpenHarmony/OpenHarmonyPlatformFile.h"

#include "Rendering/RenderingCommon.h"
#include "RenderUtils.h"
#include "Slate/SlateTextures.h"
#include "MoviePlayer.h"
#include "Containers/StringConv.h"

#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "IPlatformFilePak.h"

#include <GLES3/gl31.h>
#include <string>


DEFINE_LOG_CATEGORY_STATIC(LogOpenHarmonyMoviePlayerStreamer, Log, All);

#define MOVIE_FILE_EXTENSION "mp4"

static bool avComplete = false;

FOpenHarmonyMoviePlayerStreamer::FOpenHarmonyMoviePlayerStreamer()
	: CurrentPosition(-1)
{
	avPlayer.player = OH_AVPlayer_Create(); // m_avPlayer.player = OH_AVPlayer_Create();
	
	// 设置 mediaplayer 播放回调（注意即使回调函数为空也要设置一下，不然OH_AVPlayer_GetState接口会一直返回IDLE。）
	avComplete = false;
	AVPlayerCallback callback;
	//定义两个回调函数
	callback.onInfo = [](OH_AVPlayer *player, AVPlayerOnInfoType type, int32_t extra)
	{
		if(type == AV_INFO_TYPE_EOS)
		{
			avComplete = true;
		}
	};

	callback.onError = [](OH_AVPlayer *player, int32_t errorCode, const char *errorMsg){ };;

	//AV_ERR_OK：成功设置播放器回调。
	//AV_ERR_INVALID_VAL：输入player为空指针、callback.onInfo或onError为空、player SetPlayerCallback执行失败
	int32_t ret = OH_AVPlayer_SetPlayerCallback(avPlayer.player, callback);

	if (ret != AV_ERR_OK) { 
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OH_AVPlayer_SetPlayerCallback failed"));
	}
	
	MovieViewport = MakeShareable(new FMovieViewport());
}

FOpenHarmonyMoviePlayerStreamer::~FOpenHarmonyMoviePlayerStreamer()
{
}

bool FOpenHarmonyMoviePlayerStreamer::Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> inPlaybackType)
{
	{
		FScopeLock Lock(&MovieQueueCriticalSection);
		MovieQueue.Append(MoviePaths);
	}
	
	return StartNextMovie();
}

void FOpenHarmonyMoviePlayerStreamer::ForceCompletion()
{
	CloseMovie();
}

static void DoUpdateTextureMovieSampleExecute(FOpenHarmonyBitmapRenderer *mBitmapRenderer, int32 DestTexture)
{
	mBitmapRenderer->updateFrameData(DestTexture);

}

struct FRHICommandUpdateTextureMovieSample final : public FRHICommand<FRHICommandUpdateTextureMovieSample>
{
	int32 DestTexture;
	FOpenHarmonyBitmapRenderer *mBitmapRenderer;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateTextureMovieSample(FOpenHarmonyBitmapRenderer *InBitmapRenderer, int32 InDestTexture)
		: mBitmapRenderer(InBitmapRenderer)
		, DestTexture(InDestTexture)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandUpdateTextureMovieSample_Execute);
		DoUpdateTextureMovieSampleExecute(mBitmapRenderer, DestTexture);
	}
};

bool FOpenHarmonyMoviePlayerStreamer::Tick(float DeltaTime)
{
	// Check the list of textures pending deletion and remove any that are no longer valid
	for (int32 TextureIndex = 0; TextureIndex < TexturesPendingDeletion.Num();)
	{
		TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> Tex = TexturesPendingDeletion[TextureIndex];
		if (!Tex->IsInitialized())
		{
			// Texture can now be removed from the list
			TexturesPendingDeletion.RemoveAt(TextureIndex);

			// Don't increment i, since element 'i' wasn't just filled by the next element
		}
		else
		{
			// Advance to the next one
			++TextureIndex;
		}
	}

	if (IsInRenderingThread())
	{
		FSlateTexture2DRHIRef* CurrentTexture = Texture.Get();

		if (!CurrentTexture->IsInitialized())
		{
			CurrentTexture->InitResource();
		}

		int32 DestTexture = *reinterpret_cast<int32_t*>(CurrentTexture->GetTypedResource().GetReference()->GetNativeResource());
		
		// Movie is on a different frame than what we have. We
		// need to read the new frame data and post it to Slate.

		if (!FOpenHarmonyMisc::ShouldUseVulkan())
		{
			if (mBitmapRenderer != nullptr)
			{
				if (IsRunningRHIInSeparateThread())
				{
					FRHICommandListImmediate &RHICommandList = GetImmediateCommandList_ForRenderCommand();
					new (RHICommandList.AllocCommand<FRHICommandUpdateTextureMovieSample>()) FRHICommandUpdateTextureMovieSample(mBitmapRenderer, DestTexture);
				}
				else
				{
					DoUpdateTextureMovieSampleExecute(mBitmapRenderer, DestTexture);
				}
			}
		}else{

			uint32 Stride = 0;
            int64 SampleCount = 0;
            void* LastFrameData = nullptr;
            // 获取视频的最后一帧数据
            mBitmapRenderer->updateFrameData(LastFrameData, SampleCount);
			
            // 锁定纹理以进行写入操作
            void* DestTextureData = RHILockTexture2D(CurrentTexture->GetTypedResource(), 0, RLM_WriteOnly, Stride, false);
            // 将最后一帧数据复制到纹理缓冲区
            FMemory::Memcpy(DestTextureData, LastFrameData, SampleCount);
            // 解锁纹理，完成写入操作
            RHIUnlockTexture2D(CurrentTexture->GetTypedResource(), 0, false);
		}
	}
	
	if (avComplete)
	{
		// Current movie finished playing. Clean up current movie and move on to
		// the next movie, if any.
		CloseMovie();
		if (!StartNextMovie())
		{
			// Failed to start the next movie, need to indicate that
			// we are done.
			return true;
		}
	}
	return false; //Not finished
}

TSharedPtr<class ISlateViewport> FOpenHarmonyMoviePlayerStreamer::GetViewportInterface()
{
	return MovieViewport;
}

float FOpenHarmonyMoviePlayerStreamer::GetAspectRatio() const
{
	return (float)MovieViewport->GetSize().X / (float)MovieViewport->GetSize().Y;
}

void FOpenHarmonyMoviePlayerStreamer::Cleanup()
{
}

bool FOpenHarmonyMoviePlayerStreamer::StartNextMovie()
{
	FString MoviePath;
	{
		FScopeLock QueueLock(&MovieQueueCriticalSection);
		if (MovieQueue.Num() == 0)
		{
			return false;
		}

		// Construct a canonical path for the movie.
		MoviePath
			= FPaths::ProjectContentDir() + FString("Movies/")
			+ MovieQueue[0] + FString(".") + FString(MOVIE_FILE_EXTENSION);
		FPaths::NormalizeFilename(MoviePath);
		MovieQueue.RemoveAt(0);
	}

	bool movieOK = true;

	IOpenHarmonyPlatformFile& PlatformFile = IOpenHarmonyPlatformFile::GetPlatformPhysical();

	extern NativeResourceManager* GNativeResourceManager;
	RawFileDescriptor64 videoFileDescriptor; //视频文件描述符
	// Don't bother trying to play it if we can't find it.
	if (!PlatformFile.FileExists(*MoviePath))
	{
		// possible it is in a PAK file
		FPakPlatformFile* PakPlatformFile = (FPakPlatformFile*)(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));

		//FPakFile* PakFile = NULL;
		TRefCountPtr<FPakFile> PakFile;
		FPakEntry FileEntry;
		if (PakPlatformFile == nullptr || !PakPlatformFile->FindFileInPakFiles(*MoviePath, &PakFile, &FileEntry))
		{
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

			int32 Handle = open(TCHAR_TO_UTF8(*FileRootPath), O_RDONLY);

			// possible PAK file in rawfile
			if(Handle == -1)
			{
				RawFile64 *Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_ANSI(*FileRootPath));
				if (Rfile != nullptr)
				{
					OH_ResourceManager_GetRawFileDescriptor64(Rfile, &videoFileDescriptor);
					Handle = videoFileDescriptor.fd;
					OH_ResourceManager_CloseRawFile64(Rfile);
				}
			}
			videoFileDescriptor.start = FileOffset;
			videoFileDescriptor.length = FileSize;
			
			int32_t ret = OH_AVPlayer_SetFDSource(avPlayer.player, Handle, FileOffset, FileSize);
			if (ret != AV_ERR_OK)
			{
				UE_LOG(LogOpenHarmonyMoviePlayerStreamer, Warning, TEXT("Failed to set data source for file in PAK %s"), *MoviePath);
				return false;
			}		
		}
		else
		{
			// int32_t ret = OH_AVPlayer_SetURLSource(avPlayer.player, *MoviePath);
			// if (ret != AV_ERR_OK)
			// {
			// 	UE_LOG(LogOpenHarmonyMoviePlayerStreamer, Warning, TEXT("Failed to set data source for archive %s"), *FilePath);
			// 	return false;
			// }
		}
	}
	else
	{
		FString FileRootPath = PlatformFile.FileRootPath(*MoviePath);
		RawFile64 *Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_ANSI(*FileRootPath));
		OH_ResourceManager_GetRawFileDescriptor64(Rfile, &videoFileDescriptor);
		OH_ResourceManager_CloseRawFile64(Rfile);
		int32_t ret = OH_AVPlayer_SetFDSource(avPlayer.player, videoFileDescriptor.fd, videoFileDescriptor.start, videoFileDescriptor.length);
		if (ret != AV_ERR_OK)
		{
			UE_LOG(LogOpenHarmonyMoviePlayerStreamer, Error, TEXT("avPlayer datasource failed, error code: %d"), ret);
			return false;
		}
	}

	FIntPoint VideoDimensions;
	if (movieOK)
	{
		FString Info;
		GetVideoTracks(VideoTracks, videoFileDescriptor);
		for (FVideoTrack Track : VideoTracks)
		{
			VideoDimensions.X = Track.Dimensions.X;
			VideoDimensions.Y = Track.Dimensions.Y;
		}

		movieOK = VideoDimensions != FIntPoint::ZeroValue;
	}
	if (movieOK)
	{
		Texture = MakeShareable(new FSlateTexture2DRHIRef(
			VideoDimensions.X, VideoDimensions.Y,
			PF_B8G8R8A8,
			NULL,
			TexCreate_RenderTargetable,
			true));

		uint32 FrameBytes = VideoDimensions.X * VideoDimensions.Y * GPixelFormats[PF_B8G8R8A8].BlockBytes;

		FSlateTexture2DRHIRef* TextureRHIRef = Texture.Get();
		ENQUEUE_RENDER_COMMAND(InitMovieTexture)(
			[TextureRHIRef, FrameBytes](FRHICommandListImmediate& RHICmdList)
			{
				TextureRHIRef->InitResource();
				// clear texture to black
				uint32 Stride = 0;
				void* TextureBuffer = RHILockTexture2D(TextureRHIRef->GetTypedResource(), 0, RLM_WriteOnly, Stride, false);
				FMemory::Memset(TextureBuffer, 0, FrameBytes);
				RHIUnlockTexture2D(TextureRHIRef->GetTypedResource(), 0, false);
			});

		MovieViewport->SetTexture(Texture);
		if (mBitmapRenderer == nullptr)
		{
			bool useMoviePlayer = true;
			mBitmapRenderer = new FOpenHarmonyBitmapRenderer(true, FOpenHarmonyMisc::ShouldUseVulkan(), &avPlayer, useMoviePlayer);
			if (!mBitmapRenderer->isValid())
			{
				mBitmapRenderer = nullptr;
				return false;
			}
			mBitmapRenderer->setSize(VideoDimensions.X, VideoDimensions.Y);
		} else {
			mBitmapRenderer->setSize(VideoDimensions.X, VideoDimensions.Y);
			OH_AVPlayer_Prepare(avPlayer.player);
		}
	}

	if (movieOK)
	{
		avComplete = false;
		OH_AVPlayer_Play(avPlayer.player);
	} else {
		UE_LOG(LogOpenHarmonyMoviePlayerStreamer, Log, TEXT("reset in start"));
		OH_AVPlayer_Reset(avPlayer.player);
		OH_NativeImage_Destroy(&avPlayer.nativeImage);
		avPlayer.nativeImage = nullptr;
		avPlayer.nativeWindow = nullptr;
		avPlayer.textureValid = false;
		avPlayer.videoTexture = nullptr;
	}
	return movieOK;
}

void FOpenHarmonyMoviePlayerStreamer::CloseMovie()
{
	OH_AVPlayer_Stop(avPlayer.player);
	OH_AVPlayer_Reset(avPlayer.player);

	if (Texture.IsValid())
	{
		TexturesPendingDeletion.Add(Texture);

		MovieViewport->SetTexture(NULL);
		Texture.Reset();
	}
}

FString FOpenHarmonyMoviePlayerStreamer::GetMovieName()
{
	return MovieQueue.Num() > 0 ? MovieQueue[0] : TEXT("");
}

bool FOpenHarmonyMoviePlayerStreamer::IsLastMovieInPlaylist()
{
	return MovieQueue.Num() <= 1;
}

int FOpenHarmonyMoviePlayerStreamer::GetCurrentPosition()
{
	// todo: 判断prepare
	int position = 0;
	OH_AVPlayer_GetCurrentTime(avPlayer.player, &position);
	return position;
}

void FOpenHarmonyMoviePlayerStreamer::initBitmapRenderer()
{
	// if not already allocated.
	// Create bitmap renderer's gl resources in the renderer thread.
	if (mBitmapRenderer == nullptr)
	{
		CreateBitmapRenderer();
	}
}

bool FOpenHarmonyMoviePlayerStreamer::CreateBitmapRenderer()
{
	// todo:
	// releaseBitmapRenderer();
	
	mBitmapRenderer = new FOpenHarmonyBitmapRenderer(true, FOpenHarmonyMisc::ShouldUseVulkan(), &avPlayer);
	if (!mBitmapRenderer->isValid())
	{
		mBitmapRenderer = nullptr;
		return false;
	}
	// todo: OH_AVPlayer_GetVideoWidth 接口存在bug, 有时候返回值为0。暂时从videoTrack中获取
	// int32_t Width = 0;
	// int32_t Height = 0;
	// OH_AVPlayer_GetVideoWidth(avPlayer.player, &Width);
	// OH_AVPlayer_GetVideoHeight(avPlayer.player, &Height);
	// mBitmapRenderer->setSize(Width, Height);
	// mBitmapRenderer->setSize(VideoTrack.Dimensions.X, VideoTrack.Dimensions.Y);
	return true;
}

bool FOpenHarmonyMoviePlayerStreamer::GetVideoTracks(TArray<FVideoTrack>& VideoTracks, RawFileDescriptor64& videoFD)
{
	VideoTracks.Empty();
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
			
			int32 VideoTrackIndex = VideoTracks.AddDefaulted();
			FVideoTrack& VideoTrack = VideoTracks[VideoTrackIndex];
			
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