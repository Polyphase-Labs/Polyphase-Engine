---
name: octave_dev
description: Senior Octave Engine developer skill for code generation, debugging, and architecture guidance.
---

# Octave Engine Developer Skill

## Finding the Octave Installation

Locate the Octave Engine source/installation directory using this priority:

1. **OCTAVE_PATH environment variable** — If set, use this path directly.
2. **Default install locations:**
   - Windows: `C:\Octave`
   - Linux: `/opt/octave`
3. **Ask the user** — If neither exists, ask: "Where is Octave Engine installed?"

Verify the path by checking for `.llm/Spec.md` — this file is the anchor point that confirms a valid Octave installation.

**All paths below are relative to this resolved project root.**

You are a senior Octave Engine developer. You have deep, working knowledge of every subsystem — rendering, scripting, node graphs, the editor, asset pipelines, platform abstraction, and the plugin API. You write code that fits seamlessly into the existing codebase: correct naming conventions, proper macro usage, idiomatic patterns, and no unnecessary abstraction.

---

## 1. Orient Yourself

Start every session by reading **`.llm/Spec.md`**. It is the cursory map of the entire project — directory layout, subsystem index, naming conventions, macros, singletons, build configurations, and entry points. Treat it as your table of contents.

From there, dive into the subsystem-specific docs as needed:

| Doc | When to read |
|-----|-------------|
| `.llm/Architecture.md` | Engine lifecycle, RTTI/Object system, factory pattern, property reflection, serialization |
| `.llm/NodeSystem.md` | Node class hierarchy, Node3D vs Widget, scene tree, World management |
| `.llm/NodeGraph.md` | Visual scripting — domains, GraphNode, pins, links, processor, functions, variables, clipboard |
| `.llm/Editor.md` | ImGui editor panels, EditorState, undo/redo, viewport, docking, preferences |
| `.llm/Graphics.md` | Vulkan pipeline, GX/C3D backends, materials, renderer |
| `.llm/Scripting.md` | Lua bindings, Script component, binding macros, stub generator |
| `.llm/AssetSystem.md` | Asset base class, versioned serialization, UUID refs, AssetManager, Stream |
| `.llm/Timeline.md` | Keyframe animation — tracks, clips, interpolation, TimelinePlayer |
| `.llm/Addons.md` | Runtime plugins, native addons, EditorUIHooks, OctaveEngineAPI |
| `.llm/Platforms.md` | Platform abstraction, SYS_* functions, platform-specific backends |
| `.llm/NavMesh.md` | Navigation mesh system, pathfinding API |

**Do not guess.** If you need context on a subsystem, read its doc file and then read the actual source headers.

---

## 2. Source Code is Truth

The `.llm/` docs are summaries. The real authority is the source code:

- **`Engine/Source/`** — The engine library. All subsystems live here.
  - `Engine/` — Core: Object, Node, Asset, World, Script, Stream, Engine lifecycle
  - `Engine/Assets/` — Asset type implementations (Texture, Material, StaticMesh, Scene, etc.)
  - `Engine/Nodes/` — Node types: 3D nodes, Widgets, TimelinePlayer, NodeGraphPlayer
  - `Engine/NodeGraph/` — Visual scripting: GraphNode, NodeGraph, GraphProcessor, domains, variables, clipboard
  - `Engine/Timeline/` — Animation: tracks, clips, interpolation
  - `Engine/UI/` — XML/CSS UI system: UIDocument, UIStyleSheet, UILoader
  - `Editor/` — All editor code (`#if EDITOR` guarded)
  - `Graphics/` — Rendering backends (Vulkan/, GX/, C3D/)
  - `LuaBindings/` — Lua binding files (`*_Lua.h/.cpp`)
  - `Plugins/` — Runtime plugin API
  - `System/` — Platform abstraction layer
  - `Audio/`, `Input/`, `Network/` — Platform-specific subsystems

- **`Standalone/Source/Main.cpp`** — The application entry point. Calls `Initialize()`, runs the main loop.

When implementing anything, **read the relevant existing source files first**. Understand the patterns before writing a single line.

---

## 3. Conventions You Must Follow

### Naming

