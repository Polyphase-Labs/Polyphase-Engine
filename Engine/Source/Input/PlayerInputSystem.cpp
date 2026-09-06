#include "PlayerInputSystem.h"
#include "Input/InputActionsAsset.h"
#include "Input/InputMap.h"
#include "Input/InputPromptResolver.h"
#include "Input.h"
#include "Engine.h"
#include "AssetManager.h"
#include "Log.h"
#include "Stream.h"
#include "System/System.h"
#include "Engine/EmbeddedFile.h"
#include "Utilities.h"  // GetPlatform()

#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"

#include <cmath>
#include <algorithm>

PlayerInputSystem* PlayerInputSystem::sInstance = nullptr;

void PlayerInputSystem::Create()
{
    Destroy();
    sInstance = new PlayerInputSystem();
}

void PlayerInputSystem::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

PlayerInputSystem* PlayerInputSystem::Get()
{
    return sInstance;
}

PlayerInputSystem::PlayerInputSystem()
{
    // Seed mLastActiveDevice with a platform-appropriate default so prompt
    // widgets render the right family of icons before the player touches
    // anything. Without this, fixed-input platforms (Wii, GameCube, 3DS,
    // Android) boot showing a keyboard prompt and only switch after the
    // first input edge, which on console looks like the prompt is wrong on
    // every fresh scene load.
    switch (GetPlatform())
    {
    case Platform::Wii:
        mLastActiveDevice.kind = InputDeviceKind::Gamepad;
        mLastActiveDevice.gamepadType = GamepadType::Wiimote;
        mLastActiveDevice.gamepadIndex = 0;
        break;
    case Platform::GameCube:
        mLastActiveDevice.kind = InputDeviceKind::Gamepad;
        mLastActiveDevice.gamepadType = GamepadType::GameCube;
        mLastActiveDevice.gamepadIndex = 0;
        break;
    case Platform::N3DS:
    case Platform::Psp:
        // Handhelds without a clean GamepadType — Standard reads as a
        // generic gamepad, which is what these platforms' built-in buttons
        // map to in InputPromptMap rows.
        mLastActiveDevice.kind = InputDeviceKind::Gamepad;
        mLastActiveDevice.gamepadType = GamepadType::Standard;
        mLastActiveDevice.gamepadIndex = 0;
        break;
    case Platform::Android:
        // Touch-first device — Pointer source in PlayerInputSystem maps to
        // InputDeviceKind::Mouse for prompt purposes (the prompt map's
        // "Mouse/Left" row doubles as the tap icon, see the user's IMap
        // row 9: Android + Mouse/Left -> touch_tap).
        mLastActiveDevice.kind = InputDeviceKind::Mouse;
        mLastActiveDevice.gamepadIndex = -1;
        break;
    case Platform::Windows:
    case Platform::Linux:
    case Platform::Mac:
    default:
        // Desktop platforms — keep the default-constructed Keyboard kind,
        // which is the right starting assumption when there's no controller
        // edge yet.
        break;
    }
}

// --- Modifier checking ---

bool PlayerInputSystem::CheckModifiers(const InputActionBinding& binding) const
{
    bool ctrlDown = INP_IsKeyDown(POLYPHASE_KEY_CONTROL_L) || INP_IsKeyDown(POLYPHASE_KEY_CONTROL_R);
    bool shiftDown = INP_IsKeyDown(POLYPHASE_KEY_SHIFT_L) || INP_IsKeyDown(POLYPHASE_KEY_SHIFT_R);
    bool altDown = INP_IsKeyDown(POLYPHASE_KEY_ALT_L) || INP_IsKeyDown(POLYPHASE_KEY_ALT_R);

    if (binding.requireCtrl && !ctrlDown) return false;
    if (binding.requireShift && !shiftDown) return false;
    if (binding.requireAlt && !altDown) return false;

    return true;
}

// --- Binding polling ---

