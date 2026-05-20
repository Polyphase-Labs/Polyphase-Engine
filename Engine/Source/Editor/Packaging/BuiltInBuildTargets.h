#pragma once

#if EDITOR

class BuildTargetRegistry;

/**
 * @brief Engine-side registration of the six built-in build targets.
 *
 * Registers Windows / Linux / Android / GameCube / Wii / N3DS into the
 * BuildTargetRegistry so they appear alongside addon-provided targets in
 * the Packaging UI dropdown and can be looked up by string id.
 *
 * Built-in targets are registered with HookId == 0 and the
 * `mIsBuiltIn = true` flag so addon hot-reload cleanup never touches
 * them. Their descriptor function-pointer callbacks are intentionally
 * left as nullptr — ActionManager's Phase 1/2/3 detects built-ins
 * (via `RegisteredBuildTarget::mIsBuiltIn`) and runs its existing
 * switch-on-Platform code path. Addon-provided targets are dispatched
 * through the descriptor callbacks instead.
 *
 * This keeps zero regression risk for the six shipping platforms while
 * making the framework fully functional for new addon targets. Future
 * refactors can extract per-platform compile/cook/finalize logic into
 * BuiltInBuildTargets callbacks one platform at a time.
 */
namespace BuiltInBuildTargets
{
    /** @brief Register all six built-in targets. Call once at editor init. */
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

    /** @brief Look up the canonical built-in id for a Platform enum value, or "" if unknown. */
    const char* IdForPlatform(int platform);
}

#endif /* EDITOR */
