# CI/CD

How the Polyphase Engine builds itself in GitHub Actions, where its CI artifacts come from, and how to keep addon repos in lockstep with the engine's pinned toolchain.

## Workflows at a glance

| File | Trigger | What it does |
|---|---|---|
| `.github/workflows/release.yml` | tag `v*`, manual dispatch | Builds the Windows installer + SDK zip, the Linux `.deb` + tarball, the `polyphaselabs/polyphase-engine` Docker image, runs `verify-test-project` against the headless editor for Linux/Wii/GameCube/3DS, then publishes a GitHub Release with the artifacts. |
| `.github/workflows/docs.yml` | push to `main` / `development/*` / `version/*`, manual dispatch | Builds the MkDocs site in a Docker container and deploys to GitHub Pages. |
| `.github/workflows/verify-build.yml` | push to `version/*` / `beta/*`, PRs into `main` or `version/*` | Pre-merge smoke check that the editor still compiles AND headless-builds the test project on every supported console. |

## Workflows in detail

### `release.yml` — Build & Release

Produces every artifact the project ships: the Windows installer, the Linux `.deb` and tarball, the SDK zip, the Docker image, and a GitHub Release with everything attached.

**Triggers**
- Pushing any tag matching `v*` (e.g. `v6.2.0`, `v6.2.0-beta.5`) → full publish.
- Workflow Dispatch (manual run via the Actions tab) → same jobs run, but the GitHub Release at the end keys off `$GITHUB_REF` so a manual dispatch from a branch doesn't actually publish — useful for dry-runs.

**How a dev triggers it**
- Cut a release tag:
  ```bash
  git tag v6.2.0-beta.6
  git push origin v6.2.0-beta.6
  ```
- Or kick a dry run: GitHub → **Actions → Build & Release → Run workflow** → pick a branch → **Run workflow**.

**Jobs** (run in order; later jobs gate on earlier)
1. **`publish-docker-images`** — first, intentionally. Builds and pushes `polyphaselabs/polyphase-engine:<version>` to Docker Hub. Skipped if `DOCKERHUB_USERNAME` / `DOCKERHUB_TOKEN` secrets aren't set (with a warning in the run summary). `:latest` tag is pushed only when the tagged commit is reachable from `origin/main` — beta tags pushed off main don't pollute `:latest`. Note: the companion `polyphaselabs/polyphase-bare` toolchain image is NOT built by CI — it's pushed manually from a workstation when `Docker/Dockerfile.base` changes.
2. **`build-windows`** — needs `publish-docker-images`. Installs the Vulkan SDK via the composite action, prebuilds libgit2, runs `msbuild` for `ReleaseEditor x64` (and best-effort `ReleaseEditor Shared` / `Release Shared` for the DLL flavor), then stages → packages → builds the Inno Setup installer. Uploads `windows-installer` + `windows-sdk` artifacts.
3. **`build-linux`** — needs `publish-docker-images`. Runs inside `polyphaselabs/polyphase-bare:latest`. Compiles shaders, prebuilds libgit2, builds the static editor (and best-effort the `libPolyphaseEditor.so` / `libPolyphaseGame.so` shared variants), builds `libLua.a`, then assembles the `.deb` and tarball. Uploads `polyphase-editor-tree` (CI scaffolding), `linux-deb`, `linux-tarball` artifacts.
4. **`verify-test-project`** — needs `build-linux`. Downloads `polyphase-editor-tree`, clones the `octo-bombers` test project, headless-builds it for each platform in the matrix `[Linux, Wii, GameCube, 3DS]`. `fail-fast: false` so all four show their state on every run.
5. **`create-release`** — needs all three of `build-windows`, `build-linux`, `verify-test-project`. Pulls `windows-installer` + `windows-sdk` + `linux-deb` + `linux-tarball` artifacts and creates the GitHub Release with `softprops/action-gh-release@v2`. Anything red upstream skips publish — no broken artifacts go out.

**Required secrets**
- `DOCKERHUB_USERNAME`, `DOCKERHUB_TOKEN` — for `publish-docker-images`. If missing, that job emits a warning and exits cleanly; build-windows / build-linux still run.

