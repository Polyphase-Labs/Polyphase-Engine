# Loading Menu System

The Loading Menu system lets you author normal Polyphase Scene assets as "loading screens" and have the engine play them over the boundary between two gameplay scenes.

It is a thin orchestration layer on top of three existing engine primitives:

- The Scene asset (3D or widget-based — any normal scene).
- `SignalBus` (for the `Loading.*` progress/completion channels).
- `Node:SetPersistent(true)`, which is the engine's built-in mechanism for letting a node survive a root-scene swap.

Designers compose the look and pacing of the transition entirely from Lua scripts attached to the loading scene. The engine just spawns the menu, queues the gameplay swap, and tears the menu down when told.

## What it does, step by step

1. A script calls `LoadingMenu.Open("SC_Level2")`.
2. The engine looks up the **loading scene** name (an override from `LoadingMenu.SetMenuScene` if set; otherwise the project default from AppSettings).
3. It instantiates that loading scene, marks the new root persistent, and parents it under the current world's root. The loading scene's `Start()` runs and any intro animation kicks off.
4. The engine then calls `world:LoadScene(target, false)` internally — that queues a root swap. On the next world update, the old gameplay root is destroyed; the menu node is detached, parked in the world's `mPersistingNodes`, and re-attached as a child of the new gameplay root. The loading scene keeps ticking the whole time.
5. The target scene's scripts run their loading work and emit progress on the `Loading.*` SignalBus channels. The loading scene subscribes to those channels and updates its UI.
6. The target scene emits `SignalBus.Emit("Loading.Finished")` when it's ready. The loading scene plays its outro animation and calls `LoadingMenu.Close()`.
7. On the next engine update the loading menu node is destroyed and the system returns to `Idle`.

## Authoring a loading scene

A loading scene is a regular Polyphase Scene asset. It can be 3D, widget-based, or both. The only requirement is that its root or a child has a Lua script that subscribes to the `Loading.*` SignalBus channels and (when its outro is done) calls `LoadingMenu.Close()`.

A minimal example using widgets:

```lua
-- SC_LoadingSimple's root script
local LoadingScreen = {}

function LoadingScreen:Start()
    self.bar       = self:FindChild("ProgressBar")    -- a Slider / ProgressBar widget
    self.statusTxt = self:FindChild("StatusText")     -- a Text widget

    SignalBus.Subscribe("Loading.Progress.Percentage", self, function(_, pct)
        self.bar:SetValue(pct)
    end)

    SignalBus.Subscribe("Loading.Progress.Info", self, function(_, msg)
        self.statusTxt:SetText(msg)
    end)

    SignalBus.Subscribe("Loading.Finished", self, function(_)
        -- No outro animation? Close immediately. The engine still respects
        -- AppSettings → Loading Min Display before tearing down.
        LoadingMenu.Close()
    end)
end

return LoadingScreen
```

With a fade-out outro, swap the `Loading.Finished` handler for something like:

```lua
SignalBus.Subscribe("Loading.Finished", self, function(_)
    Tween.Value(Easing.OutQuad, 1.0, 0.0, 0.5,
        function(alpha, _, _) self:SetOpacity(alpha) end,
        function()             LoadingMenu.Close()    end)
end)
```

## Reporting progress from the target scene

The target gameplay scene's root script is responsible for telling the loading screen how it's doing:

```lua
-- SC_Level2's root script
function Level2:Start()
    self.steps = self:GatherWorkUnits()  -- e.g. tiles to spawn, assets to warm
    self.idx   = 0
    SignalBus.Emit("Loading.Progress.Info", "Spawning entities")
    SignalBus.Emit("Loading.Progress.Percentage", 0.0)
end

function Level2:Tick(dt)
    if self.idx < #self.steps then
        self.steps[self.idx + 1]()
        self.idx = self.idx + 1
        SignalBus.Emit("Loading.Progress.Percentage", self.idx / #self.steps)
    elseif not self.finished then
        self.finished = true
        SignalBus.Emit("Loading.Finished")
    end
end
```

Nothing stops you from doing all the work in `Start()` synchronously and emitting `Loading.Finished` immediately — the engine's min-display gate will hold the loading menu up for the configured duration regardless.

