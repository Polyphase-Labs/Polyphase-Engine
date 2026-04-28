# Debugging

Tools and systems for runtime debugging and performance monitoring.

## Debug Resources Widget

A built-in widget that displays live resource usage with progress bars. Shows RAM, VRAM, platform-specific memory pools, and FPS.

### Creating from Lua

```lua
local panel = Renderer.AddDebugResourcesWidget(parentWidget, showMultipleRAM, showFPS, showVRAM)
```

All boolean arguments are optional and default to `true`.

### Example: Toggle on Button Press

```lua
function MyScript:Start()
    self.debugPanel = nil
end

function MyScript:Tick(dt)
    if PlayerInput.WasJustActivated("Debug", "ToggleResources") then
        if self.debugPanel == nil then
            self.debugPanel = Renderer.AddDebugResourcesWidget(self, true, true, true)
            self.debugPanel:SetAnchorMode(AnchorMode.TopRight)
            self.debugPanel:SetPosition(-310, 10)
        else
            self.debugPanel:SetVisible(not self.debugPanel:IsVisible())
        end
    end
end
```

### Widget Layout

The widget is a Canvas containing rows of Text labels, ProgressBars, and value Text widgets:

```
 RAM   [████████████░░░░░░░]  234.5 MB
 VRAM  [██████░░░░░░░░░░░░░]   56.7 MB
 RAM1  [█████████░░░░░░░░░░]   89.0 MB
 RAM2  [██░░░░░░░░░░░░░░░░░]   12.3 MB
 FPS   [████████████████░░░]   52.3 FPS
```

- **RAM** — Process memory usage (always shown)
- **VRAM** — GPU memory usage (toggle with `showVRAM`)
- **RAM1/RAM2** — Platform-specific memory pools (toggle with `showMultipleRAM`)
- **FPS** — Frames per second averaged over 0.5s (toggle with `showFPS`)

Memory rows update every 0.5 seconds. FPS is sampled every frame and averaged.

### Progress Bar Maximums

The progress bars scale against actual hardware totals:

| Platform   | RAM             | VRAM               | RAM1             | RAM2              |
|------------|-----------------|---------------------|------------------|-------------------|
| Windows    | System physical | Vulkan allocated    | —                | —                 |
| Linux      | System physical | Vulkan allocated    | —                | —                 |
| 3DS        | 128 MB (FCRAM)  | 6 MB (dedicated)    | 128 MB (FCRAM)   | 6 MB (VRAM)       |
| Wii        | 88 MB (MEM1+2)  | 3 MB (embedded)     | 24 MB (MEM1)     | 64 MB (MEM2)      |
| GameCube   | 40 MB (main+aux)| 3 MB (embedded)     | 24 MB (1T-SRAM)  | 16 MB (DRAM)      |
| Android    | System physical | Vulkan allocated    | —                | —                 |

FPS bar maximum is 60.

## Resource Query Functions

The underlying system functions are also exposed directly on the Renderer Lua table for use without the widget:

```lua
local ram  = Renderer.GetRAMUsage()   -- MB
local vram = Renderer.GetVRAMUsage()  -- MB
local ram1 = Renderer.GetRAM1Usage()  -- MB
local ram2 = Renderer.GetRAM2Usage()  -- MB
local cpu  = Renderer.GetCPUUsage()   -- % (Windows/Linux only)
```

## Stats Overlay

The built-in stats overlay can be toggled via Lua:

```lua
Renderer.EnableStatsOverlay(true, "Performance")  -- CPU/GPU frame stats
Renderer.EnableStatsOverlay(true, "Memory")        -- Memory pool stats
Renderer.EnableStatsOverlay(true, "Network")       -- Upload/download rates
```

## PlayerInput Debugger

When running in the editor, the **Developer > Player Input Debugger** window shows a real-time table of all registered input actions with color-coded active/inactive states. See [PlayerInput](PlayerInput.md) for details.

## Debug Action Dump

When a project loads, the PlayerInput system logs all loaded actions and their bindings to the console at `LogDebug` level:

```
PlayerInput: ========== ACTION DUMP (2 actions) ==========
PlayerInput: [0] Category="Movement" Name="Jump" Trigger=SinglePress (hold=1.00s, multiCount=2, multiWindow=0.30s)
PlayerInput:   Binding[0]: Keyboard code=32(Space) dir=Positive thresh=0.50 pad=0
PlayerInput:   Binding[1]: GamepadButton code=0(A) dir=Positive thresh=0.50 pad=0
PlayerInput: ========== END ACTION DUMP ==========
```

This is useful for verifying that saved actions loaded correctly and no stale bindings exist.
