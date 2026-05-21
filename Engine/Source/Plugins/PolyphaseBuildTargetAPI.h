#pragma once

/**
 * @file PolyphaseBuildTargetAPI.h
 * @brief Stable C ABI header for native build-target addons.
 *
 * Lets a native addon register an entire build/packaging target (compile,
 * cook, finalize, run) without modifying engine source. Addons that ship
 * console SDK code (Dreamcast/KallistiOS, PS2SDK, nxdk, etc.) keep their
 * SDK dependencies fully contained inside their own DLL — the engine
 * binary has zero link-time dependency on any console SDK it does not
 * already ship with.
 *
 * The structs below are plain C with function pointers only. No STL, no
 * exceptions. The host owns deep copies of every const char* you pass in
 * via Register, so your descriptor's string literals can be stack/static
 * memory that disappears at addon-unload time.
 *
 * To register: from your addon's RegisterEditorUI(EditorUIHooks*, HookId),
 * fill in a PolyphaseBuildTargetDesc and call hooks->RegisterBuildTarget.
 * The hook auto-clears on RemoveAllHooks(hookId), so hot-reload Just Works.
 */

#include <stdint.h>
#include <stddef.h>

#if EDITOR

/** Bump this when the descriptor or context layout changes. */
#define POLYPHASE_BUILD_TARGET_API_VERSION 1

/* ===== Forward-declared opaque pointers ================================== */
/* Addons receive these as void* and pass them back unchanged; the engine
 * downcasts. Keeping them opaque keeps the ABI independent of engine STL. */
typedef struct PolyphaseBuildTargetDesc PolyphaseBuildTargetDesc;
typedef struct PolyphaseBuildContext    PolyphaseBuildContext;

/* ===== Severity values forwarded to Log() trampoline ===================== */
/* Match LogSeverity in SystemTypes.h. */
#define POLYPHASE_BT_LOG_DEBUG    0
#define POLYPHASE_BT_LOG_WARNING  1
#define POLYPHASE_BT_LOG_ERROR    2

/* ===== Build context passed to every callback ============================ */
/*
 * Lifetime: valid for the duration of a single callback invocation only.
 * Do not store the pointer or any of its const char* fields past the call
 * — the engine reuses the memory for subsequent callbacks.
 */
struct PolyphaseBuildContext
{
    uint32_t structVersion;             /* equals POLYPHASE_BUILD_TARGET_API_VERSION at call time */

    /* Identity */
    const char* targetId;               /* the targetId you registered with */
    const char* projectName;            /* logical project name (e.g. "MyGame") */
    const char* projectDir;             /* absolute path to project root */
    const char* packageOutputDir;       /* Packaged/<targetId>/ — engine creates this */
    const char* engineDir;              /* absolute path to engine root */
    const char* compiledBinaryDir;      /* hint: where the engine expects your toolchain output */

    /* Cooking compatibility */
    int32_t     basePlatform;           /* Platform enum value (Windows/Linux/etc.) for asset cook fallback */

    /* Build settings */
    int32_t     embedded;               /* non-zero if assets are embedded in the binary */
    int32_t     runAfterBuild;          /* non-zero if user pressed Build & Run */
    int32_t     runOnDevice;            /* non-zero if user wants real-hardware run */
    int32_t     forceRebuild;           /* non-zero when "Force Rebuild" was checked.
                                           Addons should clean their per-target build
                                           artifacts (e.g. `make clean`, delete .o files)
                                           inside GetCompileCommand or PreCook before
                                           kicking off their toolchain. */

    /* Addon-owned per-build state. The engine never touches this. */
    void*       userData;

    /* Engine-owned opaque pointer. Cast and use at your own risk. */
    void*       opaqueEngineState;

    /* ===== Helper trampolines ============================================
     * Provided so addons don't have to re-link engine symbols. All function
     * pointers are non-null when the context is passed to a callback. */

    /** Append a log line at the given severity. */
    void (*Log)(int32_t severity, const char* msg);

