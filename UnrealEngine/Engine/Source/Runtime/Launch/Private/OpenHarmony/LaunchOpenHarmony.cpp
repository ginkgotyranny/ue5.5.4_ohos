// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "MoviePlayer.h"
#include "OpenHarmony/OpenHarmonyPlatformMisc.h"
#define USE_OPENHARMONY_LAUNCH 1
#if USE_OPENHARMONY_LAUNCH
#include "LaunchEngineLoop.h"
#include "OpenHarmony/OpenHarmonyEventManager.h"
#include "ace/xcomponent/native_interface_xcomponent.h"
#include "uv.h"
#include "OpenHarmony/OpenHarmonyWindow.h"
#include "OpenHarmony/OpenHarmonyInputInterface.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdlib.h>
#include "OpenHarmony/OpenHarmonyPlatformFramePacer.h"
#include "plugin_render.h"
#include "aki/jsbind.h"
#include "Async/Async.h"
#include <rawfile/raw_dir.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include "IMessagingModule.h"
#include "ISessionServicesModule.h"
#include "ISessionService.h"
#include "PreLoadScreenManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EmbeddedCommunication.h"
//#include "MoviePlayer/Public/MoviePlayer.h"
#include "MoviePlayer.h"
#include "OpenHarmony/openHarmonyKeyStateManager.h"

// Event for coordinating pausing of the main and event handling threads to prevent background spinning
static FEvent* EventHandlerEvent = NULL;

//Event thread stuff
static void OpenHarmonyEventThreadWorker(void* param);

// How often to process (read & dispatch) events, in seconds.
static const float EventRefreshRate = 1.0f / 20.0f;

// Name of the UE4 commandline append setprop
static constexpr char UE4CommandLineSetprop[] = "debug.ue4.commandline";

static uint32 EventThreadID = 0;

bool GShowConsoleWindowNextTick = false;

extern NativeResourceManager* GNativeResourceManager;

// Base path for file accesses
extern FString GFilePathBase;

extern void OpenHarmonyThunkCpp_ShowConsoleWindow();

bool IsInOpenHarmonyEventThread()
{
	check(EventThreadID != 0);
	return EventThreadID == FPlatformTLS::GetCurrentThreadId();
}

void SetAppWindowsState()
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Test SetAppWindowsState"));
}

// The event thread locks this whenever the window unavailable during early init, pause and resume.
FCriticalSection GOpenHarmonyWindowLock;
bool bReadyToProcessEvents = false;
void FPlatformMisc::UnlockOpenHarmonyWindow()
{
	check(IsInGameThread());
	check(FTaskGraphInterface::IsRunning());
	UE_LOG(LogOpenHarmony, Log, TEXT("Unlocking openHarmony HW window during preinit."));
	bReadyToProcessEvents = true;
	GOpenHarmonyWindowLock.Unlock();
}

