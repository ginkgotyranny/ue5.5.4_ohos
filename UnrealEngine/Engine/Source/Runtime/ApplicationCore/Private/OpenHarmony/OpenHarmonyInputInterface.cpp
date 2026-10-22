#include "OpenHarmony/OpenHarmonyInputInterface.h"
//#include "OpenHarmonyInputDeviceMappings.h"
#include "Misc/ConfigCacheIni.h"
#include "IInputDevice.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "HAL/ThreadingBase.h"
#include "Misc/CallbackDevice.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "IHapticDevice.h"
#include <unordered_map>
#include <ace/xcomponent/native_xcomponent_key_event.h>
/*
#ifndef ANDROID_GAMEPAD_TRIGGER_THRESHOLD
	#define ANDROID_GAMEPAD_TRIGGER_THRESHOLD	0.30f
#endif
*/
TArray<TouchInput> FOpenHarmonyInputInterface::TouchInputStack = TArray<TouchInput>();
FCriticalSection FOpenHarmonyInputInterface::TouchInputCriticalSection;

FOpenHarmonyGamepadDeviceMapping FOpenHarmonyInputInterface::DeviceMapping[MAX_NUM_CONTROLLERS];

bool FOpenHarmonyInputInterface::VibeIsOn;
int32 FOpenHarmonyInputInterface::MaxVibeTime = 1000;
double FOpenHarmonyInputInterface::LastVibeUpdateTime = 0.0;
FForceFeedbackValues FOpenHarmonyInputInterface::VibeValues;

bool FOpenHarmonyInputInterface::bAllowControllers = true;
bool FOpenHarmonyInputInterface::bBlockOpenHarmonyKeysOnControllers = false;
bool FOpenHarmonyInputInterface::bControllersBlockDeviceFeedback = false;

FOpenHarmonyControllerData FOpenHarmonyInputInterface::OldControllerData[MAX_NUM_CONTROLLERS];
FOpenHarmonyControllerData FOpenHarmonyInputInterface::NewControllerData[MAX_NUM_CONTROLLERS];

FGamepadKeyNames::Type FOpenHarmonyInputInterface::ButtonMapping[MAX_NUM_CONTROLLER_BUTTONS];
float FOpenHarmonyInputInterface::InitialButtonRepeatDelay;
float FOpenHarmonyInputInterface::ButtonRepeatDelay;

FDeferredOpenHarmonyMessage FOpenHarmonyInputInterface::DeferredMessages[MAX_DEFERRED_MESSAGE_QUEUE_SIZE];
int32 FOpenHarmonyInputInterface::DeferredMessageQueueLastEntryIndex = 0;
int32 FOpenHarmonyInputInterface::DeferredMessageQueueDroppedCount   = 0;

TArray<FOpenHarmonyInputInterface::MotionData> FOpenHarmonyInputInterface::MotionDataStack
	= TArray<FOpenHarmonyInputInterface::MotionData>();

TArray<FOpenHarmonyInputInterface::MouseData> FOpenHarmonyInputInterface::MouseDataStack
	= TArray<FOpenHarmonyInputInterface::MouseData>();

int32 GOpenHarmonyOldXBoxWirelessFirmware = 0;
static FAutoConsoleVariableRef CVarOpenHarmonyOldXBoxWirelessFirmware(
	TEXT("OpenHarmony.OldXBoxWirelessFirmware"),
	GOpenHarmonyOldXBoxWirelessFirmware,
	TEXT("Determines how XBox Wireless controller mapping is handled. 0 assumes new firmware, 1 will use old firmware mapping (Default: 0)"),
	ECVF_Default);

TSharedRef< FOpenHarmonyInputInterface > FOpenHarmonyInputInterface::Create(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor)
{
	return MakeShareable(new FOpenHarmonyInputInterface(InMessageHandler, InCursor));
}

FOpenHarmonyInputInterface::~FOpenHarmonyInputInterface()
{
}

namespace OpenHarmonyKeyNames
{
	const FGamepadKeyNames::Type OpenHarmony_Back("OpenHarmony_Back");
	const FGamepadKeyNames::Type OpenHarmony_Menu("OpenHarmony_Menu");
}