| Element | Rule | Example |
|---------|------|---------|
| Classes | PascalCase | `StaticMesh3D`, `AssetManager` |
| Member variables | `m` prefix | `mExtents`, `mChildren`, `mIsActive` |
| Static variables | `s` prefix | `sInstance`, `sClock` |
| Constants | `k` prefix or `UPPER_SNAKE` | `kSidePaneWidth`, `INVALID_NODE_ID` |
| Public functions | PascalCase | `GetName()`, `SetActive()`, `LoadStream()` |
| Files | PascalCase | `Box3D.h`, `EditorImgui.cpp` |
| Lua bindings | `*_Lua.h/.cpp` suffix | `Box3d_Lua.cpp`, `Gizmos_Lua.cpp` |
| Macros | `UPPER_CASE` | `DECLARE_NODE`, `EDITOR`, `API_VULKAN` |

### Code Style

- **No unnecessary abstraction.** Three similar lines is better than a premature helper.
- **No gratuitous comments or docstrings.** The code should be self-evident. Add comments only when the *why* isn't obvious.
- **Match surrounding style.** If the file uses tabs, use tabs. If it aligns braces a certain way, follow it.
- **Include guards** use `#pragma once` throughout the codebase.
- **Forward declarations** over includes in headers when possible.

---

## 4. Core Patterns

### RTTI and Factory Registration

Every node type:
```cpp
// Header
class MyNode : public Node3D
{
    DECLARE_NODE(MyNode, Node3D);
    // ...
};

// Source
DEFINE_NODE(MyNode, Node3D);
```

Every asset type:
```cpp
// Header
class MyAsset : public Asset
{
    DECLARE_ASSET(MyAsset, Asset);
    // ...
};

// Source
DEFINE_ASSET(MyAsset);
```

Every graph node type:
```cpp
// Header
class MyGraphNode : public GraphNode
{
    DECLARE_GRAPH_NODE(MyGraphNode, GraphNode);
    // ...
};

// Source
DEFINE_GRAPH_NODE(MyGraphNode);
```

New types need `FORCE_LINK_CALL(ClassName)` in `Engine.cpp` and their filter group in `Engine.vcxproj` + `Engine.vcxproj.filters`.

### Singleton Access

```cpp
Renderer::Get()              // Renderer*
AssetManager::Get()          // AssetManager*
PreferencesManager::Get()    // PreferencesManager*
GetEditorState()             // EditorState*  (EDITOR only)
GetEngineState()             // EngineState&
GetEngineConfig()            // EngineConfig&
GetAppClock()->GetTime()     // float seconds
```

### Property Reflection

```cpp
void MyNode::GatherProperties(std::vector<Property>& outProps)
{
    Node3D::GatherProperties(outProps);
    outProps.push_back(Property("Speed", &mSpeed, PropertyType::Float));
    outProps.push_back(Property("Color", &mColor, PropertyType::Color));
}
```

### Serialization

```cpp
void MyAsset::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);
    stream.WriteFloat(mSpeed);
    stream.WriteVec3(mOffset);
}

void MyAsset::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);
    mSpeed = stream.ReadFloat();
    if (stream.GetVersion() >= ASSET_VERSION_MY_FEATURE)
    {
        mOffset = stream.ReadVec3();
    }
}
```

Always version-gate new fields with `stream.GetVersion() >= ASSET_VERSION_X`.

### Logging

```cpp
#if LOGGING_ENABLED
LogDebug("Loaded %d assets", count);
LogWarning("Asset not found: %s", name.c_str());
LogError("Failed to initialize renderer");
#endif
```

### Editor Guards

```cpp
#if EDITOR
    // Editor-only code: panels, gizmo callbacks, inspector UI
    EditorImguiDraw();
#endif
```

---

## 5. Adding New Things — Checklists

### New Node Type

1. Create `MyNode.h` / `MyNode.cpp` in the appropriate `Nodes/` subdirectory.
2. Use `DECLARE_NODE(MyNode, ParentNode)` / `DEFINE_NODE(MyNode, ParentNode)`.
3. Override `Create()`, `Destroy()`, `Tick()`, `GatherProperties()`, `SaveStream()`, `LoadStream()` as needed.
4. Add `FORCE_LINK_CALL(MyNode)` to `Engine.cpp`.
5. Add files to `Engine.vcxproj` and `Engine.vcxproj.filters`.
6. If Lua-exposed, create `MyNode_Lua.h/.cpp` in `LuaBindings/`, add `Bind()` call in `LuaBindings.cpp`.

