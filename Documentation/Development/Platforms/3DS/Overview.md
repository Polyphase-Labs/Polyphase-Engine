# 3DS Platform Overview

## Hardware Summary

| Spec | Original 3DS | New 3DS |
|------|-------------|---------|
| **CPU** | ARM11 MPCore (dual-core, 268 MHz) | ARM11 MPCore (quad-core, 804 MHz) |
| **GPU** | PICA200 | PICA200 |
| **Top Screen** | 400 x 240 (stereoscopic 3D) | 400 x 240 (super-stable 3D) |
| **Bottom Screen** | 320 x 240 (resistive touch) | 320 x 240 (resistive touch) |
| **Right Stick** | Circle Pad Pro (accessory) | C-Stick (built-in) |

Polyphase detects the New 3DS at startup via `APT_CheckNew3DS()` and enables CPU speedup automatically. See `SYS_GetPlatformTier()` which returns **1** on New 3DS and **0** on the original model.

## Toolchain & Build System

3DS builds use the **devkitARM** toolchain from devkitPro. The `DEVKITARM` environment variable must be set.

**Build command:**

```bash
make -f Makefile_3DS -j12
```

**Key build flags:**

| Flag | Value |
|------|-------|
| Architecture | `-march=armv6k -mtune=mpcore -mfloat-abi=hard` |
| Platform define | `PLATFORM_3DS=1` |
| Graphics API define | `API_C3D=1` |
| C++ standard | `gnu++11` |
| RTTI / Exceptions | Disabled (`-fno-rtti -fno-exceptions`) |

**Libraries:** `-lcitro3d -lctru -lm`

**Output:** Static library `Build/3DS/lib<target>.a`, used when linking the final `.3dsx` homebrew executable.

**Docker:** 3DS builds are also supported via Docker (`docker run ... polyphase-engine build-3ds`).

## Graphics (Citro3D)

The 3DS graphics backend uses **Citro3D** (C3D), a high-level wrapper around the PICA200 GPU.

| Constant | Value |
|----------|-------|
| `MAX_FRAMES` | 2 |
| `MAX_MESH_VERTEX_COUNT` | 65,535 |
| `MAX_GPU_BONES` | 16 |
| `SUPPORTS_SECOND_SCREEN` | 1 |

**Shaders** are written for the PICA200 vertex processor and compiled with the `picasso` assembler. Source files live in `Engine/Shaders/PICA200/` (`.v.pica` extension) and are compiled into header files in `Engine/Intermediate/3DS/`:

| Shader | Description |
|--------|-------------|
| `StaticMesh` | Static mesh rendering |
| `SkeletalMesh` | Skeletal mesh with bone transforms |
| `Particle` | Particle systems |
| `Quad` | 2D quads / widgets |
| `Text` | Text rendering |

For details on dual-screen rendering and stereoscopic 3D, see [Screens](Screens.md).

## Input

The 3DS input backend supports gamepad and touch input. Keyboard and mouse are not available.

| Input Type | Supported | Notes |
|------------|-----------|-------|
| Gamepad buttons | Yes | A, B, X, Y, L, R, ZL, ZR, D-pad, Start, Select |
| Circle Pad (left stick) | Yes | Normalized to -1.0 to 1.0 |
| C-Stick (right stick) | Yes | New 3DS only, via `irrstCstickRead()` |
| Triggers | Yes | L/R as digital (0.0 or 1.0) |
| Touch | Yes | Bottom screen, via `hidTouchRead()` |
| Accelerometer | Yes | 3-axis, normalized (scale 1/512) |
| Gyroscope | Yes | 3-axis, normalized (scale 1/1024) |
| Soft keyboard | Yes | System keyboard via `swkbdInputText()` |
| Keyboard | No | |
| Mouse | No | |

Gamepad 0 is always connected on 3DS. The accelerometer and gyroscope are enabled during `INP_Initialize()` and disabled on shutdown.

## Audio (NDSP)

Audio uses the **DSP service** via `ndspInit()`.

| Constant | Value |
|----------|-------|
| `AUDIO_MAX_VOICES` | 8 |

**Supported formats:**

- Mono PCM8 / PCM16
- Stereo PCM8 / PCM16

**Features:**

- Linear interpolation (`NDSP_INTERP_LINEAR`)
- Pitch control via sample rate scaling
- Per-channel left/right volume mixing
- Looping support
- 8-bit PCM automatic unsigned-to-signed conversion

Wave buffers are allocated in linear memory (`linearAlloc` / `linearFree`), which is required for DMA transfers to the DSP.

## Networking (SOC)

Networking uses the **SOC service** (`socInit()` / `socExit()`), which provides a BSD-style socket API.

- UDP sockets (`SOCK_DGRAM`)
- Broadcast support
- Non-blocking mode via `ioctl(FIONBIO)`
- IP address queries via `SOCU_GetIPInfo()`
- Buffer size: 1 MB (`SOC_BUFFERSIZE = 0x100000`)

## Asset Pipeline

3DS builds use **embedded mode** -- assets are compiled into the executable or placed in RomFS.

The packaging flow:

