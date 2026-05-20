#pragma once

#if EDITOR

#include "EngineTypes.h"
#include "Plugins/EditorUIHooks.h"
#include "Plugins/PolyphaseBuildTargetAPI.h"

#include <string>
#include <vector>

/**
 * @brief An owned copy of a PolyphaseBuildTargetDesc.
 *
 * When an addon registers a build target, the engine deep-copies every
 * const char* from the descriptor into this struct. The original
 * descriptor (and its string literals) may live in the addon DLL, which
 * disappears on hot-reload; this owned copy survives until
 * RemoveAllForHook(hookId) scrubs it.
 *
 * The function pointers in mDesc stay bound to the addon DLL, so they
 * are only valid until that addon is unloaded. mDesc.targetId etc. are
 * patched to point at the std::string copies below.
 */
struct RegisteredBuildTarget
{
    HookId mHookId = 0;                 /**< Hook id from the addon; 0 for built-ins. */
    bool   mIsBuiltIn = false;          /**< True if registered by the engine itself. */

    /* Deep-copied strings — own the memory the descriptor points at. */
    std::string mTargetId;
    std::string mDisplayName;
    std::string mIconText;
    std::string mCategory;
    std::string mBinaryExtension;
    std::string mPlatformExtensionDir;   // empty unless addon provides a runtime

    /** Descriptor with strings re-pointed at our std::string copies. */
    PolyphaseBuildTargetDesc mDesc{};
};

/**
 * @brief Registry of all known build targets, built-in and addon-provided.
 *
 * Owned by EditorUIHookManager. Lookups are by string id (the only stable
 * cross-DLL identifier). RemoveAllForHook is called from
 * EditorUIHookManager::RemoveAllHooks before the addon DLL is unloaded.
 *
 * Thread safety: not thread-safe. All callers must be on the editor thread.
 */
class BuildTargetRegistry
{
public:
    /**
     * Register or replace a build target. Strings are deep-copied; the
     * caller may free their descriptor immediately after this returns.
     *
     * Replacing semantics: registering twice with the same targetId
     * overwrites the previous entry. This is what makes hot-reload work
     * — when an addon DLL is reloaded the second registration replaces
     * the stale function pointers from the previous load.
     */
    void Register(HookId hookId, const PolyphaseBuildTargetDesc* desc, bool isBuiltIn = false);

    /** Remove a single target. No-op if not found. */
    void Unregister(HookId hookId, const char* targetId);

    /** Remove every non-built-in target whose mHookId matches. Called from RemoveAllHooks. */
    void RemoveAllForHook(HookId hookId);

    /** Lookup by id. Returns nullptr if absent. */
    const RegisteredBuildTarget* Find(const char* targetId) const;

    /** Lookup the first built-in target whose basePlatform matches. Used for legacy Platform-only profiles. */
    const RegisteredBuildTarget* FindBuiltInByPlatform(Platform platform) const;

    /** All currently-registered targets. */
    const std::vector<RegisteredBuildTarget>& GetAll() const { return mTargets; }

    /** Number of currently-registered targets. */
    size_t Count() const { return mTargets.size(); }

    /** Wipe every target — including built-ins. Used during editor shutdown only. */
    void Clear();

private:
    std::vector<RegisteredBuildTarget> mTargets;
};

#endif /* EDITOR */
