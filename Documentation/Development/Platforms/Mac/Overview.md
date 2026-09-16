# macOS Platform Overview

## Hardware Summary

| Spec | Requirement |
|------|-------------|
| **CPU** | Apple Silicon (arm64) and Intel (x86_64). Release builds are universal; source builds default to the host's arch (`MAC_ARCH`, below). |
| **GPU** | Any Apple GPU (Metal) via MoltenVK |
| **OS** | macOS 12.0 or newer (`-mmacosx-version-min=12.0`, `LSMinimumSystemVersion`) |

`SYS_GetPlatformTier()` returns **2** (desktop tier), the same as Windows and Linux.

## Toolchain & Build System

macOS builds use Apple's `clang` from the Xcode Command Line Tools and the LunarG Vulkan SDK for macOS (MoltenVK, loader, `glslc`, shaderc, SPIRV-Cross). `VULKAN_SDK` must point at `~/VulkanSDK/<ver>/macOS`.

**Build commands:**

```bash
make -C Standalone -f Makefile_Mac_Editor -j$(sysctl -n hw.ncpu)   # editor  -> Standalone/Build/Mac/PolyphaseEditor
make -C Standalone -f Makefile_Mac_Game   -j$(sysctl -n hw.ncpu)   # runtime -> Standalone/Build/Mac/Polyphase.macho
```

**Key build flags:**

| Flag | Value |
|------|-------|
| Architecture | `$(MAC_ARCHFLAGS) -mmacosx-version-min=12.0` — `MAC_ARCH=native` (default, `uname -m`), `arm64`, `x86_64` or `universal` (`-arch arm64 -arch x86_64`) |
| Platform define | `PLATFORM_MAC=1` |
| Graphics define | `API_VULKAN=1` |
| Language | `-std=gnu++17`; Objective-C++ files (`.mm`) get `-ObjC++ -fobjc-arc` |
| Link | `-rdynamic -Wl,-rpath,@loader_path -Wl,-rpath,@loader_path/../Frameworks -Wl,-rpath,$(VULKAN_SDK)/lib` |
| Frameworks | Cocoa, Metal, QuartzCore, IOKit, GameController, AudioToolbox, CoreAudio, Security, CoreFoundation, Carbon |

The Makefiles (`Engine/Makefile_Mac`, `Standalone/Makefile_Mac_Editor`, `Standalone/Makefile_Mac_Game`, `External/Bullet/Makefile_Mac`, `External/Assimp/Makefile_Mac`, `Template/Makefile_Mac_*`) are line-for-line copies of the Linux ones with the Apple toolchain substituted. Objects go to `Intermediate/Mac/`, outputs to `Build/Mac/`. Apple's `make` is 3.81, so the Makefiles avoid GNU make 4 features.

### Architectures

`MAC_ARCH` (make variable or environment) picks the Mach-O slices and is propagated to every sub-make:

| Value | Flags | Use |
|-------|-------|-----|
| `native` (default) | `-arch $(uname -m)` | Day-to-day builds; whatever this Mac is (a Rosetta shell reports `x86_64`) |
| `arm64` / `x86_64` | `-arch <value>` | Cross-compile a single slice |
| `universal` | `-arch arm64 -arch x86_64` | What `release.yml` ships; one clang pass emits fat objects, archives and executables, about twice the compile time |

```bash
make -C Standalone -f Makefile_Mac_Editor -j$(sysctl -n hw.ncpu) MAC_ARCH=universal
MAC_ARCH=universal bash Tools/prebuild_mac.sh      # libgit2 must be rebuilt for the same slices
```

Engine, Bullet and Assimp stamp their output directory (`Build/Mac/.mac_arch`) and wipe it plus their intermediates when the requested value differs, so a thin archive is never linked into a universal binary. The Standalone and Template makefiles stamp per flavour (`.mac_arch-Editor`, `.mac_arch-Game`, ...) and wipe only that flavour's objects and binary, so packaging a game for another architecture never deletes the editor binary next to it. `Tools/CI/mac_check_archs.sh "arm64 x86_64" <files>` verifies the slices of executables, dylibs and static archives.

## Platform Layer