**Optional repository variables**
- `DOCKERHUB_ENGINE_IMAGE` — override the engine image namespace/name (default `polyphaselabs/polyphase-engine`).

**Common failure modes**
- Vulkan SDK download — covered by the mirror section below.
- LunarG SDK URLs themselves expire — same fix.
- Docker Hub auth expired — rotate `DOCKERHUB_TOKEN`.
- `verify-test-project` Wii/GCN/3DS regressions — usually traced to engine Makefile drift, sometimes to test-project Makefile drift (the `octo-bombers` repo).

### `verify-build.yml` — Verify Build

Pre-merge gate. Catches the same kinds of regressions `release.yml` would catch, but cheaper and without producing public artifacts.

**Triggers**
- Push to `version/*` or `beta/*` branches.
- Pull request targeting `main` or `version/*`.

**How a dev triggers it**
- Open a PR against `main` (or `version/*`). It runs automatically on each push to the PR branch.
- Push directly to a `version/*` or `beta/*` branch.
- *Not* manually dispatchable today — if you want a one-off run, push a throwaway commit to a `beta/*` branch.

**Jobs**
1. **`build-linux`** — same toolchain image and same compile path as `release.yml`'s build-linux, just without the `.deb` / tarball staging. Builds the static Linux editor, builds `libLua.a`, uploads the `polyphase-editor-tree` artifact.
2. **`verify-test-project`** — needs `build-linux`. Same matrix `[Linux, Wii, GameCube, 3DS]`, same headless-build-the-octo-bombers-test-project flow as release.yml.

**Why this exists despite release.yml covering the same ground:** PRs need a green check before merge. release.yml only runs on `v*` tags — by the time it fires, the regression is already on `main`. verify-build runs early so reviewers see breakage on the PR.

**Required secrets** — none.

**Common failure modes**
- Toolchain image lag — if `polyphaselabs/polyphase-bare:latest` is out of date relative to what the engine needs, the "Ensure python3 + cmake + libssl-dev are available" step apt-installs the gap. If a build needs something the backstop step doesn't cover, the image needs a manual rebuild + push.
- mbedTLS header vendor step — pulls v3.6.2 fresh on every run; transient GitHub clone failures retry on workflow re-run.

### `docs.yml` — Deploy Docs

Builds the MkDocs site from the `Documentation/` tree and publishes to GitHub Pages.

**Triggers**
- Push to `main`, `development/jj`, `development/chore/rebranding/*`, or `version/*` branches.
- Workflow Dispatch (manual).

**How a dev triggers it**
- Merge to `main` (most common) — site rebuilds automatically.
- Push to `version/*` — same.
- Or kick a manual rebuild: **Actions → Deploy Docs → Run workflow → main → Run**.

**Jobs**
1. **`build`** — builds the docs Docker image from `Tools/CI/docs/Dockerfile`, runs it to render `site/` from `Documentation/`, uploads the result as a Pages artifact.
2. **`deploy`** — needs `build`. Deploys via `actions/deploy-pages@v4` to the `github-pages` environment.

**Concurrency**: `group: pages, cancel-in-progress: true` — only the most recent run for a given branch ever deploys. Rapid commits don't queue stale deploys.

**Local preview before pushing**:
```bash
# From the repo root, using the same Docker image as CI:
docker build -t polyphase-docs -f Tools/CI/docs/Dockerfile Tools/CI/docs
docker run --rm -it -p 8580:8000 -v "$PWD":/repo polyphase-docs serve
# → open http://localhost:8580
```
(VSCode users have `Preview Docs - Linux` / `Preview Docs - WSL` tasks pre-wired in `.vscode/tasks.json`.)

**Required permissions** — declared in the workflow:
- `contents: read` (checkout)
- `pages: write` (deploy)
- `id-token: write` (Pages authentication)

The `environment: github-pages` block on the `deploy` job is what wires it to the Pages URL — that environment is auto-created when you enable Pages in repo Settings.

### Addon repo: `native-addon-release.yml`

Lives in each native addon repo (not the engine repo). Build template scaffolded by `AddonCreator` in the editor.

**Triggers**
- Push tag matching `v*` on the addon repo.
- Workflow Dispatch.

