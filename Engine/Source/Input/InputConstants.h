#pragma once

#define INPUT_MAX_KEYS 256
#define INPUT_MAX_TOUCHES 4
#define INPUT_MAX_GAMEPADS 4

#if PLATFORM_WINDOWS
#define INPUT_KEYBOARD_SUPPORT 1
#define INPUT_MOUSE_SUPPORT 1
#define INPUT_TOUCH_SUPPORT 1
#define INPUT_GAMEPAD_SUPPORT 1
#elif PLATFORM_LINUX
#define INPUT_KEYBOARD_SUPPORT 1
#define INPUT_MOUSE_SUPPORT 1
#define INPUT_TOUCH_SUPPORT 1
#define INPUT_GAMEPAD_SUPPORT 1
#elif PLATFORM_GAMECUBE
#define INPUT_KEYBOARD_SUPPORT 0
#define INPUT_MOUSE_SUPPORT 0
#define INPUT_TOUCH_SUPPORT 0
#define INPUT_GAMEPAD_SUPPORT 1
#elif PLATFORM_WII
#define INPUT_KEYBOARD_SUPPORT 0
#define INPUT_MOUSE_SUPPORT 1
#define INPUT_TOUCH_SUPPORT 1 // Each wiimote IR treated as a separate pointer 
#define INPUT_GAMEPAD_SUPPORT 1
#elif PLATFORM_3DS
#define INPUT_KEYBOARD_SUPPORT 0
#define INPUT_MOUSE_SUPPORT 0
#define INPUT_TOUCH_SUPPORT 1
#define INPUT_GAMEPAD_SUPPORT 1
#elif PLATFORM_ANDROID
#define INPUT_KEYBOARD_SUPPORT 1
#define INPUT_MOUSE_SUPPORT 1
#define INPUT_TOUCH_SUPPORT 1
#define INPUT_GAMEPAD_SUPPORT 1
#else
// Fallback for console/addon platforms (e.g. POLYPHASE_PLATFORM_ADDON targets
// like PSP) that don't have their own #elif arm. Without these, every
// `#if INPUT_GAMEPAD_SUPPORT` in the engine silently compiles to false —
// most notably the `memcpy(mPrevGamepads, mGamepads, ...)` in
// InputUtils::InputAdvanceFrame, which means mPrevGamepads stays all-zeros
// forever and IsGamepadButtonJustDown returns TRUE every frame the button
// is held. Symptom on PSP: a single d-pad press cascades through 2-3 Button
// widget nav steps because each held frame re-fires the JustDown edge.
//
// Console default: gamepad only (no keyboard / mouse / touch hardware on
// PSP, GameCube, etc.). Touch-capable consoles (Wii IR, 3DS, Vita)
// override to 1 in their own #elif arm above.
#define INPUT_KEYBOARD_SUPPORT 0
#define INPUT_MOUSE_SUPPORT 0
#define INPUT_TOUCH_SUPPORT 0
#define INPUT_GAMEPAD_SUPPORT 1
#endif