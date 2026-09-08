---
name: polyphase-addon
description: Build and ship full-fledged native C++ addons for the Polyphase Engine — DLL/SO packages discovered under `<Project>/Packages/`, hot-reloaded by the editor, and statically compiled into shipped builds. Use this when the user wants to create a Polyphase native plugin, scaffold a `package.json` with a `native` block, register custom Node / Asset / GraphNode types from an addon, expose Lua bindings or REST routes from a plugin, attach editor UI hooks (menus, panels, inspectors, importers), or wire third-party libraries with per-platform overrides. Triggers on requests like "create a Polyphase addon", "add a native plugin called …", "register a custom node from a plugin", "make my addon hot-reload safely", "add an external library to my addon", or "ship a video / audio / network format addon".
---

# Polyphase Native Addon Skill

Author Polyphase native addons — self-contained C++ packages discovered under
`<Project>/Packages/<reverse-dns-id>/`, built per-platform from a `package.json`
manifest, loaded by the editor's `NativeAddonManager` with hot-reload, and
statically linked into shipped game/console builds.

This skill is the **agent-facing playbook**. The full developer guide already
exists and is the source of truth for everything below; this skill covers the
mental model, recipe pointers, and the gotchas that aren't called out in the
guide.

| Resource                                                                                  | What it covers                                                                   |
| ----------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| `Documentation/Development/NativeAddon/NativeAddon.md`                                    | Full dev guide — manifest schema, lifecycle, editor UI hooks, hot-reload rules.  |
| `Documentation/Development/NativeAddon/Examples/`                                         | Focused recipes (custom menu, debug window, inspector, context menu, rotator…). |
| `Documentation/Development/StaticContent.md`                                              | Static Content / Content Pak — what obfuscation means for addon file I/O.       |
| `.llm/Addons.md`                                                                          | Architecture overview.                                                           |
| `Engine/Source/Plugins/PolyphasePluginAPI.h`                                              | `PolyphasePluginDesc`, `OCTAVE_PLUGIN_API`, `POLYPHASE_PLUGIN_API_VERSION`.      |
| `Engine/Source/Plugins/PolyphaseEngineAPI.h`                                              | Engine API surface passed to `OnLoad` (logging, Lua, world, nodes, assets…).    |
| `Engine/Source/Plugins/EditorUIHooks.h`                                                   | Every editor extension hook (menus, windows, inspectors, importers, events).    |
| `Engine/Source/Editor/Addons/NativeAddonManager.cpp`                                      | Discovery, manifest parse, fingerprint, async build, lifecycle.                 |
| `M:\Projects\Polyphase\Addons\VideoPlayer\testing\GC\VideoPlayer-Demo-GC\Packages\com.polyphase.formats.video\` | Advanced exemplar — custom Node + Asset + Lua + External libs + per-platform builds. |

When the source disagrees with this skill, the **source wins**. Read the header
for the canonical API version (`POLYPHASE_PLUGIN_API_VERSION`) and the
descriptor signature.

## When to use

Reach for this skill when the user wants to **extend the engine from outside**
the engine source tree — i.e. ship a self-contained, hot-reloadable, optionally
cross-platform package that:

- adds reusable Node, Asset, or GraphNode types,
- exposes new Lua functions to project scripts,
- adds editor UI (menus, panels, inspectors, asset importers, viewport overlays),
- bundles third-party libraries with per-platform overrides, or
- registers custom REST routes on the controller server.

If the user is editing **engine source** (`Engine/Source/...`), use the
`polyphase` skill instead. If they want to drive an already-running editor over
HTTP, use `polyphase-controller`. If they're authoring a UI widget *type* in
the engine, use `polyphase-widget`.

## Mental model — six things every recipe relies on

1. **Discovery.** The editor scans `<Project>/Packages/*/package.json` for
   entries with a `"native"` block. Reverse-DNS ids
   (`com.example.myaddon`) are the convention.

2. **Fingerprint.** Source mtimes/sizes plus a hash of the manifest's `native.*`
   block produce a fingerprint; the build cache lives at
   `Intermediate/Plugins/<id>/<fingerprint>/`. Edit any source file, or any
   field under `native` / `nativePerPlatform`, to invalidate it. The fingerprint
   already encodes the host CRT, so Debug-vs-Release rebuilds are correct.

3. **Lifecycle order.**

   `OnLoad(api)` → `RegisterTypes(nodeFactory)` → `RegisterScriptFuncs(L)` →
   `RegisterEditorUI(hooks, hookId)` → per-frame `Tick(dt)` (gameplay) and
   `TickEditor(dt)` (editor) → `OnUnload()`.

   `RegisterScriptFuncs` takes `lua_State*` directly, not `void*`.

4. **Static-init registration.** `DECLARE_NODE` / `DECLARE_ASSET` /
   `DECLARE_GRAPH_NODE` register their type with the engine factory via static
   initializers that run on DLL load. **You must put `FORCE_LINK_CALL(MyType)`
   inside `OnLoad`** — without it, the linker may drop the translation unit
   because nothing in the addon references its symbols.

5. **Hot-reload safety — what the engine handles vs what you handle.**

   The engine handles, in order, before `FreeLibrary`:
   - `OnUnload()` is called.
   - `EditorUIHooks::RemoveAllHooks(hookId)` removes every hook you registered
     with the `hookId` you were given.
   - Asset instances belonging to the unloading module are purged (their UUIDs
     are stashed and reloaded after the rebuild).
   - Factory pointers from the module are stripped.

   **You are responsible for**, inside `OnUnload`:
   - Releasing every `AssetRef` and `ScriptFunc` (Lua-ref-backed callable) held
     in addon-side singletons or globals.
   - Joining or stopping any worker threads the addon spawned.
   - Clearing addon-owned event-dispatcher tables.
   - Nulling out the cached `PolyphaseEngineAPI*` *last*.

   The VideoPlayer addon's `OnUnload` (`Source/VideoPlayer.cpp`) is the
   canonical reference — `PlaylistRegistry::Get().Clear()` and
   `PlaylistEventDispatcher::Get().Clear()` before nulling the API pointer.

6. **Two entry-point names.** The same `FillDesc` body is exported under two
   different symbols depending on build mode:

   ```cpp
   #if EDITOR
   extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc) {
       return FillDesc(desc);
   }
   #else
   // Shipped: each addon exports a uniquely-named symbol (alphanumerics + underscore;
   // dots in the addon id become underscores) so the editor's auto-generated
   // `Generated/AddonPlugins.cpp` can `extern "C"`-declare and POLYPHASE_REGISTER_PLUGIN
   // them without symbol collisions.
   extern "C" int PolyphasePlugin_GetDesc_com_example_myaddon(PolyphasePluginDesc* desc) {
       return FillDesc(desc);
   }
   #endif
   ```

   You do **not** write `POLYPHASE_REGISTER_PLUGIN(...)` yourself — the editor
   emits it via **Tools → Addons → Regenerate Native Addon Dependencies**
   (implementation in `Engine/Source/Editor/ActionManager.cpp` around line
   233/872). Older docs that show authors writing the macro by hand are stale.

7. **Menu paths use `/` for nesting** — *for real now.* `AddMenuItem`,
   `AddMenuItemEx`, and the new singular `AddCreateAssetItem` all parse `/`
   in their `itemPath` into nested `ImGui::BeginMenu` calls; sibling entries
   with a shared prefix collapse under one parent submenu (in registration
   order). Before this engine revision the slash was a literal character; if
   you're targeting older editor builds, the addon source doesn't need to
   change — older engines render the slash literally, newer ones nest.

8. **Asset browser "Create Asset" menu** — two complementary hooks. Prefer
   the declarative singular `EditorUIHooks::AddCreateAssetItem(hookId,
   "MyAddon/MyType", callback, userData)` for fresh-empty-asset entries:
   pair the callback with `AssetManager::Get()->CreateAndRegisterAsset(
   type, GetCurrentAssetDir(), name, false)` and the asset appears in the
   user's current folder on the next frame — no manual Refresh. Fall back to
   the older `AddCreateAssetItems` (callback form) if you need full ImGui
   control or compatibility with older engine binaries (the new singular
   pointer is at the end of `EditorUIHooks` and lives as `nullptr` on older
   builds — always null-check). For *imported-from-source* asset types
   (`.dialogue`, `.fbx`, `.mp4`, …) keep using `RegisterImportExtension` —
   the two paths compose.

## Reading files from an addon — Static / Content Pak builds

Shipped packages can be built with **Static Content** (cooked assets, scripts and
the asset registry are obfuscated) and **Content Pak** (all of it folded into a
single `Content.pak`, loose copies deleted). Full reference:
`Documentation/Development/StaticContent.md`.

What this means for addon code:

- **Use `Stream::ReadFile` for anything that ships as engine content.** It is the
  universal chokepoint — it consults the pak and unwraps the obfuscation
  container, on every platform, including build-target addons. A raw `fopen`
  silently misses both layers and will read encrypted bytes (or nothing at all,
  once the loose file is pruned into a pak).

  ```cpp
  Stream stream;
  if (stream.ReadFile(path.c_str(), true))
  {
      // stream.GetData() / GetSize() are plaintext here regardless of build mode
  }
  ```

  Plain files pass through untouched, so this is correct in Moddable builds and
  in the editor too — there is no mode to branch on.

- **Raw assets keep working as-is.** `.mp4`, `.json`, `.png`, `.rcss` and other
  non-`.oct` files are deliberately **never** obfuscated or packed, precisely
  because addons open them with their own file I/O (FFmpeg, a decoder library, a
  parser). VideoPlayer's `.mp4` handling needs no changes. If you want an
  addon's data protected, ship it as a custom `Asset` type (`.oct`) instead of a
  raw file.

- **Lua from an addon** must go through `Stream::ReadFile` + `luaL_loadbuffer`,
  not `luaL_dofile`. `luaL_dofile` bypasses the decode *and* the embedded-script
  table, so it fails in both Static and Embedded builds.

- **Seekable/chunked reads** (streaming a large file) must use
  `SYS_FileOpenRead` / `SYS_FileRead` / `SYS_FileSeek`, which are pak- and
  container-aware. Offsets you pass to `SYS_FileSeek` are in **decoded** space;
  the wrapper maps them onto the physical file.

Nothing else is required — you don't opt in, and there's no per-mode branching.

## Locating the addon directory

| Where                                             | Purpose                                               |
| ------------------------------------------------- | ----------------------------------------------------- |
| `<Project>/Packages/<reverse-dns-id>/`            | Local-development addon. Lives in the project repo.   |
| Project addon cache                               | Installed addon (sourced from a remote / ZIP).        |

If you can't infer the project root from context, ask the user. The Polyphase
install itself is found via `POLYPHASE_PATH` / `C:\Polyphase` / `/opt/Polyphase`
or a project-local `PolyphaseConfig.cmake` (the same priority as the
`polyphase` skill).

## Quickstart — minimal addon

Use the editor's scaffolder when one is available; fall back to the manual
layout when not.

### Option A — editor scaffolder (preferred)

In the running editor:

1. **Tools → Addons → Create Native Addon…**
2. Fill in id (e.g. `com.example.hello`), display name, target (`engine` for
   gameplay code, `editor` for editor-only tools).
3. The editor writes `Packages/<id>/package.json`, `Source/<Name>.cpp`,
   `.vscode/c_cpp_properties.json`, `CMakeLists.txt`, and (on Windows) a
   `.vcxproj`.
4. Add your custom node / asset / hooks (recipes below).
5. **Tools → Addons → Reload Native Addons** — discovers, builds, and loads.
6. Look for the `OnLoad` log line in the console.

### Option B — manual scaffold

Directory:

```
<Project>/Packages/com.example.hello/
  package.json
  Source/
    HelloAddon.cpp
    HelloNode.h
    HelloNode.cpp
```

`package.json` (minimum viable manifest — read
`Engine/Source/Plugins/PolyphasePluginAPI.h` for the canonical `apiVersion`):

```json
{
    "name": "com.example.hello",
    "author": "Your Name",
    "description": "Demo native addon",
    "version": "0.1.0",
    "native": {
        "target": "engine",
        "sourceDir": "Source",
        "binaryName": "com.example.hello",
        "entrySymbol": "PolyphasePlugin_GetDesc",
        "apiVersion": 3
    }
}
```

`Source/HelloAddon.cpp` — modeled on the dual-entry pattern:

```cpp
#include "Plugins/PolyphasePluginAPI.h"
#include "Plugins/PolyphaseEngineAPI.h"
#include "HelloNode.h"

static PolyphaseEngineAPI* sAPI = nullptr;

static int OnLoad(PolyphaseEngineAPI* api)
{
    sAPI = api;
    FORCE_LINK_CALL(HelloNode);  // keep static init from getting dropped
    if (api && api->LogDebug) api->LogDebug("HelloAddon loaded");
    return 0;
}

static void OnUnload()
{
    if (sAPI && sAPI->LogDebug) sAPI->LogDebug("HelloAddon unloading");
    sAPI = nullptr;
}

static void RegisterTypes(void* /*nodeFactory*/) {}
static void RegisterScriptFuncs(lua_State* /*L*/) {}
#if EDITOR
static void RegisterEditorUI(EditorUIHooks* /*hooks*/, uint64_t /*hookId*/) {}
#endif

