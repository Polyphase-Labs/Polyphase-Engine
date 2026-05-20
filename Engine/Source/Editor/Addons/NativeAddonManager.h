#pragma once

/**
 * @file NativeAddonManager.h
 * @brief Manages native addon lifecycle including discovery, building, loading, and unloading.
 */

#if EDITOR

#include "ProjectSelect/TemplateData.h"
#include "Plugins/PolyphaseEngineAPI.h"
#include "Plugins/PolyphasePluginAPI.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

/**
 * @brief Parameters for creating a new native addon.
 */
struct NativeAddonCreateInfo
{
    std::string mName;               // Display name (e.g., "My Addon")
    std::string mId;                 // Internal ID (e.g., "my-addon", auto-generated from name if empty)
    std::string mAuthor;
    std::string mDescription;
    std::string mVersion = "1.0.0";
    NativeAddonTarget mTarget = NativeAddonTarget::EngineAndEditor;
    std::string mBinaryName;         // Auto-generated from ID if empty
};

/**
 * @brief Options for packaging a native addon.
 */
struct NativeAddonPackageOptions
{
    std::string mAddonId;
    bool mIncludeSource = true;
    bool mIncludeAssets = true;
    bool mIncludeScripts = true;
    bool mIncludeThumbnail = true;
    std::string mOutputPath;         // Full path to output zip file
};

/**
 * @brief Runtime state for a native addon.
 */
struct NativeAddonState
{
    std::string mAddonId;
    std::string mSourcePath;      // Path to addon source (local Packages/ or cache)
    std::string mLoadedPath;      // Path to loaded DLL/SO
    void* mModuleHandle = nullptr;
    std::string mFingerprint;     // Hash for rebuild detection

    // Build state
    bool mBuildInProgress = false;
    bool mBuildSucceeded = false;
    std::string mBuildLog;
    std::string mBuildError;

    // Plugin descriptor (after load)
    PolyphasePluginDesc mDesc = {};
    bool mDescValid = false;

    // Native metadata from package.json
    NativeModuleMetadata mNativeMetadata;

    // Shared metadata from package.json (name/version/dependencies/onInstall/etc.)
    ContentMetadata mContentMetadata;

    // Runtime resolve/load status
    NativeAddonResolveMode mActiveResolveMode = NativeAddonResolveMode::Source;
    bool mLoadedFromBinary = false;
    std::string mBinaryStatus;

    // UUIDs of assets that PurgeAssetsFromModule unloaded during the most
    // recent UnloadNativeAddon. LoadNativeAddon drains this on its next
    // successful load, calling LoadAsset on each so an addon-typed asset
    // that was loaded before reload comes back loaded after — without this,
    // the post-reload stub has mAsset=null and SaveAsset becomes a no-op.
    std::vector<uint64_t> mPurgedAssetUuids;

    // True once the user has dismissed the build-failure modal entry for the
    // most recent failure. Reset to false whenever a fresh build attempt starts
    // so a re-failure surfaces again.
    bool mBuildFailureAcknowledged = false;
};

/**
 * @brief Singleton manager for native addon lifecycle.
 *
 * Handles:
 * - Discovery from local Packages/ and installed addons
 * - Fingerprint computation for rebuild detection
 * - Building native addons
 * - Loading, unloading, and hot-reloading
 */
class NativeAddonManager
{
public:
    static void Create();
    static void Destroy();
    static NativeAddonManager* Get();

    // ===== Discovery =====

    /**
     * @brief Discover all native addons from Packages/ and installed addons.
     */
    void DiscoverNativeAddons();

    /**
     * @brief Get list of discovered addon IDs.
     */
    std::vector<std::string> GetDiscoveredAddonIds() const;

    // ===== Build Operations =====

    /**
     * @brief Build a native addon.
     *
     * @param addonId Addon to build
     * @param outError Error message on failure
     * @return true if build succeeded
     */
    bool BuildNativeAddon(const std::string& addonId, std::string& outError);

    /**
     * @brief Compute fingerprint hash of source files.
     *
     * @param addonId Addon to compute fingerprint for
     * @return Fingerprint string, or empty on error
     */
    std::string ComputeFingerprint(const std::string& addonId);

    /**
     * @brief Check if addon needs rebuild.
     *
     * @param addonId Addon to check
     * @return true if source has changed since last build
     */
    bool NeedsBuild(const std::string& addonId);

private:
    /** Write a small key=value meta sidecar next to a freshly-built addon DLL. */
    void WriteAddonBuildMeta(const std::string& outputPath,
                             const std::string& fingerprint);

