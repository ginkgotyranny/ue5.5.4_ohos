#ifndef _PLUGIN_MANAGER_H_
#define _PLUGIN_MANAGER_H_

#include <string>
#include <unordered_map>

#include "ace/xcomponent/native_interface_xcomponent.h"
#include "napi/native_api.h"
#include "uv.h"

#include "native_common.h"
#include "WorkerMessageQueue.h"
#include "plugin_render.h"

// init native resource manager
extern napi_value Napi_nativeResourceManagerInit(napi_env env, napi_callback_info info);

// set override log file directory
extern napi_value Napi_nativeSetOverrideLogFileDir(napi_env env, napi_callback_info info);

extern napi_value Napi_nativeGetBundleInfoForSelfSync(napi_env env, napi_callback_info info);
extern napi_value Napi_nativeSetConfigRulesVariables(napi_env env, napi_callback_info info);

class NapiManager 
{
public:
    ~NapiManager() {}

    static NapiManager* GetInstance()
    {
        return &NapiManager::manager_;
    }

    static napi_value Napi_getContext(napi_env env, napi_callback_info info);

    /******************************APP Lifecycle******************************/
    static napi_value Napi_onCreate(napi_env env, napi_callback_info info);
	static napi_value Napi_onNewWant(napi_env env, napi_callback_info info);
    static napi_value Napi_onShow(napi_env env, napi_callback_info info);
    static napi_value Napi_onHide(napi_env env, napi_callback_info info);
    static napi_value Napi_onBackPress(napi_env env, napi_callback_info info);
    static napi_value Napi_onDestroy(napi_env env, napi_callback_info info);
    /*********************************************************************/

    /******************************声明式范式******************************/
    /**                      JS Page : Lifecycle                        **/
    static napi_value Napi_onPageShow(napi_env env, napi_callback_info info);
    static napi_value Napi_onPageHide(napi_env env, napi_callback_info info);
    /*************************************************************************/

    // Worker Func
    static napi_value Napi_workerInit(napi_env env, napi_callback_info info);
    static napi_value Napi_nativeEngineStart(napi_env env, napi_callback_info info);
	static napi_value Napi_writablePathInit(napi_env env, napi_callback_info info);

    // Input Func
    static napi_value Napi_onMouseWheel(napi_env env, napi_callback_info info);

    OH_NativeXComponent* GetNativeXComponent();
    void SetNativeXComponent(OH_NativeXComponent* nativeXComponent);

public:
    // Napi export
    bool Export(napi_env env, napi_value exports);
private:
    static NapiManager manager_;

    OH_NativeXComponent* nativeXComponent_ = nullptr;
};

#endif // _PLUGIN_MANAGER_H_