---
name: polyphase-buildtarget
description: Author a Polyphase build-target addon — a native DLL that adds a new packaging platform (Dreamcast, PS2, original Xbox, Xbox 360, NDS, custom hardware) to the editor without touching engine source. Triggers on requests like "add a Dreamcast build target", "make a custom platform addon", "ship a PS2 packager", "add a build target for X", or anything that wants the editor to package + compile + run for a platform the engine doesn't ship a built-in for.
---

# Polyphase Build-Target Addon Skill

A **build-target addon** is a special kind of native Polyphase addon: instead
of adding nodes / assets / Lua bindings, it registers a
`PolyphaseBuildTargetDesc` with the editor that teaches the build pipeline
how to compile, cook, package, and launch a project for a new platform — all
without modifying engine source. The engine ships only the framework + six
built-in targets (Windows / Linux / Android / GameCube / Wii / 3DS); every
other platform lives in an addon DLL that brings its own SDK code and license.

| Resource                                                                                            | What it covers                                                          |
| --------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| `Documentation/Development/CustomBuildTarget.md`                                                    | Full developer guide for build-target addons. Source of truth.          |
| `Engine/Source/Plugins/PolyphaseBuildTargetAPI.h`                                                   | `PolyphaseBuildTargetDesc` + `PolyphaseBuildContext`. ABI definition.   |
| `Engine/Source/Plugins/EditorUIHooks.h`                                                             | `RegisterBuildTarget` / `UnregisterBuildTarget` hook signatures.        |
| `Engine/Source/Editor/Packaging/BuildTargetRegistry.h` / `.cpp`                                     | Registry behaviour, deep-copy + hot-reload cleanup.                     |
| `Engine/Source/Editor/Packaging/BuiltInBuildTargets.cpp`                                            | Reference: how the engine registers its own six targets.                |
| `Engine/Source/Editor/ActionManager.cpp` (~lines 940, 1258, 1828, 2640)                             | Phase 1 cook hook, addon-target compile dispatch, PostPackage, run.     |
| `M:\Projects\Polyphase\Addons\BuildTargets\BuildTarget-DevEnv\Packages\com.polyphase.build.target.dreamcast\` | Reference addon — Dreamcast via KallistiOS + mkdcdisc.        |

When the headers disagree with this skill, **the headers win**. Always read
`POLYPHASE_BUILD_TARGET_API_VERSION` from the live header and copy it into
your descriptor verbatim — the engine rejects descriptors with the wrong
version.

## When to use this skill (vs. the others)

- Use **this skill** when the user wants the editor to *produce a build* for
  a platform the engine doesn't ship a built-in for: Dreamcast, PS2, original
  Xbox via nxdk, Xbox 360, NDS, Saturn, embedded boards, etc.
- Use **`polyphase-addon`** when the user wants to register custom Node /
  Asset / GraphNode types, expose Lua bindings, add editor menus or panels.
- Use **`polyphase`** when the user wants to edit *engine source* (i.e.
  modify how Windows / Linux / 3DS itself builds).

A single addon DLL can do both — register types **and** register a build
target — by combining `polyphase-addon` and this skill.

## Mental model — the six things that matter

1. **Registration happens inside `RegisterEditorUI(hooks, hookId)`**, not
   `OnLoad`. The hookId is what scopes auto-cleanup on hot-reload. Calling
   `RegisterBuildTarget` from `OnLoad` will register but never unregister.

2. **The descriptor is deep-copied at registration.** Every `const char*` in
   the descriptor is duplicated into an owning `std::string` inside the
   registry. Your descriptor's string literals can live in static memory that
   disappears when the DLL is unloaded — the registry survives that.

3. **Function pointers in the descriptor stay bound to your DLL.** Only the
   strings are owned by the registry. When your DLL unloads, the engine
   wipes the whole entry; it never invokes a stale callback.

4. **`basePlatform` is your cook-compat anchor.** Pick the built-in
   `Platform` enum whose default asset cook (`Asset::SaveStream`) produces
   bytes your hardware can ingest. For Unix-ish toolchains pick
   `Platform::Linux`. For nxdk pick `Platform::Windows`. Override per-asset
   with `CookAsset` only when default bytes don't fit (PowerVR2 twiddle, GS
   palettes, NDS tiles, swizzled DXT, etc.).

5. **`GetCompileCommand` is the only required callback.** Everything else is
   optional. Build a single shell command line into the output buffer and
   return non-zero. The engine runs it with `SYS_ExecFull` from the project
   directory and streams stdout/stderr into the Packaging window.

6. **`PostPackage` runs after the compiled binary is in
   `packageOutputDir`.** Use it to wrap into a console-native image (CDI,
   ISO, CIA, NDS-rom, XBE…). The wrapping tool's input is the file the engine
   just copied; its output goes wherever you want — typically next to it.

## Quickstart — one-shot a new build target

### Step 1 — pick an id and base platform

| Decision         | Convention                                                                |
| ---------------- | ------------------------------------------------------------------------- |
| `targetId`       | reverse-DNS, lowercase: `homebrew.dreamcast`, `homebrew.ps2`, `xbox.nxdk` |
| `category`       | "Retro Consoles", "Handheld", "Mobile", "Embedded", whatever groups well   |
| `basePlatform`   | the closest built-in (`Platform::Linux` is usually right)                  |
| `binaryExtension`| post-wrap if you have a wrapper (`.cdi` for Dreamcast), else `.elf`        |

### Step 2 — scaffold the package

Place the addon under `<Project>/Packages/<your.target.id>/` (or
under a dedicated `BuildTarget-DevEnv/Packages/` workspace if you're shipping
the addon as a separate repo). Use the same layout as any
`polyphase-addon` package: `package.json`, `Source/<Name>.cpp`,
`CMakeLists.txt`, optional `<Name>.vcxproj`, `build.bat`, `build.sh`.

The **only** non-standard bit is the `native.buildTargets` array in
`package.json`:

```json
{
    "name": "com.example.dreamcast",
    "version": "1.0.0",
    "native": {
        "target": "editor",
        "sourceDir": "Source",
        "binaryName": "com.example.dreamcast",
        "entrySymbol": "PolyphasePlugin_GetDesc",
        "apiVersion": 4,
        "resolveMode": "source",
        "buildTargets": [
            {
                "id": "homebrew.dreamcast",
                "displayName": "Dreamcast (KallistiOS)",
                "category": "Retro Consoles"
            }
        ]
    }
}
```

`native.apiVersion` must be **>= 4**. Earlier versions don't have
`RegisterBuildTarget` and your registration call will hit a null function
pointer.

### Step 3 — write the descriptor + callbacks

The minimum-viable shape is in `Documentation/Development/CustomBuildTarget.md`
("End-to-end minimum"). Steal that, rename, and fill in the four callbacks
your platform needs:

| Callback                  | What you write                                                     |
| ------------------------- | ------------------------------------------------------------------ |
| `Validate`                | env-var / file-existence check that the SDK is reachable.          |
| `GetCompileCommand`       | one shell command line that invokes your toolchain on the project. |
| `GetCompiledBinaryPath`   | absolute path to the linker output. Defaults to project/Build/Target/<name><ext>. |
| `PostPackage`             | wrap the binary into a console-native image (if applicable).       |

Optional but high-value: `CookAsset` (for native texture/audio formats),
`RunInEmulator` + `RunOnDevice` (to wire up the editor's Build & Run buttons),
`DrawProfileOptions` (to expose region/disc-format settings).

### Step 4 — register on load

Inside `RegisterEditorUI`:

```cpp
#if EDITOR
static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    static PolyphaseBuildTargetDesc gTarget{};
    gTarget.apiVersion           = POLYPHASE_BUILD_TARGET_API_VERSION;
    gTarget.targetId             = "homebrew.dreamcast";
    gTarget.displayName          = "Dreamcast (KallistiOS)";
    gTarget.category             = "Retro Consoles";
    gTarget.basePlatform         = 1;       // Platform::Linux
    gTarget.binaryExtension      = ".cdi";
    gTarget.supportsEmulator     = 1;
    gTarget.Validate             = Dreamcast_Validate;
    gTarget.GetCompileCommand    = Dreamcast_GetCompileCommand;
    gTarget.GetCompiledBinaryPath= Dreamcast_GetCompiledBinaryPath;
    gTarget.PostPackage          = Dreamcast_PostPackage;
    gTarget.RunInEmulator        = Dreamcast_RunInEmulator;

    if (hooks->RegisterBuildTarget != nullptr)
    {
        hooks->RegisterBuildTarget(hookId, &gTarget);
    }
}
#endif
```

Null-check `hooks->RegisterBuildTarget` defensively — older engine binaries
(pre-API-v4) leave it null and your addon should degrade gracefully.

### Step 5 — verify

1. Drop the addon into `<Project>/Packages/`.
2. Open the project. The Addons window shows your addon; if it surfaces
   "advertised target id never registered" warnings, your
   `RegisterBuildTarget` call didn't fire.
3. **File → Build Profiles**. Under the target dropdown you should see your
   target inside its category, marked `[addon]`. Hovering shows
   `Validate`'s reason if the SDK isn't present.
4. Pick it, click **Build**. Engine cooks → calls your
   `GetCompileCommand` → runs it via `SYS_ExecFull` → calls
   `GetCompiledBinaryPath` → copies to `Packaged/<id>/` → calls
   `PostPackage` → done.

## Patterns and gotchas

### Buffer discipline

All callbacks that return a string into a caller-provided buffer (`Validate`,
`GetCompileCommand`, `GetCompiledBinaryPath`, `RunOnDevice`, `RunInEmulator`)
get `(char* out, size_t cap)`. Use `snprintf(out, cap, ...)` (never `sprintf`,
never `strcpy`). Return non-zero on success, zero on failure. The engine
treats zero as a build failure and surfaces it in the build log.

### Context lifetime

`PolyphaseBuildContext*` and every string field inside it are valid **only**
for the duration of one callback invocation. Copy out `projectDir`,
`packageOutputDir`, etc. into your own `std::string` if you need them past
the call. Same for `userData` if you assigned it during `PreCook` — store it
in addon-side static state, not on the context.

### Hot-reload behaviour

The descriptor is registered with the `hookId` parameter you receive in
`RegisterEditorUI`. When the editor hot-reloads your addon:

1. Editor calls your `OnUnload`.
2. Editor calls `RemoveAllHooks(hookId)` — your target leaves the registry.
3. Editor `FreeLibrary`s your DLL.
4. Editor rebuilds your DLL, `LoadLibrary`s it.
5. New `RegisterEditorUI` runs and re-registers the target.

If your descriptor's strings live in a *function-local static* (`static
PolyphaseBuildTargetDesc gTarget` inside `RegisterEditorUI`) they're fine — the
registry deep-copies on entry. If your strings are computed and live in
addon-side heap, free them on `OnUnload` so you don't leak across reloads.

### Per-profile options (region, BIOS, disc format)

Use `DrawProfileOptions(void* profilePtr)` to draw ImGui controls inside the
Packaging panel's auto-rendered "Target Options" header. Persist values
through `BuildProfile::mTargetOptions` — a flat `std::unordered_map<string,
string>`. Read them back from build callbacks via
`ctx->GetProfileSetting("key", buf, sizeof(buf))`.

```cpp
static void Dreamcast_DrawProfileOptions(void* profilePtr)
{
    auto* profile = static_cast<BuildProfile*>(profilePtr);
    auto it = profile->mTargetOptions.find("region");
    static const char* regions[] = { "NTSC-U", "NTSC-J", "PAL" };
    int sel = 0;
    if (it != profile->mTargetOptions.end()) {
        for (int i = 0; i < 3; ++i) if (it->second == regions[i]) sel = i;
    }
    if (ImGui::Combo("Region", &sel, regions, 3))
    {
        profile->mTargetOptions["region"] = regions[sel];
    }
}
```

### CookAsset override — when and how

`basePlatform` is enough for ~80% of homebrew targets. Override only when
your hardware needs a format the engine doesn't produce. Branch on
`assetTypeName`:

```cpp
static int32_t Dreamcast_CookAsset(const PolyphaseBuildContext* ctx,
                                   const char* assetTypeName,
                                   void* assetPtr, void* streamPtr)
{
    if (strcmp(assetTypeName, "Texture") == 0)
    {
        auto* tex = static_cast<Texture*>(assetPtr);
        auto* stream = static_cast<Stream*>(streamPtr);
        return WritePvr2TwiddledTexture(tex, *stream) ? 1 : 0;
    }
    return 0;       // fall back to default basePlatform cook
}
```

`assetTypeName` is whatever `Object::RuntimeName()` returns — match the
exact class name (e.g. `"Texture"`, `"SoundWave"`, `"StaticMesh"`).

### Licensing isolation

This is the **whole point** of the framework. Keep SDK-specific code 100%
inside your addon DLL:

- ✅ KallistiOS / PS2SDK / nxdk / libnds / XDK / Xbox 360 XDK headers and
  libs go in your addon's `External/<sdk>/` and link statically into your
  addon DLL.
- ❌ Never `#include` an SDK header from the engine. Never add SDK libs to
  the engine's link line. The engine binary's license is *your* problem to
  preserve.