FOpenHarmonyInputInterface::FOpenHarmonyInputInterface(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor)
	: MessageHandler( InMessageHandler )
	, Cursor(InCursor)
{
	
	ButtonMapping[0] = FGamepadKeyNames::FaceButtonBottom;
	ButtonMapping[1] = FGamepadKeyNames::FaceButtonRight;
	ButtonMapping[2] = FGamepadKeyNames::FaceButtonLeft;
	ButtonMapping[3] = FGamepadKeyNames::FaceButtonTop;
	ButtonMapping[4] = FGamepadKeyNames::LeftShoulder;
	ButtonMapping[5] = FGamepadKeyNames::RightShoulder;
	ButtonMapping[6] = FGamepadKeyNames::SpecialRight;
	ButtonMapping[7] = FGamepadKeyNames::SpecialLeft;
	ButtonMapping[8] = FGamepadKeyNames::LeftThumb;
	ButtonMapping[9] = FGamepadKeyNames::RightThumb;
	ButtonMapping[10] = FGamepadKeyNames::LeftTriggerThreshold;
	ButtonMapping[11] = FGamepadKeyNames::RightTriggerThreshold;
	ButtonMapping[12] = FGamepadKeyNames::DPadUp;
	ButtonMapping[13] = FGamepadKeyNames::DPadDown;
	ButtonMapping[14] = FGamepadKeyNames::DPadLeft;
	ButtonMapping[15] = FGamepadKeyNames::DPadRight;
	ButtonMapping[16] = OpenHarmonyKeyNames::OpenHarmony_Back;  // Technically just an alias for SpecialLeft
	ButtonMapping[17] = OpenHarmonyKeyNames::OpenHarmony_Menu;  // Technically just an alias for SpecialRight

	// Virtual buttons
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 0] = FGamepadKeyNames::LeftStickLeft;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 1] = FGamepadKeyNames::LeftStickRight;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 2] = FGamepadKeyNames::LeftStickUp;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 3] = FGamepadKeyNames::LeftStickDown;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 4] = FGamepadKeyNames::RightStickLeft;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 5] = FGamepadKeyNames::RightStickRight;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 6] = FGamepadKeyNames::RightStickUp;
	ButtonMapping[MAX_NUM_PHYSICAL_CONTROLLER_BUTTONS + 7] = FGamepadKeyNames::RightStickDown;

	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;

	GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("InitialButtonRepeatDelay"), InitialButtonRepeatDelay, GInputIni);
	GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("ButtonRepeatDelay"), ButtonRepeatDelay, GInputIni);

	VibeIsOn = false;

	for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
	{
		DeviceMapping[DeviceIndex].DeviceInfo.DeviceId = 0;
		DeviceMapping[DeviceIndex].DeviceState = MappingState::Unassigned;
	}
}

void FOpenHarmonyInputInterface::ResetGamepadAssignments()
{
	for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
	{
		if (DeviceMapping[DeviceIndex].DeviceState == MappingState::Valid)
		{
			//FCoreDelegates::OnControllerConnectionChange.Broadcast(false, -1, DeviceIndex);
		}

		DeviceMapping[DeviceIndex].DeviceInfo.DeviceId = 0;
		DeviceMapping[DeviceIndex].DeviceState = MappingState::Unassigned;
	}
}

void FOpenHarmonyInputInterface::ResetGamepadAssignmentToController(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
		return;

	if (DeviceMapping[ControllerId].DeviceState == MappingState::Valid)
	{
		//FCoreDelegates::OnControllerConnectionChange.Broadcast(false, -1, ControllerId);
	}

	DeviceMapping[ControllerId].DeviceInfo.DeviceId = 0;
	DeviceMapping[ControllerId].DeviceState = MappingState::Unassigned;
}

bool FOpenHarmonyInputInterface::IsControllerAssignedToGamepad(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
		return false;

	return (DeviceMapping[ControllerId].DeviceState == MappingState::Valid);
}

