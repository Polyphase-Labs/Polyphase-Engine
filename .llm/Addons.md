# Addon / Plugin System

## Overview

Polyphase has two plugin systems: a **RuntimePluginManager** for game/runtime plugins and a **NativeAddonManager** (editor-only) for editor extensions. Both share the same `EditorUIHooks` API for editor integration.

## Key Files

| File | Purpose |
|------|---------|
| `Engine/Source/Plugins/RuntimePluginManager.h/.cpp` | Runtime plugin lifecycle |
| `Engine/Source/Plugins/PolyphasePluginAPI.h` | Plugin descriptor struct |
| `Engine/Source/Plugins/PolyphaseEngineAPI.h` | Engine API exposed to plugins |
| `Engine/Source/Plugins/EditorUIHooks.h` | Editor UI extension API (~940 lines) |
| `Engine/Source/Editor/Addons/NativeAddonManager.cpp` | Editor addon hot-reload system |
| `Engine/Source/Editor/Addons/AddonManager.cpp` | Addon discovery and management |
| `Engine/Source/Editor/Addons/AddonCreator.cpp` | Addon project scaffolding |
| `Engine/Source/Editor/Addons/AddonsWindow.cpp` | Addons UI panel |
| `Plugins/` | External plugins (e.g., Blender addon) |

## Plugin Descriptor

```cpp
#define OCTAVE_PLUGIN_API_VERSION 2

struct PolyphasePluginDesc {
    uint32_t apiVersion;           // Must match OCTAVE_PLUGIN_API_VERSION
    const char* pluginName;
    const char* pluginVersion;     // e.g., "1.0.0"

    // Lifecycle
    int (*OnLoad)(PolyphaseEngineAPI* api);    // Return 0 on success
    void (*OnUnload)();

    // Per-frame
    void (*Tick)(float deltaTime);          // Game tick
    void (*TickEditor)(float deltaTime);    // Editor tick (EDITOR only)

    // Registration
    void (*RegisterTypes)(void* nodeFactory);
    void (*RegisterScriptFuncs)(lua_State* L);

    // Editor (EDITOR only, nullptr for game-only plugins)
    void (*RegisterEditorUI)(EditorUIHooks* hooks, uint64_t hookId);
    void (*OnEditorPreInit)();
    void (*OnEditorReady)();
};
```

## Plugin Registration

Static registration via macro:
```cpp
POLYPHASE_REGISTER_PLUGIN(myPlugin, MyPlugin_GetDesc)
```

This creates a static registrar that calls `QueuePluginRegistration()` before `main()`. The `RuntimePluginManager` processes queued registrations during `Create()`.

## Plugin Lifecycle

1. **Static init**: `POLYPHASE_REGISTER_PLUGIN` queues the plugin
2. **Create()**: RuntimePluginManager processes queued registrations
3. **Initialize()**: Calls `OnLoad()`, `RegisterTypes()`, `RegisterScriptFuncs()` for each plugin
4. **Main loop**: `TickAllPlugins(deltaTime)` calls `Tick()` (and `TickEditor()` in editor builds)
5. **Shutdown()**: Calls `OnUnload()` for each plugin

## Engine API (PolyphaseEngineAPI)

Exposed to plugins via `PolyphaseEngineAPI*` passed to `OnLoad()`:

**Logging**: `LogDebug()`, `LogWarning()`, `LogError()`

**Lua**: Full Lua C API wrappers — stack manipulation, type checking, table operations, metatables

**World**: `GetWorld(index)`, `GetNumWorlds()`

**Nodes**: `SpawnNode(world, typeName)`, `DestroyNode(node)`, `FindNode(world, name)`

**Node3D**: Get/Set Position, Rotation, Scale

**Assets**: `LoadAsset(name)`, `FetchAsset(name)`, `UnloadAsset(name)`

**Audio**: `PlaySound2D()`, `StopAllSounds()`, `SetMasterVolume()`

**Input**: Key, mouse button, position, delta, scroll queries

**Time**: `GetDeltaTime()`, `GetElapsedTime()`

**Editor UI**: `editorUI` pointer (nullptr in game builds)

## Editor UI Hooks (EditorUIHooks)

The `EditorUIHooks` struct provides function pointers for extending the editor. All hooks are keyed by a `HookId` (uint64) for cleanup.

**Categories:**

| Category | Key Functions | Purpose |
|----------|---------------|---------|
| Menus | `AddMenuItem()`, `AddTopLevelMenuItem()` | Extend menu bar and context menus |
| Windows | `RegisterWindow()`, `OpenWindow()`, `CloseWindow()` | Custom dockable panels |
| Inspectors | `RegisterInspector()` | Per-node-type custom property UI |
| Context Menus | `AddNodeContextItem()`, `AddAssetContextItem()`, `AddViewportContextItem()` | Right-click menus |
| Toolbar | `AddToolbarItem()` | Custom toolbar buttons |
| Node Menus | `AddNodeMenuItems()`, `AddSpawnBasic3dItems()` | Extend "Add Node" menus |
| Asset Creation | `AddCreateAssetItems()` | Extend "Create Asset" menu |
| Scene Types | `RegisterSceneType()` | Custom scene templates |
| Overlays | `RegisterViewportOverlay()` | Viewport overlays |
| Preferences | `RegisterPreferencesPanel()` | Custom settings pages |
| Shortcuts | `RegisterShortcut()` | Keyboard shortcuts |
| Property Drawers | `RegisterPropertyDrawer()` | Custom property UI |
| Hierarchy GUI | `RegisterHierarchyItemGUI()` | Per-row hierarchy overlay |
| Import | `RegisterAssetImporter()` | Custom file importers |
| Drag-Drop | `RegisterDragDropHandler()` | Custom drag-drop handling |
| Gizmo Tools | `RegisterGizmoTool()` | Custom transform tools |
| Play Targets | `AddPlayTarget()` | Custom launch targets |
| Preview | `AddGamePreviewResolution()` | Device resolution presets |

