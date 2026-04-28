# PlayerInput System

## Overview

Action-based input system that maps named actions to physical inputs. Sits between the raw `INP_*` layer and game scripts. Actions have categories, multiple bindings, trigger modes (tap, hold, multi-tap), and modifier key support.

## Key Files

| File | Purpose |
|------|---------|
| `Engine/Source/Input/PlayerInputSystem.h/.cpp` | Core singleton: registration, update loop, trigger evaluation, serialization |
| `Engine/Source/LuaBindings/PlayerInput_Lua.h/.cpp` | Lua bindings: `PlayerInput` global table |
| `Engine/Source/Editor/PlayerInputEditor.h/.cpp` | Visual editor window (EDITOR only) |
| `Engine/Source/Editor/PlayerInputDebugger.h/.cpp` | Real-time debug panel (EDITOR only) |

## Architecture

```
Raw Input (INP_IsKeyDown, INP_IsGamepadButtonDown, INP_GetGamepadAxisValue)
    |
PlayerInputSystem::Update(deltaTime)  -- polls raw input, evaluates triggers
    |
InputAction state (isActive, wasJustActivated, wasJustDeactivated, value)
    |
Lua: PlayerInput.IsActive("Category", "Action")
C++: PlayerInputSystem::Get()->IsActionActive("Category", "Action")
```

## Data Structures

```cpp
// Engine/Source/Input/PlayerInputSystem.h

enum class InputSourceType : uint8_t { Keyboard, MouseButton, GamepadButton, GamepadAxis };
enum class AxisDirection : uint8_t { Positive, Negative, Full };
enum class TriggerMode : uint8_t { SinglePress, Hold, KeepHeld, MultiPress };

struct InputActionBinding {
    InputSourceType sourceType;
    int32_t code;               // keyCode, mouseButton, gamepadButton, or gamepadAxis
    AxisDirection axisDirection;
    float axisThreshold;
    int32_t gamepadIndex;
    bool requireCtrl, requireShift, requireAlt;  // modifier keys
};

struct InputActionTrigger {
    TriggerMode mode;
    float holdDuration;         // Hold mode: seconds before activation
    int32_t multiPressCount;    // MultiPress mode: required presses
    float multiPressWindow;     // MultiPress mode: time window in seconds
};

struct InputAction {
    std::string name, category;
    std::vector<InputActionBinding> bindings;
    InputActionTrigger trigger;
    InputActionState state;     // runtime: isActive, wasJustActivated, value, holdTimer, pressCount, etc.
};
```

## Trigger Mode Logic

- **SinglePress**: `isActive = rawDown && !prevRawDown` (one frame)
- **KeepHeld**: `isActive = rawDown` (continuous)
- **Hold**: Increments `holdTimer` while held; activates when `holdTimer >= holdDuration`; resets on release
- **MultiPress**: Counts presses within `multiPressWindow`; activates when `pressCount >= multiPressCount`; resets timer and count after activation or timeout

## Update Flow (Engine.cpp ~line 668)

Called after `INP_Update()` and `sClock.Update()`:
```cpp
PlayerInputSystem::Get()->Update(sClock.DeltaTime());
```

Each frame per action:
1. Save `prevRawDown`, poll new `rawDown` (OR across all bindings)
2. Poll `value` (max across all bindings)
3. Evaluate trigger mode -> set `isActive`
4. Set `wasJustActivated` / `wasJustDeactivated` transition flags

## Binding Polling

`PollBindingDown()` checks modifier keys first (Ctrl/Shift/Alt must match), then:
- **Keyboard**: `INP_IsKeyDown(code)`
- **MouseButton**: `INP_IsMouseButtonDown(code)`
- **GamepadButton**: `INP_IsGamepadButtonDown(code, gamepadIndex)`
- **GamepadAxis**: `INP_GetGamepadAxisValue(code, gamepadIndex)` compared against `axisThreshold` with `axisDirection`

## Serialization

Actions save to `ProjectDir/InputActions.json` via rapidjson:
```json
{
    "version": 1,
    "actions": [{
        "category": "Platformer", "name": "Jump",
        "trigger": { "mode": "SinglePress", "holdDuration": 1.0, ... },
        "bindings": [{ "sourceType": "Keyboard", "code": 32, "ctrl": false, "shift": false, "alt": false, ... }]
    }]
}
```

Loaded on project open via `PlayerInputSystem::LoadProjectActions()`.

## Lua API

```lua
-- Global table: PlayerInput
PlayerInput.IsActive(category, name) -> bool
PlayerInput.WasJustActivated(category, name) -> bool
PlayerInput.WasJustDeactivated(category, name) -> bool
PlayerInput.GetValue(category, name) -> float
PlayerInput.RegisterAction(category, name, { trigger=, bindings= })
PlayerInput.UnregisterAction(category, name)
PlayerInput.SetEnabled(bool) / PlayerInput.IsEnabled() -> bool
```

## Lifecycle

1. `PlayerInputSystem::Create()` in `EditorMain.cpp` (after `InputMap::Create()`)
2. `PlayerInput_Lua::Bind()` in `LuaBindings.cpp` (after `Input_Lua::Bind()`)
3. `LoadProjectActions()` after project opens
4. `Update(deltaTime)` each frame in `Engine.cpp`
5. `PlayerInputSystem::Destroy()` on shutdown (before `InputMap::Destroy()`)

## Editor Integration

- **Developer > Player Input Editor**: category/action tree, trigger config, binding capture
- **Developer > Player Input Debugger**: real-time state table with color-coded rows
- Both added in `EditorImgui.cpp` (menu items + Draw calls)

## Action Lookup Performance

Uses `std::unordered_map<string, size_t>` keyed by `"Category/Name"` for O(1) lookup. Vector stores actions for ordered iteration in the editor.

## Adding New Trigger Modes

1. Add value to `TriggerMode` enum in `PlayerInputSystem.h`
2. Add evaluation case in `PlayerInputSystem::EvaluateTrigger()`
3. Add serialization strings in `TriggerModeToString/StringToTriggerMode`
4. Add to `TriggerModeNames[]` array in `PlayerInputEditor.cpp`
5. Add to Lua string parsing in `PlayerInput_Lua::RegisterAction()`
