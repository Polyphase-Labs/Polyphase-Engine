# Frequently Asked Questions


# Black Screen - Built Game

## Log Files
1. Go to `Edit > App Settings > Runtime > Log To File` and enable it.
2. Build your game again and run it.
3. After the game crashes, go to the `./Polyphase.log` file in your game's directory and open it with a text editor.
4. Look for any error messages or warnings that might indicate the cause of the black screen.

## No `Camera3D` in the scene
A scene with no active `Camera3D` never performs the full-window paint that the
post-process step normally does, so anything your UI doesn't cover stays black.
This bites pure-UI scenes hardest (menus, HUD-only scenes, kiosk screens) —
add a `Camera3D` even when the scene has no 3D content. See
[Android > Parts of the UI are missing](#parts-of-the-ui-are-missing--large-black-bands-or-most-of-the-screen-unpainted)
for the mechanism; it is not Android-specific, but Android is where it shows up
most reliably.

## 	Check Project Directory Structure vs Project Name
You project directory that the `{ProjectName}.oct` file is in must be named the same as the project name. For example, if your project is named "MyGame", the directory should be named "MyGame" and contain the `MyGame.oct` file. If there is a mismatch, the game will not load at all after a successful build.

## `Polyphase.log` is empty while the game is running
The log file is opened line-buffered and flushed per write, so once logging is enabled every complete line lands on disk immediately. If the log is still empty after the game has clearly produced output, check:
- `Config.ini` has both `Logging=1` **and** `LogToFile=1`. `Logging=0` compiles-in but silences all `LogDebug/Warning/Error` calls.
- The log is written to the game's working directory (the folder containing the `.exe`), not the project directory. It is named `{ProjectName}.log`, falling back to `Polyphase.log` if the project name is not yet set at init.


# Android

## Seeing logs from an Android build

`adb logcat -s Polyphase:V` is the fastest and most reliable channel — every
`LogDebug` / `LogWarning` / `LogError` is routed to Android's log under the tag
`Polyphase`, whether or not `Log To File` is enabled. Leave it running while you
reproduce the problem; a capture taken only at launch usually misses the
interesting part.

Two things silence it:
- `Config.ini` must have `Logging=1`. With `Logging=0` every `Log*` call returns
  immediately and nothing reaches logcat *or* the file.
- Settings changed in *App Settings* only take effect in the **next package**.
  Toggling `Log To File` does not alter an APK already installed on the device.

## `Log To File` produces no file on Android

Unlike desktop, the log is written to the app's private internal storage
(`ANativeActivity::internalDataPath`), not a working directory — an Android
NativeActivity has no writable current directory, so a relative path silently
fails. Retrieve it with:

```
adb shell run-as com.your.applicationid cat files/{ProjectName}.log
```

Prefer `adb logcat` (above) for live debugging.

## Parts of the UI are missing — UI offset into a corner, or large black bands

**Cause: the scene has no `Camera3D`.** Confirmed by A/B test — a UI-only scene
with no camera renders its UI offset and leaves large regions unpainted; adding
a `Camera3D` fixes it. Most visible on Android; desktop can mask it, so it is
easy to ship without noticing.

**This is now handled automatically.** `World::EnsureFallbackCamera()` spawns a
transient *"Fallback Camera"* whenever the loaded scene provides none, and
retires it the moment a real `Camera3D` registers (including one arriving later
from a scene instantiated into the root, or a streamed-in level). You will see
this warning once when it kicks in:

```
World: scene '<name>' has no Camera3D -- using a transient fallback camera.
```

The fallback is a safety net, not a recommendation — **add a real `Camera3D` to
the scene**. The stand-in sits at `(0, 0, 10)` with default settings, which is
almost certainly not the framing you want if the scene has any 3D content.

If you are on an older engine build without the fallback, add a `Camera3D`
manually. And when you do, check *what filled the gap*: a camera also enables
`Skybox3D`, so a skybox painting previously-black regions can mask a UI that
never reached the screen edges.

**A related engine bug was fixed at the same time.** The UI pass set the
viewport but not the scissor, so it inherited the **scene** viewport (the window
viewport scaled by *Resolution Scale*). With Resolution Scale below 1.0 the
entire UI was clipped to that smaller rectangle. If you saw UI clipped into a
corner on an older build, check Resolution Scale.

## What `Scissor` does, and when to turn it off

`Scissor` clips a widget's subtree to its rectangle — both **rendering**
(`Widget::Render` -> `GFX_SetScissor`) and **input** (`ContainsMouse` hit-tests
the clipped rect, so content scrolled out of view can't be clicked). The input
half is what makes it load-bearing rather than cosmetic.

These classes enable it themselves in `Create()`, so seeing it on is normal and
not a scene-authoring mistake: `Button`, `Canvas`, `InputField`,
`ScrollContainer`, `Slider`, `Window`.

- **Leave it on** for `ScrollContainer`, `InputField`, `Window` — clipping is
  their entire function, and disabling it also makes invisible off-screen
  content clickable.
- **Safe to turn off** on a `Button` if you need to: its only clipped content is
  the auto-created child `Text`, so the sole consequence is that an overlong
  label overflows the button rect.
- Only **drawable** widgets ever apply a scissor. `Canvas` and plain `Widget`
  return no draw data, so `Widget::Render()` never runs for them and their
  `Scissor` flag has no direct effect of its own (it still bounds descendants
  via the parent-clamp in `UpdateRect`).
- CSS `overflow: hidden` maps to it (`UITypes.cpp`), but `overflow: visible` is
  currently a no-op — so a class default can't be overridden from a stylesheet.

## A runtime-generated texture (photo snapshot, video frame, procedural image) renders black on Android but is fine on Windows

Applies to a texture that is created, uploaded **once**, and displayed — as
opposed to a live feed that re-uploads every frame.

The GPU upload was historically submitted without waiting for completion. A
continuously-updating texture hides that (the next frame's upload corrects it),
but a one-shot texture has no next upload, so on slower mobile GPUs it can be
sampled before its pixels have landed — and then stays black forever. The engine
now waits for the initial `Texture::Create()` upload; if you see this on an
older engine build, update.

If it is still black, verify the source pixels are actually non-black before
blaming the GPU — a camera pointed at a dark room produces a legitimately black
image, which looks identical to this bug.

## A native addon behaves as if its platform code isn't there on Android

Symptoms: a feature that works on desktop silently does nothing on device, an
addon falls back to its stub/no-op path, or libraries declared in
`package.json` fail to link.

The `nativePerPlatform` block is matched by a **case-sensitive** key, so the
platform section must be spelled exactly `"Android"`. To confirm what the
Android build actually received, package for Android and read
`Standalone/Generated/AddonInject.cmake` — `POLYPHASE_ADDON_DEFINES` and
`POLYPHASE_ADDON_LIBS` should contain your entries. If they are empty, the
per-platform block never applied.

Also note `POLYPHASE_ADDON_LIBS` feeds CMake's `target_link_libraries`
directly: list bare library names (`camera2ndk`), not `-l`-prefixed flags.

## Which changes need the editor rebuilt before packaging for Android?

Only **editor-only** code — anything under `#if EDITOR`, such as
`ActionManager.cpp` (which implements packaging itself). Ordinary engine source,
including the Vulkan renderer, is recompiled from scratch by the Android
NDK/CMake pass on every *Package -> Android*, so an engine runtime fix needs
only a repackage. Lua scripts and addon C++ likewise need only a repackage —
though with **Embedded Mode** on, scripts are baked into the binary at package
time, so editing a `.lua` still requires repackaging and reinstalling; there is
no hot-reload into an installed APK.

## An Android feature needs a runtime permission (camera, microphone, location)

Android permissions can only be requested by the Java `Activity`, and the answer
arrives asynchronously — no native call can block waiting for it. The engine's
pattern (see the `com.polyphase.formats.webcam` addon) is:

1. Declare `<uses-permission>` in
   `Standalone/Android/app/src/main/AndroidManifest.xml`.
2. Add `has*Permission()` / `request*Permission()` / `get*PermissionState()`
   methods to `PolyphaseActivity.java`.
3. Call them over JNI from native code.
4. **Poll, don't block.** Fire the request once, return failure, and retry
   later. `WebcamPlayer3D` does this automatically via its
   `Retry On Open Failure` / `Open Retry Interval` properties, so the feed
   simply appears a second after the user taps Allow.

If a permission dialog never appears at all, check that the permission is
requested from *every* entry point that needs it — some devices return empty
results from enumeration APIs until permission is granted, so code that only
requests on "open" can fail earlier and never ask.


# Native addon won't load on the installed editor (works fine in VS)

Symptom: a project with a native addon (e.g. `com.polyphase.formats.video`) opens cleanly under the VS-built `ReleaseEditor` / `DebugEditor` from `Standalone\Build\Windows\x64\...\Polyphase.exe`, but under the installer-built `C:\Polyphase\Polyphase.exe` you get either:

- `Failed to construct node '<X>' (type=N, unknown type?), using Node3D placeholder.` in the editor log, or
- The addon's nodes silently fall back to plain `Node3D` placeholders, or
- A script that depends on the addon's types fails to find them.

This means **the addon DLL was never produced or never loaded**, so its `RegisterTypes` callback never ran. The Scene-side warning is downstream of the real failure.

## Diagnose

1. Open `{ProjectDir}\Intermediate\Plugins\<addonId>\` and look at the most recently modified `<config>_<hash>\` subfolder. If it contains **only `build.bat` and no `.dll`**, the editor tried to build the addon and the build failed.
2. Open the `build.bat` and run it manually from a Visual Studio Developer Command Prompt (or a shell that has already called `vcvars64.bat`):
   ```
   cd /d M:\path\to\Project\Intermediate\Plugins\<addonId>\<config>_<hash>
   build.bat
   ```
3. Read the linker output. The most common failures:
   - `LNK1181: cannot open input file 'Polyphase.lib'` (or `Lua.lib`, or an FFmpeg lib) — the installed editor is missing an import library.
   - `LNK2019: unresolved external symbol` — addon source references engine symbols that aren't exported, or the linked `.lib` is from a different engine version than the running `.exe`.
   - `fatal error C1083: Cannot open include file: ...` — the staged `Engine\Source\` SDK is missing files the addon's `#include` directives rely on.

## Fix: missing import libraries

Check what's actually in the install root:

```
dir C:\Polyphase\*.lib
```

You should see at least `Polyphase.lib` and `Lua.lib`. If they're absent, the installer was built without bundling them — this is a packaging bug, not anything wrong with your project. The pipeline that should have shipped them:

| File | What it does | What can go wrong |
|---|---|---|
| `Standalone.vcxproj` (or per-platform Makefile) | Engine build produces `Polyphase.lib` / `Lua.lib` | Configuration didn't build, or wrong output path |
| `Installers/stage_distribution.py:277-306` | Copies them to `dist\Editor\` | `copy_file` warning printed but not fatal |
| `Installers/Windows/PolyphaseSetup.iss` `[Files]` | Bundles them into `PolyphaseSetup-*.exe` | **Missing `Source:` line — most common omission** |
| `Installers/build_installer_windows.bat` | Gates that staging produced them | Skips if `POLYPHASE_SKIP_RUNNING_CHECK` is misused |
| `.github/workflows/release.yml` | Same gates in CI | None — CI is authoritative |

To verify what's *inside* a built installer without installing it:

```
"C:\Program Files\7-Zip\7z.exe" l dist\PolyphaseSetup-*.exe | findstr /I "Polyphase.lib Lua.lib"
```

Both names should appear. If they don't, `PolyphaseSetup.iss` is missing the `Source:` lines under the `sdk` component. See `Documentation/Development/Contributing.md` for the full add-an-artifact checklist.

## Fix: stale fingerprint folder masking a real rebuild

`ComputeFingerprint` hashes absolute source-file paths plus the CRT config tag, so the installed editor and the dev editor never share a `<config>_<hash>\` folder. If the install was previously broken and you've since reinstalled, an older empty fingerprint folder may still be sitting around. After fixing the install, force a fresh build by deleting the project's intermediate addon dir:

```
rmdir /S /Q {ProjectDir}\Intermediate\Plugins\<addonId>
```

The editor will regenerate it from scratch on next open.

## Fix: missing runtime DLL dependencies

If the addon DLL builds and links but loading still fails (look for `LoadLibrary` errors in the editor log), check the addon's `package.json` for a `copyBinaries` field — those directories must contain matching DLLs alongside the addon DLL after the post-build `xcopy` in `build.bat`. For FFmpeg-using addons, the `External\ffmpeg\bin\` DLLs must end up in the addon's fingerprint folder.

## When everything looks correct but it still fails

If `Polyphase.lib` and `Lua.lib` are present, `build.bat` runs cleanly to "Build succeeded", and the DLL exists in the fingerprint folder, but the editor still prints `unknown type?`:

- Compare the editor's CRT (`/MD` vs `/MDd`) against the fingerprint prefix (`rel_` vs `dbg_`). A mismatch indicates the editor's `_DEBUG` macro disagrees with how it was actually linked — this should be impossible from a stock build but can happen with hand-mixed configs.
- Check the addon's `package.json` `entrySymbol` (typically `PolyphasePlugin_GetDesc`) is actually exported by the built DLL: `dumpbin /exports {fingerprint}\<addonId>.dll | findstr PolyphasePlugin_GetDesc`. If absent, the addon's source is missing the `OCTAVE_PLUGIN_EXPORT` / `POLYPHASE_PLUGIN_EXPORT` annotation.


# Recovering from a hard crash (BSOD / power loss) during a build

If Windows crashes while Visual Studio or our packager was mid-write, a handful of files may be truncated or padded with null bytes. Symptoms are confusing because the filesystem still lists the file and Windows Explorer shows a plausible size — it's the contents that are garbage.

## "Root element is missing" when opening the solution
Standalone's build path rewrites `Standalone/Standalone.vcxproj` in place to inject native-addon sources. If the machine crashed during that write, the vcxproj is likely truncated and ends with null bytes, which the XML parser rejects.

**Fix:** restore from the `.orig` backup the injection leaves behind:

```
copy /Y Standalone\Standalone.vcxproj.orig Standalone\Standalone.vcxproj
```

You can verify with `tail -c 200 Standalone\Standalone.vcxproj | od -c` — a healthy file ends in `</Project>\n`, a corrupted one ends in a long run of `\0 \0 \0`.

## "Engine.lib is not a valid Win32 application" on F5
Two separate causes produce similar-sounding errors:

1. **Startup project got flipped to `Engine`.** Engine's output is a `.lib`, not a launchable `.exe`, so `CreateProcess` fails with `ERROR_BAD_EXE_FORMAT (193)`. Solution Explorer → right-click **Standalone** → **Set as Startup Project** (its name goes bold).
2. **`Engine.lib` was mid-link when the crash hit and its COFF archive header is garbage.** The linker for the next build reports `LNK1107: invalid or corrupt file`. Delete the stale artifacts and rebuild:
   ```
   del /Q Engine\Build\Windows\x64\DebugEditor\Engine.lib
   del /Q Engine\Build\Windows\x64\DebugEditor\Engine.pdb
   rmdir /S /Q Engine\Intermediate\Windows\x64\DebugEditor
   ```
   Replace `DebugEditor` with whichever config the crash was in (usually `Release` if it happened during shipped-build packaging). A full rebuild of that config is ~2 min.

## BSOD during packaging (link.exe + `MiQueryAddressState`)
Bugcheck `0x0000000A IRQL_NOT_LESS_OR_EQUAL` faulting in `nt!MiQueryAddressState` while `link.exe` is running is a **kernel-side issue**, not a project bug. It has been reported on Windows 11 24H2/25H2 with VBS / HVCI enabled under heavy LTCG links. Mitigations, in order of effectiveness:

1. **Turn off HVCI** (Settings → Privacy & Security → Windows Security → Device Security → Core Isolation → Memory Integrity = Off → reboot). Most direct fix; re-test.
2. **Disable `WholeProgramOptimization` (LTCG)** in `Release|x64` and `ReleaseSteam|x64` of `Standalone.vcxproj` and `Engine.vcxproj`. LTCG forces link.exe to hold every TU's IR in memory at once, which is what stresses `MiQueryAddressState`. You lose ~2-5% runtime perf on the engine's own C++ (invisible in a frame budget dominated by Vulkan/scripts).
3. **Defender (or other AV) exclusions** for the repo root and the MSVC intermediate dirs. Real-time scan of thousands of `.obj`/`.pdb` writes during a link aggravates the MM path.
4. Make sure Windows Update is current — Microsoft has been pushing MM/hypervisor fixes in this area monthly.

## "Force Rebuild" still produced a stale build
`Build → Windows` with **Force Rebuild** checked wipes the following before invoking the linker, in the active config (`Release` or `ReleaseSteam`):
- `Standalone/Intermediate/Windows/x64/{config}/Standalone/`
- `Standalone/Build/Windows/x64/{config}/Polyphase.{exe,ilk,pdb}`
- `Engine/Build/Windows/x64/{config}/Engine.{lib,pdb}`

The Standalone wipe is necessary because MSBuild's own `.tlog`-based up-to-date check otherwise decides "nothing to do" even after the addon injection has changed the project. The `Engine.lib` delete forces MSBuild to re-lib Engine (otherwise `devenv /Build` skips it and hands the linker a stale `Engine.lib`). The `Engine.pdb` delete sidesteps the most common failure mode: every `.cpp` in Engine failing with `error C1033: cannot open program database 'Engine.pdb'` because the PDB is locked by a leaked `mspdbsrv.exe` or an active debug session of `Polyphase.exe` in Visual Studio. We do **not** wipe Engine intermediates — the .obj files survive, so re-link is fast (seconds) and only changed `.cpp` files recompile.

If `Engine.pdb` itself can't be deleted (lock is real, not stale), the packager aborts with `ERROR: Could not delete ... Engine.pdb (file is locked)` instead of letting devenv spin up a doomed compile. Stop debugging in Visual Studio (or close `devenv.exe`) and retry.

If the packaged `.exe` still looks stale after a Force Rebuild run, check:
- The **editor** (`Polyphase.exe` in `Standalone/Build/Windows/x64/DebugEditor`) is newer than your latest `ActionManager.cpp` edit. Older editor → running old build logic.
- The packager log shows `[BUILD] needCompile=1`. If you see `needCompile=0 … Reusing pre-compiled game executable.`, Force Rebuild wasn't actually honored — this indicates an older editor build.


### VSCode / GDB Debugging Issues on Ubuntu 24+

Some Linux users may encounter extremely slow debugger startup times, hangs, or failed launches when using `cppdbg` in Visual Studio Code on newer Ubuntu releases (22.04+ / 24.04+), especially inside containers, XRDP sessions, or remote development environments.

This is commonly caused by GDB attempting to automatically download external debug symbols from Ubuntu's `debuginfod` servers.

Symptoms may include:

* Debugger hangs before launch
* `Failed to set controlling terminal: Operation not permitted`
* Very slow startup times
* `cppdbg` timing out or freezing
* GUI applications never appearing

To resolve this issue, disable automatic `debuginfod` symbol downloading by setting:

```json
"remoteEnv": {
    "DEBUGINFOD_URLS": ""
}
```

For non-container environments, you can also export the variable globally:

```bash
export DEBUGINFOD_URLS=""
```

or add it to your shell profile:

```bash
echo 'export DEBUGINFOD_URLS=""' >> ~/.bashrc
source ~/.bashrc
```

Additionally, some users may need to force VSCode automation tasks to use Bash:

```json
"terminal.integrated.automationShell.linux": "/bin/bash"
```

This issue is related to newer Ubuntu debugging environments and is not specific to Polyphase or FAW itself.
