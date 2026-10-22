// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmonyWebBrowserWindow.h"

#if USE_OPENHARMONY_NAPI

#include "OpenHarmonyWebBrowserDialog.h"
#include "OpenHarmonyWebBrowserWidget.h"
#include "OpenHarmony/OpenHarmonyApplication.h"
#include "OpenHarmony/OpenHarmonyWindow.h"
#include "Misc/ConfigCacheIni.h"


namespace {

	static const FString JSGetSourceCommand = TEXT("GetSource");
	static const FString JSMessageGetSourceScript =
		TEXT("document.location = '") + FMobileJSScripting::JSMessageTag + JSGetSourceCommand +
		TEXT("/' + encodeURIComponent(document.documentElement.innerHTML);");

}

FOpenHarmonyWebBrowserWindow::FOpenHarmonyWebBrowserWindow(FString InUrl, TOptional<FString> InContentsToLoad, bool InShowErrorMessage, bool InThumbMouseButtonNavigation, bool InUseTransparency, bool bInJSBindingToLoweringEnabled)
	: CurrentUrl(MoveTemp(InUrl))
	, ContentsToLoad(MoveTemp(InContentsToLoad))
	, bUseTransparency(InUseTransparency)
	, DocumentState(EWebBrowserDocumentState::NoDocument)
	, ErrorCode(0)
	, Scripting(new FMobileJSScripting(bInJSBindingToLoweringEnabled))
	, OpenHarmonyWindowSize(FIntPoint(1024, 768))
	, bIsDisabled(false)
	, bIsVisible(true)
	, bTickedLastFrame(true)
{
	// Deal with optional texture resolution override
	FString WebViewTextureSize;
	if (GConfig->GetString(TEXT("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings"), TEXT("WebViewTextureSize"), WebViewTextureSize, GEngineIni))
	{
		TArray<FString> ResolutionVector;
		int ParseCount = WebViewTextureSize.ParseIntoArray(ResolutionVector, TEXT(","), false);
		ensureMsgf(ParseCount == 2, TEXT("WebViewTextureSize variable not properly formatted."));

		if (ParseCount == 2)
		{
			int32 Width = FCString::Atof(*ResolutionVector[0]);
			int32 Height = FCString::Atof(*ResolutionVector[1]);

			OpenHarmonyWindowSize.X = FMath::Max(1, Width);
			OpenHarmonyWindowSize.Y = FMath::Max(1, Height);
		}
	}
}

FOpenHarmonyWebBrowserWindow::~FOpenHarmonyWebBrowserWindow()
{
	CloseBrowser(true, false);
}

void FOpenHarmonyWebBrowserWindow::LoadURL(FString NewURL)
{
	BrowserWidget->LoadURL(NewURL);
}

void FOpenHarmonyWebBrowserWindow::LoadString(FString Contents, FString DummyURL)
{
	BrowserWidget->LoadString(Contents, DummyURL);
}

TSharedRef<SWidget> FOpenHarmonyWebBrowserWindow::CreateWidget()
{
	TSharedRef<SOpenHarmonyWebBrowserWidget> BrowserWidgetRef =
		SNew(SOpenHarmonyWebBrowserWidget)
		.UseTransparency(bUseTransparency)
		.InitialURL(CurrentUrl)
		.WebBrowserWindow(SharedThis(this));

	BrowserWidget = BrowserWidgetRef;

	Scripting->SetWindow(SharedThis(this));
		
	return BrowserWidgetRef;
}

void FOpenHarmonyWebBrowserWindow::SetViewportSize(FIntPoint WindowSize, FIntPoint WindowPos)
{
	OpenHarmonyWindowSize = WindowSize;
}

FIntPoint FOpenHarmonyWebBrowserWindow::GetViewportSize() const
{
	return OpenHarmonyWindowSize;
}

FSlateShaderResource* FOpenHarmonyWebBrowserWindow::GetTexture(bool bIsPopup /*= false*/)
{
	return nullptr;
}

bool FOpenHarmonyWebBrowserWindow::IsValid() const
{
	return BrowserWidget.IsValid();
}

bool FOpenHarmonyWebBrowserWindow::IsInitialized() const
{
	return true;
}

bool FOpenHarmonyWebBrowserWindow::IsClosing() const
{
	return false;
}

