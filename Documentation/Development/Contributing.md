# Contributing

This guide collects the things you need to know when modifying parts of the engine that other parts (or downstream installers) silently depend on. If you only see a section's symptom *after* shipping, the gates here are how you catch it before shipping next time.

---

## Adding a new artifact to the installed editor

If you produce a new file at engine build time that ships in `C:\Polyphase\` (Windows) or `/opt/polyphase/` (Linux) — for example a new import library, runtime DLL, ICU data file, prebuilt asset, or generated header — you need to touch **multiple files** to actually get it into the installer payload. Missing any one of them produces an installer that "looks fine" but is broken in a way that's only visible to end users.

### The pipeline (Windows)

```
[engine build]                                  produces  Standalone\Build\Windows\x64\ReleaseEditor\<artifact>
        |
        v
Installers/stage_distribution.py                copies it into  dist\Editor\<artifact>
        |
        v
Installers/Windows/PolyphaseSetup.iss           [Files] section bundles dist\Editor\<artifact> into PolyphaseSetup-*.exe
        |
        v
Installers/build_installer_windows.bat          drives the above + gates that each artifact is present
        |
        v
.github/workflows/release.yml                   the same gates in CI for tagged releases
```

**Inno Setup only packages files that are explicitly listed in `[Files]`.** A `Source: "..\..\dist\Editor\foo.ext"` line is required for every shipped file or directory. There is no implicit "ship everything in the staged dir" mode — staging the file is necessary but not sufficient.

### The pipeline (Linux)

```
[engine build]                                  produces  Standalone\Build\Linux\PolyphaseEditor.elf
        |
        v
Installers/stage_distribution.py                copies it into  dist/Editor/<artifact>
        |
        v