bool PlayerInputSystem::PollBindingDown(const InputActionBinding& binding, int32_t playerIndex) const
{
    if (!CheckModifiers(binding))
        return false;

    // Determine which gamepad index to use
    int32_t gpIdx = (playerIndex >= 0) ? playerIndex : binding.gamepadIndex;
    int32_t ptrIdx = (playerIndex >= 0) ? playerIndex : 0;

    switch (binding.sourceType)
    {
    case InputSourceType::Keyboard:
        return INP_IsKeyDown(binding.code);

    case InputSourceType::MouseButton:
        return INP_IsMouseButtonDown(binding.code);

    case InputSourceType::Pointer:
        return INP_IsPointerDown(ptrIdx);

    case InputSourceType::GamepadButton:
    {
        // Read raw physical state — bypass InputMap keyboard fallback
        InputState& input = GetEngineState()->mInput;
        if (gpIdx >= 0 && gpIdx < INPUT_MAX_GAMEPADS &&
            binding.code >= 0 && binding.code < GAMEPAD_BUTTON_COUNT)
            return input.mGamepads[gpIdx].mButtons[binding.code];
        return false;
    }

    case InputSourceType::GamepadAxis:
    {
        // Read raw physical state — bypass InputMap keyboard fallback
        float val = 0.0f;
        InputState& input = GetEngineState()->mInput;
        if (gpIdx >= 0 && gpIdx < INPUT_MAX_GAMEPADS &&
            binding.code >= 0 && binding.code < GAMEPAD_AXIS_COUNT)
            val = input.mGamepads[gpIdx].mAxes[binding.code];
        if (binding.axisDirection == AxisDirection::Positive)
            return val >= binding.axisThreshold;
        else if (binding.axisDirection == AxisDirection::Negative)
            return val <= -binding.axisThreshold;
        else
            return std::abs(val) >= binding.axisThreshold;
    }
    }

    return false;
}

float PlayerInputSystem::PollBindingValue(const InputActionBinding& binding, int32_t playerIndex) const
{
    if (!CheckModifiers(binding))
        return 0.0f;

    int32_t gpIdx = (playerIndex >= 0) ? playerIndex : binding.gamepadIndex;
    int32_t ptrIdx = (playerIndex >= 0) ? playerIndex : 0;

    switch (binding.sourceType)
    {
    case InputSourceType::Keyboard:
        return INP_IsKeyDown(binding.code) ? 1.0f : 0.0f;

    case InputSourceType::MouseButton:
        return INP_IsMouseButtonDown(binding.code) ? 1.0f : 0.0f;

    case InputSourceType::Pointer:
        return INP_IsPointerDown(ptrIdx) ? 1.0f : 0.0f;

    case InputSourceType::GamepadButton:
    {
        // Read raw physical state — bypass InputMap keyboard fallback
        InputState& input = GetEngineState()->mInput;
        if (gpIdx >= 0 && gpIdx < INPUT_MAX_GAMEPADS &&
            binding.code >= 0 && binding.code < GAMEPAD_BUTTON_COUNT)
            return input.mGamepads[gpIdx].mButtons[binding.code] ? 1.0f : 0.0f;
        return 0.0f;
    }

    case InputSourceType::GamepadAxis:
    {
        // Read raw physical state — bypass InputMap keyboard fallback
        float val = 0.0f;
        InputState& input = GetEngineState()->mInput;
        if (gpIdx >= 0 && gpIdx < INPUT_MAX_GAMEPADS &&
            binding.code >= 0 && binding.code < GAMEPAD_AXIS_COUNT)
            val = input.mGamepads[gpIdx].mAxes[binding.code];
        // Apply deadzone threshold — return 0 if below threshold
        if (binding.axisDirection == AxisDirection::Positive)
            return val >= binding.axisThreshold ? val : 0.0f;
        else if (binding.axisDirection == AxisDirection::Negative)
            return val <= -binding.axisThreshold ? -val : 0.0f;
        else
            return std::abs(val) >= binding.axisThreshold ? val : 0.0f;
    }
    }

    return 0.0f;
}

bool PlayerInputSystem::PollActionRawDown(const InputAction& action, int32_t playerIndex) const
{
    for (const auto& binding : action.bindings)
    {
        if (PollBindingDown(binding, playerIndex))
            return true;
    }
    return false;
}

float PlayerInputSystem::PollActionRawValue(const InputAction& action, int32_t playerIndex) const
{
    float maxVal = 0.0f;
    for (const auto& binding : action.bindings)
    {
        float val = PollBindingValue(binding, playerIndex);
        if (std::abs(val) > std::abs(maxVal))
            maxVal = val;
    }
    return maxVal;
}

