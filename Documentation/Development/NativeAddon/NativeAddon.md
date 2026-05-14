# Native Addon Development Guide

This guide explains how to create C++ native addons for the Polyphase Engine. Native addons allow you to extend the engine with custom C++ code that can be hot-loaded in the editor and compiled into final builds.

---

## Overview

A native addon is an extension of the standard addon format that includes C++ source code. Native addons provide:

- **Hot-loading in Editor**: Compile and reload your C++ code without restarting the editor (Windows/Linux)
- **Final Build Integration**: Your C++ code is compiled directly into console builds (GameCube, Wii, 3DS)
- **Full Engine Access**: Register custom node types, expose Lua functions, and integrate with engine systems

---

## Addon Structure

A native addon has the following directory structure:

```
MyAddon/
    package.json          # Required: Addon metadata
    thumbnail.png         # Optional: Preview image (256x256 recommended)
    Assets/               # Optional: Asset files (textures, meshes, etc.)
    Scripts/              # Optional: Lua scripts
    Source/               # Required for native: C++ source code
        MyAddon.cpp       # Main plugin entry point
        include/          # Optional: Header files
        src/              # Optional: Additional source files
```

---

## package.json Format

The `package.json` file must include a `native` block to enable native code support:

```json
{
    "name": "My Native Addon",
    "author": "Your Name",
    "description": "Description of what your addon does.",
    "version": "1.0.0",
    "url": "https://github.com/yourname/myaddon",
    "updated": "2024-01-15",
    "tags": ["gameplay", "tools"],
    "native": {
        "sourceDir": "Source",
        "binaryName": "myaddon",
        "entrySymbol": "PolyphasePlugin_GetDesc",
        "apiVersion": 3
    }
}
```

### Native Block Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `target` | string | `"engine"` | Where the code runs (see below) |
| `sourceDir` | string | `"Source"` | Relative path to C++ source directory |
| `binaryName` | string | Required | Output binary name (without .dll/.so extension) |
| `entrySymbol` | string | `"PolyphasePlugin_GetDesc"` | Export function name |
| `apiVersion` | integer | `1` | Plugin API version for compatibility checking |

### Target Values

| Value | Hot-Load in Editor | Compiled into Final Builds | Use Case |
|-------|-------------------|---------------------------|----------|
| `"engine"` | Yes | Yes | Gameplay code, custom nodes, systems |
| `"editor"` | Yes | **No** | Debug tools, editor extensions, dev utilities |

**Example - Engine addon (default):**
```json
{
    "native": {
        "target": "engine",
        "binaryName": "mygameplay"
    }
}
```

**Example - Editor-only addon:**
```json
{
    "native": {
        "target": "editor",
        "binaryName": "mydebugtool"
    }
}
```

---

## Plugin Entry Point

Every native addon must implement a single exported function that returns a plugin descriptor:

> **Canonical API version:** Read `POLYPHASE_PLUGIN_API_VERSION` from `Engine/Source/Plugins/PolyphasePluginAPI.h` — the current value is `3`. The `apiVersion` field in your `package.json` and your descriptor must match. Older snippets that show `2` are stale.

```cpp
// MyAddon.cpp

#include "PolyphasePluginAPI.h"

static PolyphaseEngineAPI* sAPI = nullptr;

// Called when plugin is loaded
static int OnLoad(PolyphaseEngineAPI* api)
{
    sAPI = api;
    api->LogDebug("MyAddon: Loaded successfully!");

    // Perform initialization here
    // - Register custom types
    // - Set up resources
    // - Hook into engine systems

    return 0;  // Return 0 for success, non-zero for failure
}

// Called when plugin is about to be unloaded
static void OnUnload()
{
    // Perform cleanup here
    // - Unregister types
    // - Release resources
    // - Remove hooks

    if (sAPI)
    {
        sAPI->LogDebug("MyAddon: Unloading...");
    }
    sAPI = nullptr;

    // IMPORTANT: After this returns, your code will be unloaded!
    // Ensure no dangling pointers or callbacks remain.
}

// Optional: Called every frame during gameplay (PIE or built game)
static void Tick(float deltaTime)
{
    // Gameplay logic, physics, AI, etc.
}

// Optional: Called every frame in editor (regardless of play state)
static void TickEditor(float deltaTime)
{
    // Editor tools, visualization, debug overlays
}

// Optional: Register custom node types
static void RegisterTypes(void* nodeFactory)
{
    // Cast and use the node factory to register types
    // NodeFactory* factory = static_cast<NodeFactory*>(nodeFactory);
    // factory->RegisterType<MyCustomNode>();
}

// Optional: Register Lua script functions
static void RegisterScriptFuncs(lua_State* L)
{
    // Register your Lua bindings here
}

// Common descriptor-fill body. Both entry-point variants below call this.
static int FillDesc(PolyphasePluginDesc* desc)
{
    desc->apiVersion = OCTAVE_PLUGIN_API_VERSION;
    desc->pluginName = "My Addon";
    desc->pluginVersion = "1.0.0";

    // Lifecycle callbacks
    desc->OnLoad = OnLoad;
    desc->OnUnload = OnUnload;

    // Tick callbacks (set to nullptr if not used)
    desc->Tick = Tick;               // Gameplay only
    desc->TickEditor = TickEditor;   // Editor always

    // Registration callbacks (set to nullptr if not used)
    desc->RegisterTypes = RegisterTypes;
    desc->RegisterScriptFuncs = RegisterScriptFuncs;
    desc->RegisterEditorUI = nullptr;  // See "Extending the Editor UI" section

    // Editor lifecycle hooks (set to nullptr if not needed)
    desc->OnEditorPreInit = nullptr;   // Before editor ImGui is fully initialized
    desc->OnEditorReady = nullptr;     // After editor is fully initialized

    return 0;  // Success
}

// Required: Plugin descriptor export. The symbol name differs between editor
// hot-load (DLL, looked up via GetProcAddress) and shipped builds (statically
// linked alongside other addons). See "Static Plugin Registration" below.
#if EDITOR
extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#else
// Shipped build: each addon exports a uniquely-named symbol so multiple addons
// can coexist in one executable. The suffix is the addon id with every char
// outside [A-Za-z0-9_] replaced by underscore (so com.example.myaddon →
// com_example_myaddon). The editor regenerates a matching `AddonPlugins.cpp`
// that POLYPHASE_REGISTER_PLUGINs each suffixed symbol — you do not write the
// macro yourself.
extern "C" int PolyphasePlugin_GetDesc_MyAddon(PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#endif
```

---

## Plugin API Reference

### PolyphasePluginDesc

The descriptor structure you must fill in:

```cpp
struct PolyphasePluginDesc
{
    uint32_t apiVersion;           // Must match OCTAVE_PLUGIN_API_VERSION
    const char* pluginName;        // Display name
    const char* pluginVersion;     // Version string (e.g., "1.0.0")

    // Lifecycle callbacks
    int (*OnLoad)(PolyphaseEngineAPI* api);   // Called after loading
    void (*OnUnload)();                     // Called before unloading

    // Tick callbacks (set to nullptr if not used)
    void (*Tick)(float deltaTime);         // Called during gameplay (PIE or built game)
    void (*TickEditor)(float deltaTime);   // Called in editor regardless of play state

    // Registration callbacks
    void (*RegisterTypes)(void* nodeFactory);
    void (*RegisterScriptFuncs)(lua_State* L);

    // Editor UI extension (editor builds only, set to nullptr otherwise)
    void (*RegisterEditorUI)(EditorUIHooks* hooks, uint64_t hookId);

    // Editor lifecycle callbacks (editor builds only)
    void (*OnEditorPreInit)();   // Called before editor ImGui is fully initialized
    void (*OnEditorReady)();     // Called after editor is fully initialized, before main loop
};
```

> **API Version Note:** The current `POLYPHASE_PLUGIN_API_VERSION` is `3`. The `OnEditorPreInit` / `OnEditorReady` fields were added in version 2; older plugins remain compatible — the engine zeros out fields it doesn't recognise. Always read the canonical value from `Engine/Source/Plugins/PolyphasePluginAPI.h` rather than hard-coding it.

### Tick Callbacks

Native addons can receive frame updates through two separate tick callbacks:

| Callback | When Called | Use Case |
|----------|-------------|----------|
| `Tick` | During gameplay only (PIE or built game) | Gameplay logic, physics, AI |
| `TickEditor` | Every frame in editor (regardless of play state) | Editor tools, visualization, debug overlays |

**Important:** In editor builds, `TickEditor` is always called. `Tick` is only called when Play-In-Editor is active. In built games, only `Tick` is called.

**Example: Rotator addon that only rotates during gameplay:**
```cpp
static Node3D* sTargetNode = nullptr;
static float sRotationSpeed = 45.0f;  // degrees per second
static PolyphaseEngineAPI* sAPI = nullptr;

static void Tick(float deltaTime)
{
    if (sTargetNode && sAPI)
    {
        sAPI->Node3D_AddRotation(sTargetNode, 0.0f, sRotationSpeed * deltaTime, 0.0f);
    }
}

extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc)
{
    desc->apiVersion = OCTAVE_PLUGIN_API_VERSION;
    desc->pluginName = "Rotator";
    desc->pluginVersion = "1.0.0";
    desc->OnLoad = OnLoad;
    desc->OnUnload = OnUnload;
    desc->Tick = Tick;           // Called during gameplay only
    desc->TickEditor = nullptr;  // No editor tick needed
    // ...
    return 0;
}
```

### PolyphaseEngineAPI

The engine API provides access to core functionality:

```cpp
struct PolyphaseEngineAPI
{
    // ===== Logging =====
    void (*LogDebug)(const char* fmt, ...);
    void (*LogWarning)(const char* fmt, ...);
    void (*LogError)(const char* fmt, ...);

    // ===== Lua Access =====
    lua_State* (*GetLua)();

    // ===== Lua Wrappers =====
    // These wrap Lua C API functions so plugins don't need to link against Lua.
    // Use sEngineAPI->Lua_pushnumber() instead of lua_pushnumber()
    void (*Lua_settop)(lua_State* L, int idx);
    void (*Lua_pushvalue)(lua_State* L, int idx);
    void (*Lua_pop)(lua_State* L, int n);
    int (*Lua_gettop)(lua_State* L);
    int (*Lua_type)(lua_State* L, int idx);
    int (*Lua_isfunction)(lua_State* L, int idx);
    int (*Lua_istable)(lua_State* L, int idx);
    int (*Lua_toboolean)(lua_State* L, int idx);
    double (*Lua_tonumber)(lua_State* L, int idx);
    const char* (*Lua_tostring)(lua_State* L, int idx);
    void* (*Lua_touserdata)(lua_State* L, int idx);
    void (*Lua_pushnil)(lua_State* L);
    void (*Lua_pushboolean)(lua_State* L, int b);
    void (*Lua_pushnumber)(lua_State* L, double n);
    void (*Lua_pushstring)(lua_State* L, const char* s);
    void (*Lua_createtable)(lua_State* L, int narr, int nrec);
    void (*Lua_setfield)(lua_State* L, int idx, const char* k);
    void (*Lua_getfield)(lua_State* L, int idx, const char* k);
    void (*Lua_setglobal)(lua_State* L, const char* name);
    void (*Lua_getglobal)(lua_State* L, const char* name);
    // ... and more Lua wrappers (see PolyphaseEngineAPI.h for full list)

    // ===== World Management =====
    World* (*GetWorld)(int32_t index);
    int32_t (*GetNumWorlds)();

    // ===== Node Operations =====
    Node* (*SpawnNode)(World* world, const char* typeName);
    void (*DestroyNode)(Node* node);
    Node* (*FindNode)(World* world, const char* name);

    // ===== Node3D Operations =====
    void (*Node3D_GetRotation)(Node3D* node, float* outX, float* outY, float* outZ);
    void (*Node3D_SetRotation)(Node3D* node, float x, float y, float z);
    void (*Node3D_AddRotation)(Node3D* node, float x, float y, float z);
    void (*Node3D_GetPosition)(Node3D* node, float* outX, float* outY, float* outZ);
    void (*Node3D_SetPosition)(Node3D* node, float x, float y, float z);
    void (*Node3D_GetScale)(Node3D* node, float* outX, float* outY, float* outZ);
    void (*Node3D_SetScale)(Node3D* node, float x, float y, float z);

    // ===== Asset System =====
    Asset* (*LoadAsset)(const char* name);
    Asset* (*FetchAsset)(const char* name);
    void (*UnloadAsset)(const char* name);

    // ===== Audio =====
    void (*PlaySound2D)(SoundWave* sound, float volume, float pitch);
    void (*StopAllSounds)();
    void (*SetMasterVolume)(float volume);
    float (*GetMasterVolume)();

    // ===== Input =====
    bool (*IsKeyDown)(int32_t key);
    bool (*IsKeyJustPressed)(int32_t key);
    bool (*IsKeyJustReleased)(int32_t key);
    bool (*IsMouseButtonDown)(int32_t button);
    bool (*IsMouseButtonJustPressed)(int32_t button);
    void (*GetMousePosition)(int32_t* x, int32_t* y);
    void (*GetMouseDelta)(int32_t* deltaX, int32_t* deltaY);
    int32_t (*GetScrollWheelDelta)();

    // ===== Time =====
    float (*GetDeltaTime)();
    float (*GetElapsedTime)();

    // ===== Editor UI Hooks (Editor builds only) =====
    EditorUIHooks* editorUI;  // nullptr in game builds
};
```

