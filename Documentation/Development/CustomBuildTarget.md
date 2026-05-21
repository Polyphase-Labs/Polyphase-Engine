# Custom Build Targets (Native Addon API)

> Add a new platform to the editor — Dreamcast, PS2, original Xbox, Xbox 360,
> NDS, your own bespoke hardware — without touching engine source. The engine
> ships **only** the framework + the six built-in targets (Windows / Linux /
> Android / GameCube / Wii / 3DS); every other target lives in an addon DLL
> that carries its own SDK code and license.

## Why this exists

Adding a new platform used to mean editing `ActionManager.cpp`,
`PackagingWindow.cpp`, the `Platform` enum, asset-cook switches, Makefile
generators, and emulator-launch glue. Worse, it meant the engine repo had to
link against (or at least vendor) the new platform's SDK — and many of those
SDKs ship under licenses that the engine cannot inherit.

A custom build target is a native addon that registers a
**`PolyphaseBuildTargetDesc`** descriptor at editor-load time. The engine
calls back into the descriptor for compile, cook, finalize, and launch. The
addon DLL stays the **only** place that knows about the SDK, and the engine
binary has zero link-time dependency on any new platform.

## High-level shape

```
┌──────────────────────────────────────────────────────────────────┐
│  Your addon DLL (e.g. com.polyphase.build.target.dreamcast)      │
│  ────────────────────────────────────────────────                │
│  void RegisterEditorUI(EditorUIHooks* hooks, HookId id) {        │
│      static PolyphaseBuildTargetDesc desc = { ... };             │
│      hooks->RegisterBuildTarget(id, &desc);                      │
│  }                                                                │
└──────────────────────────┬───────────────────────────────────────┘
                           │ (auto-unregistered on hot-reload)
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  BuildTargetRegistry  (owned by EditorUIHookManager)             │
│  ────────────────────                                            │
│  • deep-copies every string in your descriptor                   │
│  • scoped by HookId — wiped on addon unload                      │
│  • built-in targets register here too, so they share the dropdown│
└──────────────────────────┬───────────────────────────────────────┘
                           │ lookup by target id string
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  ActionManager::BuildData(targetId, embedded)                    │
│                                                                   │
│  Phase 1  cook:      PreCook → for each asset:                   │
│                        CookAsset(override)? else                 │
│                        Asset::SaveStream(stream, basePlatform)   │
│  Phase 2  compile:   GetCompileCommand → SYS_ExecFull            │
│  Phase 3  finalize:  GetCompiledBinaryPath → copy artifact       │
│                      PostPackage (wrap into .cdi / .iso / ...)   │
│                      RunOnDevice / RunInEmulator (optional)      │
└──────────────────────────────────────────────────────────────────┘
```

## The descriptor

See `Engine/Source/Plugins/PolyphaseBuildTargetAPI.h` for the authoritative
definition. The shape, summarised:

