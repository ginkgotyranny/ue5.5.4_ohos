// Copyright Epic Games, Inc. All Rights Reserved.
#include "OpenHarmony/OpenHarmonyPlatformInput.h"

#if USE_OPENHARMONY_INPUT
#include <ace/xcomponent/native_xcomponent_key_event.h>
#endif
uint32 FOpenHarmonyPlatformInput::GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
#if USE_OPENHARMONY_INPUT

#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; if(KeyNames) { KeyNames[NumMappings]=KeyName; } ++NumMappings; };

	uint32 NumMappings = 0;

	if ( KeyCodes && (MaxMappings > 0) )
	{
		ADDKEYMAP( KEY_0, TEXT("Zero") );
		ADDKEYMAP( KEY_1, TEXT("One") );
		ADDKEYMAP( KEY_2, TEXT("Two") );
		ADDKEYMAP( KEY_3, TEXT("Three") );
		ADDKEYMAP( KEY_4, TEXT("Four") );
		ADDKEYMAP( KEY_5, TEXT("Five") );
		ADDKEYMAP( KEY_6, TEXT("Six") );
		ADDKEYMAP( KEY_7, TEXT("Seven") );
		ADDKEYMAP( KEY_8, TEXT("Eight") );
		ADDKEYMAP( KEY_9, TEXT("Nine") );

		ADDKEYMAP( KEY_A, TEXT("A") );
		ADDKEYMAP( KEY_B, TEXT("B") );
		ADDKEYMAP( KEY_C, TEXT("C") );
		ADDKEYMAP( KEY_D, TEXT("D") );
		ADDKEYMAP( KEY_E, TEXT("E") );
		ADDKEYMAP( KEY_F, TEXT("F") );
		ADDKEYMAP( KEY_G, TEXT("G") );
		ADDKEYMAP( KEY_H, TEXT("H") );
		ADDKEYMAP( KEY_I, TEXT("I") );
		ADDKEYMAP( KEY_J, TEXT("J") );
		ADDKEYMAP( KEY_K, TEXT("K") );
		ADDKEYMAP( KEY_L, TEXT("L") );
		ADDKEYMAP( KEY_M, TEXT("M") );
		ADDKEYMAP( KEY_N, TEXT("N") );
		ADDKEYMAP( KEY_O, TEXT("O") );
		ADDKEYMAP( KEY_P, TEXT("P") );
		ADDKEYMAP( KEY_Q, TEXT("Q") );
		ADDKEYMAP( KEY_R, TEXT("R") );
		ADDKEYMAP( KEY_S, TEXT("S") );
		ADDKEYMAP( KEY_T, TEXT("T") );
		ADDKEYMAP( KEY_U, TEXT("U") );
		ADDKEYMAP( KEY_V, TEXT("V") );
		ADDKEYMAP( KEY_W, TEXT("W") );
		ADDKEYMAP( KEY_X, TEXT("X") );
		ADDKEYMAP( KEY_Y, TEXT("Y") );
		ADDKEYMAP( KEY_Z, TEXT("Z") );

		ADDKEYMAP( KEY_SEMICOLON, TEXT("Semicolon") );
		ADDKEYMAP( KEY_EQUALS, TEXT("Equals") );
		ADDKEYMAP( KEY_COMMA, TEXT("Comma") );
		ADDKEYMAP( KEY_PERIOD, TEXT("Period") );
		ADDKEYMAP( KEY_SLASH, TEXT("Slash") );
		ADDKEYMAP( KEY_GRAVE, TEXT("Tilde") );
		ADDKEYMAP( KEY_LEFT_BRACKET, TEXT("LeftBracket") );
		ADDKEYMAP( KEY_BACKSLASH, TEXT("Backslash") );
		ADDKEYMAP( KEY_RIGHT_BRACKET, TEXT("RightBracket") );
		ADDKEYMAP( KEY_APOSTROPHE, TEXT("Quote") );
		ADDKEYMAP( KEY_SPACE, TEXT("SpaceBar") );


		ADDKEYMAP( KEY_DEL, TEXT("BackSpace") );
		ADDKEYMAP( KEY_TAB, TEXT("Tab") );
		ADDKEYMAP( KEY_ENTER, TEXT("Enter") );


		ADDKEYMAP( KEY_CAPS_LOCK, TEXT("CapsLock") );
		ADDKEYMAP( KEY_ESCAPE, TEXT("Escape") );
		ADDKEYMAP( KEY_SPACE, TEXT("SpaceBar") );

		ADDKEYMAP( KEY_DPAD_LEFT, TEXT("Left") );
		ADDKEYMAP( KEY_DPAD_UP, TEXT("Up") );
		ADDKEYMAP( KEY_DPAD_RIGHT, TEXT("Right") );
		ADDKEYMAP( KEY_DPAD_DOWN, TEXT("Down") );

		ADDKEYMAP( KEY_INSERT, TEXT("Insert") );
		ADDKEYMAP( KEY_FORWARD_DEL, TEXT("Delete") );


		ADDKEYMAP( KEY_F1, TEXT("F1") );
		ADDKEYMAP( KEY_F2, TEXT("F2") );
		ADDKEYMAP( KEY_F3, TEXT("F3") );
		ADDKEYMAP( KEY_F4, TEXT("F4") );
		ADDKEYMAP( KEY_F5, TEXT("F5") );
		ADDKEYMAP( KEY_F6, TEXT("F6") );
		ADDKEYMAP( KEY_F7, TEXT("F7") );
		ADDKEYMAP( KEY_F8, TEXT("F8") );
		ADDKEYMAP( KEY_F9, TEXT("F9") );
		ADDKEYMAP( KEY_F10, TEXT("F10") );
		ADDKEYMAP( KEY_F11, TEXT("F11") );
		ADDKEYMAP( KEY_F12, TEXT("F12") );


		ADDKEYMAP( KEY_SHIFT_LEFT, TEXT("LeftShift") );
		ADDKEYMAP( KEY_SHIFT_RIGHT, TEXT("RightShift") );
		ADDKEYMAP( KEY_CTRL_LEFT, TEXT("LeftControl") );
		ADDKEYMAP( KEY_CTRL_RIGHT, TEXT("RightControl") );
		ADDKEYMAP( KEY_ALT_LEFT, TEXT("LeftAlt") );
		ADDKEYMAP( KEY_ALT_RIGHT, TEXT("RightAlt") );




    // 暂时没有,可能后续会补充
		// ADDKEYMAP( AKEYCODE_BREAK, TEXT("Pause") );
    // ADDKEYMAP( AKEYCODE_PAGE_UP, TEXT("PageUp") );
		// ADDKEYMAP( AKEYCODE_PAGE_DOWN, TEXT("PageDown") );
    // ADDKEYMAP( AKEYCODE_META_LEFT, TEXT("LeftCommand") );
		// ADDKEYMAP( AKEYCODE_META_RIGHT, TEXT("RightCommand") );
    // ADDKEYMAP( AKEYCODE_NUM, TEXT("NumLock") );
		// ADDKEYMAP( VK_SCROLL, TEXT("ScrollLock") );
    // ADDKEYMAP( AKEYCODE_STAR, TEXT("Multiply") );
		// ADDKEYMAP( AKEYCODE_PLUS, TEXT("Add") );
		// ADDKEYMAP( AKEYCODE_MINUS, TEXT("Subtract") );
		// ADDKEYMAP( AKEYCODE_COMMA, TEXT("Decimal") );
		// ADDKEYMAP( AKEYCODE_SLASH, TEXT("Divide") );
    //ADDKEYMAP( VK_END, TEXT("End") );
		//ADDKEYMAP( VK_HOME, TEXT("Home") );
    // ADDKEYMAP( OH_NATIVEXCOMPONENT_LEFT_BUTTON, TEXT("LeftMouseButton") );
		// ADDKEYMAP( OH_NATIVEXCOMPONENT_RIGHT_BUTTON, TEXT("RightMouseButton") );
		// ADDKEYMAP( OH_NATIVEXCOMPONENT_MIDDLE_BUTTON, TEXT("MiddleMouseButton") );

		//ADDKEYMAP( AKEYCODE_BUTTON_THUMBL, TEXT("ThumbMouseButton") );
		//ADDKEYMAP( AKEYCODE_BUTTON_THUMBR, TEXT("ThumbMouseButton2") );
    //ADDKEYMAP( '-', TEXT("Underscore") );
	}

	check(NumMappings < MaxMappings);
	return NumMappings;
