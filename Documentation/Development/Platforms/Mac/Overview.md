# macOS Platform Overview

## Hardware Summary

| Spec | Requirement |
|------|-------------|
| **CPU** | Apple Silicon (arm64). Intel Macs are not supported. |
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
| Architecture | `-arch arm64 -mmacosx-version-min=12.0` |
| Platform define | `PLATFORM_MAC=1` |
| Graphics define | `API_VULKAN=1` |
| Language | `-std=gnu++17`; Objective-C++ files (`.mm`) get `-ObjC++ -fobjc-arc` |
| Link | `-rdynamic -Wl,-rpath,@loader_path -Wl,-rpath,@loader_path/../Frameworks -Wl,-rpath,$(VULKAN_SDK)/lib` |
| Frameworks | Cocoa, Metal, QuartzCore, IOKit, GameController, AudioToolbox, CoreAudio, Security, CoreFoundation, Carbon |

The Makefiles (`Engine/Makefile_Mac`, `Standalone/Makefile_Mac_Editor`, `Standalone/Makefile_Mac_Game`, `External/Bullet/Makefile_Mac`, `External/Assimp/Makefile_Mac`, `Template/Makefile_Mac_*`) are line-for-line copies of the Linux ones with the Apple toolchain substituted. Objects go to `Intermediate/Mac/`, outputs to `Build/Mac/`. Apple's `make` is 3.81, so the Makefiles avoid GNU make 4 features.

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

Apple Silicon refuses to run unsigned arm64 code, and `install_name_tool` invalidates the linker's signature, so the packagers always sign — ad-hoc (`codesign --sign -`) by default. Ad-hoc bundles run on the machine that built them; once downloaded they are quarantined and Gatekeeper shows a warning (right-click > Open, or `xattr -dr com.apple.quarantine <app>`). For distribution supply a Developer ID identity, notarize (`xcrun notarytool`) and staple; the packagers pass `--options runtime` with an entitlements file that disables library validation so addon dylibs load under the hardened runtime.

## Shipping the Editor

```bash
bash Installers/build_app_mac.sh     # stage_distribution.py --platform mac  ->  dist/Polyphase.app
bash Installers/build_dmg_mac.sh     # dist/PolyphaseEditor-<version>-macos-arm64.dmg
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

- Intel (x86_64) and universal binaries.
- The compute path tracer (light baking) stays Windows/Linux only.
- `wideLines` is unavailable on Metal; debug lines render 1px.
- Docker builds of macOS targets.
- The Lua debugger transport (LuaSocket) remains Windows only, as on Linux.