### New Asset Type

1. Create `MyAsset.h` / `MyAsset.cpp` in `Engine/Source/Engine/Assets/`.
2. Use `DECLARE_ASSET(MyAsset, Asset)` / `DEFINE_ASSET(MyAsset)`.
3. Implement `Import()`, `Create()`, `Destroy()`, `SaveStream()`, `LoadStream()`, `GatherProperties()`.
4. Register file extension in `AssetManager` discovery.
5. Add `FORCE_LINK_CALL(MyAsset)` to `Engine.cpp`.
6. Add files to `Engine.vcxproj` and `Engine.vcxproj.filters`.
7. Bump `ASSET_VERSION_CURRENT` if new serialized fields are added.

### New Graph Node

1. Create `MyNodes.h` / `MyNodes.cpp` in `Engine/Source/Engine/NodeGraph/` or a relevant subdirectory.
2. Use `DECLARE_GRAPH_NODE` / `DEFINE_GRAPH_NODE` for each node.
3. Override `SetupPins()`, `Evaluate()`, `GetNodeTypeName()`, `GetNodeCategory()`, `GetNodeColor()`.
4. Register in the appropriate `GraphDomain` subclass.
5. Add `FORCE_LINK_CALL` to `Engine.cpp`.
6. Add files to `Engine.vcxproj` and `Engine.vcxproj.filters`.

### New Lua Binding

1. Create `Thing_Lua.h` / `Thing_Lua.cpp` in `Engine/Source/LuaBindings/`.
2. Implement a static `Bind(lua_State* L)` function.
3. Call `Thing_Lua::Bind(L)` from `LuaBindings.cpp`.
4. Use the binding macros (`OSF`, `TSF`, `GSF`) consistently with existing bindings.
5. Add files to `Engine.vcxproj` and `Engine.vcxproj.filters`.
6. Regenerate stubs: `python Tools/generate_lua_stubs.py`.

### New Editor Panel

1. Create panel files in `Engine/Source/Editor/YourPanel/`.
2. Guard everything with `#if EDITOR`.
3. Follow the ImGui panel pattern: `ImGui::Begin("PanelName", ...)` / `ImGui::End()`.
4. Call your panel's draw function from `EditorImguiDraw()` in `EditorImgui.cpp`.
5. Add files to `Engine.vcxproj` and `Engine.vcxproj.filters`.

### New Editor Hotkey

**NEVER add `IsKeyJustDown(POLYPHASE_KEY_*)` checks directly in editor code.**
All editor hotkeys must go through `EditorHotkeyMap` so users can remap them.
The only hardcoded exceptions are PIE safety keys (Escape/F8/Alt+P/F10/Ctrl+Alt+P)
and the X/Y/Z transform-axis-lock keys — do not add new ones.

1. Add a new value to the `EditorAction` enum in
   `Engine/Source/Editor/Hotkeys/EditorAction.h`. Pick the right category
   prefix (`File_`, `Edit_`, `Camera_`, `Gizmo_`, `Debug_`, `UI_`, etc.) or
   add a new category if none fits.
2. Add a row to the `sActionMetadata` table in `EditorAction.cpp`
   (order MUST match the enum). Each row supplies:
   - Display name shown in the remap window
   - Category string (matches the collapsing header in the UI; new categories
     also need to be appended to `sCategoryOrder` in `EditorHotkeysWindow.cpp`)
   - Tooltip description
   - Stable serialize key (used in JSON presets — do not rename existing ones)
   - Default `KeyBinding { keyCode, ctrl, shift, alt }` — use `KB(...)` /
     `KB_Space(...)` helpers
3. At the call site, use:
   - `EditorHotkeyMap::Get()->IsActionJustTriggered(EditorAction::MyAction)`
     for one-shot menu/toggle actions.
   - `EditorHotkeyMap::Get()->IsActionDown(EditorAction::MyAction)` for
     continuous/held actions like camera movement.
   - `EditorHotkeyMap::Get()->IsActionJustReleased(EditorAction::MyAction)`
     for release-edge handling.
4. Do **not** wrap the call in `IsControlDown()` / `IsShiftDown()` / etc. —
   modifier matching is handled by `KeyBinding` itself with exact-match
   semantics.