**How a dev triggers it**
- Tag the addon repo: `git tag v1.0.0 && git push origin v1.0.0`.

**Jobs**
1. **`build-linux`** — runs inside `polyphaselabs/polyphase-engine:latest` (engine source + prebuilt engine static libs + Vulkan SDK already baked in). Reads addon binary name from `package.json` (`native.binaryName` or `name`), runs `.github/workflows/build.sh <name> Both`, uploads Release + Debug `.so` artifacts.
2. **`build-windows`** — downloads `polyphase-sdk-windows-x64.zip` from the engine repo's **latest** GitHub release (contains engine headers + `Polyphase.lib` + `Lua.lib`), installs Vulkan SDK via **the engine's composite action** (`uses: Polyphase-Labs/Polyphase-Engine/.github/actions/install-vulkan-sdk@main`), runs `build.bat <name> Both`, uploads Release `.dll` artifact (Debug is best-effort — see below).
3. **`release`** — needs both build jobs. Downloads all artifacts, updates `package.json` with `binaries[]` descriptors so the editor's `NativeAddonManager` knows which release asset to fetch per platform/config, publishes a GitHub Release with the binaries + the updated `package.json` attached.

**Required secrets** — none beyond the default `GITHUB_TOKEN`.

**Why the SDK zip, not a source clone**
Engine headers decorate exported symbols with `__declspec(dllimport)` when consumed from outside the engine TU. An addon DLL that touches `LogWarning`, `Stream::GetSize`, `ImGui::Button`, `lua_pushstring`, etc. emits `__imp_*` references that need `Polyphase.lib` + `Lua.lib` to resolve at link time. Cloning the engine source gives you headers but not the import libs — the linker then fails with ~220 `LNK2019` / `LNK2001` errors against every engine symbol the addon touches. The SDK zip exists specifically to ship those import libs alongside the headers; `release.yml` produces it from `Installers/package_windows_sdk.py` as part of every engine release.

**Why Windows Debug is best-effort**
The SDK zip currently ships `Lib/Windows/x64/ReleaseEditor/` only (Release-CRT engine libs). A Debug-CRT (`/MDd`) addon linked against Release-CRT (`/MD`) engine libs ABI-mismatches across the engine ↔ addon boundary (different `std::string` allocators, different debug-only struct fields). To avoid that, `build.bat` probes for `Lib/Windows/x64/DebugEditor/Polyphase.lib` and **skips the Debug addon link cleanly** when it's absent. `NativeAddonManager` source-compiles Debug addons at runtime when no Debug binary is shipped (the `#if defined(_DEBUG)` fallback in `Engine/Source/Editor/Addons/NativeAddonManager.cpp`), so end-users with Debug editors aren't blocked. If a fork wants shipped Debug binaries, build `DebugEditor|x64` in `release.yml` and extend `package_windows_sdk.py` to copy the resulting libs into the SDK zip under `Lib/Windows/x64/DebugEditor/`.

**Common failure modes**
- Vulkan SDK install — same mirror dependency as the engine workflow.
- SDK zip download 404 — the engine hasn't cut a release yet that includes `polyphase-sdk-windows-x64.zip` as an asset. Either cut an engine release (push a `v*` tag on `main`) or temporarily point `POLYPHASE_REPO` at a fork that has one.
- Engine import-lib path drift — when the engine's `Polyphase.lib` location inside the SDK zip moves (e.g. `ReleaseEditor` is renamed), `build.bat`'s probe paths need updating. Track engine SDK release notes for these.

## The Vulkan SDK mirror

The engine pins **Vulkan SDK 1.3.275.0** in `Documentation/Development/SetupEnvironment/Windows.md` and `Linux.md`. CI uses the same version so it builds against the same toolchain every contributor uses locally — important because the engine treats certain shaderc warnings as errors and the spirv-cross/shaderc `.lib` ABI is version-sensitive.

LunarG periodically removes older SDK installers from their CDN. 1.3.275.0 went offline around 2026-05. Rather than chase the upstream URL — or worse, drift to a newer SDK and risk shader-compile breakage — the engine repo **self-hosts the installed SDK directory** as a GitHub Release asset and CI extracts it on every run.

