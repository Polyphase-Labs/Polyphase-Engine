#if EDITOR

#include "AutoUpdater.h"
#include "HttpClient.h"
#include "Preferences/Updates/UpdatesModule.h"
#include "Preferences/Updates/CuttingEdgeModule.h"
#include "Constants.h"
#include "Engine.h"
#include "Log.h"
#include "System/System.h"

#include "document.h"

#include <chrono>
#include <cstdlib>
#include <sstream>

#if PLATFORM_WINDOWS
#include <Windows.h>
#include <shellapi.h>
#endif

AutoUpdater* AutoUpdater::sInstance = nullptr;

void AutoUpdater::Create()
{
    if (sInstance == nullptr)
    {
        sInstance = new AutoUpdater();
    }
}

void AutoUpdater::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

AutoUpdater* AutoUpdater::Get()
{
    return sInstance;
}

AutoUpdater::AutoUpdater()
{
}

AutoUpdater::~AutoUpdater()
{
    Cancel();
    JoinThreads();
}

void AutoUpdater::JoinThreads()
{
    if (mCheckThread.joinable())
    {
        mCheckThread.join();
    }
    if (mDownloadThread.joinable())
    {
        mDownloadThread.join();
    }
}

void AutoUpdater::CheckForUpdates(bool showUIOnNoUpdate)
{
    if (!HttpClient::IsAvailable())
    {
        LogWarning("AutoUpdater: %s", HttpClient::GetMissingDependencyMessage().c_str());
        return;
    }

    if (mRunning.load())
    {
        LogDebug("AutoUpdater: Check already in progress");
        return;
    }

    // Join any previous thread
    JoinThreads();

    mShowUIOnNoUpdate = showUIOnNoUpdate;
    mCancelRequested.store(false);
    mRunning.store(true);
    mState.store(UpdateCheckState::Checking);
    mErrorMessage.clear();

    mCheckThread = std::thread(&AutoUpdater::CheckThreadFunc, this);
}

void AutoUpdater::Cancel()
{
    mCancelRequested.store(true);
}

void AutoUpdater::CheckThreadFunc()
{
    LogDebug("AutoUpdater: Checking for updates...");

    // Check if cutting edge builds are enabled
    CuttingEdgeModule* cuttingEdge = CuttingEdgeModule::Get();
    bool isCuttingEdge = cuttingEdge && cuttingEdge->GetCuttingEdgeEnabled();

    const char* url = isCuttingEdge ? kGitHubApiAllReleasesUrl : kGitHubApiUrl;
    HttpResponse response = HttpClient::Get(url, 15000);

    if (mCancelRequested.load())
    {
        mState.store(UpdateCheckState::Idle);
        mRunning.store(false);
        return;
    }

    if (!response.IsSuccess())
    {
        std::lock_guard<std::mutex> lock(mResultMutex);
        mErrorMessage = response.mError.empty() ?
            "HTTP error " + std::to_string(response.mStatusCode) : response.mError;
        LogError("AutoUpdater: Failed to check for updates: %s", mErrorMessage.c_str());
        mState.store(UpdateCheckState::Error);
        mRunning.store(false);
        return;
    }

    // Parse JSON response
    if (!ParseReleaseJson(response.mBody))
    {
        std::lock_guard<std::mutex> lock(mResultMutex);
        mErrorMessage = "Failed to parse release info";
        LogError("AutoUpdater: %s", mErrorMessage.c_str());
        mState.store(UpdateCheckState::Error);
        mRunning.store(false);
        return;
    }

    // Update last check time
    UpdatesModule* updates = UpdatesModule::Get();
    if (updates)
    {
        auto now = std::chrono::system_clock::now();
        int64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        updates->SetLastCheckTime(timestamp);
    }

    // Skip pre-release versions when cutting edge is disabled (safety net)
    if (!isCuttingEdge && mLatestRelease.mPrerelease)
    {
        LogDebug("AutoUpdater: Skipping pre-release %s (cutting edge disabled)",
            mLatestRelease.GetVersion().c_str());
        mState.store(UpdateCheckState::UpToDate);
        mRunning.store(false);
        return;
    }

    // Compare versions
    std::string currentVersion = POLYPHASE_VERSION_STRING;
    if (mLatestRelease.IsNewerThan(currentVersion, isCuttingEdge))
    {
        // Check if this version is skipped
        if (updates && !updates->GetSkippedVersion().empty())
        {
            if (updates->GetSkippedVersion() == mLatestRelease.GetVersion())
            {
                LogDebug("AutoUpdater: Version %s is skipped", mLatestRelease.GetVersion().c_str());
                mState.store(UpdateCheckState::UpToDate);
                mRunning.store(false);
                return;
            }
        }

        LogDebug("AutoUpdater: Update available: %s (current: %s)",
            mLatestRelease.GetVersion().c_str(), currentVersion.c_str());
        mState.store(UpdateCheckState::UpdateAvailable);
        mShowWindow = true;
    }
    else
    {
        LogDebug("AutoUpdater: Up to date (current: %s, latest: %s)",
            currentVersion.c_str(), mLatestRelease.GetVersion().c_str());
        mState.store(UpdateCheckState::UpToDate);
        if (mShowUIOnNoUpdate)
        {
            mShowWindow = true;
        }
    }

    mRunning.store(false);
}