static int FillDesc(PolyphasePluginDesc* desc)
{
    desc->apiVersion = OCTAVE_PLUGIN_API_VERSION;
    desc->pluginName = "HelloAddon";
    desc->pluginVersion = "0.1.0";
    desc->OnLoad = OnLoad;
    desc->OnUnload = OnUnload;
    desc->RegisterTypes = RegisterTypes;
    desc->RegisterScriptFuncs = RegisterScriptFuncs;
#if EDITOR
    desc->RegisterEditorUI = RegisterEditorUI;
#else
    desc->RegisterEditorUI = nullptr;
#endif
    desc->OnEditorPreInit = nullptr;
    desc->OnEditorReady = nullptr;
    return 0;
}

#if EDITOR
extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc)
{ return FillDesc(desc); }
#else
extern "C" int PolyphasePlugin_GetDesc_com_example_hello(PolyphasePluginDesc* desc)
{ return FillDesc(desc); }
#endif
```

`Source/HelloNode.h` / `.cpp` follows the standard Node pattern:

```cpp
// HelloNode.h
#pragma once
#include "Nodes/3D/Node3D.h"

class HelloNode : public Node3D
{
public:
    DECLARE_NODE(HelloNode, Node3D);
    virtual void Tick(float deltaTime) override;
};
```

```cpp
// HelloNode.cpp
#include "HelloNode.h"
FORCE_LINK_DEF(HelloNode);
DEFINE_NODE(HelloNode, Node3D);