static void InitCommandLine()
{
	static const uint32 CMD_LINE_MAX = 16384u;

	// initialize the command line to an empty string
	FCommandLine::Set(TEXT(""));

	RawFile64* Rfile = OH_ResourceManager_OpenRawFile64(GNativeResourceManager, TCHAR_TO_UTF8(TEXT("UE4CommandLine.txt")));
	if (nullptr != Rfile)
	{
		// int64 FileLength = OH_ResourceManager_GetRawFileSize(Rfile);
		// uint8* FileContents = (uint8*)malloc(Length);
		// OH_ResourceManager_ReadRawFile(Rfile, FileContents, FileLength);

		size_t FileLength = OH_ResourceManager_GetRawFileSize64(Rfile);
		void* FileContents = malloc(FileLength);
		OH_ResourceManager_ReadRawFile64(Rfile, FileContents, FileLength);

		char CommandLine[CMD_LINE_MAX];
		FileLength = (FileLength < CMD_LINE_MAX - 1) ? FileLength : CMD_LINE_MAX - 1;
		memcpy(CommandLine, FileContents, FileLength);
		CommandLine[FileLength] = '\0';
		
		OH_ResourceManager_CloseRawFile64(Rfile);
		Rfile = nullptr;

		// chop off trailing spaces
		while (*CommandLine && isspace(CommandLine[strlen(CommandLine) - 1]))
		{
			CommandLine[strlen(CommandLine) - 1] = 0;
		}

		FCommandLine::Append(UTF8_TO_TCHAR(CommandLine));
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("APK Commandline: %s"), FCommandLine::Get());
	}

	// read in the command line text file from the sdcard if it exists
	FString CommandLineFilePath = GFilePathBase + FString("/UE4Game/") + (!FApp::IsProjectNameEmpty() ? FApp::GetProjectName() : FPlatformProcess::ExecutableName()) + FString("/UE4CommandLine.txt");
	FILE* CommandLineFile = fopen(TCHAR_TO_UTF8(*CommandLineFilePath), "r");
	if(CommandLineFile == NULL)
	{
		// if that failed, try the lowercase version
		CommandLineFilePath = CommandLineFilePath.Replace(TEXT("UE4CommandLine.txt"), TEXT("ue4commandline.txt"));
		CommandLineFile = fopen(TCHAR_TO_UTF8(*CommandLineFilePath), "r");
	}

	if(CommandLineFile)
	{
		char CommandLine[CMD_LINE_MAX];
		fgets(CommandLine, UE_ARRAY_COUNT(CommandLine) - 1, CommandLineFile);

		fclose(CommandLineFile);

		// chop off trailing spaces
		while (*CommandLine && isspace(CommandLine[strlen(CommandLine) - 1]))
		{
			CommandLine[strlen(CommandLine) - 1] = 0;
		}

		// initialize the command line to an empty string
		FCommandLine::Set(TEXT(""));

		FCommandLine::Append(UTF8_TO_TCHAR(CommandLine));
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Override Commandline: %s"), FCommandLine::Get());
	}

#if !UE_BUILD_SHIPPING
	if (FString* ConfigRulesCmdLineAppend = FOpenHarmonyMisc::GetConfigRulesVariable(TEXT("cmdline")))
	{
		FCommandLine::Append(**ConfigRulesCmdLineAppend);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("ConfigRules appended: %s"), **ConfigRulesCmdLineAppend);
	}
#endif
}

extern void OpenHarmonyThunkCpp_DismissSplashScreen();

int32 OpenHarmonyMain(void* window, int width, int height)
{
	BootTimingPoint("OpenHarmonyMain");
	FPlatformMisc::LowLevelOutputDebugString(TEXT("Entered OpenHarmonyMain()\n"));

	// read the command line file
	InitCommandLine();
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Final commandline: %s\n"), FCommandLine::Get());

	EventHandlerEvent = FPlatformProcess::GetSynchEventFromPool(false);
	FAppEventManager::GetInstance()->SetEventHandlerEvent(EventHandlerEvent);

	{
		SCOPED_BOOT_TIMING("Wait for GOpenHarmonyWindowLock.Lock()");
		// wait for a valid window
		// Lock GOpenHarmonyWindowLock to ensure no window destroy shenanigans occur between early phase of preinit and UnlockOpenHarmonyWindow
		// Note: this is unlocked after OpenHarmony's PlatformCreateDynamicRHI when the RHI is then able to process window changes.
		// We don't wait for all of preinit to complete as PreLoadScreens will need to process events during preinit.

		UE_LOG(LogOpenHarmony, Log, TEXT("PreInit openHarmony HW window lock."));
		GOpenHarmonyWindowLock.Lock();
	}

	// initialize the engine
	int32 PreInitResult = GEngineLoop.PreInit(0, NULL, FCommandLine::Get());

	if (PreInitResult != 0)
	{
		checkf(false, TEXT("Engine Preinit Failed"));
		return PreInitResult;
	}

	UE_LOG(LogOpenHarmony, Display, TEXT("Passed PreInit()"));
	
	GLog->SetCurrentThreadAsPrimaryThread();

	FAppEventManager::GetInstance()->SetEmptyQueueHandlerEvent(FPlatformProcess::GetSynchEventFromPool(false));

	GEngineLoop.Init();

	UE_LOG(LogOpenHarmony, Log, TEXT("Passed GEngineLoop.Init()"));

	OpenHarmonyThunkCpp_DismissSplashScreen();

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("Messaging")))
	{
		// initialize messaging subsystem
		FModuleManager::LoadModuleChecked<IMessagingModule>("Messaging");
		TSharedPtr<ISessionService> SessionService = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices").GetSessionService();
		SessionService->Start();

		// Initialize functional testing
		FModuleManager::Get().LoadModule("FunctionalTesting");
	}
