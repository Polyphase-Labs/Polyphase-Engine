# LoadingMenu

`LoadingMenu` is the global controller for "loading screen" scene transitions. It overlays a designer-authored Scene asset on top of the current world, swaps in the target gameplay scene under it, and tears the menu down when the new scene reports it's ready.

The loading-menu node is parented under the active world's root and marked persistent (`Node:SetPersistent(true)`), so it survives the engine's root-node swap and stays visible across the boundary between the old and new scene.

Progress reporting and "I'm done loading" notifications travel through the existing [`SignalBus`](SignalBus.md) using the reserved `Loading.*` channel namespace — no separate event API.

> ℹ️ `LoadingMenu` only operates on **world index 0** (the main game world). On 3DS, world 1 is the bottom screen and is not routed through the loading menu. Calls with `worldIndex` ≠ 0 are forwarded as direct `world:LoadScene` calls.

---
### SetMenuScene
Choose which Scene asset acts as the loading screen for the next `Open` call. Pass `""` (empty string) to fall back to the AppSettings default.

Sig: `LoadingMenu.SetMenuScene(sceneName)`
- Arg: `string sceneName` Scene asset name (e.g. `"SC_LoadingSciFi"`) or `""` to use `AppSettings → Default Loading Scene`.

Example:
```lua
-- Pick a sci-fi loading screen for the next transition.
LoadingMenu.SetMenuScene("SC_LoadingSciFi")
LoadingMenu.Open("SC_Level2")
```

---
### GetMenuScene
Return the currently configured loading-scene override.

Sig: `name = LoadingMenu.GetMenuScene()`
- Ret: `string name` The override name set via `SetMenuScene`, or `""` if no override is active (the AppSettings default will be used).

---
### Open
Start a loading-screen transition to `targetSceneName`. The engine:

1. Resolves the loading scene (override if set, else AppSettings default).
2. Instantiates it, marks it persistent, parents it under the current world root.
3. Subscribes the menu node to `"Loading.Finished"` on the SignalBus.
4. Calls `world:LoadScene(targetSceneName, false)` internally — the gameplay root is queued for the swap-in-place that the World does at the top of its next update.

If no loading scene is resolvable, the engine falls back to a direct non-instant `world:LoadScene(targetSceneName, false)` and returns `false`.

Sig: `started = LoadingMenu.Open(targetSceneName [, worldIndex])`
- Arg: `string targetSceneName` Scene asset to load underneath the menu.
- Arg: `integer worldIndex` Optional. Defaults to `0`. Only world 0 actually shows the loading menu.
- Ret: `boolean started` `true` if the loading menu was opened, `false` if the call fell through to a direct load (no menu scene, headless, mid-transition, or non-zero world).

> ⚠️ `Open` is rejected if `LoadingMenu` is already mid-transition. A warning is logged and the call returns `false`. To swap targets mid-load, call `Close()` first, wait for the menu to fully tear down, then call `Open` again.

Example:
```lua
function MainMenu:OnPlayClicked()
    LoadingMenu.SetMenuScene("")  -- use the project default
    LoadingMenu.Open("SC_World1")
end
```

---
### Close
Request the loading menu to tear down. Typically called by the **loading scene** itself when its outro animation has finished.

`Close` is **gated by `Loading Min Display (s)`** from AppSettings: if the gate hasn't opened yet, the close request is parked and applied as soon as it does. This means a loading scene can safely call `Close` the instant it receives `Loading.Finished` and the engine will hold the menu visible for the configured minimum.

Sig: `LoadingMenu.Close()`

Example:
```lua
function LoadingScreen:Start()
    SignalBus.Subscribe("Loading.Finished", self, function(_)
        self:PlayOutroAnimation(function()
            LoadingMenu.Close()
        end)
    end)
end
```

---
### IsActive
Check whether a loading-screen transition is in flight.

Sig: `active = LoadingMenu.IsActive()`
- Ret: `boolean active` `true` while state is `Loading` or `Closing`.

---
### GetState
Return the current internal state as a string.

Sig: `state = LoadingMenu.GetState()`
- Ret: `string state` One of `"Idle"`, `"Loading"`, `"Closing"`.

---
### GetTargetScene
Return the name of the scene being loaded under the menu, or `""` when idle.

Sig: `name = LoadingMenu.GetTargetScene()`
- Ret: `string name` Target scene asset name.

