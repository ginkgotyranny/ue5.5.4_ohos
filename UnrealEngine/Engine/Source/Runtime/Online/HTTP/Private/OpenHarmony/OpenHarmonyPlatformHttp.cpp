// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenHarmony/OpenHarmonyPlatformHttp.h"
#include "Curl/CurlHttp.h"
#include "Curl/CurlHttpManager.h"

void FOpenHarmonyPlatformHttp::Init()
{
	FCurlHttpManager::InitCurl();
}

class FHttpManager * FOpenHarmonyPlatformHttp::CreatePlatformHttpManager()
{
	return new FCurlHttpManager();
}

void FOpenHarmonyPlatformHttp::Shutdown()
{
	FCurlHttpManager::ShutdownCurl();
}

IHttpRequest* FOpenHarmonyPlatformHttp::ConstructRequest()
{
	return new FCurlHttpRequest();
}

TOptional<FString> FOpenHarmonyPlatformHttp::GetOperatingSystemProxyAddress()
{
	FString ProxyAddress;
	return ProxyAddress;
}

bool FOpenHarmonyPlatformHttp::IsOperatingSystemProxyInformationSupported()
{
	return true;
}