#include "OpenHarmony/OpenHarmonyBitmapRenderer.h"

const char mBlitVextexShader[] = "attribute vec2 Position;\n"
                                 "attribute vec2 TexCoords;\n"
                                 "varying vec2 TexCoord;\n"
                                 "void main() {\n"
                                 "	TexCoord = TexCoords;\n"
                                 "	gl_Position = vec4(Position, 0.0, 1.0);\n"
                                 "}\n";

const char mBlitFragmentShaderBGRA[] = "#extension GL_OES_EGL_image_external : require\n"
                                       "uniform samplerExternalOES VideoTexture;\n"
                                       "varying highp vec2 TexCoord;\n"
                                       "void main()\n"
                                       "{\n"
                                       "	gl_FragColor = texture2D(VideoTexture, TexCoord).bgra;\n"
                                       "}\n";

const char mBlitFragmentShaderRGBA[] = "#extension GL_OES_EGL_image_external : require\n"
                                       "uniform samplerExternalOES VideoTexture;\n"
                                       "varying highp vec2 TexCoord;\n"
                                       "void main()\n"
                                       "{\n"
                                       "	gl_FragColor = texture2D(VideoTexture, TexCoord).rgba;\n"
                                       "}\n";
DEFINE_LOG_CATEGORY_STATIC(LogOpenHarmonyBitmapRenderer, Log, All);

FOpenHarmonyBitmapRenderer::FOpenHarmonyBitmapRenderer(bool swizzlePixels, bool isVulkan, OHAvPlayer *player, bool useMoviePlayer)
{
	mSwizzlePixels = false;
	if (FOpenHarmonyMisc::ShouldUseVulkan()) {
		mSwizzlePixels = swizzlePixels;
	}
    mVulkanRenderer = isVulkan;
	mUseMoviePlayer = useMoviePlayer;
    initContext();
    saveContext();
    makeCurrent();
    initSurfaceTexture(player);
    restoreContext();
}

void FOpenHarmonyBitmapRenderer::initContext() {
    // 1
    mEglDisplay = EGL_NO_DISPLAY;
    EGLContext shareContext = EGL_NO_CONTEXT;
    EGLint majorVersion;
    EGLint minorVersion;
    if (!mVulkanRenderer) {
        mEglDisplay = eglGetCurrentDisplay();
        shareContext = eglGetCurrentContext();

        // on some devices eg Galaxy S6, the above fails but we do get EGL14.EGL_CONTEXT_CLIENT_VERSION=3
        if (eglQueryContext(mEglDisplay, shareContext, EGL_CONTEXT_CLIENT_VERSION, &majorVersion))
        {
            // UE_LOG(LogOpenHarmonyBitmapRendererm, Log, TEXT("Existing GL context is version = %d"), majorVersion);
        }
        else
        {
            // UE_LOG(LogOpenHarmonyBitmapRendererm, Log, TEXT("Existing GL context version not detected"));
        }
    } else {
        mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(mEglDisplay, &majorVersion, &minorVersion);
    }

    // 2 创建一个配置规范configSpec，指定要创建的EGL配置的属性，每行是一个键值对，EGL_NONE是终止符
    EGLint configSpec[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_NONE
    };

    // 3 使从mEglDisplay中选择符合要求的EGL配置，结果存储在configs数组中，num_config用来存储满足条件的配置数量。
    EGLConfig configs[1];
    EGLint num_config;
    eglChooseConfig(mEglDisplay, configSpec, configs, 1, &num_config);

    // 4 创建两种上下文属性数组contextAttribsES2和contextAttribsES31，分别表示OpenGL ES 2.0和OpenGL ES 3.1的上下文属性。
    // 根据majorver[0]的值选择合适的上下文属性，调用EGL14.eglCreateContext创建EGL上下文。
    EGLint contextAttribsES2[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };
    EGLint contextAttribsES31[] = {
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_NONE
    };
    mEglContext = eglCreateContext(mEglDisplay, configs[0], shareContext, (majorVersion == 3) ? contextAttribsES31 : contextAttribsES2);

    // 5 检查EGL扩展字符串是否包含EGL_KHR_surfaceless_context，如果包含，表示支持无表面上下文，设置mEglSurface为EGL_NO_SURFACE。
    // 否则，创建一个属性数组pbufferAttribs，并调用eglCreatePbufferSurface创建一个Pbuffer表面，存储在mEglSurface中。
    const char *extensions = eglQueryString(mEglDisplay, EGL_EXTENSIONS);
    EGLSurface mEglSurface;

    if (strstr(extensions, "EGL_KHR_surfaceless_context")) {
        mEglSurface = EGL_NO_SURFACE;
    }
    else {
        EGLint pbufferAttribs[] = {EGL_NONE};
        mEglSurface = eglCreatePbufferSurface(mEglDisplay, configs[0], pbufferAttribs);
    }
}

