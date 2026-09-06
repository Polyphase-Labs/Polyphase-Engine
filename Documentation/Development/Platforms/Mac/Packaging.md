# Packaging for macOS

The **macOS (App Bundle)** target (`polyphase.mac`) is the canonical build target for `Platform::Mac`. It reuses the Linux-style Makefile compile path (`Makefile_Mac_Game`) and adds a `PostPackage` step, implemented in `Engine/Source/Editor/Packaging/MacBundlePackager.cpp`, that wraps the result into a signed `.app`.

## Requirements

- A macOS host with the Xcode Command Line Tools and the LunarG Vulkan SDK (see [SetupEnvironment/Mac.md](../../SetupEnvironment/Mac.md)). The target's `Validate` reports what is missing.
- Docker is not available for this target.

## What a build produces

```
<Project>/Packaged/Mac/
    <Project>.macho           loose arm64 Mach-O (kept, like the .elf beside an AppImage)
    Config.ini, <Project>.octp, <Project>/, Engine/, Addons/ ...   loose payload
    <Project>.app/            the deliverable
    <Project>.dmg             optional (Create .dmg)
```

Inside the bundle, `Contents/Resources/` holds the loose payload, `Contents/Frameworks/` holds `libvulkan.1.dylib` and `libMoltenVK.dylib` copied from the Vulkan SDK, and `Contents/Resources/vulkan/icd.d/MoltenVK_icd.json` points the loader at the bundled MoltenVK. Game addons are copied to `Contents/MacOS/Addons/`.

Script-only projects reuse the prebuilt runtime at `Standalone/Build/Mac/Polyphase.macho`; projects with C++ or native addons compile through `Makefile_Mac_Game` (`make -C <BuildProjDir> -f Makefile_TEMP -j 12`, then `strip -S`).

## Target Options

| Key | Option | Default | Notes |
|-----|--------|---------|-------|
| `mac.bundleId` | Bundle Identifier | `com.polyphase.<ProjectName>` | `CFBundleIdentifier` |
| `mac.version` | Version | `1.0.0` | `CFBundleShortVersionString` / `CFBundleVersion` |
| `mac.minOsVersion` | Minimum macOS | `12.0` | `LSMinimumSystemVersion` |
| `mac.iconPath` | Icon (PNG) | project PNG icon, else the engine logo | Converted with `sips` + `iconutil` |
| `mac.signingIdentity` | Signing Identity | ad-hoc (`-`) | e.g. `Developer ID Application: Name (TEAMID)` |
| `mac.notarize` | Notarize | off | Needs an identity and `mac.notaryProfile` |
| `mac.notaryProfile` | Notary Keychain Profile | | From `xcrun notarytool store-credentials` |
| `mac.createDmg` | Create .dmg | off | `hdiutil` image with an Applications shortcut |

Changing any option changes the build-cache variant key, so the bundle is re-wrapped on the next build even when the cook is up to date.

## Command line

```bash
Standalone/Build/Mac/PolyphaseEditor -headless -project /path/to/Game/Game.octp -build Mac embedded
codesign --verify --deep --strict --verbose=2 /path/to/Game/Packaged/Mac/Game.app
open -n /path/to/Game/Packaged/Mac/Game.app
```

`Tools/CI/TestBuildProject/verify_project_build.sh <editor> <project> Mac` runs the same build and verifies the bundle and its signature.

## Run after build

**Build & Run** launches the `.app` with `open -n` so LaunchServices treats it exactly like a user double-click (Dock icon, signature evaluation). Saves from a bundle go to `~/Library/Application Support/<Project>/Saves/` because the bundle is read-only.
