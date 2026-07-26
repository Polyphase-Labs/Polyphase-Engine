# Static Content & Content Pak — Build-Target Addon Brief

**Audience:** maintainers of Polyphase build-target addons (Dreamcast, PSP, PS2,
PS3, N64, Xbox, Sega Genesis, GCN, LinuxARM64, AndroidTV, ZIP, …).

**TL;DR: most targets need to change nothing.** Read §1, check the table in §2,
and stop there unless it points you at §3 or §4.

---

## 1. What landed in the engine

Two packaging options that stop shipped content from being casually read or
edited:

| Mode | What it does |
|---|---|
| **Static Content** | Cooked `.oct` assets, `.lua` scripts and `AssetRegistry.txt` are wrapped in an obfuscated container and decoded on demand at load |
| **Content Pak** | Builds on Static. Everything is folded into one `Content.pak` with an **obfuscated index**, and the loose copies are deleted — so filenames and the folder tree don't ship either |

Both are per-build-profile checkboxes in the Packaging window.

### Why your target gets this for free

The decode is **not** in the platform layer. It lives in two pieces of shared
engine code that every target links:

| Chokepoint | Covers |
|---|---|
| `Stream::ReadFile` (`Engine/Source/Engine/Stream.cpp`) | Every asset, script and registry read — including targets that ship their own `SYS_AcquireFileData` |
| `SYS_FileOpenRead` / `SYS_FileRead` / `SYS_FileSeek` (`Engine/Source/System/SystemUtils.cpp`) | Seekable/chunked reads (streaming audio) |

Your `CookAsset` output is picked up by the packaging sweep automatically —
whatever bytes you write get wrapped or packed without you doing anything.

This was verified end-to-end on the **Dreamcast** target with **zero addon
changes**, on both Static and Content Pak.

> **This is obfuscation, not DRM.** The key is a compile-time constant in the
> engine and ships inside the executable. Don't describe it to users as
> encryption or copy protection.

---

## 2. Does my target need changes?

| Question | If yes |
|---|---|
| Does your target's **runtime read content with its own `fopen`/`open`/`read`** — i.e. not via `Stream` or `SYS_File*`? | **§4 — required.** It will read encrypted bytes, or nothing at all once a pak prunes the loose tree. |
| Is your target **non-Vulkan** (no `.spv` shader files at runtime)? | **§3 — optional, cosmetic.** One line to hide a checkbox that does nothing on your platform. |
| Neither? | **Nothing to do.** |

Most targets are "neither" or "§3 only".

### Known target classification

| Target | `basePlatform` | Vulkan at runtime? | Needs §3? |
|---|---|---|---|
| Dreamcast | Linux | No | Yes |
| PSP | Linux | No | Yes |
| PS3 | Linux | No | Yes |
| N64 | Linux | No | Yes |
| Xbox (nxdk) | Windows | No | Yes |
| LinuxARM64 | Linux | **Yes** | No |
| AndroidTV | Android | **Yes** | No |
| ZIP (wraps a desktop build) | Linux | **Yes** | No |

If your target isn't listed: the question is only *"does my runtime load `.spv`
shader files from `Engine/Shaders/GLSL/bin/` at startup?"* — not what
`basePlatform` says.

---

## 3. Optional: hide the Content Pak checkbox (cosmetic)

The **Content Pak** checkbox hides itself when **Embedded** is on and the target
gains nothing from a pak. The only thing embedding doesn't cover is the Vulkan
`.spv` shaders — fixed-function console backends compile theirs in, so there
Embedded is already a single deliverable and a pak beside it carries nothing.

**The engine cannot infer this from `basePlatform`.** That field is a *cook-compat
anchor*, not a statement about the runtime backend: Dreamcast, PSP, PS2, PS3 and
N64 all declare `Platform::Linux`, and so do the genuinely-Vulkan `LinuxARM64`
and `ZIP` targets. Nothing distinguishes them.

So non-Vulkan targets opt in explicitly:

```cpp
// Signature per Engine/Source/Plugins/PolyphaseBuildTargetAPI.h:
//     void (*DrawProfileOptions)(const PolyphaseBuildContext* ctx);
static void MyTarget_DrawProfileOptions(const PolyphaseBuildContext* ctx)
{
    // Content Pak adds nothing on this platform when Embedded is on --
    // no runtime shader files to deliver. Hide the checkbox.
    char buf[8] = {0};
    if (!ctx->GetProfileSetting("polyphase.hideContentPak", buf, sizeof(buf)))
    {
        ctx->SetProfileSetting("polyphase.hideContentPak", "1");
    }

    // ... your existing region / BIOS / disc-format controls ...
}
```

It rides the existing `mTargetOptions` map, so there is **no
`POLYPHASE_BUILD_TARGET_API_VERSION` bump** and no descriptor change. Omitting it
is harmless — the checkbox merely appears where it isn't useful.

### Caveat you should know about

`DrawProfileOptions` is the **only** callback that receives a context capable of
writing profile settings (`Validate` takes just `char* outReason`), and the
engine only calls it while the **"Target Options" collapsing header is expanded**
(`PackagingWindow.cpp`). So the flag is written the first time a user expands
that header for the profile, and persists in `BuildProfiles.json` from then on.