See `PolyphaseEngineAPI.h` for the complete API with documentation comments.

---

## Export Macros

Use these macros for cross-platform compatibility:

```cpp
#include "PolyphasePluginAPI.h"

// OCTAVE_PLUGIN_API - marks functions for export
// Expands to:
//   Windows: __declspec(dllexport) or __declspec(dllimport)
//   Linux:   __attribute__((visibility("default")))

// OCTAVE_PLUGIN_API_VERSION - current API version number
// Check this matches the engine version you're targeting
```

When building your addon, define `OCTAVE_PLUGIN_EXPORT` to export symbols:

```cpp
// This is done automatically by the build system
#define OCTAVE_PLUGIN_EXPORT
#include "PolyphasePluginAPI.h"
```

---

## Engine API Surface Available to Addons

Native addons link against the editor's import library (`Polyphase.lib` on Windows, the editor exe's dynamic symbol table on Linux). Only engine classes and free functions marked with `POLYPHASE_API` are visible across that boundary — derive from or call anything else and you get `LNK2001 unresolved external symbol` against an installed editor build, even though it links fine against the in-tree development tree (where the addon links the full `Engine.lib` static archive directly).

### Supported derive-able / consumable surface

The following are confirmed `POLYPHASE_API`-exported and safe to use from addons:

- **Asset bases** — derive your custom asset types from any of these:
  - `Asset`
  - `Material`, `MaterialBase`, `MaterialLite`, `MaterialInstance`
  - `Texture`, `StaticMesh`, `SkeletalMesh`, `Scene`, `SoundWave`, `Font`, `ParticleSystem`
- **Node bases** — derive custom nodes from:
  - `Node`, `Node3D`, `Widget`
- **Managers / singletons** — call from your addon:
  - `Renderer::Get()`
  - `AssetManager::Get()` and `AssetManager::RegisterTransientAsset(Asset*)`
  - `GetWorld(index)`, `GetNumWorlds()`, `GetSignalBus()`
  - `GetEngineState()`, `GetEngineConfig()`, `GetMutableEngineConfig()`, `GetAppClock()`
- **Free functions**:
  - `LogDebug()`, `LogWarning()`, `LogError()`, `LogConsole()`
  - `IsHeadless()`
  - `FetchAsset()`, `LoadAsset()`
  - `RegisterImportExtension()`
- **Plugin hook entry points**:
  - `RegisterOctHooks()`, `GetOctHooks()`

The engine's `Engine/Source/Plugins/PolyphaseEngineAPI.h` wrapper struct is the older, function-pointer-based way to reach a subset of the same surface — useful if you want to keep your addon binary-compatible across engine versions. Using the exported classes directly is more ergonomic but couples your addon to the exact engine ABI.

### If you hit LNK2001 against an engine symbol

If your addon links against an installed editor and you see `unresolved external symbol "<engine class or function>"`, the symbol is almost certainly missing its `POLYPHASE_API` annotation. File an issue with the LNK2001 output — the engine maintainer adds the macro and a CI smoke-test entry that prevents future regressions (see `Tools/CI/TestBuildAddon/com.polyphase.smoke.material/` for the existing fixture, which is built on every Windows/Linux release).

---

## Development Workflow

There are two ways to develop native addons:

### Option A: Local Development (Recommended for Development)

Create your addon directly in your project's `Packages/` folder:

```
{YourProject}/
    Packages/
        MyAddon/
            package.json
            Source/
                MyAddon.cpp
```

**Steps:**
1. Create `{ProjectDir}/Packages/MyAddon/` directory
2. Add `package.json` with native configuration
3. Add your C++ source files in `Source/`
4. Use **Tools > Addons > Reload Native Addons**
5. Your addon is automatically discovered, built, and loaded

**Advantages:**
- No installation step required
- Immediate hot-reload workflow
- Easy to iterate during development
- Files stay in your project folder

### Option B: Installed Addons (For Distribution)

Use the Addons window to install from a repository:

**Steps:**
1. Go to **Edit > Addons...**
2. Add your addon repository or install from local ZIP
3. The addon assets/scripts are merged into your project
4. Native source stays in the addon cache
5. Enable native code in the **Installed** tab

### Building and Reloading

**From the Addons Window:**
- Find your addon in the **Installed** tab
- Click **Build** to compile
- Click **Reload** to hot-load changes
- Check the build log for errors

**From the Tools Menu:**

| Menu Item | Description |
|-----------|-------------|
| **Tools > Addons > Reload Native Addons** | Discovers, builds changed addons, and reloads all native addons |
| **Tools > Addons > Discover Native Addons** | Scans for native addons without building or loading |
| **Tools > Addons > Regenerate Native Addon Dependencies** | Rewrites IDE configs (.vcxproj, .vscode/c_cpp_properties.json) for all addons |
| **Tools > Addons > Create Native Addon...** | Opens dialog to create a new native addon with template files |
| **Tools > Addons > Package Native Addon...** | Opens dialog to package an addon as a distributable ZIP |

**From the File Menu:**

| Menu Item | Description |
|-----------|-------------|
| **File > Reload All Scripts** | Reloads Lua scripts AND regenerates native addon dependencies AND reloads native addons |

### Iterate Quickly

The hot-reload workflow:
1. Edit your C++ code
2. Press **Tools > Addons > Reload Native Addons**
3. Changes take effect immediately - no editor restart!

**Tip:** If you also have Lua scripts, use **File > Reload All Scripts** to reload everything at once.

### IDE Configuration

Native addons generate IDE configuration files for IntelliSense support:

- `.vscode/c_cpp_properties.json` - VS Code C++ configuration
- `{AddonName}.vcxproj` - Visual Studio project file
- `CMakeLists.txt` - CMake configuration

These files include the correct engine include paths. If the engine is moved or paths change, use **Tools > Addons > Regenerate Native Addon Dependencies** to update them.

---

## Source vs Binary Mode

Native addons support two resolve modes that control how the addon is loaded:

### Source Mode (Default)

In source mode, the addon is compiled from source code on demand:
- Build/Reload buttons compile the C++ code
- Fingerprint-based caching avoids redundant rebuilds
- Full hot-reload workflow with immediate code changes

### Binary Mode