FString FOpenHarmonyInputInterface::GetGamepadControllerName(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
	{
		return FString(TEXT("None"));
	}

	if (DeviceMapping[ControllerId].DeviceState != MappingState::Valid)
	{
		return FString(TEXT("None"));
	}

	return DeviceMapping[ControllerId].DeviceInfo.Name;
}

void FOpenHarmonyInputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetMessageHandler(InMessageHandler);
	}
}

void FOpenHarmonyInputInterface::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		ExternalInputDevices.Add(InputDevice);
	}
}

//extern bool AndroidThunkCpp_GetInputDeviceInfo(int32 deviceId, FAndroidInputDeviceInfo &results);

void FOpenHarmonyInputInterface::Tick(float DeltaTime)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(DeltaTime);
	}

	if (VibeIsOn && 1000 * (FPlatformTime::Seconds() - LastVibeUpdateTime) > MaxVibeTime)
	{
		VibeIsOn = false;
		UpdateVibeMotors();
	}
}

void FOpenHarmonyInputInterface::SetLightColor(int32 ControllerId, FColor Color)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetLightColor(ControllerId, Color);
	}
}

void FOpenHarmonyInputInterface::ResetLightColor(int32 ControllerId)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->ResetLightColor(ControllerId);
	}
}

void FOpenHarmonyInputInterface::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	bool bDidFeedback = false;
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		if ((*DeviceIt)->SupportsForceFeedback(ControllerId))
		{
			bDidFeedback = true;
			(*DeviceIt)->SetChannelValue(ControllerId, ChannelType, Value);
		}
	}

	bDidFeedback |= IsGamepadAttached() && bControllersBlockDeviceFeedback;

	// If controller handled force feedback don't do it on the phone
	if (bDidFeedback)
	{
		return;
	}

	// Note: only one motor on Android at the moment, but remember all the settings
	// update will look at combination of all values to pick state

	// Save a copy of the value for future comparison
	switch (ChannelType)
	{
		case FForceFeedbackChannelType::LEFT_LARGE:
			VibeValues.LeftLarge = Value;
			break;

		case FForceFeedbackChannelType::LEFT_SMALL:
			VibeValues.LeftSmall = Value;
			break;

		case FForceFeedbackChannelType::RIGHT_LARGE:
			VibeValues.RightLarge = Value;
			break;

		case FForceFeedbackChannelType::RIGHT_SMALL:
			VibeValues.RightSmall = Value;
			break;

		default:
			// Unknown channel, so ignore it
			break;
	}

	// Update with the latest values (wait for SendControllerEvents later?)
	UpdateVibeMotors();
}

void FOpenHarmonyInputInterface::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
	bool bDidFeedback = false;
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		if ((*DeviceIt)->SupportsForceFeedback(ControllerId))
		{
			bDidFeedback = true;
			(*DeviceIt)->SetChannelValues(ControllerId, Values);
		}
	}

	bDidFeedback |= IsGamepadAttached() && bControllersBlockDeviceFeedback;

	// If controller handled force feedback don't do it on the phone
	if (bDidFeedback)
	{
		VibeValues.LeftLarge = VibeValues.RightLarge = VibeValues.LeftSmall = VibeValues.RightSmall = 0.0f;
	}
	else
	{
		VibeValues = Values;
	}

	// Note: only one motor on Android at the moment, but remember all the settings
	// update will look at combination of all values to pick state

	// Update with the latest values (wait for SendControllerEvents later?)
	UpdateVibeMotors();
}

void FOpenHarmonyInputInterface::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		IHapticDevice* HapticDevice = (*DeviceIt)->GetHapticDevice();
		if (HapticDevice)
		{
			HapticDevice->SetHapticFeedbackValues(ControllerId, Hand, Values);
		}
	}
}

//extern bool AndroidThunkCpp_IsGamepadAttached();