| Field                       | Required | What it does                                                                                   |
| --------------------------- | -------- | ---------------------------------------------------------------------------------------------- |
| `apiVersion`                | ✔        | Must equal `POLYPHASE_BUILD_TARGET_API_VERSION`. Engine rejects mismatched descriptors.        |
| `targetId`                  | ✔        | Stable reverse-DNS id (e.g. `"homebrew.dreamcast"`). Used as the lookup key everywhere.        |
| `displayName`               | ✔        | Human label shown in the UI dropdown.                                                          |
| `iconText`                  |          | Optional ImGui glyph or short text rendered next to the label.                                 |
| `category`                  |          | Group heading in the dropdown ("Retro Consoles", "Mobile", etc.).                              |
| `basePlatform`              | ✔        | The built-in `Platform` whose default asset cook your target reuses (e.g. Dreamcast → Linux).  |
| `binaryExtension`           | ✔        | `.elf`, `.cdi`, `.xbe`, `.nds`, …                                                              |
| `requiresDocker`            |          | Non-zero if your toolchain must run inside a configured Docker image.                          |
| `supportsRunOnDevice`       |          | Enables the "Run on Device" toggle.                                                            |
| `supportsEmulator`          |          | Enables the "Run in Emulator" launch path.                                                     |
| `Validate(out, cap)`        |          | Pre-flight SDK check ("is `$KOS_BASE` set?"). 0 = unavailable; engine grays out the entry.     |
| `PreCook(ctx)`              |          | Fires once before the cook loop. Use for temp dirs, manifests, asset listing.                  |
| `CookAsset(...)`            |          | Per-asset override. Return non-zero if you've written cooked bytes into the stream.            |
| `GetCompileCommand`         | ✔        | Build the shell command line the engine runs via `SYS_ExecFull`.                               |
| `GetCompiledBinaryPath`     |          | Tell the engine where to find the linker output. Defaults to `<compiledBinaryDir>/<name><ext>`. |
| `PostPackage(ctx)`          |          | Wrap the packaged dir into the platform-native image (GDI, CDI, ISO, CIA, …).                  |
| `RunOnDevice(ctx, out, cap)`|          | Returns the shell command to deploy/run on real hardware.                                      |
| `RunInEmulator(ctx, out, cap)` |       | Returns the shell command to launch the emulator.                                              |
| `DrawProfileOptions`        |          | ImGui callback called inside the Packaging panel's "Target Options" header.                    |
| `SerializeProfileOptions` / `DeserializeProfileOptions` | | Persist target-specific config inside the BuildProfile JSON.                |
| `platformExtensionDir`      |          | **Variant 2**: addon-relative directory containing engine platform extension headers. Set when the addon ships a runtime (System/Input/Audio/Network for the new platform). See below. |

All function-pointer fields except `GetCompileCommand` are nullable. Built-in
targets register descriptors with **every** callback null — they fall through
to the engine's legacy switch-on-`Platform` pipeline. Addon targets supply
real callbacks and bypass the legacy path entirely.

### Build context

Every callback receives a `PolyphaseBuildContext*` with the active project /
output paths and a handful of helper trampolines:

| Trampoline                 | Purpose                                                                |
| -------------------------- | ---------------------------------------------------------------------- |
| `Log(severity, msg)`       | Log at Debug / Warning / Error severity.                               |
| `WriteOutputLine(line)`    | Append a line to the live build-output console.                        |
| `GetProfileSetting(key, …)`| Read a per-profile target-option string (drawn from `BuildProfile::mTargetOptions`). |
| `SetProfileSetting(key, val)` | Write/overwrite a per-profile option.                               |
| `ResolvePath(rel, …)`      | Resolve a relative path against `projectDir`.                          |

**`forceRebuild` field**: non-zero when the user checked "Force Rebuild" in the
Packaging panel. Addons should clean their per-target build artifacts (e.g.
`make clean`, delete `.o` / `.d` files) inside `GetCompileCommand` or `PreCook`
before kicking off their toolchain. The engine itself already wipes the
project-level `Intermediate/` cache and the per-target `Packaged/<id>/` dir
when this flag is set; addons only need to handle their own build outputs.