In binary mode, the addon uses precompiled binaries and **never compiles**:
- Eliminates startup compilation freezes for end-users
- Uses prebuilt `.dll`/`.so` files
- Requires manual Sync to download updates

### Switching Modes

In the **Installed** tab of the Addons window:
1. Find your addon
2. Use the **Mode** dropdown to switch between "Source" and "Binary"
3. Click **Reload** to apply the change

### Binary Resolution Order

When in binary mode, the addon loader searches for binaries in this order:

1. **Synced prebuilt** - Downloaded via the Sync button, stored in `Intermediate/Plugins/<addon>/Synced/`
2. **Local intermediate** - Previously compiled binary in `Intermediate/Plugins/<addon>/<fingerprint>/`
3. **Missing Binary** - If neither is found, displays an error status

### Configuring Remote Binaries

To enable the Sync button, add a `binaries` array to your `package.json`:

```json
{
    "native": {
        "binaryName": "myaddon",
        "resolveMode": "source",
        "binaries": [
            {
                "platform": "Windows",
                "arch": "x64",
                "type": "releaseAsset",
                "value": "myaddon-Windows-x64.dll",
                "checksumSha256": "abc123..."
            },
            {
                "platform": "Linux",
                "arch": "x64",
                "type": "releaseAsset",
                "value": "libmyaddon-Linux-x64.so"
            }
        ]
    }
}
```

### Binary Descriptor Fields

| Field | Type | Description |
|-------|------|-------------|
| `platform` | string | Target platform: "Windows", "Linux" |
| `arch` | string | Architecture: "x64" |
| `config` | string | Optional: "Debug" or "Release" |
| `type` | string | Source type: "releaseAsset", "url", or "zip" |
| `value` | string | Asset name (for releaseAsset), URL, or ZIP URL |
| `checksumSha256` | string | Optional: SHA256 hash for verification |
| `entryPath` | string | Optional: Path inside ZIP to extract |

### Binary Source Types

| Type | Description | Example `value` |
|------|-------------|-----------------|
| `releaseAsset` | GitHub release asset | `myaddon-Windows-x64.dll` |
| `url` | Direct download URL | `https://example.com/myaddon.dll` |
| `zip` | ZIP archive URL | `https://example.com/myaddon.zip` |

For `releaseAsset`, the Sync function constructs the URL as:
`{repoUrl}/releases/latest/download/{value}`

### GitHub Workflow for Binary Releases

New native addons include a `.github/workflows/native-addon-release.yml` template that:
- Triggers on tags matching `v*`
- Builds for Windows and Linux
- Generates SHA256 checksums
- Publishes release assets

Customize the build commands in the workflow file, then configure your `binaries` array to reference the published assets.

### Status Indicators

The Addons window shows these statuses in binary mode:

| Status | Meaning |
|--------|---------|
| **Loaded (Synced)** | Running from downloaded prebuilt |
| **Loaded (Local Binary)** | Running from local intermediate |
| **Missing Binary** | No binary found; use Sync or switch to Source mode |
| **Sync Failed** | Download failed; check network and URL |

---

## Hot-Reload Best Practices

Hot-reloading replaces your code while the editor is running. Follow these guidelines to avoid crashes:

### Do

- Keep plugin state minimal
- Use the engine's systems for persistent data
- Clean up all resources in `OnUnload()`
- Store configuration in project files, not static variables

### Don't

- Store raw pointers to plugin data in engine objects
- Use static variables that persist across reloads
- Register callbacks without a way to unregister them
- Create threads that outlive the plugin

### Example: Safe Callback Registration

```cpp
static void* sMyCallbackHandle = nullptr;

static int OnLoad(PolyphaseEngineAPI* api)
{
    // Register with a handle we can use to unregister
    sMyCallbackHandle = SomeSystem::RegisterCallback(MyCallback);
    return 0;
}

static void OnUnload()
{
    // Always unregister before unload!
    if (sMyCallbackHandle)
    {
        SomeSystem::UnregisterCallback(sMyCallbackHandle);
        sMyCallbackHandle = nullptr;
    }
}
```

---

## Registering Custom Node Types

To add custom nodes that appear in the editor:

```cpp
#include "PolyphasePluginAPI.h"
#include "Node.h"  // From engine includes

class MyCustomNode : public Node3D
{
    DECLARE_NODE(MyCustomNode, Node3D);

public:
    virtual void Tick(float deltaTime) override
    {
        // Custom behavior
    }
};

DEFINE_NODE(MyCustomNode, Node3D);

static void RegisterTypes(void* nodeFactory)
{
    // The factory handles registration via the DEFINE_NODE macro
    // Just ensure the symbol is linked
    FORCE_LINK_CALL(MyCustomNode);
}
```

**Note:** Custom node types require more careful handling during hot-reload. Existing instances of your node type will become invalid after unload.

---

## Extending the Editor UI

Native addons can extend the editor interface with custom menus, windows, and inspectors.

### Adding Menu Items

```cpp
static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    // Add a menu item under the Tools menu
    hooks->AddMenuItem(
        hookId,
        "Tools",                  // Menu to add to
        "My Addon/Do Something",  // Item path (supports submenus)
        [](void* userData) {
            LogDebug("Menu item clicked!");
        },
        nullptr,                  // User data
        "Ctrl+Shift+D"            // Optional shortcut
    );

    // Add to a custom top-level menu
    hooks->AddMenuItem(hookId, "My Addon", "Open Settings", MySettingsCallback, nullptr, nullptr);
}
```

### Creating Custom Windows

```cpp
static void DrawMyWindow(void* userData)
{
    ImGui::Text("Hello from my addon!");

    if (ImGui::Button("Click Me"))
    {
        LogDebug("Button clicked!");
    }

    static float myValue = 0.5f;
    ImGui::SliderFloat("Value", &myValue, 0.0f, 1.0f);
}

static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    // Register a dockable window
    hooks->RegisterWindow(
        hookId,
        "My Addon Window",        // Display name
        "myaddon_main_window",    // Unique ID for docking persistence
        DrawMyWindow,
        nullptr
    );

    // Optionally open it immediately
    hooks->OpenWindow("myaddon_main_window");
}
```

### Custom Inspectors

```cpp
static void DrawMyNodeInspector(void* node, void* userData)
{
    MyCustomNode* myNode = static_cast<MyCustomNode*>(node);

    ImGui::Text("Custom Inspector for MyCustomNode");

    // Draw custom properties
    float value = myNode->GetCustomValue();
    if (ImGui::DragFloat("Custom Value", &value))
    {
        myNode->SetCustomValue(value);
    }
}

static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    hooks->RegisterInspector(hookId, "MyCustomNode", DrawMyNodeInspector, nullptr);
}
```