void HelloNode::Tick(float deltaTime) { Node3D::Tick(deltaTime); }
```

Open the editor, run **Tools → Addons → Reload Native Addons**, confirm the
log line and that `HelloNode` appears in the *Add Node* menu.

## Recipe pointers

For each common task, jump to the canonical reference rather than reproducing
boilerplate.

| Task                                                  | Where to look                                                                                                                                                |
| ----------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Custom Node type                                      | `NativeAddon.md` *Registering Custom Node Types*; `Examples/Rotator3D.md`. Remember `FORCE_LINK_CALL(MyNode)` in `OnLoad`.                                   |
| Custom Asset type with importer                       | `Examples/CustomAssetType.md`. Use `RegisterImportExtension(".ext", MyAsset::GetStaticType())` inside `#if EDITOR` in `OnLoad`. VideoPlayer's `VideoClip` is a fully-worked example. |
| Custom **Create Asset** menu entry (fresh asset, current folder, no Refresh) | `NativeAddon.md` *Asset Browser "Create Asset" Menu*. Use the declarative `EditorUIHooks::AddCreateAssetItem(hookId, "MyAddon/MyType", cb, ud)` and let the callback call `AssetManager::Get()->CreateAndRegisterAsset(type, GetCurrentAssetDir(), name, false)`. Pair `RegisterImportExtension` with this when you also want drag-drop. |
| Custom GraphNode (visual scripting)                   | `Examples/CustomGraphNode.md`. Then the `polyphase` skill's *New Graph Node* checklist for the engine-side patterns.                                         |
| Lua bindings                                          | `NativeAddon.md` *Exposing Lua Functions*; `polyphase-widget` skill for the binding-macro reference.                                                         |
| Editor UI — menus, windows, inspectors, importers     | `NativeAddon.md` *Extending the Editor UI* and *Editor Lifecycle Hooks*; `Examples/Editor/`. The engine handles cleanup via `RemoveAllHooks(hookId)`.        |
| External library + per-platform overrides             | `Examples/ExternalLibrary.md` and `NativeAddon.md` *Per-Platform Build Configuration*. VideoPlayer's FFmpeg integration is the worked example.               |
| Custom REST routes from an addon                      | `polyphase-controller` skill (server-side). Register routes via `EditorUIHooks::RegisterControllerRoute(...)`.                                               |
| Per-platform native backend inside one addon (camera, audio, input)  | `com.polyphase.formats.webcam` is the worked example: one `IWebcamBackend` interface, one `.cpp` per platform each wrapped in `#if PLATFORM_X && POLYPHASE_WITH_Y`, and a factory TU with an `#if/#elif` chain selecting them (final `#else` = a stub that returns an empty device list). The `POLYPHASE_WITH_Y` define and the platform libs are declared per-platform in `package.json`; `CMakeLists.txt` globs `Source/`, so a new backend file needs no registration. Keep format conversion inside each backend so the shared frame type stays uniform. A silently-stubbed platform usually means the per-platform defines never arrived — see Troubleshooting. |
| Hot-reload-safe `OnUnload`                            | `NativeAddon.md` *Hot-Reload Best Practices*. VideoPlayer cleans `PlaylistRegistry` and `PlaylistEventDispatcher` before nulling its cached API pointer.     |