EWebBrowserDocumentState FOpenHarmonyWebBrowserWindow::GetDocumentLoadingState() const
{
	return DocumentState;
}

FString FOpenHarmonyWebBrowserWindow::GetTitle() const
{
	return Title;
}

FString FOpenHarmonyWebBrowserWindow::GetUrl() const
{
	return CurrentUrl;
}

bool FOpenHarmonyWebBrowserWindow::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FOpenHarmonyWebBrowserWindow::OnKeyUp(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FOpenHarmonyWebBrowserWindow::OnKeyChar(const FCharacterEvent& InCharacterEvent)
{
	return false;
}

FVector2D FOpenHarmonyWebBrowserWindow::ConvertMouseEventToLocal(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	FGeometry MouseGeometry = MyGeometry;

	float DPIScale = MouseGeometry.Scale;
	FVector2D LocalPos = MouseGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()) * DPIScale;

	return LocalPos;
}

FReply FOpenHarmonyWebBrowserWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	FReply Reply = FReply::Unhandled();
	/*
		FKey Button = MouseEvent.GetEffectingButton();
		bool bSupportedButton = (Button == EKeys::LeftMouseButton); // || Button == EKeys::RightMouseButton || Button == EKeys::MiddleMouseButton);
	
		if (bSupportedButton)
		{
			Reply = FReply::Handled();
			BrowserWidget->SendTouchDown(ConvertMouseEventToLocal(MyGeometry, MouseEvent, bIsPopup));
		}
	*/
	return Reply;
}

FReply FOpenHarmonyWebBrowserWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	FReply Reply = FReply::Unhandled();
	/*
		FKey Button = MouseEvent.GetEffectingButton();
		bool bSupportedButton = (Button == EKeys::LeftMouseButton); // || Button == EKeys::RightMouseButton || Button == EKeys::MiddleMouseButton);
	
		if (bSupportedButton)
		{
			Reply = FReply::Handled();
			BrowserWidget->SendTouchUp(ConvertMouseEventToLocal(MyGeometry, MouseEvent, bIsPopup));
		}
	*/
	return Reply;
}

FReply FOpenHarmonyWebBrowserWindow::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FOpenHarmonyWebBrowserWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	FReply Reply = FReply::Unhandled();
	/*
		FKey Button = MouseEvent.GetEffectingButton();
		bool bSupportedButton = (Button == EKeys::LeftMouseButton); // || Button == EKeys::RightMouseButton || Button == EKeys::MiddleMouseButton);
	
		if (bSupportedButton)
		{
			Reply = FReply::Handled();
			BrowserWidget->SendTouchMove(ConvertMouseEventToLocal(MyGeometry, MouseEvent, bIsPopup));
		}
	*/
	return Reply;
}

void FOpenHarmonyWebBrowserWindow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
}

void FOpenHarmonyWebBrowserWindow::SetSupportsMouseWheel(bool bValue)
{
}

bool FOpenHarmonyWebBrowserWindow::GetSupportsMouseWheel() const
{
	return false;
}

FReply FOpenHarmonyWebBrowserWindow::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FOpenHarmonyWebBrowserWindow::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

void FOpenHarmonyWebBrowserWindow::OnFocus(bool SetFocus, bool bIsPopup)
{
}

void FOpenHarmonyWebBrowserWindow::OnCaptureLost()
{
}

bool FOpenHarmonyWebBrowserWindow::CanGoBack() const
{
	return BrowserWidget->CanGoBack();
}

void FOpenHarmonyWebBrowserWindow::GoBack()
{
	BrowserWidget->GoBack();
}

bool FOpenHarmonyWebBrowserWindow::CanGoForward() const
{
	return BrowserWidget->CanGoForward();
}

void FOpenHarmonyWebBrowserWindow::GoForward()
{
	BrowserWidget->GoForward();
}

bool FOpenHarmonyWebBrowserWindow::IsLoading() const
{
	return DocumentState != EWebBrowserDocumentState::Loading;
}

void FOpenHarmonyWebBrowserWindow::Reload()
{
	BrowserWidget->Reload();
}

void FOpenHarmonyWebBrowserWindow::StopLoad()
{
	BrowserWidget->StopLoad();
}

