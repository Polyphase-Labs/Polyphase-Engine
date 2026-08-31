
![Image](https://github.com/Polyphase-Labs/Polyphase-Engine/blob/main/PolyphaseLogo_256.png?raw=true)

# Polyphase

A 3D Game Engine for GameCube, Wii, 3DS, Windows, Linux, and Android
This is a modernized fork of [Octave Game Engine by mholtkamp/octave](https://github.com/mholtkamp/octave) .

[Tutorial Video (Octave v5)](https://youtu.be/0CHLn0ie-DY)

[Lua Documentation](https://Polyphase-Labs.github.io/Polyphase-Engine/Lua/)

[C++ API Documentation](https://Polyphase-Labs.github.io/Polyphase-Engine/api/annotated.html)

### Lua IntelliSense (VS Code)

For autocompletion and type checking in Lua scripts, install the [Lua Language Server](https://marketplace.visualstudio.com/items?itemName=sumneko.lua) extension and add the following to your workspace `.vscode/settings.json`:

```json
{
  "Lua.workspace.library": [
    "Engine/Generated/LuaMeta"
  ]
}
```

[Docker Documentation](Documentation/Docker/README.md)

[Editor Hotkeys](Documentation/Info/Editor.md)

**Check out the Releases page for precompiled standalone builds.**

---

Instructions for building from source below.

## Windows Compiling

1. Download and Install:

   - Visual Studio Community 2022 (with C++ support)
   - Vulkan SDK version 1.3.275.0 (During install select "Shader Toolchain Debug Symbols - 64 bit" and deselect all other options)
   - devkitPPC for GameCube/Wii development
   - devkitARM for 3DS development
   - Instructions for installing the devkitPro toolchains can be found in the devkitPro wiki [here](https://devkitpro.org/wiki/Getting_Started)
2. Install devkitPro:

   - Open your Start Menu and launch `devkitPro > MSys2`
   - `pacman-key --recv-keys C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE --keyserver keyserver.ubuntu.com`
   - `pacman-key --lsign-key C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE`
   - Add this entry to `C:\devkitPro\msys2\etc\pacman.conf` above the existing `[dkp-libs]` entry (file content, not commands — the two `Server` lines are mirrors of the same repository):

   ```ini
   [libogc2-devkitpro]
   Server = https://packages.libogc2.org/devkitpro/windows/$arch
   Server = https://packages.extremscorner.org/devkitpro/windows/$arch
   ```

   - `pacman -Syuu`
   - `pacman -S wii-dev 3ds-dev gamecube-tools-git libogc2 libogc2-libdvm`
     - Accept overwriting if asked.
     - `wii-dev` / `3ds-dev` pull in the actual compilers (devkitPPC's `powerpc-eabi-g++`, devkitARM's `arm-none-eabi-g++`); the `libogc2` packages are only the libraries.
   - Restart computer if you've opened Visual Studio prior to installing the toolchains to make sure the environment variables are found.
3. Build shaders by running compile.bat in `/Engine/Shaders/GLSL`.
4. Open Polyphase.sln.
5. Switch to the DebugEditor solution configuration.
6. Set the Standalone project as the Startup Project.
7. In the debug settings for Standalone, change the working directory to $(SolutionDir).
8. Build and run Standalone. This is the standalone level editor if you were making a game with Lua script only.

## Linux Compiling

### Installing Dependencies

1. For Debian/Ubuntu based distros:
   `sudo apt install g++ make libx11-dev libasound2-dev`
   For Arch-based distributions:
   `sudo pacman -S gcc make libx11 alsa-lib`
   Note: arch users may get a dependency error when attempting to install `alsa-lib`, in this case try to install `lib32-alsa-lib`.
2. Install Vulkan SDK version 1.3.275.0:

- Download the 1.3.275.0 tar file from [https://vulkan.lunarg.com/sdk/home#linux](https://vulkan.lunarg.com/sdk/home#linux)
- Extract the tar file somewhere (e.g. ~/VulkanSDK/)
- Add these to your ~/.bashrc file (replace `~/VulkanSDK` with the directory where you extracted the files to). You may instead add these to a .sh file in your /etc/profiles.d directory to set up Vulkan for all users.

  ```
  export VULKAN_SDK=~/VulkanSDK/1.3.275.0/x86_64
  export PATH=$VULKAN_SDK/bin:$PATH
  export LD_LIBRARY_PATH=$VULKAN_SDK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
  export VK_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d
  ```
- Close and reopen your terminal to apply the .bashrc (or run `source ~/.bashrc`)

1. Install devkitPro Pacman for GameCube/Wii/3DS development (Optional) ([https://devkitpro.org/wiki/devkitPro_pacman](https://devkitpro.org/wiki/devkitPro_pacman))

   ```bash
   wget https://apt.devkitpro.org/install-devkitpro-pacman
   chmod +x ./install-devkitpro-pacman
   sudo ./install-devkitpro-pacman
   ```
2. Install the Wii/3DS toolchains ([https://devkitpro.org/wiki/Getting_Started](https://devkitpro.org/wiki/Getting_Started)). Required for GameCube too — `wii-dev` provides devkitPPC (`powerpc-eabi-g++`), which the GameCube build compiles with even though its libraries come from `libogc2` in step 3.

   ```bash
   sudo dkp-pacman -S wii-dev 3ds-dev
   ```

   - Restart computer
3. If you want to package for GameCube, install `libogc2` ([https://github.com/extremscorner/pacman-packages#readme](https://github.com/extremscorner/pacman-packages#readme))

   - `sudo dkp-pacman-key --recv-keys C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE --keyserver keyserver.ubuntu.com`
   - `sudo dkp-pacman-key --lsign-key C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE`
   - Add this entry to `/opt/devkitpro/pacman/etc/pacman.conf` above the existing `[dkp-libs]` entry (file content, not commands — the two `Server` lines are mirrors of the same repository):

   ```ini
   [libogc2-devkitpro]
   Server = https://packages.libogc2.org/devkitpro/linux/$arch
   Server = https://packages.extremscorner.org/devkitpro/linux/$arch
   ```

   - `sudo dkp-pacman -Syuu`
   - `sudo dkp-pacman -S gamecube-tools-git libogc2 libogc2-libdvm`
     - Accept overwriting if asked.
4. cd Engine/Shaders/GLSL/ then run ./compile.sh.

### Compiling (Visual Studio Code)

1. Open the root polyphase folder in VsCode (where this README is located).
2. Install the C/C++ Extension Pack.
3. Run the Polyphase Editor config (click the Run and Debug tab on the left, then in the drop down where the green Play button is, select `Polyphase Editor`).

### Compiling (Terminal)

1. From the project's root directory (where this README is located) `cd Standalone`
2. Run `make -f Makefile_Linux_Editor`
3. Go back to the root directory `cd ..`
4. Run `Standalone/Build/Linux/PolyphaseEditor.elf` It's important that the working directory is the root directory where the Engine and Standalone folders are located.

## Packaging

1. When packaging for any platform on a Windows machine, you will likely need to install Msys2 so that linux commands can be executed. This comes packaged along with devkitPro libraries, so you if you install the devkitPro libraries, you shouldn't need to worry about this.
2. Packaging for Android requires installing Android Studio (Last tested with Android Studio 2022.2.1 Patch 2) with the following tools installed via the SDK Manager:
   - Android SDK Build Tools: 34.0.0
   - Android NDK (Side by side): 25.2.9519653
   - CMake: 3.22.1
3. The Linux installer targets (RPM / AppImage) require `rpmbuild` and `appimagetool` on the host (inside WSL2 on Windows). See [Packaging Linux Installers](Development/SetupEnvironment/Linux.md#packaging-linux-installers-rpm--appimage).

### Protecting shipped content

Build profiles offer two options that stop packaged content from being casually
read or edited:

- **Static Content** — cooked assets, scripts and the asset registry are
  obfuscated on disk and decoded on demand at load time.
- **Content Pak** — additionally folds everything into a single `Content.pak`
  with an obfuscated index, so filenames and the folder tree don't ship either.
  A packed build distributes as just the executable plus `Content.pak`.

Both work on every platform including consoles and custom build targets, with no
per-platform work. This is obfuscation, not DRM — the key ships in the
executable. See
[Documentation/Development/StaticContent.md](Development/StaticContent.md).

## CMake Support

CMake support is currently a work-in-progress, and only Linux support has been implemented and tested. If you want to try building with CMake, here are some tips:

- Make sure you pull all submodules `git submodule update --init --recursive`
- Install pkg-config `sudo apt install pkg-config`(debian/ubuntu), `
- Install vorbis dev libraries `sudo apt install libvorbis-dev`

# Docker

Polyphase includes a Docker build system for reproducible builds across all supported platforms. You can also use the Docker system to build Polyphase itself from source without installing any dependencies on your host machine. You can get more information about using the Docker build system at [Documentation/Docker.md](Documentation/Info/Docker.md).

## Requirements

- Install Docker from [https://docs.docker.com/get-docker/](https://docs.docker.com/get-docker/)

## Build the Polyphase Docker Image

From your terminal, run:

```bash
# Clone the Polyphase repository if you haven't already, or to get the latest version
git clone https://github.com/polyphase-engine/polyphase-engine
# Move into the polyphase directory
cd polyphase-engine
# Build the Docker image
./Docker/build.sh
```

## Packaging Games With Docker

To package your game using the Docker build system, run the following command from the root of your project directory (where your .octp file is located):

```bash
docker run --rm -v ./dist/3DS:/game -v .:/project polyphase-engine build-3ds
```

This command mounts your project directory to `/project` in the Docker container, and tells the system to export your file to `./dist`. You should create the `dist` directory or whatever you want to export to beforehand or else the directory will be created by Docker and you will have to `sudo chmod -R 777 ./dist` to change permissions so you can access it.
 or do a `sudo rm -rf ./dist` to delete the directory.

### Available Docker Build Commands

- `build-linux` - Build a Linux `.elf` executable
- `build-gamecube` - Build a GameCube `.dol` file
- `build-wii` - Build a Wii `.dol` file
- `build-3ds` - Build a Nintendo `.3dsx` ROM

# Special Thanks

- Polyphase logo designed by overcookedchips.
