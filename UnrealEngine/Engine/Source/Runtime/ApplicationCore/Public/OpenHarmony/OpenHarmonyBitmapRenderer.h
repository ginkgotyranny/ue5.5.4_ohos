#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <string>
#include <cstring>

#include <native_image/native_image.h>
#include <native_window/external_window.h>
#include <native_buffer/native_buffer.h>

#include <napi/native_api.h>
#include <multimedia/player_framework/avplayer.h>
#include <multimedia/player_framework/avplayer_base.h>
#include "HAL/Platform.h"

#include "OpenHarmonyAvPlayer.h"


class FOpenHarmonyBitmapRenderer
{
public:
	EGLDisplay mEglDisplay = EGL_NO_DISPLAY;
	EGLContext mEglContext = EGL_NO_CONTEXT;
	EGLSurface mEglSurface = EGL_NO_SURFACE;

	EGLDisplay mSavedDisplay = EGL_NO_DISPLAY;
	EGLConfig mSavedContext = EGL_NO_CONTEXT;
	EGLSurface mSavedSurfaceDraw = EGL_NO_SURFACE;
	EGLSurface mSavedSurfaceRead = EGL_NO_SURFACE;
    
	// -------------------
	bool mUseMoviePlayer = false;
	bool mSwizzlePixels = true;
	bool mVulkanRenderer = true;
	GLuint mTextureID;
	GLuint mFBO;
	OH_NativeImage *mNativeImage;
	OHNativeWindow *mNativeWindow;
	float *mTriangleVertices;
	float mTriangleVerticesData[16] = {
		// X, Y, U, V
		-1.0f, -1.0f, 0.f, 0.f,
		1.0f, -1.0f, 1.f, 0.f,
		-1.0f, 1.0f, 0.f, 1.f,
		1.0f, 1.0f, 1.f, 1.f,
	};
	float mTransformMatrix[16];
	bool mTriangleVerticesDirty = true;
    
	GLuint mBlitVertexShaderID = -1;
	GLuint mBlitFragmentShaderID = -1;
	GLuint mProgram;
    
	GLuint mPositionAttrib;
	GLuint mTexCoordsAttrib;
	GLuint mBlitBuffer;
	GLuint mTextureUniform;
	int numFrameAvailable = 0;
	int mTextureWidth = -1;
	int mTextureHeight = -1;
	unsigned char *mFrameData = nullptr;

	// FOpenHarmonyBitmapRenderer(bool swizzlePixels, bool isVulkan, OHAvPlayer *player);

	FOpenHarmonyBitmapRenderer(bool swizzlePixels, bool isVulkan, OHAvPlayer *player, bool useMoviePlayer = false);
	

	void initContext();
	void saveContext();
	void makeCurrent();
	void restoreContext();
	void initSurfaceTexture(OHAvPlayer *player);
	bool copyFrameTexture(int destTexture, unsigned char *destData);
	void UpdateVertexData();

	bool updateFrameData(void* & outPixels, int64 & outCount, OHAvPlayer *player);
	bool updateFrameData(void* & outPixels, int64 & outCount);
	bool updateFrameData(int destTexture, OHAvPlayer *player);
	bool updateFrameData(int destTexture);
	void setSize(int width, int height);
	bool isValid(); 
};


