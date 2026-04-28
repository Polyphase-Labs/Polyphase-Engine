# Editor System

## Overview

The editor is built on ImGui with a docking layout. All editor code is guarded by `#if EDITOR`. The main rendering happens in `EditorImgui.cpp` (~7600 lines), with specialized panels in subdirectories.

## Key Files

All paths relative to `Engine/Source/Editor/`:

| File | Purpose |
|------|---------|
| `EditorImgui.h/.cpp` | Main UI rendering: `EditorImguiInit()`, `EditorImguiDraw()` |
| `EditorState.h/.cpp` | Global editor state singleton (`GetEditorState()`) |
| `ActionManager.h/.cpp` | Undo/redo system |
| `EditorUIHookManager.h/.cpp` | Plugin extension hooks |
| `EditorTypes.h` | Enums: ControlMode, TransformLock, EditorMode, PaintMode |
| `EditorConstants.h` | Drag-drop types, basic node names |
| `EditorUtils.h/.cpp` | Editor utility functions |
| `InputManager.h/.cpp` | Hotkey handling |
| `Viewport3d.h/.cpp` | 3D viewport camera (pilot, orbit, pan) |
| `Viewport2d.h/.cpp` | 2D widget viewport |
| `PaintManager.h/.cpp` | Mesh instance/color painting |
| `Grid.h/.cpp` | Grid rendering |
| `CustomImgui.h/.cpp` | ImGui extensions |

## Major Panels

Rendered in `EditorImguiDraw()` via docked ImGui windows:

| Panel | Location | Purpose |
|-------|----------|---------|
| **Viewport** | `EditorImgui.cpp` | 3D/2D scene view with gizmos |
| **Scene Hierarchy** | `EditorImgui.cpp` (`DrawScenePanel()`) | Node tree, drag-drop, context menus |
| **Properties** | `EditorImgui.cpp` (`DrawPropertiesPanel()`) | Inspector for selected node/asset |
| **Assets** | `EditorImgui.cpp` | Asset browser (Project/Addons tabs) |
| **Debug Log** | `DebugLog/DebugLogWindow.h/.cpp` | Log viewer with severity filtering |
| **Scripts** | `EditorImgui.cpp` (`DrawScriptsPanel()`) | Script browser |
| **Script Editor** | `ScriptEditor/ScriptEditorWindow.h/.cpp` | Zep-based code editor |
| **Node Graph** | `NodeGraph/NodeGraphPanel.h/.cpp` | Visual scripting editor (Ctrl+C/V/D, export/import) |
| **Timeline** | `Timeline/TimelinePanel.h/.cpp` | Animation timeline editor |
| **Game Preview** | `GamePreview/GamePreview.h/.cpp` | Device resolution preview |
| **3DS Preview** | `EditorImgui.cpp` | 3DS hardware preview |
| **Theme Editor** | `ThemeEditor/ThemeEditorWindow.h/.cpp` | Theme customization |
| **Packaging** | `Packaging/PackagingWindow.h/.cpp` | Build & deploy system |

## EditorState

**Singleton accessor:** `GetEditorState()`

Key state groups:
- **Selection**: `mSelectedNodes`, `mSelectedAssetStub`, `mInspectedObject`
- **Mode**: `mMode` (Scene/Scene2D/Scene3D), `mControlMode` (Default/Pilot/Translate/Rotate/Scale/Pan/Orbit)
- **Camera**: `mEditorCamera`, perspective/ortho settings
- **Viewport**: `mViewportX/Y/Width/Height`, pane visibility toggles
- **Play In Editor**: `mPlayInEditor`, `mPaused`, `mEjected`
- **Transform**: `mTransformLock` (axis/plane locking)
- **Gizmo**: `mGizmoOperation` (Translate/Rotate/Scale), `mGizmoMode` (World/Local)
- **Paint**: `mPaintMode` (None/Color/Instance)
- **Timeline**: playhead time, zoom, scroll, selection indices
- **Asset Browser**: current directory per tab, filter strings, navigation history

## ActionManager (Undo/Redo)

**Pattern:** Every undoable operation creates an `Action` subclass with `Execute()` and `Reverse()` methods, pushed to the history stack.

