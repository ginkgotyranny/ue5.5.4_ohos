#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "OpenHarmonySDKSettings.generated.h"

class IOpenHarmonyDeviceDetection;

/**
 * Implements the settings for the OpenHarmony SDK setup.
 */
UCLASS(config=Engine, globaluserconfig)
class OPENHARMONYPLATFORMEDITOR_API UOpenHarmonySDKSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	// Location on disk of the OpenHarmony SDK (falls back to OHSDKROOT environment variable if this is left blank)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = SDKConfig, Meta = (DisplayName = "Location of OpenHarmony SDK (the root path of OpenHarmony SDK)"))
	FDirectoryPath SDKPath;

	// Location on disk of the OpenHarmony NDK (falls back to OHNDKROOT environment variable if this is left blank)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = SDKConfig, Meta = (DisplayName = "Location of OpenHarmony NDK (the path usually is [OH SDK]/openharmony/native)"))
	FDirectoryPath NDKPath;

	// location of DevEco Studio"
	UPROPERTY(GlobalConfig, EditAnywhere, Category = SDKConfig, Meta = (DisplayName = "Location of DevEco"))
	FDirectoryPath DevEcoPath;

#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	void SetTargetModule(ITargetPlatformManagerModule * TargetManagerModule);
	void SetDeviceDetection(IOpenHarmonyDeviceDetection * OpenHarmonyDeviceDetection);
	void UpdateTargetModulePaths();
	ITargetPlatformManagerModule * TargetManagerModule;
	IOpenHarmonyDeviceDetection * OpenHarmonyDeviceDetection;
#endif
};