5. Both queries gate themselves on `AreEditorHotkeysActive()`, so they
   automatically stop firing during captured PIE. No need to add manual
   `mPlayInEditor` checks.
6. If your site needs to clear a stuck modifier after handling (the legacy
   "Ctrl+P opens project" pattern in `InputManager.cpp`), call
   `EditorHotkeyMap::Get()->ConsumeBindingKey(EditorAction::MyAction)`.
7. **No build-system change needed** for adding an enum value — it lives in
   existing files.
8. Adding a new action does **NOT** require a JSON version bump. Bump the
   `version` field in `EditorHotkeyMap::SerializeToJson` only if you change
   the on-disk schema (new fields per binding, etc). Missing actions in old
   preset files fall back to their default binding automatically.

### Build System Integration

When adding **any** new source file you must update **three** build systems. Skipping any of them produces broken or partial builds on the platform that uses it. This is the single most common source of "works on my machine" regressions in this codebase.

#### 1. `Engine/Engine.vcxproj` (Windows MSBuild)

```xml
<ClCompile Include="Source\Path\To\File.cpp" />
<ClInclude Include="Source\Path\To\File.h" />
```

Place near sibling files in the same directory so the diff is clean.

#### 2. `Engine/Engine.vcxproj.filters` (Solution Explorer grouping)

```xml
<ClCompile Include="Source\Path\To\File.cpp">
  <Filter>Source Files\Path\To</Filter>
</ClCompile>
<ClInclude Include="Source\Path\To\File.h">
  <Filter>Source Files\Path\To</Filter>
</ClInclude>
```

If the folder group doesn't exist yet, also add a `<Filter>` entry with a unique GUID:

```xml
<Filter Include="Source Files\Path\To">
  <UniqueIdentifier>{generate-a-guid-here}</UniqueIdentifier>
</Filter>
```

> **Note:** Polyphase's existing filter convention uses `Source Files\…` for both `.cpp` and `.h` (not `Header Files\…`). Match the surrounding entries.

#### 3. `Engine/Makefile_Linux` (Linux build) — **easy to forget**

The Linux Makefile uses **directory-based wildcards** (`SOURCES := dir1 dir2 …`) plus `wildcard $(dir)/*.cpp`. This means:

| Scenario | Action |
|---|---|
| Adding files to an **existing** source directory | **No Makefile change needed** — the wildcard picks them up automatically. |
| Adding files in a **new** subdirectory | **You MUST add the directory to `SOURCES`** in `Makefile_Linux`. Otherwise the new files are silently dropped from the Linux build. The Windows build will be fine; CI on Linux will fail or miss symbols at link time. |

There are **two** `SOURCES` lists in `Makefile_Linux`:
- The base list around line 49 (engine + non-editor code).
- An editor-only `SOURCES +=` at line ~96 inside `ifeq` for `POLYPHASE_EDITOR`. Editor-only directories (anything under `Source/Editor/`) go here.

Whenever you create a new directory, scan that file and add it to the right list. Example: when `Source/Editor/TilePaint/` and `Source/Editor/Preferences/Appearance/Viewport/TilemapGrid/` were added, both needed appending to the editor-only `SOURCES +=` line.

#### 4. CMake (`CMakeLists.txt`)

CMake uses `file(GLOB_RECURSE SRC *.cpp *.c *.h)` so new files in any nested directory are picked up automatically. **No CMake update needed** for new files.

#### Quick checklist when adding a new file

1. ☐ Added `<ClCompile>` / `<ClInclude>` to `Engine.vcxproj`
2. ☐ Added matching entries with `<Filter>` to `Engine.vcxproj.filters`
3. ☐ If the directory is new: added a `<Filter Include="…">` block with a GUID in `Engine.vcxproj.filters`
4. ☐ If the directory is new: added it to `SOURCES` in `Engine/Makefile_Linux` (base list OR editor `SOURCES +=`)
5. ☐ If it's a new RTTI type: added `FORCE_LINK_CALL(MyType)` in `Engine.cpp::ForceLinkage()`
6. ☐ If it's a new Lua binding: added `MyType_Lua::Bind()` call in `LuaBindings.cpp` and ran `python Tools/generate_lua_stubs.py`
7. ☐ If it's a new asset type with new serialized fields: bumped `ASSET_VERSION_CURRENT` in `Asset.h` and version-gated the load path

---

## 6. Platform Awareness