### How it's wired

`.github/actions/install-vulkan-sdk/action.yml` is a small composite action that downloads `VulkanSDK-<version>-installed.zip` from the engine repo's Release tagged `vulkan-sdk-<version>` and extracts it to `C:\VulkanSDK\<version>\`. It exports `VULKAN_SDK` so downstream MSBuild/CMake steps pick it up.

The engine's `release.yml` calls it locally:

```yaml
- name: Install Vulkan SDK
  uses: ./.github/actions/install-vulkan-sdk
```

Addon repos call it from the engine repo via a cross-repo `uses:`:

```yaml
- name: Install Vulkan SDK
  uses: Polyphase-Labs/Polyphase-Engine/.github/actions/install-vulkan-sdk@main
```

The action has three optional inputs:

| Input | Default | When to override |
|---|---|---|
| `version` | `1.3.275.0` | Bumping SDK versions (do this together with the docs and the matching `vulkan-sdk-<ver>` Release). |
| `polyphase-repo` | `Polyphase-Labs/Polyphase-Engine` | Forks publishing the engine under a different namespace. |
| `mirror-url` | `""` (computed from `version` + `polyphase-repo`) | Pulling from a private mirror or an alternate host. |

Output `vulkan-sdk-dir` is the absolute install path, in case a later step needs more than the `VULKAN_SDK` env var.

Linux CI does **not** use this action — it runs inside `polyphaselabs/polyphase-bare` / `polyphase-engine` Docker images which already have the SDK baked in. The action is Windows-only.

### Publishing the mirror

One-time per SDK version, from a Windows workstation with the SDK installed locally:

```powershell
Compress-Archive `
  -Path 'C:\VulkanSDK\1.3.275.0\*' `
  -DestinationPath VulkanSDK-1.3.275.0-installed.zip `
  -CompressionLevel Optimal
```

Then on GitHub:
1. Polyphase-Labs/Polyphase-Engine → **Releases** → **Draft a new release**.
2. Tag: `vulkan-sdk-1.3.275.0`. **Do NOT prefix with `v`** — `v*` tags trigger the Build & Release workflow.
3. Drag `VulkanSDK-1.3.275.0-installed.zip` in as an asset.
4. Publish.

The asset URL the action expects is `https://github.com/<polyphase-repo>/releases/download/vulkan-sdk-<version>/VulkanSDK-<version>-installed.zip` — using the GitHub Release tag/asset naming convention, no manual URL config required.

The Vulkan SDK is freely redistributable (its components are permissively licensed — MIT/Apache/Khronos); the `Licenses/` subdirectory inside the zip has the full breakdown.

### Bumping SDK versions

A version bump is a coordinated change across **four** places. Do all of them together or CI and local-dev environments will drift apart:

1. `Documentation/Development/SetupEnvironment/Windows.md` — the version line in the dependency list.
2. `Documentation/Development/SetupEnvironment/Linux.md` — three occurrences of the version string (download instructions and the `~/VulkanSDK/<ver>` env-var example).
3. `.github/actions/install-vulkan-sdk/action.yml` — the `version: default` value.
4. Publish a new `vulkan-sdk-<new-ver>` Release with the matching `VulkanSDK-<new-ver>-installed.zip` asset (procedure above).

Once the engine's `main` carries the new default, every addon workflow that `uses: …@main` picks it up on its next run with no per-addon edit. Addons pinned to a tag (e.g. `@v6.2.0`) keep using the older version until the addon's pin is bumped — useful for change isolation if an addon hasn't been re-tested against the new SDK yet.

### Why `installed.zip` instead of the installer

The Vulkan SDK installer self-cleans after running, so once a dev box is set up there's no `.exe` left to rearchive. The installed *tree* (`C:\VulkanSDK\<ver>\`) is what CI actually needs at the end of the install step anyway — headers, libs, `glslc.exe`. Skipping the installer means:

- No license-prompt flag plumbing.
- No "Shader Toolchain Debug Symbols - 64 bit" component-selection drift between local and CI installs (CI just inherits whatever the upload was built from).
- Zip extract is dramatically faster than the installer's component-registration step.
- The zip is portable — drop it into any Windows runner image, no admin rights required.