#endif

	BootTimingPoint("Tick loop starting");
	DumpBootTiming();
	// tick until done
	while (!IsEngineExitRequested())
	{
		FAppEventManager::GetInstance()->Tick();
		if (!FAppEventManager::GetInstance()->IsGamePaused())
		{
			GEngineLoop.Tick();
		}
		else
		{
			// use less CPU when paused
			FPlatformProcess::Sleep(0.10f);
		}
#if !UE_BUILD_SHIPPING
		// show console window on next game tick
		if (GShowConsoleWindowNextTick)
		{
			GShowConsoleWindowNextTick = false;
			OpenHarmonyThunkCpp_ShowConsoleWindow();
		}
#endif
	}
	
	FAppEventManager::GetInstance()->TriggerEmptyQueue();

	UE_LOG(LogOpenHarmony, Log, TEXT("Exiting"));

	// exit out!
	GEngineLoop.Exit();

	UE_LOG(LogOpenHarmony, Log, TEXT("Exiting is over"));

	FPlatformMisc::RequestExit(1);
	return 0;
}

void openHarmony_main(PluginRender* app)
{
	FTaskTagScope Scope(ETaskTag::EGameThread);
	GGameThreadId = FPlatformTLS::GetCurrentThreadId();
	// BootTimingPoint("openHarmony_main");
	FPlatformMisc::LowLevelOutputDebugString(TEXT("Entering native app glue main function"));
	
	uv_thread_t thread;
	uv_thread_create(&thread, OpenHarmonyEventThreadWorker, app);
	
	FPlatformMisc::LowLevelOutputDebugString(TEXT("Created event thread"));
	OpenHarmonyMain(app->window_, app->width_, app->height_);
}


FAppEventData::FAppEventData(void* WindowIn)
{
	check(WindowIn);
	OH_NativeWindow_NativeWindowHandleOpt((OHNativeWindow*)WindowIn, GET_BUFFER_GEOMETRY, &WindowHeight, &WindowWidth);
	check(WindowWidth >= 0 && WindowHeight >= 0);
}

FAppEventData::FAppEventData(int32 Width, int32 Height)
{
	check(Width >= 0 && Height >= 0);
	WindowHeight = Height;
	WindowWidth = Width;
}

static bool bAppIsActive_EventThread = false;

// called when the app has focus + window + resume.
static void ActivateApp_EventThread()
{
	if (bAppIsActive_EventThread)
	{
		// Seems this can occur.
		return;
	}

	// Unlock window when we're ready.
	UE_LOG(LogOpenHarmony, Log, TEXT("event thread, activate app, unlocking HW window"));
	GOpenHarmonyWindowLock.Unlock();
	// wake the GT up.
	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_APP_ACTIVATED);

	bAppIsActive_EventThread = true;
	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_RUN_CALLBACK, FAppEventData([]()
		{
			UE_LOG(LogOpenHarmony, Log, TEXT("performing app foregrounding callback."));
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
			FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
			FAppEventManager::GetInstance()->ResumeAudio();
		}));
	FPreLoadScreenManager::EnableRendering(true);
}

