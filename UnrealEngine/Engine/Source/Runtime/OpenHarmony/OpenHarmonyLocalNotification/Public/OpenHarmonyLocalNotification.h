// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenHarmonyLocalNotification.h: Unreal OpenHarmony local notification interface object.
 =============================================================================*/

#pragma once

#include "LocalNotification.h"

#if PLATFORM_OPENHARMONY
	#include "OpenHarmony/OpenHarmonyApplication.h"
	#include <openharmony_native_app_glue.h>
#endif

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOpenHarmonyLocalNotification, Log, All);

/**
 * OpenHarmonyLocalNotification implementation of an Unreal local notification service.
 */
class FOpenHarmonyLocalNotificationService : public ILocalNotificationService
{
public:
	FOpenHarmonyLocalNotificationService();

	virtual void ClearAllLocalNotifications();
	virtual int32 ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent);
	virtual int32 ScheduleLocalNotificationAtTimeOverrideId(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent, int32 IdOverride);
	virtual int32 ScheduleLocalNotificationBadgeAtTime(const FDateTime& FireDateTime, bool LocalTime, const FString& ActivationEvent);
	virtual void GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate);
	virtual void SetLaunchNotification(FString const& ActivationEvent, int32 FireDate);
	virtual void CancelLocalNotification(const FString& ActivationEvent);
	virtual void CancelLocalNotification(int NotificationId);

private:
	bool	AppLaunchedWithNotification;
	FString	LaunchNotificationActivationEvent;
	int32	LaunchNotificationFireDate;
};