void FOpenHarmonyWebBrowserWindow::GetSource(TFunction<void (const FString&)> Callback) const
{
	//@todo: decide what to do about multiple pending requests
	GetPageSourceCallback.Emplace(Callback);

	// Ugly hack: Work around the fact that ExecuteJavascript is non-const.
	const_cast<FOpenHarmonyWebBrowserWindow*>(this)->ExecuteJavascript(JSMessageGetSourceScript);
}

int FOpenHarmonyWebBrowserWindow::GetLoadError()
{
	return ErrorCode;
}

void FOpenHarmonyWebBrowserWindow::NotifyDocumentError(const FString& InCurrentUrl, int InErrorCode)
{
	if(!CurrentUrl.Equals(InCurrentUrl, ESearchCase::CaseSensitive))
	{
		CurrentUrl = InCurrentUrl;
		UrlChangedEvent.Broadcast(CurrentUrl);
	}

	ErrorCode = InErrorCode;
	DocumentState = EWebBrowserDocumentState::Error;
	DocumentStateChangedEvent.Broadcast(DocumentState);
}

void FOpenHarmonyWebBrowserWindow::NotifyDocumentLoadingStateChange(const FString& InCurrentUrl, bool IsLoading)
{
	// Ignore a load completed notification if there was an error.
	// For load started, reset any errors from previous page load.
	if (IsLoading || DocumentState != EWebBrowserDocumentState::Error)
	{
		if(!CurrentUrl.Equals(InCurrentUrl, ESearchCase::CaseSensitive))
		{
			CurrentUrl = InCurrentUrl;
			UrlChangedEvent.Broadcast(CurrentUrl);
		}

		if(!IsLoading && !InCurrentUrl.StartsWith("javascript:"))
		{
			Scripting->PageLoaded(SharedThis(this));
		}
		ErrorCode = 0;
		DocumentState = IsLoading
			? EWebBrowserDocumentState::Loading
			: EWebBrowserDocumentState::Completed;
		DocumentStateChangedEvent.Broadcast(DocumentState);
	}

}

void FOpenHarmonyWebBrowserWindow::SetIsDisabled(bool bValue)
{
	bIsDisabled = bValue;
}

TSharedPtr<SWindow> FOpenHarmonyWebBrowserWindow::GetParentWindow() const
{
	return ParentWindow;
}

void FOpenHarmonyWebBrowserWindow::SetParentWindow(TSharedPtr<SWindow> Window)
{
	ParentWindow = Window;
}

void FOpenHarmonyWebBrowserWindow::ExecuteJavascript(const FString& Script)
{
	BrowserWidget->ExecuteJavascript(Script);
}

void FOpenHarmonyWebBrowserWindow::CloseBrowser(bool bForce, bool bBlockTillClosed)
{
	BrowserWidget->Close();
}

bool FOpenHarmonyWebBrowserWindow::OnJsMessageReceived(const FString& Command, const TArray<FString>& Params, const FString& Origin)
{
	if( Command.Equals(JSGetSourceCommand, ESearchCase::CaseSensitive) && GetPageSourceCallback.IsSet() && Params.Num() == 1)
	{
		GetPageSourceCallback.GetValue()(Params[0]);
		GetPageSourceCallback.Reset();
		return true;
	}
	return Scripting->OnJsMessageReceived(Command, Params, Origin);
}

void FOpenHarmonyWebBrowserWindow::BindUObject(const FString& Name, UObject* Object, bool bIsPermanent /*= true*/)
{
	Scripting->BindUObject(SharedThis(this), Name, Object, bIsPermanent);
}

void FOpenHarmonyWebBrowserWindow::UnbindUObject(const FString& Name, UObject* Object /*= nullptr*/, bool bIsPermanent /*= true*/)
{
	Scripting->UnbindUObject(SharedThis(this), Name, Object, bIsPermanent);
}

void FOpenHarmonyWebBrowserWindow::CheckTickActivity()
{
	if (bIsVisible != bTickedLastFrame)
	{
		bIsVisible = bTickedLastFrame;
		BrowserWidget->SetWebBrowserVisibility(bIsVisible);
	}

	bTickedLastFrame = false;
}

void FOpenHarmonyWebBrowserWindow::SetTickLastFrame()
{
	bTickedLastFrame = !bIsDisabled;
}

bool FOpenHarmonyWebBrowserWindow::IsVisible()
{
	return bIsVisible;
}

#endif // USE_OPENHARMONY_NAPI