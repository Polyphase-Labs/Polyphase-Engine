#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <unordered_map>
#include <stdint.h>

/**
 * @brief Specifies where native addon code runs.
 */
enum class NativeAddonTarget
{
    EditorOnly,     // Only loads in editor, NOT compiled into final builds
    EngineAndEditor // Loads in editor AND compiled into final builds
};

enum class NativeAddonResolveMode
{
    Source,
    Binary
};

struct NativeBinaryDescriptor
{
    std::string mPlatform;
    std::string mArch;
    std::string mConfig;
    std::string mType;           // releaseAsset | url | zip
    std::string mValue;          // asset name or URL
    std::string mChecksumSha256; // optional
    std::string mEntryPath;      // optional (zip extraction)
};

/**
 * @brief Source for a single addon-to-addon dependency.
 */
struct AddonDependencySpec
{
    enum Kind
    {
        Registry,    // Resolve via configured AddonManager repos
        GitHubRepo,  // URL is a GitHub repo; mRef selects branch/tag/commit
        ZipUrl       // URL points directly at a .zip
    };

    std::string mId;   // Addon ID (folder name under Packages/)
    std::string mUrl;  // Source URL (empty for Registry)
    std::string mRef;  // GitHub ref (branch/tag/commit); empty = default branch
    Kind mKind = Registry;

    // Build a spec from "id" and "<url>[#ref]" / "" (registry lookup).
    // Classification:
    //   - empty / no "://"     -> Registry
    //   - ends in ".zip"        -> ZipUrl
    //   - contains "github.com" -> GitHubRepo (ref split on '#')
    //   - other URL             -> ZipUrl (assume direct download)
    static AddonDependencySpec FromValue(const std::string& id, const std::string& value)
    {
        AddonDependencySpec out;
        out.mId = id;
        if (value.empty() || value.find("://") == std::string::npos)
        {
            out.mKind = Registry;
            out.mUrl = value;
            return out;
        }

        std::string url = value;
        size_t hashPos = url.find('#');
        if (hashPos != std::string::npos)
        {
            out.mRef = url.substr(hashPos + 1);
            url = url.substr(0, hashPos);
        }
        out.mUrl = url;

        std::string lower = url;
        for (char& c : lower) c = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
        if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".zip") == 0)
        {
            out.mKind = ZipUrl;
        }
        else if (lower.find("github.com/") != std::string::npos)
        {
            out.mKind = GitHubRepo;
        }
        else
        {
            out.mKind = ZipUrl;
        }
        return out;
    }
};

/**
 * @brief Native module configuration for addons with C++ code.
 */
struct NativeModuleMetadata
{
    bool mHasNative = false;           // Whether addon has native code
    NativeAddonTarget mTarget = NativeAddonTarget::EngineAndEditor;  // Where code runs
    std::string mSourceDir = "Source"; // Relative path to source directory
    std::string mBinaryName;           // Output binary name (without extension)
    std::string mEntrySymbol = "PolyphasePlugin_GetDesc";
    std::string mExportDefine;         // Optional custom export macro (e.g., "INVENTORY_RUNTIME_EXPORTS")
    uint32_t mPluginApiVersion = 1;
    NativeAddonResolveMode mResolveMode = NativeAddonResolveMode::Source;
    std::vector<NativeBinaryDescriptor> mBinaries;

    // Optional extras for addons that bundle third-party libraries (e.g. FFmpeg).
    // All relative paths resolve against the addon's package root (folder containing package.json).
    // These are the COMMON arrays applied to every platform; per-platform overrides
    // (mPerPlatform) are concatenated on top by ResolveExtras().
    std::vector<std::string> mExtraDefines;     // e.g. ["POLYPHASE_WITH_FFMPEG=1"]
    std::vector<std::string> mExtraIncludeDirs; // e.g. ["External/ffmpeg/include"]
    std::vector<std::string> mExtraLibDirs;     // e.g. ["External/ffmpeg/lib"]
    std::vector<std::string> mExtraLibs;        // e.g. ["avformat.lib", "avcodec.lib", ...]
    std::vector<std::string> mCopyBinaries;     // Dirs whose contents get copied next to the addon DLL post-build (e.g. ["External/ffmpeg/bin"])

    // Per-platform extras. Same shape as the common arrays above; merged
    // (concatenated, common-first) by ResolveExtras() when generating the build
    // command for a specific target. Keys are GetPlatformString(Platform) values:
    // "Windows", "Linux", "Android", "GameCube", "Wii", "3DS". Missing keys mean
    // "no overrides for that platform". Lets PC-only deps (e.g. FFmpeg) live
    // under nativePerPlatform.Windows so console builds don't try to link them.
    struct PlatformExtras
    {
        std::vector<std::string> mExtraDefines;
        std::vector<std::string> mExtraIncludeDirs;
        std::vector<std::string> mExtraLibDirs;
        std::vector<std::string> mExtraLibs;
        std::vector<std::string> mCopyBinaries;
    };
    std::unordered_map<std::string, PlatformExtras> mPerPlatform;