bool FOpenHarmonyInputInterface::IsGamepadAttached() const
{
	// Check for gamepads that have already been validated
	for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
	{
		FOpenHarmonyGamepadDeviceMapping& CurrentDevice = DeviceMapping[DeviceIndex];

		if (CurrentDevice.DeviceState == MappingState::Valid)
		{
			return true;
		}
	}

	for (auto DeviceIt = ExternalInputDevices.CreateConstIterator(); DeviceIt; ++DeviceIt)
	{
		if ((*DeviceIt)->IsGamepadAttached())
		{
			return true;
		}
	}

	//if all of this fails, do a check on the Java side to see if the gamepad is attached
	//return AndroidThunkCpp_IsGamepadAttached();
	return false;
}

extern void OpenHarmonyThunkCpp_Vibrate(int32_t Duration);

void FOpenHarmonyInputInterface::UpdateVibeMotors()
{
	// Use largest vibration state as value
	const float MaxLeft = VibeValues.LeftLarge > VibeValues.LeftSmall ? VibeValues.LeftLarge : VibeValues.LeftSmall;
	const float MaxRight = VibeValues.RightLarge > VibeValues.RightSmall ? VibeValues.RightLarge : VibeValues.RightSmall;
	const float Value = MaxLeft > MaxRight ? MaxLeft : MaxRight;

	if (VibeIsOn)
	{
		// Turn it off if below threshold
		if (Value < 0.3f)
		{
			OpenHarmonyThunkCpp_Vibrate(0);
			VibeIsOn = false;
		}
	}
	else {
		if (Value >= 0.3f)
		{
			// Turn it on for 10 seconds (or until below threshold)
			OpenHarmonyThunkCpp_Vibrate(MaxVibeTime);
			LastVibeUpdateTime = FPlatformTime::Seconds();
			VibeIsOn = true;
		}
	}
}


extern void OpenHarmonyThunkCpp_PushSensorEvents();

