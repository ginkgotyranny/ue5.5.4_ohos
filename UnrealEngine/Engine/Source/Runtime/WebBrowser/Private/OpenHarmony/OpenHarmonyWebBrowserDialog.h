// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if USE_OPENHARMONY_NAPI

#include "IWebBrowserDialog.h"

class SOpenHarmonyWebBrowserWidget;

class FOpenHarmonyWebBrowserDialog
	: public IWebBrowserDialog
{
public:
	virtual ~FOpenHarmonyWebBrowserDialog()
	{}

	// IWebBrowserDialog interface:

	virtual EWebBrowserDialogType GetType() override
	{
		return Type;
	}

	virtual const FText& GetMessageText() override
	{
		return MessageText;
	}

	virtual const FText& GetDefaultPrompt() override
	{
		return DefaultPrompt;
	}

	virtual bool IsReload() override
	{
		check(Type == EWebBrowserDialogType::Unload);
		return false; // The OpenHarmony webkit browser does not provide this infomation
	}

	virtual void Continue(bool Success = true, const FText& UserResponse = FText::GetEmpty()) override;

private:

	EWebBrowserDialogType Type;
	FText MessageText;
	FText DefaultPrompt;

	//jobject Callback; // Either a reference to a JsResult or a JsPromptResult object depending on Type

	// Create a dialog from OnJSPrompt arguments
	FOpenHarmonyWebBrowserDialog(/*jstring InMessageText, jstring InDefaultPrompt, jobject InCallback*/);

	// Create a dialog from OnJSAlert|Confirm|BeforeUnload arguments
	//FOpenHarmonyWebBrowserDialog(EWebBrowserDialogType InDialogType, jstring InMessageText, jobject InCallback);

	friend class FOpenHarmonyWebBrowserWindow;
	friend class SOpenHarmonyWebBrowserWidget;
};

typedef FOpenHarmonyWebBrowserDialog FWebBrowserDialog;

#endif // USE_OPENHARMONY_NAPI