## macOS Developer Environment Setup

Polyphase builds and runs natively on Apple Silicon (arm64). Rendering goes through Vulkan on top of MoltenVK, the window and input layer is native Cocoa, and audio uses CoreAudio. Intel Macs are not supported.

### Pull Submodules

`git submodule update --init --recursive`

### Download and Install:

- Xcode Command Line Tools (clang, make, codesign, install_name_tool):

  ```bash
  xcode-select --install
  ```

- [Homebrew](https://brew.sh), then the build helpers:

  ```bash
  brew install cmake python3
  ```

- Vulkan SDK for macOS version 1.4.357.1 (LunarG). MoltenVK, the Vulkan loader, `glslc`, `libshaderc_combined.a` and `libspirv-cross-core.a` all come from this package.

Note: `cmake` is only used to build the bundled `libgit2` once (`Tools/prebuild_mac.sh`) and by native addons that ship a CMake project. The engine itself builds with `make`.
Note: The system `curl` on macOS has no WebSocket support, so `wss://` connections from the editor need Homebrew's curl: `brew install curl` and export `POLYPHASE_LIBCURL=/opt/homebrew/opt/curl/lib/libcurl.4.dylib`. Plain `http(s)://` works with the system library.

### Installing Dependencies

#### Install Vulkan SDK version 1.4.357.1:

- Download the macOS SDK from [https://vulkan.lunarg.com/sdk/home#mac](https://vulkan.lunarg.com/sdk/home#mac)
- Run the installer (or unzip it and run `vulkansdk-macOS-<ver>.app/Contents/MacOS/vulkansdk-macOS-<ver> --root ~/VulkanSDK/<ver> --accept-licenses --default-answer --confirm-command install` from a terminal). Install into `~/VulkanSDK/<ver>` — the editor looks there when `VULKAN_SDK` is not set.
- Add these to your `~/.zshrc` (replace `1.4.357.1` with the version you installed). `VULKAN_SDK` must point at the `macOS` subdirectory, which holds `include/`, `lib/` and `bin/`:

  ```
  export VULKAN_SDK=$HOME/VulkanSDK/1.4.357.1/macOS
  export PATH=$VULKAN_SDK/bin:$PATH
  export VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
  export VK_DRIVER_FILES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
  export VK_ADD_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d
  ```

  The SDK ships the same thing as `~/VulkanSDK/<ver>/setup-env.sh`; `source` it instead if you prefer.
- Close and reopen your terminal (or run `source ~/.zshrc`)
- Verify the driver is visible:

  ```bash
  vulkaninfo --summary | grep -E "deviceName|driverName"
  ```

  Expect `driverName = MoltenVK` and your GPU as `deviceName`. `which glslc` should print a path inside the SDK.

Note: `VK_ICD_FILENAMES` / `VK_DRIVER_FILES` are only needed for the editor binary you build from source. A packaged `.app` carries its own MoltenVK and ICD manifest under `Contents/Resources/vulkan/icd.d`, and the editor also fills these in itself at startup when it finds an SDK under `~/VulkanSDK` (or the path set in **Preferences > External > Vulkan SDK Root**), so a Finder launch works without a shell environment.

#### Install devkitPro

Only needed to package for GameCube, Wii or 3DS. Skip this section if you never will.

1. Install devkitPro Pacman for macOS ([https://devkitpro.org/wiki/devkitPro_pacman](https://devkitpro.org/wiki/devkitPro_pacman)) — download the `.pkg` from the [devkitPro pacman releases](https://github.com/devkitPro/pacman/releases) and install it. It creates `/opt/devkitpro` and sets `DEVKITPRO`, `DEVKITPPC`, `DEVKITARM` in `/etc/profile.d/devkit-env.sh`.
2. Install the Wii/3DS toolchains ([https://devkitpro.org/wiki/Getting_Started](https://devkitpro.org/wiki/Getting_Started)). This is **required for GameCube too**: `wii-dev` provides devkitPPC (`powerpc-eabi-g++`), which the GameCube build compiles with even though its libraries come from `libogc2` in step 3.

   ```bash
   sudo dkp-pacman -S wii-dev 3ds-dev
   ```

   - Restart the terminal so the `DEVKIT*` variables are picked up
3. If you want to package for GameCube, install `libogc2` ([https://github.com/extremscorner/pacman-packages#readme](https://github.com/extremscorner/pacman-packages#readme))

   ```bash
   sudo dkp-pacman-key --recv-keys C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE --keyserver keyserver.ubuntu.com
   sudo dkp-pacman-key --lsign-key C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE
   ```
   - Add this entry to `/opt/devkitpro/pacman/etc/pacman.conf` above the existing `[dkp-libs]` entry. This is **file content, not commands** — the two `Server` lines are mirrors of the same repository (pacman falls back to the second if the first is unreachable), and both lines belong in the file:

     ```ini
     [libogc2-devkitpro]
     Server = https://packages.libogc2.org/devkitpro/osx/$arch
     Server = https://packages.extremscorner.org/devkitpro/osx/$arch
     ```
   - Then sync and install (accept overwriting if asked):

     ```bash
     sudo dkp-pacman -Syuu
     sudo dkp-pacman -S gamecube-tools-git libogc2 libogc2-libdvm
     ```

#### Compile Shaders, libgit2, and Standalone embedded-asset stubs

```bash
bash Tools/prebuild_mac.sh
```

This is the macOS counterpart of `Tools/prebuild.sh`. It checks that `VULKAN_SDK`, `cmake` and the Command Line Tools are present, then runs three steps: builds `libgit2` (arm64, SecureTransport TLS), compiles shaders with the SDK's `glslc`, and writes minimal stubs for `Standalone/Generated/EmbeddedAssets.{h,cpp}`, `EmbeddedScripts.{h,cpp}`, and `AddonPlugins.cpp`. The stub step only writes files that are missing — these are gitignored and normally regenerated by the Editor's "Build Data" action, but a fresh clone needs the stubs so the `Standalone` build succeeds.

### Build and Run the Editor

**VS Code:** open the repo root, install the C/C++ Extension Pack, and pick **Polyphase Editor - Mac** in Run and Debug (it runs the **Make Standalone Editor - Mac** task, then launches under lldb with the repo root as the working directory). **Polyphase Game - Mac** does the same for the runtime.

**Terminal:**


1. From the repo root: `cd Standalone`
2. Run `make -f Makefile_Mac_Editor -j$(sysctl -n hw.ncpu)`
3. Go back to the root directory `cd ..`
4. Run `Standalone/Build/Mac/PolyphaseEditor`. As on Linux, run it from the repo root so `Engine/Assets` and `Engine/Shaders` resolve; a copy of the binary is also placed at the repo root after every build.

The game runtime used by packaging is built the same way: `make -f Makefile_Mac_Editor` is the editor, `make -f Makefile_Mac_Game` produces `Standalone/Build/Mac/Polyphase.macho`, which the Packaging window reuses for script-only projects.

Build outputs live under `Build/Mac/` and `Intermediate/Mac/` next to their Linux siblings. `Engine/Makefile_Mac`, `Standalone/Makefile_Mac_Editor`, `Standalone/Makefile_Mac_Game`, `External/Bullet/Makefile_Mac` and `External/Assimp/Makefile_Mac` mirror the Linux makefiles line for line: same targets, same variables, with Apple's `clang`/`ar`/`ld64` flags (`-arch arm64 -mmacosx-version-min=12.0`, `-Wl,-rpath,@loader_path`, `-dynamiclib`) and the platform sources under `Source/{System,Input,Audio,Network,Serial}/Mac`.

Keyboard note: the editor's Ctrl-based hotkeys accept the Command key as well (⌘S saves, ⌘Z undoes). F-keys need **Use F1, F2, etc. keys as standard function keys** in System Settings > Keyboard, or the Fn key.

### Packaging a macOS App Bundle

The **macOS (App Bundle)** build target compiles an arm64 Mach-O with `Makefile_Mac_Game` and wraps it into `Packaged/Mac/<Project>.app` with MoltenVK and the Vulkan loader inside `Contents/Frameworks`. It needs the same tools as building the editor (Command Line Tools + Vulkan SDK); nothing else. macOS targets can only be built on a macOS host — the "Use Docker" option is disabled for them.

Signing: every bundle is at least ad-hoc signed so it runs on the machine that built it. For distribution set **Signing Identity** (`Developer ID Application: …`) in the profile's Target Options, optionally **Notarize** with a keychain profile created by `xcrun notarytool store-credentials`, and tick **Create .dmg**. Details in [Platforms/Mac/Packaging.md](../Platforms/Mac/Packaging.md).

To ship the editor itself, `bash Installers/build_app_mac.sh` produces `dist/Polyphase.app` and `bash Installers/build_dmg_mac.sh` wraps it into `dist/PolyphaseEditor-<version>-macos-arm64.dmg` (see [Platforms/Mac/Overview.md](../Platforms/Mac/Overview.md)).

### Packaging a 3DS installable (.cia)

The plain **Nintendo 3DS** build target needs nothing beyond `3ds-dev` above. The **Nintendo 3DS (CIA)** target, which produces an installable HOME Menu title, shells out to tools that devkitPro does not ship. None of them are needed unless you use that target.

| Tool | Needed for | Where it comes from |
|------|-----------|---------------------|
| `makerom` | the `.cia` (required) | [3DSGuy/Project_CTR releases](https://github.com/3DSGuy/Project_CTR/releases), `makerom-v0.19.0-macos_x86_64.zip` (runs under Rosetta) |
| `bannertool` | HOME Menu banner and tune (optional) | [carstene1ns/3ds-bannertool releases](https://github.com/carstene1ns/3ds-bannertool/releases), `bannertool-1.2.3-macos.tar.gz` |
| `cwavtool` | DSP-ADPCM tune encoding (optional, experimental) | [PabloMK7/cwavtool releases](https://github.com/PabloMK7/cwavtool/releases), `cwavtool.zip`, use `macos-x86_64/cwavtool` |
| Python 3.12 + pycgfx | 3D scene banners (optional) | `brew install python@3.12` and [skyfloogle/pycgfx](https://github.com/skyfloogle/pycgfx) |

**Easiest:** in the editor open **Preferences > External > Launchers**, scroll to **3DS CIA Tools**, and click **Download makerom + bannertool + cwavtool**. The archives are fetched from the release pages above into `~/Library/Application Support/PolyphaseEditor/Tools/3DS`, made executable, and picked up immediately. Downloaded binaries are quarantined by Gatekeeper; if one refuses to run, `xattr -d com.apple.quarantine <tool>`.

**Manual:** extract the binaries, `chmod +x makerom bannertool cwavtool`, and either copy them into `/opt/devkitpro/tools/bin`, put them somewhere on `PATH` such as `~/.local/bin`, or set the path fields in the same Preferences page.

**3D banners** additionally need Python 3 with pip. Then click **Install pycgfx** in the same Preferences page (it downloads pycgfx, pinned to a fixed commit, and runs `python3 -m pip install --user gltflib pillow`). pycgfx has no license file, which is why the editor only fetches it on request and never bundles it.

Details on the target, its options and the HOME Menu limits are in [Platforms/3DS/Overview.md](../Platforms/3DS/Overview.md#installable-cia).

### Where the editor keeps its files

| What | Path |
|------|------|
| Preferences, hotkey and input presets, downloaded tools | `~/Library/Application Support/PolyphaseEditor/` |
| Game saves when the project directory is read-only (e.g. inside a `.app`) | `~/Library/Application Support/<Project>/Saves/` |
| Addon-recovery sentinel | `~/Library/Application Support/Polyphase/` |

### Troubleshooting

- **`Failed to find platform surface extension` / `No physical device found`** — the Vulkan loader cannot see MoltenVK. Export `VK_DRIVER_FILES` (see above) or install the SDK under `~/VulkanSDK` so the editor can find it by itself. `vulkaninfo --summary` must list MoltenVK.
- **`dyld: Library not loaded: @rpath/libvulkan.1.dylib`** — the editor was linked against a `VULKAN_SDK` that has since moved. Rebuild with the current `VULKAN_SDK`, or set `DYLD_LIBRARY_PATH=$VULKAN_SDK/lib` for that run.
- **Shaders fail to compile in `prebuild_mac.sh`** — `VULKAN_SDK` must contain `bin/glslc`. Check it points at `~/VulkanSDK/<ver>/macOS`, not the version directory above it.
- **`Validation layers requested (ValidateGraphics=1) but VK_LAYER_KHRONOS_validation is not available`** — a packaged game (or an editor started from Finder) cannot see the SDK's validation layer; the engine now continues without validation instead of aborting. To validate a game, run it from a terminal with the SDK environment exported.
- **`ar: ... has no symbols`** warnings while building — harmless; those objects come from sources compiled out on macOS.
- **`wss://` connections report "no WebSocket support"** — install Homebrew curl and set `POLYPHASE_LIBCURL` as described above.
- **Gatekeeper blocks a downloaded editor or game** — ad-hoc signed builds are not notarized. Right-click > Open once, or `xattr -dr com.apple.quarantine <app>`.
- **Debugging** — `lldb Standalone/Build/Mac/PolyphaseEditor` from the repo root, or VS Code with the CodeLLDB extension pointing at the same binary with `cwd` set to the repo root.