## Build-system rules from the `polyphase` skill **do not apply**

The main `polyphase` skill enforces a checklist for new files:
`Engine/Engine.vcxproj`, `Engine/Engine.vcxproj.filters`,
`Engine/Makefile_Linux`, `FORCE_LINK_CALL` in `Engine.cpp`, etc. **Addons do
not use any of that.**

Addons build from their own `package.json` plus the editor-generated CMake / VS
project. New source files in an addon's `Source/` directory are picked up
automatically by the glob/wildcard. The only file you may need to regenerate is
`.vscode/c_cpp_properties.json` (and the `.vcxproj` on Windows) when engine
include paths change — **Tools → Addons → Regenerate Native Addon
Dependencies** does that. The only `FORCE_LINK_CALL` you need is the one in
your addon's own `OnLoad`.

## Troubleshooting

| Symptom                                                              | Likely cause                                                                                                                                                                   |
| -------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| "API version mismatch on load"                                       | Your manifest's `native.apiVersion` doesn't match `POLYPHASE_PLUGIN_API_VERSION` in `PolyphasePluginAPI.h`. Read the header for the current value.                              |
| Custom type doesn't appear in *Add Node* / asset menu                | `FORCE_LINK_CALL(MyType)` missing inside the addon's `OnLoad`. The linker dropped the TU.                                                                                       |
| Editor crashes inside `DrawWindows` (or any UI redraw) on reload     | Addon kept callbacks / Lua refs / threads alive past `OnUnload`. Audit addon-side singletons for `AssetRef` / `ScriptFunc` / threads.                                          |
| Debug build works, Release crashes inside the addon's STL            | CRT mismatch. The fingerprint encodes host CRT, so the engine itself rebuilds when you switch — but custom build scripts may not. Match `/MD` vs `/MDd` to the host.            |
| Custom REST route returns 404                                        | Controller server is disabled. Enable in *Preferences → Network → Controller Server Enabled* (default off). See `polyphase-controller` skill.                                  |
| Shipped build link error: `undefined reference to PolyphasePlugin_GetDesc_<id>` | The editor's generated `AddonPlugins.cpp` is out of date. Run **Tools → Addons → Regenerate Native Addon Dependencies** and rebuild.                                          |
| Addon rebuilds on every load                                         | A timestamped artifact ends up under the source dir, or a build script writes mtimes inside `Source/`. Move generated outputs to `Intermediate/`.                              |
| `RegisterScriptFuncs(void* L)` fails to compile                      | The signature is `RegisterScriptFuncs(lua_State* L)`. Older docs have the wrong type.                                                                                          |
| Calling `hooks->AddCreateAssetItem(...)` crashes on an older engine  | The pointer is `nullptr` on engine builds older than the slash-path / declarative-create patch. Null-check it and fall back to `AddCreateAssetItems` (callback form).            |
| Addon reads garbage bytes / "file not found" only in a packaged build | The addon used raw `fopen` on engine content. Switch to `Stream::ReadFile` — it handles Content Pak and the Static obfuscation container. See *Reading files from an addon*. |
| Addon Lua fails to load in a shipped build but works in the editor    | `luaL_dofile` bypasses both the obfuscation decode and the embedded-script table. Use `Stream::ReadFile` + `luaL_loadbuffer`. |
| Asset created from menu doesn't appear until Refresh                 | The menu callback used `Asset::SaveFile` directly. Switch to `AssetManager::Get()->CreateAndRegisterAsset(type, GetCurrentAssetDir(), name, false)` — that inserts the stub into the AssetDir tree as well as writing the .oct, so the browser sees it on the next frame. |
| Build fails with `C1083: Cannot open include file: 'vulkan/vulkan.h'` | Engine headers include Vulkan under `API_VULKAN`, so every addon build needs the Vulkan headers even if the addon never uses Vulkan. The editor resolves them from `VULKAN_SDK`, then `<engine>/External/Vulkan/include` (shipped by the installer), then default SDK locations. Install the Vulkan SDK and restart the editor, or use an engine install that ships `External/Vulkan/include`. The "Native Addon Problem" modal names this case directly. |
| Addon loads with no errors but its window / nodes never appear      | A dependency declared in `package.json` is not installed (for example the addon was cloned into `Packages/` by hand). Look for "Missing dependency: <id>" in the Addons window Installed tab and press Install, or install the dependency from the Browse tab. |
| Build fails with `C1083: Cannot open include file: 'EditorUtils.h'` or `'document.h'` (rapidjson) even though other addons in the same suite build fine | The addon has editor-only code or uses rapidjson but its base include set doesn't cover `Engine/Source/Editor` or `External/Assimp/contrib/rapidjson` — those two are NOT in every addon's default include list. Add them via `native.extraIncludeDirs` in `package.json`. Paths there are resolved **relative to the addon's own directory**, not `POLYPHASE_PATH` (see `NativeAddonManager.cpp`'s build.bat/CMake generation) — if the addon's project lives outside the engine repo (a common layout for a multi-project suite), you need a `../../../..` relative path across the two checkouts, e.g. `"../../../../../Polyphase/CODE/mergePolyphase/polyphase-engine/Engine/Source/Editor"`. Verify the path resolves with a plain `ls` before trusting it. |
| Hand-edited `CMakeLists.txt` or `.vcxproj` fix "disappears" / build error comes back unchanged after a fresh build attempt | Both files are editor-generated from `package.json` and get overwritten on the next build/reload (the real build the editor runs is a generated `build.bat` under `Intermediate/Plugins/<id>/<fingerprint>/`, not the `.vcxproj` — the `.vcxproj` is a separate, only-regenerated-on-request IDE convenience). Never hand-edit generated build files to fix an include/lib path — edit `native.extraIncludeDirs` / `extraLibDirs` / `extraLibs` in `package.json` instead, which survives regeneration and correctly invalidates the build fingerprint. |
| `node:SetScriptFile("Scripts/Foo.lua")` (or the same path passed to `Script::SetFile`) logs "Class has not been loaded" for a file that definitely exists | `ScriptUtils::RunScript` already prepends `"Scripts/"` to whatever filename it's given (`fullFileName = projectDir + "Scripts/" + relativeFileName`). Passing a path that already includes that prefix doubles it (`Scripts/Scripts/Foo.lua`), which doesn't exist, so the load silently fails. Pass just the bare filename relative to the project's `Scripts/` root, e.g. `SetScriptFile("Foo.lua")`. The same doubling risk applies to the `"Packages/{id}/{name}"` form `RunScript` also understands — don't prepend `Scripts/` there either. |
| A Lua script's `Create()` crashes with "attempt to index a nil value" on a `FindChild(...)` result, or on a property the editor clearly has a value for | `Create()` fires too early for both: `Node::Start()` recurses into children *before* calling a node's own script `Start` (see `Node.cpp`, bottom-up), but nothing analogous guarantees children exist before `Create()` runs, and `Script::CreateScriptInstance` calls `Create()` *before* `UploadScriptProperties()` (see `Script.cpp`) — so editor-assigned property values (including `DatumType.Node3D` references) aren't in the Lua table yet either. Do cross-node `FindChild`/`ConnectSignal` wiring and any property-dependent init in `Start()`, not `Create()`. Reserve `Create()` for state that depends on nothing but the node itself. |
| An addon-created runtime `Texture` (video frame, webcam feed, procedural image) is handed to a `Quad`/material via `SetTexture`, the call "succeeds," but the quad never shows it — stays its flat color or the texture's seeded black | The texture was created with a bare `new Texture()` + `Init()` + `Create()` and never registered with the AssetManager. `Quad::ResolveQuadTexture` (engine, `Quad.cpp`) validates every bound texture with `AssetManager::IsAssetLive()` before use — a defense against dangling `Asset*` with dead vtables — and an unregistered texture fails that check, so the quad `ClearDangling()`s the ref and draws nothing on the very next `PreRender`. Create it with `NewTransientAsset<Texture>()` (`AssetManager.h`, addon-exported) instead. **Ownership then changes:** the AssetManager owns transient-asset memory and frees it in `RefSweep()` once its `AssetRef` refcount is 0 — so hold an `AssetRef` in your node (not a raw `Texture*`), never `delete` it yourself (release the ref instead; deleting dangles every Quad/Lua userdata still bound to it — the exact crash `IsAssetLive` guards against), and remember `RefSweep` is not periodic: it runs on `GarbageCollect()` (`Engine.GarbageCollect()` from Lua) and a few editor actions, so a long-running app that churns big textures should release refs and call it at a quiet point. `UnregisterTransientAsset()` exists for the rarer "addon truly owns the memory" case. Worked example: `com.polyphase.formats.webcam`'s `WebcamPlayer3D::EnsureTexture()`. |
| Reading a runtime texture's pixels back (`Texture::GetPixels()`) to encode/upload/save works in the editor but the buffer is empty in a packaged build | `Texture::Create()` frees `mPixels` after the GPU upload in non-editor builds (`#if !EDITOR`); only the editor keeps the CPU copy, for saving. Call `SetRetainPixels(true)` on the texture **before** `Create()` (also keeps `UpdatePixels()` mirroring into the CPU copy). Costs `w*h*4` bytes per texture, so only for textures that are actually read back — not a live video/webcam feed that is only drawn. Consumers should validate `GetPixels().size() == w*h*4` and say why it can fail; `PhotoUploadManager::Submit` in `com.polyphase.assetio.camera` is the pattern. |
| A Lua handler connected with `node:ConnectSignal("MySignal", listener, listener.OnMySignal)` gets its payload shifted by one — the first parameter holds the payload and the last is `nil` | `Signal::Emit` (`Signals.cpp`) calls the handler as `handler(listener, ...emittedArgs)`; the **emitter is not prepended**. Built-in widgets appear as the first argument only because `Button`/`CheckBox`/etc. explicitly emit `{ this }`. For your own `EmitSignal("MySignal", payload)`, either pass the emitter yourself — `self:EmitSignal("MySignal", self, payload)` — to keep the `(emitter, payload)` shape, or declare the handler without an emitter parameter. Don't model custom-signal handlers on `OnActivated(button)` without doing one of those. || An addon's `nativePerPlatform.Android` `extraDefines` / `extraLibs` never reach the Android build — code behind your define compiles out (falling back to a stub), or libs you declared don't link | `NativeModuleMetadata::ResolveExtras` looks the platform up by **case-sensitive** key. `ActionManager`'s Android packaging path passed lowercase `"android"` for a long time, so *no* addon's `nativePerPlatform.Android` block applied on affected engine builds. `extraIncludeDirs` coming from a platform-common block still worked, which masks the failure (headers resolve, defines don't). Verify by reading the generated `Standalone/Generated/AddonInject.cmake` after a package — `POLYPHASE_ADDON_DEFINES` / `POLYPHASE_ADDON_LIBS` should list your entries; empty means the wrong-case call. Also note `POLYPHASE_ADDON_LIBS` is spliced straight into `target_link_libraries`, so use bare CMake names (`camera2ndk`), never `-l`-prefixed linker flags. |
| An Android addon needs a runtime permission (camera, microphone, location) | The NDK cannot request one — only an `Activity` can raise the dialog, and its result arrives asynchronously on the UI thread, so no call can block for it. Pattern from `com.polyphase.formats.webcam`: declare `<uses-permission>` in `Standalone/Android/app/src/main/AndroidManifest.xml`, add `has*Permission()` / `request*Permission()` / `get*PermissionState()` to `PolyphaseActivity.java`, and call them over JNI (`GetEngineState()->mSystem.mActivity->vm` + `->clazz` — same shape as `System_Android.cpp`'s `iterateDirFiles`). Two things that bite: (1) guard the "already requested" flag with a **process-wide** `static`, not a per-instance member — the owning backend is often reconstructed on every retry, which re-fires the dialog once a second; (2) check the permission at **every** entry point that needs it, not just `Open()` — some devices return an empty device list from enumeration APIs until it's granted, so gating only on open dead-ends before the request is ever made. |

## Where to look for an advanced full-feature example

The VideoPlayer addon at
`M:\Projects\Polyphase\Addons\VideoPlayer\testing\GC\VideoPlayer-Demo-GC\Packages\com.polyphase.formats.video\`
demonstrates, in production code, every pattern this skill points at:

- Custom `Node3D` (`VideoPlayer3D`) with editor properties, Lua bindings, and
  per-frame async work.
- Custom `Asset` (`VideoClip`) with `Import()`, `SaveStream`/`LoadStream`,
  cook-time inspector knobs, and a sidecar file for streaming.
- Editor-side asset-import wiring via `RegisterImportExtension` for six file
  extensions.
- External library integration (FFmpeg) via `nativePerPlatform.Windows.{
  extraDefines, extraIncludeDirs, extraLibDirs, extraLibs, copyBinaries }`.
- Cross-platform decoder factory with a console fallback (PCV1 / THP / N3MV)
  so the addon links without FFmpeg on GameCube / Wii / 3DS.
- Dual editor/shipped entry-point names — the addon works as both a hot-reload
  DLL and a statically-linked module.
- An addon-owned event dispatcher (`PlaylistEventDispatcher`) with explicit
  `Clear()` in `OnUnload` to prevent stale Lua refs after hot-reload.

Read the addon's source when implementing equivalent patterns; do not
copy-paste blindly.