Reference (PSP addon's `GetCompileCommand`):

```cpp
if (ctx->forceRebuild)
{
    cleanPrefix =
        "(cd " + ShellPath(ctx->projectDir) +
        " && rm -f *.o *.d *.elf 2>/dev/null; true) && ";
}
std::snprintf(outCmd, cap, "%s%smake -C %s -f %s -j%d",
              wslPrefix.c_str(), cleanPrefix.c_str(),
              ShellPath(projectDir).c_str(),
              ShellPath(makefilePath).c_str(), jobs);
```

**Lifetime caveat:** the context pointer and every string it holds are valid
only for the duration of one callback invocation. Copy out anything you need
to keep.

## Hot-reload safety

The registry is built around the same `HookId` cleanup pattern as every
other addon hook. When the editor unloads your DLL:

1. `EditorUIHookManager::RemoveAllHooks(hookId)` fires.
2. `BuildTargetRegistry::RemoveAllForHook(hookId)` strips your entry from the
   registry — built-ins (registered with `HookId == 0` and `mIsBuiltIn = true`)
   are skipped.
3. The descriptor's `const char*` fields are owned by the registry as deep
   `std::string` copies, so the strings outlive your DLL's string literals.

What you must do:

- **Register inside `RegisterEditorUI(hooks, hookId)`**, not in `OnLoad`. The
  hookId is the cleanup key.
- **Don't keep any pointer to engine state past the end of a callback.** Every
  callback gets a fresh `PolyphaseBuildContext*`.
- **Don't capture engine `Asset*` / `Stream*` pointers across calls.** They're
  reused.

## Asset cooking

By default the engine cooks every asset for your `basePlatform`. This is
usually enough — pick the built-in platform whose binary/data layout is
closest. Reasonable starting points:

| Target          | basePlatform        | Why                                          |
| --------------- | ------------------- | -------------------------------------------- |
| Dreamcast       | `Platform::Linux`   | Unix-like toolchain, ELF binary, raw audio.  |
| PS2             | `Platform::Linux`   | Same. PS2SDK is also Unix-like.              |
| Original Xbox   | `Platform::Windows` | nxdk and DXT textures.                       |
| Xbox 360        | `Platform::Windows` | Win32 derivatives.                           |
| Nintendo DS     | `Platform::N3DS`    | Closer to the 3DS cook than to PC.           |
| Bespoke / Misc. | `Platform::Linux`   | Default to the most permissive cook.         |

If the basePlatform cook doesn't fit your hardware (PVR2 swizzling for
Dreamcast, GS palettes for PS2, NDS tile/palette formats…), register a
`CookAsset` callback. Returning non-zero from it tells the engine "I've
written my own bytes into the stream — commit those, don't call
`SaveStream` for this asset on this target."

You can override **per asset type** by inspecting `assetTypeName`:

```cpp
static int32_t Dreamcast_CookAsset(const PolyphaseBuildContext* ctx,
                                   const char* assetTypeName,
                                   void* assetPtr, void* streamPtr)
{
    if (strcmp(assetTypeName, "Texture") == 0)
    {
        // Twiddle pixels into PowerVR2 layout, write to stream, return 1.
        return 1;
    }
    return 0;       // fall through to default Linux cook
}
```

## Compile / link

The engine doesn't care what toolchain you use — gcc, clang, MSVC, sh-elf-gcc,
nxdk's CMake, devkitPro, an in-house assembler. `GetCompileCommand` returns a
single `SYS_ExecFull`-able command line. Stream stdout/stderr live with
`ctx->WriteOutputLine(...)` if you want progress in the build console.

After the compile succeeds, the engine copies the file at
`GetCompiledBinaryPath` into `packageOutputDir`. From there `PostPackage` can
do whatever wrapping the platform needs — wrap an `.elf` into a `.cdi`,
sign a `.cia`, prepare a `.iso`, etc.

## Per-profile options

Hardware targets accumulate config — region code, BIOS path, disc format.
Profiles persist these inside the JSON via:

```cpp
// Inside DrawProfileOptions(void* profilePtr):
auto* profile = static_cast<BuildProfile*>(profilePtr);
char regionBuf[8] = {0};
ctx->GetProfileSetting("region", regionBuf, sizeof(regionBuf));   // if you have ctx
// Render an ImGui combo bound to profile->mTargetOptions["region"]
```

`BuildProfile::mTargetOptions` is a plain `std::unordered_map<std::string,
std::string>`. The engine serialises it under `targetOptions` in the saved
profile JSON. You see the same map through `GetProfileSetting` /
`SetProfileSetting` on the build context during callbacks.