extern void BlockRendering();
// called whenever the app loses focus, loses window or pause.
static void SuspendApp_EventThread()
{
	if (!bAppIsActive_EventThread)
	{
		return;
	}
	bAppIsActive_EventThread = false;
	// Lock the window, this prevents event thread from removing the window whilst the RHI initializes.

	UE_LOG(LogOpenHarmony, Log, TEXT("event thread, suspending app, acquiring HW window lock."));
	GOpenHarmonyWindowLock.Lock();

	if (bReadyToProcessEvents == false)
	{
		// App has stopped before we can process events.
		// OpenHarmonyLaunch will lock GOpenHarmonyWindowLock, and set bReadyToProcessEvents when we are able to block the RHI and queue up other events.
		// we ignore events until this point as acquiring GOpenHarmonyWindowLock means requires the window to be properly initialized.
		UE_LOG(LogOpenHarmony, Log, TEXT("event thread, app not yet ready."));
		return;
	};

	TSharedPtr<FEvent, ESPMode::ThreadSafe> EMDoneTrigger = MakeShareable(FPlatformProcess::GetSynchEventFromPool(), [](FEvent* EventToDelete)
		{
			FPlatformProcess::ReturnSynchEventToPool(EventToDelete);
		});

	// perform the delegates before the window handle is cleared.
	// This ensures any tasks that require a window handle will have it before we block the RT on the invalid window.
	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_RUN_CALLBACK, FAppEventData([EMDoneTrigger]()
		{
			UE_LOG(LogOpenHarmony, Log, TEXT("performing app backgrounding callback. %p"), EMDoneTrigger.Get());

			FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
			FAppEventManager::GetInstance()->PauseAudio();
			//FAppEventManager::ReleaseMicrophone(false);
			EMDoneTrigger->Trigger();
		}));

	FEmbeddedCommunication::WakeGameThread();

	FPreLoadScreenManager::EnableRendering(false);

	// wait for a period of time before blocking rendering
	UE_LOG(LogOpenHarmony, Log, TEXT("OpenHarmonyEGL::  SuspendApp_EventThread, waiting for event manager to process. tid: %d"), FPlatformTLS::GetCurrentThreadId());
	bool bSuccess = EMDoneTrigger->Wait(4000);
	UE_CLOG(!bSuccess, LogOpenHarmony, Log, TEXT("backgrounding callback, not responded in timely manner."));

	BlockRendering();

	// Suspend the GT.
	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_APP_SUSPENDED);
}

static bool IsStartupMoviePlaying()
{
	return GEngine && GEngine->IsInitialized() && GetMoviePlayer() && GetMoviePlayer()->IsStartupMoviePlaying();
}

static bool IsPreLoadScreenPlaying()
{
	return IsStartupMoviePlaying() 
		|| (FPreLoadScreenManager::Get() && (FPreLoadScreenManager::Get()->HasValidActivePreLoadScreen()));
}