void FOpenHarmonyBitmapRenderer::saveContext() {
    mSavedDisplay = eglGetCurrentDisplay();
    mSavedContext = eglGetCurrentContext();
    mSavedSurfaceDraw = eglGetCurrentSurface(EGL_DRAW);
    mSavedSurfaceRead = eglGetCurrentSurface(EGL_READ);
}

void FOpenHarmonyBitmapRenderer::makeCurrent() {
    // 将特定的上下文和表面设为当前线程的当前上下文和当前表面，以便在后续的 OpenGL ES 函数调用中使用。
    // 通常在初始化完成上下文和表面后调用，以确保后续的 OpenGL ES 渲染操作都能正确作用在当前上下文和表面上。
    eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext);
}

void FOpenHarmonyBitmapRenderer::restoreContext() {
    eglMakeCurrent(mSavedDisplay, mSavedSurfaceDraw, mSavedSurfaceRead, mSavedContext);
}

GLuint createShader(GLenum shaderType, const char *source) {
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            char *infoLog = new char[infoLen];
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
            FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Error compiling shader:\n%s\n"), infoLog);
            delete[] infoLog;
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void FOpenHarmonyBitmapRenderer::initSurfaceTexture(OHAvPlayer *player) {
	if(mUseMoviePlayer){ //movie player 
		// 生成一个纹理ID; 创建一个 NativeImage(对应SurfaceTexture), 绑定到生成的纹理ID上; 
        // 设置帧可用监听器; 创建一个(对应Surface)对象。
		glGenTextures(1, &mTextureID);
		mNativeImage = OH_NativeImage_Create(mTextureID, GL_TEXTURE_EXTERNAL_OES);
		OH_NativeImage_SetOnFrameAvailableListener(mNativeImage, {
			this, [] (void *context) {
				FOpenHarmonyBitmapRenderer *mBitmapRenderer = static_cast<FOpenHarmonyBitmapRenderer*>(context);
				mBitmapRenderer->numFrameAvailable++;
			}
		});
		mNativeWindow = OH_NativeImage_AcquireNativeWindow(mNativeImage);
		int ret = OH_AVPlayer_SetVideoSurface(player->player, mNativeWindow);
	    if (ret != AV_ERR_OK) {
		    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OH_AVPlayer_SetVideoSurface failed"));
	    }
	    OH_AVPlayer_Prepare(player->player);
	    player->textureValid = true;
	}else{ //media player 
		mTextureID = player->textureId;
        numFrameAvailable = player->numFrameAvailable;
        mNativeImage = player->nativeImage;
        mNativeWindow = player->nativeWindow;
	}
    // 生成一个帧缓冲区对象
    glGenFramebuffers(1, &mFBO);

    // Special shaders for blit of movie texture. 编译着色器, 创建OpenGL程序
    mBlitVertexShaderID = createShader(GL_VERTEX_SHADER, mBlitVextexShader);
    if (mBlitVertexShaderID == 0) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("mBlitVertexShaderID == 0"));
    }

    mBlitFragmentShaderID = createShader(GL_FRAGMENT_SHADER, mSwizzlePixels ? mBlitFragmentShaderBGRA : mBlitFragmentShaderRGBA);
    if (mBlitFragmentShaderID == 0) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("mBlitFragmentShaderID == 0"));
    }

    mProgram = glCreateProgram();
    if (mProgram <= 0) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("mProgram <= 0"));
    }

    glAttachShader(mProgram, mBlitVertexShaderID);
    glAttachShader(mProgram, mBlitFragmentShaderID);
    glLinkProgram(mProgram);

    GLint linkStatus[1];
    glGetProgramiv(mProgram, GL_LINK_STATUS, linkStatus);
    if (linkStatus[0] != GL_TRUE) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Could not link program: "));
    }

    // 获取顶点位置、纹理坐标和纹理uniform的位置。
    mPositionAttrib = glGetAttribLocation(mProgram, "Position");
    mTexCoordsAttrib = glGetAttribLocation(mProgram, "TexCoords");
    mTextureUniform = glGetUniformLocation(mProgram, "VideoTexture");

    // 生成一个VBO，并保存其ID。
    glGenBuffers(1, &mBlitBuffer);
    if (mBlitBuffer <= 0) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("mBlitBuffer <= 0"));
    }

    // Create blit mesh.
    // 创建一个FloatBuffer，用于存储顶点数据。
    mTriangleVertices = new float[16];
    mTriangleVerticesDirty = true;

    // Set up GL state
    // 禁用一些OpenGL功能，如混合、剔除、剪裁测试、模板测试、深度测试和抖动; 启用颜色掩码。
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

