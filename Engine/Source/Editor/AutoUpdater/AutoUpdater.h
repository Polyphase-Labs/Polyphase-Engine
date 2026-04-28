#pragma once

#if EDITOR

#include "ReleaseInfo.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <string>

/**
 * @brief States for the update checking process.
 */
enum class UpdateCheckState
{
    Idle,               // No check in progress
    Checking,           // Currently checking for updates
    UpdateAvailable,    // A new version is available
    UpToDate,           // Current version is up to date
    Error,              // An error occurred during check
    Downloading,        // Currently downloading the update
    DownloadComplete    // Download finished, ready to install
};

/**
 * @brief Singleton class for managing auto-updates from GitHub releases.
 */
class AutoUpdater
{
public:
    static void Create();
    static void Destroy();
    static AutoUpdater* Get();

    /**
     * @brief Start checking for updates in a background thread.
     * @param showUIOnNoUpdate If true, show UI even when up-to-date
     */
    void CheckForUpdates(bool showUIOnNoUpdate = false);

    /**
     * @brief Cancel any in-progress check or download.
     */
    void Cancel();

    /**
     * @brief Start downloading the update installer.
     */
    void StartDownload();

    /**
     * @brief Launch the downloaded installer and close the editor.
     */
    void LaunchInstaller();

    /**
     * @brief Called each frame to process background thread results.
     */
    void Update();

    /**
     * @brief Get the current update check state.
     */
    UpdateCheckState GetState() const { return mState.load(); }

    /**
     * @brief Get the latest release info (valid after UpdateAvailable state).
     */
    const ReleaseInfo& GetLatestRelease() const { return mLatestRelease; }

    /**
     * @brief Get any error message from the last operation.
     */
    const std::string& GetErrorMessage() const { return mErrorMessage; }

    /**
     * @brief Get download progress (0.0 - 1.0).
     */
    float GetDownloadProgress() const;

    /**
     * @brief Get downloaded bytes.
     */
    size_t GetDownloadedBytes() const { return mDownloadedBytes.load(); }

    /**
     * @brief Get total download size in bytes.
     */
    size_t GetTotalBytes() const { return mTotalBytes.load(); }

    /**
     * @brief Dismiss the update notification (user chose "Remind Me Later").
     */
    void DismissUpdate();

    /**
     * @brief Skip this version (won't show notification for it again).
     */
    void SkipThisVersion();

    /**
     * @brief Check if the update window should be shown.
     */
    bool ShouldShowWindow() const { return mShowWindow; }

    /**
     * @brief Set whether the update window should be shown.
     */
    void SetShowWindow(bool show) { mShowWindow = show; }

private:
    AutoUpdater();
    ~AutoUpdater();

    void CheckThreadFunc();
    void DownloadThreadFunc();
    bool ParseReleaseJson(const std::string& json);
    void JoinThreads();

    static AutoUpdater* sInstance;

    // Background threads
    std::thread mCheckThread;
    std::thread mDownloadThread;

    // Thread control
    std::atomic<bool> mRunning{false};
    std::atomic<bool> mCancelRequested{false};
    std::atomic<UpdateCheckState> mState{UpdateCheckState::Idle};

    // Download progress
    std::atomic<size_t> mDownloadedBytes{0};
    std::atomic<size_t> mTotalBytes{0};

    // Results (protected by mutex)
    std::mutex mResultMutex;
    ReleaseInfo mLatestRelease;
    std::string mErrorMessage;
    std::string mDownloadedInstallerPath;

    // UI state
    bool mShowUIOnNoUpdate = false;
    bool mShowWindow = false;
    bool mCheckCompleteThisFrame = false;
    bool mDownloadCompleteThisFrame = false;

    // GitHub API config
    static constexpr const char* kGitHubApiUrl = "https://api.github.com/repos/polyphase-labs/polyphase-engine/releases/latest";
    static constexpr const char* kGitHubApiAllReleasesUrl = "https://api.github.com/repos/polyphase-labs/polyphase-engine/releases";
};

#endif // EDITOR
