#include "napi/native_api.h"
#include "plugin_manager.h"
#include "OHLog.h"
#include "plugin_render.h"

extern napi_value InitNapiEnv(napi_env env, napi_value exports);

napi_value NapiInit(napi_env& env, napi_value& exports)
{
	OHOS_LOGD("------------------------------------- Napi Init-------------------------------------");

	napi_property_descriptor desc[] = {
		DECLARE_NAPI_FUNCTION("Napi_getContext", NapiManager::Napi_getContext),
	};
	NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    
	bool ret = NapiManager::GetInstance()->Export(env, exports);
	if (!ret) {
		OHOS_LOGD("Init failed");
	}

	exports = InitNapiEnv(env, exports);
	return exports;
}