`grep -r '<sdk-token>' Engine/Source/` after your work — it should return
zero hits.

## Gotchas from real ports

These are paid-in-blood findings from completing the PSP port. Most apply to
any new fixed-function or fixed-pipeline platform, not just PSP.

### Build system

- **Verify the SDK's Makefile actually tracks header deps.** PSPSDK's
  `build.mak` emits **no `.d` files** in this distribution — header changes
  don't invalidate stale `.o` files. After editing any engine header that
  affects struct layout (e.g. resource structs in `GraphicsTypes.h`), `rm -f
  *.o` in the project root before rebuilding, or you'll get ABI-skew crashes
  inside `Factory_<Type>::Create` calling `memcpy` to a ~null destination.
- **Beware addon-copy drift.** If your addon ships from a separate workspace
  (e.g. `BuildTarget-DevEnv/Packages/...`) AND a per-target test project
  (`BuildTarget-PSP/Packages/...`), edits to one need to sync to the other.
  Diagnose by `diff -q` between the two `Runtime/<platform>/` trees if a
  rebuild seems to ignore your change.
- **`Makefile_PSP` (or equivalent) isn't tracked as a link dep**, so changing
  `LIBS = ...` doesn't trigger a relink. After a libs change, delete the
  staged ELF (`rm <project>.elf`) before rebuilding.

