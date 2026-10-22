#include "OpenHarmonySDKSettings.h"
#include "Misc/Paths.h"
#include "Interfaces/IOpenHarmonyDeviceDetection.h"

#include "Interfaces/ITargetPlatformManagerModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenHarmonySDKSettings)

UOpenHarmonySDKSettings::UOpenHarmonySDKSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#if WITH_EDITOR
void UOpenHarmonySDKSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateTargetModulePaths();	
}

void UOpenHarmonySDKSettings::SetTargetModule(ITargetPlatformManagerModule * InTargetManagerModule)
{
	TargetManagerModule = InTargetManagerModule;
}

void UOpenHarmonySDKSettings::SetDeviceDetection(IOpenHarmonyDeviceDetection * InOpenHarmonyDeviceDetection)
{
	OpenHarmonyDeviceDetection = InOpenHarmonyDeviceDetection;
}

void UOpenHarmonySDKSettings::UpdateTargetModulePaths()
{
	TArray<FString> Keys;
	TArray<FString> Values;
	
	if (!SDKPath.Path.IsEmpty())
	{
		FPaths::NormalizeFilename(SDKPath.Path);
		Keys.Add(TEXT("OPENHARMONY_HOME"));
		Values.Add(SDKPath.Path);
	}
	
	if (!SDKPath.Path.IsEmpty())
	{
		FPaths::NormalizeFilename(SDKPath.Path);
		Keys.Add(TEXT("OHSDKROOT"));
		Values.Add(SDKPath.Path);
	}
	
	if (!NDKPath.Path.IsEmpty())
	{
		FPaths::NormalizeFilename(NDKPath.Path);
		Keys.Add(TEXT("OHNDKROOT"));
		Values.Add(NDKPath.Path);
	}

	if (!DevEcoPath.Path.IsEmpty())
	{
		FPaths::NormalizeFilename(DevEcoPath.Path);
		Keys.Add(TEXT("DEVECO_PATH"));
		Values.Add(DevEcoPath.Path);
	}
	
	SaveConfig();
	
	if (Keys.Num() != 0)
	{
		TargetManagerModule->UpdatePlatformEnvironment(TEXT("OpenHarmony"), Keys, Values);
		OpenHarmonyDeviceDetection->UpdateHDCPath();
	}
}

#endif

