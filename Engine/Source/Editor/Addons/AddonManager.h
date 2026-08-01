#pragma once

#if EDITOR

#include "../ProjectSelect/TemplateData.h"
#include <string>
#include <vector>

/**
 * @brief Singleton manager for addon repositories and installed addons.
 *
 * Manages addon discovery from configured repositories, download/installation
 * to the current project, and tracking of installed addons for updates.
 */
class AddonManager
{
public:
    static void Create();
    static void Destroy();
    static AddonManager* Get();

    /** @brief Directory for addon cache: {AppData}/PolyphaseEditor/AddonCache/ */
    std::string GetAddonCacheDirectory();

    /** @brief Settings file: {AppData}/PolyphaseEditor/addons.json */
    std::string GetSettingsPath();

    /** @brief Installed addons file in project: {ProjectDir}/Settings/installed_addons.json */
    std::string GetInstalledAddonsPath();

    // Repository management
    void LoadSettings();
    void SaveSettings();
    void AddRepository(const std::string& url);
    void RemoveRepository(const std::string& url);
    const std::vector<AddonRepository>& GetRepositories() const { return mRepositories; }

    // Addon discovery
    void RefreshAllRepositories();
    void RefreshRepository(const std::string& url);
    const std::vector<Addon>& GetAvailableAddons() const { return mAvailableAddons; }
    const std::vector<AddonCategory>& GetCategories() const { return mCategories; }

    // Installation
    bool DownloadAddon(const Addon& addon, std::string& outError);
    bool InstallAddon(const std::string& addonCachePath, const std::string& addonId, std::string& outError);
    bool UninstallAddon(const std::string& addonId);

    // Fetch an addon from an arbitrary URL (GitHub repo or direct .zip), unzip it,
    // and install into the current project's Packages/. Used by the dependency resolver.
    bool DownloadAndInstallFromUrl(const std::string& addonId,
                                   const std::string& url,
                                   const std::string& ref,
                                   std::string& outError);

    // Auto-resolve declared dependencies on install / project load.
    bool GetAutoResolveDependencies() const { return mAutoResolveDeps; }
    void SetAutoResolveDependencies(bool v) { mAutoResolveDeps = v; SaveSettings(); }

    // Tracking
    void LoadInstalledAddons();
    void SaveInstalledAddons();
    const std::vector<InstalledAddon>& GetInstalledAddons() const { return mInstalledAddons; }
    std::vector<InstalledAddon>& GetInstalledAddonsMutable() { return mInstalledAddons; }
    bool IsAddonInstalled(const std::string& addonId) const;
    bool HasUpdate(const std::string& addonId) const;
    std::string GetInstalledVersion(const std::string& addonId) const;
    bool SetInstalledAddonNativeMode(const std::string& addonId, NativeAddonResolveMode mode);
    bool SyncNativeAddonBinary(const std::string& addonId, std::string& outError);

    // Find addon by ID
    const Addon* FindAddon(const std::string& addonId) const;

private:
    static AddonManager* sInstance;
    AddonManager();
    ~AddonManager();

    /** @brief Ensure cache directory exists */
    void EnsureCacheDirectory();

    /**
     * @brief Fetch the auto-generated registry manifest.json and populate available addons.
     *
     * Single request per repo: reads the top-level name/categories and every published
     * package's full metadata directly, replacing the per-addon package.json crawl.
     * @param outFound set true when a manifest.json was fetched+parsed (so the caller can
     *                 fall back to the legacy package.json path when it is false).
     */
    bool FetchManifest(const std::string& url, const std::string& branch, bool& outFound);

    bool FetchRepositoryManifest(const std::string& url, AddonRepository& outRepo, const std::string& branch);
    /** @brief Fetch addon metadata from addon's package.json */
    bool FetchAddonMetadata(const std::string& repoUrl, const std::string& addonId, Addon& outAddon, const std::string& branch);

    /** @brief Merge addon files into current project */
    bool MergeAddonIntoProject(const std::string& addonPath, std::string& outError);

    /** @brief Download a file from URL */
    bool DownloadFile(const std::string& url, const std::string& destPath, std::string& outError);

    /** @brief Extract a zip file */
    bool ExtractZip(const std::string& zipPath, const std::string& destDir, std::string& outError);

    std::string NormalizePath(const std::string& in);

    /** @brief Convert GitHub URL to raw content URL */
    std::string ConvertToRawUrl(const std::string& gitHubUrl, const std::string& filePath, const std::string& branch);

    /** @brief Convert GitHub URL to download URL */
    std::string ConvertToDownloadUrl(const std::string& gitHubUrl, const std::string& branch);

    /** @brief Get current timestamp as ISO string */
    std::string GetCurrentTimestamp();

    std::vector<AddonRepository> mRepositories;
    std::vector<Addon> mAvailableAddons;
    std::vector<AddonCategory> mCategories;  // Registry categories from manifest.json
    std::vector<InstalledAddon> mInstalledAddons;
    bool mAutoResolveDeps = true;
};

#endif