### Graphics — choosing the right matrix path

- **Don't mix matrix utility libs with the raw API.** On PSP, `sceGum*`
  retroactively corrupts already-issued 3D draws even when called *after*
  the draw — even when no `sceGumDrawArray` is invoked. Use only the raw
  `sceGuSetMatrix` / `sceGuDrawArray` path. (See `project_psp_pspgum_breaks_state`
  memory.) Test the equivalent on your platform: write the matrix-utility
  path first, and if 3D primitives mis-render in surprising ways, drop to
  the raw API.
- **Link the VFPU/SIMD-accelerated lib variant** when the platform has one.
  PSPSDK ships `libpspgum.a` (FPU) and `libpspgum_vfpu.a` (VFPU) — the VFPU
  one is what all official samples link and is the actually-tested path. Pair
  with `-lpspvfpu` for VFPU context.
- **glm::mat4 and platform `*Matrix4` types often share layout** — both
  16-float column-major on GL-style platforms. A direct `memcpy(&dst,
  &src[0][0], 64)` works; no per-element conversion needed.

### Graphics — display + per-frame setup

- **Trust the SDK's buffer-swap state machine** unless you have proof it's
  broken. Manually re-emitting `sceGuDrawBuffer` / equivalent each frame
  caused alternating-buffer flicker because the SDK was already tracking
  swap state internally.
