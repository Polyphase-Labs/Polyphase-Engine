#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "EngineTypes.h"

struct FileEntry
{
    std::string mPath;
    int64_t mModTime = 0;
};

struct BuildManifest
{
    static constexpr uint32_t kCurrentVersion = 1;

    uint32_t mVersion = kCurrentVersion;
    Platform mPlatform = Platform::Windows;
    bool mEmbedded = false;
    bool mStaticContent = false;
    bool mContentPak = false;
    int64_t mBuildTime = 0;
    std::string mProjectName;
    std::string mOutputDirectory;
    std::string mTargetVariant;     // discriminates targets sharing a basePlatform; "" for the canonical built-in

    std::vector<FileEntry> mAssets;
    std::vector<FileEntry> mScripts;
    std::vector<FileEntry> mConfigFiles;
};

enum class BuildCacheResult
{
    UpToDate,
    NeedsRebuild,
    ManifestMissing,
    ManifestCorrupt,
    OutputMissing,
    VersionMismatch
};

class BuildCache
{
public:
    static BuildCache* Get();
    static void Create();
    static void Destroy();

    // staticContent participates in the manifest identity: a Moddable package and
    // a Static package of the same content are different outputs, so they must not
    // share a manifest or flipping the checkbox reports "up to date" and ships the
    // previous, unobfuscated build.
    //
    // targetVariant extends that identity to the build target. Platform alone is
    // not unique — RPM, AppImage, PSP, Dreamcast and PS2 all declare Linux as
    // their basePlatform — so without it they share one manifest and switching
    // between them falsely reports "up to date". Empty for the canonical built-in
    // of each platform, which keeps legacy manifest filenames unchanged. Callers
    // fold the target's profile options into the string so option edits invalidate.
    // outputDirectory is the absolute Packaged/<subdir>/ path this build writes
    // to; empty derives the legacy Packaged/<PlatformName>/. It has to be told
    // rather than derived because only ActionManager knows which target owns a
    // platform outright, and VerifyOutputDirectory checks it to decide whether a
    // cached build's output still exists.
    BuildCacheResult CheckRebuildNeeded(Platform platform, bool embedded, bool staticContent, bool contentPak,
                                        const std::string& targetVariant = "",
                                        const std::string& outputDirectory = "");
    std::string GetRebuildReason() const { return mRebuildReason; }

    void BuildCurrentManifest(Platform platform, bool embedded, bool staticContent, bool contentPak,
                              const std::string& targetVariant = "",
                              const std::string& outputDirectory = "");
    bool SaveManifest();
    bool LoadManifest(Platform platform, bool embedded, bool staticContent, bool contentPak,
                      const std::string& targetVariant = "");

    std::string GetManifestPath(Platform platform, bool embedded, bool staticContent, bool contentPak,
                                const std::string& targetVariant = "") const;

private:
    BuildCache();
    ~BuildCache();

    void GatherAssetFiles(std::vector<FileEntry>& outAssets);
    void GatherScriptFiles(std::vector<FileEntry>& outScripts);
    void GatherConfigFiles(std::vector<FileEntry>& outConfigs);

    int64_t GetFileModTime(const std::string& path) const;
    bool CompareManifests(const BuildManifest& current, const BuildManifest& saved);
    bool VerifyOutputDirectory() const;

    static BuildCache* sInstance;

    BuildManifest mCurrentManifest;
    BuildManifest mSavedManifest;
    std::string mRebuildReason;
};

#endif
