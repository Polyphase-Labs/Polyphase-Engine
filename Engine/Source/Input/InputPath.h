#pragma once

#include "PlayerInputSystem.h"
#include "InputTypes.h"

#include <string>

// Canonical lookup-key synthesis for InputPromptMap.
//
// Keyboard      + POLYPHASE_KEY_F            -> "Keyboard/F"
// MouseButton   + MOUSE_LEFT                 -> "Mouse/Left"
// GamepadButton + GAMEPAD_A                  -> "Gamepad.Button/A"
// GamepadAxis   + GAMEPAD_AXIS_LTRIGGER +ve  -> "Gamepad.Axis/LTrigger+"
// Pointer       + (any)                      -> "Pointer/0"
//
// The path is stable across platforms (uses InputMap's display names) so a
// single InputPromptMap entry of "Keyboard/F" matches every host that resolves
// POLYPHASE_KEY_F.
std::string MakeInputPath(const InputActionBinding& binding);

// Inverse helper used by the editor capture modal — builds a binding from the
// last-pressed key/button/axis. The caller already knows the (sourceType, code,
// axisDirection); this just wraps the construction for readability.
InputActionBinding MakeBinding(InputSourceType type, int32_t code,
                               AxisDirection axisDir = AxisDirection::Positive,
                               int32_t gamepadIndex = 0);