## Discoverability — `native.buildTargets`

Add an optional `buildTargets` array to your addon's `native` block so the
AddonsWindow can advertise the target before your DLL even loads:

```json
{
  "name": "com.polyphase.build.target.dreamcast",
  "version": "1.0.0",
  "native": {
    "target": "editor",
    "sourceDir": "Source",
    "binaryName": "com.polyphase.build.target.dreamcast",
    "entrySymbol": "PolyphasePlugin_GetDesc",
    "apiVersion": 4,
    "resolveMode": "source",
    "buildTargets": [
      { "id": "homebrew.dreamcast", "displayName": "Dreamcast (KallistiOS)", "category": "Retro Consoles" }
    ]
  }
}
```

These entries are metadata only — your `RegisterEditorUI` callback still
performs the actual registration. The engine cross-checks the two and warns
if you advertise a target id and then fail to register it.

## End-to-end minimum

```cpp
#include "Plugins/PolyphasePluginAPI.h"
#include "Plugins/PolyphaseEngineAPI.h"
#include "Plugins/EditorUIHooks.h"
#include "Plugins/PolyphaseBuildTargetAPI.h"

#include <cstdio>
#include <cstring>

static int32_t Validate(char* out, size_t cap)
{
    const char* kos = std::getenv("KOS_BASE");
    if (!kos || !*kos)
    {
        std::snprintf(out, cap, "KOS_BASE not set");
        return 0;
    }
    return 1;
}

static int32_t GetCompileCommand(const PolyphaseBuildContext* ctx,
                                 char* out, size_t cap)
{
    std::snprintf(out, cap,
                  "make -C \"%s\" -f Makefile_Dreamcast -j",
                  ctx->projectDir);
    return 1;
}

static int32_t GetCompiledBinaryPath(const PolyphaseBuildContext* ctx,
                                     char* out, size_t cap)
{
    std::snprintf(out, cap, "%s/Build/Dreamcast/main.elf", ctx->projectDir);
    return 1;
}

static int32_t PostPackage(const PolyphaseBuildContext* ctx)
{
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd),
                  "mkdcdisc -e \"%s/main.elf\" -o \"%s/game.cdi\"",
                  ctx->packageOutputDir, ctx->packageOutputDir);
    return std::system(cmd) == 0 ? 1 : 0;
}

static PolyphaseBuildTargetDesc gTarget{};

#if EDITOR
static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    gTarget.apiVersion           = POLYPHASE_BUILD_TARGET_API_VERSION;
    gTarget.targetId             = "homebrew.dreamcast";
    gTarget.displayName          = "Dreamcast (KallistiOS)";
    gTarget.category             = "Retro Consoles";
    gTarget.basePlatform         = 1;       // Platform::Linux
    gTarget.binaryExtension      = ".cdi";
    gTarget.supportsEmulator     = 1;
    gTarget.Validate             = Validate;
    gTarget.GetCompileCommand    = GetCompileCommand;
    gTarget.GetCompiledBinaryPath= GetCompiledBinaryPath;
    gTarget.PostPackage          = PostPackage;

    hooks->RegisterBuildTarget(hookId, &gTarget);
}
#endif

static int OnLoad(PolyphaseEngineAPI*) { return 0; }
static void OnUnload() {}

extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* d)
{
    d->apiVersion        = OCTAVE_PLUGIN_API_VERSION;   // >= 4 for build targets
    d->pluginName        = "com.example.dreamcast";
    d->pluginVersion     = "1.0.0";
    d->OnLoad            = OnLoad;
    d->OnUnload          = OnUnload;
    d->RegisterTypes     = nullptr;
    d->RegisterScriptFuncs = nullptr;
#if EDITOR
    d->RegisterEditorUI  = RegisterEditorUI;
#else
    d->RegisterEditorUI  = nullptr;
#endif
    d->OnEditorPreInit   = nullptr;
    d->OnEditorReady     = nullptr;
    return 0;
}
```