void DispatchTouchEvent(OH_NativeXComponent* component, void* window, OH_NativeXComponent_TouchEvent* touchEvent);
void DispatchKeyEvent(OH_NativeXComponent* component, void* window, OH_NativeXComponent_KeyEvent* keyEvent);
void DispatchMouseEvent(OH_NativeXComponent* component, void* window, OH_NativeXComponent_MouseEvent mouseEvent);
void DispatchMouseWheelEvent(OH_NativeXComponent* component, void* window, float x, float y, float scrollY);
void onMessageCallback(const uv_async_t* handle)
{
	// uv_close((uv_handle_t*)&my_signal, close_cb);	//如果async没有关闭，消息队列是会阻塞的

    WorkerMessageData msgData;
	static bool bHasFocus = false;
	static bool bHasWindow = false;
	static bool bIsResumed = false;

    FPlatformMisc::LowLevelOutputDebugString(TEXT("onMessageCallback"));
    
    while (true) {
        //loop until all msg dispatch
        if (!PluginRender::GetInstance()->dequeue(reinterpret_cast<WorkerMessageData*>(&msgData))) {
            // Queue has no data
            break;
        }

    	if(msgData.window) {
            FOpenHarmonyWindow::SetWindowDimensions_EventThread(msgData.window);
		}		
        
        if ((msgData.type >= MessageType::WM_XCOMPONENT_SURFACE_CREATED) && (msgData.type <= MessageType::WM_XCOMPONENT_SURFACE_DESTROY)) {
            OH_NativeXComponent* nativexcomponet = reinterpret_cast<OH_NativeXComponent*>(msgData.data);
            if (msgData.type == MessageType::WM_XCOMPONENT_SURFACE_CREATED) {
            	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case WM_XCOMPONENT_SURFACE_CREATED"));
            	UE_LOG(LogOpenHarmony, Log, TEXT("Case WM_XCOMPONENT_SURFACE_CREATED"));
            	FAppEventManager::GetInstance()->HandleWindowCreated_EventThread(msgData.window);
            	bHasWindow = true;
            	ActivateApp_EventThread();
            } else if (msgData.type == MessageType::WM_XCOMPONENT_TOUCH_EVENT) {
                DispatchTouchEvent(nativexcomponet, msgData.window, msgData.touchEvent);
            } else if (msgData.type == MessageType::WM_XCOMPONENT_KEY_EVENT) {
			    DispatchKeyEvent(nativexcomponet, msgData.window, msgData.keyEvent);
			} else if (msgData.type == MessageType::WM_XCOMPONENT_MOUSE_EVENT) {
			    DispatchMouseEvent(nativexcomponet, msgData.window, msgData.mouseEvent);
			} else if (msgData.type == MessageType::WM_XCOMPONENT_MOUSE_WHEEL_EVENT) {
			    DispatchMouseWheelEvent(nativexcomponet, msgData.window, msgData.x, msgData.y, msgData.scrollY);
			} else if (msgData.type == MessageType::WM_XCOMPONENT_SURFACE_CHANGED) {
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case WM_XCOMPONENT_SURFACE_CHANGED"));
				uint64_t width_;
				uint64_t height_;
				int32_t ret = OH_NativeXComponent_GetXComponentSize(nativexcomponet, msgData.window, &width_, &height_);
				if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
					// 更新Buffer大小；由于XComponent在此场景下会重置BufferSize，在GameThread未处理完WindowResize逻辑前大概率出现闪黑（绘制内容填不满Buffer）；
					// 在此处主动更新一次Size保证Resize完成前的绘制内容填满屏幕。
					FOpenHarmonyWindow::UpdateWindowSizeWhenSurfaceReset(width_, height_);
					FAppEventManager::GetInstance()->HandleWindowResize_EventThread(msgData.window, width_, height_);
				}
            } else if (msgData.type == MessageType::WM_XCOMPONENT_SURFACE_DESTROY) {
            	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case WM_XCOMPONENT_SURFACE_DESTROY"));
            	SuspendApp_EventThread();

            	FAppEventManager::GetInstance()->HandleWindowClosed_EventThread();
            	bHasWindow = false;
            } else {
            }
            continue;
        }

        if (msgData.type == MessageType::WM_APP_SHOW) {
        	// 应用切前台
        	bIsResumed = true;
        	if (bHasWindow && bIsResumed)
        	{
        		ActivateApp_EventThread();
        	}
        	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case WM_APP_SHOW"));
        	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_ON_RESUME);
        } else if (msgData.type == MessageType::WM_APP_HIDE) {
        	// 应用切后台
        	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case WM_APP_HIDE"));

        	bIsResumed = false;
        	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_ON_PAUSE);

        	bool bAllowReboot = true;

        	/*
			 * On the initial loading the pause method must be called immediately
			 * in order to stop the startup movie's sound
			*/
        	if (IsPreLoadScreenPlaying() && bAllowReboot)
        	{
        		UE_LOG(LogOpenHarmony, Log, TEXT("MoviePlayer force completion"));
        		GetMoviePlayer()->ForceCompletion();
        	}

        	SuspendApp_EventThread();
        } else if (msgData.type == MessageType::WM_APP_DESTROY) {
        	// 应用退出
        	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case APP_CMD_DESTROY"));
        	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_ON_DESTROY);
        }
    	if(msgData.type == MessageType::WM_VSYNC) {
    		// render->runTask();
    	}
    	
    	if (EventHandlerEvent)
    	{
    		EventHandlerEvent->Trigger();
    	}
    }
}

