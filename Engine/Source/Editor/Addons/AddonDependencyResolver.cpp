#if EDITOR

#include "AddonDependencyResolver.h"
#include "AddonManager.h"
#include "Engine.h"
#include "Log.h"
#include "Stream.h"
#include "System/System.h"
#include "Utilities.h"

#include "document.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace
{
    std::string PackagesDir()
    {
        const std::string& projDir = GetEngineState()->mProjectDirectory;
        if (projDir.empty()) return std::string();
        return projDir + "Packages/";
    }

    bool ReadJsonFile(const std::string& path, rapidjson::Document& outDoc)
    {
        Stream stream;
        if (!stream.ReadFile(path.c_str(), false))
        {
            return false;
        }
        std::string str(stream.GetData(), stream.GetSize());
        outDoc.Parse(str.c_str());
        return !outDoc.HasParseError();
    }

    // Try to fetch & install a missing addon. Returns true on success.
    bool FetchAndInstall(const AddonDependencySpec& dep, std::string& outError)
    {
        AddonManager* mgr = AddonManager::Get();
        if (mgr == nullptr)
        {
            outError = "AddonManager not available";
            return false;
        }

        switch (dep.mKind)
        {
            case AddonDependencySpec::GitHubRepo:
            case AddonDependencySpec::ZipUrl:
                return mgr->DownloadAndInstallFromUrl(dep.mId, dep.mUrl, dep.mRef, outError);

            case AddonDependencySpec::Registry:
            default:
            {
                const Addon* found = mgr->FindAddon(dep.mId);
                if (found == nullptr)
                {
                    outError = "Dependency '" + dep.mId + "' not found in any configured repository.";
                    return false;
                }
                return mgr->DownloadAddon(*found, outError);
            }
        }
    }

    // Three-color DFS visit. Returns true if subtree fully resolved (no cycle, no
    // unresolvable dep). Pushes `id` onto outOrder after its deps are visited.
    bool Visit(const std::string& id,
               std::unordered_map<std::string, int>& color, // 0=white, 1=gray, 2=black
               std::vector<std::string>& outOrder,
               std::vector<std::string>& outMissing,
               std::string& outError,
               bool autoFetch)
    {
        auto& c = color[id];
        if (c == 2) return true;            // already finalized
        if (c == 1)
        {
            LogWarning("Addon dependency cycle detected at '%s'; skipping.", id.c_str());
            return false;
        }
        c = 1;

        std::string addonDir = PackagesDir() + id + "/";
        std::vector<AddonDependencySpec> deps;
        std::string onInstall;

        if (!AddonDependencyResolver::ReadDependenciesFromDisk(addonDir, deps, onInstall))
        {
            // Addon not on disk yet — try to fetch it; if we can't, it's unresolved.
            // (Caller only invokes Visit for ids it expects on disk; missing here means
            // a recursive dep that we couldn't auto-install yet.)
            outMissing.push_back(id);
            c = 2;
            return false;
        }

        bool allOk = true;
        for (const AddonDependencySpec& dep : deps)
        {
            std::string depDir = PackagesDir() + dep.mId + "/";
            std::string depJson = depDir + "package.json";
            if (!SYS_DoesFileExist(depJson.c_str(), false))
            {
                if (autoFetch)
                {
                    std::string err;
                    LogDebug("Resolving missing addon dependency '%s' for '%s'...", dep.mId.c_str(), id.c_str());
                    if (!FetchAndInstall(dep, err))
                    {
                        LogWarning("Could not install dependency '%s' for '%s': %s",
                                   dep.mId.c_str(), id.c_str(), err.c_str());
                        outMissing.push_back(dep.mId);
                        outError = err;
                        allOk = false;
                        continue;
                    }
                }
                else
                {
                    outMissing.push_back(dep.mId);
                    allOk = false;
                    continue;
                }
            }

            if (!Visit(dep.mId, color, outOrder, outMissing, outError, autoFetch))
            {
                allOk = false;
            }
        }

        c = 2;
        outOrder.push_back(id);
        return allOk;
    }
}

