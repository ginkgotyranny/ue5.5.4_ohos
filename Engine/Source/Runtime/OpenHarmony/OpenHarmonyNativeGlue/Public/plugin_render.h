#ifndef _PLUGIN_RENDER_H_
#define _PLUGIN_RENDER_H_

#include <string>
#include <unordered_map>
#include "uv.h"

#include "ace/xcomponent/native_interface_xcomponent.h"
#include "napi/native_api.h"

#include "WorkerMessageQueue.h"

class PluginRender 
{
public:
    PluginRender();
    static PluginRender* GetInstance();
    static OH_NativeXComponent_Callback* GetNXComponentCallback();

    static void onMessageCallback(const uv_async_t* req);

    void SetNativeXComponent(OH_NativeXComponent* component);

    void workerInit();

    void sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window); 
    void sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window, OH_NativeXComponent_TouchEvent* touchEvent); 
    void sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window, OH_NativeXComponent_KeyEvent* keyEvent);
    void sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window, OH_NativeXComponent_MouseEvent mouseEvent);
    void sendMsgToWorker(const MessageType& type, OH_NativeXComponent* component, void* window, float x, float y, float scrollY);

    void enqueue(const WorkerMessageData& data);
    bool dequeue(WorkerMessageData* data);
    void triggerMessageSignal();
    void napiMouseWheelCB(std::string eventType, float x, float y, float offsetY);

public:
    // Exposed to JS developers by NAPI
    static napi_value NapiChangeColor(napi_env env, napi_callback_info info);
    static napi_value NapiChangeColorWorker(napi_env env, napi_callback_info info);


public:
    static PluginRender* instance_;
    static OH_NativeXComponent_Callback callback_;
    static OH_NativeXComponent_MouseEvent_Callback mouseCallback_;
    static std::queue<OH_NativeXComponent_TouchEvent*> touchEventQueue_;

    OH_NativeXComponent* component_{nullptr};


    bool timerInited_{false};
    uv_loop_t* workerLoop_{nullptr};
    uv_async_t messageSignal_{};
    WorkerMessageQueue messageQueue_;

    uint64_t width_;
    uint64_t height_;
	void *window_;

    double x_;
    double y_;
};

extern void openHarmony_main(PluginRender* app);

#endif // _PLUGIN_RENDER_H_