Drop the addon into `<Project>/Packages/com.example.dreamcast/`, restart the
editor (or trigger a hot-reload from the Addons window), and the new target
appears in the **Build Profile → Target** dropdown under "Retro Consoles".

## Checklist

- ☐ `package.json` has `native.apiVersion >= 4` and lists your target ids in `native.buildTargets`.
- ☐ Descriptor's `apiVersion = POLYPHASE_BUILD_TARGET_API_VERSION`.
- ☐ `targetId` is reverse-DNS and stable across versions.
- ☐ `basePlatform` is a sensible cook fallback.
- ☐ `binaryExtension` is set even if `PostPackage` rewrites it.
- ☐ `Validate` returns 0 with a clear reason when the SDK is missing — never crash.
- ☐ Every callback writes to its `out`/`outCmd` buffer with `snprintf` (size-checked) and returns non-zero on success.
- ☐ `PostPackage` cleans up its own temp files; the engine doesn't sweep them.
- ☐ Hot-reload tested: rebuild the addon, the target replaces itself in the dropdown without restarting the editor.

## Variant 2 — addon-provided runtime (no engine source changes)

If your addon ships an **engine runtime** for the target — i.e. PSPSDK / KOS /
nxdk implementations of `System`, `Input`, `Audio`, `Network` for the new
platform — set `platformExtensionDir` on the descriptor to an addon-relative
folder that contains:

```
<addonRoot>/<platformExtensionDir>/
├── SystemTypes_Platform.h    (required)
├── InputTypes_Platform.h     (required)
├── AudioTypes_Platform.h     (required)
└── NetworkTypes_Platform.h   (optional; falls back to "no networking")
```

Each file supplies the platform-specific halves of the engine's fork
headers (`SystemTypes.h`, `InputTypes.h`, etc.) — typedefs, struct-member
macros, platform SDK includes. The engine's fork headers detect
`#if defined(POLYPHASE_PLATFORM_ADDON)` and prefer your bridge over the
built-in `PLATFORM_*` arms.

How it wires together at build time:

1. Editor user picks your target → `ActionManager::BuildPhase1` runs.
2. The addon-dispatch branch reads `platformExtensionDir`, resolves it to
   an absolute path under your addon root, and writes four bridge files
   under `<projectDir>/Generated/`:
   - `PolyphasePlatform_SystemTypes.h` → `#include "<abs>/SystemTypes_Platform.h"`
   - `PolyphasePlatform_InputTypes.h`  → `#include "<abs>/InputTypes_Platform.h"`
   - `PolyphasePlatform_AudioTypes.h`  → `#include "<abs>/AudioTypes_Platform.h"`
   - `PolyphasePlatform_NetworkTypes.h` → `#include "<abs>/NetworkTypes_Platform.h"`
     (or a `// stub` if your addon omits the file)
3. Your project's makefile must:
   - Define `POLYPHASE_PLATFORM_ADDON=1`.
   - Add `Generated/` and your addon's `Runtime/<platform>/` to the
     include path.

The result: the engine compiles for your new platform without anyone
ever editing engine source. Your `Runtime/<platform>/` files own every
SDK reference; the engine remains license-clean.

### Struct-member injection

For structs the engine forks *inside* (e.g. `SystemState`, `DirEntry`),
inject your platform's members via these optional macros, defined in
your `SystemTypes_Platform.h`:

```cpp
// Adds PSP-specific fields to the engine's SystemState struct
#define POLYPHASE_PLATFORM_ADDON_SYSTEMSTATE_MEMBERS \
    uint32_t mGuListId = 0; \
    SceUID mDisplayThread = -1;

// Adds PSP-specific fields to the engine's DirEntry struct
#define POLYPHASE_PLATFORM_ADDON_DIRENTRY_MEMBERS \
    SceUID mDirHandle = -1;

// If your platform's thread-entry functions return `void` (not `int`),
// also #define this so the engine's THREAD_RETURN expands to `return;`
//   #define POLYPHASE_PLATFORM_ADDON_VOID_THREAD_RETURN
```

