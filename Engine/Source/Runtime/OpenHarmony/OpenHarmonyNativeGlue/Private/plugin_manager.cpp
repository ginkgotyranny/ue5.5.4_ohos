#include "plugin_manager.h"
#include "OHLog.h"
#include <stdint.h>
#include <string>
#include <stdio.h>
#include "ace/xcomponent/native_interface_xcomponent.h"




const int32_t kMaxStringLen = 512;
enum ContextType {
    APP_LIFECYCLE = 0,
    JS_PAGE_LIFECYCLE,
    RAW_FILE_UTILS,
    WORKER_INIT,
    NATIVE_RENDER_API,
    INPUT_NAPI,
	MISC
// todo: EditBox, WebView, VideoPlayer, Sensor
};

NapiManager NapiManager::manager_;

napi_value NapiManager::Napi_getContext(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value exports;
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc != 1) {
        napi_throw_type_error(env, NULL, "Wrong number of arguments");
        return nullptr;
    }

    napi_valuetype valuetype;
    status = napi_typeof(env, args[0], &valuetype);
    if (status != napi_ok) {
        return nullptr;
    }
    if (valuetype != napi_number) {
        napi_throw_type_error(env, NULL, "Wrong arguments");
        return nullptr;
    }

    int64_t value;
    NAPI_CALL(env, napi_get_value_int64(env, args[0], &value));

    NAPI_CALL(env, napi_create_object(env, &exports));

    switch (value) {
        case APP_LIFECYCLE:
            {
                /**** AppInit 对应 app.ets中的应用生命周期 onCreate, onShow, onHide, onDestroy ******/
                OHOS_LOGD("Napi_getContext APP_LIFECYCLE");
                /**** Register App Lifecycle  ******/
                napi_property_descriptor desc[] = {
                    DECLARE_NAPI_FUNCTION("Napi_onCreate", NapiManager::Napi_onCreate),
                	DECLARE_NAPI_FUNCTION("Napi_onNewWant", NapiManager::Napi_onNewWant),
                    DECLARE_NAPI_FUNCTION("Napi_onShow", NapiManager::Napi_onShow),
                    DECLARE_NAPI_FUNCTION("Napi_onHide", NapiManager::Napi_onHide),
                    DECLARE_NAPI_FUNCTION("Napi_onBackPress", NapiManager::Napi_onBackPress),
                    DECLARE_NAPI_FUNCTION("Napi_onDestroy", NapiManager::Napi_onDestroy),
                };
                NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
            }
            break;
        case JS_PAGE_LIFECYCLE:
            {
                /****************  声明式开发范式 JS Page 生命周期注册 ****************************/
                OHOS_LOGD("Napi_getContext JS_PAGE_LIFECYCLE");
                napi_property_descriptor desc[] = {
                    DECLARE_NAPI_FUNCTION("Napi_onPageShow", NapiManager::Napi_onPageShow),
                    DECLARE_NAPI_FUNCTION("Napi_onPageHide", NapiManager::Napi_onPageHide),
                };
                NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
            }
            break;
        case RAW_FILE_UTILS:
            {
                /****************  声明式开发范式 JS Page 生命周期注册 ****************************/
                OHOS_LOGD("Napi_getContext RAW_FILE_UTILS");
                napi_property_descriptor desc[] = {
                    DECLARE_NAPI_FUNCTION("Napi_nativeResourceManagerInit", Napi_nativeResourceManagerInit),
					DECLARE_NAPI_FUNCTION("Napi_nativeSetOverrideLogFileDir", Napi_nativeSetOverrideLogFileDir),
                    DECLARE_NAPI_FUNCTION("Napi_writablePathInit", NapiManager::Napi_writablePathInit),
                    DECLARE_NAPI_FUNCTION("Napi_nativeGetBundleInfoForSelfSync", Napi_nativeGetBundleInfoForSelfSync),
                };
                NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));

            }
            break;
        case WORKER_INIT:
            {
                OHOS_LOGD("NapiManager::Napi_getContext WORKER_INIT");
                napi_property_descriptor desc[] = {
                    DECLARE_NAPI_FUNCTION("Napi_workerInit", NapiManager::Napi_workerInit),
                };
                NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
            }
            break;
        case NATIVE_RENDER_API:
            {
                OHOS_LOGD("NapiManager::Napi_getContext NATIVE_RENDER_API");
                napi_property_descriptor desc[] = {
                    DECLARE_NAPI_FUNCTION("Napi_nativeEngineStart", NapiManager::Napi_nativeEngineStart),
                };
                NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
            }
            break;
        case INPUT_NAPI:
            {
                OHOS_LOGD("NapiManager::Napi_getContext INPUT_NAPI");
                napi_property_descriptor desc[] = {
                    DECLARE_NAPI_FUNCTION("Napi_onMouseWheel", NapiManager::Napi_onMouseWheel),
                };
                NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
            }
    		break;
		case MISC:
	        {
        		napi_property_descriptor desc[] = {
        			DECLARE_NAPI_FUNCTION("nativeSetConfigRulesVariables", Napi_nativeSetConfigRulesVariables),
				};
        		NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
	        }
    		break;
        default:
            OHOS_LOGE("unknown type");
    }
    return exports;
}

