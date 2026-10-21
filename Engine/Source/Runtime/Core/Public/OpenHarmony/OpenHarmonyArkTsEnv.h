// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "napi/native_api.h"

class CORE_API FArkTsHelper
{
public:
	static FString FStringFromLocalRef(const napi_env &env, const napi_value &napiString);
};