---
## SignalBus Channels (Loading.*)

`Loading.*` is a **reserved engine namespace** on [`SignalBus`](SignalBus.md). The loading scene reads from and writes to these channels — the target gameplay scene is responsible for emitting progress and completion.

| Channel | Direction | Payload | Meaning |
|---|---|---|---|
| `Loading.Progress.Percentage` | Target scene → loading scene | `number` 0..1 | Progress bar value. |
| `Loading.Progress.Info` | Target scene → loading scene | `string` | Optional status text ("Loading meshes…"). |
| `Loading.Finished` | Target scene → loading scene | none | Target scene is fully loaded; loading scene should play its outro. |
| `Loading.MinDisplayElapsed` | Engine → loading scene | none | Emitted by the engine when the AppSettings min-display gate opens. Useful for scenes that have a conditional outro. |

Example — target scene reports its progress as it loads:
```lua
function Level2:Start()
    SignalBus.Emit("Loading.Progress.Info", "Loading meshes")
    SignalBus.Emit("Loading.Progress.Percentage", 0.0)
    self.tasks = self:GatherLoadTasks()
end

function Level2:Tick(dt)
    if not self.done then
        local pct = self:DoOneStep()
        SignalBus.Emit("Loading.Progress.Percentage", pct)
        if pct >= 1.0 then
            self.done = true
            SignalBus.Emit("Loading.Finished")
        end
    end
end
```

Example — loading scene drives a progress bar from the same channels:
```lua
function LoadingScreen:Start()
    SignalBus.Subscribe("Loading.Progress.Percentage", self, function(_, pct)
        self.progressBar:SetValue(pct)
    end)
    SignalBus.Subscribe("Loading.Progress.Info", self, function(_, text)
        self.statusText:SetText(text)
    end)
    SignalBus.Subscribe("Loading.Finished", self, function(_)
        self:PlayOutroThen(function() LoadingMenu.Close() end)
    end)
end
```

---
## Auto-routing of `World:LoadScene`

When a loading scene is configured (either by `SetMenuScene` or by `AppSettings → Default Loading Scene`), **non-instant** `world:LoadScene` calls automatically route through the loading menu:

```lua
-- Equivalent to LoadingMenu.Open("SC_Level2") when a loading scene is configured.
-- Falls back to a direct queued swap when no loading scene is set.
GetWorld(0):LoadScene("SC_Level2")
```

Instant loads (`world:LoadScene(name, true)`) **bypass** the loading menu — this is intentional so engine-internal calls (startup default scene, second-screen preview, editor PIE entry) never get wrapped in a menu.

---
## AppSettings

Three project-wide fields live in **Editor → App Settings → General**:

| Field | Type | Default | Effect |
|---|---|---|---|
| Default Loading Scene | string | `""` | Scene name used by `LoadingMenu.Open` when no `SetMenuScene` override is active. |
| Loading Min Display (s) | float | `0.0` | Minimum time the menu remains visible. `Close()` is parked until this elapses. |
| Loading Timeout (s) | float | `0.0` | If the target scene never emits `Loading.Finished` within this many seconds, the engine logs a warning and auto-closes. `0` disables the watchdog. |

All three are persisted to `<project>/Config.ini` as `DefaultLoadingScene=`, `LoadingMinDisplaySeconds=`, and `LoadingTimeoutSeconds=`.

---
## State diagram

```
              Open(target)
   Idle  ──────────────────►  Loading
    ▲                            │ target scene emits Loading.Finished
    │                            │ AND min-display elapsed
    │ (next tick)                ▼
    └───────── Closing  ◄────── Close()
```

- **Idle** — no transition in flight.
- **Loading** — menu visible, target scene swapped in underneath, both ticking. SignalBus channels are active.
- **Closing** — `Close()` was called and the min-display gate has opened. The menu node is destroyed on the next engine update.

---
## C++

```cpp
#include "LoadingMenu.h"

GetLoadingMenu()->SetMenuScene("SC_LoadingSciFi");
GetLoadingMenu()->Open("SC_Level2");

// From any node, emit progress on the bus:
GetSignalBus()->Emit("Loading.Progress.Percentage", { Datum(0.5f) });
GetSignalBus()->Emit("Loading.Finished", {});

// Force-close (used at shutdown):
GetLoadingMenu()->ForceClose();
```