// --- Trigger evaluation ---

void PlayerInputSystem::EvaluateTrigger(InputAction& action, float deltaTime)
{
    InputActionState& s = action.state;
    const InputActionTrigger& t = action.trigger;

    switch (t.mode)
    {
    case TriggerMode::SinglePress:
        s.isActive = (s.rawDown && !s.prevRawDown);
        break;

    case TriggerMode::KeepHeld:
        s.isActive = s.rawDown;
        break;

    case TriggerMode::Hold:
        if (s.rawDown)
        {
            s.holdTimer += deltaTime;
            if (s.holdTimer >= t.holdDuration && !s.holdTriggered)
            {
                s.isActive = true;
                s.holdTriggered = true;
            }
            else if (s.holdTriggered)
            {
                s.isActive = true;
            }
            else
            {
                s.isActive = false;
            }
        }
        else
        {
            s.holdTimer = 0.0f;
            s.holdTriggered = false;
            s.isActive = false;
        }
        break;

    case TriggerMode::MultiPress:
        // Detect new press
        if (s.rawDown && !s.prevRawDown)
        {
            if (s.multiPressTimer > 0.0f)
            {
                s.pressCount++;
            }
            else
            {
                s.pressCount = 1;
            }
            s.multiPressTimer = t.multiPressWindow;
        }

        // Count down window
        if (s.multiPressTimer > 0.0f)
        {
            s.multiPressTimer -= deltaTime;
            if (s.multiPressTimer <= 0.0f)
            {
                s.multiPressTimer = 0.0f;
                s.pressCount = 0;
            }
        }

        // Activate when target count reached
        if (s.pressCount >= t.multiPressCount)
        {
            s.isActive = true;
            s.pressCount = 0;
            s.multiPressTimer = 0.0f;
        }
        else
        {
            s.isActive = false;
        }
        break;
    }
}

// --- Update ---

void PlayerInputSystem::Update(float deltaTime)
{
    if (!mEnabled)
        return;

    mFrameCounter++;
    UpdateLastActiveDevice();

    if (mActionEvaluationEnabled)
    {
        for (auto& action : mActions)
        {
            InputActionState& s = action.state;

            // Sample raw input
            s.prevRawDown = s.rawDown;
            s.rawDown = PollActionRawDown(action);
            s.value = PollActionRawValue(action);

            // Clear per-frame flags
            s.wasJustActivated = false;
            s.wasJustDeactivated = false;

            bool wasActive = s.isActive;

            // Evaluate trigger
            EvaluateTrigger(action, deltaTime);

            // Zero value when not active to prevent drift
            if (!s.isActive)
                s.value = 0.0f;

            // Set transition flags
            s.wasJustActivated = (s.isActive && !wasActive);
            s.wasJustDeactivated = (!s.isActive && wasActive);
        }
    }
    else
    {
        // Capture-modal mute: don't fire game actions while editor is listening.
        // Keep per-action flags clean so resuming evaluation doesn't see ghost edges.
        for (auto& action : mActions)
        {
            InputActionState& s = action.state;
            s.prevRawDown = s.rawDown;
            s.wasJustActivated = false;
            s.wasJustDeactivated = false;
        }
    }

    if (InputPromptResolver* resolver = InputPromptResolver::Get())
    {
        resolver->Tick();
    }
}

