# Quick Start

This guide walks you through installing the Polyphase Engine, creating a project, building a scene, and packaging it for Wii.

---

## 1. Install Editor Dependencies

Before using the engine, you need to set up your development environment based on your operating system.

## Windows

Follow the[Windows Setup](../Development/SetupEnvironment/Windows.md) guide to install:

- Visual Studio Community 2022 (with C++ support)
- Vulkan SDK 1.3.275.0
- devkitPro toolchains (for console targets like Wii)

## Linux

Follow the[Linux Setup](../Development/SetupEnvironment/Linux.md) guide to install:

- Build essentials (`g++`, `make`, `libx11-dev`, `libasound2-dev`)
- Vulkan SDK 1.3.275.0
- devkitPro toolchains (for console targets like Wii)

---

## 2. Get the Editor

You have two options:

### Option A: Build from Source

Follow the guide for your preferred workflow:

- **Visual Studio Code** -- See [Compiling (VS Code)](../Development/SetupEnvironment/VSCode.md): Open the project root in VS Code, install the C/C++ Extension Pack, and run the `Polyphase Editor` debug config.
- **Terminal** -- See [Compiling (Terminal)](../Development/SetupEnvironment/Terminal.md): Build with `make` and run the editor binary directly.
- **Visual Studio** -- See [Compiling](../Development/SetupEnvironment/Compiling.md): Open `Polyphase.sln`, switch to the `DebugEditor` configuration, and build the `Standalone` project.
- Make sure to run ``git submodule update --init --recursive`` in your Polyphase directory to make sure the submodules got cloned/updated

### Option B: Download a Prebuilt Release

Go to the **Releases** page and download the installer for your platform. Installers are provided for supported operating systems -- just run the installer and follow the prompts.

---

## 3. Create a New Project

1. Launch the Polyphase Editor.
2. In the **Recent Projects** window, click **Create New Project**.
3. Choose a name and location for your project, then confirm.

---

## 4. Create a Scene

1. In the **Asset Panel**, right-click and select **Create Asset > Scene**.
2. Name your new scene (e.g. `MyScene`).
3. Double-click the scene to open it.
4. In the **Scene Hierarchy**, right-click and select **Add New > Static Mesh** to add a mesh to the scene.
5. Save the scene with **Ctrl+S** (or **File > Save**).

---

## 5. Add Your Scene to the Default Scene

The project starts with a default scene called `SC_Default`. You need to add your new scene to it:

1. Open `SC_Default` by double-clicking it in the Asset Panel.
2. In the **Scene Hierarchy**, right-click and select **Add New**.
3. Find and select the scene you just created (e.g. `MyScene`).
4. Save `SC_Default`.

---

## 6. Package for Wii

1. Go to **File > Packaging**.
2. Set up a new **Wii** build profile.
3. Click **Build**.

Once the build completes, you will have a `.dol` file ready to run on Wii hardware or an emulator.