    /** Resolve effective per-addon native mode (installed settings override manifest default). */
    NativeAddonResolveMode ResolveModeForAddon(const std::string& addonId) const;

    /** Resolve binary path in binary mode: synced prebuilt first, then local intermediate. */
    bool ResolveBinaryModulePath(const std::string& addonId, std::string& outModulePath, std::string& outStatus, std::string& outError);

    /** Validate binary descriptor against current host and addon API. */
    bool IsBinaryDescriptorCompatible(const NativeBinaryDescriptor& descriptor, const NativeAddonState& state) const;

    /** Read the meta sidecar and return true if the cached DLL is stale
     *  (config tag differs, or recorded engine binary mtime differs from
     *  the current Polyphase.exe). Missing/malformed meta also returns true. */
    bool MetaIndicatesRebuildNeeded(const std::string& outputPath) const;
public:

    // ===== Load/Unload Operations =====

    /**
     * @brief Load a native addon.
     *
     * @param addonId Addon to load
     * @param outError Error message on failure
     * @return true if load succeeded
     */
    bool LoadNativeAddon(const std::string& addonId, std::string& outError);

    /**
     * @brief Unload a native addon.
     *
     * @param addonId Addon to unload
     * @return true if unload succeeded
     */
    bool UnloadNativeAddon(const std::string& addonId);

    /**
     * @brief Reload a native addon (unload, build if needed, load).
     *
     * @param addonId Addon to reload
     * @param outError Error message on failure
     * @return true if reload succeeded
     */
    bool ReloadNativeAddon(const std::string& addonId, std::string& outError);

    /**
     * @brief Reload all native addons.
     *
     * Discovers, builds changed addons, and reloads all.
     */
    void ReloadAllNativeAddons();

    // Force Rebuild was removed in favour of the per-addon Reload button +
    // the per-project chokepoint. To force a fresh compile of all addons,
    // call ReloadNativeAddonsWithProjectRestart({}, /*forceRebuild*/true, ...).

    // ===== Async build state (drives the progress modal) =====

    /** Per-frame: poll the active background build, finalize on the main
     *  thread when it completes, and start the next queued addon. Should be
     *  called once per editor frame. */
    void TickAsyncBuilds();

    /** True while an async build is in flight or queued. */
    bool IsBuildingAsync() const;

    /** Total number of addons enqueued for the current build session. */
    int  GetAsyncBuildTotal() const;

    /** 1-based index of the addon currently being built. */
    int  GetAsyncBuildIndex() const;

    /** Addon id currently being built (empty if idle). */
    std::string GetAsyncBuildAddonId() const;

    /** Snapshot of the current build's stdout (returned by value because
     *  the worker thread mutates the underlying buffer under a mutex). */
    std::string GetAsyncBuildOutput() const;

    // ===== Build-blocked state (locked intermediate files) =====
    //
    // Before a build runs, BuildNativeAddon / StartNextQueuedBuild sweep the
    // addon's intermediate fingerprint dir and try to delete every file. If
    // any file is locked (most commonly the .pdb held open by mspdbsrv.exe
    // across DLL unload, producing LNK1201 at link time), the sweep records
    // the offending paths and the build is paused. The editor surfaces a
    // modal listing the locked files with Retry / Cancel — Retry re-sweeps
    // and resumes if clean, Cancel abandons the operation.
    struct BuildBlocked
    {
        bool                     mActive = false;
        std::string              mAddonId;
        std::vector<std::string> mLockedFiles;
        // Absolute path to <project>/Intermediate/Plugins/<addonId>/ — the
        // simplest manual fix is to delete this entire directory. The modal
        // surfaces this as a copy-paste shell command.
        std::string              mIntermediateDir;
    };
    bool                IsBuildBlocked() const   { return mBlocked.mActive; }
    const BuildBlocked& GetBuildBlocked() const  { return mBlocked; }
    /** User clicked Retry: re-trigger the reload that was blocked. The reload
     *  pipeline is idempotent — already-loaded, up-to-date addons are skipped,
     *  so this re-attempts the addon that previously failed. */
    void RetryBlockedBuild();
    /** User clicked Cancel: clear blocked state without retrying. */
    void CancelBlockedBuild();