void PlayerInputSystem::UpdateLastActiveDevice()
{
    InputState& input = GetEngineState()->mInput;
    InputDeviceDescriptor newDev = mLastActiveDevice;
    bool changed = false;

    // Keyboard edge — any key just-pressed promotes Keyboard.
    if (!input.mJustDownKeys.empty())
    {
        if (mLastActiveDevice.kind != InputDeviceKind::Keyboard)
        {
            newDev.kind = InputDeviceKind::Keyboard;
            newDev.gamepadIndex = -1;
            newDev.gamepadType = GamepadType::Standard;
            changed = true;
        }
    }

    // Mouse edge — any mouse button just-pressed promotes Mouse.
    for (int32_t b = 0; b < MOUSE_BUTTON_COUNT && !changed; ++b)
    {
        if (input.mMouseButtons[b] && !input.mPrevMouseButtons[b])
        {
            if (mLastActiveDevice.kind != InputDeviceKind::Mouse)
            {
                newDev.kind = InputDeviceKind::Mouse;
                newDev.gamepadIndex = -1;
                newDev.gamepadType = GamepadType::Standard;
                changed = true;
            }
        }
    }

    // Gamepad edge — any button just-pressed OR axis past promote threshold.
    constexpr float kDevicePromoteThreshold = 0.5f;
    for (int32_t gp = 0; gp < INPUT_MAX_GAMEPADS && !changed; ++gp)
    {
        const GamepadState& cur = input.mGamepads[gp];
        const GamepadState& prev = input.mPrevGamepads[gp];
        if (!cur.mConnected)
            continue;

        bool promote = false;
        for (int32_t i = 0; i < GAMEPAD_BUTTON_COUNT && !promote; ++i)
        {
            if (cur.mButtons[i] && !prev.mButtons[i])
                promote = true;
        }
        if (!promote)
        {
            for (int32_t a = 0; a < GAMEPAD_AXIS_COUNT && !promote; ++a)
            {
                if (std::abs(cur.mAxes[a]) >= kDevicePromoteThreshold &&
                    std::abs(prev.mAxes[a]) < kDevicePromoteThreshold)
                    promote = true;
            }
        }

        if (promote)
        {
            if (mLastActiveDevice.kind != InputDeviceKind::Gamepad ||
                mLastActiveDevice.gamepadIndex != gp ||
                mLastActiveDevice.gamepadType != cur.mType)
            {
                newDev.kind = InputDeviceKind::Gamepad;
                newDev.gamepadIndex = gp;
                newDev.gamepadType = cur.mType;
                changed = true;
            }
        }
    }

    if (changed)
    {
        mLastActiveDevice = newDev;
        mDeviceChangeFrame = mFrameCounter;
    }
}

const InputDeviceDescriptor& PlayerInputSystem::GetLastActiveDevice() const
{
    return mHasForcedDevice ? mForcedDevice : mLastActiveDevice;
}

uint32_t PlayerInputSystem::GetDeviceChangeFrame() const
{
    return mDeviceChangeFrame;
}

void PlayerInputSystem::SetForcedDevice(const InputDeviceDescriptor& device)
{
    // Bump the change-frame counter on any real change so InputPromptResolver
    // flushes its cache and every widget re-resolves against the new device.
    if (!mHasForcedDevice ||
        mForcedDevice.kind != device.kind ||
        mForcedDevice.gamepadType != device.gamepadType ||
        mForcedDevice.gamepadIndex != device.gamepadIndex)
    {
        mForcedDevice = device;
        mHasForcedDevice = true;
        mDeviceChangeFrame = ++mFrameCounter;
    }
}

void PlayerInputSystem::ClearForcedDevice()
{
    if (mHasForcedDevice)
    {
        mHasForcedDevice = false;
        mDeviceChangeFrame = ++mFrameCounter;
    }
}

bool PlayerInputSystem::HasForcedDevice() const
{
    return mHasForcedDevice;
}

void PlayerInputSystem::SetActionEvaluationEnabled(bool enabled)
{
    mActionEvaluationEnabled = enabled;
}

bool PlayerInputSystem::AreInputActionsActive() const
{
    return mEnabled && mActionEvaluationEnabled;
}

// --- Registration ---

std::string PlayerInputSystem::MakeKey(const std::string& category, const std::string& name) const
{
    return category + "/" + name;
}

