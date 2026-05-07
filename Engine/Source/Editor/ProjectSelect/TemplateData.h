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
    std::vector<std::string> mDependencies;  // IDs of other native addons this depends on

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
};

#endif
