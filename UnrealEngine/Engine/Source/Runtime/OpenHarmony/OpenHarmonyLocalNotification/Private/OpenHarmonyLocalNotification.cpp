// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	OpenHarmonyLocalNotification.cpp: Unreal OpenHarmonyLocalNotification service interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
	Includes
 ------------------------------------------------------------------------------------*/

#include "OpenHarmonyLocalNotification.h"
#include "Async/TaskGraphInterfaces.h"
#include "Stats/Stats.h"

DEFINE_LOG_CATEGORY(LogOpenHarmonyLocalNotification);

class FOpenHarmonyLocalNotificationModule : public ILocalNotificationModule
{
public:
	/** Creates a new instance of the service implemented by the module. */
	virtual ILocalNotificationService* GetLocalNotificationService() override
	{
		static ILocalNotificationService*	oneTrueLocalNotificationService = nullptr;
		
		if(oneTrueLocalNotificationService == nullptr)
		{
			oneTrueLocalNotificationService = new FOpenHarmonyLocalNotificationService;
		}
		
		return oneTrueLocalNotificationService;
	}
};


#if USE_OPENHARMONY_JNI
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeAppOpenedWithLocalNotification(JNIEnv* jenv, jobject thiz, jstring jactivationEvent, int32 jFireDate)
{
	auto ActivationEvent = FJavaHelper::FStringFromParam(jenv, jactivationEvent);
	
	int32 FireDate = (int32)jFireDate;

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessAppOpenedWithLocalNotification"), STAT_FSimpleDelegateGraphTask_ProcessAppOpenedWithLocalNotification, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]() 
		{
			ILocalNotificationService* OpenHarmonyLocalNotificationService = NULL;

			FString ModuleName = "OpenHarmonyLocalNotification";
			// load the module by name
			ILocalNotificationModule* module = FModuleManager::LoadModulePtr<ILocalNotificationModule>(*ModuleName);
			// did the module exist?
			if (module != nullptr)
			{
				OpenHarmonyLocalNotificationService = module->GetLocalNotificationService();
				OpenHarmonyLocalNotificationService->SetLaunchNotification(ActivationEvent, FireDate);
			}
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessAppOpenedWithLocalNotification),
		nullptr,
		ENamedThreads::GameThread
		);
}
#endif


IMPLEMENT_MODULE(FOpenHarmonyLocalNotificationModule, OpenHarmonyLocalNotification);

/*------------------------------------------------------------------------------------
	FOpenHarmonyLocalNotification
 ------------------------------------------------------------------------------------*/
FOpenHarmonyLocalNotificationService::FOpenHarmonyLocalNotificationService(){
	AppLaunchedWithNotification = false;
	LaunchNotificationActivationEvent = "None";
	LaunchNotificationFireDate = 0;
}

void FOpenHarmonyLocalNotificationService::ClearAllLocalNotifications()
{
#if USE_OPENHARMONY_JNI
	extern void OpenHarmonyThunkCpp_ClearAllLocalNotifications();
	OpenHarmonyThunkCpp_ClearAllLocalNotifications();
#endif
}

int32 FOpenHarmonyLocalNotificationService::ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent)
{
#if USE_OPENHARMONY_JNI
	extern int32 OpenHarmonyThunkCpp_ScheduleLocalNotificationAtTime(const FDateTime & FireDateTime, bool LocalTime, const FText & Title, const FText & Body, const FText & Action, const FString & ActivationEvent, int32 IdOverride);
	return OpenHarmonyThunkCpp_ScheduleLocalNotificationAtTime(FireDateTime, LocalTime, Title, Body, Action, ActivationEvent, -1);
#else
	return -1;
#endif
}

int32 FOpenHarmonyLocalNotificationService::ScheduleLocalNotificationAtTimeOverrideId(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent, int32 IdOverride)
{
#if USE_OPENHARMONY_JNI
	extern int32 OpenHarmonyThunkCpp_ScheduleLocalNotificationAtTime(const FDateTime & FireDateTime, bool LocalTime, const FText & Title, const FText & Body, const FText & Action, const FString & ActivationEvent, int32 IdOverride);
	return OpenHarmonyThunkCpp_ScheduleLocalNotificationAtTime(FireDateTime, LocalTime, Title, Body, Action, ActivationEvent, IdOverride);
#else
	return -1;
#endif
}

int32 FOpenHarmonyLocalNotificationService::ScheduleLocalNotificationBadgeAtTime(const FDateTime& FireDateTime, bool LocalTime, const FString& ActivationEvent)
{
	// Do nothing...
	return -1;
}

void FOpenHarmonyLocalNotificationService::GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate)
{
#if USE_OPENHARMONY_JNI
	extern void OpenHarmonyThunkCpp_GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate);
	OpenHarmonyThunkCpp_GetLaunchNotification(NotificationLaunchedApp, ActivationEvent, FireDate);
#endif
}

void FOpenHarmonyLocalNotificationService::SetLaunchNotification(FString const& ActivationEvent, int32 FireDate)
{
	// Don't do anything here since this is taken care of in java land
}

void FOpenHarmonyLocalNotificationService::CancelLocalNotification(const FString& ActivationEvent)
{
	// TODO
}

void FOpenHarmonyLocalNotificationService::CancelLocalNotification(int32 NotificationId)
{
	if (NotificationId < 0)
	{
		return;
	}
	
#if USE_OPENHARMONY_JNI
	extern bool OpenHarmonyThunkCpp_DestroyScheduledNotificationIfExists(int32 NotificationId);
	OpenHarmonyThunkCpp_DestroyScheduledNotificationIfExists(NotificationId);
#endif
}