1. Assets are cooked to platform-specific `.oct` format
2. `AssetRegistry.txt` is generated
3. Embedded asset headers (`EmbeddedAssets.h/.cpp`) are generated
4. Assets are copied to `{IntermediateDir}/Romfs/` for RomFS packaging
5. The final `.3dsx` + `.smdh` are produced via `make`

See [Packaging Flow](../PackagingFlow.md) and [Build Profiles](../../Info/BuildProfiles.md) for more details.

### SMDH metadata (title, description, author, icon)

Every 3DS build profile has a **3DS Target Options** block: Title, Description, Author and an Icon. The icon is an image file (PNG/JPG/BMP/TGA, project-relative or absolute) **or the name of an imported Texture asset**; either way it is resized to the 48x48 SMDH icon. They are passed to `make` as `APP_TITLE` / `APP_DESCRIPTION` / `APP_AUTHOR` / `ICON` overrides and end up in the `.smdh`, which is what the Homebrew Launcher and the HOME Menu display. Empty fields fall back to the project name, libctru's defaults, and the project icon from App Settings (when it is an image file rather than an `.ico`). The banner image and audio fields of the CIA target follow the same rule: a file path, or an imported Texture / SoundWave asset name.

## Installable CIA

The plain **Nintendo 3DS** target produces a `.3dsx` for the Homebrew Launcher. The **Nintendo 3DS (CIA)** target reuses the exact same cook + `make` pipeline and additionally wraps the result into `<Project>.cia`, an installable HOME Menu title. Output lands in `Packaged/polyphase.n3ds.cia/` next to the `.3dsx`.

Why bother: a `.3dsx` runs inside the Homebrew Launcher's memory allocation, whereas a CIA is its own title and asks the kernel for the full application region (64 MB on an original 3DS, 124 MB on a New 3DS). Memory-hungry scenes behave better installed.

### Requirements