Reference implementation: `Packages/com.polyphase.build.target.psp/Runtime/PSP/`.

### Lessons from the PSP runtime port

The PSPGU renderer port turned up a number of patterns that generalise to any
new fixed-function platform. Treat these as a checklist when implementing
`Graphics_<Platform>.cpp` / `System_<Platform>.cpp`:

#### Graphics state and matrix path

- **Pick raw API over utility libs for matrix work.** Most fixed-function
  SDKs ship a "Gum"-style matrix utility on top of their raw API (`sceGum*`
  vs `sceGu*` on PSP, similar split on other consoles). The utility libs
  add CPU-side state that can interact badly with the engine's per-frame
  state churn. **Always use the raw API for projection / view / model /
  texture matrix uploads.** Test by writing the utility path first; if 3D
  primitives mis-render in unexplainable ways, drop to raw immediately.
  
  Concretely on PSP: `sceGuSetMatrix(GU_PROJECTION/VIEW/MODEL/TEXTURE,
  &ScePspFMatrix4)` works; `sceGumLoadMatrix` etc. retroactively corrupted
  already-issued draws.

- **glm::mat4 layout often matches the platform type.** Column-major,
  16 floats — identical to `ScePspFMatrix4`, `C3D_Mtx`, and most others.
  A direct `memcpy(&dst, &glm[0][0], 64)` works; per-element conversion is
  rarely necessary.

- **Mandatory state for 3D rasterizers.** Some platforms gate the 3D
  pipeline on specific state being enabled — on PSP, disabling
  `GU_CLIP_PLANES` silently drops every 3D primitive even when no user
  clip planes are defined. When porting, mirror the platform's canonical
  sample (e.g. PSPSDK `cube.c`) state enables exactly, then disable
  individual bits only after verifying basic 3D rendering works.

- **Hardcode platform screen dims in `GFX_SetViewport` / `GFX_SetScissor`.**
  Engine `Renderer` passes the editor's scene-tab viewport, which on a
  fixed-resolution handheld won't match the native 480×272 / 400×240 / etc.
  Without hardcoding, the right portion of the screen gets scissor-clipped.

#### Cache coherency

Most consoles have a CPU dcache that the GPU/GE bypasses via DMA. Without
explicit flushes, the GE reads stale or zero data:

- **Vertex buffers**: flush in `GFX_CreateStaticMeshResource` (and any
  update path).
- **Index buffers**: same.
- **Texture pixels**: flush in `GFX_CreateTextureResource` AND again in
  `BindTexture` if the texture data might have been modified.
- **Matrix data** that the GE pulls via DMA: same.

On PSP: `sceKernelDcacheWritebackRange(ptr, bytes)`. On other platforms,
look for the equivalent (`GSPGPU_FlushDataCache`, `DCFlushRange`, etc.).

#### Engine vertex layout vs hardware-mandated layout

Engine `Vertex` is `(pos, uv0, uv1, normal)`. Most fixed-function GPUs
mandate a specific field order in the vertex stream (PSP: `tex → color →
normal → pos`). You cannot pass engine vertex data straight to the GE —
implement a `RepackVertices` helper that converts engine layout to
hardware layout at `CreateStaticMeshResource` time. Eventually this moves
to cook time (Phase 6) but Phase 2 can repack at runtime.

The vertex-flag mask in the draw call MUST exactly match the struct
layout. If the mask claims `NORMAL_32BITF` but the struct doesn't reserve
12 bytes for normal in the correct position, every subsequent field
offsets wrong and the draw fails silently.

#### Texture state — set everything explicitly

When binding a texture, set the entire state every time, even if it looks
redundant:

