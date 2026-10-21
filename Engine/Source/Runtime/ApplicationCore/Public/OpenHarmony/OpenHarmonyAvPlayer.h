#pragma once
#include <napi/native_api.h>
#include <multimedia/player_framework/avplayer.h>
#include <multimedia/player_framework/avplayer_base.h>
#include <multimedia/player_framework/native_avsource.h>

#include <native_image/native_image.h>
#include <native_window/external_window.h>
#include <native_buffer/native_buffer.h>

#include "IMediaControls.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RHIUtilities.h"
//#include "OpenHarmony/OpenHarmonyBitmapRenderer.h"
struct OHAvPlayer
{
    OH_AVPlayer *player = nullptr;
    FTextureRHIRef videoTexture {nullptr};
    bool avCompleted = false;
    bool textureValid = false;
    OH_NativeImage *nativeImage = nullptr;
    OHNativeWindow *nativeWindow = nullptr;
    int32_t textureId = 0;
    int numFrameAvailable = 0;
public:
	int GetCurrentPosition()
	{
		// todo: 判断prepare
		int position = 0;
		OH_AVPlayer_GetCurrentTime(player, &position);
		return position;
	};

};


//class FOpenHarmonyAvPlayer
//{
//public:
//	FOpenHarmonyAvPlayer(bool SwizzlePixels);
//	~FOpenHarmonyAvPlayer();
//
//	void Play();
//	void Pause();
//	void Stop();
//	void Reset();
//	void Release();
//	void ClearAllData();
//	void ResetAvComplete();
//	bool GetAvComplete();
//	void SetLooping(bool Looping);
//	AVPlayerState GetAVPlayerState();
//
//	bool SetFDSource(const FString& Url);
//	bool SetFDSource(const int32 fd, int64 offset, int64 size);
//
//	void SetVideoTexture(FTextureRHIRef Texture);
//	FTextureRHIRef& GetVideoTexture();
//	void SetVolume(float Volume);
//	void Seek(int32 Time, AVPlayerSeekMode SeekMode);
//	int GetCurrentTime();
//	int GetDuration();
//	bool GetVideoLastFrame(int destTexture);
//	bool GetVideoLastFrameData(void*& outPixels, int64& outCount);
//	void initBitmapRenderer();
//	bool CreateBitmapRenderer();
//	bool Prepare();
//	bool CreateTextureAndPrepare(bool ShouldUseGlGen);
//	void SetDimensions(FIntPoint InDimensions);
//	void SetSize(int width, int height);
//
//	bool avCompleted = false;
//	bool textureValid = false;
//	OH_NativeImage* nativeImage = nullptr;
//	OHNativeWindow* nativeWindow = nullptr;
//	int32_t textureId = 0;
//	int numFrameAvailable = 0;
//	FIntPoint Dimensions;
//
//private:
//	bool mSwizzlePixels = false;
//	OH_AVPlayer* player = nullptr;
//	FTextureRHIRef videoTexture{ nullptr };
//
//	TSharedPtr<FOpenHarmonyBitmapRenderer, ESPMode::ThreadSafe> BitmapRenderer = nullptr;
//
//	/** Vulkan **/
//	void initContext();
//	void saveContext();
//	void makeCurrent();
//	void restoreContext();
//
//	EGLDisplay mEglDisplay = EGL_NO_DISPLAY;
//	EGLContext mEglContext = EGL_NO_CONTEXT;
//	EGLSurface mEglSurface = EGL_NO_SURFACE;
//
//	EGLDisplay mSavedDisplay = EGL_NO_DISPLAY;
//	EGLConfig mSavedContext = EGL_NO_CONTEXT;
//	EGLSurface mSavedSurfaceDraw = EGL_NO_SURFACE;
//	EGLSurface mSavedSurfaceRead = EGL_NO_SURFACE;
//};