    /**
     * @brief Metadata-only entries from `native.buildTargets` in package.json.
     *
     * Pure documentation/discovery: the addon's actual build-target registration
     * happens inside its RegisterEditorUI callback via
     * EditorUIHooks::RegisterBuildTarget. The engine uses this list to (a) show
     * "this addon provides X targets" in the AddonsWindow, (b) warn if the
     * addon failed to register a declared target, and (c) keep dropdown labels
     * correct when the addon DLL fails to load.
     */
    struct BuildTargetMetadata
    {
        std::string mId;            // Reverse-DNS id, e.g. "homebrew.dreamcast"
        std::string mDisplayName;   // Human label, e.g. "Dreamcast (KallistiOS)"
        std::string mCategory;      // UI grouping, e.g. "Retro Consoles"
    };
    std::vector<BuildTargetMetadata> mBuildTargets;

    // Returns the effective extras for a given platform: common arrays first,
    // then mPerPlatform[platformName] appended (if present). Pass an empty string
    // to get just the common arrays.
    PlatformExtras ResolveExtras(const std::string& platformName) const
    {
        PlatformExtras out;
        out.mExtraDefines     = mExtraDefines;
        out.mExtraIncludeDirs = mExtraIncludeDirs;
        out.mExtraLibDirs     = mExtraLibDirs;
        out.mExtraLibs        = mExtraLibs;
        out.mCopyBinaries     = mCopyBinaries;

        if (!platformName.empty())
        {
            auto it = mPerPlatform.find(platformName);
            if (it != mPerPlatform.end())
            {
                const PlatformExtras& over = it->second;
                out.mExtraDefines.insert    (out.mExtraDefines.end(),     over.mExtraDefines.begin(),     over.mExtraDefines.end());
                out.mExtraIncludeDirs.insert(out.mExtraIncludeDirs.end(), over.mExtraIncludeDirs.begin(), over.mExtraIncludeDirs.end());
                out.mExtraLibDirs.insert    (out.mExtraLibDirs.end(),     over.mExtraLibDirs.begin(),     over.mExtraLibDirs.end());
                out.mExtraLibs.insert       (out.mExtraLibs.end(),        over.mExtraLibs.begin(),        over.mExtraLibs.end());
                out.mCopyBinaries.insert    (out.mCopyBinaries.end(),     over.mCopyBinaries.begin(),     over.mCopyBinaries.end());
            }
        }
        return out;
    }
};

/**
 * @brief Metadata for a project template or addon.
 */
struct ContentMetadata
{
    std::string mId;           // Unique ID (directory name)
    std::string mName;
    std::string mAuthor;
    std::string mDescription;
    std::string mUrl;          // Source URL (GitHub, etc.)
    std::string mVersion;
    std::string mUpdated;      // ISO date string
    std::vector<std::string> mTags;
    bool mIsCpp = false;       // C++ or Lua (templates only)

    // Cross-addon dependencies, parsed from top-level "dependencies" in package.json
    // (works for both native and non-native addons).
    std::vector<AddonDependencySpec> mDependencies;

    // Relative path (from addon root) to a shell script run after install/dep-resolution.
    // Invoked as: bash <addonDir>/<mOnInstallScript> <platform> <addonDir>
    std::string mOnInstallScript;
};

/**
 * @brief Represents an installed template.
 */
struct Template
{
    ContentMetadata mMetadata;
    std::string mPath;         // Full path to template directory
    bool mHasThumbnail = false;
};

/**
 * @brief Represents an addon available from a repository.
 */
struct Addon
{
    ContentMetadata mMetadata;
    std::string mRepoUrl;      // Repository URL this addon came from
    bool mHasThumbnail = false;
    bool mIsInstalled = false;
    bool mIsMain = true;
    bool mIsStandalone = false; // Addon is entire repo, not a subdirectory
    std::string mInstalledVersion;
    NativeModuleMetadata mNative;  // Native module configuration
};

/**
 * @brief Represents an addon repository.
 */
struct AddonRepository
{
    std::string mName;
    std::string mUrl;          // Base URL (GitHub repo URL)
    std::vector<std::string> mAddonIds;  // List of addon IDs in this repo
};

/**
 * @brief Represents an installed addon in a project.
 */
struct InstalledAddon
{
    std::string mId;
    std::string mVersion;
    std::string mInstalledDate;  // ISO date string
    std::string mRepoUrl;
    bool mEnabled = true;        // Whether addon is enabled
    bool mEnableNative = true;   // Whether native code should be loaded
    NativeAddonResolveMode mNativeMode = NativeAddonResolveMode::Source;
    std::string mLastSyncAt;
    std::string mLastSyncSource;
    std::string mLastSyncStatus;
    bool mTrustedScripts = false; // User opted into "Always trust this addon" for onInstall scripts
};

#endif