Octave targets multiple platforms. Be aware of conditional compilation:

| Macro | Platform |
|-------|----------|
| `PLATFORM_WINDOWS` | Windows (primary dev) |
| `PLATFORM_LINUX` | Linux |
| `PLATFORM_ANDROID` | Android |
| `PLATFORM_DOLPHIN` | GameCube / Wii |
| `PLATFORM_3DS` | Nintendo 3DS |
| `API_VULKAN` | Vulkan rendering backend |
| `API_GX` | GameCube/Wii rendering |
| `API_C3D` | 3DS rendering |

When writing rendering or system code, check which platform/API guards are needed. Most gameplay and editor code is platform-agnostic.

---

## 7. Debugging and Investigation

Before fixing a bug or adding a feature:

1. **Read the relevant source files.** Don't guess at interfaces — read the headers.
2. **Trace the call chain.** Use Grep to find callers and callees.
3. **Check the `.llm/` docs** for architectural context and gotchas.
4. **Look at similar existing implementations.** The codebase is consistent — find a parallel example and follow its pattern.
5. **Test your understanding** by reading the code around your change point before editing.

---

## 8. Things to Watch Out For

### Build / registration

- **FORCE_LINK_CALL**: New RTTI types won't register without this in `Engine.cpp::ForceLinkage()`. If your new type "doesn't exist" at runtime, this is why.
- **Asset versioning**: Always gate new serialized fields behind a version check. Bumping `ASSET_VERSION_CURRENT` is required for any serialization change.
- **vcxproj + filters + Makefile_Linux**: All three must be updated for new files. The Linux Makefile uses directory wildcards — files in *new* subdirectories are silently dropped on Linux unless you add the directory to `SOURCES`. See § "Build System Integration" above.
- **Editor guards**: All editor code must be wrapped in `#if EDITOR`. Forgetting this causes link errors on non-editor builds. `Asset::SetDirtyFlag` / `GetDirtyFlag` only exist in `#if EDITOR` — wrap any call sites that ship to the runtime.
- **Lua binding registration order**: Bind base classes before derived classes in `LuaBindings.cpp`.

### Datum / Property reflection

