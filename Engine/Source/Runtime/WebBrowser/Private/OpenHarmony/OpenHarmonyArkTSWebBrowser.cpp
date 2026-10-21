// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyArkTSWebBrowser.h"
#include "aki/jsbind.h"

#if USE_OPENHARMONY_NAPI

#include "OpenHarmony/OpenHarmonyApplication.h"

FArkTSOpenHarmonyWebBrowser::FArkTSOpenHarmonyWebBrowser(int64_t viewTag, bool swizzlePixels, bool vulkanRenderer, int32 width, int32 height,
	bool bEnableRemoteDebugging, bool bUseTransparency, bool bEnableDomStorage, bool bShouldUseBitmapRender)
{
	VideoTexture = nullptr;
	bVideoTextureValid = false;

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("WebView FArkTSOpenHarmonyWebBrowser"));
	WebViewTag = viewTag;

	auto jsFunc = aki::JSBind::GetJSFunction("WebView.createWebView");
	jsFunc->Invoke<void>(WebViewTag);
}

FArkTSOpenHarmonyWebBrowser::~FArkTSOpenHarmonyWebBrowser()
{
}

void FArkTSOpenHarmonyWebBrowser::Release()
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.removeWebView");
	jsFunc->Invoke<void>(WebViewTag);
}

bool FArkTSOpenHarmonyWebBrowser::GetVideoLastFrameBitmap(void* outPixels, int64 outCount)
{
	// // This can return an exception in some cases
	// JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
	// auto Result = NewScopedJavaObject(JEnv, JEnv->CallObjectMethod(Object, GetVideoLastFrameBitmapMethod.Method));
	// if (JEnv->ExceptionCheck())
	// {
	// 	JEnv->ExceptionDescribe();
	// 	JEnv->ExceptionClear();
	// 	return false;
	// }
	//
	// if (!Result)
	// {
	// 	return false;
	// }
	//
	// auto bitmap = NewScopedJavaObject(JEnv, JEnv->GetObjectField(*Result, FrameUpdateInfo_Bitmap));
	// if (bitmap)
	// {
	// 	AndroidBitmapInfo BitmapInfo;
	//
	// 	AndroidBitmap_getInfo(JEnv, *bitmap, &BitmapInfo);
	// 	if (BitmapInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888)
	// 	{
	// 		return false;
	// 	}
	// 	int32 BitmapBytes = BitmapInfo.stride * BitmapInfo.height;
	//
	// 	BitmapBytes = FMath::Min(outCount, BitmapBytes);
	// 	if (BitmapBytes > 0)
	// 	{
	// 		void* BitmapDataPtr;
	//
	// 		if (AndroidBitmap_lockPixels(JEnv, *bitmap, &BitmapDataPtr) >= 0)
	// 		{
	// 			// unfortunately the order is wrong (RGBA instead of BGRA)
	// 			//memcpy(outPixels, BitmapDataPtr, BitmapBytes);
	// 			int32 NumPixels = BitmapBytes >> 2;
	// 			uint8* SrcData = (uint8*)BitmapDataPtr;
	// 			uint8* DstData = (uint8*)outPixels;
	// 			while (NumPixels--)
	// 			{
	// 				*DstData++ = SrcData[2];
	// 				*DstData++ = SrcData[1];
	// 				*DstData++ = SrcData[0];
	// 				*DstData++ = SrcData[3];
	// 				SrcData += 4;
	// 			}
	// 			AndroidBitmap_unlockPixels(JEnv, *bitmap);
	// 			return true;
	// 		}
	// 	}
	//
	// 	return false;
	// }

	return false;
}

bool FArkTSOpenHarmonyWebBrowser::GetVideoLastFrameData(void* & outPixels, int64 & outCount, bool *bRegionChanged)
{
	//// This can return an exception in some cases
	//JNIEnv*	JEnv = FOpenHarmonyApplication::GetJavaEnv();
	//auto Result = NewScopedJavaObject(JEnv, JEnv->CallObjectMethod(Object, GetVideoLastFrameDataMethod.Method));
	//if (JEnv->ExceptionCheck())
	//{
	//	JEnv->ExceptionDescribe();
	//	JEnv->ExceptionClear();
	//	*bRegionChanged = false;
	//	return false;
	//}

	//if (!Result)
	//{
	//	return false;
	//}

	//auto buffer = NewScopedJavaObject(JEnv, JEnv->GetObjectField(*Result, FrameUpdateInfo_Buffer));
	//if (buffer)
	//{
	//	bool bFrameReady = (bool)JEnv->GetBooleanField(*Result, FrameUpdateInfo_FrameReady);
	//	*bRegionChanged = (bool)JEnv->GetBooleanField(*Result, FrameUpdateInfo_RegionChanged);
	//	
	//	outPixels = JEnv->GetDirectBufferAddress(*buffer);
	//	outCount = JEnv->GetDirectBufferCapacity(*buffer);
	//	
	//	return !(nullptr == outPixels || 0 == outCount);
	//}
	
	return false;
}

