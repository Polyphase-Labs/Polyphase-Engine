# Compiling (Visual Studio Code)

1. Open the root polyphase folder in VsCode (where this README is located).
2. Install the C/C++ Extension Pack.
3. Run the Polyphase Editor config (click the Run and Debug tab on the left, then in the drop down where the green Play button is, select `Polyphase Editor`).


# Compiling (Terminal)

1. From the project's root directory (where this README is located) `cd Standalone`
2. Run `make -f Makefile_Linux_Editor`
3. Go back to the root directory `cd ..`
4. Run `Standalone/Build/Linux/PolyphaseEditor.elf` It's important that the working directory is the root directory where the Engine and Standalone folders are located.

# Compiling (Terminal, macOS)

1. Follow [macOS setup](Mac.md) once (Command Line Tools, Vulkan SDK, `bash Tools/prebuild_mac.sh`).
2. From the project's root directory `cd Standalone`
3. Run `make -f Makefile_Mac_Editor -j$(sysctl -n hw.ncpu)`
4. Go back to the root directory `cd ..`
5. Run `Standalone/Build/Mac/PolyphaseEditor` from the root directory (same working-directory rule as Linux). `make -f Makefile_Mac_Game` builds the game runtime that packaging reuses.

## Packaging

1. When packaging for any platform on a Windows machine, you will likely need to install Msys2 so that linux commands can be executed. This comes packaged along with devkitPro libraries, so you if you install the devkitPro libraries, you shouldn't need to worry about this.
3. Packaging for macOS (the `macOS (App Bundle)` target) requires a macOS host with the Xcode Command Line Tools and the Vulkan SDK; see [Platforms/Mac/Packaging.md](../Platforms/Mac/Packaging.md). It cannot be built through Docker.
2. Packaging for Android requires installing Android Studio (Last tested with Android Studio 2022.2.1 Patch 2) with the following tools installed via the SDK Manager:
   - Android SDK Build Tools: 34.0.0
   - Android NDK (Side by side): 25.2.9519653
   - CMake: 3.22.1

## CMake Support

CMake support is currently a work-in-progress, and only Linux support has been implemented and tested. If you want to try building with CMake, here are some tips:

- Make sure you pull all submodules `git submodule update --init --recursive`
- Install pkg-config `sudo apt install pkg-config`(debian/ubuntu), `
- Install vorbis dev libraries `sudo apt install libvorbis-dev`



# Docker

Polyphase includes a Docker build system for reproducible builds across all supported platforms. You can also use the Docker system to build Polyphase itself from source without installing any dependencies on your host machine. You can get more information about using the Docker build system at [Documentation/Docker.md](Documentation/Info/Docker.md).

## Requirements

- Install Docker from <https://docs.docker.com/get-docker/>

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