bool NapiManager::Export(napi_env env, napi_value exports)
{
    static int time = 0;
    OHOS_LOGD("NapiManager::Export time %{public}d", time);
    time++;
    napi_status status;
    napi_value exportInstance = nullptr;
    OH_NativeXComponent *nativeXComponent = nullptr;

    status = napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    if (status != napi_ok) {
        OHOS_LOGD("NapiManager::Export Failed 1 status = %{public}d", status);
        return false;
    }

    status = napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&nativeXComponent));
    if (status != napi_ok) {
        OHOS_LOGD("NapiManager::Export Failed 2 status = %{public}d", status);
        return false;
    }

    auto context = NapiManager::GetInstance();
    if (context) {
        OHOS_LOGD("NapiManager::Export Success");
        context->SetNativeXComponent(nativeXComponent);
        PluginRender::GetInstance()->SetNativeXComponent(nativeXComponent);
        return true;
    }

    OHOS_LOGD("NapiManager::Export Failed context empty");
    return false;
}

void NapiManager::SetNativeXComponent(OH_NativeXComponent* nativeXComponent)
{
    nativeXComponent_ = nativeXComponent;
}

OH_NativeXComponent* NapiManager::GetNativeXComponent()
{
    return nativeXComponent_;
}

napi_value NapiManager::Napi_onCreate(napi_env env, napi_callback_info info)
{
	OHOS_LOGD("NapiManager NapiOnCreate");
    return nullptr;
}

napi_value NapiManager::Napi_onNewWant(napi_env env, napi_callback_info info)
{
	OHOS_LOGD("NapiManager NapiOnNewWant");
	return nullptr;
}

napi_value NapiManager::Napi_onShow(napi_env env, napi_callback_info info)
{
    OHOS_LOGD("NapiManager NapiOnShow");
	// 通知UE切换到前台
    WorkerMessageData data{MessageType::WM_APP_SHOW, nullptr, nullptr};
    PluginRender::GetInstance()->enqueue(data);
    return nullptr;
}

napi_value NapiManager::Napi_onHide(napi_env env, napi_callback_info info)
{
    OHOS_LOGD("NapiManager NapiOnHide");
	// 通知UE切换到后台
    WorkerMessageData data{MessageType::WM_APP_HIDE, nullptr, nullptr};
    PluginRender::GetInstance()->enqueue(data);
    return nullptr;
}

napi_value NapiManager::Napi_onBackPress(napi_env env, napi_callback_info info)
{
    // todo: 通知UE返回上一级
    return nullptr;
}

napi_value NapiManager::Napi_onDestroy(napi_env env, napi_callback_info info)
{
    WorkerMessageData data{MessageType::WM_APP_DESTROY, nullptr, nullptr};
    PluginRender::GetInstance()->enqueue(data);
    return nullptr;
}

napi_value NapiManager::Napi_workerInit(napi_env env, napi_callback_info info) {
    OHOS_LOGD("NapiManager::Napi_workerInit");
    PluginRender::GetInstance()->workerInit();
    return nullptr;
}

extern napi_value initCCArkJsDialog(napi_env env, napi_value exports);

napi_value NapiManager::Napi_nativeEngineStart(napi_env env, napi_callback_info info) {
    OHOS_LOGD("NapiManager::Napi_nativeEngineStart");
    initCCArkJsDialog(env, nullptr);
    return nullptr;
}

napi_value NapiManager::Napi_writablePathInit(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value  args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    char buffer[kMaxStringLen];
    size_t result = 0;
    NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], buffer, kMaxStringLen, &result));
    return nullptr;
}

napi_value NapiManager::Napi_onPageShow(napi_env env, napi_callback_info info)
{
    OHOS_LOGD("NapiManager::NapiOnPageShow");
    return nullptr;
}

napi_value NapiManager::Napi_onPageHide(napi_env env, napi_callback_info info)
{
    OHOS_LOGD("NapiManager::NapiOnPageHide");
    return nullptr;
}

napi_value NapiManager::Napi_onMouseWheel(napi_env env, napi_callback_info info)
{   
    OHOS_LOGD("NapiManager::NapiOnMouseWheel");
    size_t argc = 4;
    napi_value  args[4];
    napi_status status;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc != 4) {
        napi_throw_type_error(env, NULL, "Wrong number of arguments");
        return nullptr;
    }

    napi_valuetype valuetype;

    // get eventType
    status = napi_typeof(env, args[0], &valuetype);
    if (status != napi_ok) {
        return nullptr;
    }
    if (valuetype != napi_string) {
        napi_throw_type_error(env, NULL, "Wrong arguments in eventType");
        return nullptr;
    }

    // get x
    status = napi_typeof(env, args[1], &valuetype);
    if (status != napi_ok) {
        return nullptr;
    }
    if (valuetype != napi_number) {
        napi_throw_type_error(env, NULL, "Wrong arguments in eventType");
        return nullptr;
    }

    // get y
    status = napi_typeof(env, args[2], &valuetype);
    if (status != napi_ok) {
        return nullptr;
    }
    if (valuetype != napi_number) {
        napi_throw_type_error(env, NULL, "Wrong arguments in eventType");
        return nullptr;
    }

    // get offsetY
    status = napi_typeof(env, args[3], &valuetype);
    if (status != napi_ok) {
        return nullptr;
    }
    if (valuetype != napi_number) {
        napi_throw_type_error(env, NULL, "Wrong arguments in eventType");
        return nullptr;
    }

    size_t pInt;
    char eventType[256];
    NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], eventType, 256, &pInt));
    double x;
    NAPI_CALL(env, napi_get_value_double(env, args[1], &x));
    double y;
    NAPI_CALL(env, napi_get_value_double(env, args[2], &y));
    double offsetY;
    NAPI_CALL(env, napi_get_value_double(env, args[3], &offsetY));
    PluginRender::GetInstance()->napiMouseWheelCB(eventType, float(x), float(y), float(offsetY));

    return nullptr;
}