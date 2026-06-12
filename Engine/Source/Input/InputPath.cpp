#include "InputPath.h"
#include "InputMap.h"

static const char* MouseButtonName(int32_t code)
{
    switch (code)
    {
    case MOUSE_LEFT:   return "Left";
    case MOUSE_RIGHT:  return "Right";
    case MOUSE_MIDDLE: return "Middle";
    case MOUSE_X1:     return "X1";
    case MOUSE_X2:     return "X2";
    default:           return "?";
    }
}

std::string MakeInputPath(const InputActionBinding& binding)
{
    switch (binding.sourceType)
    {
    case InputSourceType::Keyboard:
    {
        const char* name = InputMap::GetKeyCodeName(binding.code);
        return std::string("Keyboard/") + (name ? name : "?");
    }
    case InputSourceType::MouseButton:
        return std::string("Mouse/") + MouseButtonName(binding.code);
    case InputSourceType::GamepadButton:
    {
        const char* name = (binding.code >= 0 && binding.code < GAMEPAD_BUTTON_COUNT)
            ? InputMap::GetGamepadButtonName((GamepadButtonCode)binding.code)
            : "?";
        return std::string("Gamepad.Button/") + (name ? name : "?");
    }
    case InputSourceType::GamepadAxis:
    {
        const char* name = (binding.code >= 0 && binding.code < GAMEPAD_AXIS_COUNT)
            ? InputMap::GetGamepadAxisName((GamepadAxisCode)binding.code)
            : "?";
        const char* suffix =
            binding.axisDirection == AxisDirection::Positive ? "+" :
            binding.axisDirection == AxisDirection::Negative ? "-" : "";
        return std::string("Gamepad.Axis/") + (name ? name : "?") + suffix;
    }
    case InputSourceType::Pointer:
        return std::string("Pointer/") + std::to_string(binding.gamepadIndex);
    }
    return "?";
}

InputActionBinding MakeBinding(InputSourceType type, int32_t code,
                               AxisDirection axisDir, int32_t gamepadIndex)
{
    InputActionBinding b;
    b.sourceType = type;
    b.code = code;
    b.axisDirection = axisDir;
    b.gamepadIndex = gamepadIndex;
    return b;
}