    /** Append a line to the PackagingWindow build-output log. */
    void (*WriteOutputLine)(const char* line);

    /**
     * Read a per-profile target option key (json) into outVal. Returns
     * non-zero on hit. Useful for "what disc format does the user want?"
     */
    int32_t (*GetProfileSetting)(const char* key, char* outVal, size_t cap);

    /** Write/overwrite a per-profile target option key. Persisted with the profile. */
    void (*SetProfileSetting)(const char* key, const char* val);

    /**
     * Resolve a relative path against the project dir into an absolute path.
     * Returns non-zero on success.
     */
    int32_t (*ResolvePath)(const char* relative, char* outAbs, size_t cap);
};

/* ===== Build target descriptor ============================================
 *
 * Fill one of these inside RegisterEditorUI and pass to RegisterBuildTarget.
 *
 * REQUIRED fields: apiVersion, targetId, displayName, basePlatform,
 *                  binaryExtension, GetCompileCommand.
 *
 * All function-pointer callbacks may be set to NULL except GetCompileCommand.
 * Convention: a callback that returns int32_t returns non-zero on success.
 */
struct PolyphaseBuildTargetDesc
{
    /* ----- Identity / metadata --------------------------------------------- */
    uint32_t    apiVersion;             /* must equal POLYPHASE_BUILD_TARGET_API_VERSION */
    const char* targetId;               /* stable reverse-DNS id, e.g. "homebrew.dreamcast" */
    const char* displayName;            /* human label, e.g. "Dreamcast (KallistiOS)" */
    const char* iconText;               /* optional ImGui glyph/short text */
    const char* category;               /* grouping in the UI dropdown ("Retro Consoles", etc.) */

    /* ----- Cook compatibility ---------------------------------------------- */
    int32_t     basePlatform;           /* Platform enum value to use for default asset cook */

    /* ----- Output ---------------------------------------------------------- */
    const char* binaryExtension;        /* ".elf", ".cdi", ".xbe", ".nds", ... */

    /* ----- Capability flags ------------------------------------------------ */
    int32_t     requiresDocker;         /* non-zero if Docker build is required */
    int32_t     supportsRunOnDevice;    /* non-zero if RunOnDevice callback is meaningful */
    int32_t     supportsEmulator;       /* non-zero if RunInEmulator callback is meaningful */

    /* ----- Lifecycle callbacks (all nullable except GetCompileCommand) ----- */

    /**
     * Validate that the toolchain/SDK is present on this machine.
     * Filled with a user-visible reason in outReason if it returns 0.
     * Engine treats null callback as "always valid". Called when the
     * Packaging UI is drawn — keep it fast.
     */
    int32_t (*Validate)(char* outReason, size_t reasonCap);

    /**
     * Called once at build start, before asset cooking begins. Use to
     * create per-build temp dirs, manifest files, etc. Non-zero return =
     * proceed; zero = abort the build.
     */
    int32_t (*PreCook)(const PolyphaseBuildContext* ctx);

    /**
     * Per-asset cook override. Called once per asset during Phase 1.
     *
     * Return non-zero if you handled the cook (you have written the cooked
     * bytes into the Stream at streamPtr — engine will then commit the
     * stream to disk). Return zero to fall back to the engine's default
     * Asset::SaveStream(stream, basePlatform) cook.
     *
     * assetTypeName is the asset class name as returned by Asset::GetClassName
     * (e.g. "Texture", "SoundWave", "StaticMesh").
     * assetPtr is the Asset* (cast appropriately).
     * streamPtr is the Stream* the engine will commit to disk.
     */
    int32_t (*CookAsset)(const PolyphaseBuildContext* ctx,
                         const char* assetTypeName, void* assetPtr, void* streamPtr);

    /**
     * REQUIRED. Produce the shell command line that compiles the project
     * for this target. The engine runs it via SYS_ExecFull from the
     * project directory. Write the command into outCmd (NUL-terminated).
     * Return non-zero on success.
     */
    int32_t (*GetCompileCommand)(const PolyphaseBuildContext* ctx, char* outCmd, size_t cmdCap);