    // ===== Build-failure surface =====
    //
    // Aggregates per-addon compile/link failures across both the sync
    // BuildNativeAddon path and the async TickAsyncBuilds path. Drives the
    // build-failure modal so users don't have to scan the log to know which
    // addon broke. An entry stays "active" while:
    //   state.mBuildSucceeded == false &&
    //   !state.mBuildError.empty() &&
    //   !state.mBuildInProgress &&
    //   !state.mBuildFailureAcknowledged
    // and is implicitly cleared the next time the addon starts a build.
    struct BuildFailureEntry
    {
        std::string mAddonId;
        std::string mError;    // High-level message (exit code, "Build failed" etc.)
        std::string mLog;      // Captured stdout/stderr from the compiler/linker
    };
    std::vector<BuildFailureEntry> GetActiveBuildFailures() const;
    bool                           HasUnacknowledgedBuildFailures() const;
    void                           DismissBuildFailure(const std::string& addonId);
    void                           DismissAllBuildFailures();
    /** Reset the addon's failure state and re-trigger its build (sync or queued
     *  depending on the host editor's build pipeline). */
    void                           RetryFailedBuild(const std::string& addonId);

    // ===== Project-restart reload chokepoint =====
    //
    // Native addon reload is unsafe when scenes are open — live nodes hold
    // vtable pointers into the addon's mapped DLL pages and any unload
    // invalidates them, plus Node factories get stripped from the global
    // registry so a later scene reopen falls back to Node3D and silently
    // corrupts the in-memory tree on save. The fix is to close the project
    // entirely (saving dirty scenes first per user choice), unload every
    // addon, rebuild, then reopen the project from disk so factories,
    // assets, and scenes all rehydrate cleanly.
    //
    // The flow is staged across frames because the rebuild runs async on a
    // worker thread. Phase advances:
    //   None
    //     -> AwaitingConfirm  (one-shot confirm modal)
    //     -> AwaitingDirty    (per-scene Save/Discard/Cancel — one popup at
    //                          a time, advancing through mDirtyScenes)
    //     -> Building         (project closed, builds in flight; advanced by
    //                          TickAsyncBuilds when the queue drains)
    //     -> Reopening        (synchronous OpenProject + scene restore; only
    //                          held briefly for telemetry, then cleared)
    //     -> None
    enum class ProjectRestartPhase
    {
        None,
        AwaitingConfirm,
        AwaitingDirty,
        Building,
        Reopening,
    };

    struct ProjectRestart
    {
        ProjectRestartPhase mPhase = ProjectRestartPhase::None;

        // Addons being rebuilt. Empty = all installed enabled native addons.
        std::vector<std::string> mTargetAddons;
        // forceRebuild=true wipes each target's fingerprint dir before
        // building so NeedsBuild() returns true even for an unchanged source.
        bool                     mForceRebuild = false;
        std::string              mReason;       // user-facing modal copy

        // Snapshot — captured at restart entry, restored after OpenProject.
        std::string              mProjectPath;
        std::vector<std::string> mOpenSceneNames;   // names of edit scenes to reopen
        std::string              mActiveSceneName;  // active edit scene at snapshot

        // Per-scene dirty queue. Walked one-at-a-time during AwaitingDirty.
        std::vector<std::string> mDirtyScenes;
        int32_t                  mDirtyCursor = 0;
    };

    bool                  IsProjectRestartActive() const { return mRestart.mPhase != ProjectRestartPhase::None; }
    const ProjectRestart& GetProjectRestart() const      { return mRestart; }

    /** Public entry. Snapshots editor state, then hands off to the modal
     *  state machine. addonIds may be empty (= all installed enabled native
     *  addons). reason is shown in the confirm modal — keep it short
     *  ("user clicked Reload", "Edit > Reload Native Addons"). */
    void ReloadNativeAddonsWithProjectRestart(const std::vector<std::string>& addonIds,
                                              bool forceRebuild,
                                              const char* reason);

    // Modal callbacks. Called from the EditorImgui modal renderers when the
    // user clicks the corresponding button. Public because the modal lives
    // outside this class.
    void ProjectRestartConfirm();        // [Continue] on confirm modal
    void ProjectRestartCancel();         // [Cancel] on confirm modal — abort whole flow
    void ProjectRestartDirtySave();      // [Save] for the current dirty scene
    void ProjectRestartDirtyDiscard();   // [Discard] for the current dirty scene
    void ProjectRestartDirtyCancel();    // [Cancel] in dirty prompt — abort whole flow

    /**
     * @brief Tick all loaded plugins (gameplay tick).
     *
     * Calls the Tick callback for each loaded plugin that has one.
     * Should be called only when playing (PIE or built game).
     *
     * @param deltaTime Time elapsed since last frame in seconds
     */
    void TickAllPlugins(float deltaTime);