- **The engine's `GFX_SetViewport` / `GFX_SetScissor` may pass non-platform
  dimensions** — `Renderer` propagates the editor's scene-tab viewport,
  which on a fixed handheld may not match the platform's hard-coded screen
  resolution. **Hardcode the platform's physical screen dims** in these
  functions; don't trust the engine inputs.
- **Some platforms require certain clipping state always enabled.** On PSP,
  `GU_CLIP_PLANES` disabled silently drops every 3D primitive (not just
  user clip-plane ones). When in doubt, mirror the canonical sample's init
  sequence exactly.

### Graphics — resource lifecycle + cache coherency

- **Flush CPU dcache before the GPU/GE reads a resource.** Platforms where
  the GE reads RAM via DMA (PSP, GameCube, others) bypass CPU caches.
  Call `sceKernelDcacheWritebackRange` (or equivalent) on:
  - Vertex / index buffers after writing (in `CreateStaticMeshResource`)
  - Texture pixels after writing (in `CreateTextureResource` AND again in
    `BindTexture` if the resource was modified)
  - Any matrix or uniform data the GE will consume
- **Verify per-vertex stride alignment.** Engine `Vertex` types are usually
  4-byte-aligned (8/12/16/32 byte natural), so packed structs are fine.
  But mixed sizes (e.g. `uint32_t color + 3×int16 = 10 bytes`) may need
  explicit padding to a natural alignment boundary; check by drawing a
  triangle with N=3 vertices and inspecting whether all three rasterize
  correctly.

### Graphics — engine vertex layout vs platform HW layout