    /**
     * Tell the engine where to find the freshly-compiled binary after
     * GetCompileCommand succeeds. The engine copies that file into
     * packageOutputDir. Write the absolute path into outPath. Return
     * non-zero on success. Null callback = engine uses
     * compiledBinaryDir + projectName + binaryExtension.
     */
    int32_t (*GetCompiledBinaryPath)(const PolyphaseBuildContext* ctx, char* outPath, size_t pathCap);

    /**
     * Optional post-package step. Runs after the compiled binary is in
     * packageOutputDir. Use it to wrap into GDI/CDI/ISO/CIA/NDS-rom etc.
     * Non-zero return = success, zero = mark the build as failed.
     */
    int32_t (*PostPackage)(const PolyphaseBuildContext* ctx);

    /**
     * Optional: produce the shell command to deploy/run on real hardware.
     * Only invoked when supportsRunOnDevice != 0 and runOnDevice is set.
     */
    int32_t (*RunOnDevice)(const PolyphaseBuildContext* ctx, char* outCmd, size_t cmdCap);

    /**
     * Optional: produce the shell command to launch the emulator.
     * Only invoked when supportsEmulator != 0 and runAfterBuild is set.
     */
    int32_t (*RunInEmulator)(const PolyphaseBuildContext* ctx, char* outCmd, size_t cmdCap);

    /* ----- Editor UI integration (all optional) ---------------------------- */

    /**
     * Draw target-specific options into the Build Profile panel. Called
     * inside an ImGui group every frame the profile is selected. The
     * addon uses ImGui from its DLL (the engine wires the ImGui context
     * via ImGuiPluginContext at load time).
     *
     * Read/write per-profile option strings via ctx->GetProfileSetting /
     * ctx->SetProfileSetting — these round-trip through the BuildProfile
     * JSON automatically. The addon never needs to know what BuildProfile
     * actually is.
     */
    void (*DrawProfileOptions)(const PolyphaseBuildContext* ctx);

    /** Reserved for future use; engine persists ctx->Set/GetProfileSetting values automatically. */
    void (*SerializeProfileOptions)(const PolyphaseBuildContext* ctx, void* rapidJsonValuePtr);

    /** Reserved for future use; engine restores ctx->Set/GetProfileSetting values automatically. */
    void (*DeserializeProfileOptions)(const PolyphaseBuildContext* ctx, const void* rapidJsonValuePtr);

    /* ----- Variant-2 platform extension (optional) ------------------------
     *
     * Used by addons that ship an engine runtime (Graphics/Input/Audio/etc.
     * implementations) for the target platform — i.e. real new-platform
     * support, not just packaging.
     *
     * platformExtensionDir is an addon-relative directory containing well-
     * known files that supply the platform-specific halves of engine fork
     * headers:
     *
     *   <addonRoot>/<platformExtensionDir>/
     *     SystemTypes_Platform.h     (required)
     *     InputTypes_Platform.h      (required)
     *     AudioTypes_Platform.h      (required)
     *     NetworkTypes_Platform.h    (optional — falls back to stub)
     *
     * When set, ActionManager writes bridge headers to
     * <projectDir>/Generated/PolyphasePlatform_*.h that forward to these
     * files via absolute path. The engine's fork headers detect this via
     * `#if defined(POLYPHASE_PLATFORM_ADDON)` and prefer the bridge over
     * the legacy PLATFORM_* arms.
     *
     * Leave NULL if your addon only does packaging (asset cook + EBOOT
     * wrap) and reuses the engine's existing built-in platform runtime
     * (e.g. cross-builds a Linux .elf for the basePlatform).
     */
    const char* platformExtensionDir;

    /* ----- Reserved for ABI growth (always zero-initialise) --------------- */
    void* reserved[7];
};

#endif /* EDITOR */
