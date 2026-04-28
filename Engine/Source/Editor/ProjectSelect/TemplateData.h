#pragma once

#if EDITOR

#include <string>
#include <vector>
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
    std::vector<std::string> mExtraDefines;     // e.g. ["POLYPHASE_WITH_FFMPEG=1"]
    std::vector<std::string> mExtraIncludeDirs; // e.g. ["External/ffmpeg/include"]
    std::vector<std::string> mExtraLibDirs;     // e.g. ["External/ffmpeg/lib"]
    std::vector<std::string> mExtraLibs;        // e.g. ["avformat.lib", "avcodec.lib", ...]
    std::vector<std::string> mCopyBinaries;     // Dirs whose contents get copied next to the addon DLL post-build (e.g. ["External/ffmpeg/bin"])
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
