# Static Content & Content Pak

Two packaging options that stop shipped content from being casually read, edited
or extracted.

- **Static Content** — cooked `.oct` assets, `.lua` scripts and
  `AssetRegistry.txt` are wrapped in an obfuscated container. Contents are
  protected; filenames and the folder tree still ship as-is.
- **Content Pak** — builds on Static. Everything is folded into a single
  `Content.pak` with an **obfuscated index**, and the loose copies are deleted,
  so neither the content nor the names ship.

Both work on **every platform, including consoles and out-of-tree build-target
addons**, with no per-platform code. See [Why it works everywhere](#why-it-works-everywhere).

> **This is obfuscation, not DRM.** The key is a compile-time constant in
> `ContentObfuscation.cpp` and therefore ships inside the executable. It defeats
> opening `Player.lua` in Notepad, hex-editing a `.oct`, and casual asset
> ripping. It does not defeat someone with a disassembler. Say so in any user
> messaging; don't imply otherwise.

---

## Enabling

**Packaging window → Build Profile:**

| Checkbox | Requires | Effect |
|---|---|---|
| **Static Content** | — | Obfuscates cooked assets, scripts and the registry |
| **Content Pak** | Static Content | Packs everything into `Content.pak`, deletes loose copies |

Serialized to `BuildProfiles.json` as `"staticContent"` and `"contentPak"`.
Profiles saved before these existed load as `false`, so existing builds are
unchanged.

Both compose with **Embedded Mode** — see
[Interaction with Embedded](#interaction-with-embedded).

**Content Pak hides itself** when Embedded is on and the target gains nothing
from it (see [Build-target addon opt-in](#build-target-addon-opt-in)).

---

## What ships

| Mode | Distribution |
|---|---|
| Moddable (both off) | Whole package tree, plain files |
| Static | Whole package tree, contents obfuscated |
| Static + Content Pak | Executable + `Content.pak` |
| Embedded + Static | Executable (+ `Engine/Shaders/GLSL/bin/` on Vulkan targets) |

**Never packed, always loose:**

| File | Why |
|---|---|
| `Config.ini` | `ReadEngineConfig` runs *before* the pak mounts; falls back to the copy embedded in the executable |
| Raw assets (`.png`, `.json`, `.mp4`, `.rcss`) | Addon code and `UILoader` open these with their own file I/O and never see the pak or the decode |

---

## Container format

Every obfuscated file is the original bytes behind a 24-byte header. All
multi-byte fields are read and written **a byte at a time**, so the format is
endianness-independent and works unchanged on big-endian GameCube/Wii.

```
offset  size  field
  0      6    magic 'P','L','Y','O','B','F'
  6      1    version
  7      1    flags        (bit0 = payload checksum present)
  8      4    decodedSize
 12      4    salt
 16      4    checksum     FNV-1a-32 over the decoded payload
 20      4    headerCheck  FNV-1a-32 over bytes 0..19
 24      N    keystream-XOR'd payload
```

**Detection is by magic, not a config flag.** Plain files fail the test and pass
through untouched, so obfuscated and clear content coexist in one package and the
editor keeps reading its own unobfuscated project tree. `headerCheck` makes a
false positive effectively impossible.

### Cipher

A 4-byte-block keystream derived from `(engineSecret, salt, blockIndex)`, XOR'd
into the payload. Three properties drive the choice:

- **Cheap.** ~1200 MB/s on a desktop host; roughly 67 ms for a 4 MB asset on a
  486 MHz Gekko. The DVD/SD read it rides along with takes 1.5–3 s for the same
  data, so decode is single-digit percent of load time.
- **Byte-oriented.** The keystream byte is extracted by shifting, never by
  aliasing a `uint32_t`, so it produces identical bytes on every endianness.
- **Offset-addressable.** A byte's keystream depends only on its own payload
  offset. This is what makes truncated reads and mid-file seeks work — see
  [Truncated reads](#truncated-reads) and [Streaming audio](#streaming-audio).

**Keying is salt-only, deliberately not path-based.** The salt is random per file
and stored in the header. The four decode sites see a file under four different
names (on-disk path, VFS lookup key, `EmbeddedFile::mName`, asset name), and any
disagreement between cook-side and runtime-side canonicalisation would be a
silent corruption bug. Salt-only keying makes every decode site trivially
correct, and identical files still encrypt differently.

---

## Content.pak format

```
[ header  32 bytes, plain — just enough to find the index ]
[ data    per-entry ContentObfuscation containers, concatenated ]
[ index   entry records + path blob, itself in a container ]
```

```
Header:
  0   8  magic "PLYPAK1\0"
  8   4  version
 12   4  entryCount
 16   4  indexOffset
 20   4  indexSize
 24   4  headerCheck   FNV-1a-32 over bytes 0..23
 28   4  reserved

Index record (24 bytes each, sorted by pathHash):
  0   8  pathHash      FNV-1a-64 of the canonical key
  8   4  dataOffset
 12   4  dataSize
 16   4  pathOffset    into the path blob following the records
 20   4  pathLength
```

Only the index is resident at runtime (~10–20 KB for a few hundred entries);
entries are read and decoded on demand, so console memory behaviour matches the
loose-file path it replaces.

Entries are stored as the **same containers** Static writes, so `Stream::ReadFile`'s
existing decode handles them unchanged — the pak layer only hands back the right
span of bytes.

Lookups binary-search on the hash, then **confirm against the stored path**, so a
64-bit collision degrades to a miss rather than silently serving the wrong asset.

### Keys

Package-relative, forward slashes — exactly the form the runtime asks with:

```
Game/Assets/Textures/T_Grass.oct
Game/Scripts/Player.lua
Game/AssetRegistry.txt
Engine/Assets/Textures/T_White.oct
Engine/Shaders/GLSL/bin/Forward.vert
```

Path-keying is safe **here**, unlike the per-file cipher, because the pak index
is the single authority on naming: the cook writes the keys and the runtime reads
them back verbatim, with no per-platform canonicalisation to disagree about.

---

## Runtime

### Why it works everywhere

There are exactly two shared chokepoints, and nothing else needs touching:

| Chokepoint | Covers |
|---|---|
| `Stream::ReadFile` (`Engine/Source/Engine/Stream.cpp`) | Every asset, script and registry read on **every** platform — including targets whose `SYS_AcquireFileData` lives in an out-of-tree build-target addon. Also covers the embedded raw-asset VFS, since those hits return through the same call. |
| `SYS_FileOpenRead` / `Read` / `Seek` (`Engine/Source/System/SystemUtils.cpp`) | Seekable streaming (audio). Implemented once for all platforms. |

> **Do not** put the decode in per-platform `SYS_AcquireFileData`. That would
> require editing five in-repo backends plus every out-of-tree addon backend, and
> defeats the whole design. Static and Content Pak were both verified on a
> Dreamcast build-target addon with no addon changes at all.

### Decode sites

| Site | Handles |
|---|---|
| `Stream::ReadFile` | Loose files, pak entries, embedded raw VFS |
| `Asset::LoadEmbedded` | Cooked assets in the embedded byte-array table |
| `AssetManager::DiscoverEmbeddedAssets` | The asset header read at *registration* time — long before `LoadEmbedded` |
| `ScriptUtils::RunScript` | Embedded Lua scripts |
| `SYS_FileOpenRead`/`Read`/`Seek` | Streaming audio |

`DiscoverEmbeddedAssets` is the one that's easy to miss: it builds a `Stream`
directly over `EmbeddedFile::mData` and parses the asset header to get the TypeId
and UUID. Reading the raw container there registers every asset with garbage and
the game crashes on startup. Symptom: **Embedded + Static crashes, while
Embedded-only and Static-only are both fine.**

### Mount order

The pak mounts in `Engine::Initialize` **before `LoadProject`**, trying:

1. `Content.pak` (root-relative)
2. `<projectDir>Content.pak` — where console packages deployed into a per-project
   SD folder end up

On mount it forces `mUseAssetRegistry = true`, because directory discovery can't
work once the loose tree is pruned.

> **The registry flag must be forced twice.** `LoadProject` calls
> `ReadEngineConfig(projectDir + "Config.ini")` partway through, which resets
> `mUseAssetRegistry` to the project's authored value (usually `0`). Setting it
> only at mount time is silently undone, the registry step is skipped, nothing
> registers, and you get **"No default scene found"** on a black screen — but
> *only* in non-embedded builds, since embedded assets bypass the registry.
> It is re-asserted immediately before `DiscoverAssetRegistry`.

This also means the shipped `Config.ini`'s `UseAssetRegistry` value is ignored in
pak builds. That's deliberate: rewriting the packaged config can't reach the copy
embedded in the executable, which is what actually gets read on a per-project
console deploy.

### Truncated reads

`Asset::LoadFile` caps SoundWave reads on memory-tight console runtimes, and
`AssetManager::DiscoverDirectory` reads only a 21-byte asset header. Both are
smaller than the container header itself.

`Stream::ReadFile` adds `ContentObfuscation::kReadHeadroom` (32 bytes) to any
non-zero `maxSize`, so stripping the header still leaves the caller its requested
count. The offset-addressable keystream decodes the available prefix correctly;
the checksum is skipped when truncated, because it covers the whole payload.

### Streaming audio

`SoundWave::mStreamPcmOffset` is an offset into the **decoded** payload
(`SoundWave::LoadStream` stores `stream.GetPos()`). `SysFile` therefore carries
the payload base, salt and a logical cursor, maps logical offsets onto physical
ones on seek, and decodes each chunk as it is read.

For a packed asset it opens its **own** handle on the archive — streaming holds
its file position across chunks, so sharing the handle `ContentPak::Read` seeks
would corrupt both.

Getting this backwards produces audio that plays correctly for the first chunk
and turns to noise afterwards.

### Shader enumeration

Vulkan global shaders are *discovered* by walking
`Engine/Shaders/GLSL/bin/`, not loaded from a known list
(`VulkanContext::CreateGlobalShaders`). A packed build has no such directory, so
the pak exposes `ContentPak::List(prefix, outKeys)` and `CreateGlobalShaders`
enumerates the index instead, falling back to the directory walk when no pak is
mounted.

This is also why shaders can't simply be embedded: there'd be no name list to
embed against.

---

## Cook

Both steps run in `BuildPhase1`, **after** the embedded generators and **before**
the 3DS romfs copy.

> **Ordering is load-bearing.**
> `ConvertFileToByteString` reads cooked files back through `Stream::ReadFile` —
> which now decodes — so running the sweep first would make the generators emit
> *plaintext* byte arrays, silently defeating Static+Embedded. The generators
> encode their own copies instead.
> The romfs step is `SYS_CopyDirectoryRecursive(packagedDir → romfsDir)`, so
> running after it ships 3DS in the clear.

| Mode | Function |
|---|---|
| Static | `ActionManager::ObfuscatePackagedContent` — wraps each file in place, idempotent |
| Static + Pak | `ActionManager::PackPackagedContent` — packs, then deletes the originals |

Packing **only prunes after a successful build**. A half-written pak plus deleted
originals would ship an unbootable package; on failure the cook falls back to
obfuscated loose files.

### Interaction with Embedded

`embedded && !useRomfs` — **not** `embedded` — is what actually puts content into
byte arrays. On 3DS, `useRomfs = (platform == N3DS) && embedded` routes content
through romfs as *loose files* instead.

When content really is embedded, the pack step:

- **skips** `.oct` / `.lua` — the embedded copy always wins in `LoadAsset`, so
  packing them would ship everything twice;
- still **deletes** the dead loose originals;
- still **packs** shaders, `.octp` and the registry.

Treating 3DS's romfs mode as "embedded" would delete the very files romfs is
about to bundle.

Static remains meaningful with Embedded, and matters most there: without it, an
embedded build carries **Lua source and every asset path as plaintext in the
binary** — `strings game.dol | grep function` finds it immediately.

### Build cache

`BuildCache::GetManifestPath` keys the manifest on platform + embedded + static +
pak. Any new build-affecting option must be added there, or flipping it reports
`UpToDate` and silently ships the previous build.

---

## Build-target addon opt-in

Content Pak hides itself when Embedded is on and the target gains nothing:

```cpp
pakRedundant = mEmbedded && (!PlatformUsesShaderFiles(basePlatform) || addonOptsOut);
```

The only thing embedding doesn't cover is the Vulkan `.spv` shaders; GX/C3D and
other fixed-function backends compile theirs in, so there Embedded is already a
single deliverable.

**`basePlatform` alone is not a reliable signal.** Console build-target addons
conventionally declare `basePlatform = Platform::Linux` as a *cook-compat anchor*
(Dreamcast, PSP, PS2, PS3, N64 all do; Xbox uses Windows), while
`BuildTarget-LinuxARM64` and `BuildTarget-AndroidTV` are genuine Vulkan targets
declaring the same values. Nothing in that field distinguishes them.

So a non-Vulkan addon target should opt in explicitly:

```cpp
// In DrawProfileOptions(const PolyphaseBuildContext* ctx) -- the only callback
// whose context can write profile settings (Validate takes just outReason).
char buf[8] = {0};
if (!ctx->GetProfileSetting("polyphase.hideContentPak", buf, sizeof(buf)))
{
    ctx->SetProfileSetting("polyphase.hideContentPak", "1");
}
```

The engine only calls `DrawProfileOptions` while the "Target Options" collapsing
header is expanded, so the flag lands the first time a user opens it and persists
in `BuildProfiles.json` thereafter. It can also be set there by hand. Purely
cosmetic either way.

| Key | Value | Effect |
|---|---|---|
| `polyphase.hideContentPak` | `"1"` | Hide the Content Pak checkbox when Embedded is on |

It rides the existing `mTargetOptions` map, so **no `POLYPHASE_BUILD_TARGET_API_VERSION`
bump** and no descriptor change. Omitting it is harmless — the checkbox merely
appears where it isn't useful.

Nothing else is required of a build target. `CookAsset` output is picked up by
the sweep/pack automatically, and the runtime decode is inherited from shared
engine code.

---

## Files

| File | Role |
|---|---|
| `Engine/Source/Engine/ContentObfuscation.{h,cpp}` | Container format, keystream, encode/decode |
| `Engine/Source/Engine/ContentPak.{h,cpp}` | Pak reader (`Mount`/`Exists`/`Read`/`List`/`FindEntry`) and writer (`Build`) |
| `Engine/Source/Engine/Stream.cpp` | Pak lookup + container decode on every read |
| `Engine/Source/System/SystemUtils.cpp` | `SysFile` — pak-aware, obfuscation-aware seekable reads |
| `Engine/Source/Editor/ActionManager.cpp` | `ObfuscatePackagedContent`, `PackPackagedContent`, generator encoding |
| `Engine/Source/Editor/Packaging/BuildProfile.{h,cpp}` | `mStaticContent`, `mContentPak`, `PlatformUsesShaderFiles`, `POLYPHASE_OPT_HIDE_CONTENT_PAK` |

Both new modules live in `Engine/Source/Engine/`, which is already in every
platform's `SOURCES` list — **no Makefile changes**. (A new *directory* would need
adding to `Makefile_Linux`, `Makefile_GCN`, `Makefile_Wii` and `Makefile_3DS`;
`Source/Engine/Utils/` is in the Linux list only.)

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Embedded + Static crashes; Embedded-only and Static-only are fine | A decode site was missed in the embedded path — most likely `DiscoverEmbeddedAssets`, which parses the asset header at registration |
| Black screen, `No default scene found`, non-embedded pak build | `mUseAssetRegistry` reset by `LoadProject`'s config re-read; must be re-asserted before `DiscoverAssetRegistry` |
| `Failed to open file: …` on console | Not a decode problem at all — that message means `fopen` returned null. Check the deploy layout; see `GetEngineContentDir` for the per-project-folder fallback |
| `checksum mismatch` at load | File corrupt, truncated in transit, or built with a different engine key |
| Pak silently absent on 3DS/PS2/PSP | `ContentPak::Mount` needs the `SYS_GetAbsolutePath` fallback for `romfs:/` and `host:` device prefixes |
| Toggling Static/Pak appears to do nothing | Build cache manifest key missing the flag |
| Streaming music is noise after the first chunk | Logical vs physical offset swapped in the `SysFile` seek path |
| Same file size but different salt bytes vs the package | The deployed copy is stale — a sync that skips same-size files didn't update it. The per-file random salt makes this provable |

---

## See also

- [PackagingFlow.md](PackagingFlow.md) — where these steps sit in the build
- [CustomBuildTarget.md](CustomBuildTarget.md) — build-target addon API
- `.llm/AssetSystem.md` — asset loading architecture