static bool ParseSingleReleaseObject(const rapidjson::Value& obj, ReleaseInfo& outRelease)
{
    if (!obj.IsObject())
    {
        return false;
    }

    // Parse tag_name
    if (obj.HasMember("tag_name") && obj["tag_name"].IsString())
    {
        outRelease.mTagName = obj["tag_name"].GetString();
    }
    else
    {
        return false;
    }

    // Parse name
    if (obj.HasMember("name") && obj["name"].IsString())
    {
        outRelease.mName = obj["name"].GetString();
    }

    // Parse body (changelog)
    if (obj.HasMember("body") && obj["body"].IsString())
    {
        outRelease.mBody = obj["body"].GetString();
    }

    // Parse html_url
    if (obj.HasMember("html_url") && obj["html_url"].IsString())
    {
        outRelease.mHtmlUrl = obj["html_url"].GetString();
    }

    // Parse published_at
    if (obj.HasMember("published_at") && obj["published_at"].IsString())
    {
        outRelease.mPublishedAt = obj["published_at"].GetString();
    }

    // Parse prerelease flag
    if (obj.HasMember("prerelease") && obj["prerelease"].IsBool())
    {
        outRelease.mPrerelease = obj["prerelease"].GetBool();
    }

    // Parse assets
    outRelease.mAssets.clear();
    if (obj.HasMember("assets") && obj["assets"].IsArray())
    {
        const rapidjson::Value& assets = obj["assets"];
        for (rapidjson::SizeType i = 0; i < assets.Size(); ++i)
        {
            const rapidjson::Value& asset = assets[i];
            if (!asset.IsObject()) continue;

            ReleaseAsset ra;

            if (asset.HasMember("name") && asset["name"].IsString())
            {
                ra.mName = asset["name"].GetString();
            }

            if (asset.HasMember("browser_download_url") && asset["browser_download_url"].IsString())
            {
                ra.mDownloadUrl = asset["browser_download_url"].GetString();
            }

            if (asset.HasMember("content_type") && asset["content_type"].IsString())
            {
                ra.mContentType = asset["content_type"].GetString();
            }

            if (asset.HasMember("size") && asset["size"].IsUint64())
            {
                ra.mSize = (size_t)asset["size"].GetUint64();
            }

            if (!ra.mName.empty() && !ra.mDownloadUrl.empty())
            {
                outRelease.mAssets.push_back(std::move(ra));
            }
        }
    }

    return true;
}

bool AutoUpdater::ParseReleaseJson(const std::string& json)
{
    rapidjson::Document doc;
    doc.Parse(json.c_str());

    if (doc.HasParseError())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mResultMutex);

    if (doc.IsObject())
    {
        // Single release object (/releases/latest response)
        return ParseSingleReleaseObject(doc, mLatestRelease);
    }
    else if (doc.IsArray() && doc.Size() > 0)
    {
        // Array of releases (/releases response) - first entry is newest
        return ParseSingleReleaseObject(doc[0], mLatestRelease);
    }

    return false;
}

void AutoUpdater::StartDownload()
{
    if (mRunning.load())
    {
        return;
    }

    const ReleaseAsset* asset = mLatestRelease.GetAssetForPlatform();
    if (!asset)
    {
        mErrorMessage = "No compatible download found for this platform";
        mState.store(UpdateCheckState::Error);
        return;
    }

    JoinThreads();

    mCancelRequested.store(false);
    mRunning.store(true);
    mDownloadedBytes.store(0);
    mTotalBytes.store(asset->mSize);
    mState.store(UpdateCheckState::Downloading);

    mDownloadThread = std::thread(&AutoUpdater::DownloadThreadFunc, this);
}