Installers/build_deb_linux.sh                   does `cp -r dist/Editor/* $DEB_DIR/opt/polyphase/`  (recursive — picks up new files automatically)
Installers/build_tarball_linux.sh               does `cp -r dist/Editor $TARBALL_DIR/polyphase`     (same)
```

Linux deb/tarball builders recursively copy whatever staging produced, so they don't have a per-file enumeration to forget. Linux's risk is upstream: forgetting to teach `stage_distribution.py` to copy the artifact in the first place.

### Checklist when adding a shipped artifact

When you introduce a new file that needs to be on a user's installed machine:

1. **Engine build produces it at a known path.** Confirm `msbuild Polyphase.sln /p:Configuration=ReleaseEditor /p:Platform=x64` (Windows) or `make -C Standalone -f Makefile_Linux_Editor` (Linux) writes the artifact reliably to a stable location.
2. **`Installers/stage_distribution.py` copies it to `dist/Editor/`.** If it's a single file outside the engine's main copied subtrees (`Engine/`, `External/`, `Standalone/`, `Tools/`, `Template/`, `Documentation/`), add an explicit `copy_file(src, dst, verbose)` block in `stage_distribution.py` near the existing `Polyphase.lib` / `Lua.lib` block (around line 277). If it lives inside an already-copied subtree, check that `EXCLUDED_EXTENSIONS` (line 32) and `EXCLUDED_DIRS` (line 26) don't filter it out.
3. **`Installers/Windows/PolyphaseSetup.iss` bundles it from `dist/Editor/`.** Add a `Source:` line in `[Files]` under the appropriate `Components: core | sdk | tools | docs` component. Use `core` for files the editor needs to run, `sdk` for files only addon authors need (headers, import libraries), `tools` for Python scripts, `docs` for Documentation.
4. **Add a presence gate to `Installers/build_installer_windows.bat`.** Right after the staging step, add an `if not exist "dist\Editor\<artifact>" (echo ERROR ... & exit /b 1)` check — staging failures otherwise produce a silently broken installer (`stage_distribution.py` only `print`s warnings, it does not exit non-zero on missing inputs).
5. **Add the same gate to `.github/workflows/release.yml`** in the `Verify staged import libraries` step (around line 90). CI is the only gate non-Windows contributors and tagged releases hit.
6. **Verify the .exe payload, not just the staged dir.** Both gates above only check that `dist/Editor/<artifact>` exists — neither verifies that ISCC actually packaged it. If you forget step 3, both gates pass and the installer still ships broken. After ISCC, run `7z l dist\PolyphaseSetup-*.exe | findstr /I "<artifact>"` locally to confirm; consider adding this as a CI step.
7. **Linux: usually automatic.** As long as the file is in `dist/Editor/` after staging, both `build_deb_linux.sh` and `build_tarball_linux.sh` pick it up via recursive copy. The exceptions are files that need special placement (e.g. `/usr/share/applications/...`) — those need an explicit `cp` in `build_deb_linux.sh`.

### Files to grep when this kind of bug bites

If a user reports "addon X fails to load on installed Polyphase but works in VS", the cause is almost always a missing artifact in `C:\Polyphase\`. Walk back up the pipeline:

| Symptom | Where to look |
|---|---|
| `LNK1181: cannot open input file '<x>.lib'` in `{Project}\Intermediate\Plugins\<addonId>\<fingerprint>\build.bat` | `PolyphaseSetup.iss` `[Files]` (most likely culprit). Check `dist\Editor\<x>.lib` exists; if yes, .iss is missing the `Source:` line. |
| `dist\Editor\<x>.lib` is missing | `stage_distribution.py` either skipped it or `EXCLUDED_EXTENSIONS` filtered it. |
| `Standalone\Build\Windows\x64\ReleaseEditor\<x>.lib` is missing | Engine build itself is producing nothing — check the `.vcxproj` configuration. |
| `Failed to construct node '<X>' (type=N, unknown type?)` | Addon DLL didn't load. The build step before it failed silently — open `{Project}\Intermediate\Plugins\<addonId>\<fingerprint>\build.bat` and run it manually from a `vcvars64`-initialized prompt to surface the linker error. |

### Why this is easy to miss

Each pipeline step has its own validation, but **none of them validates the next step's inputs**. The engine build doesn't know about staging, staging doesn't know about ISCC, ISCC doesn't know about the user's project. The .iss gap that motivated this guide (commit `2cfdb8c03 Bug: Build was not copying .libs` fixed staging but not ISCC) is the canonical example. Add a gate at every layer or accept that the next omission will only surface as an end-user bug report.

---

## Native addon ABI / fingerprint considerations

`Engine/Source/Editor/Addons/NativeAddonManager.cpp::ComputeFingerprint` (around line 1014) hashes the addon's source files plus the host CRT config tag (`dbg` vs `rel`) to produce the `{config}_{hex}` directory under `{Project}/Intermediate/Plugins/<addonId>/`. Two consequences worth knowing:

- The hash includes **absolute source-file paths**, so the dev editor and the installed editor never share a fingerprint folder — the installed editor will rebuild from scratch on first run, even if the dev editor already built a working DLL for the exact same source. This is intended — it lets a single project switch between editors safely — but it does mean addon developers should expect a one-time compile when they switch editors.
- The CRT tag (`/MD` vs `/MDd`) is part of the fingerprint, so a debug-built addon will never be loaded by a release editor and vice versa. If your editor was built with a non-standard CRT, addons must be rebuilt to match — see `project_addon_dll_crt_fingerprint.md` in the team memory for prior incidents.

---

## Where the build pipeline is documented in detail

- `Documentation/Development/PackagingFlow.md` — `ActionManager::BuildData`, the per-platform game packaging path. Different from the engine-installer flow this doc covers, but useful when an addon ships build hooks.
- `Documentation/Development/SetupEnvironment/Compiling.md` — local dev build prerequisites.
- `.github/workflows/release.yml` — the source of truth for what a tagged release runs end to end. If you change anything in `Installers/`, run through that workflow mentally before merging.