### Context Menu Extensions

```cpp
static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    // Add to node right-click menu
    hooks->AddNodeContextItem(hookId, "My Addon/Do Something", MyNodeAction, nullptr);

    // Add to asset right-click menu (for textures only)
    hooks->AddAssetContextItem(hookId, "Process Texture", "Texture", MyTextureAction, nullptr);
}
```

### Cleanup on Unload

**Important:** Always clean up your hooks when the plugin unloads!

```cpp
static uint64_t sHookId = 0;

static int OnLoad(PolyphaseEngineAPI* api)
{
    // hookId will be provided by RegisterEditorUI
    return 0;
}

static void OnUnload()
{
    // Hooks are automatically cleaned up by the engine when a plugin unloads
    // But you can also manually remove specific items if needed
}

static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    sHookId = hookId;  // Save for later if needed

    hooks->AddMenuItem(hookId, "Tools", "My Tool", MyCallback, nullptr, nullptr);
    hooks->RegisterWindow(hookId, "My Window", "my_window", DrawMyWindow, nullptr);

    // All hooks registered with this hookId are automatically removed
    // when the plugin is unloaded
}
```

---

## Editor Lifecycle Hooks

In addition to the UI extension hooks above, addons can register for editor lifecycle events. These hooks fire when specific editor actions occur (project open/close, scene changes, packaging, etc.), enabling addons to react to editor state changes without polling.

All lifecycle hooks are registered through the `EditorUIHooks` struct in your `RegisterEditorUI` callback. Each registration takes a `HookId` for automatic cleanup on unload.

### Editor Init Hooks (`PolyphasePluginDesc`)

These two hooks are set directly in `PolyphasePluginDesc`, not via `EditorUIHooks`:

| Callback | When Fired | Use Case |
|----------|-----------|----------|
| `OnEditorPreInit` | Before editor ImGui is fully initialized | Custom fonts, ImGui config flags |
| `OnEditorReady` | After editor is fully initialized, before main loop | Query project state, open windows |

```cpp
static void OnEditorPreInit()
{
    // ImGui context exists but UI is not yet fully configured
    // Add custom fonts, set ImGui config flags, etc.
}

static void OnEditorReady()
{
    // Editor is fully up - safe to query project state, open windows, etc.
}

extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc)
{
    // ... other fields ...
    desc->OnEditorPreInit = OnEditorPreInit;
    desc->OnEditorReady = OnEditorReady;
    return 0;
}
```

### Project Lifecycle Events

| Hook | Callback Type | Parameter | When Fired |
|------|--------------|-----------|-----------|
| `RegisterOnProjectOpen` | `StringEventCallback` | Project path | After a project is opened |
| `RegisterOnProjectClose` | `StringEventCallback` | Project path | Before a project is closed |
| `RegisterOnProjectSave` | `StringEventCallback` | File path | After a project/scene is saved |

```cpp
static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    hooks->RegisterOnProjectOpen(hookId, [](const char* path, void*) {
        LogDebug("Project opened: %s", path);
    }, nullptr);

    hooks->RegisterOnProjectClose(hookId, [](const char* path, void*) {
        LogDebug("Project closing: %s", path);
    }, nullptr);

    hooks->RegisterOnProjectSave(hookId, [](const char* path, void*) {
        LogDebug("Project saved: %s", path);
    }, nullptr);
}
```

### Scene Lifecycle Events

| Hook | Callback Type | Parameter | When Fired |
|------|--------------|-----------|-----------|
| `RegisterOnSceneOpen` | `StringEventCallback` | Scene name | After a scene is opened for editing |
| `RegisterOnSceneClose` | `StringEventCallback` | Scene name | Before a scene is closed |

```cpp
hooks->RegisterOnSceneOpen(hookId, [](const char* scene, void*) {
    LogDebug("Scene opened: %s", scene);
}, nullptr);

hooks->RegisterOnSceneClose(hookId, [](const char* scene, void*) {
    LogDebug("Scene closing: %s", scene);
}, nullptr);
```

### Packaging/Build Events

| Hook | Callback Type | Parameters | When Fired |
|------|--------------|-----------|-----------|
| `RegisterOnPackageStarted` | `PlatformEventCallback` | Platform (int32_t) | Before build begins |
| `RegisterOnPackageFinished` | `PackageFinishedCallback` | Platform, success (bool) | After build completes or fails |

```cpp
hooks->RegisterOnPackageStarted(hookId, [](int32_t platform, void*) {
    LogDebug("Build starting for platform %d", platform);
    // Pre-build validation here
}, nullptr);

hooks->RegisterOnPackageFinished(hookId, [](int32_t platform, bool success, void*) {
    if (success)
        LogDebug("Build succeeded for platform %d", platform);
    else
        LogError("Build failed for platform %d", platform);
}, nullptr);
```

### Editor State Events

| Hook | Callback Type | Parameter | When Fired |
|------|--------------|-----------|-----------|
| `RegisterOnSelectionChanged` | `EventCallback` | (none) | When the selected node changes |
| `RegisterOnPlayModeChanged` | `PlayModeCallback` | State: 0=Enter, 1=Exit, 2=Eject | When PIE state changes |
| `RegisterOnEditorShutdown` | `EventCallback` | (none) | Before editor shuts down |

```cpp
hooks->RegisterOnSelectionChanged(hookId, [](void*) {
    LogDebug("Selection changed");
    // Query current selection via editor API
}, nullptr);

hooks->RegisterOnPlayModeChanged(hookId, [](int32_t state, void*) {
    const char* names[] = { "Enter", "Exit", "Eject" };
    LogDebug("Play mode: %s", names[state]);
}, nullptr);

hooks->RegisterOnEditorShutdown(hookId, [](void*) {
    LogDebug("Editor shutting down - saving addon state");
}, nullptr);
```

### Asset Pipeline Events

| Hook | Callback Type | Parameter | When Fired |
|------|--------------|-----------|-----------|
| `RegisterOnAssetImported` | `StringEventCallback` | Asset name | After an asset is imported |
| `RegisterOnAssetDeleted` | `StringEventCallback` | Asset name | After an asset is deleted |
| `RegisterOnAssetSaved` | `StringEventCallback` | Asset name | After an asset is saved |

```cpp
hooks->RegisterOnAssetImported(hookId, [](const char* name, void*) {
    LogDebug("Asset imported: %s", name);
}, nullptr);

hooks->RegisterOnAssetDeleted(hookId, [](const char* name, void*) {
    LogDebug("Asset deleted: %s", name);
}, nullptr);

hooks->RegisterOnAssetSaved(hookId, [](const char* name, void*) {
    LogDebug("Asset saved: %s", name);
}, nullptr);
```