The trade-off is the asset is larger than the original installer (~300-500 MB vs ~150 MB) because it's the fully-installed tree. GitHub allows 2 GB per release asset.

## CI artifact map

What the release workflow produces and where it ends up:

| Artifact | Built by | Shipped as |
|---|---|---|
| `PolyphaseSetup-<version>.exe` | `build-windows` → `iscc Installers/Windows/PolyphaseSetup.iss` | GitHub Release asset. End-user Windows installer. |
| `polyphase-sdk-windows-x64.zip` | `build-windows` → `package_windows_sdk.py` | GitHub Release asset. Headers + import libs (`Polyphase.lib`, `Lua.lib`, plus the optional W1 `PolyphaseEditor.lib` / `PolyphaseGame.lib`) for native-addon CI. |
| `polyphase-engine_<version>_amd64.deb` | `build-linux` → `build_deb_linux.sh` | GitHub Release asset. Debian/Ubuntu installer. |
| `PolyphaseEditor-linux-x64.tar.gz` | `build-linux` → `build_tarball_linux.sh` | GitHub Release asset. Generic Linux tarball with `install.sh`. |
| `polyphaselabs/polyphase-engine:<ver>` | `publish-docker-images` → `Docker/Dockerfile` | Docker Hub image. Engine + prebuilt headless editor + `build-{editor,linux,3ds,gcn,wii}` helper scripts. |
| `polyphase-editor-tree` (artifact, not released) | `build-linux` (uploads), `verify-test-project` (consumes) | CI-only. Whole engine tree + the static `PolyphaseEditor.elf`, used by the per-platform packaging matrix to gate the release. |

## Build flow gating

`publish-docker-images` runs **first**. Both `build-windows` and `build-linux` depend on it, so a Dockerfile regression aborts the release before the Windows installer (~20 min) and Linux editor (~10 min) start. The Docker build rebuilds the Linux editor against the same toolchain `verify-test-project` uses, so if the editor regresses we catch it twice — once in the Docker build, once in the per-platform packaging matrix.

`verify-test-project` headless-builds the `octo-bombers` test project for **Linux, Wii, GameCube, and 3DS** using the static editor uploaded by `build-linux`. If any of those four packaging paths break, the create-release job won't run.

`create-release` is the final gate — it depends on `build-windows`, `build-linux`, AND `verify-test-project`. Anything red upstream skips publish and no broken artifacts go out.

## Addon CI

Native addons follow the same SDK and toolchain pinning via `native-addon-release.yml` (template lives in the engine repo's `Tools/CI/` and is scaffolded into new addons by `AddonCreator`).

The addon workflow:
1. Checks out the addon repo.
2. Reads the binary name from `package.json` (`native.binaryName` or `name`).
3. Clones the engine repo shallowly for headers.
4. **Installs the Vulkan SDK via the engine's composite action** (the `uses:` line above).
5. Runs `build.bat`/`build.sh` to compile the addon DLL/SO against the engine's import lib.
6. Uploads four artifacts: Windows Release/Debug `.dll`, Linux Release/Debug `.so`.
7. Writes a `binaries` array into `package.json` so the editor's `NativeAddonManager` knows which release asset to download for each platform/config at install time.

Addons with their own `External/` dependencies (FFmpeg, etc. — see `com.polyphase.formats.video`) handle those via `package.json`'s `nativePerPlatform.<platform>.{extraIncludeDirs,extraLibs,copyBinaries}`. The build scripts honor those entries, mirroring what the editor's `NativeAddonManager` does for hot-reload. No CI changes needed per-addon for that case.

## Retrofitting existing addons

When porting an older addon repo to use the composite action, replace the inlined PowerShell install step with the one-liner:

```yaml
- name: Install Vulkan SDK
  uses: Polyphase-Labs/Polyphase-Engine/.github/actions/install-vulkan-sdk@main
```

Pin to a tag (`@v6.2.0` etc.) instead of `@main` if you want change isolation — the addon won't follow engine bumps until you re-pin. Most addons can stay on `@main` since the action only ever wraps a known-good SDK download.