#undef ADDKEYMAP

#else
	// by default use the standard printable keys for chars
	return FGenericPlatformInput::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, true, true);
#endif
}

uint32 FOpenHarmonyPlatformInput::GetKeyMap( uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings )
{
#if USE_OPENHARMONY_INPUT

#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; if(KeyNames) { KeyNames[NumMappings]=KeyName; } ++NumMappings; };

	uint32 NumMappings = 0;

	if ( KeyCodes && (MaxMappings > 0) )
	{
		ADDKEYMAP( KEY_0, TEXT("Zero") );
		ADDKEYMAP( KEY_1, TEXT("One") );
		ADDKEYMAP( KEY_2, TEXT("Two") );
		ADDKEYMAP( KEY_3, TEXT("Three") );
		ADDKEYMAP( KEY_4, TEXT("Four") );
		ADDKEYMAP( KEY_5, TEXT("Five") );
		ADDKEYMAP( KEY_6, TEXT("Six") );
		ADDKEYMAP( KEY_7, TEXT("Seven") );
		ADDKEYMAP( KEY_8, TEXT("Eight") );
		ADDKEYMAP( KEY_9, TEXT("Nine") );

		ADDKEYMAP( KEY_A, TEXT("A") );
		ADDKEYMAP( KEY_B, TEXT("B") );
		ADDKEYMAP( KEY_C, TEXT("C") );
		ADDKEYMAP( KEY_D, TEXT("D") );
		ADDKEYMAP( KEY_E, TEXT("E") );
		ADDKEYMAP( KEY_F, TEXT("F") );
		ADDKEYMAP( KEY_G, TEXT("G") );
		ADDKEYMAP( KEY_H, TEXT("H") );
		ADDKEYMAP( KEY_I, TEXT("I") );
		ADDKEYMAP( KEY_J, TEXT("J") );
		ADDKEYMAP( KEY_K, TEXT("K") );
		ADDKEYMAP( KEY_L, TEXT("L") );
		ADDKEYMAP( KEY_M, TEXT("M") );
		ADDKEYMAP( KEY_N, TEXT("N") );
		ADDKEYMAP( KEY_O, TEXT("O") );
		ADDKEYMAP( KEY_P, TEXT("P") );
		ADDKEYMAP( KEY_Q, TEXT("Q") );
		ADDKEYMAP( KEY_R, TEXT("R") );
		ADDKEYMAP( KEY_S, TEXT("S") );
		ADDKEYMAP( KEY_T, TEXT("T") );
		ADDKEYMAP( KEY_U, TEXT("U") );
		ADDKEYMAP( KEY_V, TEXT("V") );
		ADDKEYMAP( KEY_W, TEXT("W") );
		ADDKEYMAP( KEY_X, TEXT("X") );
		ADDKEYMAP( KEY_Y, TEXT("Y") );
		ADDKEYMAP( KEY_Z, TEXT("Z") );

		ADDKEYMAP( KEY_SEMICOLON, TEXT("Semicolon") );
		ADDKEYMAP( KEY_EQUALS, TEXT("Equals") );
		ADDKEYMAP( KEY_COMMA, TEXT("Comma") );
		ADDKEYMAP( KEY_PERIOD, TEXT("Period") );
		ADDKEYMAP( KEY_SLASH, TEXT("Slash") );
		ADDKEYMAP( KEY_GRAVE, TEXT("Tilde") );
		ADDKEYMAP( KEY_LEFT_BRACKET, TEXT("LeftBracket") );
		ADDKEYMAP( KEY_BACKSLASH, TEXT("Backslash") );
		ADDKEYMAP( KEY_RIGHT_BRACKET, TEXT("RightBracket") );
		ADDKEYMAP( KEY_APOSTROPHE, TEXT("Quote") );
		ADDKEYMAP( KEY_SPACE, TEXT("SpaceBar") );


    ADDKEYMAP( KEY_DEL, TEXT("BackSpace") );
		ADDKEYMAP( KEY_TAB, TEXT("Tab") );
		ADDKEYMAP( KEY_ENTER, TEXT("Enter") );

    ADDKEYMAP( KEY_CAPS_LOCK, TEXT("CapsLock") );
		ADDKEYMAP( KEY_ESCAPE, TEXT("Escape") );
		ADDKEYMAP( KEY_SPACE, TEXT("SpaceBar") );

		ADDKEYMAP( KEY_DPAD_LEFT, TEXT("Left") );
		ADDKEYMAP( KEY_DPAD_UP, TEXT("Up") );
		ADDKEYMAP( KEY_DPAD_RIGHT, TEXT("Right") );
		ADDKEYMAP( KEY_DPAD_DOWN, TEXT("Down") );

		ADDKEYMAP( KEY_INSERT, TEXT("Insert") );
		ADDKEYMAP( KEY_FORWARD_DEL, TEXT("Delete") );

		ADDKEYMAP( KEY_F1, TEXT("F1") );
		ADDKEYMAP( KEY_F2, TEXT("F2") );
		ADDKEYMAP( KEY_F3, TEXT("F3") );
		ADDKEYMAP( KEY_F4, TEXT("F4") );
		ADDKEYMAP( KEY_F5, TEXT("F5") );
		ADDKEYMAP( KEY_F6, TEXT("F6") );
		ADDKEYMAP( KEY_F7, TEXT("F7") );
		ADDKEYMAP( KEY_F8, TEXT("F8") );
		ADDKEYMAP( KEY_F9, TEXT("F9") );
		ADDKEYMAP( KEY_F10, TEXT("F10") );
		ADDKEYMAP( KEY_F11, TEXT("F11") );
		ADDKEYMAP( KEY_F12, TEXT("F12") );


		ADDKEYMAP( KEY_SHIFT_LEFT, TEXT("LeftShift") );
		ADDKEYMAP( KEY_SHIFT_RIGHT, TEXT("RightShift") );
		ADDKEYMAP( KEY_CTRL_LEFT, TEXT("LeftControl") );
		ADDKEYMAP( KEY_CTRL_RIGHT, TEXT("RightControl") );
		ADDKEYMAP( KEY_ALT_LEFT, TEXT("LeftAlt") );
		ADDKEYMAP( KEY_ALT_RIGHT, TEXT("RightAlt") );


    // 暂时没有,可能后续会补充
		// ADDKEYMAP( VK_LWIN, TEXT("LeftCommand") );
		// ADDKEYMAP( VK_RWIN, TEXT("RightCommand") );
    // ADDKEYMAP( AKEYCODE_NUM, TEXT("NumLock") );
		// ADDKEYMAP( VK_SCROLL, TEXT("ScrollLock") );
    // ADDKEYMAP( AKEYCODE_AT, TEXT("At"));
		// ADDKEYMAP( AKEYCODE_POUND, TEXT("Pound"));
		// ADDKEYMAP( AKEYCODE_STAR, TEXT("Multiply") );
		// ADDKEYMAP( AKEYCODE_PLUS, TEXT("Add") );
		// ADDKEYMAP( AKEYCODE_MINUS, TEXT("Subtract") );
		// ADDKEYMAP( AKEYCODE_COMMA, TEXT("Decimal") );
		// ADDKEYMAP( AKEYCODE_SLASH, TEXT("Divide") );
    // ADDKEYMAP( AKEYCODE_PAGE_UP, TEXT("PageUp") );
		// ADDKEYMAP( AKEYCODE_PAGE_DOWN, TEXT("PageDown") );
		// ADDKEYMAP( VK_END, TEXT("End") );
		// ADDKEYMAP( VK_HOME, TEXT("Home") );
    // ADDKEYMAP( VK_PAUSE, TEXT("Pause") );
    // ADDKEYMAP( VK_LBUTTON, TEXT("LeftMouseButton") );
		// ADDKEYMAP( VK_RBUTTON, TEXT("RightMouseButton") );
		// ADDKEYMAP( VK_MBUTTON, TEXT("MiddleMouseButton") );
		// ADDKEYMAP( AKEYCODE_BUTTON_THUMBL, TEXT("ThumbMouseButton") );
		// ADDKEYMAP( AKEYCODE_BUTTON_THUMBR, TEXT("ThumbMouseButton2") );
    // ADDKEYMAP( '-', TEXT("Underscore") );
	}

	check(NumMappings < MaxMappings);
	return NumMappings;
#undef ADDKEYMAP

#else
	// nothing we can do by default here - sub-platforms may need to do stuff here
	return 0;
#endif
}