Key high-level methods (all `EXE_*` prefixed):
- `EXE_SpawnNode()`, `EXE_DeleteNodes()`, `EXE_AttachNode()`
- `EXE_EditTransform()`, `EXE_SetWorldPosition/Rotation/Scale()`
- `EXE_EditProperty()` — generic property editing
- `EXE_TimelineAddTrack/RemoveTrack/AddClip/RemoveClip()`

Also: `CreateNewProject()`, `OpenProject()`, `SaveScene()`, `ImportAsset()`, `BuildData()`, `DuplicateNodes()`.

## EditorUIHookManager (Extension System)

Enables addons/plugins to extend the editor UI. Categories of hooks:

- **Menus**: `RegisteredMenuItem`, top-level menus, context menus
- **Windows**: Custom dockable windows
- **Inspectors**: Per-node-type custom property UI
- **Toolbar**: Custom toolbar items
- **Node/Asset Creation**: Extend "Add Node" and "Create Asset" menus
- **Scene Types**: Custom scene creation templates
- **Viewport Overlays**: Render over the viewport
- **Preferences Panels**: Custom settings pages
- **Shortcuts**: Keyboard shortcuts with key parsing
- **Property Drawers**: Custom UI for specific property types
- **Drag-Drop & Import**: Custom handlers and importers
- **Gizmo Tools**: Custom transform tools
- **Play Targets**: Custom launch targets (emulators, devices)

Events: `FireOnProjectOpen/Close()`, `FireOnSceneOpen/Close()`, `FireOnSelectionChanged()`, `FireOnPlayModeChanged()`, `FireOnAssetImported/Deleted/Saved()`, etc.

Cleanup: `RemoveAllHooks(hookId)` removes all hooks for a given addon.

## Preferences System

**Files:** `Preferences/PreferencesManager.h/.cpp`, `Preferences/PreferencesModule.h/.cpp`

Hierarchical module system. Each module: `Render()`, `LoadSettings()`, `SaveSettings()`.

Modules: General (autosave, debug flags), Appearance > Theme (CSS themes, font) + Viewport, External > Editors + Launchers, Packaging > Docker, Input (gamepad emulation), Editor Hotkeys (rebindable editor shortcuts).

`PreferencesManager::Get()->RegisterModule(module)`, `LoadAllSettings()`, `SaveAllSettings()`.

## Editor Hotkey System

Every rebindable editor hotkey routes through a central registry so users can remap shortcuts and viewport movement keys. Game code in PIE never sees keys the editor owns when input is in editor scope.

### Files (all under `Engine/Source/Editor/`)

| File | Role |
|------|------|
| `Hotkeys/EditorAction.h/.cpp` | `enum class EditorAction` + `struct KeyBinding` + static metadata table (display name, category, description, default binding, stable serialize key, symbolic key-name conversion). |
| `Hotkeys/EditorHotkeyMap.h/.cpp` | Singleton (`Create/Destroy/Get`). Query API, PIE-aware gate, preset save/load/delete, JSON serialization, conflict detection, file-based import/export. |
| `Hotkeys/EditorHotkeysWindow.h/.cpp` | ImGui remap dialog. Preset bar, search, category-collapsing rows, capture overlay with timeout, conflict warnings. Opened from **Edit > Editor Hotkeys...** or the Preferences module. |
| `Preferences/EditorHotkeys/EditorHotkeysModule.h/.cpp` | `PreferencesModule` subclass that persists the binding map through `LoadSettings`/`SaveSettings`. Registered in `PreferencesManager::Create()`. |

### Query API contract

```cpp
EditorHotkeyMap* h = EditorHotkeyMap::Get();
if (h->IsActionJustTriggered(EditorAction::Debug_Wireframe))  { ... }   // one-shot, menus/toggles
if (h->IsActionDown(EditorAction::Camera_Forward))            { ... }   // continuous, camera movement
if (h->IsActionJustReleased(EditorAction::Some_Action))       { ... }   // release-edge
h->ConsumeBindingKey(EditorAction::File_SaveScene);                     // explicit consume helper
```