If you'd rather not depend on that, set it directly in the profile JSON:

```json
"targetOptions": {
    "polyphase.hideContentPak": "1"
}
```

Purely cosmetic either way — nothing breaks if it's never set.

---

## 4. Required *only* if your addon ships its own runtime

This applies to **Variant 2** targets — those providing their own
`Graphics`/`Input`/`Audio`/`System` implementations for the platform. If your
addon is packaging-only (it just cooks and invokes a compiler), skip this.

### Rule: never `fopen` engine content directly

| Need | Use | Not |
|---|---|---|
| Read a whole asset / script / registry file | `Stream::ReadFile(path, isAsset)` | `fopen` + `fread` |
| Chunked / seekable read (streaming audio) | `SYS_FileOpenRead` / `SYS_FileRead` / `SYS_FileSeek` / `SYS_FileClose` | `fopen` + `fseek` |
| Run a Lua chunk | `Stream::ReadFile` + `luaL_loadbuffer` | `luaL_dofile` |

```cpp
Stream stream;
if (stream.ReadFile(path.c_str(), true))
{
    // GetData()/GetSize() are plaintext here in every build mode.
    // Plain files pass through untouched, so this is also correct
    // in Moddable builds and in the editor -- no mode to branch on.
}
```

`luaL_dofile` is especially worth grepping for: it bypasses the obfuscation
decode **and** the embedded-script table, so it fails in Static *and* Embedded
builds.

### Offsets are in decoded space

`SYS_FileSeek` takes an offset into the **decoded** payload. The shared wrapper
maps it onto the physical file, accounting for the container header and, for a
packed asset, the entry's base offset inside `Content.pak`.

If you implement your own seek/read path and get this backwards, the symptom is
**audio that plays correctly for the first chunk and turns to noise afterwards**.

### If you override `SYS_AcquireFileData`

That's fine and needs no changes — it sits *below* `Stream::ReadFile`, so decode
still happens above you. Just don't try to decode there yourself.

### Device path prefixes

`ContentPak::Mount` tries the raw path first, then `SYS_GetAbsolutePath(path)`.
If your platform needs a device prefix (`romfs:/`, `host:`, `/cd/`, …), make sure
`SYS_GetAbsolutePath` applies it, or the pak silently fails to open and the
runtime falls back to loose files that are no longer there.

### Raw assets are untouched — on purpose

`.mp4`, `.json`, `.png`, `.rcss` and other non-`.oct` files are **never**
obfuscated or packed, precisely because addon code opens them with its own I/O
(FFmpeg, a decoder library, a parser). Existing addon file I/O against those
keeps working unchanged. Only `.oct`, `.lua`, `AssetRegistry.txt`, shaders and
the `.octp` are protected.

---

## 5. Verify

Package your target twice and compare:

1. **Static off** (baseline) — confirm it still builds and boots as before.
2. **Static on** — `.oct` / `.lua` in the package should start with the ASCII
   bytes `PLYOBF`; the game should behave identically.
3. **Static + Content Pak on** — the package should contain `Content.pak` and no
   loose `.oct`/`.lua`. Boot it.

In the runtime log, a working pak build starts with:

```
ContentPak: mounted 'Content.pak' (<N> entries)
```

Handy trick: the container header stores a **random per-file salt at offset 12**,
regenerated every packaging run. Two files with the same size but different salt
bytes means the deployed copy is stale — useful when a deploy script skips
same-size files.

---

## 6. Troubleshooting

| Symptom | Cause |
|---|---|
| `Failed to open file: …` | **Not** a decode problem — that message means `fopen` returned null. Check your deploy layout and `SYS_GetAbsolutePath`. |
| Garbage bytes / parse failures only in a packaged build | Runtime read content with raw `fopen`. See §4. |
| Lua fails to load in a shipped build but works in the editor | `luaL_dofile`. See §4. |
| `ContentPak: '…' is not a valid pak` | Truncated or corrupted copy on the target medium. |
| `checksum mismatch` at load | File corrupt, truncated in transit, or built with a different engine key. |
| Pak silently absent; falls back to loose files | `SYS_GetAbsolutePath` isn't applying your device prefix. |
| Streaming audio is noise after the first chunk | Logical vs physical offset swapped in a custom seek path. |
| Black screen, `No default scene found`, non-embedded pak build | Engine-side bug, not yours — report it. The asset-registry flag must be re-asserted after `LoadProject`. |

---

## 7. Reference

| Path | What |
|---|---|
| `Documentation/Development/StaticContent.md` | Full reference — formats, decode sites, cook pipeline |
| `Documentation/Development/CustomBuildTarget.md` | Build-target addon API |
| `Engine/Source/Engine/ContentObfuscation.{h,cpp}` | Container format + keystream |
| `Engine/Source/Engine/ContentPak.{h,cpp}` | Pak reader/writer |
| `Engine/Source/Plugins/PolyphaseBuildTargetAPI.h` | Canonical descriptor + context signatures |

When this brief disagrees with the headers, **the headers win**.