    /**
     * @brief Tick all loaded plugins (editor tick).
     *
     * Calls the TickEditor callback for each loaded plugin that has one.
     * Should be called every frame in the editor regardless of play state.
     *
     * @param deltaTime Time elapsed since last frame in seconds
     */
    void TickEditorAllPlugins(float deltaTime);

    /**
     * @brief Call OnEditorPreInit on all loaded plugins that implement it.
     */
    void CallOnEditorPreInit();

    /**
     * @brief Call OnEditorReady on all loaded plugins that implement it.
     */
    void CallOnEditorReady();

    // ===== State Queries =====

    /**
     * @brief Get state for an addon.
     *
     * @param addonId Addon to query
     * @return Pointer to state, or nullptr if not found
     */
    const NativeAddonState* GetState(const std::string& addonId) const;

    /**
     * @brief Check if addon is currently loaded.
     */
    bool IsLoaded(const std::string& addonId) const;

    /**
     * @brief Get source path for an addon.
     *
     * @param addonId Addon to query
     * @return Source path (local Packages/ or cache), or empty if not found
     */
    std::string GetAddonSourcePath(const std::string& addonId) const;

    /**
     * @brief Find the addon source path that owns a given build-target id.
     *
     * Iterates all discovered native addons and matches against each one's
     * parsed `native.buildTargets[].id` metadata. Used by ActionManager to
     * resolve a target's `platformExtensionDir` to an absolute path when
     * generating Variant 2 bridge headers before compile.
     *
     * @param buildTargetId Reverse-DNS build target id (e.g. "homebrew.psp")
     * @return Absolute source path of the addon that declares this target,
     *         or empty if no installed addon advertises this id.
     */
    std::string FindAddonRootForBuildTarget(const std::string& buildTargetId) const;

    /**
     * @brief Get all addons with engine target (for final build injection).
     */
    std::vector<NativeAddonState> GetEngineAddons() const;

    /**
     * @brief Get the engine API struct for plugins.
     */
    PolyphaseEngineAPI* GetEngineAPI() { return &mEngineAPI; }

    // ===== Creation and Packaging =====

    /**
     * @brief Create a new native addon with folder structure and template files.
     *
     * Creates:
     * - {ProjectDir}/Packages/{addonId}/
     *     - package.json
     *     - Source/{AddonName}.cpp (template)
     *     - .vscode/c_cpp_properties.json
     *
     * @param info Creation parameters
     * @param outError Error message on failure
     * @param outPath Output path to the created addon folder (optional)
     * @return true if creation succeeded
     */
    bool CreateNativeAddon(const NativeAddonCreateInfo& info, std::string& outError, std::string* outPath = nullptr);

    /**
     * @brief Create a native addon at a custom target directory.
     *
     * Same as CreateNativeAddon() but creates the addon in the specified
     * directory instead of the project's Packages/ folder.
     *
     * @param info Creation parameters
     * @param targetDir Directory where the addon folder will be created
     * @param outError Error message on failure
     * @param outPath Output path to the created addon folder (optional)
     * @return true if creation succeeded
     */
    bool CreateNativeAddonAtPath(const NativeAddonCreateInfo& info, const std::string& targetDir,
                                  std::string& outError, std::string* outPath = nullptr);

    /**
     * @brief Package a native addon for distribution.
     *
     * Creates a zip file containing the addon contents.
     *
     * @param options Packaging options
     * @param outError Error message on failure
     * @return true if packaging succeeded
     */
    bool PackageNativeAddon(const NativeAddonPackageOptions& options, std::string& outError);

    /**
     * @brief Generate IDE configuration files for an addon.
     *
     * Creates .vscode/c_cpp_properties.json and optionally CMakeLists.txt
     *
     * @param addonPath Path to addon root folder
     * @return true if generation succeeded
     */
    bool GenerateIDEConfig(const std::string& addonPath);

    /**
     * @brief Get list of local package addon IDs (in Packages/ folder).
     */
    std::vector<std::string> GetLocalPackageIds() const;

    /**
     * @brief Generate the AddonIncludes.json manifest file.
     *
     * Creates Engine/Generated/AddonIncludes.json with all include paths
     * that native addons need for compilation and IDE support.
     *
     * @return true if generation succeeded
     */
    static bool GenerateAddonIncludesManifest();

    /**
     * @brief Load the AddonIncludes.json manifest.
     *
     * @param outIncludePaths Output: list of relative include paths
     * @param outDefines Output: list of preprocessor defines
     * @return true if manifest was loaded successfully
     */
    static bool LoadAddonIncludesManifest(std::vector<std::string>& outIncludePaths,
                                           std::vector<std::string>& outDefines);

private:
    static NativeAddonManager* sInstance;
    NativeAddonManager();
    ~NativeAddonManager();