| Tool | Needed for | Source |
|------|-----------|--------|
| `makerom` | the `.cia` (required) | [3DSGuy/Project_CTR releases](https://github.com/3DSGuy/Project_CTR/releases) (v0.19.0) |
| `bannertool` | HOME Menu banner + tune (optional) | [carstene1ns/3ds-bannertool releases](https://github.com/carstene1ns/3ds-bannertool/releases) (1.2.3) |
| `cwavtool` | banner tune as DSP-ADPCM (optional, experimental) | [PabloMK7/cwavtool releases](https://github.com/PabloMK7/cwavtool/releases) (1.0.0), only used when the profile's "DSP-ADPCM Tune" option is on |
| Python 3.12 + `pycgfx` | 3D banners (optional) | [skyfloogle/pycgfx](https://github.com/skyfloogle/pycgfx) |

None of these ship with devkitPro. The editor looks for them, in order, at the explicit paths set in **Preferences > External > Launchers > 3DS CIA Tools**, in the editor tools folder (`%APPDATA%\PolyphaseEditor\Tools\3DS` on Windows, `~/.config/PolyphaseEditor/Tools/3DS` on Linux), in `<devkitPro>/tools/bin`, and finally on `PATH`.

**Getting makerom and bannertool**

*Easiest (Windows and Linux):* open **Preferences > External > Launchers**, scroll to **3DS CIA Tools** and click **Download makerom + bannertool + cwavtool**. The editor fetches the pinned release archives from GitHub, extracts them into the tools folder above and picks them up immediately.

*Manual, Windows:* download `makerom-v0.19.0-win_x86_64.zip`, `bannertool-1.2.3-windows.zip` and `cwavtool.zip` (use `windows-x86_64/cwavtool.exe`) from the release pages linked above, extract `makerom.exe`, `bannertool.exe` and `cwavtool.exe`, then either copy them into `C:\devkitPro\tools\bin`, add their folder to `PATH`, or point the path fields in Preferences at them.

*Manual, Linux:* download `makerom-v0.19.0-ubuntu_x86_64.zip`, `bannertool-1.2.3-linux.tar.gz` and `cwavtool.zip` (use `linux-x86_64/cwavtool`), extract, `chmod +x makerom bannertool cwavtool`, then copy them into `/opt/devkitpro/tools/bin`, somewhere on `PATH` (e.g. `~/.local/bin`), or set the path fields in Preferences.

The **Build Dependencies** window also reports makerom as an optional dependency.

### Profile options

The CIA target's **Target Options** header adds:

| Option | Default | Notes |
|--------|---------|-------|
| Product Code | `CTR-P-` + 4 letters of the project name | Any 4 uppercase letters/digits |
| Unique ID | hash of the project name in `0xFF000`-`0xFFFFF` | The homebrew block; stable across rebuilds. Two games must not share one. Title ID = `0x000400000<id>00` |
| Version | `1.0.0` | major.minor.micro, max 63.63.15; bump to install over an older build |
| Custom RSF | engine `Standalone/3DS/template.rsf` | Your own makerom descriptor (save data size, services, memory mode); `$(APP_*)` placeholders are still substituted |
| Banner Mode | Image | Image (256x128 card) or 3D Scene |
| Banner Image | project icon / engine logo | PNG path or a Texture asset name, scaled to fit a dark 256x128 card |
| Banner Scene | – | Scene asset for the 3D banner |
| Rotate / Speed / Rotation Min / Max | on / 30 deg/s / 0 / 360 | Rotation about the vertical axis. Min 0 to Max 360 = continuous spin; a smaller span sways between the two angles with eased reversals; Rotate off = static at Min |
| Banner Audio / Loop | silence / off | WAV/OGG path or a SoundWave asset name; resampled to 44.1 kHz stereo, first ~2.2 s |
| DSP-ADPCM Tune | off | Experimental cwavtool encoding of the tune |

Changing any option invalidates the build cache for this target, so the `.cia` is regenerated.

### HOME Menu banner and tune

A banner is a `.bnr` holding a model (CGFX) and a tune (BCWAV). The limits are hard: the model must be under **512 KB** decompressed, and the tune must be 16-bit stereo at **44.1 kHz** and roughly **2 seconds** (about 96,000 sample frames). A 32 kHz tune, whatever its encoding, plays on the HOME Menu as a string of loud beeps (verified on hardware), so do not fight the converter on this. The editor normalises whatever you give it: images are letterboxed onto a 256x128 card; SoundWave assets and WAV/OGG files are resampled to 44.1 kHz stereo, trimmed to the sample limit and faded out over the last 50 ms (the classic "click" at the end of custom banners comes from an abrupt cut). The tune is PCM16 by default, the same as every working homebrew banner; the "DSP-ADPCM Tune" option encodes it with cwavtool at a quarter of the size but is experimental.

**3D banners** take a Scene asset: every visible `StaticMesh3D` is exported as glTF (world transform, first material texture, colour, blend mode, two-sidedness), auto-fitted into the HOME Menu camera and optionally spun, then converted with pycgfx and wrapped by bannertool. Skeletal meshes, particles and text are skipped. Keep textures small; the converter refuses models over 512 KB and the editor falls back to the image banner with a warning. pycgfx has **no license file**, so the engine never bundles it: **Install pycgfx** in Preferences downloads the main branch on your explicit request and runs `pip install --user gltflib pillow`; you can also `git clone https://github.com/skyfloogle/pycgfx` and set the folder in Preferences.

Azahar does not emulate the HOME Menu, so banners (2D or 3D) can only be checked on real hardware.

### Installing

- **Hardware:** copy the `.cia` to the SD card and install it with FBI, or use FBI's Remote Install to fetch it over the network. Launch from the HOME Menu.
- **Azahar:** File > Install CIA, then launch from the game list. Build & Run keeps launching the sibling `.3dsx` directly.

**Reinstalling after a change.** The HOME Menu caches each title's icon, banner model and banner animation by Title ID. Installing a new `.cia` over the old one at the same version keeps showing the cached banner, and even a version bump only partially refreshes it (the tune updates, the model and animation may not). When the icon or banner changed, delete the title first (System Settings > Data Management, or FBI's title list) and then install. Bump the Version option when you only want the game content updated.

**Embedded mode.** The CIA carries the cooked content in its romfs whether the profile is Embedded or not, but keep Embedded on for 3DS profiles: it is the documented mode for the platform and it also makes the `.3dsx` written beside the `.cia` self-contained. Toggling it changes the build-cache manifest, so the first build afterwards recooks.

The `.cia` is signed with makerom's test keys (`-target t`), which is what CFW consoles and Azahar accept.

### Headless / CI

`-build` accepts a build-target id as well as a platform name, so a CIA can be produced without the UI (makerom must be on `PATH`, in `devkitPro/tools/bin`, or in the editor tools folder):

```bash
PolyphaseEditor -headless -project MyGame/MyGame.octp -build polyphase.n3ds.cia embedded
```

Only built-in targets resolve in headless mode; profile options (product code, banner, ...) take their defaults.

## Platform-Specific Source Files

| File | Description |
|------|-------------|
| `Engine/Source/System/3DS/System_3DS.cpp` | Platform init/shutdown, file I/O, threading, time |
| `Engine/Source/Graphics/C3D/Graphics_C3D.cpp` | Citro3D rendering backend |
| `Engine/Source/Graphics/C3D/C3dTypes.h` | Graphics context struct |
| `Engine/Source/Input/3DS/Input_3DS.cpp` | Gamepad, touch, motion input |
| `Engine/Source/Audio/3DS/Audio_3DS.cpp` | NDSP audio backend |
| `Engine/Source/Network/3DS/Network_3DS.cpp` | SOC networking backend |
| `Engine/Source/Input/InputConstants.h` | Per-platform input capability flags |
| `Engine/Source/Audio/AudioConstants.h` | `AUDIO_MAX_VOICES` |
| `Engine/Source/Graphics/GraphicsConstants.h` | GPU limits, `SUPPORTS_SECOND_SCREEN` |
| `Engine/Makefile_3DS` | Build configuration |
| `Engine/Shaders/PICA200/` | PICA200 vertex shaders |

## Further Reading

- [Screens](Screens.md) -- dual-screen architecture, stereoscopic 3D, Lua API