## Configuring AppSettings

Open **Editor → Project → App Settings → General**:

| Field | What it does |
|---|---|
| **Default Loading Scene** | Used by `LoadingMenu.Open` (and auto-routed `World:LoadScene`) whenever no override has been set via `LoadingMenu.SetMenuScene`. Leave blank if you want auto-routing to be a no-op. |
| **Loading Min Display (s)** | Minimum time the menu stays visible. A `LoadingMenu.Close()` request that arrives before this elapses is queued and applied as soon as it does. Useful to avoid flicker on fast loads. |
| **Loading Timeout (s)** | If the target scene never emits `Loading.Finished` within this many seconds, the engine logs a warning and force-closes the menu. `0` disables this watchdog. |

These persist to `<project>/Config.ini` as `DefaultLoadingScene=`, `LoadingMinDisplaySeconds=`, `LoadingTimeoutSeconds=`.

## Switching themes per context

Use `LoadingMenu.SetMenuScene` to swap which scene plays as the loading screen. The override persists until you change it again or pass `""` to revert to the AppSettings default.

```lua
function Game:OnEnterSciFiAct()
    LoadingMenu.SetMenuScene("SC_LoadingSciFi")
end

function Game:OnEnterFantasyAct()
    LoadingMenu.SetMenuScene("SC_LoadingFantasy")
end

function Game:OnEnterTitleScreen()
    LoadingMenu.SetMenuScene("")  -- back to project default
end
```

## Implicit `World:LoadScene` routing

When a loading scene is resolvable (override or default), non-instant `world:LoadScene` calls automatically go through the menu:

```lua
-- Both of these now show the loading menu when one is configured:
GetWorld(0):LoadScene("SC_Level2")
GetWorld(0):LoadScene("SC_Level2", false)
```

Instant loads (`world:LoadScene(name, true)`) **always bypass** the menu. This is what the engine uses internally for startup, the second-screen preview, and editor PIE entry — so the loading menu only ever shows for genuine in-game transitions.

If you specifically want a raw swap from gameplay code without invoking the menu (e.g. a "Quit to main menu" debug button), pass `true`:

```lua
GetWorld(0):LoadScene("SC_MainMenu", true)
```

## Authoring tips

- **Test with no animation first.** Build a simple solid-color widget loading scene whose script just subscribes to `Loading.Progress.Percentage` and updates a `ProgressBar` widget. Once that works end-to-end, add intro/outro animations.
- **Set `Loading Min Display` early.** Without it, fast loads will flicker the menu for a single frame. `0.5` – `1.0` seconds is a reasonable default.
- **Use `Loading Timeout` only in shipping builds.** Leave it at `0` during development so you don't paper over a target scene that genuinely deadlocked.
- **Treat the `Loading.*` channels as reserved.** Don't emit on them from unrelated systems. If you need other "global progress" events, namespace them differently (e.g. `MyGame.Save.Percentage`).
- **Loading scenes themselves block while instantiating.** `Scene::Instantiate` is synchronous. Keep the loading scene's node tree small enough that its own spawn doesn't itself stall the frame.

## Built-in behaviours

- **Re-entrant `Open`.** Calling `LoadingMenu.Open` again while a transition is already in flight logs a warning and returns `false`. To swap targets mid-load, `Close()` first.
- **Headless mode.** `LoadingMenu.Open` becomes a direct `world:LoadScene` call — no menu node is spawned. Useful for server / dedicated runtimes.
- **Editor shutdown.** When the engine shuts down with a transition in flight, `LoadingMenu` is force-closed before worlds are destroyed so the menu node never points at freed memory.
- **Min display gate.** If `Close()` arrives before `Loading Min Display` elapses, the request is parked. As soon as the gate opens the engine also emits `Loading.MinDisplayElapsed` on the bus — scenes with conditional outros can listen to it.

## Related

- [Lua API: LoadingMenu](../Lua/Systems/LoadingMenu.md) — full reference of every method and signal channel.
- [Lua API: SignalBus](../Lua/Systems/SignalBus.md) — the global bus used by the `Loading.*` channels.
- [Lua API: Scene](../Lua/Assets/Scene.md) — the asset type that backs every loading screen.