int32 lastX = -1;
int32 lastY = -1;
int count = 0;
static std::unordered_map<int32, uint32> ohCharMap = {
		{int32(KEY_0), L'0'},
		{int32(KEY_1), L'1'},
		{int32(KEY_2), L'2'},
		{int32(KEY_3), L'3'},
		{int32(KEY_4), L'4'},
		{int32(KEY_5), L'5'},
		{int32(KEY_6), L'6'},
		{int32(KEY_7), L'7'},
		{int32(KEY_8), L'8'},
		{int32(KEY_9), L'9'},
		{int32(KEY_A), L'a'},
		{int32(KEY_B), L'b'},
		{int32(KEY_C), L'c'},
		{int32(KEY_D), L'd'},
		{int32(KEY_E), L'e'},
		{int32(KEY_F), L'f'},
		{int32(KEY_G), L'g'},
		{int32(KEY_H), L'h'},
		{int32(KEY_I), L'i'},
		{int32(KEY_J), L'j'},
		{int32(KEY_K), L'k'},
		{int32(KEY_L), L'l'},
		{int32(KEY_M), L'm'},
		{int32(KEY_N), L'n'},
		{int32(KEY_O), L'o'},
		{int32(KEY_P), L'p'},
		{int32(KEY_Q), L'q'},
		{int32(KEY_R), L'r'},
		{int32(KEY_S), L's'},
		{int32(KEY_T), L't'},
		{int32(KEY_U), L'u'},
		{int32(KEY_V), L'v'},
		{int32(KEY_W), L'w'},
		{int32(KEY_X), L'x'},
		{int32(KEY_Y), L'y'},
		{int32(KEY_Z), L'z'},
    {int32(KEY_GRAVE), L'`'},
    {int32(KEY_MINUS), L'-'},
    {int32(KEY_EQUALS), L'='},
    {int32(KEY_DEL), L'\b'},
    {int32(KEY_TAB), L'\t'},
    {int32(KEY_LEFT_BRACKET), L'['},
    {int32(KEY_RIGHT_BRACKET), L']'},
    {int32(KEY_BACKSLASH), L'\\'},
    {int32(KEY_SEMICOLON), L';'},
    {int32(KEY_APOSTROPHE), L'\''},
    {int32(KEY_ENTER), L'\n'},
    {int32(KEY_COMMA), L','},
    {int32(KEY_PERIOD), L'.'},
    {int32(KEY_SLASH), L'/'},
    {int32(KEY_SPACE), L' '},
};
static std::unordered_map<int32, uint32> ohCharMapShift = {
		{int32(KEY_0), L')'},
		{int32(KEY_1), L'!'},
		{int32(KEY_2), L'@'},
		{int32(KEY_3), L'#'},
		{int32(KEY_4), L'$'},
		{int32(KEY_5), L'%'},
		{int32(KEY_6), L'^'},
		{int32(KEY_7), L'&'},
		{int32(KEY_8), L'*'},
		{int32(KEY_9), L'('},
		{int32(KEY_A), L'A'},
		{int32(KEY_B), L'B'},
		{int32(KEY_C), L'C'},
		{int32(KEY_D), L'D'},
		{int32(KEY_E), L'E'},
		{int32(KEY_F), L'F'},
		{int32(KEY_G), L'G'},
		{int32(KEY_H), L'H'},
		{int32(KEY_I), L'I'},
		{int32(KEY_J), L'J'},
		{int32(KEY_K), L'K'},
		{int32(KEY_L), L'L'},
		{int32(KEY_M), L'M'},
		{int32(KEY_N), L'N'},
		{int32(KEY_O), L'O'},
		{int32(KEY_P), L'P'},
		{int32(KEY_Q), L'Q'},
		{int32(KEY_R), L'R'},
		{int32(KEY_S), L'S'},
		{int32(KEY_T), L'T'},
		{int32(KEY_U), L'U'},
		{int32(KEY_V), L'V'},
		{int32(KEY_W), L'W'},
		{int32(KEY_X), L'X'},
		{int32(KEY_Y), L'Y'},
		{int32(KEY_Z), L'Z'},
    {int32(KEY_GRAVE), L'~'},
    {int32(KEY_MINUS), L'_'},
    {int32(KEY_EQUALS), L'+'},
    {int32(KEY_DEL), L'\b'},
    {int32(KEY_TAB), L'\t'},
    {int32(KEY_LEFT_BRACKET), L'{'},
    {int32(KEY_RIGHT_BRACKET), L'}'},
    {int32(KEY_BACKSLASH), L'|'},
    {int32(KEY_SEMICOLON), L':'},
    {int32(KEY_APOSTROPHE), L'"'},
    {int32(KEY_ENTER), L'\n'},
    {int32(KEY_COMMA), L'<'},
    {int32(KEY_PERIOD), L'>'},
    {int32(KEY_SLASH), L'?'},
    {int32(KEY_SPACE), L' '},
};

int32 ohKeyToChar(int32 ohKeyCode, bool isShiftOn)
{
    const auto& mapToUse = isShiftOn ? ohCharMapShift : ohCharMap;
    auto it = mapToUse.find(ohKeyCode);
    if (it != mapToUse.end()) {
        return it->second;
    }
    return ohKeyCode;
}