namespace AddonDependencyResolver
{
    bool ReadDependenciesFromDisk(const std::string& addonDir,
                                  std::vector<AddonDependencySpec>& outDeps,
                                  std::string& outOnInstall)
    {
        outDeps.clear();
        outOnInstall.clear();

        std::string packageJsonPath = addonDir;
        if (!packageJsonPath.empty() && packageJsonPath.back() != '/' && packageJsonPath.back() != '\\')
        {
            packageJsonPath += "/";
        }
        packageJsonPath += "package.json";

        if (!SYS_DoesFileExist(packageJsonPath.c_str(), false))
        {
            return false;
        }

        rapidjson::Document doc;
        if (!ReadJsonFile(packageJsonPath, doc))
        {
            return false;
        }

        if (doc.HasMember("onInstall") && doc["onInstall"].IsString())
        {
            outOnInstall = doc["onInstall"].GetString();
        }

        auto pushFromValue = [&](const rapidjson::Value& v) {
            if (v.IsObject())
            {
                for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it)
                {
                    if (!it->name.IsString()) continue;
                    std::string id = it->name.GetString();
                    std::string val = it->value.IsString() ? it->value.GetString() : std::string();
                    outDeps.push_back(AddonDependencySpec::FromValue(id, val));
                }
            }
            else if (v.IsArray())
            {
                for (rapidjson::SizeType i = 0; i < v.Size(); ++i)
                {
                    if (!v[i].IsString()) continue;
                    outDeps.push_back(AddonDependencySpec::FromValue(v[i].GetString(), std::string()));
                }
            }
        };

        if (doc.HasMember("dependencies"))
        {
            pushFromValue(doc["dependencies"]);
        }
        else if (doc.HasMember("native") && doc["native"].IsObject())
        {
            const rapidjson::Value& nat = doc["native"];
            if (nat.HasMember("dependencies"))
            {
                pushFromValue(nat["dependencies"]);
            }
        }
        return true;
    }

    bool Resolve(const std::string& rootAddonId,
                 std::vector<std::string>& outOrder,
                 std::string& outError)
    {
        outOrder.clear();
        outError.clear();

        if (PackagesDir().empty())
        {
            outError = "No project loaded";
            return false;
        }

        std::unordered_map<std::string, int> color;
        std::vector<std::string> missing;
        Visit(rootAddonId, color, outOrder, missing, outError, /*autoFetch=*/true);

        if (!missing.empty())
        {
            std::string m;
            for (const std::string& s : missing) { m += s; m += " "; }
            LogWarning("Resolve('%s'): unresolved deps -> %s", rootAddonId.c_str(), m.c_str());
        }
        return true;
    }

    bool ResolveAll(std::vector<std::string>& outOrder,
                    std::vector<std::string>& outMissing,
                    std::string& outError)
    {
        outOrder.clear();
        outMissing.clear();
        outError.clear();

        std::string pkgDir = PackagesDir();
        if (pkgDir.empty())
        {
            outError = "No project loaded";
            return false;
        }

        if (!DoesDirExist(pkgDir.c_str()))
        {
            // No packages at all — nothing to resolve.
            return true;
        }

        AddonManager* mgr = AddonManager::Get();
        bool autoFetch = (mgr == nullptr) ? true : mgr->GetAutoResolveDependencies();

        std::vector<std::string> rootIds;
        DirEntry dirEntry;
        SYS_OpenDirectory(pkgDir, dirEntry);
        while (dirEntry.mValid)
        {
            if (dirEntry.mDirectory &&
                strcmp(dirEntry.mFilename, ".") != 0 &&
                strcmp(dirEntry.mFilename, "..") != 0)
            {
                std::string pj = pkgDir + dirEntry.mFilename + "/package.json";
                if (SYS_DoesFileExist(pj.c_str(), false))
                {
                    rootIds.push_back(dirEntry.mFilename);
                }
            }
            SYS_IterateDirectory(dirEntry);
        }
        SYS_CloseDirectory(dirEntry);

        std::unordered_map<std::string, int> color;
        for (const std::string& id : rootIds)
        {
            Visit(id, color, outOrder, outMissing, outError, autoFetch);
        }

        // De-duplicate outMissing (a missing dep may have been declared by several addons).
        std::sort(outMissing.begin(), outMissing.end());
        outMissing.erase(std::unique(outMissing.begin(), outMissing.end()), outMissing.end());

        return true;
    }
}

#endif