bool FOpenHarmonyBitmapRenderer::updateFrameData(void* & outPixels, int64 & outCount, OHAvPlayer *player)
{
    if (player->numFrameAvailable <= 0) {
		
        return false;
    }
    player->numFrameAvailable --;

    outCount = 4 * mTextureWidth * mTextureHeight;
    if (mFrameData == nullptr) {
        mFrameData = new unsigned char[outCount];
    }
    copyFrameTexture(0, mFrameData);
    outPixels = mFrameData;
    return true;
}
bool FOpenHarmonyBitmapRenderer::updateFrameData(void* & outPixels, int64 & outCount)
{
    if (numFrameAvailable <= 0) {
        return false;
    }
    numFrameAvailable --;

    outCount = 4 * mTextureWidth * mTextureHeight;
    if (mFrameData == nullptr) {
        mFrameData = new unsigned char[outCount];
    }
    copyFrameTexture(0, mFrameData);
    outPixels = mFrameData;
    return true;
}

bool FOpenHarmonyBitmapRenderer::updateFrameData(int destTexture, OHAvPlayer *player)
{
    if (player->numFrameAvailable <= 0) {
        return false;
    }
    player->numFrameAvailable --;

    copyFrameTexture(destTexture, nullptr);
    return true;
}

bool FOpenHarmonyBitmapRenderer::updateFrameData(int destTexture)
{
	if (numFrameAvailable <= 0) {
		return false;
	}
	numFrameAvailable--;
	copyFrameTexture(destTexture, nullptr);
	return true;
}