void FOpenHarmonyInputInterface::SendControllerEvents()
{
	OpenHarmonyThunkCpp_PushSensorEvents();
	
	FScopeLock Lock(&TouchInputCriticalSection);


	for(int i = 0; i < FOpenHarmonyInputInterface::TouchInputStack.Num(); ++i)
	{
		TouchInput Touch = FOpenHarmonyInputInterface::TouchInputStack[i];
		int32 ControllerId = FindExistingDevice(Touch.DeviceId);
		ControllerId = (ControllerId == -1) ? 0 : ControllerId;

		// send input to handler
		switch ( Touch.Type )
		{
		case TouchBegan:
			MessageHandler->OnTouchStarted(nullptr, Touch.Position, 1.0f, Touch.Handle, ControllerId);
			break;
		case TouchEnded:
			MessageHandler->OnTouchEnded(Touch.Position, Touch.Handle, ControllerId);
			break;
		case TouchMoved:
			MessageHandler->OnTouchMoved(Touch.Position, 1.0f, Touch.Handle, ControllerId);
			break;
		}
	}

	for (int i = 0; i < FOpenHarmonyInputInterface::MotionDataStack.Num(); ++i)
	{
		MotionData motion_data = FOpenHarmonyInputInterface::MotionDataStack[i];

		MessageHandler->OnMotionDetected(
			motion_data.Tilt, motion_data.RotationRate,
			motion_data.Gravity, motion_data.Acceleration,
			0);
	}

	for (int i = 0; i < FOpenHarmonyInputInterface::MouseDataStack.Num(); ++i)
	{
		MouseData mouse_data = FOpenHarmonyInputInterface::MouseDataStack[i];

		switch (mouse_data.EventType)
		{
			case MouseEventType::MouseMove:
				if (Cursor.IsValid())
				{
					if (lastX == -1 && lastY == -1) {
						Cursor->SetPosition(mouse_data.DeltaX, mouse_data.DeltaY);
					// OH�ϵ�mouse�¼�̫�����ˣ���������100��֮���ٴ���move�¼�,��Ӱ��ʵ��Ч��
					} else if (count > 100) {
						Cursor->SetPosition(mouse_data.DeltaX, mouse_data.DeltaY);
						MessageHandler->OnMouseMove();
					} else {
						count++;
					}
				}
				if (lastX == -1 && lastY == -1) {
					lastX = mouse_data.DeltaX;
					lastY = mouse_data.DeltaY;
				}
				// relative move
				MessageHandler->OnRawMouseMove(mouse_data.DeltaX - lastX, mouse_data.DeltaY - lastY);
				lastX = mouse_data.DeltaX;
				lastY = mouse_data.DeltaY;
				break;

			case MouseEventType::MouseWheel:
				MessageHandler->OnMouseWheel(mouse_data.WheelDelta);
				break;

			case MouseEventType::MouseButtonDown:
				MessageHandler->OnMouseDown(nullptr, mouse_data.Button);
				break;

			case MouseEventType::MouseButtonUp:
				MessageHandler->OnMouseUp(mouse_data.Button);
				break;
		}
	}

	for (int32 MessageIndex = 0; MessageIndex < FMath::Min(DeferredMessageQueueLastEntryIndex, MAX_DEFERRED_MESSAGE_QUEUE_SIZE); ++MessageIndex)
	{
		const FDeferredOpenHarmonyMessage& DeferredMessage = DeferredMessages[MessageIndex];
		const int32 Char = ohKeyToChar(DeferredMessage.KeyEventData.keyId, DeferredMessage.KeyEventData.isShiftOn);
		switch (DeferredMessage.messageType)
		{

			case MessageType_KeyDown:

				MessageHandler->OnKeyDown(DeferredMessage.KeyEventData.keyId, Char, DeferredMessage.KeyEventData.isRepeat);
				MessageHandler->OnKeyChar(Char,  DeferredMessage.KeyEventData.isRepeat);
				break;

			case MessageType_KeyUp:

				MessageHandler->OnKeyUp(DeferredMessage.KeyEventData.keyId, Char, false);
				break;
		} 
	}

	if (DeferredMessageQueueDroppedCount)
	{
		//should warn that messages got dropped, which message queue?
		DeferredMessageQueueDroppedCount = 0;
	}

	DeferredMessageQueueLastEntryIndex = 0;

	FOpenHarmonyInputInterface::TouchInputStack.Empty(0);

	FOpenHarmonyInputInterface::MotionDataStack.Empty();

	FOpenHarmonyInputInterface::MouseDataStack.Empty();

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SendControllerEvents();
	}
}

void FOpenHarmonyInputInterface::QueueTouchInput(const TArray<TouchInput>& InTouchEvents)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	FOpenHarmonyInputInterface::TouchInputStack.Append(InTouchEvents);
}

int32 FOpenHarmonyInputInterface::FindExistingDevice(int32 deviceId)
{
	if (!bAllowControllers)
	{
		return -1;
	}
	
	// Treat non-positive devices ids special
	if (deviceId < 1)
		return -1;

	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_CONTROLLERS; ControllerIndex++)
	{
		if (DeviceMapping[ControllerIndex].DeviceInfo.DeviceId == deviceId && DeviceMapping[ControllerIndex].DeviceState == MappingState::Valid)
		{
			return ControllerIndex;
		}
	}

	// Did not find it
	return -1;
}