bool FArkTSOpenHarmonyWebBrowser::DidResolutionChange()
{
	//return CallMethod<bool>(DidResolutionChangeMethod);
	return false;
}

bool FArkTSOpenHarmonyWebBrowser::UpdateVideoFrame(int32 ExternalTextureId, bool *bRegionChanged)
{
	//// This can return an exception in some cases
	//JNIEnv*	JEnv = FOpenHarmonyApplication::GetJavaEnv();
	//auto Result = NewScopedJavaObject(JEnv, JEnv->CallObjectMethod(Object, UpdateVideoFrameMethod.Method, ExternalTextureId));
	//if (JEnv->ExceptionCheck())
	//{
	//	JEnv->ExceptionDescribe();
	//	JEnv->ExceptionClear();
	//	*bRegionChanged = false;
	//	return false;
	//}

	//if (!Result)
	//{
	//	*bRegionChanged = false;
	//	return false;
	//}

	//bool bFrameReady = (bool)JEnv->GetBooleanField(*Result, FrameUpdateInfo_FrameReady);
	//*bRegionChanged = (bool)JEnv->GetBooleanField(*Result, FrameUpdateInfo_RegionChanged);
	//
	//return bFrameReady;
	return false;
}

bool FArkTSOpenHarmonyWebBrowser::GetVideoLastFrame(int32 destTexture)
{
	// This can return an exception in some cases
	//JNIEnv*	JEnv = FOpenHarmonyApplication::GetJavaEnv();
	//auto Result = NewScopedJavaObject(JEnv, JEnv->CallObjectMethod(Object, GetVideoLastFrameMethod.Method, destTexture));
	//if (JEnv->ExceptionCheck())
	//{
	//	JEnv->ExceptionDescribe();
	//	JEnv->ExceptionClear();
	//	return false;
	//}

	//if (!Result)
	//{
	//	return false;
	//}

	//bool bFrameReady = (bool)JEnv->GetBooleanField(*Result, FrameUpdateInfo_FrameReady);
	//
	//return bFrameReady;
	return false;
}

void FArkTSOpenHarmonyWebBrowser::Update(const int posX, const int posY, const int sizeX, const int sizeY)
{
	if (posX != lastPosX || posY != lastPosY || sizeX != lastSizeX || sizeY != lastSizeY)
	{
		auto jsFunc = aki::JSBind::GetJSFunction("WebView.setWebViewRect");
		jsFunc->Invoke<void>(WebViewTag, posX, posY, sizeX, sizeY);

		lastPosX = posX;
		lastPosY = posY;
		lastSizeX = sizeX;
		lastSizeY = sizeY;
	}
}

void FArkTSOpenHarmonyWebBrowser::ExecuteJavascript(const FString& Script)
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.evaluateJS");
	jsFunc->Invoke<void>(WebViewTag, TCHAR_TO_UTF8(*Script));
}

void FArkTSOpenHarmonyWebBrowser::LoadURL(const FString& NewURL)
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.loadURL");
	jsFunc->Invoke<void>(WebViewTag, TCHAR_TO_UTF8(*NewURL));
}

void FArkTSOpenHarmonyWebBrowser::LoadString(const FString& Contents, const FString& BaseUrl)
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.LoadString");
	jsFunc->Invoke<void>(WebViewTag, TCHAR_TO_UTF8(*Contents), TCHAR_TO_UTF8(*BaseUrl));
}

void FArkTSOpenHarmonyWebBrowser::StopLoad()
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.stopLoading");
	jsFunc->Invoke<void>(WebViewTag);
}

void FArkTSOpenHarmonyWebBrowser::Reload()
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.reload");
	jsFunc->Invoke<void>(WebViewTag);
}

void FArkTSOpenHarmonyWebBrowser::Close()
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.close");
	jsFunc->Invoke<void>(WebViewTag);
}

void FArkTSOpenHarmonyWebBrowser::GoBack()
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.goBack");
	jsFunc->Invoke<void>(WebViewTag);
}

void FArkTSOpenHarmonyWebBrowser::GoForward()
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.goForward");
	jsFunc->Invoke<void>(WebViewTag);
}

void FArkTSOpenHarmonyWebBrowser::SendTouchDown(float x, float y)
{

}

void FArkTSOpenHarmonyWebBrowser::SendTouchUp(float x, float y)
{

}

void FArkTSOpenHarmonyWebBrowser::SendTouchMove(float x, float y)
{

}

bool FArkTSOpenHarmonyWebBrowser::SendKeyDown(int32 KeyCode)
{
	return false;
}

bool FArkTSOpenHarmonyWebBrowser::SendKeyUp(int32 KeyCode)
{
	return false;
}

void FArkTSOpenHarmonyWebBrowser::SetOpenHarmony3DBrowser(bool InIsOpenHarmony3DBrowser)
{
}

void FArkTSOpenHarmonyWebBrowser::SetVisibility(bool InIsVisible)
{
	auto jsFunc = aki::JSBind::GetJSFunction("WebView.setVisible");
	jsFunc->Invoke<void>(WebViewTag, InIsVisible);
}

#endif // USE_OPENHARMONY_NAPI