### Undo/Redo Events

| Hook | Callback Type | Parameter | When Fired |
|------|--------------|-----------|-----------|
| `RegisterOnUndoRedo` | `EventCallback` | (none) | After an undo or redo operation |

```cpp
hooks->RegisterOnUndoRedo(hookId, [](void*) {
    LogDebug("Undo/Redo performed - refreshing addon state");
}, nullptr);
```

### Top-Level Menus

Add a custom top-level menu to the editor viewport bar alongside File, Edit, View, etc.

| Hook | When Called |
|------|-----------|
| `AddTopLevelMenuItem` | Draws custom menu contents inside ImGui popup |
| `RemoveTopLevelMenuItem` | Removes a previously added top-level menu |

```cpp
hooks->AddTopLevelMenuItem(hookId, "My Addon", [](void*) {
    if (ImGui::MenuItem("Open Dashboard")) { /* ... */ }
    if (ImGui::MenuItem("Settings")) { /* ... */ }
    ImGui::Separator();
    if (ImGui::MenuItem("About")) { /* ... */ }
}, nullptr);
```

### Toolbar Items

Add custom items to the editor viewport toolbar (next to the Play button).

| Hook | When Called |
|------|-----------|
| `AddToolbarItem` | Draws custom toolbar widgets inline |
| `RemoveToolbarItem` | Removes a previously added toolbar item |

```cpp
hooks->AddToolbarItem(hookId, "MyToolbarButton", [](void*) {
    if (ImGui::Button("Quick Build"))
    {
        // Trigger build action
    }
}, nullptr);
```

### Callback Type Reference

| Type | Signature | Used By |
|------|-----------|---------|
| `EventCallback` | `void(void* userData)` | Selection, Shutdown, UndoRedo |
| `StringEventCallback` | `void(const char* str, void* userData)` | Project, Scene, Asset events |
| `PlatformEventCallback` | `void(int32_t platform, void* userData)` | PackageStarted |
| `PackageFinishedCallback` | `void(int32_t platform, bool success, void* userData)` | PackageFinished |
| `PlayModeCallback` | `void(int32_t state, void* userData)` | PlayModeChanged |
| `TopLevelMenuDrawCallback` | `void(void* userData)` | Top-level menus |
| `ToolbarDrawCallback` | `void(void* userData)` | Toolbar items |

---

## Exposing Lua Functions

Add custom functions callable from Lua scripts:

```cpp
#include "PolyphasePluginAPI.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

static int Lua_MyFunction(lua_State* L)
{
    // Get arguments
    const char* message = luaL_checkstring(L, 1);

    // Do something
    LogDebug("MyAddon: %s", message);

    // Return values
    lua_pushboolean(L, true);
    return 1;  // Number of return values
}

static void RegisterScriptFuncs(lua_State* L)
{
    // Create a table for your addon's functions
    lua_newtable(L);

    lua_pushcfunction(L, Lua_MyFunction);
    lua_setfield(L, -2, "MyFunction");

    // Register the table globally
    lua_setglobal(L, "MyAddon");
}
```

Usage from Lua:

```lua
-- In your Lua scripts
MyAddon.MyFunction("Hello from Lua!")
```

---

## Lua-Based Editor Extensions

You can also extend the editor UI using pure Lua scripts (no C++ required).

### Lua UI Hooks

```lua
-- MyEditorTool.lua (in your addon's Scripts/ folder)

-- Only runs in editor
if not Editor then return end

-- Use this script's UUID as the hook identifier
local hookId = Script.GetUUID()

-- Add a menu item
Editor.AddMenuItem(hookId, "Tools", "My Lua Tool", function()
    Log.Debug("Lua tool activated!")
    Editor.OpenWindow("my_lua_window")
end)

-- Register a custom window
Editor.RegisterWindow(hookId, "My Lua Window", "my_lua_window", function()
    ImGui.Text("Hello from Lua!")

    if ImGui.Button("Do Something") then
        Log.Debug("Button clicked from Lua!")
    end

    -- Access engine state
    local world = Engine.GetWorld(0)
    if world then
        ImGui.Text("World: " .. tostring(world))
    end
end)

-- Clean up when script is unloaded/reloaded
function OnUnload()
    Editor.RemoveAllHooks(hookId)
end
```

### Available Lua Editor Functions

```lua
-- Menu items
Editor.AddMenuItem(hookId, menuPath, itemPath, callback, shortcut)
Editor.AddMenuSeparator(hookId, menuPath)
Editor.RemoveMenuItem(hookId, menuPath, itemPath)

-- Windows
Editor.RegisterWindow(hookId, displayName, windowId, drawCallback)
Editor.UnregisterWindow(hookId, windowId)
Editor.OpenWindow(windowId)
Editor.CloseWindow(windowId)
Editor.IsWindowOpen(windowId)

-- Context menus
Editor.AddNodeContextItem(hookId, itemPath, callback)
Editor.AddAssetContextItem(hookId, itemPath, assetTypeFilter, callback)

-- Cleanup
Editor.RemoveAllHooks(hookId)
```

### IMPORTANT: Game Build Safety

The `Editor` table **does not exist** in game builds. Always check before using:

```lua
-- CORRECT - Safe for game builds
if Editor then
    Editor.AddMenuItem(...)
end

-- CORRECT - Guard entire script
if not Editor then return end

-- WRONG - Will crash in game builds!
Editor.AddMenuItem(...)  -- Editor is nil in game!
```

This ensures your addon's Lua scripts work in both editor and game builds.

### ImGui Bindings for Lua

Common ImGui functions are exposed to Lua:

```lua
-- Layout
ImGui.Text("Hello")
ImGui.TextColored(1, 0, 0, 1, "Red text")
ImGui.Separator()
ImGui.SameLine()
ImGui.NewLine()

-- Input
if ImGui.Button("Click") then ... end
local changed, value = ImGui.DragFloat("Speed", currentValue, 0.1, 0, 100)
local changed, value = ImGui.SliderInt("Count", currentValue, 0, 10)
local changed, value = ImGui.Checkbox("Enabled", currentValue)
local changed, text = ImGui.InputText("Name", currentText, 256)

-- Containers
if ImGui.CollapsingHeader("Section") then ... end
if ImGui.TreeNode("Node") then ... ImGui.TreePop() end
if ImGui.BeginChild("Scroll", 0, 200) then ... ImGui.EndChild() end
```

---

## Final Build Integration

When building for consoles or release:

1. Enable your addon in the project
2. Ensure "Enable Native" is checked
3. Build via **File > Build Data > [Platform]**