    // Discovery helpers
    void ScanLocalPackages();
    void ScanInstalledAddons();
    bool ParsePackageJson(const std::string& path, NativeModuleMetadata& outMetadata, ContentMetadata* outContent = nullptr);

    /// Last-computed topological order for native addons (filtered from the
    /// project-wide resolver order). Falls back to mStates iteration order when
    /// the resolver hasn't run yet. Used to drive build-all and load-all.
    std::vector<std::string> GetLoadOrder() const;

    // Cached topo order produced by the most recent ResolveAll() during discovery.
    std::vector<std::string> mCachedLoadOrder;

    // Build helpers
    std::string GetIntermediateDir(const std::string& addonId);
    std::string GetOutputPath(const std::string& addonId, const std::string& fingerprint);
    bool GenerateBuildScript(const std::string& addonId, const std::string& outputDir,
                             const std::string& outputPath, std::string& outScriptPath);
    std::vector<std::string> GatherSourceFiles(const std::string& sourceDir);

    /** Walk the addon's intermediate fingerprint directory and try to delete
     *  every file. Returns the list of paths that could not be deleted (held
     *  open by another process). On success the directory is left empty (or
     *  removed entirely if the OS allows). The caller decides what to do
     *  with a non-empty result — typically: stop the build and surface a
     *  modal so the user can release the lock holder. */
    std::vector<std::string> TryClearAddonIntermediates(const std::string& addonId);

    // Engine API setup
    void InitializeEngineAPI();

    // Creation helpers
    std::string GenerateIdFromName(const std::string& name);
    bool WriteTemplateSourceFile(const std::string& path, const std::string& addonName,
                                  const std::string& binaryName);
    bool WritePackageJson(const std::string& path, const NativeAddonCreateInfo& info);
    bool WriteVSCodeConfig(const std::string& addonPath);
    bool WriteCMakeLists(const std::string& addonPath, const std::string& binaryName);
    bool WriteVSProject(const std::string& addonPath, const std::string& addonName,
                        const std::string& binaryName);

    std::unordered_map<std::string, NativeAddonState> mStates;
    PolyphaseEngineAPI mEngineAPI;

    // ----- Async build queue -----
    //
    // One worker thread shells out to build.bat / build.sh per addon. The
    // main thread polls completion in TickAsyncBuilds(), runs the post-
    // build steps (write meta, MOD_Load, register types), and starts the
    // next queued item. This keeps the editor interactive while addons
    // compile, especially during multi-addon Force Rebuild.
    struct AsyncAddonBuild
    {
        std::string addonId;
        std::string scriptPath;
        std::string outputPath;
        std::string fingerprint;

        std::thread thread;
        std::atomic<bool> complete{false};
        std::atomic<int>  exitCode{0};

        mutable std::mutex outputMutex;
        std::string output;  // guarded by outputMutex
    };

    std::unique_ptr<AsyncAddonBuild> mActiveBuild;
    std::vector<std::string>         mBuildQueue;
    int                              mBuildQueueTotal = 0;
    int                              mBuildQueueIndex = 0;  // 1-based, advanced when a build starts

    // Set when a pre-build sweep finds locked files in the intermediate dir.
    BuildBlocked                     mBlocked;

    // Project-restart state machine. See ProjectRestartPhase for the flow.
    ProjectRestart                   mRestart;

    // One-shot per-addon override: addon IDs in this set get their next
    // LoadNativeAddon() invocation treated as resolveMode=source even when
    // package.json says "binary". Set when the user clicks Reload Native
    // Addons on a binary-mode addon — Reload means "recompile my local
    // source", not "redownload the published binary". Consumed (erased)
    // on first read so the override doesn't persist beyond a single load.
    std::unordered_set<std::string>  mForceSourceForNextLoad;

    // Internal helpers
    void StartNextQueuedBuild();
    void FinalizeAsyncBuild(AsyncAddonBuild& job, bool success);
    bool LoadNativeAddonAfterBuild(const std::string& addonId, std::string& outError);

    // Project-restart helpers
    void ProjectRestartBeginClose();   // dirty queue exhausted → close project + enqueue rebuilds
    void ProjectRestartOnBuildsDone(); // called from TickAsyncBuilds when in Building phase
    void ProjectRestartReset();        // clear state back to Phase::None
};

#endif // EDITOR
