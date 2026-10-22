#include "OpenHarmony/OpenHarmonyArkTsEnv.h"
#include "CoreMinimal.h"

FString FArkTsHelper::FStringFromLocalRef(const napi_env &env, const napi_value &napiString)
{
	size_t length = 0;
	napi_status status = napi_get_value_string_utf8(env, napiString, nullptr, 0, &length);
	if (status != napi_ok) {
		return FString();
	}

	TArray<char> buf;
	buf.SetNumUninitialized(length + 1);	// +1 for null terminator
	status = napi_get_value_string_utf8(env, napiString, buf.GetData(), buf.Num(), &length);
	if (status != napi_ok) {
		return FString();
	}
	return FString(buf.GetData());
}