void AutoUpdater::DownloadThreadFunc()
{
    const ReleaseAsset* asset = mLatestRelease.GetAssetForPlatform();
    if (!asset)
    {
        mState.store(UpdateCheckState::Error);
        mRunning.store(false);
        return;
    }

    // Build temp file path
    std::string tempDir;
#if PLATFORM_WINDOWS
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    tempDir = tempPath;
#else
    tempDir = "/tmp/";
#endif

    std::string destPath = tempDir + asset->mName;

    LogDebug("AutoUpdater: Downloading %s to %s", asset->mName.c_str(), destPath.c_str());

    auto progressCallback = [this](size_t downloaded, size_t total)
    {
        mDownloadedBytes.store(downloaded);
        if (total > 0)
        {
            mTotalBytes.store(total);
        }
    };

    bool success = HttpClient::DownloadFile(
        asset->mDownloadUrl,
        destPath,
        progressCallback,
        mCancelRequested);

    if (mCancelRequested.load())
    {
        mState.store(UpdateCheckState::Idle);
        mRunning.store(false);
        return;
    }

    if (success)
    {
        std::lock_guard<std::mutex> lock(mResultMutex);
        mDownloadedInstallerPath = destPath;
        LogDebug("AutoUpdater: Download complete: %s", destPath.c_str());
        mState.store(UpdateCheckState::DownloadComplete);
    }
    else
    {
        std::lock_guard<std::mutex> lock(mResultMutex);
        mErrorMessage = "Download failed";
        LogError("AutoUpdater: %s", mErrorMessage.c_str());
        mState.store(UpdateCheckState::Error);
    }

    mRunning.store(false);
}

void AutoUpdater::LaunchInstaller()
{
    std::string installerPath;
    {
        std::lock_guard<std::mutex> lock(mResultMutex);
        installerPath = mDownloadedInstallerPath;
    }

    if (installerPath.empty())
    {
        LogError("AutoUpdater: No installer to launch");
        return;
    }

    LogDebug("AutoUpdater: Launching installer: %s", installerPath.c_str());

#if PLATFORM_WINDOWS
    // Launch the installer with silent upgrade flags
    // /SILENT = show progress bar, no wizard pages
    // /CLOSEAPPLICATIONS = close running Polyphase instances
    // /RESTARTAPPLICATIONS = restart Polyphase after install
    HINSTANCE result = ShellExecuteA(
        nullptr,
        "open",
        installerPath.c_str(),
        "/SILENT /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS",
        nullptr,
        SW_SHOWNORMAL);

    if ((intptr_t)result <= 32)
    {
        LogError("AutoUpdater: Failed to launch installer (error %d)", (int)(intptr_t)result);
        return;
    }

    // Request editor shutdown
    Quit();
#elif PLATFORM_LINUX
    // For .deb files, open a terminal with the install command
    // For tarballs, just open the file manager
    if (installerPath.find(".deb") != std::string::npos)
    {
        std::string command = "x-terminal-emulator -e \"sudo dpkg -i '" + installerPath + "' && echo 'Installation complete. Press Enter to close.' && read\"";
        system(command.c_str());
    }
    else
    {
        // Open file manager to the download location
        std::string command = "xdg-open \"" + installerPath + "\"";
        system(command.c_str());
    }

    // Request editor shutdown
    Quit();
#endif
}

void AutoUpdater::Update()
{
    // Nothing to do for now - state is updated atomically by background threads
    // UI reads state directly
}

float AutoUpdater::GetDownloadProgress() const
{
    size_t total = mTotalBytes.load();
    if (total == 0)
    {
        return 0.0f;
    }
    return (float)mDownloadedBytes.load() / (float)total;
}

void AutoUpdater::DismissUpdate()
{
    mShowWindow = false;
    mState.store(UpdateCheckState::Idle);
}

void AutoUpdater::SkipThisVersion()
{
    UpdatesModule* updates = UpdatesModule::Get();
    if (updates)
    {
        updates->SetSkippedVersion(mLatestRelease.GetVersion());
    }
    DismissUpdate();
}

#endif // EDITOR