- **The engine repacks vertices for fixed-function GPUs that mandate field
  order.** Engine `Vertex` is `(pos, uv0, uv1, normal)`; PSP HW demands
  `(tex, color, normal, pos)` — you cannot just `memcpy` engine vertex data
  into the GE's expected slot. Implement a `RepackVertices` helper in your
  addon that converts engine layout to HW layout once at create time
  (Phase 2-style — eventually move to cook time for performance).
- **Vertex flag mask MUST match struct layout exactly.** If you set
  `GU_NORMAL_32BITF` in the mask but the struct doesn't have 12 bytes for
  normal after texture, every subsequent field offsets wrong → no draw.

### Graphics — texture sampling state

- **Always set the full texture state explicitly per bind**, even if it
  looks redundant. PSP defaults for `sceGuTexScale`/`Offset`/`EnvColor` can
  get left in unexpected states by other engine calls. Five lines of
  redundant set-up is cheaper than chasing "right side of texture vanishes"
  bugs.
- **Force the texture matrix slot to identity.** PSP has 4 matrix slots
  (proj/view/model/**texture**). The texture matrix transforms UVs; if
  something left it non-identity, UVs sample off the visible texture region.
- **Use `TCC_RGB` not `TCC_RGBA`** for the texture-color-component mode
  unless you genuinely need texture alpha. With RGBA, any 0-alpha pixels
  in the texture (image borders, padding) become invisible.

## Authoring a Variant 2 platform runtime

If the addon ships an engine runtime (`System`, `Input`, `Audio`, `Network`,
`Graphics` for the new platform), follow these patterns in addition to the
descriptor work:

- **Place platform-extension headers** at the path `platformExtensionDir`
  points at. The engine writes `Generated/PolyphasePlatform_*.h` bridge
  files that `#include` yours at compile time.
- **Inject struct members** via the documented macros:
  - `POLYPHASE_PLATFORM_ADDON_SYSTEMSTATE_MEMBERS`
  - `POLYPHASE_PLATFORM_ADDON_DIRENTRY_MEMBERS`
  - `POLYPHASE_PLATFORM_ADDON_VOID_THREAD_RETURN` (if your thread fn returns `void`)
- **`Runtime/<platform>/` owns all SDK references** — never `#include` an
  SDK header from engine source.
- **Logging from very early boot** — write to two sinks: stdout (visible in
  emulator host log) AND a file on the platform's writable media (e.g.
  `ms0:/PSP/GAME/<id>/<name>.log`). The platform may take seconds to
  set up the writable filesystem; without stdout you lose pre-init crashes.
- **Re-write `GFX_SetViewport` / `GFX_SetScissor` to hardcode physical screen
  dims** for fixed-resolution platforms (handhelds). Engine input here
  comes from editor window state and won't match.
- **Phase your `GFX_*` work** as Phase 2 / Phase 3 / Phase 4 / etc. (see the
  PSP plan template). Stub everything not in the current phase to a no-op
  so you can compile-link-boot incrementally.

## Reference addons

| Addon                                                                                                    | Coverage                                                                |
| -------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| `…/Packages/com.polyphase.build.target.dreamcast/`                                                       | KallistiOS / kos-cc / mkdcdisc / lxdream + PVR2 cook hook + region opt. |
| `…/Packages/com.polyphase.build.target.psp/`                                                             | **PSPSDK + WSL routing + PSPGU runtime (Phase 2 complete). Read this for any Variant-2 addon with a full runtime.** |

Most new targets are a structural copy of one of these with the SDK-specific
bits swapped out.

## Checklist for a one-shot

- ☐ Resolved `POLYPHASE_PATH` (env / `C:\Polyphase` / `/opt/Polyphase` / walk-up for `PolyphaseConfig.cmake`).
- ☐ Read the live `PolyphaseBuildTargetAPI.h` and copied `POLYPHASE_BUILD_TARGET_API_VERSION` verbatim.
- ☐ `package.json` has `native.apiVersion >= 4` and a `native.buildTargets` entry matching the descriptor's `targetId`.
- ☐ Descriptor registered inside `RegisterEditorUI`, not `OnLoad`.
- ☐ Null-checked `hooks->RegisterBuildTarget` (older engines don't have it).
- ☐ `GetCompileCommand` uses `snprintf(out, cap, ...)`, returns 1 on success.
- ☐ `Validate` returns 0 with a *user-readable* reason on missing SDK, never crashes.
- ☐ `PostPackage` (if present) cleans its own temp files.
- ☐ Zero SDK references in `Engine/Source/`.
- ☐ Built the addon (`build.bat` / CMake) and verified it appears in the Build Profile dropdown.
