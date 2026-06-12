#pragma once

#include "InputTypes.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <stdint.h>

enum class InputSourceType : uint8_t
{
    Keyboard,
    MouseButton,
    GamepadButton,
    GamepadAxis,
    Pointer
};

enum class InputDeviceKind : uint8_t
{
    Keyboard,
    Mouse,
    Gamepad,

    Count
};

struct InputDeviceDescriptor
{
    InputDeviceKind kind = InputDeviceKind::Keyboard;
    int32_t gamepadIndex = -1;                       // valid only when kind == Gamepad
    GamepadType gamepadType = GamepadType::Standard; // valid only when kind == Gamepad
};

enum class AxisDirection : uint8_t
{
    Positive,
    Negative,
    Full
};

enum class TriggerMode : uint8_t
{
    SinglePress,
    Hold,
    KeepHeld,
    MultiPress
};

struct InputActionBinding
{
    InputSourceType sourceType = InputSourceType::Keyboard;
    int32_t code = 0;
    AxisDirection axisDirection = AxisDirection::Positive;
    float axisThreshold = 0.5f;
    int32_t gamepadIndex = 0;
    bool requireCtrl = false;
    bool requireShift = false;
    bool requireAlt = false;
};

struct InputActionTrigger
{
    TriggerMode mode = TriggerMode::SinglePress;
    float holdDuration = 1.0f;
    int32_t multiPressCount = 2;
    float multiPressWindow = 0.3f;
};

struct InputActionState
{
    bool isActive = false;
    bool wasJustActivated = false;
    bool wasJustDeactivated = false;
    float value = 0.0f;

    // Internal tracking
    bool rawDown = false;
    bool prevRawDown = false;
    float holdTimer = 0.0f;
    bool holdTriggered = false;
    int32_t pressCount = 0;
    float multiPressTimer = 0.0f;
};

struct InputAction
{
    std::string name;
    std::string category;
    std::vector<InputActionBinding> bindings;
    InputActionTrigger trigger;
    InputActionState state;
};

class PlayerInputSystem
{
public:

    static void Create();
    static void Destroy();
    static PlayerInputSystem* Get();

    void Update(float deltaTime);

    // Registration
    void RegisterAction(const std::string& category, const std::string& name,
                        TriggerMode mode = TriggerMode::SinglePress);
    void UnregisterAction(const std::string& category, const std::string& name);
    void AddBinding(const std::string& category, const std::string& name,
                    const InputActionBinding& binding);
    void ClearBindings(const std::string& category, const std::string& name);
    void SetTrigger(const std::string& category, const std::string& name,
                    const InputActionTrigger& trigger);

    // Queries (playerIndex: -1 = any, 0-3 = specific gamepad/pointer)
    bool IsActionActive(const std::string& category, const std::string& name, int32_t playerIndex = -1) const;
    bool WasActionJustActivated(const std::string& category, const std::string& name, int32_t playerIndex = -1) const;
    bool WasActionJustDeactivated(const std::string& category, const std::string& name, int32_t playerIndex = -1) const;
    float GetActionValue(const std::string& category, const std::string& name, int32_t playerIndex = -1) const;

    // Connected players
    int32_t GetPlayersConnected() const;

    // Bulk access (for editor/debugger)
    const std::vector<InputAction>& GetActions() const;
    InputAction* FindAction(const std::string& category, const std::string& name);
    const InputAction* FindAction(const std::string& category, const std::string& name) const;

    // Serialization
    void SaveProjectActions();
    void LoadProjectActions();

    // Replace the in-memory action set wholesale (used by the editor when
    // opening a specific InputActionsAsset). Rebuilds the lookup table.
    void SetActions(const std::vector<InputAction>& actions);

    // Enable/disable
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // Last-active input device. Updated each Update() based on edge-triggered
    // input. Consumers (e.g. InputPromptResolver) compare GetDeviceChangeFrame()
    // against a cached epoch to know when to re-resolve.
    //
    // If a forced device is set via SetForcedDevice(), GetLastActiveDevice
    // returns the forced descriptor instead of the auto-detected one. The
    // change-frame counter bumps whenever the forced device is set, changed,
    // or cleared, so InputPromptResolver flushes its cache and every widget
    // immediately reflects the override.
    const InputDeviceDescriptor& GetLastActiveDevice() const;
    uint32_t GetDeviceChangeFrame() const;

    // Editor-only force-device override. Used by the InputPromptMapInspector's
    // Test Device combobox so an artist can preview "what does the Gamepad.A
    // prompt look like on DualSense?" without unplugging real controllers.
    // Pass nullptr (or call ClearForcedDevice) to resume real auto-detection.
    void SetForcedDevice(const InputDeviceDescriptor& device);
    void ClearForcedDevice();
    bool HasForcedDevice() const;

    // Editor capture modal mute gate. While disabled, Update() still tracks the
    // last-active device and clears per-frame edge flags, but action evaluation
    // is skipped so a captured key doesn't also fire a registered game action.
    void SetActionEvaluationEnabled(bool enabled);
    bool AreInputActionsActive() const;

private:

    static PlayerInputSystem* sInstance;

    PlayerInputSystem();

    bool PollBindingDown(const InputActionBinding& binding, int32_t playerIndex = -1) const;
    float PollBindingValue(const InputActionBinding& binding, int32_t playerIndex = -1) const;
    bool PollActionRawDown(const InputAction& action, int32_t playerIndex = -1) const;
    float PollActionRawValue(const InputAction& action, int32_t playerIndex = -1) const;
    bool CheckModifiers(const InputActionBinding& binding) const;
    void EvaluateTrigger(InputAction& action, float deltaTime);

    std::string MakeKey(const std::string& category, const std::string& name) const;
    bool LoadFromJsonFile(const std::string& filePath);
    void RebuildLookup();

    void UpdateLastActiveDevice();

    std::vector<InputAction> mActions;
    std::unordered_map<std::string, size_t> mActionLookup;
    bool mEnabled = true;
    bool mActionEvaluationEnabled = true;

    InputDeviceDescriptor mLastActiveDevice;
    uint32_t mDeviceChangeFrame = 0;
    uint32_t mFrameCounter = 0;

    InputDeviceDescriptor mForcedDevice;
    bool mHasForcedDevice = false;
};
