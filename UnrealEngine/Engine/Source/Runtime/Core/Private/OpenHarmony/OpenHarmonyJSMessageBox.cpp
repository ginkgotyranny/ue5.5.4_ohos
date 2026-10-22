#include "OpenHarmony/OpenHarmonyJSMessageBox.h"

#if USE_OPENHARMONY_NAPI

void FJSOpenHarmonyMessageBox::SetCaption(const FString & Text)
{
    std::string Msg(TCHAR_TO_UTF8(*Text));
	// C++调用JS方法
	auto jsFunc = aki::JSBind::GetJSFunction("MessageBox.setCaption");
    jsFunc -> Invoke<void>(Msg);
}

void FJSOpenHarmonyMessageBox::SetText(const FString & Text)
{
	std::string Msg(TCHAR_TO_UTF8(*Text));
	// C++调用JS方法
	auto jsFunc = aki::JSBind::GetJSFunction("MessageBox.setText");
    jsFunc -> Invoke<void>(Msg);
}

void FJSOpenHarmonyMessageBox::AddButton(const FString & Text)
{
	std::string Msg(TCHAR_TO_UTF8(*Text));
	// C++调用JS方法
	auto jsFunc = aki::JSBind::GetJSFunction("MessageBox.addButton");
    jsFunc -> Invoke<void>(Msg);
}

void FJSOpenHarmonyMessageBox::Clear()
{
	// C++调用JS方法
	auto jsFunc = aki::JSBind::GetJSFunction("MessageBox.clear");
    jsFunc -> Invoke<void>();
}

int32 FJSOpenHarmonyMessageBox::Show()
{
	// C++调用JS方法
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("MessageBox Show start!"));

	auto jsFunc = aki::JSBind::GetJSFunction("MessageBox.show");

	std::promise<int32> insertPromise;
    std::function<void(int32)> insertCallback = [&insertPromise](int32 message) {
        AKI_DLOG(INFO) << "callback will be invoked in JS Thread.";
        insertPromise.set_value(message);
    };
	int index = jsFunc -> Invoke<int32>(insertCallback); // 未点击按钮的默认值-1返回
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("index is : %d"), index);

	AKI_DLOG(INFO) << "wait until promise set value.";
    int32 message = insertPromise.get_future().get(); // 子线程阻塞，等待按钮点击完成后返回值
    AKI_DLOG(INFO) << "insert with message: " << message;

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("message is : %d"), message);
	return message;
}

#endif
