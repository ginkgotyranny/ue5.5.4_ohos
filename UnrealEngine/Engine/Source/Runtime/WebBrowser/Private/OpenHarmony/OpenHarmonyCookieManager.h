// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if USE_OPENHARMONY_NAPI
#include "IWebBrowserCookieManager.h"

/**
 * Implementation of interface for dealing with a Web Browser cookies for iOS.
 */
class FOpenHarmonyCookieManager
	: public IWebBrowserCookieManager
	, public TSharedFromThis<FOpenHarmonyCookieManager>
{
public:

	// IWebBrowserCookieManager interface

	virtual void SetCookie(const FString& URL, const FCookie& Cookie, TFunction<void(bool)> Completed = nullptr) override;
	virtual void DeleteCookies(const FString& URL = TEXT(""), const FString& CookieName = TEXT(""), TFunction<void(int)> Completed = nullptr) override;

	// FOpenHarmonyCookieManager

	FOpenHarmonyCookieManager();
	virtual ~FOpenHarmonyCookieManager();
};
#endif // USE_OPENHARMONY_NAPI
