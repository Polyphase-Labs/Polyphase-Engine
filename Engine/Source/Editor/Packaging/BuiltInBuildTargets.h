#pragma once

#if EDITOR

class BuildTargetRegistry;

/**
 * @brief Engine-side registration of the built-in build targets.
 *
 * Registers Windows / Linux / Android / GameCube / Wii / N3DS into the
 * BuildTargetRegistry so they appear alongside addon-provided targets in
 * the Packaging UI dropdown and can be looked up by string id, plus the
 * Linux packaging targets (RPM, AppImage) that ride on Platform::Linux.
 *
 * Built-in targets are registered with HookId == 0 and the
 * `mIsBuiltIn = true` flag so addon hot-reload cleanup never touches
 * them. The six platform-owning targets leave every descriptor callback
 * null, so ActionManager's Phase 1/2/3 runs its existing
 * switch-on-Platform code path for them unchanged. Dispatch is on the
 * callback being present, not on `mIsBuiltIn`, which is what lets the
 * packaging targets below reuse the whole Linux pipeline and contribute
 * only a PostPackage step.
 *
 * `mIsBuiltIn` still decides one thing: which target *owns* a platform.
 * IdForPlatform names the canonical target per Platform, and anything
 * else sharing that basePlatform gets its own Packaged/<targetId>/
 * directory and its own build-cache manifest so the two can't collide.
 */
namespace BuiltInBuildTargets
{
    /** @brief Register every built-in target. Call once at editor init. */
    void RegisterAll(BuildTargetRegistry& registry);

    // Stable canonical ids — used by ActionManager / PackagingWindow as
    // fallback lookups when a BuildProfile only has the legacy
    // mTargetPlatform set (mTargetId empty).
    extern const char* const kWindowsId;   // "polyphase.windows"
    extern const char* const kLinuxId;     // "polyphase.linux"
    extern const char* const kAndroidId;   // "polyphase.android"
    extern const char* const kGameCubeId;  // "polyphase.gamecube"
    extern const char* const kWiiId;       // "polyphase.wii"
    extern const char* const kN3DSId;      // "polyphase.n3ds"

    /**
     * @brief Look up the canonical built-in id for a Platform enum value, or "" if unknown.
     *
     * Deliberately keeps returning kLinuxId for Platform::Linux even though the
     * RPM and AppImage targets share that basePlatform — "canonical" is what
     * grants the bare Packaged/Linux/ output path.
     */
    const char* IdForPlatform(int platform);
}

#endif /* EDITOR */
