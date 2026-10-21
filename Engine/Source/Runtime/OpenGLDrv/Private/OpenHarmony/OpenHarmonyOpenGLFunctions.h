// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OpenGLPlatform.h"

typedef khronos_stime_nanoseconds_t EGLnsecsOPENHARMONY;

typedef GLboolean(GL_APIENTRYP PFNeglPresentationTimeOPENHARMONY) (EGLDisplay dpy, EGLSurface surface, EGLnsecsOPENHARMONY time);
typedef GLboolean(GL_APIENTRYP PFNeglGetNextFrameIdOPENHARMONY) (EGLDisplay dpy, EGLSurface surface, EGLuint64KHR* frameId);
typedef GLboolean(GL_APIENTRYP PFNeglGetCompositorTimingOPENHARMONY) (EGLDisplay dpy, EGLSurface surface, EGLint numTimestamps, const EGLint* names, EGLnsecsOPENHARMONY* values);
typedef GLboolean(GL_APIENTRYP PFNeglGetFrameTimestampsOPENHARMONY) (EGLDisplay dpy, EGLSurface surface, EGLuint64KHR frameId, EGLint numTimestamps, const EGLint* timestamps, EGLnsecsOPENHARMONY* values);
typedef GLboolean(GL_APIENTRYP PFNeglQueryTimestampSupportedOPENHARMONY) (EGLDisplay dpy, EGLSurface surface, EGLint timestamp);

extern "C"
{
	extern PFNEGLGETSYSTEMTIMENVPROC eglGetSystemTimeNV_p;
	extern PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_p;
	extern PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_p;
	extern PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_p;
	extern PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR_p;

	extern PFNeglPresentationTimeOPENHARMONY eglPresentationTimeOPENHARMONY_p;
	extern PFNeglGetNextFrameIdOPENHARMONY eglGetNextFrameIdOPENHARMONY_p;
	extern PFNeglGetCompositorTimingOPENHARMONY eglGetCompositorTimingOPENHARMONY_p;
	extern PFNeglGetFrameTimestampsOPENHARMONY eglGetFrameTimestampsOPENHARMONY_p;
	extern PFNeglQueryTimestampSupportedOPENHARMONY eglQueryTimestampSupportedOPENHARMONY_p;
	extern PFNeglQueryTimestampSupportedOPENHARMONY eglGetCompositorTimingSupportedOPENHARMONY_p;
	extern PFNeglQueryTimestampSupportedOPENHARMONY eglGetFrameTimestampsSupportedOPENHARMONY_p;
}

namespace GLFuncPointers
{
	// GL_QCOM_shader_framebuffer_fetch_noncoherent
	extern PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC	glFramebufferFetchBarrierQCOM;
}

// FIXME: include gl32.h
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTUREPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level);

namespace GLFuncPointers
{
	// GL_EXT_multisampled_render_to_texture
	extern PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC	glFramebufferTexture2DMultisampleEXT;
	extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC	glRenderbufferStorageMultisampleEXT;

	// GL_EXT_debug_marker
	extern PFNGLPUSHGROUPMARKEREXTPROC		glPushGroupMarkerEXT;
	extern PFNGLPOPGROUPMARKEREXTPROC 		glPopGroupMarkerEXT;

	// GL_EXT_debug_label
	extern PFNGLLABELOBJECTEXTPROC			glLabelObjectEXT;
	extern PFNGLGETOBJECTLABELEXTPROC		glGetObjectLabelEXT;
	//GL_EXT_buffer_storage
	extern PFNGLBUFFERSTORAGEEXTPROC		glBufferStorageEXT;

	extern PFNGLDEBUGMESSAGECONTROLKHRPROC	glDebugMessageControlKHR;
	extern PFNGLDEBUGMESSAGEINSERTKHRPROC	glDebugMessageInsertKHR;
	extern PFNGLDEBUGMESSAGECALLBACKKHRPROC	glDebugMessageCallbackKHR;
	extern PFNGLGETDEBUGMESSAGELOGKHRPROC	glDebugMessageLogKHR;
	extern PFNGLGETPOINTERVKHRPROC			glGetPointervKHR;
	extern PFNGLPUSHDEBUGGROUPKHRPROC		glPushDebugGroupKHR;
	extern PFNGLPOPDEBUGGROUPKHRPROC		glPopDebugGroupKHR;
	extern PFNGLOBJECTLABELKHRPROC			glObjectLabelKHR;
	extern PFNGLGETOBJECTLABELKHRPROC		glGetObjectLabelKHR;
	extern PFNGLOBJECTPTRLABELKHRPROC		glObjectPtrLabelKHR;
	extern PFNGLGETOBJECTPTRLABELKHRPROC	glGetObjectPtrLabelKHR;

	// ES 3.2
	extern PFNGLTEXBUFFEREXTPROC				glTexBufferEXT;
	extern PFNGLTEXBUFFERRANGEEXTPROC			glTexBufferRangeEXT;
	extern PFNGLCOPYIMAGESUBDATAEXTPROC			glCopyImageSubData;
	extern PFNGLENABLEIEXTPROC					glEnableiEXT;
	extern PFNGLDISABLEIEXTPROC					glDisableiEXT;
	extern PFNGLBLENDEQUATIONIEXTPROC			glBlendEquationiEXT;
	extern PFNGLBLENDEQUATIONSEPARATEIEXTPROC	glBlendEquationSeparateiEXT;
	extern PFNGLBLENDFUNCIEXTPROC				glBlendFunciEXT;
	extern PFNGLBLENDFUNCSEPARATEIEXTPROC		glBlendFuncSeparateiEXT;
	extern PFNGLCOLORMASKIEXTPROC				glColorMaskiEXT;
	extern PFNGLFRAMEBUFFERTEXTUREPROC			glFramebufferTexture;

	// Mobile multi-view
	extern PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR;
	extern PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR;
};

using namespace GLFuncPointers;