The build system will:
- Add your `Source/` directory to include paths
- Compile all `.cpp` files directly into the executable
- No dynamic loading on consoles - code is statically linked

### Static Plugin Registration

In built games (non-editor), addons are statically linked and registered at startup via the `POLYPHASE_REGISTER_PLUGIN` macro — but **the editor generates that registrar for you**. You should not write it yourself.

Each addon exports two differently-named entry-point symbols depending on build mode:

| Build mode  | Exported symbol                                      | How it's invoked                                                                         |
|-------------|------------------------------------------------------|-------------------------------------------------------------------------------------------|
| Editor      | `PolyphasePlugin_GetDesc`                            | `GetProcAddress` / `dlsym` on the loaded DLL.                                             |
| Shipped     | `PolyphasePlugin_GetDesc_<sanitized_id>`             | A `POLYPHASE_REGISTER_PLUGIN` call in `Generated/AddonPlugins.cpp`, generated by the editor. |

The sanitized id replaces every character outside `[A-Za-z0-9_]` with `_`, so `com.example.myaddon` becomes `com_example_myaddon` (see `Engine/Source/Editor/ActionManager.cpp` — the `SanitizeAddonIdForSymbol` helper around line 193 and the registrar emitter around line 233 / 872).

Your addon's source therefore looks like this:

```cpp
#include "PolyphasePluginAPI.h"

// ... OnLoad / OnUnload / Tick / RegisterTypes / RegisterScriptFuncs / FillDesc ...

#if EDITOR
extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#else
// Suffix uses your sanitized addon id. For an addon whose package.json
// "name" is "com.example.myaddon", the suffix is com_example_myaddon.
extern "C" int PolyphasePlugin_GetDesc_com_example_myaddon(PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#endif
```

The editor writes `Generated/AddonPlugins.cpp` containing one `POLYPHASE_REGISTER_PLUGIN(<sanitized_id>, PolyphasePlugin_GetDesc_<sanitized_id>)` line per enabled addon, plus the matching `extern "C"` forward declaration. The `RuntimePluginManager` invokes those at static init time during engine startup and then calls `OnLoad` during the normal init sequence.

**Run** **Tools → Addons → Regenerate Native Addon Dependencies** **after** adding, removing, or renaming an addon — that's what regenerates `AddonPlugins.cpp`. Without it, the shipped build will fail to link with `undefined reference to PolyphasePlugin_GetDesc_<id>`.

> **Migration note:** Older addons that wrote `POLYPHASE_REGISTER_PLUGIN(MyAddon, PolyphasePlugin_GetDesc)` themselves (typically inside an `#if !defined(OCTAVE_PLUGIN_EXPORT)` block) should remove that line, switch to the dual-entry pattern above, and run **Tools → Addons → Regenerate Native Addon Dependencies** so the editor-generated registrar takes over.

### Platform-Specific Code

Use preprocessor checks for platform-specific code:

```cpp
#if PLATFORM_WINDOWS
    // Windows-specific code
#elif PLATFORM_LINUX
    // Linux-specific code
#elif PLATFORM_GCN
    // GameCube-specific code
#elif PLATFORM_WII
    // Wii-specific code
#elif PLATFORM_3DS
    // 3DS-specific code
#endif

#if EDITOR
    // Editor-only code (hot-reload support, debug UI, etc.)
#endif
```

### Tick in Built Games

The `Tick` callback works in both editor (during Play-In-Editor) and built games:

```cpp
static void Tick(float deltaTime)
{
    // This runs every frame during gameplay
    // Works in both editor PIE and built games
}
```