static void OpenHarmonyEventThreadWorker(void* param)
{
	FOpenHarmonyMisc::SetThreadName("EventWorker");
	FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetMainGameMask());

	FPlatformMisc::LowLevelOutputDebugString(TEXT("Entering event processing thread engine entry point"));
	EventThreadID = FPlatformTLS::GetCurrentThreadId();

	PluginRender* app = reinterpret_cast<PluginRender*>(param);

	// 拉起libuv的事件循环
	app->workerLoop_ = uv_default_loop();
	uv_async_init(app->workerLoop_, &app->messageSignal_, reinterpret_cast<uv_async_cb>(onMessageCallback));

	// 处理掉事件循环启动前UI线程已传入的事件
	if(!app->messageQueue_.empty())
	{
		uv_async_send(&app->messageSignal_);
	}
	
	uv_run(app->workerLoop_, UV_RUN_DEFAULT);
}


int32_t HandleInputCB(int deviceID, int touchID, float inputX, float inputY, int OHtouchType)
{
	TArray<TouchInput> TouchesArray;
			
	TouchType type = TouchEnded;

	switch (OHtouchType)
		{
		case OH_NATIVEXCOMPONENT_DOWN:
			type = TouchBegan;
			break;

		case OH_NATIVEXCOMPONENT_MOVE:
			type = TouchMoved;
			break;

		case OH_NATIVEXCOMPONENT_UP:
		case OH_NATIVEXCOMPONENT_CANCEL:
			type = TouchEnded;
			break;

		case OH_NATIVEXCOMPONENT_UNKNOWN:
			return 0;

		default:

			return 0;
		}

	int32_t Width = 0 ;
	int32_t Height = 0 ;

	OHNativeWindow* Window = static_cast<OHNativeWindow*>(FOpenHarmonyWindow::GetHardwareWindow_EventThread());
	if (!Window)
	{
		return 0;
	}
	if(Window)
	{
		// we are on the event thread. true here indicates we will retrieve dimensions from the current window.
		FOpenHarmonyWindow::CalculateSurfaceSize(Width, Height, true);
	}

	FPlatformRect ScreenRect = FOpenHarmonyWindow::GetScreenRect(true);

	inputX = FMath::Min<float>(inputX / Width, 1.f);
	inputX *= (ScreenRect.Right - 1);
	inputY = FMath::Min<float>(inputY / Height, 1.f);
	inputY *= (ScreenRect.Bottom - 1);
		
	TouchInput TouchMessage;
	TouchMessage.DeviceId = deviceID;//need to check
	TouchMessage.Handle = touchID;
	TouchMessage.Type = type;
	TouchMessage.Position = FVector2D(inputX, inputY);
	TouchMessage.LastPosition = FVector2D(inputX, inputY);
	TouchesArray.Add(TouchMessage);
		
	FOpenHarmonyInputInterface::QueueTouchInput(TouchesArray);
	
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("HandleInputCB in FOpenHarmonyInputInterface::QueueTouchInput x=%f y = %f"), inputX, inputY);
	
	return 0;
}

void DispatchTouchEvent(OH_NativeXComponent* component, void* window, OH_NativeXComponent_TouchEvent* touchEvent)
{
	
	HandleInputCB(touchEvent->deviceId, touchEvent->id, touchEvent->x, touchEvent->y, touchEvent->type);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("- Touch Info : id: %d, x: %f, y: %f"), touchEvent->deviceId, touchEvent->id, touchEvent->x, touchEvent->y);

#if !UE_BUILD_SHIPPING
			//if ((touchEvent->numPoints >= 4) && (type == TouchBegan))
			if ((touchEvent->numPoints >= 4) && (touchEvent->type == OH_NATIVEXCOMPONENT_DOWN))
			{
				bool bShowConsole = true;
				GConfig->GetBool(TEXT("/Script/Engine.InputSettings"), TEXT("bShowConsoleOnFourFingerTap"), bShowConsole, GInputIni);

				if (bShowConsole)
				{
					GShowConsoleWindowNextTick = true;
				}
			}
#endif

	delete touchEvent;
}