`IsActionJustTriggered` does **not** auto-consume the key — instead the gate below stops the editor from reading at all during captured PIE. Some callers (Ctrl+S, Ctrl+P) call `ConsumeBindingKey` after handling to mirror the legacy "clear stuck modifier" behavior.

### PIE hand-off rule

`EditorHotkeyMap::AreEditorHotkeysActive()` is `true` iff one of:
1. Not in PIE (`!mPlayInEditor`)
2. PIE running but ejected (`mEjected` — toggled by F8)
3. PIE running but the cursor is not captured by the game preview (`!mGamePreviewCaptured` — toggled by Ctrl+Alt+P or by clicking outside the Game Preview window)

When the gate is `false` every editor hotkey query returns `false`, so the editor goes silent and game code in PIE owns the keyboard. Clicking back into the Game Preview re-captures the cursor (`GamePreview.cpp:604-609`) which closes the gate again.

### Hardcoded exceptions (NOT routed through `EditorAction`)

- **PIE safety keys:** Escape / F8 / Alt+P / F10 / Ctrl+Alt+P inside `InputManager.cpp::UpdateHotkeys()`. These are always-on safety controls during PIE and must work even if the user has rebound everything else.
- **Transform axis-lock:** X / Y / Z while inside an active rotate/scale mode at `Viewport3d.cpp::HandleAxisLocking()`. These are semantic — pressing X means lock to the X axis, that's not a "hotkey".
- **Modal Enter/Escape:** Confirm/cancel keys inside ImGui dialogs (`EditorImgui.cpp:1308`, `1418`, `1499`) are dialog UI, not editor hotkeys.

### Preset storage and sharing

JSON presets live in:

- **Windows:** `%APPDATA%/PolyphaseEditor/HotkeyPresets/*.json`
- **Linux:** `~/.config/PolyphaseEditor/HotkeyPresets/*.json`

The preset bar in the remap window supports save / load / delete plus import-from-file and export-to-file (any path) and an "Open Presets Folder" button for sharing presets between users.

Bindings serialize by **symbolic key name** (`"Z"`, `"Numpad 5"`, `"F11"`) — not by numeric `POLYPHASE_KEY_*` value — because the codes differ across Windows/Linux/Android/3DS. This makes presets fully portable.

## Packaging System

**Files:** `Packaging/PackagingWindow.h/.cpp`, `PackagingSettings.h`, `BuildProfile.h`

Multi-platform build system with build profiles. Supports Docker for console builds. Targets: Windows, Linux, GameCube, Wii, 3DS. Features: asset embedding, emulator launching, 3dslink support.

## ImGuizmo Integration

**File:** `ImGuizmo/ImGuizmo.cpp`

3D/2D transform gizmos: Translate, Rotate, Scale in World/Local space. Configured via `EditorState::mGizmoOperation` and `mGizmoMode`.

## Key Constants

```cpp
// EditorConstants.h
DRAGDROP_NODE "DND_NODE"
DRAGDROP_ASSET "DND_ASSET"
BASIC_STATIC_MESH "Static Mesh"
BASIC_BOX "Box"
// etc.

// EditorTypes.h
enum class ControlMode { Default, Pilot, Translate, Rotate, Scale, Pan, Orbit };
enum class EditorMode { Scene, Scene2D, Scene3D };
enum class PaintMode { None, Color, Instance };
```

## Editor Subdirectories

| Directory | Contents |
|-----------|----------|
| `Addons/` | AddonManager, AddonCreator, NativeAddonManager, AddonsWindow |
| `DebugLog/` | DebugLogWindow (bottom panel log viewer) |
| `GamePreview/` | Device resolution preview |
| `ImGuizmo/` | Transform gizmo library |
| `NodeGraph/` | Node graph editor panel |
| `Packaging/` | Build profiles and deployment |
| `Preferences/` | Settings hierarchy (General, Appearance, External, Packaging) |
| `ProjectSelect/` | Project creation and selection |
| `ScriptCreator/` | Lua/C++ script creation dialogs |
| `ScriptEditor/` | Zep-based code editor |
| `SecondScreenPreview/` | Second display preview |
| `ThemeEditor/` | Theme editor with CSS export |
| `Timeline/` | Timeline animation editor |