```cpp
sceGuTexMode(format, /*maxMips=*/0, /*a2=*/0, swizzled);
sceGuTexImage(0, w, h, bufW, pixels);
sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB);  // RGB not RGBA — see below
sceGuTexFilter(GU_LINEAR, GU_LINEAR);
sceGuTexWrap(GU_REPEAT, GU_REPEAT);
sceGuTexScale(1.0f, 1.0f);                  // defaults may be stale
sceGuTexOffset(0.0f, 0.0f);
sceGuTexEnvColor(0xFFFFFFFFu);
sceGuSetMatrix(GU_TEXTURE, &identity);      // texture-coord matrix slot
sceGuEnable(GU_TEXTURE_2D);
```

`GU_TCC_RGB` (not `GU_TCC_RGBA`) is the right default — using RGBA makes
any 0-alpha pixels in the texture invisible.

#### Build system constraints

- **The SDK's makefile may not generate `.d` dep files** — PSPSDK doesn't.
  Header-only changes won't invalidate stale `.o` files. After modifying
  any engine header that affects struct sizes (e.g. resource structs in
  `GraphicsTypes.h`), do `rm -f *.o` in the project root before rebuilding,
  or you get ABI-skew crashes (`Factory_<Type>::Create` calling `memcpy`
  to ~null).
- **The SDK's makefile may not track `Makefile_*` itself as a link dep**.
  After changing `LIBS = ...` or other link variables, delete the staged
  ELF (`rm <project>.elf`) before rebuilding.
- **Link the VFPU/SIMD-accelerated library variant** when available.
  PSPSDK: `-lpspgum_vfpu -lpspvfpu -lpspgu` not `-lpspgum`. The official
  samples link the VFPU variant, which is the actually-tested path.

#### Phased implementation strategy

Implement `Graphics_<Platform>.cpp` in stages with stubs:

| Phase | Scope |
| ----- | ----- |
| 1 | Init / shutdown / clear / swap. Engine boots, ticks, exits cleanly. Black screen expected. |
| 2 | Static-mesh draw (one mesh visible, textured). |
| 3 | Lighting / shading / skeletal / translucency. |
| 4 | UI / quads / text. |
| 5 | Audio + Input. |
| 6 | Polish (cook-time swizzle, VRAM allocator, perf). |

Stub everything outside the current phase to no-ops so the build compiles
and links incrementally. The PSP addon's `Graphics_PSPGU.cpp` is a worked
reference for this layout.

#### Logging in early boot

Write to **two** sinks from the boot path:

1. `stdout` (visible in PPSSPP / emulator host log immediately)
2. A file on the platform's writable storage (e.g. `ms0:/PSP/GAME/<id>/<name>.log`)

The platform's writable filesystem may take seconds to mount — without
stdout you lose pre-init crash output. Truncate the file on each boot so
each session starts fresh. Chain `Mkdir` calls for nested parent dirs
since most platforms' `mkdir` is single-level.

## Reference files

| Path                                                              | What it contains                                        |
| ----------------------------------------------------------------- | ------------------------------------------------------- |
| `Engine/Source/Plugins/PolyphaseBuildTargetAPI.h`                 | Descriptor + context structs. Source of truth.          |
| `Engine/Source/Plugins/EditorUIHooks.h`                           | `RegisterBuildTarget` hook signature.                   |
| `Engine/Source/Editor/Packaging/BuildTargetRegistry.h` / `.cpp`   | Registry implementation; deep copy + hot-reload.        |
| `Engine/Source/Editor/Packaging/BuiltInBuildTargets.h` / `.cpp`   | How the engine registers its own six built-in targets.  |
| `Engine/Source/Editor/ActionManager.cpp`                          | Phase 1 cook hook, addon-target dispatch, post-package. |
| `Engine/Source/Editor/Packaging/PackagingWindow.cpp`              | Dropdown UI, "Target Options" panel.                    |