void DispatchKeyEvent(OH_NativeXComponent* component, void* window, OH_NativeXComponent_KeyEvent* keyEvent)
{
	static KeyStateManager keyStateManager;
	OH_NativeXComponent_KeyAction action;
  OH_NativeXComponent_GetKeyEventAction(keyEvent, &action);
  OH_NativeXComponent_KeyCode code;
  OH_NativeXComponent_GetKeyEventCode(keyEvent, &code);
	FDeferredOpenHarmonyMessage Message;
	if (action == OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN) {
		Message.messageType = MessageType_KeyDown;
	} else if (action == OH_NATIVEXCOMPONENT_KEY_ACTION_UP) {
		Message.messageType = MessageType_KeyUp;
	}
	Message.KeyEventData.keyId = int32(code);
	Message.KeyEventData.unichar = int32(code);
	Message.KeyEventData.isShiftOn = keyStateManager.isShiftOn(action, code);
	FOpenHarmonyInputInterface::DeferMessage(Message);
}

void DispatchMouseEvent(OH_NativeXComponent* component, void* window, OH_NativeXComponent_MouseEvent mouseEvent)
{
	int32 mouseButton = -1;
	switch (mouseEvent.button) {
		case OH_NATIVEXCOMPONENT_LEFT_BUTTON:
				mouseButton = EMouseButtons::Type::Left;
				break;
		case OH_NATIVEXCOMPONENT_RIGHT_BUTTON:
				mouseButton = EMouseButtons::Type::Right;
				break;
		case OH_NATIVEXCOMPONENT_MIDDLE_BUTTON:
				mouseButton = EMouseButtons::Type::Middle;
				break;
		default:
				mouseButton = EMouseButtons::Type::Invalid;
  }
	
	switch (mouseEvent.action) {
		case OH_NATIVEXCOMPONENT_MOUSE_PRESS:
				FOpenHarmonyInputInterface::MouseButtonEvent(10, mouseButton, true);
				break;
		case OH_NATIVEXCOMPONENT_MOUSE_RELEASE:
				FOpenHarmonyInputInterface::MouseButtonEvent(10, mouseButton, false);
				break;
		case OH_NATIVEXCOMPONENT_MOUSE_MOVE:
				FOpenHarmonyInputInterface::MouseMoveEvent(10, mouseEvent.screenX, mouseEvent.screenY, mouseEvent.x, mouseEvent.y);
				break;
		default:
				return;
  }
}

void DispatchMouseWheelEvent(OH_NativeXComponent* component, void* window, float x, float y, float scrollY)
{
	FOpenHarmonyInputInterface::MouseWheelEvent(10, -scrollY);
}

//JS调用C++，回传JS侧选中的ConsoleCommand给UE侧
void Napi_nativeConsoleCommand(std::string commandString)
{
	FString Command(UTF8_TO_TCHAR(commandString.c_str()));
	if (GEngine != NULL)
	{
		// Run on game thread to avoid race condition with DeferredCommands
		AsyncTask(ENamedThreads::GameThread, [Command]()
		{
			GEngine->DeferredCommands.Add(Command);
		});
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Ignoring console command (too early): %s"), *Command);
	}
}

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeOnConfigurationChanged(boolean bPortrait);
void Napi_nativeOnConfigurationChanged(bool bPortrait)
{

	// enqueue a window changed event if orientation changed,
	// note that the HW window handle does not necessarily change.
	if (FOpenHarmonyWindow::OnWindowOrientationChanged(bPortrait))
	{
		// Enqueue an event to trigger gamethread to update the orientation:
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CHANGED);

		if (EventHandlerEvent)
		{
			EventHandlerEvent->Trigger();
		}
	}
}

void nativeOnWindowSizeChange(int width,int height)
{
	FOpenHarmonyWindow::SetWindowSize(width,height);
}

//JS调用C++，方法绑定
JSBIND_GLOBAL() { 
	JSBIND_FUNCTION(Napi_nativeConsoleCommand);
	JSBIND_FUNCTION(Napi_nativeOnConfigurationChanged);
	JSBIND_FUNCTION(nativeOnWindowSizeChange);
}

#endif