void PlayerInputSystem::RebuildLookup()
{
    // Remove phantom entries (empty names or duplicate keys)
    for (auto it = mActions.begin(); it != mActions.end(); )
    {
        if (it->name.empty())
        {
            it = mActions.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Sort by category then name so same-category actions stay grouped
    std::sort(mActions.begin(), mActions.end(), [](const InputAction& a, const InputAction& b)
    {
        if (a.category != b.category) return a.category < b.category;
        return a.name < b.name;
    });

    // Remove duplicates (same category+name) that survive after sort
    for (size_t i = 1; i < mActions.size(); )
    {
        if (mActions[i].category == mActions[i - 1].category &&
            mActions[i].name == mActions[i - 1].name)
        {
            // Keep the first, merge bindings from the duplicate, then erase
            for (const auto& b : mActions[i].bindings)
                mActions[i - 1].bindings.push_back(b);
            mActions.erase(mActions.begin() + i);
        }
        else
        {
            ++i;
        }
    }

    mActionLookup.clear();
    for (size_t i = 0; i < mActions.size(); ++i)
    {
        mActionLookup[MakeKey(mActions[i].category, mActions[i].name)] = i;
    }
}

void PlayerInputSystem::RegisterAction(const std::string& category, const std::string& name, TriggerMode mode)
{
    if (name.empty())
        return;

    std::string key = MakeKey(category, name);
    if (mActionLookup.find(key) != mActionLookup.end())
        return; // already registered

    InputAction action;
    action.name = name;
    action.category = category;
    action.trigger.mode = mode;

    mActions.push_back(action);
    RebuildLookup();
}

void PlayerInputSystem::UnregisterAction(const std::string& category, const std::string& name)
{
    std::string key = MakeKey(category, name);
    auto it = mActionLookup.find(key);
    if (it == mActionLookup.end())
        return;

    size_t idx = it->second;
    mActions.erase(mActions.begin() + idx);
    RebuildLookup();
}

void PlayerInputSystem::AddBinding(const std::string& category, const std::string& name,
                                   const InputActionBinding& binding)
{
    InputAction* action = FindAction(category, name);
    if (action != nullptr)
    {
        action->bindings.push_back(binding);
    }
}

void PlayerInputSystem::ClearBindings(const std::string& category, const std::string& name)
{
    InputAction* action = FindAction(category, name);
    if (action != nullptr)
    {
        action->bindings.clear();
    }
}

void PlayerInputSystem::SetTrigger(const std::string& category, const std::string& name,
                                   const InputActionTrigger& trigger)
{
    InputAction* action = FindAction(category, name);
    if (action != nullptr)
    {
        action->trigger = trigger;
    }
}

// --- Queries ---

bool PlayerInputSystem::IsActionActive(const std::string& category, const std::string& name, int32_t playerIndex) const
{
    const InputAction* action = FindAction(category, name);
    if (action == nullptr) return false;

    // If playerIndex specified, do a live poll instead of using cached state
    if (playerIndex >= 0)
        return PollActionRawDown(*action, playerIndex);

    return action->state.isActive;
}

bool PlayerInputSystem::WasActionJustActivated(const std::string& category, const std::string& name, int32_t playerIndex) const
{
    const InputAction* action = FindAction(category, name);
    if (action == nullptr) return false;

    // For player-specific queries, check raw transition
    if (playerIndex >= 0)
    {
        // Can't track per-player transitions without per-player state, so just check raw down
        // This is a limitation — for full per-player transition tracking, actions need per-player state
        return PollActionRawDown(*action, playerIndex);
    }

    return action->state.wasJustActivated;
}

bool PlayerInputSystem::WasActionJustDeactivated(const std::string& category, const std::string& name, int32_t playerIndex) const
{
    const InputAction* action = FindAction(category, name);
    if (action == nullptr) return false;

    if (playerIndex >= 0)
        return !PollActionRawDown(*action, playerIndex);

    return action->state.wasJustDeactivated;
}

float PlayerInputSystem::GetActionValue(const std::string& category, const std::string& name, int32_t playerIndex) const
{
    const InputAction* action = FindAction(category, name);
    if (action == nullptr) return 0.0f;

    if (playerIndex >= 0)
        return PollActionRawValue(*action, playerIndex);

    return action->state.value;
}

int32_t PlayerInputSystem::GetPlayersConnected() const
{
    int32_t count = 0;
    for (int32_t i = 0; i < INPUT_MAX_GAMEPADS; ++i)
    {
        if (INP_IsGamepadConnected(i))
            ++count;
    }
    return count;
}

// --- Bulk access ---

const std::vector<InputAction>& PlayerInputSystem::GetActions() const
{
    return mActions;
}

InputAction* PlayerInputSystem::FindAction(const std::string& category, const std::string& name)
{
    std::string key = MakeKey(category, name);
    auto it = mActionLookup.find(key);
    if (it != mActionLookup.end() && it->second < mActions.size())
        return &mActions[it->second];
    return nullptr;
}

const InputAction* PlayerInputSystem::FindAction(const std::string& category, const std::string& name) const
{
    std::string key = MakeKey(category, name);
    auto it = mActionLookup.find(key);
    if (it != mActionLookup.end() && it->second < mActions.size())
        return &mActions[it->second];
    return nullptr;
}

// --- Enable/Disable ---

void PlayerInputSystem::SetEnabled(bool enabled) { mEnabled = enabled; }
bool PlayerInputSystem::IsEnabled() const { return mEnabled; }

// --- Serialization ---

static const char* TriggerModeToString(TriggerMode mode)
{
    switch (mode)
    {
    case TriggerMode::SinglePress: return "SinglePress";
    case TriggerMode::Hold: return "Hold";
    case TriggerMode::KeepHeld: return "KeepHeld";
    case TriggerMode::MultiPress: return "MultiPress";
    default: return "SinglePress";
    }
}

static TriggerMode StringToTriggerMode(const char* str)
{
    if (strcmp(str, "Hold") == 0) return TriggerMode::Hold;
    if (strcmp(str, "KeepHeld") == 0) return TriggerMode::KeepHeld;
    if (strcmp(str, "MultiPress") == 0) return TriggerMode::MultiPress;
    return TriggerMode::SinglePress;
}

static const char* SourceTypeToString(InputSourceType type)
{
    switch (type)
    {
    case InputSourceType::Keyboard: return "Keyboard";
    case InputSourceType::MouseButton: return "MouseButton";
    case InputSourceType::GamepadButton: return "GamepadButton";
    case InputSourceType::GamepadAxis: return "GamepadAxis";
    case InputSourceType::Pointer: return "Pointer";
    default: return "Keyboard";
    }
}

static InputSourceType StringToSourceType(const char* str)
{
    if (strcmp(str, "MouseButton") == 0) return InputSourceType::MouseButton;
    if (strcmp(str, "GamepadButton") == 0) return InputSourceType::GamepadButton;
    if (strcmp(str, "GamepadAxis") == 0) return InputSourceType::GamepadAxis;
    if (strcmp(str, "Pointer") == 0) return InputSourceType::Pointer;
    return InputSourceType::Keyboard;
}

static const char* AxisDirToString(AxisDirection dir)
{
    switch (dir)
    {
    case AxisDirection::Positive: return "Positive";
    case AxisDirection::Negative: return "Negative";
    case AxisDirection::Full: return "Full";
    default: return "Positive";
    }
}

static AxisDirection StringToAxisDir(const char* str)
{
    if (strcmp(str, "Negative") == 0) return AxisDirection::Negative;
    if (strcmp(str, "Full") == 0) return AxisDirection::Full;
    return AxisDirection::Positive;
}

static const char* GetBindingCodeName(InputSourceType type, int32_t code)
{
    switch (type)
    {
    case InputSourceType::Keyboard:
        return InputMap::GetKeyCodeName(code);
    case InputSourceType::GamepadButton:
        if (code >= 0 && code < GAMEPAD_BUTTON_COUNT)
            return InputMap::GetGamepadButtonName((GamepadButtonCode)code);
        return "?";
    case InputSourceType::GamepadAxis:
        if (code >= 0 && code < GAMEPAD_AXIS_COUNT)
            return InputMap::GetGamepadAxisName((GamepadAxisCode)code);
        return "?";
    default:
        return "";
    }
}

static void DumpLoadedActions(const std::vector<InputAction>& actions)
{

    for (size_t i = 0; i < actions.size(); ++i)
    {
        const InputAction& action = actions[i];


        if (action.bindings.empty())
        {
        }

        for (size_t b = 0; b < action.bindings.size(); ++b)
        {
            const InputActionBinding& binding = action.bindings[b];
            
        }
    }

}

void PlayerInputSystem::SetActions(const std::vector<InputAction>& actions)
{
    mActions = actions;
    RebuildLookup();
}

void PlayerInputSystem::SaveProjectActions()
{
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    if (projectDir.empty())
    {
        LogWarning("PlayerInput: Cannot save — no project directory set");
        return;
    }

    std::string path = projectDir + "Assets/InputActions.oct";

    InputActionsAsset tempAsset;
    tempAsset.SetName("InputActions");
    tempAsset.mActions = mActions;
    tempAsset.SaveFile(path.c_str(), GetPlatform());

    // Update the cached asset so PIE picks up changes without editor restart
    Asset* cached = LoadAsset("InputActions");
    if (cached != nullptr && cached->GetType() == InputActionsAsset::GetStaticType())
    {
        static_cast<InputActionsAsset*>(cached)->mActions = mActions;
    }

}

void PlayerInputSystem::LoadProjectActions()
{
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    if (projectDir.empty())
    {
        LogWarning("PlayerInput: Project directory is empty, cannot load actions");
        return;
    }


    std::string octPath = projectDir + "Assets/InputActions.oct";

    // Clear existing actions before loading to prevent duplicates on reload
    mActions.clear();
    mActionLookup.clear();

    // Path 1 — AssetManager. Works for editor (asset on disk), for packaged
    // builds with embedded assets (cooked .oct is in gEmbeddedAssets), and
    // for packaged builds with loose .oct on disk. Prefer this over the
    // direct file path because the existence check below only looks at disk
    // + the *raw* embedded asset table (gEmbeddedRawAssets), which misses
    // the regular embedded asset table where InputActions.oct actually ends
    // up after cook.
    if (Asset* cached = LoadAsset("InputActions"))
    {
        if (cached->GetType() == InputActionsAsset::GetStaticType())
        {
            InputActionsAsset* ia = static_cast<InputActionsAsset*>(cached);
            mActions = ia->mActions;
            RebuildLookup();
            LogDebug("PlayerInput: Loaded %d actions via AssetManager.",
                     (int)mActions.size());
            return;
        }
        LogWarning("PlayerInput: LoadAsset('InputActions') returned an asset "
                   "of type %u, expected InputActionsAsset (type %u). Asset "
                   "stub may be registered under the wrong type id.",
                   (unsigned)cached->GetType(),
                   (unsigned)InputActionsAsset::GetStaticType());
    }
    else
    {
        LogDebug("PlayerInput: AssetManager has no 'InputActions' stub; "
                 "falling back to direct file read paths.");
    }

    // Path 2 — direct file read. Kept for backward compatibility with
    // earlier packaging flows that put InputActions.oct in the raw embedded
    // asset table or on disk somewhere AssetManager doesn't index. Gate the
    // read on existence (disk OR raw embedded) so a missing file doesn't
    // emit a Stream::ReadFile error.
    uint32_t embeddedSize = 0;
    bool octExists =
        SYS_DoesFileExist(octPath.c_str(), false) ||
        SYS_DoesFileExist(octPath.c_str(), true)  ||
        SYS_LookupEmbeddedRawAsset(octPath.c_str(), embeddedSize) != nullptr;

    if (octExists)
    {
        // Try loading the .oct asset (non-embedded first, then embedded for romfs/3DS)
        Stream stream;
        if (stream.ReadFile(octPath.c_str(), false) || stream.ReadFile(octPath.c_str(), true))
        {
            InputActionsAsset asset;
            asset.LoadStream(stream, GetPlatform());
            mActions = asset.mActions;
            RebuildLookup();
            DumpLoadedActions(mActions);
            return;
        }

        LogWarning("PlayerInput: Failed to read %s", octPath.c_str());
    }

    // Path 3 — legacy InputActions.json fallback.
    std::string jsonPath = projectDir + "InputActions.json";
    embeddedSize = 0;
    bool jsonExists =
        SYS_DoesFileExist(jsonPath.c_str(), false) ||
        SYS_DoesFileExist(jsonPath.c_str(), true)  ||
        SYS_LookupEmbeddedRawAsset(jsonPath.c_str(), embeddedSize) != nullptr;

    if (jsonExists && LoadFromJsonFile(jsonPath))
    {
        DumpLoadedActions(mActions);
        SaveProjectActions();
        return;
    }

    LogWarning("PlayerInput: InputActions.oct not found via AssetManager, "
               "disk, or embedded raw asset table. No actions will be available; "
               "every InputActionPrompt widget will show its fallback text.");
}

bool PlayerInputSystem::LoadFromJsonFile(const std::string& filePath)
{
    uint32_t embeddedSize = 0;
    bool exists =
        SYS_DoesFileExist(filePath.c_str(), false) ||
        SYS_DoesFileExist(filePath.c_str(), true)  ||
        SYS_LookupEmbeddedRawAsset(filePath.c_str(), embeddedSize) != nullptr;
    if (!exists)
        return false;

    Stream stream;
    if (!stream.ReadFile(filePath.c_str(), false))
    {
        if (!stream.ReadFile(filePath.c_str(), true))
            return false;
    }

    std::string jsonStr(stream.GetData(), stream.GetSize());
    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        LogError("Failed to parse PlayerInput JSON: %s", filePath.c_str());
        return false;
    }

    mActions.clear();
    mActionLookup.clear();

    if (!doc.HasMember("actions") || !doc["actions"].IsArray())
        return true;

    const rapidjson::Value& actionsArr = doc["actions"];
    for (rapidjson::SizeType i = 0; i < actionsArr.Size(); ++i)
    {
        const rapidjson::Value& aObj = actionsArr[i];
        if (!aObj.IsObject()) continue;

        std::string category = aObj.HasMember("category") && aObj["category"].IsString()
            ? aObj["category"].GetString() : "";
        std::string name = aObj.HasMember("name") && aObj["name"].IsString()
            ? aObj["name"].GetString() : "";

        if (name.empty()) continue;

        RegisterAction(category, name);
        InputAction* action = FindAction(category, name);
        if (action == nullptr) continue;

        // Trigger
        if (aObj.HasMember("trigger") && aObj["trigger"].IsObject())
        {
            const rapidjson::Value& tObj = aObj["trigger"];
            if (tObj.HasMember("mode") && tObj["mode"].IsString())
                action->trigger.mode = StringToTriggerMode(tObj["mode"].GetString());
            if (tObj.HasMember("holdDuration") && tObj["holdDuration"].IsNumber())
                action->trigger.holdDuration = tObj["holdDuration"].GetFloat();
            if (tObj.HasMember("multiPressCount") && tObj["multiPressCount"].IsInt())
                action->trigger.multiPressCount = tObj["multiPressCount"].GetInt();
            if (tObj.HasMember("multiPressWindow") && tObj["multiPressWindow"].IsNumber())
                action->trigger.multiPressWindow = tObj["multiPressWindow"].GetFloat();
        }

        // Bindings
        if (aObj.HasMember("bindings") && aObj["bindings"].IsArray())
        {
            const rapidjson::Value& bArr = aObj["bindings"];
            for (rapidjson::SizeType b = 0; b < bArr.Size(); ++b)
            {
                const rapidjson::Value& bObj = bArr[b];
                if (!bObj.IsObject()) continue;

                InputActionBinding binding;
                if (bObj.HasMember("sourceType") && bObj["sourceType"].IsString())
                    binding.sourceType = StringToSourceType(bObj["sourceType"].GetString());
                if (bObj.HasMember("code") && bObj["code"].IsInt())
                    binding.code = bObj["code"].GetInt();
                if (bObj.HasMember("axisDirection") && bObj["axisDirection"].IsString())
                    binding.axisDirection = StringToAxisDir(bObj["axisDirection"].GetString());
                if (bObj.HasMember("axisThreshold") && bObj["axisThreshold"].IsNumber())
                    binding.axisThreshold = bObj["axisThreshold"].GetFloat();
                if (bObj.HasMember("gamepadIndex") && bObj["gamepadIndex"].IsInt())
                    binding.gamepadIndex = bObj["gamepadIndex"].GetInt();
                if (bObj.HasMember("ctrl") && bObj["ctrl"].IsBool())
                    binding.requireCtrl = bObj["ctrl"].GetBool();
                if (bObj.HasMember("shift") && bObj["shift"].IsBool())
                    binding.requireShift = bObj["shift"].GetBool();
                if (bObj.HasMember("alt") && bObj["alt"].IsBool())
                    binding.requireAlt = bObj["alt"].GetBool();

                action->bindings.push_back(binding);
            }
        }
    }

    RebuildLookup();
    //LogDebug("Loaded %d input actions from JSON %s", (int)mActions.size(), filePath.c_str());
    return true;
}
