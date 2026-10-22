#include "plugin_render.h"
//#include "OpenHarmony/OpenHarmonyNativeGlue/Public/plugin_render.h"

#include "OHLog.h"
#include "plugin_manager.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

PluginRender* PluginRender::instance_ = nullptr;
OH_NativeXComponent_Callback PluginRender::callback_;
OH_NativeXComponent_MouseEvent_Callback PluginRender::mouseCallback_;
float scrollDistance = 0.0;

void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window)
{
    OHOS_LOGD("OnSurfaceCreatedCB");
	PluginRender::GetInstance()->window_ = window;
	int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &PluginRender::GetInstance()->width_, &PluginRender::GetInstance()->height_);
	if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
		OHOS_LOGD("SurfaceCreate success");
		PluginRender::GetInstance()->sendMsgToWorker(MessageType::WM_XCOMPONENT_SURFACE_CREATED, component, window);
	}
}

void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window)
{
    OHOS_LOGD("OnSurfaceChangedCB");
    PluginRender::GetInstance()->sendMsgToWorker(MessageType::WM_XCOMPONENT_SURFACE_CHANGED, component, window);
}

void OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window)
{
    OHOS_LOGD("OnSurfaceDestroyedCB");
    PluginRender::GetInstance()->sendMsgToWorker(MessageType::WM_XCOMPONENT_SURFACE_DESTROY, component, window);
}

void DispatchTouchEventCB(OH_NativeXComponent* component, void* window)
{
	OHOS_LOGD("DispatchTouchEventCB");
    OH_NativeXComponent_TouchEvent* touchEvent = new(std::nothrow) OH_NativeXComponent_TouchEvent();
    if (!touchEvent) {
        OHOS_LOGE("DispatchTouchEventCB::touchEvent alloc failed");
        return;
    }
    int32_t ret = OH_NativeXComponent_GetTouchEvent(component, window, touchEvent);
    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        PluginRender::GetInstance()->sendMsgToWorker(MessageType::WM_XCOMPONENT_TOUCH_EVENT, component, window, touchEvent);
    }
    
}

void DispatchKeyEventCB(OH_NativeXComponent* component, void* window)
{
    OHOS_LOGD("DispatchKeyEventCB");
    OH_NativeXComponent_KeyEvent* keyEvent;
    if (OH_NativeXComponent_GetKeyEvent(component, &keyEvent) >= 0) {
        PluginRender::GetInstance()->sendMsgToWorker(MessageType::WM_XCOMPONENT_KEY_EVENT, component, window, keyEvent);
    } else {
        OHOS_LOGE("DispatchKeyEventCB::get keyEvent error");
    }
}

void DispatchMouseEventCB(OH_NativeXComponent* component, void* window)
{
    OH_NativeXComponent_MouseEvent mouseEvent;
    int32_t ret = OH_NativeXComponent_GetMouseEvent(component, window, &mouseEvent);
    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        PluginRender::GetInstance()->sendMsgToWorker(MessageType::WM_XCOMPONENT_MOUSE_EVENT, component, window, mouseEvent);
    } else {
        OHOS_LOGE("DispatchMouseEventCB::get MouseEvent Error");
    }
}

void DispatchHoverEventCB(OH_NativeXComponent* component, bool isHover)
{
    OHOS_LOGE("OpenHarmonyPlatform::DispatchHoverEventCB");
}

PluginRender::PluginRender() : component_(nullptr)
{
    auto renderCallback = PluginRender::GetNXComponentCallback();
    renderCallback->OnSurfaceCreated = OnSurfaceCreatedCB;
    renderCallback->OnSurfaceChanged = OnSurfaceChangedCB;
    renderCallback->OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    renderCallback->DispatchTouchEvent = DispatchTouchEventCB;
}

PluginRender* PluginRender::GetInstance()
{
    if (instance_ == nullptr) {
        instance_ = new PluginRender();
    }
    return instance_;
}

OH_NativeXComponent_Callback* PluginRender::GetNXComponentCallback()
{
    return &PluginRender::callback_;
}

void PluginRender::SetNativeXComponent(OH_NativeXComponent* component)
{
    component_ = component;
    OH_NativeXComponent_RegisterCallback(component_, &PluginRender::callback_);
    // register keyEvent
    OH_NativeXComponent_RegisterKeyEventCallback(component_, DispatchKeyEventCB);
    // register mouseEvent
    PluginRender::mouseCallback_.DispatchMouseEvent = DispatchMouseEventCB;
    PluginRender::mouseCallback_.DispatchHoverEvent = DispatchHoverEventCB;
    OH_NativeXComponent_RegisterMouseEventCallback(component_, &PluginRender::mouseCallback_);
}

void PluginRender::workerInit()
{
    OHOS_LOGD("PluginRender::workerInit");
	openHarmony_main(GetInstance());
}

void PluginRender::sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window)
{
    WorkerMessageData data{type, static_cast<void*>(component), window};
    enqueue(data);
}

void PluginRender::sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window, OH_NativeXComponent_TouchEvent* touchEvent)
{
    WorkerMessageData data{type, static_cast<void*>(component), window, touchEvent};
    enqueue(data);
}

void PluginRender::sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window, OH_NativeXComponent_KeyEvent* keyEvent) {
    WorkerMessageData data{type, static_cast<void*>(component), window};
    data.keyEvent = keyEvent;
    enqueue(data);
}

void PluginRender::sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window, OH_NativeXComponent_MouseEvent mouseEvent) {
    WorkerMessageData data{type, static_cast<void*>(component), window};
    data.mouseEvent = mouseEvent;
    enqueue(data);
}

void PluginRender::sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window, float x, float y, float scrollY) {
    WorkerMessageData data;
    data.type = type;
    data.x = x;
    data.y = y;
    data.scrollY = scrollY;
    enqueue(data);
}

void PluginRender::napiMouseWheelCB(std::string eventType, float x, float y, float offsetY)
{
    if (eventType == "actionUpdate") {
        float moveScrollY = offsetY - scrollDistance;
        scrollDistance = offsetY;
        PluginRender::GetInstance()->sendMsgToWorker(MessageType::WM_XCOMPONENT_MOUSE_WHEEL_EVENT, nullptr, nullptr, x, y, moveScrollY);
    } else {
        scrollDistance = 0;
    }
}

void PluginRender::enqueue(const WorkerMessageData& msg)
{
    messageQueue_.enqueue(msg);
    triggerMessageSignal();
}

bool PluginRender::dequeue(WorkerMessageData* msg)
{
    return messageQueue_.dequeue(msg);
}

void PluginRender::triggerMessageSignal()
{
    if(workerLoop_ != nullptr) {
        // It is possible that when the message is sent, the worker thread has not yet started.
        uv_async_send(&messageSignal_);
    }
}

#ifdef __cplusplus
}
#endif