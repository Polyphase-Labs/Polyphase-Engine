# LoadingMenu Subsystem

Engine-level orchestrator for "loading screen" scene transitions. Owns a small state machine that overlays a designer-authored Scene asset on top of the current world, drives the gameplay-scene swap underneath it, and tears the overlay down when the new scene reports completion via SignalBus.

The subsystem is intentionally thin: it sits on top of `Scene::Instantiate`, `World::LoadScene`, `Node::SetPersistent`, and `SignalBus` — none of which it reimplements.

---

## 1. Files

| File | Purpose |
|---|---|
| `Engine/Source/Engine/LoadingMenu.h` | `LoadingMenu` class, `LoadingState` enum, `GetLoadingMenu()` accessor. |
| `Engine/Source/Engine/LoadingMenu.cpp` | State machine, scene spawn, persistence, SignalBus subscription, headless bypass. |
| `Engine/Source/LuaBindings/LoadingMenu_Lua.h` | Lua binding declarations (Input_Lua pattern). |
| `Engine/Source/LuaBindings/LoadingMenu_Lua.cpp` | Lua binding implementations + `Bind()` global table install. |

## 2. Engine integration points

| File | Edit |
|---|---|
| `Engine/Source/Engine/EngineTypes.h` | `EngineConfig::mDefaultLoadingScene` (string), `mLoadingMinDisplaySeconds` (float), `mLoadingTimeoutSeconds` (float). |
| `Engine/Source/Engine/Engine.cpp` | `#include "LoadingMenu.h"`. In `Update`'s per-world loop: `GetLoadingMenu()->Update(gameDeltaTime, (int32_t)i)` before each `sWorlds[i]->Update`. In `Shutdown`: `GetLoadingMenu()->ForceClose()` immediately before the world destruction loop. In `WriteEngineConfig` / `ReadEngineConfig`: 3 new fields under `DefaultLoadingScene=`, `LoadingMinDisplaySeconds=`, `LoadingTimeoutSeconds=`. |
| `Engine/Source/Engine/World.cpp` | `#include "LoadingMenu.h"`. Top of `LoadScene(name, instant)`: auto-route into `LoadingMenu::Open` when `!instant && GetIndex() == 0 && !mHeadless && GetLoadingMenu()->ShouldInterceptLoadScene()`. Reentrancy is guarded by `LoadingMenu::mInternalLoad` so the internal `world->LoadScene` issued by `Open` doesn't recurse. |
| `Engine/Source/LuaBindings/LuaBindings.cpp` | `#include "LuaBindings/LoadingMenu_Lua.h"` + `LoadingMenu_Lua::Bind()` immediately after `SignalBus_Lua::Bind()`. |
| `Engine/Source/Editor/AppSettings/AppSettingsWindow.{h,cpp}` | `mDefaultLoadingSceneBuffer[256]` + `ImGui::InputText` / `InputFloat` controls in `DrawGeneralSection`, mirrored in `Open()` `strncpy` block. |
| `Engine/Engine.vcxproj` + `.vcxproj.filters` | 4 new file entries (`Source Files\Engine` + `Source Files\LuaBindings`). CMake globs and `Makefile_Linux` directory wildcards pick the files up automatically. |

## 3. State machine

```
              Open(target)
   Idle  ──────────────────►  Loading
    ▲                            │  Loading.Finished AND elapsed >= minDisplay
    │                            │  OR mElapsed > timeout > 0
    │ (next tick)                ▼
    └───────── Closing  ◄────── Close()
```

| State | Tick behaviour | Set by |
|---|---|---|
| `Idle` | No-op. | Constructor; `ForceClose`; one tick after `Closing`. |
| `Loading` | `mElapsed += dt`; emit `Loading.MinDisplayElapsed` once when gate opens; auto-close on timeout; transition to `Closing` if `mCloseRequested && elapsed >= minDisplay`. | `Open` on success. |
| `Closing` | `TeardownMenuRoot()` then `mState = Idle`. | `Close()` when the min-display gate is open, or while it was already open. |

Reentrant `Open` while not `Idle` is logged as a warning and returns `false`. The user-facing rule is "call `Close()`, then `Open()` — don't queue mid-load".

## 4. Persistence across the root swap

`Open` instantiates the loading scene via `Scene::Instantiate()`, calls `Node::SetPersistent(true)` on the new root, then `AddChild`s it under the current world's root. The follow-up `world->LoadScene(target, false)` queues `mQueuedRootNode`. On the next `World::Update`:

1. `mQueuedRootNode` is detached.
2. `DestroyRootNode()` runs `ExtractPersistingNodes()` (when `IsPlaying()`), which detaches the menu root and parks it in `mPersistingNodes`.
3. The old gameplay root is dropped.
4. `SetRootNode(mQueuedRootNode)` re-parents every `mPersistingNodes` entry under the new root.

End result: the menu node is continuously rendered, the gameplay scene under it is fully swapped, and `LoadingMenu` itself does no manual detach/reparent dance — it relies entirely on the engine's existing persistence path.

This is gated on `IsPlaying()` in `World.cpp:712 / :757 / :778`. `LoadingMenu::Open` only ever runs from gameplay contexts (PIE and standalone), so this is always true when it matters. In rare pure-editor contexts the loading scene would still spawn but would be destroyed with the old root — a corner case considered acceptable; the editor doesn't surface loading transitions today.

