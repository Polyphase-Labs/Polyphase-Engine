#if PLATFORM_MAC

// macOS input backend. Keyboard and mouse events are fed by the NSEvent pump
// in System_MacCocoa.mm; this file owns gamepads (GameController.framework, polled
// every frame) and the cursor show/lock/trap/warp state.

#include "Input/Input.h"
#include "Input/InputUtils.h"

#include "Engine.h"
#include "Log.h"
#include "Profiler.h"

#import <Cocoa/Cocoa.h>
#import <GameController/GameController.h>
#import <CoreGraphics/CoreGraphics.h>

bool gWarpCursor = false;
int32_t gWarpCursorX = -1;
int32_t gWarpCursorY = -1;

static GCController* sSlots[INPUT_MAX_GAMEPADS] = { nil, nil, nil, nil };
static bool sCursorHiddenByEngine = false;

static GamepadType ClassifyController(GCController* controller)
{
    if (@available(macOS 11.0, *))
    {
        NSString* category = controller.productCategory;
        if ([category isEqualToString:GCProductCategoryDualShock4])
            return GamepadType::DualShock4;
        if ([category isEqualToString:GCProductCategoryDualSense])
            return GamepadType::DualSense;
    }
    return GamepadType::Standard;
}

static void ReconcileControllers()
{
    NSArray<GCController*>* controllers = GCController.controllers;
    InputState& input = GetEngineState()->mInput;

    // Drop slots whose controller went away.
    for (int32_t i = 0; i < INPUT_MAX_GAMEPADS; ++i)
    {
        if (sSlots[i] != nil && ![controllers containsObject:sSlots[i]])
        {
            sSlots[i] = nil;
            input.mGamepads[i] = GamepadState();
        }
    }

    // Fill empty slots with newly connected controllers.
    for (GCController* controller in controllers)
    {
        if (controller.extendedGamepad == nil)
            continue;

        bool known = false;
        for (int32_t i = 0; i < INPUT_MAX_GAMEPADS; ++i)
        {
            if (sSlots[i] == controller) { known = true; break; }
        }
        if (known)
            continue;

        for (int32_t i = 0; i < INPUT_MAX_GAMEPADS; ++i)
        {
            if (sSlots[i] == nil)
            {
                sSlots[i] = controller;
                input.mGamepads[i] = GamepadState();
                input.mGamepads[i].mConnected = true;
                input.mGamepads[i].mDevice = i;
                input.mGamepads[i].mType = ClassifyController(controller);
                LogDebug("Gamepad %d connected: %s", i, controller.vendorName.UTF8String);
                break;
            }
        }
    }
}

