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
  *.o` in the project's intermediate dir before rebuilding, or you'll get
  ABI-skew crashes inside `Factory_<Type>::Create` calling `memcpy` to a
  ~null destination.
- **Beware addon-copy drift.** If your addon ships from a separate workspace
  (e.g. `BuildTarget-DevEnv/Packages/...`) AND a per-target test project
  (`BuildTarget-PSP/Packages/...`), edits to one need to sync to the other.
  Diagnose by `diff -q` between the two `Runtime/<platform>/` trees if a
  rebuild seems to ignore your change.
- **`Makefile_PSP` (or equivalent) isn't tracked as a link dep**, so changing
  `LIBS = ...` doesn't trigger a relink. After a libs change, delete the
  staged ELF (`rm <project>.elf`) before rebuilding.
- **Build out-of-tree** — `mkdir -p <projectDir>/Intermediate/<Platform> && cd`
  into it before invoking make, then pass `PROJECT_ROOT=<projectDir>` so the
  Makefile resolves all sources/includes/outputs through that variable. Keeps
  `.o`/`.d`/`.elf` from polluting the project root (which would otherwise
  trivially `git add .` into a repo). The final binary still stages out to
  `<projectDir>/Build/<Platform>/<name>.elf` via a `stage:` target so the
  engine's `GetCompiledBinaryPath` works unchanged. PSPSDK's `build.mak`
  emits objects to `$(CURDIR)` — running make from a non-project dir is how
  you redirect the dropzone.
- **Trailing-slash trap on `PROJECT_ROOT` (cost a debug session).** If
  `ctx->projectDir` ends in `/` (it usually does), and you pass it as a make
  variable, then `TARGET = $(notdir $(PROJECT_ROOT))` returns **empty** — GNU
  make's `notdir` splits on the final `/` and finds nothing after it. Result:
  link target becomes `.elf` (filename literally starts with a dot), build
  silently succeeds with a 10 MB orphan, and the engine's post-build check
  reports "invalid executable size=0" with no make error in sight. Normalise
  at the top of the Makefile:
  ```makefile
  override PROJECT_ROOT  := $(patsubst %/,%,$(PROJECT_ROOT))
  override POLYPHASE_PATH := $(patsubst %/,%,$(POLYPHASE_PATH))
  ```
- **`override` is REQUIRED for command-line variables.** GNU make precedence:
  command-line `make VAR=val` beats plain `:=` reassignment in the Makefile
  body. So `PROJECT_ROOT := $(patsubst ...)` is **silently ignored** when the
  caller set `PROJECT_ROOT` on the make line. Without `override` the
  normalisation above becomes a no-op and the trailing slash survives. Debug:
  `make -p -n ... 2>&1 | grep '^PROJECT_ROOT'` prints what make actually
  resolved.
- **Make `PROJECT_ROOT` resolution robust** so the Makefile works under both
  old-style `make -C <projectDir>` AND new-style
  `make -C <projectDir>/Intermediate/<Platform> PROJECT_ROOT=...` invocations.
  Walk-up search finds the `.octp` marker:
  ```makefile
  ifeq ($(origin PROJECT_ROOT), undefined)
  PROJECT_ROOT := $(strip \
      $(if $(wildcard $(CURDIR)/*.octp),       $(CURDIR), \
      $(if $(wildcard $(CURDIR)/../*.octp),    $(abspath $(CURDIR)/..), \
      $(if $(wildcard $(CURDIR)/../../*.octp), $(abspath $(CURDIR)/../..), ))))
  endif
  ```
  Lets the addon DLL get updated independently of the Makefile and the build
  still works either way.
- **Memory budget on `make -j`.** psp-gcc TUs peak at 1–2 GB each compiling
  the engine's larger files. `make -j8` on a 16 GB host OOM-kills the
  compiler silently and leaves a partial `.o`. Default to `-j4` and expose a
  per-profile slider for hosts with more RAM. Other consoles have similar
  ceilings — measure before you choose a default.

### Engine integration — fixes the engine itself may need

Every new console addon may surface engine assumptions that only triggered
on built-in platforms. Catalogue what you patched in the engine so the next
addon doesn't rediscover:

- **`Engine/Source/Input/InputConstants.h` needs an `#elif PLATFORM_FOO` arm
  OR a default `#else` fallback** — if neither matches, the four
  `INPUT_*_SUPPORT` macros stay **undefined** (false). Then `#if
  INPUT_GAMEPAD_SUPPORT` in `InputUtils::InputAdvanceFrame` evaluates
  false, the `memcpy(mPrevGamepads, mGamepads, ...)` is compiled out,
  `mPrevGamepads` stays all-zeros, and `IsGamepadButtonJustDown` returns
  TRUE every frame the button is held. Symptom: Button widget nav cycles
  through every entry per single d-pad press. The engine now has an `#else`
  fallback (gamepad-only console default) so this won't bite the next addon
  unless the new platform needs keyboard/mouse/touch — in which case add an
  explicit arm.
- **Force window size in BOTH `OctPreInitialize` AND `OctPostInitialize`**
  for any fixed-resolution console. The boot flow is:
  ```
  OctPreInitialize(config)   ← your override goes here (belt)
  ReadEngineConfig()         ← reloads Config.ini, CLOBBERS your override
  Initialize()               ← copies config → EngineState
  OctPostInitialize()        ← re-override EngineState directly (suspenders)
  ```
  `EngineConfig::mWindowWidth/Height` defaults to **1280×720** (not zero), so
  the `if (config.mWindowWidth == 0)` idiom never fires. Set unconditionally.
  `Renderer::GetViewportWidth/Height` reads `EngineState` live every frame,
  so the `OctPostInitialize` re-override fixes widget sizing on the first
  `Update()`.
- **Have `PostPackage` rewrite `Config.ini`** for fixed-resolution consoles.
  The engine packager copies the project's `Config.ini` verbatim into the
  output dir (two copies: root + `<projectName>/`) and `Config.ini` carries
  the project's *desktop* `WindowWidth`/`Height` because that's what the
  editor was running at. The Oct hooks above catch this at runtime, but
  rewriting the packaged file at package time is the cleaner fix:
  ```cpp
  void ForceFixedWindowSizeInConfig(const std::string& path, int w, int h) {
      // read line-by-line, replace WindowWidth=/WindowHeight= if present,
      // append if absent, write back
  }
  ```
- **Embedded-assets vs embedded-scripts memory budgeting.** The editor
  generates `Generated/EmbeddedAssets.cpp` (large — tens of MB of cooked
  asset bytes) and `Generated/EmbeddedScripts.cpp` (small — Lua source
  text). On consoles with tight RAM (PSP: 32 MB total), **pull only
  `EmbeddedScripts.cpp`** into the executable — assets get loaded from
  removable storage at runtime via `AssetManager::Discover`. Wire it via
  `config.mEmbeddedScripts = gEmbeddedScripts; config.mEmbeddedScriptCount
  = gNumEmbeddedScripts;` in `OctPreInitialize`.

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
- **GE vertex pointers are async-consumed; per-draw transforms need a
  frame-scoped ring buffer, NOT a shared scratch.** `sceGuDrawArray`
  (and equivalents on PS2, Saturn, etc.) records the **vertex pointer** into
  the GE command list and returns immediately. The GE processes commands
  asynchronously, only finishing between `sceGuFinish`/`sceGuSync` at end-
  of-frame. So every vertex buffer pointer handed to DrawArray must remain
  valid (and contain the correct data) until end-of-frame sync. A single
  shared "scratch buffer" that's overwritten between draws means every
  command in the GE list points to the SAME memory — by the time the GE
  processes draw #1, the scratch has whatever draw #N's writer left. Use a
  ring buffer reset in `GFX_BeginFrame` (right after `sceGuStart`):
  ```cpp
  static void*    sUIRing       = nullptr;
  static uint32_t sUIRingCap    = 0;
  static uint32_t sUIRingOffset = 0;

  void* GetUIScratch(uint32_t bytes) {
      // 16-byte align each slice for DMA
      const uint32_t aligned = (bytes + 15u) & ~15u;
      if (!sUIRing || sUIRingOffset + aligned > sUIRingCap) {
          // grow + reset (safe — only happens before any GE work this frame)
      }
      void* slice = (uint8_t*)sUIRing + sUIRingOffset;
      sUIRingOffset += aligned;
      return slice;
  }
  void ResetUIScratch() { sUIRingOffset = 0; }  // called from GFX_BeginFrame
  ```
  Symptom of the broken-scratch version: UI widgets rendering with vertices
  that belong to a *different* widget — e.g. Button quads rendering with
  vertex extents that match the last-rendered Text widget's local-space
  cursor coords, so the quad appears "anchored at top with bottom empty."
  Resist the urge to fix by writing transformed vertices back into the
  widget's persistent resource buffer — the next dirty cycle will re-apply
  the transform on already-transformed data, doubling it.

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
- **Honour the asset's filter/wrap settings.** Don't hardcode
  `GU_LINEAR`/`GU_REPEAT` in `BindTexture` — read `tex->GetFilterType()` and
  `tex->GetWrapMode()` and map them. Pixel-art widgets (4×4 calibration
  patterns, sprite fonts) ship `FilterType::Nearest` and render as smooth
  colour gradients if you ignore that. Dense test cards that bilinear-
  average to a single colour produce baffling "fullscreen green wash"
  bugs that look like rendering catastrophes when really it's just LINEAR
  filtering a small texture over a large surface.
- **bufWidth alignment is mandatory for fixed pixel formats** (PSP, PS2, 3DS
  C3D). PSP's `sceGuTexImage(level, width, height, bufWidth, data)` requires
  the **row stride** to be a multiple of 16 bytes. For PSM_8888 (RGBA8,
  4 B/texel) that means `bufWidth >= 4`. A 2×2 fallback white texture with
  `bufWidth=2` reads row 0 correctly but row 1 from an offset that doesn't
  match the actual buffer layout — the bottom half samples garbage. The
  visible symptom is exactly "untextured widgets render with top half
  opaque, bottom half empty" (GE Debugger preview shows the texture itself
  with row 0 valid and row 1 transparent/checkered). Minimum legal
  `bufWidth` per format:
  - PSM_8888 / PSM_T32: 4 texels (16 B stride)
  - PSM_5650 / PSM_5551 / PSM_4444 / PSM_T16: 8 texels
  - PSM_T8: 16 texels
  - PSM_T4: 32 texels
  Pad small engine-internal textures to satisfy this. User assets are
  almost always large enough that the requirement is automatic.

### Graphics — 2D / through mode for shaderless platforms

- **"Through mode" coordinates are not normalised.** PSP `GU_TRANSFORM_2D`
  treats float texcoords as **texel units**, not normalised 0..1. Engine
  widgets emit normalised UVs assuming a shader maps them; on a shaderless
  platform a vertex UV of `(1.0, 1.0)` samples **one texel** (the one at
  texel coord (1,1)) and that single texel's colour fills the entire face.
  Symptom: every textured UI widget renders as a single-colour smear
  (nearest filter) or a faintly graded wash (linear). Bake the UV-to-texel
  multiply into the vertex buffer at draw time — typically as part of a
  `ApplyUIDrawTransform` helper that also bakes per-vertex tint and any
  position scale/offset. **`sceGuTexScale` does NOT apply in through mode**
  — don't waste a day trying that first.
- **Bake position transforms into vertices for Text widgets.** The engine
  builds text glyph vertices in widget-LOCAL space at the font's native
  size (cursor starts at `(0, 0)` at e.g. 32 pt). Vulkan/GX apply the
  widget rect translation + scale via shader/TEV uniform. PSP / shaderless
  paths must bake the equivalent into the per-vertex buffer:
  ```cpp
  posOffset = (text->GetRect().mX + justified.x,
               text->GetRect().mY + justified.y);
  posScale  = text->GetScaledTextSize() / font->GetSize();
  // then vertex.xy = vertex.xy * posScale + posOffset
  ```
  Without it, every Text widget renders at top-left at font-native size
  regardless of its anchor.
- **`Widget::mTransform` is a `glm::mat3` that silently drops translation.**
  The engine builds it as `glm::mat4(translate(pivot) * rotate * translate(-pivot))`
  then assigns to a `mat3` member. `glm::mat3(mat4)` truncates to the
  upper-left 3×3 — losing the translation columns. GameCube's
  `ApplyWidgetRotation` reads `trans3[2][0/1]` for translation but it's
  always zero. So `mTransform` only contains rotation; using it to transform
  a widget would rotate around screen origin, not around the widget's
  pivot. Reconstruct rotation-around-pivot directly:
  ```cpp
  const float rad = widget->GetRotation() * DEGREES_TO_RADIANS;
  const glm::vec2 pivot = (widget->GetRect().mX + widget->GetRect().mWidth  * widget->GetPivot().x,
                           widget->GetRect().mY + widget->GetRect().mHeight * widget->GetPivot().y);
  // for each vertex: dx,dy = pos - pivot; new = pivot + rotate(dx,dy,rad)
  ```
  Fast-path the `rotRad ≈ 0` case so the common (non-rotated) widget pays
  no sin/cos cost per vertex.

### Input

- **`InputConstants.h` is the FIRST thing to audit on a new platform.** If
  there's no `#elif PLATFORM_FOO` arm AND no fallback `#else`, your gamepad's
  transition detection silently doesn't work (see Engine integration above).
  Status as of writing: engine now has a gamepad-only `#else` fallback for
  console addons. If your platform needs keyboard/mouse/touch, add an
  explicit arm.
- **Analog stick drift requires a deadzone.** Cheap analog hardware (PSP,
  Vita, original Xbox controllers) rests anywhere from 100..160 raw out of
  0..255 — often well above the engine's 0.5 virtual-button threshold for
  `GAMEPAD_L_UP/DOWN/LEFT/RIGHT`. Without a deadzone, drift wobbles the
  virtual buttons on/off and Button widget nav reads them as a stream of
  presses. 0.30 normalised (~38 raw) is a reasonable starting point — wide
  enough to absorb typical drift, narrow enough that deliberate stick push
  still crosses the 0.5 virtual-button threshold:
  ```cpp
  auto applyDeadzone = [](float v) {
      if (v >  0.30f) return (v - 0.30f) / 0.70f;
      if (v < -0.30f) return (v + 0.30f) / 0.70f;
      return 0.0f;
  };
  ```
- **Use non-blocking peek, not blocking read** for the input poll. PSP's
  `sceCtrlPeekBufferPositive` returns the latest sample; `sceCtrlReadBufferPositive`
  blocks until next VBlank sample, which **halves effective input rate** if
  the render loop already VSyncs. Same gotcha exists on other consoles —
  check whether the SDK's "read" call is blocking and prefer the "peek"
  variant unless you specifically need to sync.
- **Invert the Y axis** if the platform reports stick Y as screen-down-
  positive (PSP, DS). The engine's convention is `LTHUMB_Y > 0` = stick
  pushed UP. Without inversion the L_UP virtual button never fires on
  upward stick push.

### Filesystem (small/console media)

- **FAT 8.3 returns uppercase short names.** Files like `tent.oct` come back
  from `sceIoDread` as `TENT.OCT`. Asset name normalisation should only
  fire when no lowercase letters are present in the filename (treat all-
  upper as the FAT 8.3 case and lowercase it; treat mixed-case as
  authored-as-is). Extension comparisons MUST be case-insensitive.
- **Don't interleave file I/O with directory iteration.** PSP's
  `sceIoDread` keeps internal state that gets corrupted if another file
  open/read happens between calls — entries silently get skipped. Drain
  the entire directory into a `std::vector` first, then iterate the vector
  and process files. Likely true on other consoles too — when in doubt,
  collect-then-process.

### Diagnostics — distinguishing real bugs from emulator quirks

- **Emulator display layouts can lie about aspect ratio.** PPSSPP's default
  "Stretch" display setting scales X and Y of the framebuffer **non-
  uniformly** to fill the host window. An OS screenshot of the PPSSPP
  window then shows visuals with the wrong aspect ratio even though the
  underlying framebuffer is correct. Before debugging a "wrong aspect" bug:
  - Switch PPSSPP's display layout to "Auto" or "Stretch (maintain aspect)"
  - OR use the emulator's native framebuffer screenshot (PPSSPP `File →
    Save Screenshot`) which dumps at the PSP's exact 480×272
  - Compare that against the editor's `Screenshots/GamePreview_*.png` at
    the same resolution — only then can you say there's a real rendering
    bug
- **Add per-vertex / per-rect debug logging gated by frame count.** When a
  UI widget looks wrong, log its `mRect`, vertex `v0`, and computed
  scratch slice before submitting to the GE. The `[UIDBG]` pattern (see
  `Graphics_PSPGU.cpp::GFX_DrawQuad`) tracks down rendering issues that
  would otherwise need a GPU debugger.
- **The platform's GE-equivalent debugger is the fastest way to localise a
  texture bug.** PPSSPP's GE Debugger shows the actual bound texture data
  the GE sees — for the bufWidth alignment bug above, the preview pane
  visibly showed a 2×2 texture with row 0 white and row 1 transparent
  even though the source buffer was four `0xFFFFFFFF`s. Use this BEFORE
  speculating about UV/filter/blend bugs.
- **Per-frame validation log lines should always print "before" and
  "after" snapshots.** When `mPrev` vs `mCurrent` state matters (input
  transitions, scratch reuse, frame counters), log both at the same call
  site so you can prove the value didn't get corrupted between snapshots:
  ```cpp
  LogDebug("[FOO] frame=%u  before: state=%d  after: state=%d  &state=%p",
           ...);
  ```
  Found the InputConstants bug by adding exactly this; the `before/after`
  diff showed `InputAdvanceFrame` was a no-op on PSP.

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
| `…/Packages/com.polyphase.build.target.psp/`                                                             | **PSPSDK + WSL routing + PSPGU runtime + input/UI/scripts/asset registry complete. The most thoroughly debugged reference — covers Phases 2-5 (3D rendering, UI widgets w/ rotation, Lua scripts ticking, gamepad input with transition detection, asset registry with baked UUIDs, FAT filesystem quirks, out-of-tree builds, Config.ini auto-override). Read this for any Variant-2 addon with a full runtime.** |

Most new targets are a structural copy of one of these with the SDK-specific
bits swapped out.

### Memory references (paid-in-blood findings)

Each of these is a captured `~/.claude/.../memory/project_*.md` from the PSP
port. They go beyond what fits in this skill — read the relevant one before
chasing a similar symptom on your own platform:

- `project_psp_force_window_size` — both Oct hooks needed; Config.ini reload bug
- `project_psp_post_package_config_override` — packager Config.ini rewrite pattern
- `project_psp_out_of_tree_build` — Intermediate/PSP layout, trailing-slash + override gotchas
- `project_psp_ge_async_vertex_lifetime` — frame-scoped ring buffer pattern
- `project_psp_texture_bufwidth_alignment` — 16-byte stride mandate
- `project_psp_transform2d_texcoords_are_texels` — through-mode UV semantics
- `project_psp_texture_filter_must_honour_asset` — don't hardcode LINEAR/REPEAT
- `project_input_constants_console_fallback` — engine `#else` arm
- `project_psp_pspgum_breaks_state` — don't mix matrix utility lib with raw API
- `project_psp_dir_iter_intermixed_io` — sceIoDread state corruption
- `project_psp_fat_8_3_uppercase` — case-insensitive ext, lowercase normalisation
- `project_psp_scripts_embedded_assets_disk` — RAM budget split
- `project_psp_makefile_no_dep_tracking` — manual rm -f *.o for header changes
- `project_psp_make_j_oom` — -j4 default for psp-gcc
- `project_addon_validate_must_be_cached` — never per-frame, especially WSL-shellouts
- `project_psp_addon_active_location` — addon-copy-drift warning

## Checklist for a one-shot

**Addon scaffolding**
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

**Build hygiene (out-of-tree)**
- ☐ Make is invoked from `<projectDir>/Intermediate/<Platform>/`, not the project root.
- ☐ `PROJECT_ROOT` and `POLYPHASE_PATH` are normalised with `override ... := $(patsubst %/,%,...)` to defend against trailing-slash inputs from the addon's `ctx->projectDir`.
- ☐ Final binary stages to `<projectDir>/Build/<Platform>/<name>.<ext>` via a `stage:` target.
- ☐ Project `.gitignore` covers `Intermediate/`, `Build/`, `Packaged/` plus `*.o *.d *.elf` as belt-and-suspenders.
- ☐ Default `make -j` is sized for the cheapest expected host RAM (psp-gcc TUs peak at 1-2 GB; `-j4` is safe).

**Engine integration (fixed-resolution console)**
- ☐ `InputConstants.h` has an arm for your platform OR you're OK with the gamepad-only `#else` fallback.
- ☐ `OctPreInitialize` sets `config.mWindowWidth/Height` unconditionally (NOT gated on `== 0` — defaults to 1280x720).
- ☐ `OctPostInitialize` re-sets `GetEngineState()->mWindowWidth/Height` to defeat `Config.ini`'s desktop-resolution clobber.
- ☐ `PostPackage` rewrites the packaged `Config.ini`'s `WindowWidth`/`Height` to the platform's native size (both root + `<projectName>/` copies).
- ☐ Pulled only `EmbeddedScripts.cpp` (NOT `EmbeddedAssets.cpp`) from `Generated/` if console RAM is tight (<= 32 MB).

**Runtime / graphics**
- ☐ Per-draw vertex transforms use a frame-scoped ring buffer, not a shared scratch (otherwise GE-async aliasing).
- ☐ DCache writeback before any DMA-read by the GE (vertex, texture, matrix data).
- ☐ Texture `bufWidth` satisfies the pixel format's 16-byte stride requirement (PSM_8888 → bufWidth >= 4).
- ☐ For shaderless 2D mode: UVs baked to texel units (not normalised); position scale/offset baked for Text widgets; rotation reconstructed from `Widget::GetRotation()` + `GetPivot()` (NOT from `mTransform`).
- ☐ Texture binding honours `tex->GetFilterType()` / `GetWrapMode()` instead of hardcoding LINEAR/REPEAT.
- ☐ `GFX_SetViewport` / `GFX_SetScissor` hardcode platform-native dimensions (engine input from editor may not match).

**Input**
- ☐ Analog stick deadzone (~0.30 normalised) applied before passing to engine — defeats hardware drift.
- ☐ Non-blocking poll (Peek-equivalent), not the blocking Read-equivalent — won't halve frame rate.
- ☐ Y axis inverted if platform reports screen-down-positive (the engine convention is +Y = up).

**Filesystem**
- ☐ Directory iteration drains to a `std::vector` before processing files (don't interleave file I/O with `dread`-equivalents).
- ☐ Case-insensitive extension matching (FAT short names come back uppercase).
- ☐ Asset name normalisation only fires when the filename has no lowercase letters (treat all-upper as FAT 8.3, mixed-case as authored).