In built games, `TickEditor` is NOT called (it's editor-only). Only `Tick` is called during the game loop.

---

## Per-Platform Build Configuration & External Libraries

Native addons can declare extra compiler/linker inputs and per-platform overrides directly in `package.json`. This is how you bundle third-party libraries (FFmpeg, OpenAL, Bullet, …) and how you keep platform-specific decoders/backends out of unsupported targets.

### Common (cross-platform) extras

The top-level `native` block accepts five optional arrays applied to **every** platform:

| Field              | Effect                                                                                       |
| ------------------ | -------------------------------------------------------------------------------------------- |
| `extraDefines`     | Added to the compiler command line as preprocessor definitions.                              |
| `extraIncludeDirs` | Added to the include search path. Paths are resolved relative to the addon directory.        |
| `extraLibDirs`     | Added to the linker library search path.                                                     |
| `extraLibs`        | Linker inputs (e.g. `avcodec.lib`, `pulse-simple`).                                          |
| `copyBinaries`     | Files or directories copied next to the built addon binary post-build (typically runtime DLLs). |

```json
{
    "native": {
        "target": "engine",
        "sourceDir": "Source",
        "binaryName": "myaddon",
        "apiVersion": 3,
        "extraDefines": ["MYADDON_FEATURE_X=1"],
        "extraIncludeDirs": ["External/somelib/include"]
    }
}
```

### Per-platform overrides

The `nativePerPlatform.<PlatformName>` block lets you append platform-specific values without polluting other targets. Recognised platform names match `GetPlatformString(Platform)`:

```
"Windows"  "Linux"  "Android"  "GameCube"  "Wii"  "3DS"
```

Unknown platform names are silently ignored at resolve time — typos are not flagged.

```json
{
    "name": "com.example.myaudio",
    "version": "1.0.0",
    "native": {
        "target": "engine",
        "sourceDir": "Source",
        "binaryName": "com.example.myaudio",
        "apiVersion": 3
    },
    "nativePerPlatform": {
        "Windows": {
            "extraDefines": ["MYAUDIO_WITH_FFMPEG=1"],
            "extraIncludeDirs": ["External/ffmpeg/include"],
            "extraLibDirs":     ["External/ffmpeg/lib"],
            "extraLibs":        ["avformat.lib", "avcodec.lib", "avutil.lib"],
            "copyBinaries":     ["External/ffmpeg/bin"]
        },
        "Linux": {
            "extraDefines": ["MYAUDIO_WITH_FFMPEG=1"]
        },
        "GameCube": {},
        "Wii": {},
        "3DS": {}
    }
}
```

The matching directory layout for the Windows entry above:

```
Packages/com.example.myaudio/
    package.json
    Source/
    External/
        ffmpeg/
            include/        (third-party headers)
            lib/            (import libs: avformat.lib, ...)
            bin/            (runtime DLLs: avformat-62.dll, ...)
```

### Practical guidance

- **Vendor on Windows; system-link on Linux.** Prebuilt `.lib` + `.dll` pairs under `External/<lib>/` are the de-facto Windows pattern. On Linux, prefer the system package (`pkg-config`) and only set `extraDefines` to enable the matching feature flag in your code.
- **Consoles get their own backend.** GameCube / Wii / 3DS targets in the example above pass empty objects so they don't pull in FFmpeg. Implement a console-only fallback (e.g. a custom decoder) and gate it with `#if PLATFORM_DOLPHIN` / `#if PLATFORM_3DS`.
- **Keep platform-specific code in `#if` blocks** so the build link-completes everywhere even if the active backend is stubbed out — this prevents "missing symbol" surprises during a console package.
- **Manifest changes invalidate the fingerprint.** Any edit to `native.*` or `nativePerPlatform.*.*` triggers a rebuild of the affected addon (see `NativeAddonManager::ComputeFingerprint`).
- **Worked example.** The VideoPlayer addon at `Addons/VideoPlayer/.../com.polyphase.formats.video/` uses this exact pattern to integrate FFmpeg on Windows/Linux and ship a console-native decoder on GameCube/Wii/3DS — read its `package.json` and the matching `External/ffmpeg/` layout. See also [External Library Integration](Examples/ExternalLibrary.md).

---

## Troubleshooting

### Build Fails

- Check the build log in the Addons window
- Verify all includes are correct
- Ensure `PolyphasePluginAPI.h` is in your include path

### Plugin Won't Load

- Check that `PolyphasePlugin_GetDesc` is exported correctly
- Verify `apiVersion` matches `OCTAVE_PLUGIN_API_VERSION`
- Look for errors in the editor console

### Crash on Reload

- Ensure `OnUnload()` cleans up all resources
- Check for dangling pointers in engine objects
- Verify no static state persists incorrectly

### Missing Symbols on Console Build

- Add `FORCE_LINK_CALL()` for all custom types
- Check that source files are in the `sourceDir` path

---

## Example: Complete Addon

Here's a complete native addon example with tick callbacks:

**package.json:**
```json
{
    "name": "Hello Native",
    "author": "Polyphase Team",
    "description": "A minimal native addon example.",
    "version": "1.0.0",
    "tags": ["example"],
    "native": {
        "target": "engine",
        "sourceDir": "Source",
        "binaryName": "hellonative",
        "apiVersion": 3
    }
}
```

Use `"target": "editor"` if your addon should only run in the editor and not be compiled into final builds.

**Source/HelloNative.cpp:**
```cpp
#include "PolyphasePluginAPI.h"

static PolyphaseEngineAPI* sAPI = nullptr;
static float sTotalTime = 0.0f;

static int OnLoad(PolyphaseEngineAPI* api)
{
    sAPI = api;
    sTotalTime = 0.0f;
    api->LogDebug("Hello Native: Plugin loaded!");
    return 0;
}

static void OnUnload()
{
    if (sAPI)
    {
        sAPI->LogDebug("Hello Native: Plugin unloading...");
    }
    sAPI = nullptr;
}

// Called every frame during gameplay (PIE or built game)
static void Tick(float deltaTime)
{
    sTotalTime += deltaTime;
    // Gameplay logic here...
}

// Called every frame in editor (regardless of play state)
static void TickEditor(float deltaTime)
{
    // Editor visualization, debug overlays, etc.
}

static int FillDesc(PolyphasePluginDesc* desc)
{
    desc->apiVersion = OCTAVE_PLUGIN_API_VERSION;
    desc->pluginName = "Hello Native";
    desc->pluginVersion = "1.0.0";

    // Lifecycle
    desc->OnLoad = OnLoad;
    desc->OnUnload = OnUnload;

    // Tick callbacks
    desc->Tick = Tick;               // Gameplay only
    desc->TickEditor = TickEditor;   // Editor always (set to nullptr if not needed)

    // Registration
    desc->RegisterTypes = nullptr;
    desc->RegisterScriptFuncs = nullptr;
    desc->RegisterEditorUI = nullptr;

    // Editor lifecycle
    desc->OnEditorPreInit = nullptr;
    desc->OnEditorReady = nullptr;

    return 0;
}

#if EDITOR
extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#else
// Shipped: suffix is the sanitized addon id (non-alphanumerics → '_'). Run
// Tools → Addons → Regenerate Native Addon Dependencies after renaming.
extern "C" int PolyphasePlugin_GetDesc_hellonative(PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#endif
```

---

## See Also

- [Addons Documentation](../Info/Addons.md) - General addon system
- [Templates Documentation](../Info/Templates.md) - Project templates
- [Lua Scripting Guide](../Scripting/LuaGuide.md) - Lua API reference

### Examples

- [Custom Menu Item](Examples/CustomMenuItem.md) - Adding menu items to existing menus
- [Custom Debug Window](Examples/CustomDebugWindow.md) - Creating dockable windows
- [Custom Inspector](Examples/CustomScriptInspector.md) - Custom node inspectors
- [Custom Context Menu](Examples/CustomContextMenuItem.md) - Right-click context menus
- [Rotator3D](Examples/Rotator3D.md) - Gameplay tick example
- [Custom Asset Type](Examples/CustomAssetType.md) - Defining a new asset class with importer + serialization
- [Custom Graph Node](Examples/CustomGraphNode.md) - Adding a visual-scripting node from an addon
- [Addon-Packaged Material Shaders](Examples/AddonMaterialShaders.md) - Ship `MaterialBase` shader source inside your addon package
- [External Library Integration](Examples/ExternalLibrary.md) - Bundling third-party libraries with per-platform overrides

### Editor Hook Examples

- [Top Level Menu](Examples/Editor/TopLevelMenu.md) - Custom top-level menu in the viewport bar
- [Packaging Hooks](Examples/Editor/PackagingHooks.md) - Pre/post build validation
- [Project Lifecycle](Examples/Editor/ProjectLifecycle.md) - Project open/close/save events
- [Editor Init Hooks](Examples/Editor/EditorInitHooks.md) - OnEditorPreInit and OnEditorReady
- [Selection Handler](Examples/Editor/SelectionHandler.md) - Reacting to selection changes
- [Play Mode Hooks](Examples/Editor/PlayModeHooks.md) - PIE state change events
- [Scene Lifecycle](Examples/Editor/SceneLifecycle.md) - Scene open/close events
- [Asset Pipeline Hooks](Examples/Editor/AssetPipelineHooks.md) - Asset import/delete/save events
- [Toolbar Extension](Examples/Editor/ToolbarExtension.md) - Custom toolbar buttons
- [Undo/Redo Hook](Examples/Editor/UndoRedoHook.md) - Syncing state with undo/redo
- [Editor Shutdown Hook](Examples/Editor/EditorShutdownHook.md) - Cleanup before editor exit
