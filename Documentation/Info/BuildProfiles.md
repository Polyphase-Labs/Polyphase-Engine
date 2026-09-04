# Build Profiles

Build Profiles let you save packaging configurations for different platforms and quickly build your game with one click.

## Opening the Packaging Window

Go to **File > Build Profiles** to open the Packaging window.

## Creating a Build Profile

1. Click the **+** button in the Build Profiles list
2. Give your profile a name (e.g., "GameCube Release")
3. Configure the settings:
   - **Platform** - Select your target platform (Windows, Linux, GameCube, Wii, 3DS, etc.)
   - **Embedded Mode** - Embeds assets into the executable (required for console platforms)
   - **Output Directory** - Optional custom location for built files (leave empty for default)
   - **Use Docker** - Forces Docker-based builds (see Docker section below)

## Building Your Game

1. Select a profile from the list
2. Click **Build** to package your game
3. Click **Build & Run** to package and automatically launch in an emulator

## Build & Run Setup

To use Build & Run, you need to configure your emulator paths:

1. Click the gear icon next to "Build & Run"
2. This opens **Preferences > External > Launchers**
3. Set the paths for your emulators:

| Platform | Emulator |
|----------|----------|
| GameCube | Dolphin |
| Wii | Dolphin |
| 3DS | Azahar or Citra |

### Custom Launch Arguments

You can customize how games launch using placeholders:

- `{emulator}` - The emulator executable path
- `{output}` - The built game file path
- `{outputdir}` - The output directory

**Examples:**
- Dolphin fullscreen: `{emulator} -e {output} -f`
- Dolphin with debugger: `{emulator} -e {output} -d`

### 3DS Hardware (3dslink)

To send games directly to your 3DS:

1. Set the **3dslink Path** in Preferences
2. Select **Hardware** under "3DS Launch Method"
3. When you Build & Run, ensure your 3DS has Homebrew Launcher open and ready

### 3DS Targets and Options

Two built-in targets share the 3DS platform:

| Target | Output | Use |
|--------|--------|-----|
| Nintendo 3DS | `Packaged/N3DS/<Project>.3dsx` | Homebrew Launcher / 3dslink / Azahar |
| Nintendo 3DS (CIA) | `Packaged/polyphase.n3ds.cia/<Project>.3dsx` + `.cia` | Installable HOME Menu title (FBI, Azahar File > Install CIA) |

Both show a **3DS Target Options** block for the SMDH metadata (Title, Description, Author, 48x48 Icon). The CIA target adds product code, unique id, version, an optional custom RSF, and the HOME Menu banner settings (image or 3D scene, tune, loop). The CIA target needs `makerom` (and optionally `bannertool`, Python + pycgfx for banners). Install them from **Preferences > External > Launchers > 3DS CIA Tools** with the download buttons, or see [3DS Overview](../Development/Platforms/3DS/Overview.md#installable-cia) for manual setup on Windows and Linux.

## Docker Builds
### Requirements

- [Docker Desktop](https://www.docker.com/products/docker-desktop/) installed and running
- The `polyphase-engine` Docker image built (see Docker.md)

### When Docker is Used

| Host OS | Platform | Docker Required |
|---------|----------|-----------------|
| Windows | GameCube | Yes (automatic) |
| Windows | Wii | Yes (automatic) |
| Windows | 3DS | Yes (automatic) |
| Windows | Windows | No |
| Linux | Any | Optional (checkbox) |

### Troubleshooting

**"Docker Desktop is not running"**
- Open Docker Desktop and wait for it to start
- Try the build again

**"Docker image not found"**
- Build the Docker image first (see Docker.md)
- Try `wsl docker ...`

## Profile Storage

Build profiles are saved per-project in `{ProjectDir}/Settings/BuildProfiles.json` and travel with your project.

Emulator settings are saved in your user preferences and persist across all projects.
