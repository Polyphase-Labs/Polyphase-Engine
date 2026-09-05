#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <unordered_map>

#include "imgui.h"

struct Addon;
struct InstalledAddon;

/**
 * @brief Window for browsing and installing addons.
 *
 * Provides:
 * - Browse addons from configured repositories
 * - View installed addons
 * - Manage addon repositories
 */
class AddonsWindow
{
public:
    AddonsWindow();
    ~AddonsWindow();

    void Open();
    void Close();
    void Draw();
    bool IsOpen() const { return mIsOpen; }

    // Drop the thumbnail path memo and ask the shared EditorImageCache to
    // forget our entries. Called from EditorImguiPreShutdown; the GPU release
    // itself is the cache's business.
    void Shutdown();

    // Queue an addon for download + install by id. Used by the native addon
    // problem modal's "Install <dependency>" button and the Installed tab.
    // The work itself happens in ProcessPendingInstalls.
    void InstallAddonById(const std::string& addonId);

    // Drain EditorState::mPendingAddonInstalls. Called by EditorMain's
    // end-of-frame dispatcher (never from inside an ImGui frame): downloads
    // and installs each queued addon under an EditorProgress modal, refreshes
    // the registry first if this session has not, registers any new native
    // package with NativeAddonManager, re-checks addons that were blocked on
    // a missing dependency, and finally reloads the affected native addons
    // through the project-restart path so they get compiled.
    void ProcessPendingInstalls();

private:
    void DrawAddonBrowser();
    void DrawInstalledAddons();
    void DrawRepositoryManager();
    void DrawAddonCard(const Addon& addon, float cardWidth);
    void DrawAddonDetailsPopup();
    void DrawAddRepoPopup();

    // Table-view + shared helpers
    void DrawViewModeToggle();
    void DrawAddonTable_Browse(const std::vector<const Addon*>& filtered);
    void DrawAddonTable_Installed(const std::vector<InstalledAddon>& installed);
    void DrawClampedName(const char* name, float maxWidth);
    void LoadViewSettings();
    void SaveViewSettings();

    void OnDownloadAddon(const std::string& addonId);
    void OnViewMore(const std::string& addonId);
    void OnUninstallAddon(const std::string& addonId);

    // Git-repo helpers for the per-row "Open in Version Control" /
    // "Init Git" actions. HasGitRepo does a libgit2 discover under the
    // hood (filesystem walk) — caching per row avoids re-running it every
    // frame across the full installed-addon list. Cache invalidates when
    // the active project changes.
    bool DoesAddonHaveGitRepo(const std::string& addonId);
    void InvalidateAddonGitRepoCacheIfProjectChanged();
    std::string GetAddonDirectory(const std::string& addonId) const;
    void OpenAddonInVersionControl(const std::string& addonId);
    void InitAddonGitRepo(const std::string& addonId);
    void OnAddRepository();
    void OnRemoveRepository(const std::string& url);
    void OnRefreshRepositories();
    void OnResolveDependencies();

    // Native addon operations
    void OnBuildNativeAddon(const std::string& addonId);
    void OnReloadNativeAddon(const std::string& addonId);
    void OnToggleNativeEnabled(const std::string& addonId);
    void OnToggleNativeMode(const std::string& addonId);
    void OnSyncNativeAddonBinary(const std::string& addonId);

    bool mIsOpen = false;
    int mSelectedTab = 0;  // 0=Browse, 1=Installed, 2=Repositories

    // View mode: true = table (default), false = cards. Persisted to disk via JsonSettings.
    bool mUseTableView = true;

    // Popup state
    bool mShowAddonDetails = false;
    bool mShowAddRepoPopup = false;
    std::string mSelectedAddonId;
    char mRepoUrlBuffer[512] = {};
    std::string mErrorMessage;
    std::string mStatusMessage;

    // Filter
    char mSearchBuffer[256] = {};
    std::vector<std::string> mSelectedTags;
    std::string mSelectedCategory;  // empty = "All"

    // Available tags (collected from addons)
    std::vector<std::string> mAvailableTags;

    // Refresh state
    bool mNeedsRefresh = true;
    bool mIsRefreshing = false;

    // Uninstall confirmation
    bool mShowUninstallConfirm = false;
    std::string mUninstallAddonId;
    void DrawUninstallConfirmPopup();

    // Build log popup
    bool mShowBuildLog = false;
    std::string mBuildLogAddonId;

    // Thumbnail path memo: addonId -> resolved absolute PNG path ("" when the
    // addon has none). The decoded texture lives in the shared
    // EditorImageCache, which owns it for the editor session; this map only
    // memoizes the two-location path search so we don't stat the filesystem
    // once per card per frame.
    ImTextureID GetAddonThumbnail(const std::string& addonId);
    void ClearThumbnailCache();
    std::unordered_map<std::string, std::string> mThumbnailPaths;

    // Per-addon "is this dir inside a git repo" cache. Keyed by addonId
    // (the Packages/ subdir name). Cleared on project switch.
    std::unordered_map<std::string, bool> mAddonGitRepoCache;
    std::string mGitRepoCacheProjectDir;
};

AddonsWindow* GetAddonsWindow();

#endif