## 5. Listener identity

`SignalBus::Subscribe` requires a `Node*` listener stored as `NodePtrWeak`. `LoadingMenu` is not a `Node`. The implementation uses `mMenuRoot.Get()` itself as the listener — it's a real `Node`, alive for the whole transition, and self-prunes from the bus when `Close` resets the `NodePtr`. The C++ handler is the static free function `LoadingMenu::OnLoadingFinishedSignal` which calls `GetLoadingMenu()->NotifyFinished()`. The listener pointer is purely for subscription lifetime — the handler does not read from it.

## 6. SignalBus channel contract

| Channel | Direction | Datum types | Notes |
|---|---|---|---|
| `Loading.Progress.Percentage` | target scene → loading scene | `[Datum(float)]` | 0..1. Not validated. |
| `Loading.Progress.Info` | target scene → loading scene | `[Datum(string)]` | Free-form status text. |
| `Loading.Finished` | target scene → engine | `[]` | Sets `mFinishedSignalReceived`. Independent of `Close()`. |
| `Loading.MinDisplayElapsed` | engine → loading scene | `[]` | Emitted exactly once per `Open()`, when `mElapsed` first reaches `mLoadingMinDisplaySeconds`. |

`Loading.*` is a **reserved engine namespace** — document it as off-limits to gameplay scripts. There is no enforcement.

## 7. Auto-routing of `World::LoadScene`

`World::LoadScene(const char* name, bool instant)` intercepts when **all** of these hold:

- `!instant` (instant loads bypass — used by startup default scene, second-screen preview, editor PIE entry; bypassing here avoids surprise menus during engine-internal swaps).
- `GetIndex() == 0` (world 0 only — 3DS world 1 is the bottom screen).
- `!GetEngineConfig()->mHeadless`.
- `GetLoadingMenu()->ShouldInterceptLoadScene()`, which is true iff `mState == Idle`, `!mInternalLoad`, and a loading-scene name resolves (override or `mDefaultLoadingScene`).

When intercepted, `LoadingMenu::Open(name, GetIndex())` runs and the original `LoadScene` returns. Inside `Open`, `mInternalLoad = true` is set around the internal `world->LoadScene(target, false)` call so the intercept doesn't recurse.

## 8. Lua API surface

Global table `LoadingMenu` (binding pattern mirrors `Input_Lua` / `SignalBus_Lua`):

- `LoadingMenu.SetMenuScene(string)` — `""` clears the override.
- `LoadingMenu.GetMenuScene() -> string`
- `LoadingMenu.Open(string target [, int worldIndex=0]) -> bool` — `false` means "fell through to direct load" or "rejected mid-transition".
- `LoadingMenu.Close()` — gated by min display.
- `LoadingMenu.IsActive() -> bool`
- `LoadingMenu.GetState() -> string` ("Idle" | "Loading" | "Closing")
- `LoadingMenu.GetTargetScene() -> string`

Registration: `LoadingMenu_Lua::Bind()` at `LuaBindings.cpp:130` (right after `SignalBus_Lua::Bind()`). Stub regeneration via `python Tools/generate_lua_stubs.py` picks up the binding automatically.

## 9. Out-of-scope (v2 candidates)

- `AssetManager.AsyncPreload({list})` helper that emits progress signals as each asset resolves. Requires extending `AssetManager` with a pending-count API; not added.
- Per-world `LoadingMenu` instances for 3DS bottom screen. v1 owns world 0 only; world ≠ 0 falls through to direct loads.
- Transaction IDs on `Loading.*` channels to prevent cross-talk. Not possible today (only one transition at a time) but useful insurance if v2 ever allows parallel transitions.
- Built-in fade-in/fade-out helper widget. Designers compose intro/outro from existing widget tooling.

## 10. Gotchas

- `Scene::Instantiate` is synchronous. A huge loading scene will stall the frame in which `Open` is called. `LogDebug` lines bracket the call so a hang shows in logs.
- `Close()` is **not** an immediate teardown — it is a request, gated by `mLoadingMinDisplaySeconds`. The actual destruction happens in the first `LoadingMenu::Update` tick after the gate opens.
- `Loading Timeout (s)` is **off by default** (`0.0` = disabled). It is intended for shipping safety nets; turning it on during development can mask genuine deadlocks.
- The engine consults `EngineConfig` every frame in `Update` (no caching) — changing `mLoadingMinDisplaySeconds` mid-transition takes effect immediately.
- `World::LoadScene` auto-routing only kicks in when `instant == false`. Code that explicitly wants a raw root swap (debug menus, error recovery) passes `true`.
- Persistence depends on `World::IsPlaying()` being `true`. Pure-editor calls without entering PIE will destroy the menu with the old root — out of scope for v1.

## 11. Related docs

- Designer guide: `Documentation/Development/LoadingMenu.md`
- Lua reference: `Documentation/Lua/Systems/LoadingMenu.md`
- Persistence machinery: see `.llm/NodeSystem.md` (Node lifecycle, `SetPersistent`) and `Engine/Source/Engine/World.cpp:1459-1475` (`ExtractPersistingNodes`).
- Event plumbing: `.llm/Architecture.md` covers SignalBus at the macro level; `Engine/Source/Engine/SignalBus.{h,cpp}` is the source of truth.