FOpenHarmonyGamepadDeviceMapping* FOpenHarmonyInputInterface::GetDeviceMapping(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
	{
		return nullptr;
	}
	return &DeviceMapping[ControllerId];
}

int32 FOpenHarmonyInputInterface::GetControllerIndex(int32 deviceId)
{
	if (!bAllowControllers)
	{
		return -1;
	}
	
	// Treat non-positive device ids special (always controller 0)
	if (deviceId < 1)
		return 0;

	// Look for this deviceId in controllers discovered
	int32 UnassignedIndex = -1;
	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_CONTROLLERS; ControllerIndex++)
	{
		if (DeviceMapping[ControllerIndex].DeviceState == MappingState::Unassigned)
		{
			if (UnassignedIndex == -1)
				UnassignedIndex = ControllerIndex;
			continue;
		}

		if (DeviceMapping[ControllerIndex].DeviceInfo.DeviceId == deviceId)
		{
			return ControllerIndex;
		}
	}

	// Haven't seen this one before, make sure there is room for a new one
	if (UnassignedIndex == -1)
		return -1;

	// Register it
	DeviceMapping[UnassignedIndex].DeviceInfo.DeviceId = deviceId;
	OldControllerData[UnassignedIndex].DeviceId = UnassignedIndex;
	NewControllerData[UnassignedIndex].DeviceId = UnassignedIndex;

	// Mark it for validation later
	DeviceMapping[UnassignedIndex].DeviceState = MappingState::ToValidate;

	return UnassignedIndex;
}

void FOpenHarmonyInputInterface::MouseMoveEvent(int32 deviceId, float absoluteX, float absoluteY, float deltaX, float deltaY)
{
	// for now only deal with one mouse
	FScopeLock Lock(&TouchInputCriticalSection);

	FOpenHarmonyInputInterface::MouseDataStack.Push(
		MouseData{ MouseEventType::MouseMove, EMouseButtons::Invalid, (int32)absoluteX, (int32)absoluteY, (int32)deltaX, (int32)deltaY, 0.0f });
}

void FOpenHarmonyInputInterface::MouseWheelEvent(int32 deviceId, float wheelDelta)
{
	// for now only deal with one mouse
	FScopeLock Lock(&TouchInputCriticalSection);

	FOpenHarmonyInputInterface::MouseDataStack.Push(
		MouseData{ MouseEventType::MouseWheel, EMouseButtons::Invalid, 0, 0, 0, 0, wheelDelta });
}

void FOpenHarmonyInputInterface::MouseButtonEvent(int32 deviceId, int32 buttonId, bool buttonDown)
{
	// for now only deal with one mouse
	FScopeLock Lock(&TouchInputCriticalSection);

	MouseEventType EventType = buttonDown ? MouseEventType::MouseButtonDown : MouseEventType::MouseButtonUp;

	EMouseButtons::Type UE4Button = (buttonId == 0) ? EMouseButtons::Left : (buttonId == 1) ? EMouseButtons::Middle : EMouseButtons::Right;
	FOpenHarmonyInputInterface::MouseDataStack.Push(
		MouseData{ EventType, UE4Button, 0, 0, 0, 0, 0.0f });
}

void FOpenHarmonyInputInterface::DeferMessage(const FDeferredOpenHarmonyMessage& DeferredMessage)
{
	FScopeLock Lock(&TouchInputCriticalSection);
	// Get the index we should be writing to
	int32 Index = DeferredMessageQueueLastEntryIndex++;

	if (Index >= MAX_DEFERRED_MESSAGE_QUEUE_SIZE)
	{
		// Actually, if the queue is full, drop the message and increment a counter of drops
		DeferredMessageQueueDroppedCount++;
		return;
	}
	DeferredMessages[Index] = DeferredMessage;
}

void FOpenHarmonyInputInterface::QueueMotionData(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	FOpenHarmonyInputInterface::MotionDataStack.Push(
		MotionData { Tilt, RotationRate, Gravity, Acceleration });
}

