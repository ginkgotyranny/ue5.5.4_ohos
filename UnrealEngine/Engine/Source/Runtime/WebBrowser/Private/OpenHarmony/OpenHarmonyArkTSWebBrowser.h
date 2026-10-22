// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if USE_OPENHARMONY_NAPI

#include "OpenHarmony/OpenHarmonyPlatform.h"
#include "RHI.h"
#include "RHIResources.h"

class FArkTSOpenHarmonyWebBrowser
{
public:
	FArkTSOpenHarmonyWebBrowser(int64_t viewTag, bool swizzlePixels, bool vulkanRenderer, int32 width, int32 height, bool bEnableRemoteDebugging, bool bUseTransparency, bool bEnableDomStorage, bool bShouldUseBitmapRender);
	virtual ~FArkTSOpenHarmonyWebBrowser();
	void Release();
	bool GetVideoLastFrameBitmap(void* outPixels, int64 outCount);
	bool GetVideoLastFrameData(void* & outPixels, int64 & outCount, bool *bRegionChanged);
	bool GetVideoLastFrame(int32 destTexture);
	bool DidResolutionChange();
	bool UpdateVideoFrame(int32 ExternalTextureId, bool *bRegionChanged);
	void ExecuteJavascript(const FString& Script);
	void LoadURL(const FString& NewURL);
	void LoadString(const FString& Contents, const FString& BaseUrl);
	void StopLoad();
	void Reload();
	void Close();
	void GoBack();
	void GoForward();
	void SendTouchDown(float x, float y);
	void SendTouchUp(float x, float y);
	void SendTouchMove(float x, float y);
	bool SendKeyDown(int32 KeyCode);
	bool SendKeyUp(int32 KeyCode);
	void SetOpenHarmony3DBrowser(bool InIsOpenHarmony3DBrowser);
	void SetVisibility(bool InIsVisible);
	void Update(const int posX, const int posY, const int sizeX, const int sizeY);
private:
	FTextureRHIRef VideoTexture;
	bool bVideoTextureValid;
	int64_t WebViewTag{ -1 };
	int lastPosX{ 0 };
	int lastPosY{ 0 };
	int lastSizeX{ 0 };
	int lastSizeY{ 0 };

public:
	FTextureRHIRef GetVideoTexture()
	{
		return VideoTexture;
	}

	void SetVideoTexture(FTextureRHIRef Texture)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Fetch RT: SetVideoTexture: %d"), Texture.IsValid());

		VideoTexture = Texture;
	}

	void SetVideoTextureValid(bool Condition)
	{
		bVideoTextureValid = Condition;
	}

	bool IsVideoTextureValid()
	{
		return bVideoTextureValid;
	}

};

#endif // USE_OPENHARMONY_NAPI