| Subsystem | Implementation |
|-----------|----------------|
| Window / event pump | `Engine/Source/System/Mac/System_MacCocoa.mm` — `NSWindow` with a `CAMetalLayer` content view, polled `NSEvent` pump in `SYS_Update`, `NSOpenPanel`/`NSSavePanel` dialogs, `NSPasteboard` clipboard, Finder drag-and-drop |
| POSIX half | `Engine/Source/System/Mac/System_Mac.cpp` — files, threads, saves, process spawning (shared `SystemUtils.cpp` provides `SYS_Exec*`), `task_info` RAM/CPU stats |
| Graphics | Vulkan through MoltenVK: `VK_EXT_metal_surface` surface, `VK_KHR_portability_enumeration` instance flag, `VK_KHR_portability_subset` device extension. The UI quad pipeline draws triangle lists (Metal has no fans; `Quad::kExpandFanToList`) |
| Editor ImGui backend | `Engine/Source/Editor/imgui_impl_mac.mm`, forked from the stock OSX backend but fed by the engine pump in backing pixels |
| Input | `Engine/Source/Input/Mac/Input_Mac.mm` — keyboard/mouse from the pump (Carbon `kVK_*` keycodes in `InputTypes.h`), gamepads via GameController.framework, cursor trap via `CGAssociateMouseAndMouseCursorPosition` |
| Audio | `Engine/Source/Audio/Mac/Audio_Mac.cpp` — software mixer feeding a DefaultOutput AudioUnit, one AudioUnit per streaming voice |
| Network / Serial | BSD sockets and termios, copied from the Linux files (`Network/Mac`, `Serial/Mac`) |
| HTTP / WSS | `dlopen` of `libcurl.4.dylib`; `POLYPHASE_LIBCURL` overrides the library (Homebrew curl for WebSocket support) |

### Coordinate model

Everything the engine sees is in **backing pixels**: `mWindowWidth/Height` equal the `CAMetalLayer` drawable size and mouse positions are multiplied by the window's `backingScaleFactor`. Cocoa's points only appear at the boundary inside `System_MacCocoa.mm`. On a Retina display the editor's interface scale preference is what makes the UI readable.

### Command key

`System_MacCocoa.mm` aliases the Command key onto `POLYPHASE_KEY_CONTROL_L/R` so the editor's Ctrl-based hotkeys work as ⌘ shortcuts. ImGui text fields see the real Super modifier as well.

## App Bundle Layout

Both the packaged game (`Packaged/Mac/<Project>.app`) and the shipped editor (`dist/Polyphase.app`) use the same layout:

```
<Name>.app/Contents/
    Info.plist, PkgInfo
    MacOS/<Name>                     the Mach-O (+ MacOS/Addons/*.dylib for games)
    Frameworks/libvulkan.1.dylib     Vulkan loader
    Frameworks/libMoltenVK.dylib     MoltenVK
    Resources/                       the engine/game tree (Engine/, Standalone/, ... or the Packaged/ payload)
    Resources/vulkan/icd.d/MoltenVK_icd.json
    Resources/<Name>.icns
```

`SYS_GetPolyphasePath()` returns `Contents/Resources/` whenever the executable lives under `Contents/MacOS/`, and `GameMain` pivots its working directory there, so no environment variables are needed. The Vulkan loader searches `Contents/Resources/vulkan/icd.d` first on macOS, which is how the bundled MoltenVK is found; the executable reaches the loader through `@rpath` (`@executable_path/../Frameworks`).

## Signing and Gatekeeper

Apple Silicon refuses to run unsigned native code, and `install_name_tool` invalidates the linker's signature, so the packagers always sign — ad-hoc (`codesign --sign -`) by default. Ad-hoc bundles run on the machine that built them; once downloaded they are quarantined and Gatekeeper shows a warning (right-click > Open, or `xattr -dr com.apple.quarantine <app>`). For distribution supply a Developer ID identity, notarize (`xcrun notarytool`) and staple; the packagers pass `--options runtime` with an entitlements file that disables library validation so addon dylibs load under the hardened runtime.

## Shipping the Editor

```bash
bash Installers/build_app_mac.sh     # stage_distribution.py --platform mac  ->  dist/Polyphase.app
bash Installers/build_dmg_mac.sh     # dist/PolyphaseEditor-<version>-macos-<universal|arm64|x86_64>.dmg, tag taken from the binary's slices
```

`MAC_SIGN_IDENTITY` and `MAC_NOTARY_PROFILE` switch both scripts from ad-hoc to Developer ID signing and notarization. The bundled editor still needs the Xcode Command Line Tools and the Vulkan SDK on the user's machine to package projects or build native addons; it locates the SDK via `VULKAN_SDK`, **Preferences > External > Vulkan SDK Root**, or the newest install under `~/VulkanSDK`, and prepends the SDK `bin/` plus Homebrew to `PATH` for the child processes it spawns.

Known gap: double-clicking an `.octp` in Finder delivers the path as an Apple Event, not on the command line. Use File > Open, or `open -a Polyphase --args /path/to/Game.octp`.

## Preferences and Saves

| What | Path |
|------|------|
| Editor preferences / presets / downloaded tools | `~/Library/Application Support/PolyphaseEditor/` |
| Saves when the project is read-only | `~/Library/Application Support/<Project>/Saves/` |
| `POLYPHASE_SAVE_DIR` | overrides the save directory, as on Linux |

## Not Supported / Limitations

- The compute path tracer (light baking) stays Windows/Linux only.
- `wideLines` is unavailable on Metal; debug lines render 1px.
- Docker builds of macOS targets.
- The Lua debugger transport (LuaSocket) remains Windows only, as on Linux.
