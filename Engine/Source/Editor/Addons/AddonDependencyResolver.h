#pragma once

#if EDITOR

#include "../ProjectSelect/TemplateData.h"

#include <string>
#include <vector>

/**
 * @brief Resolves cross-addon dependencies declared in package.json.
 *
 * Walks the dependency graph of installed addons under <Project>/Packages/,
 * fetching+installing any missing entries via AddonManager. Cycle-safe:
 * cycled addons are skipped (not fatal). Native and non-native addons are
 * treated uniformly here — non-native parents can depend on native deps and
 * vice-versa. Native build/load order is then derived from this same graph
 * by NativeAddonManager.
 */
namespace AddonDependencyResolver
{
    /**
     * @brief Recursively resolve dependencies of a single addon.
     * @param rootAddonId   ID of the addon whose deps to walk
     * @param outOrder      [out] topological order: deps first, rootAddonId last
     * @param outError      [out] human-readable error if any non-fatal issue occurred
     * @return false on fatal error (parse failures, project dir missing); true otherwise.
     */
    bool Resolve(const std::string& rootAddonId,
                 std::vector<std::string>& outOrder,
                 std::string& outError);

    /**
     * @brief Resolve dependencies for every addon under <Project>/Packages/.
     * @param outOrder      [out] global topo order
     * @param outMissing    [out] IDs declared as deps somewhere but not resolvable
     * @param outError      [out] human-readable error if any non-fatal issue occurred
     * @return false on fatal error; true otherwise.
     */
    bool ResolveAll(std::vector<std::string>& outOrder,
                    std::vector<std::string>& outMissing,
                    std::string& outError);

    /**
     * @brief Parse a single addon's top-level "dependencies" map from disk.
     *        Skips the rest of the package.json so it works for native or non-native.
     */
    bool ReadDependenciesFromDisk(const std::string& addonDir,
                                  std::vector<AddonDependencySpec>& outDeps,
                                  std::string& outOnInstall);

    /** @brief Top-level "version" from an addon's package.json; empty if absent. */
    std::string ReadPackageVersion(const std::string& addonDir);

    /** @brief Ids of every <Project>/Packages/<id>/ that has a package.json. */
    std::vector<std::string> ListPackageIds();

    /**
     * @brief Every package on disk that (transitively) depends on addonId.
     *
     * No fetching. outInstalled holds dependents that have an InstalledAddon
     * record, ordered farthest-dependent-first so they can be removed in that
     * order; outLocalOnly holds dependents present in Packages/ with no record
     * (hand-made dev packages the Addons window never deletes). addonId itself
     * is never included. Cycle-safe.
     */
    void CollectDependents(const std::string& addonId,
                           std::vector<std::string>& outInstalled,
                           std::vector<std::string>& outLocalOnly);
}

#endif
