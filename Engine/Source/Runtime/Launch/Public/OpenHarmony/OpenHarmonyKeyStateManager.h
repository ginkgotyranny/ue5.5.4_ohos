#include <ace/xcomponent/native_xcomponent_key_event.h>

class KeyStateManager {
public:
    bool isShiftLeftOn = false;
    bool isShiftRightOn = false;

    bool isShiftOn(OH_NativeXComponent_KeyAction action, OH_NativeXComponent_KeyCode code) {
        if (code == KEY_SHIFT_LEFT) {
            isShiftLeftOn = (action == OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN);
        } else if (code == KEY_SHIFT_RIGHT) {
            isShiftRightOn = (action == OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN);
        }
        return isShiftLeftOn || isShiftRightOn;
    }
};