# PlayerInput System

The PlayerInput system provides a named, action-based input layer on top of the raw input API. Instead of checking specific keys or buttons directly, you define actions with names and categories, bind them to physical inputs, and query them by name. This decouples game logic from hardware and makes input remappable.

## Use Cases

- Define "Jump" once, bind to Space + Gamepad A, query from any script
- Organize inputs by context ("Platformer", "UI", "Vehicle")
- Support hold-to-interact, double-tap-to-dash without manual timer code
- Let designers tweak bindings in the editor without touching scripts
- Debug input issues with the real-time debugger panel

## Quick Start

### From Lua

```lua
-- Register actions (typically in a startup script)
PlayerInput.RegisterAction("Platformer", "Jump", {
    trigger = "SinglePress",
    bindings = {
        { type = "Key", key = Key.Space },
        { type = "Gamepad", button = Gamepad.A }
    }
})

PlayerInput.RegisterAction("Platformer", "MoveX", {
    trigger = "KeepHeld",
    bindings = {
        { type = "GamepadAxis", axis = Gamepad.AxisLX, axisDirection = "Full" }
    }
})

-- Query in Tick
function MyScript:Tick()
    if PlayerInput.WasJustActivated("Platformer", "Jump") then
        self:Jump()
    end

    local moveX = PlayerInput.GetValue("Platformer", "MoveX")
    self:Move(moveX)
end
```

### From the Editor

1. Open **Developer > Player Input Editor**
2. Enter a category and action name, click **Add**
3. Select the action, choose a trigger mode, click **+ Add Binding**
4. Click **Remap** and press a key/button to bind it
5. Click **Save to Project** to persist to `InputActions.json`

## Trigger Modes

| Mode | Behavior |
|------|----------|
| **SinglePress** | Active for one frame when the input is first pressed |
| **Hold** | Active after holding for `holdDuration` seconds. Stays active while held after that. |
| **KeepHeld** | Active every frame while the input is held down |
| **MultiPress** | Active when pressed `multiPressCount` times within `multiPressWindow` seconds (e.g., double-tap) |

## Lua API Reference

### Query Functions

```lua
-- Is the action currently active?
PlayerInput.IsActive(category, name) -> boolean

-- Did the action just become active this frame?
PlayerInput.WasJustActivated(category, name) -> boolean

-- Did the action just become inactive this frame?
PlayerInput.WasJustDeactivated(category, name) -> boolean

-- Get the analog value (0-1 for buttons, -1 to 1 for axes)
PlayerInput.GetValue(category, name) -> number
```

### Registration Functions

```lua
PlayerInput.RegisterAction(category, name, {
    trigger = "SinglePress",         -- "SinglePress", "Hold", "KeepHeld", "MultiPress"
    holdDuration = 1.0,              -- seconds (Hold mode only)
    multiPressCount = 2,             -- press count (MultiPress mode only)
    multiPressWindow = 0.3,          -- seconds (MultiPress mode only)
    bindings = {
        {
            type = "Key",            -- "Key", "Mouse", "Gamepad", "GamepadAxis"
            key = Key.Space,         -- use 'key' for Key, 'button' for Mouse/Gamepad, 'axis' for GamepadAxis
            ctrl = false,            -- require Ctrl modifier
            shift = false,           -- require Shift modifier
            alt = false,             -- require Alt modifier
            axisDirection = "Positive", -- "Positive", "Negative", "Full" (GamepadAxis only)
            axisThreshold = 0.5,     -- threshold for axis-as-button (GamepadAxis only)
            gamepadIndex = 0,        -- which gamepad (0 = first)
        }
    }
})

PlayerInput.UnregisterAction(category, name)
```

### Enable/Disable

```lua
PlayerInput.SetEnabled(false)  -- disable all action processing
PlayerInput.IsEnabled() -> boolean
```

## Binding Types

| Type | Field | Description |
|------|-------|-------------|
| `"Key"` | `key = Key.Space` | Keyboard key from the `Key` table |
| `"Mouse"` | `button = Mouse.Left` | Mouse button from the `Mouse` table |
| `"Gamepad"` | `button = Gamepad.A` | Gamepad button from the `Gamepad` table |
| `"GamepadAxis"` | `axis = Gamepad.AxisLX` | Gamepad axis, with optional `axisDirection` and `axisThreshold` |

## Modifier Keys

Each binding can require modifier keys (Ctrl, Shift, Alt). This lets you have both `Space` for "Jump" and `Shift+Space` for "SuperJump":

```lua
PlayerInput.RegisterAction("Platformer", "SuperJump", {
    trigger = "SinglePress",
    bindings = {
        { type = "Key", key = Key.Space, shift = true }
    }
})
```

## Editor Tools

### Player Input Editor (Developer > Player Input Editor)

Visual tool for defining and configuring actions:

- **Left panel**: Tree view of categories and actions
- **Right panel**: Trigger mode, parameters, and binding list
- **Capture mode**: Click "Remap" then press any input to bind it (records modifier state)
- **Save/Load**: Persists to `ProjectDir/InputActions.json`

### Player Input Debugger (Developer > Player Input Debugger)

Real-time monitoring panel for debugging input during PIE:

- Shows all registered actions with current state
- Color-coded: green = active, yellow = just activated, red = just deactivated
- Filter by name or active-only
- Shows hold timer, press count, and analog value per action

## Persistence

Action definitions are saved to `InputActions.json` in the project directory. This file is loaded automatically when a project is opened. Actions can also be registered at runtime via Lua without saving.

## Examples

### Hold to Interact

```lua
PlayerInput.RegisterAction("Game", "Interact", {
    trigger = "Hold",
    holdDuration = 0.5,
    bindings = {
        { type = "Key", key = Key.E },
        { type = "Gamepad", button = Gamepad.Y }
    }
})

function InteractScript:Tick()
    if PlayerInput.WasJustActivated("Game", "Interact") then
        self:DoInteraction()
    end
end
```

### Double-Tap to Dash

```lua
PlayerInput.RegisterAction("Platformer", "Dash", {
    trigger = "MultiPress",
    multiPressCount = 2,
    multiPressWindow = 0.3,
    bindings = {
        { type = "Key", key = Key.ShiftL },
        { type = "Gamepad", button = Gamepad.B }
    }
})
```

### Analog Movement

```lua
PlayerInput.RegisterAction("Platformer", "MoveX", {
    trigger = "KeepHeld",
    bindings = {
        { type = "GamepadAxis", axis = Gamepad.AxisLX, axisDirection = "Full" },
        { type = "Key", key = Key.D },  -- returns 1.0 when held
    }
})

PlayerInput.RegisterAction("Platformer", "MoveXNeg", {
    trigger = "KeepHeld",
    bindings = {
        { type = "Key", key = Key.A },
    }
})

function Controller:Tick()
    local moveX = PlayerInput.GetValue("Platformer", "MoveX")
                - PlayerInput.GetValue("Platformer", "MoveXNeg")
    -- moveX is -1 to 1
end
```