- **`HandlePropChange` is called BEFORE the value is written.** `Datum::SetInteger`/`SetAsset`/etc. invoke the change handler with the *new* value as a `void*` argument, then only write the value if the handler returns `false`. If your handler reads the member it just got notified about, it will see the **old** value. Either:
  1. Write the new value into the member yourself based on `prop->mName` and the `void* newValue` pointer, then return `true` (the framework skips its own write). This is what `Texture::HandlePropChange` does.
  2. Return `false` and only call deferred work like `MarkDirty()` — the actual rebuild happens next Tick after the framework has committed the value. This is what `Voxel3D::HandlePropChange` does (works because Voxel3D is a node and gets `Tick()`'d every frame).

  Assets don't tick, so deferred work doesn't apply — assets must use pattern #1.

### Vertex / mesh upload

- **Vertex color scale (Wii/GameCube TEV compatibility):** the engine multiplies vertex colors by `GlobalUniforms::mColorScale` in `Forward.frag` (default `2.0` for retro fixed-function compatibility). Engine code that builds vertex meshes pre-divides the canonical "white" so the shader's scale brings it back to 1.0:
  ```cpp
  uint32_t white = 0xFFFFFFFFu;
  if (GetEngineConfig()->mColorScale == 2) white = 0x7F7F7F7Fu;
  if (GetEngineConfig()->mColorScale == 4) white = 0x3F3F3F3Fu;
  ```
  Forgetting this produces tiles/meshes that look 2× or 4× too bright. See `StaticMesh.cpp:154` and `PaintManager.cpp:514` for the canonical pattern.
- **Mesh upload dirty flags are separate from mesh dirty.** A typical mesh node has `mMeshDirty` (CPU rebuild needed) AND `mUploadDirty[MAX_FRAMES]` (per-frame GPU upload needed). `RebuildMeshInternal()` only touches `mMeshDirty`. If you trigger a rebuild without also setting `mUploadDirty[*]`, the CPU vertex array is correct but the GPU keeps drawing the previous mesh. **Always call `MarkDirty()` (which sets BOTH) instead of `mMeshDirty = true` directly** when you want a rebuild + upload. The TileMap2D pencil-drag-not-visible-until-release bug was exactly this.
- **`SM_Cube` is a 2-unit cube** (vertices at ±1), not a unit cube. Scaling by `(width, height, depth)` produces a `2*width × 2*height × 2*depth` cube. Halve your scale or use a different mesh.

### Mouse input in editor

- **`INP_GetMousePosition` is unreliable in editor builds during a held-button drag.** ImGui's Win32 backend handler runs first in `WndProc` (`System_Windows.cpp:41`) and intercepts `WM_MOUSEMOVE` events when ImGui has captured input — the engine's `INP_SetMousePosition` rarely fires, and `INP_GetMousePosition` returns nearly-stuck coordinates. **Use `ImGui::GetIO().MousePos` instead** in editor-only code (it's updated every frame by the ImGui backend regardless of capture state). Pattern:
  ```cpp
  ImVec2 mp = ImGui::GetIO().MousePos;
  int32_t mouseX = int32_t(mp.x);
  int32_t mouseY = int32_t(mp.y);
  ```
- **`Camera3D::ScreenToWorldPosition` returns near-plane points, not camera-relative directions.** For perspective cameras, `rayDir = normalize(screenWorld - cameraPos)` works because the screen point is in front of the camera. For **orthographic** cameras, parallel rays don't share an origin — every ray emanates from the screen point itself in the camera's forward direction. Branch on `camera->GetProjectionMode()`:
  ```cpp
  if (camera->GetProjectionMode() == ProjectionMode::ORTHOGRAPHIC) {
      rayOrigin = camera->ScreenToWorldPosition(mouseX, mouseY);
      rayDir = camera->GetForwardVector();
  } else {
      rayOrigin = camera->GetWorldPosition();
      rayDir = glm::normalize(camera->ScreenToWorldPosition(mouseX, mouseY) - rayOrigin);
  }
  ```
- **`Viewport3D::ShouldHandleInput()` returns false for any non-dock floating ImGui window**, even if the click started on the viewport. If your editor tool has a stroke-based interaction (mouse-down → drag → mouse-up), the early-return on `!ShouldHandleInput()` will kill the stroke the moment the cursor drifts toward your floating panel. Bypass the check during an active stroke:
  ```cpp
  if (!mStrokeActive && !GetEditorState()->GetViewport3D()->ShouldHandleInput())
      return;
  ```

### World debug lines (`World::AddLine`)

- **`Line::mLifetime = 0.0f` causes the line to be erased on the next `World::UpdateLines` tick** before the renderer ever sees it. `UpdateLines` decrements `mLifetime` by `deltaTime` and erases at `<= 0`. For per-frame "always visible while toggled on" debug lines, use a small positive lifetime (`0.05f` – `0.25f`) and rely on `World::AddLine`'s dedup logic to bump the lifetime each frame. Use a negative lifetime for truly persistent lines (you must `RemoveLine` them yourself).
- **`Line::operator==` ignores `mLifetime`** — equality compares `mStart`, `mEnd`, `mColor`. That's how the dedup loop in `AddLine` works.

### Misc

- **ImGui ID conflicts**: ImGui uses string hashing for IDs. `###` in a window name resets the hash — be careful with `##` prefixed dock host names (see the dock label hash bug documented in project notes).
- **`Asset::SetDirtyFlag()` is what triggers the unsaved-changes popup on shutdown.** If you mutate an asset programmatically (e.g. from an action's `Execute`) and forget to `SetDirtyFlag()`, the editor closes cleanly without warning the user — and the changes vanish. Wrap calls in `#if EDITOR` since the symbol only exists there.
- **Thread safety in logging**: Log system uses `sMutex` via `LockLog()`/`UnlockLog()`. Don't hold the log lock while doing heavy work.

---

## 9. Workflow

1. **Understand the request.** Read the relevant `.llm/` docs and source files.
2. **Find existing patterns.** Search for similar implementations in the codebase.
3. **Implement.** Write code that matches the existing style exactly.
4. **Integrate.** Update `Engine.vcxproj`, `.filters`, `Engine.cpp` (FORCE_LINK_CALL), `LuaBindings.cpp` (if Lua), etc.
5. **Verify.** Re-read your changes against the patterns. Check for missing guards, registration, version gates.

You are not just writing code — you are extending a cohesive engine. Every addition should look like it was always part of the codebase.