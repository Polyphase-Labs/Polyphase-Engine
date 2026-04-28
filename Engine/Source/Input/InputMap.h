#pragma once

#include "InputTypes.h"

#include <stdint.h>
#include <string>
#include <vector>

class InputMap
{
public:

    static void Create();
    static void Destroy();
    static InputMap* Get();

    void ResetToDefaults();

    // Button mappings (keyboard key -> gamepad button)
    void SetButtonMapping(GamepadButtonCode button, int32_t keyCode);
    int32_t GetButtonMapping(GamepadButtonCode button) const;

    // Axis mappings (keyboard keys -> gamepad axis)
    void SetAxisMapping(GamepadAxisCode axis, int32_t positiveKey, int32_t negativeKey);
    void GetAxisMapping(GamepadAxisCode axis, int32_t& positiveKey, int32_t& negativeKey) const;

    // Mouse-to-axis mapping
    void SetAxisMouseMapping(GamepadAxisCode axis, bool useMouseX, bool useMouseY, float sensitivity);
    bool GetAxisUsesMouseX(GamepadAxisCode axis) const;
    bool GetAxisUsesMouseY(GamepadAxisCode axis) const;
    float GetAxisMouseSensitivity(GamepadAxisCode axis) const;

    // Mouse/Pointer enable/disable for debugging
    void SetMouseEnabled(bool enabled);
    bool IsMouseEnabled() const;
    void SetPointerEnabled(bool enabled);
    bool IsPointerEnabled() const;

    // Query: is this gamepad button triggered via a mapped keyboard key?
    bool IsButtonDownViaKeyboard(GamepadButtonCode button) const;
    bool IsButtonJustDownViaKeyboard(GamepadButtonCode button) const;
    bool IsButtonJustUpViaKeyboard(GamepadButtonCode button) const;

    // Query: get axis value from mapped keyboard keys and/or mouse
    float GetAxisValueFromMapping(GamepadAxisCode axis) const;

    // Presets (saved globally to user's AppData)
    bool SavePreset(const std::string& name) const;
    bool LoadPreset(const std::string& name);
    bool DeletePreset(const std::string& name);
    std::vector<std::string> GetPresetNames() const;
    static std::string GetPresetsDirectory();

    // Display string helpers
    static const char* GetKeyCodeName(int32_t keyCode);
    static const char* GetGamepadButtonName(GamepadButtonCode button);
    static const char* GetGamepadAxisName(GamepadAxisCode axis);

private:

    static InputMap* sInstance;

    InputMap();

    int32_t mButtonMappings[GAMEPAD_BUTTON_COUNT];
    int32_t mAxisPositiveKeys[GAMEPAD_AXIS_COUNT];
    int32_t mAxisNegativeKeys[GAMEPAD_AXIS_COUNT];
    bool mAxisUseMouseX[GAMEPAD_AXIS_COUNT];
    bool mAxisUseMouseY[GAMEPAD_AXIS_COUNT];
    float mAxisMouseSensitivity[GAMEPAD_AXIS_COUNT];
    bool mMouseEnabled = true;
    bool mPointerEnabled = true;
};
