// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OpenHarmonyArkTSSurfaceViewDevices.generated.h"

USTRUCT()
struct FArkTSSurfaceViewDevice
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Manufacturer;

	UPROPERTY()
	FString Model;
};

UCLASS(config = DeviceProfiles)
class UOpenHarmonyArkTSSurfaceViewDevices : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Array of devices that require the java view scaling workaround */
	UPROPERTY(EditAnywhere, config, Category = "Matching Rules")
	TArray<FArkTSSurfaceViewDevice> SurfaceViewDevices;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
