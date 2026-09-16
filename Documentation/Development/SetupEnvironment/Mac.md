## macOS Developer Environment Setup

Polyphase builds and runs natively on Apple Silicon (arm64) and Intel (x86_64) Macs. Rendering goes through Vulkan on top of MoltenVK, the window and input layer is native Cocoa, and audio uses CoreAudio. The shipped editor is a universal binary. A source build targets the Mac it runs on unless you set `MAC_ARCH` (see [Architectures](#architectures-apple-silicon-intel-universal)).

### What to install

Everything not marked optional is required. Polyphase always packages for GameCube, Wii and 3DS, so the devkitPro toolchains are part of the baseline setup, not an extra.

| Install | Needed for | Verify with |
|---------|-----------|-------------|
| **Xcode Command Line Tools** (`clang`, `make`, `lipo`, `codesign`) | Everything | `xcode-select -p` prints a path |
| **Homebrew** with `cmake` and `python3` | libgit2 prebuild, asset-stub generator, native addons that ship a CMake project | `cmake --version`, `python3 --version` |
| **Vulkan SDK 1.4.357.1** (LunarG, macOS) | Editor and every game build: MoltenVK, loader, `glslc`, shaderc, SPIRV-Cross | `vulkaninfo --summary` lists `driverName = MoltenVK` |
| Rosetta 2 (Apple Silicon only, optional) | Running the Intel slice of a universal build, and `cwavtool` for 3DS banners | `arch -x86_64 /usr/bin/true` |
| Homebrew `curl` (optional) | `wss://` connections from the editor | `ls /opt/homebrew/opt/curl/lib` (`/usr/local/opt/curl/lib` on Intel) |
| **devkitPro pacman** with `wii-dev` and `3ds-dev` | Packaging for **Wii**, **GameCube** and **3DS** (the devkitPPC and devkitARM compilers) | `powerpc-eabi-g++ --version`, `arm-none-eabi-g++ --version` |
| **libogc2**, `libogc2-libdvm`, `gamecube-tools-git` | Packaging for **GameCube** (its libraries come from libogc2, not the stock libogc) | `ls /opt/devkitpro/libogc2/lib/cube` |
| `makerom`, `bannertool`, `cwavtool`, pycgfx | The **Nintendo 3DS (CIA)** installable target | [Packaging a 3DS installable](#packaging-a-3ds-installable-cia) |

Not needed on a Mac: Docker (macOS targets cannot be built in a container, and the console toolchains above run natively), Visual Studio, MSYS2.

### Pull Submodules

`git submodule update --init --recursive`

### Download and Install:

- Xcode Command Line Tools (clang, make, lipo, codesign, install_name_tool):

  ```bash
  xcode-select --install
  ```

  A full Xcode install also works. Nothing here uses Xcode projects.

- [Homebrew](https://brew.sh), then the build helpers:

  ```bash
  brew install cmake python3
  ```

  Homebrew lives in `/opt/homebrew` on Apple Silicon and `/usr/local` on Intel Macs. The editor looks in both when it spawns build tools.

- Vulkan SDK for macOS version 1.4.357.1 (LunarG). MoltenVK, the Vulkan loader, `glslc`, `libshaderc_combined.a` and `libspirv-cross-core.a` all come from this package. Installation steps are in the next section.

Note: `cmake` is only used to build the bundled `libgit2` once (`Tools/prebuild_mac.sh`) and by native addons that ship a CMake project. The engine itself builds with `make`.
Note: The system `curl` on macOS has no WebSocket support, so `wss://` connections from the editor need Homebrew's curl: `brew install curl` and export `POLYPHASE_LIBCURL=/opt/homebrew/opt/curl/lib/libcurl.4.dylib` (`/usr/local/opt/curl/lib/libcurl.4.dylib` on Intel Macs). Plain `http(s)://` works with the system library.

### Installing Dependencies

#### Install Vulkan SDK version 1.4.357.1:

One installer covers both architectures. The SDK's dylibs and static libraries are universal, so the same install serves native, Intel and universal builds.

- Download the macOS SDK from [https://vulkan.lunarg.com/sdk/home#mac](https://vulkan.lunarg.com/sdk/home#mac)
- Run the installer (or unzip it and run `vulkansdk-macOS-<ver>.app/Contents/MacOS/vulkansdk-macOS-<ver> --root ~/VulkanSDK/<ver> --accept-licenses --default-answer --confirm-command install` from a terminal). Install into `~/VulkanSDK/<ver>`; the editor looks there when `VULKAN_SDK` is not set.
- Add these to your `~/.zshrc` (replace `1.4.357.1` with the version you installed). `VULKAN_SDK` must point at the `macOS` subdirectory, which holds `include/`, `lib/` and `bin/`:

  ```
  export VULKAN_SDK=$HOME/VulkanSDK/1.4.357.1/macOS
  export PATH=$VULKAN_SDK/bin:$PATH
  export VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
  export VK_DRIVER_FILES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
  export VK_ADD_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d
  ```

  The SDK ships the same thing as `~/VulkanSDK/<ver>/setup-env.sh`; `source` it instead if you prefer. The `make` commands below need `VULKAN_SDK` exported in the shell they run from. A shell that skipped `~/.zshrc` (an SSH session, a CI runner, a script started from Finder) must export it explicitly.

  Note: the release workflow's `build-mac` job installs this same version through `.github/actions/install-vulkan-sdk-mac` (engine-repo mirror first, LunarG CDN fallback). Bumping the SDK version here means bumping that action's default and the `actions/cache` key in `.github/workflows/release.yml` too. Uploading the unmodified `vulkansdk-macos-<ver>.zip` as an asset on a `vulkan-sdk-<ver>` GitHub Release makes CI independent of LunarG cycling old SDKs off their CDN.
- Close and reopen your terminal (or run `source ~/.zshrc`)
- Verify the driver is visible:

  ```bash
  vulkaninfo --summary | grep -E "deviceName|driverName"
  ```

  Expect `driverName = MoltenVK` and your GPU as `deviceName`. `which glslc` should print a path inside the SDK.

Note: `VK_ICD_FILENAMES` / `VK_DRIVER_FILES` are only needed for the editor binary you build from source. A packaged `.app` carries its own MoltenVK and ICD manifest under `Contents/Resources/vulkan/icd.d`, and the editor also fills these in itself at startup when it finds an SDK under `~/VulkanSDK` (or the path set in **Preferences > External > Vulkan SDK Root**), so a Finder launch works without a shell environment.

#### Install devkitPro toolchains

Required: Polyphase packages for GameCube, Wii and 3DS, and the editor's **Build Dependencies** window flags whichever of these it cannot find. It also sets `DEVKITPRO`, `DEVKITPPC` and `DEVKITARM` itself when `/opt/devkitpro` exists, so an editor started from Finder works without shell exports.

| Target | Needs |
|--------|-------|
| Wii | devkitPro pacman + `wii-dev` (devkitPPC, libogc, wiiload) |
| GameCube | devkitPro pacman + `wii-dev` (for the devkitPPC compiler) + `libogc2`, `libogc2-libdvm`, `gamecube-tools-git` |
| Nintendo 3DS | devkitPro pacman + `3ds-dev` (devkitARM, libctru, citro3d, 3dsxtool, smdhtool) |
| Nintendo 3DS (CIA) | `3ds-dev` plus the extra tools in [Packaging a 3DS installable](#packaging-a-3ds-installable-cia) |

1. Install devkitPro Pacman for macOS ([https://devkitpro.org/wiki/devkitPro_pacman](https://devkitpro.org/wiki/devkitPro_pacman)). Download the `.pkg` from the [devkitPro pacman releases](https://github.com/devkitPro/pacman/releases) and install it; it supports Intel and Apple Silicon Macs. It creates `/opt/devkitpro` and sets `DEVKITPRO`, `DEVKITPPC`, `DEVKITARM` in `/etc/profile.d/devkit-env.sh`.
2. Install the Wii/3DS toolchains ([https://devkitpro.org/wiki/Getting_Started](https://devkitpro.org/wiki/Getting_Started)). This is **required for GameCube too**: `wii-dev` provides devkitPPC (`powerpc-eabi-g++`), which the GameCube build compiles with even though its libraries come from `libogc2` in step 3.

   ```bash
   sudo dkp-pacman -S wii-dev 3ds-dev
   ```

   - Restart the terminal so the `DEVKIT*` variables are picked up, then check `powerpc-eabi-g++ --version` and `arm-none-eabi-g++ --version`.
3. Install `libogc2`, which GameCube builds link against ([https://github.com/extremscorner/pacman-packages#readme](https://github.com/extremscorner/pacman-packages#readme))

   ```bash
   sudo dkp-pacman-key --recv-keys C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE --keyserver keyserver.ubuntu.com
   sudo dkp-pacman-key --lsign-key C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE
   ```
   If `--recv-keys` fails with *keyserver receive failed: No route to host* (the HKP port 11371 is often blocked), fetch the key over HTTPS and import the file instead:

   ```bash
   curl -o ~/Downloads/libogc2-key.asc "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0xC8A2759C315CFBC3429CC2E422B803BA8AA3D7CE"
   sudo dkp-pacman-key --add ~/Downloads/libogc2-key.asc
   sudo dkp-pacman-key --lsign-key C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE
   ```
   - Add this entry to `/opt/devkitpro/pacman/etc/pacman.conf` above the existing `[dkp-libs]` entry. This is **file content, not commands**. The two `Server` lines are mirrors of the same repository (pacman falls back to the second if the first is unreachable), and both lines belong in the file:

     ```ini
     [libogc2-devkitpro]
     Server = https://packages.libogc2.org/devkitpro/macos/$arch
     Server = https://packages.extremscorner.org/devkitpro/macos/$arch
     ```
   - Then sync and install (accept overwriting if asked):

     ```bash
     sudo dkp-pacman -Syuu
     sudo dkp-pacman -S gamecube-tools-git libogc2 libogc2-libdvm
     ```

   - Check that `ls /opt/devkitpro/libogc2/lib/cube` lists `libogc.a`.

> Note: the `libogc2` packages are only the GameCube/Wii **libraries**. `wii-dev` and `3ds-dev` are the meta-packages that pull in the actual compilers, devkitPPC (`powerpc-eabi-g++`) and devkitARM (`arm-none-eabi-g++`). Without them the editor works, but GameCube/Wii/3DS packaging fails partway through `make` with a missing-compiler error.

Running the result: Wii builds go to a console over the network with `wiiload` (see [Platforms/Wii/Wiiload.md](../Platforms/Wii/Wiiload.md)) or into Dolphin. GameCube builds run in Dolphin. 3DS `.3dsx` files run in Citra/Azahar or from the Homebrew Launcher. Set the emulator paths in **Preferences > External > Launchers**.

#### Compile Shaders, libgit2, and Standalone embedded-asset stubs

```bash
bash Tools/prebuild_mac.sh
```

This is the macOS counterpart of `Tools/prebuild.sh`. It checks that `VULKAN_SDK`, `cmake` and the Command Line Tools are present, then runs three steps: builds `libgit2` (for the slices `MAC_ARCH` selects, SecureTransport TLS), compiles shaders with the SDK's `glslc`, and writes minimal stubs for `Standalone/Generated/EmbeddedAssets.{h,cpp}`, `EmbeddedScripts.{h,cpp}`, and `AddonPlugins.cpp`. The stub step only writes files that are missing. These are gitignored and normally regenerated by the Editor's "Build Data" action, but a fresh clone needs the stubs so the `Standalone` build succeeds.

### Architectures: Apple Silicon, Intel, universal

The Makefiles take a `MAC_ARCH` variable, on the command line or in the environment:

| `MAC_ARCH` | Builds | When |
|------------|--------|------|
| `native` (default) | This Mac's arch (`uname -m`: `arm64` on Apple Silicon, `x86_64` on Intel) | Day-to-day development |
| `universal` | arm64 + x86_64 in one binary | What the release `.dmg` ships. About twice the compile time |
| `arm64` / `x86_64` | A single slice, cross-compiled if it is not the host's | Testing the other architecture |

```bash
MAC_ARCH=universal bash Tools/prebuild_mac.sh      # libgit2 must be built for the same slices
make -C Standalone -f Makefile_Mac_Editor -j$(sysctl -n hw.ncpu) MAC_ARCH=universal
```

Switching `MAC_ARCH` prints `MAC_ARCH arm64 -> universal: cleaning ...` lines and rebuilds the affected objects. That is expected, not an error. Which directories get cleaned is described in [Platforms/Mac/Overview.md](../Platforms/Mac/Overview.md#architectures).

Intel Macs need nothing extra: the same Vulkan SDK, Command Line Tools and devkitPro installers work, and `native` builds x86_64. On Apple Silicon, install Rosetta 2 once (`softwareupdate --install-rosetta --agree-to-license`) to run the Intel slice of a universal build with `arch -x86_64 Standalone/Build/Mac/PolyphaseEditor`. A terminal running under Rosetta reports `uname -m` as `x86_64`, so `native` builds Intel there. `lipo -archs <binary>` shows which slices a file contains.

### Build and Run the Editor

**VS Code:** open the repo root, install the C/C++ Extension Pack, and pick **Polyphase Editor - Mac** in Run and Debug (it runs the **Make Standalone Editor - Mac** task, then launches under lldb with the repo root as the working directory). **Polyphase Game - Mac** does the same for the runtime.

**Terminal:**


1. From the repo root: `cd Standalone`
2. Run `make -f Makefile_Mac_Editor -j$(sysctl -n hw.ncpu)`
3. Go back to the root directory `cd ..`
4. Run `Standalone/Build/Mac/PolyphaseEditor`. As on Linux, run it from the repo root so `Engine/Assets` and `Engine/Shaders` resolve. A copy of the binary is also placed at the repo root after every build.

The game runtime used by packaging is built the same way: `make -f Makefile_Mac_Editor` is the editor, `make -f Makefile_Mac_Game` produces `Standalone/Build/Mac/Polyphase.macho`, which the Packaging window reuses for script-only projects.

Build outputs live under `Build/Mac/` and `Intermediate/Mac/` next to their Linux siblings. `Engine/Makefile_Mac`, `Standalone/Makefile_Mac_Editor`, `Standalone/Makefile_Mac_Game`, `External/Bullet/Makefile_Mac` and `External/Assimp/Makefile_Mac` mirror the Linux makefiles line for line: same targets, same variables, with Apple's `clang`/`ar`/`ld64` flags (`-arch <MAC_ARCH> -mmacosx-version-min=12.0`, `-Wl,-rpath,@loader_path`, `-dynamiclib`) and the platform sources under `Source/{System,Input,Audio,Network,Serial}/Mac`.

Keyboard note: the editor's Ctrl-based hotkeys accept the Command key as well (⌘S saves, ⌘Z undoes). F-keys need **Use F1, F2, etc. keys as standard function keys** in System Settings > Keyboard, or the Fn key.

### Packaging a macOS App Bundle

The **macOS (App Bundle)** build target compiles a Mach-O with `Makefile_Mac_Game` (slices per the profile's **Architecture** option: Native, Universal, arm64 or x86_64) and wraps it into `Packaged/Mac/<Project>.app` with MoltenVK and the Vulkan loader inside `Contents/Frameworks`. It needs the same tools as building the editor (Command Line Tools + Vulkan SDK); nothing else. macOS targets can only be built on a macOS host. The "Use Docker" option is disabled for them.

Signing: every bundle is at least ad-hoc signed so it runs on the machine that built it. For distribution set **Signing Identity** (`Developer ID Application: ...`) in the profile's Target Options, optionally **Notarize** with a keychain profile created by `xcrun notarytool store-credentials`, and tick **Create .dmg**. Details in [Platforms/Mac/Packaging.md](../Platforms/Mac/Packaging.md).

To ship the editor itself, `bash Installers/build_app_mac.sh` produces `dist/Polyphase.app` and `bash Installers/build_dmg_mac.sh` wraps it into `dist/PolyphaseEditor-<version>-macos-<universal|arm64|x86_64>.dmg` (see [Platforms/Mac/Overview.md](../Platforms/Mac/Overview.md)).

### Packaging a 3DS installable (.cia)

The plain **Nintendo 3DS** build target needs nothing beyond `3ds-dev` above. The **Nintendo 3DS (CIA)** target, which produces an installable HOME Menu title, shells out to tools that devkitPro does not ship. None of them are needed unless you use that target.

| Tool | Needed for | Where it comes from |
|------|-----------|---------------------|
| `makerom` | the `.cia` (required) | [3DSGuy/Project_CTR releases](https://github.com/3DSGuy/Project_CTR/releases), `makerom-v0.19.0-macos_arm64.zip` (Intel Macs: the `macos_x86_64` zip) |
| `bannertool` | HOME Menu banner and tune (optional) | No macOS binary is published. Build from source: [carstene1ns/3ds-bannertool](https://github.com/carstene1ns/3ds-bannertool) `bannertool-1.2.3.tar.gz`, then `make` and set the path in Preferences > External > Launchers |
| `cwavtool` | DSP-ADPCM tune encoding (optional, experimental) | [PabloMK7/cwavtool releases](https://github.com/PabloMK7/cwavtool/releases), `cwavtool.zip`, use `mac-x86_64/cwavtool` (runs under Rosetta on Apple Silicon) |
| Python 3.12 + pycgfx | 3D scene banners (optional) | `brew install python@3.12` and [skyfloogle/pycgfx](https://github.com/skyfloogle/pycgfx) |

**Easiest:** in the editor open **Preferences > External > Launchers**, scroll to **3DS CIA Tools**, and click **Download makerom + bannertool + cwavtool**. makerom and cwavtool are fetched from the release pages above into `~/Library/Application Support/PolyphaseEditor/Tools/3DS`, made executable, and picked up immediately (bannertool is skipped with a warning on macOS, see the table). Downloaded binaries are quarantined by Gatekeeper; if one refuses to run, `xattr -d com.apple.quarantine <tool>`.

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

- **`Failed to find platform surface extension` / `No physical device found`**: the Vulkan loader cannot see MoltenVK. Export `VK_DRIVER_FILES` (see above) or install the SDK under `~/VulkanSDK` so the editor can find it by itself. `vulkaninfo --summary` must list MoltenVK.
- **`ERROR: VULKAN_SDK is not set`** from `prebuild_mac.sh` or `make`: the shell did not load `~/.zshrc` (SSH, CI, a script). Export `VULKAN_SDK=$HOME/VulkanSDK/<ver>/macOS` in that shell.
- **`dyld: Library not loaded: @rpath/libvulkan.1.dylib`**: the editor was linked against a `VULKAN_SDK` that has since moved. Rebuild with the current `VULKAN_SDK`, or set `DYLD_LIBRARY_PATH=$VULKAN_SDK/lib` for that run.
- **Shaders fail to compile in `prebuild_mac.sh`**: `VULKAN_SDK` must contain `bin/glslc`. Check it points at `~/VulkanSDK/<ver>/macOS`, not the version directory above it.
- **`MAC_ARCH ... cleaning ...` lines and a long rebuild**: you changed the architecture (or a build profile's Architecture option did). The Makefiles discard objects of the other arch on purpose. `MAC_ARCH must be native, arm64, x86_64 or universal` means a typo in the value.
- **`ld: ... building for macOS-x86_64 but attempting to link with file built for macOS-arm64`**: a static library was built before the arch switch. Run `bash Tools/prebuild_mac.sh` (libgit2) with the same `MAC_ARCH`, or `make clean` in `Standalone` and rebuild.
- **`Validation layers requested (ValidateGraphics=1) but VK_LAYER_KHRONOS_validation is not available`**: a packaged game (or an editor started from Finder) cannot see the SDK's validation layer. The engine continues without validation instead of aborting. To validate a game, run it from a terminal with the SDK environment exported.
- **`ar: ... has no symbols`** warnings while building: harmless. Those objects come from sources compiled out on macOS.
- **`powerpc-eabi-g++: command not found` when packaging for Wii/GameCube** (or `arm-none-eabi-g++` for 3DS): `wii-dev` / `3ds-dev` are not installed, or the terminal was not restarted after installing devkitPro. `dkp-pacman -Q` lists what is installed.
- **GameCube link errors about missing `libogc2`**: step 3 of the devkitPro section was skipped. The stock `libogc` from `wii-dev` is not enough for GameCube.
- **`wss://` connections report "no WebSocket support"**: install Homebrew curl and set `POLYPHASE_LIBCURL` as described above.
- **Gatekeeper blocks a downloaded editor or game**: ad-hoc signed builds are not notarized. Right-click > Open once, or `xattr -dr com.apple.quarantine <app>`.
- **Debugging**: `lldb Standalone/Build/Mac/PolyphaseEditor` from the repo root, or VS Code with the CodeLLDB extension pointing at the same binary with `cwd` set to the repo root.