**Events:**
- Project: `RegisterOnProjectOpen/Close/Save()`
- Scene: `RegisterOnSceneOpen/Close()`
- Packaging: `RegisterOnPackageStarted/Finished()`, `RegisterOnPreBuild/PostBuild()`
- Editor: `RegisterOnSelectionChanged()`, `RegisterOnPlayModeChanged()`, `RegisterOnEditorShutdown()`, `RegisterOnEditorModeChanged()`
- Assets: `RegisterOnAssetImported/Deleted/Saved/Open()`
- Hierarchy: `RegisterOnHierarchyChanged()`

**Cleanup:** `RemoveAllHooks(hookId)` removes all hooks for a given addon.

## NativeAddonManager (Editor)

Located in `Engine/Source/Editor/Addons/`. Manages editor-side addon loading, including hot-reload support for DLL-based addons. Uses the `EditorUIHookManager` internally to register hooks.

## Example: Blender Addon

**Location:** `Plugins/Blender/polyshade-gameengine-connect/`

A Python Blender addon that exports glTF with Polyphase metadata (mesh type, asset reference, script, material type). Demonstrates external tooling integration rather than runtime plugin API usage.

## Native Addon Build Requirements

Native addons compile against engine headers with `API_VULKAN=1`, and `Graphics/GraphicsTypes.h`
pulls in the Vulkan backend headers, so every addon build needs `vulkan/vulkan.h` even if the
addon never touches Vulkan. `NativeAddonManager::ResolveVulkanIncludeDir()` probes, in order:
`VULKAN_SDK`, `<engine>/External/Vulkan/include` (staged by `Installers/stage_distribution.py`
and `package_windows_sdk.py` so installed editors need no Vulkan SDK), then the default SDK
install locations (`C:\VulkanSDK\*`, `/usr/include`, `~/VulkanSDK/*`).

`NativeAddonManager::RunBuildPreflight()` runs before any compiler is spawned and checks
declared dependency addons on disk, engine include directories, Vulkan headers, and the engine
import libraries. Failures (and compiler failures classified by `ClassifyBuildFailure()`) set
`mBuildError` + `mFixHint` on the addon state and open the "Native Addon Problem" modal
(`EditorImgui.cpp`, `DrawNativeAddonBuildFailureModal`), which shows the fix, a download link
when one applies, and an Install button for each missing dependency.

Dependencies declared in `package.json` are resolved by `AddonDependencyResolver` at install
time and at project load. If the registry has not been fetched yet, the resolver refreshes it
once before declaring a dependency unresolvable. Addons with missing dependencies are not built
or loaded; the Addons window shows "Missing dependency: <id>" with an Install button.

## Removing and Updating Addons

Removal cascades: `AddonDependencyResolver::CollectDependents()` walks every
`Packages/*/package.json` and the Remove modal lists the installed dependents that go with the
target (local packages without an `installed_addons.json` record are only warned about). This
matters because `DiscoverNativeAddons` runs `ResolveAll` with auto-fetch on every project open,
so any surviving package that still declares the removed addon would pull it straight back.
The Remove button queues ids dependents-first into `EditorState::mPendingAddonUninstalls`;
`AddonsWindow::ProcessPendingUninstalls` runs from the end-of-frame dispatcher. If any queued
addon's DLL is loaded it goes through `NativeAddonManager::RemoveNativeAddonsWithProjectRestart`
(close scenes, unload, `ForgetAddon`, `UninstallAddon`, reopen with no rebuild); otherwise the
packages are deleted under `EditorProgress` and the asset browser's Packages dir is rescanned.
`UninstallAddon` deletes `Packages/<id>` and `Intermediate/Plugins/<id>` but keeps the shared
AppData `AddonCache` copy.

Update detection has two signals. The primary one is the branch head commit: `InstallAddon`
stamps `branch` / `commit` / `commitDate` (plus `pinned`, `standalone`) into
`installed_addons.json` from an `AddonInstallSource`, and "Check for Updates" (manual only, via
`EditorState::mCheckAddonUpdatesAtEndOfFrame`) calls `AddonManager::FetchLatestCommit`, which
queries `api.github.com/repos/<o>/<r>/commits?sha=<branch>&path=<id>` through the AutoUpdater
`HttpClient`. The secondary signal is the registry version string. `GetUpdateStatus()` reports
NewCommits / NewVersion / UpToDate / NotChecked / Pinned / NoSource / Error; `HasUpdate()` is
true for the first two. Update re-runs the normal download path after a confirm modal, and
`InstallAddon` carries the previous record's enabled / native-mode / trust settings over.

## Documentation

User-facing docs:
- `Documentation/Info/Addons.md` — General addon info
- `Documentation/Development/NativeAddon/NativeAddon.md` — Native addon overview
- `Documentation/Development/NativeAddon/Examples/` — 30+ examples covering all hook types