static void PollController(int32_t index)
{
    GCController* controller = sSlots[index];
    if (controller == nil)
        return;

    GCExtendedGamepad* gp = controller.extendedGamepad;
    if (gp == nil)
        return;

    GamepadState& pad = GetEngineState()->mInput.mGamepads[index];
    pad.mConnected = true;

    pad.mButtons[GAMEPAD_A] = gp.buttonA.isPressed;
    pad.mButtons[GAMEPAD_B] = gp.buttonB.isPressed;
    pad.mButtons[GAMEPAD_X] = gp.buttonX.isPressed;
    pad.mButtons[GAMEPAD_Y] = gp.buttonY.isPressed;
    pad.mButtons[GAMEPAD_L1] = gp.leftShoulder.isPressed;
    pad.mButtons[GAMEPAD_R1] = gp.rightShoulder.isPressed;
    pad.mButtons[GAMEPAD_LEFT] = gp.dpad.left.isPressed;
    pad.mButtons[GAMEPAD_RIGHT] = gp.dpad.right.isPressed;
    pad.mButtons[GAMEPAD_UP] = gp.dpad.up.isPressed;
    pad.mButtons[GAMEPAD_DOWN] = gp.dpad.down.isPressed;

    if (@available(macOS 10.15, *))
    {
        pad.mButtons[GAMEPAD_START] = gp.buttonMenu.isPressed;
        pad.mButtons[GAMEPAD_SELECT] = (gp.buttonOptions != nil) && gp.buttonOptions.isPressed;
    }
    if (@available(macOS 11.0, *))
    {
        pad.mButtons[GAMEPAD_HOME] = (gp.buttonHome != nil) && gp.buttonHome.isPressed;
    }
    if (@available(macOS 10.14.1, *))
    {
        pad.mButtons[GAMEPAD_THUMBL] = (gp.leftThumbstickButton != nil) && gp.leftThumbstickButton.isPressed;
        pad.mButtons[GAMEPAD_THUMBR] = (gp.rightThumbstickButton != nil) && gp.rightThumbstickButton.isPressed;
    }

    float lt = gp.leftTrigger.value;
    float rt = gp.rightTrigger.value;
    pad.mAxes[GAMEPAD_AXIS_LTRIGGER] = lt;
    pad.mAxes[GAMEPAD_AXIS_RTRIGGER] = rt;
    // Same trigger-as-button rule as the Linux joystick backend.
    pad.mButtons[GAMEPAD_L2] = lt > 0.5f;
    pad.mButtons[GAMEPAD_R2] = rt > 0.5f;

    // GameController reports y up-positive, which matches what the engine
    // expects (Linux inverts the raw evdev axis to get here).
    pad.mAxes[GAMEPAD_AXIS_LTHUMB_X] = glm::clamp(gp.leftThumbstick.xAxis.value, -1.0f, 1.0f);
    pad.mAxes[GAMEPAD_AXIS_LTHUMB_Y] = glm::clamp(gp.leftThumbstick.yAxis.value, -1.0f, 1.0f);
    pad.mAxes[GAMEPAD_AXIS_RTHUMB_X] = glm::clamp(gp.rightThumbstick.xAxis.value, -1.0f, 1.0f);
    pad.mAxes[GAMEPAD_AXIS_RTHUMB_Y] = glm::clamp(gp.rightThumbstick.yAxis.value, -1.0f, 1.0f);
}

void INP_Initialize()
{
    InputInit();
}

void INP_Shutdown()
{
    InputShutdown();
    for (int32_t i = 0; i < INPUT_MAX_GAMEPADS; ++i)
    {
        sSlots[i] = nil;
    }
}

void INP_Update()
{
    {
        SCOPED_FRAME_STAT("INP.Advance");
        InputAdvanceFrame();
    }

    {
        SCOPED_FRAME_STAT("INP.Gamepad");
        @autoreleasepool
        {
            ReconcileControllers();
            for (int32_t i = 0; i < INPUT_MAX_GAMEPADS; ++i)
            {
                PollController(i);
            }
        }
    }

    {
        SCOPED_FRAME_STAT("INP.Post");
        InputPostUpdate();
    }
}

void INP_SetCursorPos(int32_t x, int32_t y)
{
    gWarpCursor = true;
    gWarpCursorX = x;
    gWarpCursorY = y;

    INP_SetMousePosition(x, y);
}

void INP_ShowCursor(bool show)
{
    // NSCursor hide/unhide is a counted pair; keep it balanced.
    if (!show && !sCursorHiddenByEngine)
    {
        [NSCursor hide];
        sCursorHiddenByEngine = true;
    }
    else if (show && sCursorHiddenByEngine)
    {
        [NSCursor unhide];
        sCursorHiddenByEngine = false;
    }
}

bool INP_IsCursorHiddenByEngine()
{
    return sCursorHiddenByEngine;
}

void INP_LockCursor(bool lock)
{
    InputState& input = GetEngineState()->mInput;
    input.mCursorLocked = lock;
}

void INP_TrapCursor(bool trap)
{
    SystemState& system = GetEngineState()->mSystem;
    InputState& input = GetEngineState()->mInput;

    if (system.mWindowHasFocus)
    {
        // Detaching the cursor from mouse movement is the closest thing to an
        // X11 pointer grab: the cursor stays put and System_MacCocoa.mm reads
        // per-event deltas while trapped.
        CGAssociateMouseAndMouseCursorPosition(trap ? false : true);
    }
    else if (!trap)
    {
        CGAssociateMouseAndMouseCursorPosition(true);
    }

    input.mCursorTrapped = trap;
}

void INP_TrapCursorToRect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    // No sub-window confinement on macOS; fall back to full-window trap.
    INP_TrapCursor(true);
}

const char* INP_ShowSoftKeyboard(bool show)
{
    return nullptr;
}

bool INP_IsSoftKeyboardShown()
{
    return false;
}

#endif