bool FOpenHarmonyBitmapRenderer::copyFrameTexture(int destTexture, unsigned char *destData) {


    // todo: 好像不需要 - 获取当前帧的位置并更新 mLastFramePosition
    // int current_frame_position = getCurrentPosition();
    // mLastFramePosition = current_frame_position;
    if(mNativeImage == nullptr) {
        return false;
    }

    // Either use own context or save states
    bool previousBlend = false, previousCullFace = false, previousScissorTest = false,
            previousStencilTest = false, previousDepthTest = false, previousDither = false;
    GLint previousFBO = 0, previousVBO = 0, previousMinFilter = 0, previousMagFilter = 0;
    int previousViewport[4];

    glActiveTexture(GL_TEXTURE0);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &previousMinFilter);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &previousMagFilter);

    saveContext();
    makeCurrent();

    int ret = OH_NativeImage_UpdateSurfaceImage(mNativeImage);
    if (ret != 0) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OH_NativeImage_UpdateSurfaceImage failed"));
    }

    // todo: 更新视频帧的矩阵(发生在横竖屏切换、缩放等)。
    // OH_NativeImage_GetTransformMatrix(mNativeImage, mTransformMatrix);
    // float UMin = mTransformMatrix[12];
    // float UMax = UMin + mTransformMatrix[0];
    // float VMin = mTransformMatrix[13];
    // float VMax = VMin + mTransformMatrix[5];
    //
    // if (mTriangleVerticesData[2] != UMin ||
    // 	mTriangleVerticesData[6] != UMax ||
    // 	mTriangleVerticesData[11] != VMin ||
    // 	mTriangleVerticesData[3] != VMax)
    // {
    // 	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("NativeImage: update Transform Matrix."));
    // 	mTriangleVerticesData[ 2] = mTriangleVerticesData[10] = UMin;
    // 	mTriangleVerticesData[ 6] = mTriangleVerticesData[14] = UMax;
    // 	mTriangleVerticesData[11] = mTriangleVerticesData[15] = VMin;
    // 	mTriangleVerticesData[ 3] = mTriangleVerticesData[ 7] = VMax;
    // 	mTriangleVerticesDirty = true;
    // }

    glViewport(0, 0, mTextureWidth, mTextureHeight);

    // todo: glVerify

    // 设置目标纹理和帧缓冲区对象(FBO)。如果传入了缓冲区数据 destData，创建一个临时的纹理和 FBO，用于读取纹理数据。
    GLuint FBOTextureID = 0;
    if (destData != nullptr) {
        glGenTextures(1, &FBOTextureID);
    } else {
        // Use the given texture as the FBO.
        FBOTextureID = destTexture;
    }

    // Set the FBO to draw into the texture one-to-one.
    // 绑定 FBO 并将视频帧渲染到 FBO。
    glBindTexture(GL_TEXTURE_2D, FBOTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (destData != nullptr) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mTextureWidth, mTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBOTextureID, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to complete framebuffer attachment (%d)"), status);
    }

    glUseProgram(mProgram);
    UpdateVertexData();


    glBindBuffer(GL_ARRAY_BUFFER, mBlitBuffer);
    glEnableVertexAttribArray(mPositionAttrib);
    glVertexAttribPointer(mPositionAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, 0);
    glEnableVertexAttribArray(mTexCoordsAttrib);
    glVertexAttribPointer(mTexCoordsAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void *)(sizeof(float) * 2));

    glClear(GL_COLOR_BUFFER_BIT);

    // connect 'VideoTexture' to video source texture (mTextureID).
    // mTextureID is bound to GL_TEXTURE_EXTERNAL_OES in updateTexImage
    glUniform1i(mTextureUniform, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, mTextureID);

    // Draw the video texture mesh.
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFlush();

    if (destData != nullptr) {
        glReadPixels(0, 0, mTextureWidth, mTextureHeight, GL_RGBA, GL_UNSIGNED_BYTE, destData);
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    if (destData != nullptr && FBOTextureID > 0) {
        glDeleteTextures(1, &FBOTextureID);
    }

    restoreContext();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, previousMinFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, previousMagFilter);

    return true;
}

void FOpenHarmonyBitmapRenderer::UpdateVertexData()
{
    if (!mTriangleVerticesDirty || mBlitBuffer <= 0)
    {
        return;
    }
    memcpy(mTriangleVertices, mTriangleVerticesData, 16 * sizeof(float));

    // 保存当前的 VBO 状态
    GLint previousVBO;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousVBO);

    // 绑定当前 VBO 并更新数据
    glBindBuffer(GL_ARRAY_BUFFER, mBlitBuffer);
    glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), mTriangleVertices, GL_STATIC_DRAW);

    // 恢复之前的 VBO 状态
    glBindBuffer(GL_ARRAY_BUFFER, previousVBO);

    // 标记顶点数据已更新
    mTriangleVerticesDirty = false;
}

void FOpenHarmonyBitmapRenderer::setSize(int width, int height)
{
    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("setsize: width: %d, height: %d"), width, height);
    if (width != mTextureWidth || height != mTextureHeight)
    {
        mTextureWidth = width;
        mTextureHeight = height;
        mFrameData = nullptr;
    }
}

bool FOpenHarmonyBitmapRenderer::isValid()
{
    return mNativeImage != nullptr;
}

// todo: release

