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

    // Enable/disable
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

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

    std::vector<InputAction> mActions;
    std::unordered_map<std::string, size_t> mActionLookup;
    bool mEnabled = true;
};
