#if EDITOR

#include "AddonManager.h"
#include "EditorImgui.h"
#include "AddonDependencyResolver.h"
#include "AddonScriptRunner.h"
#include "AutoUpdater/HttpClient.h"
#include "System/System.h"
#include "Engine.h"
#include "Stream.h"
#include "Utilities.h"
#include "Log.h"

#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <unordered_map>

AddonManager* AddonManager::sInstance = nullptr;

void AddonManager::Create()
{
    if (sInstance == nullptr)
    {
        sInstance = new AddonManager();
    }
}

void AddonManager::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

AddonManager* AddonManager::Get()
{
    return sInstance;
}

AddonManager::AddonManager()
{
    LoadSettings();
}

AddonManager::~AddonManager()
{
    SaveSettings();
}

std::string AddonManager::GetAddonCacheDirectory()
{
    std::string cacheDir;

#if PLATFORM_WINDOWS
    const char* appData = getenv("APPDATA");
    if (appData != nullptr)
    {
        cacheDir = std::string(appData) + "/PolyphaseEditor/AddonCache";
    }
    else
    {
        const char* userProfile = getenv("USERPROFILE");
        if (userProfile != nullptr)
        {
            cacheDir = std::string(userProfile) + "/AppData/Roaming/PolyphaseEditor/AddonCache";
        }
    }
#else
    const char* home = getenv("HOME");
    if (home != nullptr)
    {
        cacheDir = std::string(home) + "/.config/PolyphaseEditor/AddonCache";
    }
#endif

    if (cacheDir.empty())
    {
        cacheDir = "Engine/Saves/AddonCache";
    }

    return cacheDir;
}

std::string AddonManager::GetSettingsPath()
{
    std::string settingsDir;

#if PLATFORM_WINDOWS
    const char* appData = getenv("APPDATA");
    if (appData != nullptr)
    {
        settingsDir = std::string(appData) + "/PolyphaseEditor";
    }
#else
    const char* home = getenv("HOME");
    if (home != nullptr)
    {
        settingsDir = std::string(home) + "/.config/PolyphaseEditor";
    }
#endif

    if (settingsDir.empty())
    {
        settingsDir = "Engine/Saves";
    }

    return settingsDir + "/addons.json";
}

std::string AddonManager::GetInstalledAddonsPath()
{
    const std::string& projDir = GetEngineState()->mProjectDirectory;
    if (projDir.empty())
    {
        return "";
    }
    return projDir + "Settings/installed_addons.json";
}

void AddonManager::EnsureCacheDirectory()
{
    std::string cacheDir = GetAddonCacheDirectory();

#if PLATFORM_WINDOWS
    const char* appData = getenv("APPDATA");
    if (appData != nullptr)
    {
        std::string polyphaseDir = std::string(appData) + "/PolyphaseEditor";
        if (!DoesDirExist(polyphaseDir.c_str()))
        {
            SYS_CreateDirectory(polyphaseDir.c_str());
        }
    }
#else
    const char* home = getenv("HOME");
    if (home != nullptr)
    {
        std::string configDir = std::string(home) + "/.config";
        if (!DoesDirExist(configDir.c_str()))
        {
            SYS_CreateDirectory(configDir.c_str());
        }
        std::string polyphaseDir = configDir + "/PolyphaseEditor";
        if (!DoesDirExist(polyphaseDir.c_str()))
        {
            SYS_CreateDirectory(polyphaseDir.c_str());
        }
    }
#endif

    if (!DoesDirExist(cacheDir.c_str()))
    {
        SYS_CreateDirectory(cacheDir.c_str());
    }
}

void AddonManager::LoadSettings()
{
    mRepositories.clear();

    std::string settingsPath = GetSettingsPath();
    if (!SYS_DoesFileExist(settingsPath.c_str(), false))
    {
        // Add default repository on first run
        AddonRepository defaultRepo;
        defaultRepo.mName = "Polyphase Engine Official Addons";
        defaultRepo.mUrl = "https://github.com/Polyphase-Labs/Polyphase-Engine---Official-Addons";
        mRepositories.push_back(defaultRepo);
        SaveSettings();
        return;
    }

    Stream stream;
    if (!stream.ReadFile(settingsPath.c_str(), false))
    {
        return;
    }

    std::string jsonStr(stream.GetData(), stream.GetSize());
    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        LogError("Failed to parse addons.json");
        return;
    }

    if (doc.HasMember("repositories") && doc["repositories"].IsArray())
    {
        const rapidjson::Value& repos = doc["repositories"];
        for (rapidjson::SizeType i = 0; i < repos.Size(); ++i)
        {
            const rapidjson::Value& repoObj = repos[i];
            AddonRepository repo;

            if (repoObj.HasMember("name") && repoObj["name"].IsString())
            {
                repo.mName = repoObj["name"].GetString();
            }
            if (repoObj.HasMember("url") && repoObj["url"].IsString())
            {
                repo.mUrl = repoObj["url"].GetString();
            }

            if (!repo.mUrl.empty())
            {
                mRepositories.push_back(repo);
            }
        }
    }

    if (doc.HasMember("autoResolveDependencies") && doc["autoResolveDependencies"].IsBool())
    {
        mAutoResolveDeps = doc["autoResolveDependencies"].GetBool();
    }
}

void AddonManager::SaveSettings()
{
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("version", 1, allocator);

    rapidjson::Value reposArray(rapidjson::kArrayType);
    for (const AddonRepository& repo : mRepositories)
    {
        rapidjson::Value repoObj(rapidjson::kObjectType);
        repoObj.AddMember("name", rapidjson::Value(repo.mName.c_str(), allocator), allocator);
        repoObj.AddMember("url", rapidjson::Value(repo.mUrl.c_str(), allocator), allocator);
        reposArray.PushBack(repoObj, allocator);
    }
    doc.AddMember("repositories", reposArray, allocator);
    doc.AddMember("autoResolveDependencies", mAutoResolveDeps, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    // Ensure directory exists
    std::string settingsPath = GetSettingsPath();
    std::string settingsDir = settingsPath.substr(0, settingsPath.find_last_of("/\\"));
    if (!DoesDirExist(settingsDir.c_str()))
    {
        SYS_CreateDirectory(settingsDir.c_str());
    }

    Stream stream(buffer.GetString(), (uint32_t)buffer.GetSize());
    stream.WriteFile(settingsPath.c_str());
}

void AddonManager::AddRepository(const std::string& url)
{
    // Check if already exists
    for (const AddonRepository& repo : mRepositories)
    {
        if (repo.mUrl == url)
        {
            return;
        }
    }

    AddonRepository newRepo;
    newRepo.mUrl = url;
    newRepo.mName = url;  // Will be updated when fetched

    mRepositories.push_back(newRepo);
    SaveSettings();

    // Refresh to get repo info
    RefreshRepository(url);
}

void AddonManager::RemoveRepository(const std::string& url)
{
    for (auto it = mRepositories.begin(); it != mRepositories.end(); ++it)
    {
        if (it->mUrl == url)
        {
            mRepositories.erase(it);
            SaveSettings();

            // Remove addons from this repo from available list
            for (auto addonIt = mAvailableAddons.begin(); addonIt != mAvailableAddons.end();)
            {
                if (addonIt->mRepoUrl == url)
                {
                    addonIt = mAvailableAddons.erase(addonIt);
                }
                else
                {
                    ++addonIt;
                }
            }
            return;
        }
    }
}

static bool IsGitHubUrl(const std::string& str)
{
    return str.find("github.com/") != std::string::npos;
}

std::string AddonManager::ConvertToRawUrl(const std::string& gitHubUrl, const std::string& filePath, const std::string& branch = "main")
{
    // Convert: https://github.com/user/repo
    // To: https://raw.githubusercontent.com/user/repo/main/filePath

    std::string url = gitHubUrl;

    // Remove trailing slash
    while (!url.empty() && url.back() == '/')
    {
        url.pop_back();
    }

    // Replace github.com with raw.githubusercontent.com
    size_t githubPos = url.find("github.com");
    if (githubPos != std::string::npos)
    {
        url.replace(githubPos, 10, "raw.githubusercontent.com");
    }

    return url + "/"+ branch +"/" + filePath;
}

std::string AddonManager::ConvertToDownloadUrl(const std::string& gitHubUrl,  const std::string& branch = "main")
{
    std::string url = gitHubUrl;

    while (!url.empty() && url.back() == '/')
    {
        url.pop_back();
    }

    return url + "/archive/refs/heads/"+ branch +".zip";
}

bool AddonManager::DownloadFile(const std::string& url, const std::string& destPath, std::string& outError)
{
    std::string output;

#if PLATFORM_WINDOWS
    std::string cmd = "curl -L -s -o \"" + destPath + "\" \"" + url + "\" 2>&1";
    SYS_Exec(cmd.c_str(), &output);

    if (!SYS_DoesFileExist(destPath.c_str(), false))
    {
        cmd = "powershell -Command \"Invoke-WebRequest -Uri '" + url + "' -OutFile '" + destPath + "'\" 2>&1";
        SYS_Exec(cmd.c_str(), &output);
    }
#else
    std::string cmd = "curl -L -s -o \"" + destPath + "\" \"" + url + "\" 2>&1";
    SYS_Exec(cmd.c_str(), &output);
#endif

    if (!SYS_DoesFileExist(destPath.c_str(), false))
    {
        outError = "Failed to download: " + output;
        return false;
    }

    return true;
}

bool AddonManager::ExtractZip(const std::string& zipPath, const std::string& destDir, std::string& outError)
{
    std::string output;

    if (!DoesDirExist(destDir.c_str()))
    {
        SYS_CreateDirectory(destDir.c_str());
    }
    std::string zipPath_ = NormalizePath(zipPath);
    std::string extractPath = NormalizePath(destDir);
#if PLATFORM_WINDOWS
    std::string cmd = "C:\\Windows\\System32\\tar.exe -xf \"" + zipPath_ + "\" -C \"" + extractPath + "\" 2>&1";
    SYS_Exec(cmd.c_str(), &output);
#else
    std::string cmd = "unzip -o \"" + zipPath_ + "\" -d \"" + extractPath + "\" 2>&1";
    SYS_Exec(cmd.c_str(), &output);
#endif

    return true;
}
std::string AddonManager::NormalizePath(const std::string& in)
{
    std::string out = in;
    for (char& c : out)
    {
        if (c == '\\')
            c = '/';
    }
    return out;
}
bool AddonManager::FetchRepositoryManifest(const std::string& url, AddonRepository& outRepo, const std::string& branch = "main")
{
    EnsureCacheDirectory();

    std::string rawUrl = ConvertToRawUrl(url, "package.json", branch);
    std::string tempPath = GetAddonCacheDirectory() + "/_temp_manifest.json";

    std::string error;
    if (!DownloadFile(rawUrl, tempPath, error))
    {
		// test "master" branch if "main" fails
		rawUrl = ConvertToRawUrl(url, "package.json", "master");
        if (!DownloadFile(rawUrl, tempPath, error)) {

            LogWarning("Failed to fetch repository manifest from %s: %s", url.c_str(), error.c_str());
            return false;
        }
    }

    Stream stream;
    if (!stream.ReadFile(tempPath.c_str(), false))
    {
        SYS_RemoveFile(tempPath.c_str());
        return false;
    }

    std::string jsonStr(stream.GetData(), stream.GetSize());
    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    SYS_RemoveFile(tempPath.c_str());

    if (doc.HasParseError())
    {
        LogWarning("Failed to parse repository manifest from %s", url.c_str());
        return false;
    }

    outRepo.mUrl = url;

    if (doc.HasMember("name") && doc["name"].IsString())
    {
        outRepo.mName = doc["name"].GetString();
    }
    else
    {
        outRepo.mName = url;
    }

    outRepo.mAddonIds.clear();
    if (doc.HasMember("addons") && doc["addons"].IsArray())
    {
        const rapidjson::Value& addons = doc["addons"];
        for (rapidjson::SizeType i = 0; i < addons.Size(); ++i)
        {
            if (addons[i].IsString())
            {
                outRepo.mAddonIds.push_back(addons[i].GetString());
            }
        }
    }

    return true;
}

bool AddonManager::FetchAddonMetadata(const std::string& repoUrl, const std::string& addonId, Addon& outAddon, const std::string& branch = "main")
{
    EnsureCacheDirectory();

    std::string rawUrl;
    std::string effectiveId = addonId;

    if (addonId.empty())
    {
        // Standalone repo - package.json is at root
        rawUrl = ConvertToRawUrl(repoUrl, "package.json", branch);
        // Derive ID from URL: https://github.com/user/cool-addon -> cool-addon
        std::string trimmed = repoUrl;
        while (!trimmed.empty() && trimmed.back() == '/') trimmed.pop_back();
        size_t lastSlash = trimmed.find_last_of('/');
        effectiveId = (lastSlash != std::string::npos) ? trimmed.substr(lastSlash + 1) : trimmed;
    }
    else
    {
        rawUrl = ConvertToRawUrl(repoUrl, addonId + "/package.json", branch);
    }

    std::string tempPath = GetAddonCacheDirectory() + "/_temp_addon_meta.json";

    std::string error;
    if (!DownloadFile(rawUrl, tempPath, error))
    {
        LogWarning("Failed to fetch addon metadata for %s: %s", effectiveId.c_str(), error.c_str());
        return false;
    }

    Stream stream;
    if (!stream.ReadFile(tempPath.c_str(), false))
    {
        SYS_RemoveFile(tempPath.c_str());
        return false;
    }

    std::string jsonStr(stream.GetData(), stream.GetSize());
    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    SYS_RemoveFile(tempPath.c_str());

    if (doc.HasParseError())
    {
        LogWarning("Failed to parse addon metadata for %s", effectiveId.c_str());
        return false;
    }

    outAddon.mMetadata.mId = effectiveId;
    outAddon.mRepoUrl = repoUrl;

    if (doc.HasMember("name") && doc["name"].IsString())
    {
        outAddon.mMetadata.mName = doc["name"].GetString();
    }
    else
    {
        outAddon.mMetadata.mName = effectiveId;
    }

    if (doc.HasMember("author") && doc["author"].IsString())
    {
        outAddon.mMetadata.mAuthor = doc["author"].GetString();
    }

    if (doc.HasMember("description") && doc["description"].IsString())
    {
        outAddon.mMetadata.mDescription = doc["description"].GetString();
    }

    if (doc.HasMember("url") && doc["url"].IsString())
    {
        outAddon.mMetadata.mUrl = doc["url"].GetString();
    }

    if (doc.HasMember("version") && doc["version"].IsString())
    {
        outAddon.mMetadata.mVersion = doc["version"].GetString();
    }

    if (doc.HasMember("updated") && doc["updated"].IsString())
    {
        outAddon.mMetadata.mUpdated = doc["updated"].GetString();
    }

    if (doc.HasMember("tags") && doc["tags"].IsArray())
    {
        const rapidjson::Value& tags = doc["tags"];
        for (rapidjson::SizeType i = 0; i < tags.Size(); ++i)
        {
            if (tags[i].IsString())
            {
                outAddon.mMetadata.mTags.push_back(tags[i].GetString());
            }
        }
    }

    // Parse native module metadata
    if (doc.HasMember("native") && doc["native"].IsObject())
    {
        const rapidjson::Value& native = doc["native"];
        outAddon.mNative.mHasNative = true;

        // Parse target: "engine" (default) or "editor"
        if (native.HasMember("target") && native["target"].IsString())
        {
            std::string target = native["target"].GetString();
            if (target == "editor")
            {
                outAddon.mNative.mTarget = NativeAddonTarget::EditorOnly;
            }
            else
            {
                outAddon.mNative.mTarget = NativeAddonTarget::EngineAndEditor;
            }
        }

        if (native.HasMember("sourceDir") && native["sourceDir"].IsString())
        {
            outAddon.mNative.mSourceDir = native["sourceDir"].GetString();
        }

        if (native.HasMember("binaryName") && native["binaryName"].IsString())
        {
            outAddon.mNative.mBinaryName = native["binaryName"].GetString();
        }
        else
        {
            // Default binary name to addon ID (lowercase, no spaces)
            outAddon.mNative.mBinaryName = effectiveId;
        }

        if (native.HasMember("entrySymbol") && native["entrySymbol"].IsString())
        {
            outAddon.mNative.mEntrySymbol = native["entrySymbol"].GetString();
        }

        if (native.HasMember("apiVersion") && native["apiVersion"].IsUint())
        {
            outAddon.mNative.mPluginApiVersion = native["apiVersion"].GetUint();
        }
    }

    // Check if installed
    outAddon.mIsInstalled = IsAddonInstalled(effectiveId);
    if (outAddon.mIsInstalled)
    {
        outAddon.mInstalledVersion = GetInstalledVersion(effectiveId);
    }

    return true;
}

// Map one manifest.json "packages[]" entry onto an Addon record. The registry manifest is
// auto-generated and already inlines every field the browser needs, so no per-addon
// package.json fetch is required. Installed status is (re)stamped by RefreshAllRepositories.
static void ParseAddonObject(const rapidjson::Value& pkg, Addon& out)
{
    ContentMetadata& meta = out.mMetadata;

    if (pkg.HasMember("id") && pkg["id"].IsString())
        meta.mId = pkg["id"].GetString();

    if (pkg.HasMember("name") && pkg["name"].IsString())
        meta.mName = pkg["name"].GetString();
    else
        meta.mName = meta.mId;

    if (pkg.HasMember("description") && pkg["description"].IsString())
        meta.mDescription = pkg["description"].GetString();
    else if (pkg.HasMember("summary") && pkg["summary"].IsString())
        meta.mDescription = pkg["summary"].GetString();

    if (pkg.HasMember("author") && pkg["author"].IsString())
        meta.mAuthor = pkg["author"].GetString();

    if (pkg.HasMember("version") && pkg["version"].IsString())
        meta.mVersion = pkg["version"].GetString();

    if (pkg.HasMember("updatedAt") && pkg["updatedAt"].IsString())
        meta.mUpdated = pkg["updatedAt"].GetString();
    else if (pkg.HasMember("updated") && pkg["updated"].IsString())
        meta.mUpdated = pkg["updated"].GetString();

    if (pkg.HasMember("category") && pkg["category"].IsString())
        meta.mCategory = pkg["category"].GetString();

    if (pkg.HasMember("tags") && pkg["tags"].IsArray())
    {
        const rapidjson::Value& tags = pkg["tags"];
        for (rapidjson::SizeType i = 0; i < tags.Size(); ++i)
        {
            if (tags[i].IsString())
                meta.mTags.push_back(tags[i].GetString());
        }
    }

    // Each manifest package is its own standalone repo.
    out.mIsStandalone = true;
    out.mIsMain = true;
    if (pkg.HasMember("repository") && pkg["repository"].IsObject())
    {
        const rapidjson::Value& repo = pkg["repository"];
        if (repo.HasMember("url") && repo["url"].IsString())
        {
            out.mRepoUrl = repo["url"].GetString();
            meta.mUrl = out.mRepoUrl;
        }
        if (repo.HasMember("branch") && repo["branch"].IsString() && repo["branch"].GetStringLength() > 0)
        {
            out.mBranch = repo["branch"].GetString();
            out.mIsMain = (out.mBranch == "main");
        }
    }

    // Cross-addon dependencies: { "id": "^1.0.0" | "<url>[#ref]" | "" }.
    if (pkg.HasMember("dependencies") && pkg["dependencies"].IsObject())
    {
        const rapidjson::Value& deps = pkg["dependencies"];
        for (auto it = deps.MemberBegin(); it != deps.MemberEnd(); ++it)
        {
            if (!it->name.IsString()) continue;
            std::string id = it->name.GetString();
            std::string value = it->value.IsString() ? it->value.GetString() : std::string();
            meta.mDependencies.push_back(AddonDependencySpec::FromValue(id, value));
        }
    }

    // Native "engine" block. A null/absent target means the package is non-native
    // (a project or pure-asset addon).
    if (pkg.HasMember("engine") && pkg["engine"].IsObject())
    {
        const rapidjson::Value& engine = pkg["engine"];

        if (engine.HasMember("target") && engine["target"].IsString())
        {
            out.mNative.mHasNative = true;
            std::string target = engine["target"].GetString();
            out.mNative.mTarget = (target == "editor")
                ? NativeAddonTarget::EditorOnly
                : NativeAddonTarget::EngineAndEditor;
        }

        if (engine.HasMember("apiVersion") && engine["apiVersion"].IsUint())
            out.mNative.mPluginApiVersion = engine["apiVersion"].GetUint();

        if (engine.HasMember("entrySymbol") && engine["entrySymbol"].IsString())
            out.mNative.mEntrySymbol = engine["entrySymbol"].GetString();

        if (engine.HasMember("binaryName") && engine["binaryName"].IsString())
            out.mNative.mBinaryName = engine["binaryName"].GetString();
        else
            out.mNative.mBinaryName = meta.mId;

        if (engine.HasMember("resolveMode") && engine["resolveMode"].IsString())
        {
            std::string mode = engine["resolveMode"].GetString();
            out.mNative.mResolveMode = (mode == "binary")
                ? NativeAddonResolveMode::Binary
                : NativeAddonResolveMode::Source;
        }
    }

    // Declarative build targets (metadata only; the addon registers them at load time).
    if (pkg.HasMember("buildTargets") && pkg["buildTargets"].IsArray())
    {
        const rapidjson::Value& bt = pkg["buildTargets"];
        for (rapidjson::SizeType i = 0; i < bt.Size(); ++i)
        {
            if (!bt[i].IsObject()) continue;
            const rapidjson::Value& entry = bt[i];
            NativeModuleMetadata::BuildTargetMetadata m;
            if (entry.HasMember("id") && entry["id"].IsString())
                m.mId = entry["id"].GetString();
            if (entry.HasMember("displayName") && entry["displayName"].IsString())
                m.mDisplayName = entry["displayName"].GetString();
            if (entry.HasMember("category") && entry["category"].IsString())
                m.mCategory = entry["category"].GetString();
            if (!m.mId.empty())
                out.mNative.mBuildTargets.push_back(std::move(m));
        }
    }
}

bool AddonManager::FetchManifest(const std::string& url, const std::string& branch, bool& outFound)
{
    outFound = false;
    EnsureCacheDirectory();

    std::string rawUrl = ConvertToRawUrl(url, "manifest.json", branch);
    std::string tempPath = GetAddonCacheDirectory() + "/_temp_registry_manifest.json";

    std::string error;
    if (!DownloadFile(rawUrl, tempPath, error))
    {
        return false;
    }

    Stream stream;
    if (!stream.ReadFile(tempPath.c_str(), false))
    {
        SYS_RemoveFile(tempPath.c_str());
        return false;
    }

    std::string jsonStr(stream.GetData(), stream.GetSize());
    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    SYS_RemoveFile(tempPath.c_str());

    // A file always comes back (curl writes GitHub's 404 body too), so only treat this as a
    // real manifest when it parses into an object carrying a "packages" array. Otherwise let
    // the caller fall back to the legacy package.json crawl.
    if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("packages") || !doc["packages"].IsArray())
    {
        return false;
    }

    outFound = true;

    std::string repoName;
    if (doc.HasMember("name") && doc["name"].IsString())
        repoName = doc["name"].GetString();

    // Top-level categories dictionary (id/name/description).
    if (doc.HasMember("categories") && doc["categories"].IsArray())
    {
        const rapidjson::Value& cats = doc["categories"];
        for (rapidjson::SizeType i = 0; i < cats.Size(); ++i)
        {
            if (!cats[i].IsObject()) continue;
            const rapidjson::Value& c = cats[i];
            AddonCategory cat;
            if (c.HasMember("id") && c["id"].IsString())                   cat.mId = c["id"].GetString();
            if (c.HasMember("name") && c["name"].IsString())               cat.mName = c["name"].GetString();
            if (c.HasMember("description") && c["description"].IsString())  cat.mDescription = c["description"].GetString();
            if (cat.mId.empty()) continue;

            bool exists = false;
            for (const AddonCategory& existing : mCategories)
            {
                if (existing.mId == cat.mId) { exists = true; break; }
            }
            if (!exists)
                mCategories.push_back(std::move(cat));
        }
    }

    std::vector<std::string> repoAddonIds;

    const rapidjson::Value& packages = doc["packages"];
    for (rapidjson::SizeType i = 0; i < packages.Size(); ++i)
    {
        if (!packages[i].IsObject()) continue;
        const rapidjson::Value& pkg = packages[i];

        // Hide non-published packages from the browser.
        if (pkg.HasMember("status") && pkg["status"].IsString() &&
            std::string(pkg["status"].GetString()) != "published")
        {
            continue;
        }

        Addon addon;
        ParseAddonObject(pkg, addon);
        if (addon.mMetadata.mId.empty())
            continue;

        repoAddonIds.push_back(addon.mMetadata.mId);

        // Dedupe by id (an addon may be listed by more than one configured repo).
        bool exists = false;
        for (const Addon& existing : mAvailableAddons)
        {
            if (existing.mMetadata.mId == addon.mMetadata.mId) { exists = true; break; }
        }
        if (!exists)
            mAvailableAddons.push_back(std::move(addon));
    }

    // Update the configured repository entry: name + id list feed the Repositories tab.
    for (AddonRepository& repo : mRepositories)
    {
        if (repo.mUrl == url)
        {
            if (!repoName.empty())
                repo.mName = repoName;
            repo.mAddonIds = repoAddonIds;
            break;
        }
    }

    return true;
}

void AddonManager::RefreshAllRepositories()
{
    mAvailableAddons.clear();
    mCategories.clear();

    for (AddonRepository& repo : mRepositories)
    {
        RefreshRepository(repo.mUrl);
    }
    mRepositoriesRefreshed = true;

    // Update installed status
    LoadInstalledAddons();
    for (Addon& addon : mAvailableAddons)
    {
        addon.mIsInstalled = IsAddonInstalled(addon.mMetadata.mId);
        if (addon.mIsInstalled)
        {
            addon.mInstalledVersion = GetInstalledVersion(addon.mMetadata.mId);
        }
    }
}

void AddonManager::RefreshRepository(const std::string& url)
{
    if (EditorProgress::IsActive())
    {
        std::string label = "Fetching addon registry " + url + "...";
        EditorProgress::SetStatus(label.c_str());
    }

    // Preferred path: one auto-generated manifest.json carrying every package's full
    // metadata. Try main, then master.
    bool found = false;
    if (FetchManifest(url, "main", found) || (!found && FetchManifest(url, "master", found)))
    {
        SaveSettings();
        return;
    }

    // Legacy fallback: repos without a manifest.json still expose a root package.json with an
    // "addons" index that we crawl one package.json at a time.
    AddonRepository repoInfo;
    if (!FetchRepositoryManifest(url, repoInfo, "main"))
    {
        if (!FetchRepositoryManifest(url, repoInfo, "master"))
        {
            return;
        }

    }
    
  
    // Update repository in list
    for (AddonRepository& repo : mRepositories)
    {
        if (repo.mUrl == url)
        {
            repo.mName = repoInfo.mName;
            repo.mAddonIds = repoInfo.mAddonIds;
            break;
        }
    }

    // Fetch each addon's metadata
    for (const std::string& addonId : repoInfo.mAddonIds)
    {
        Addon addon;
        bool fetched = false;

        if (IsGitHubUrl(addonId))
        {
            // Standalone addon from external repo
            fetched = FetchAddonMetadata(addonId, "", addon, "main");
            if (fetched)
            {
                addon.mIsStandalone = true;
            }
            else {
                fetched = FetchAddonMetadata(addonId, "", addon, "master");
                if (fetched)
                {
                    addon.mIsStandalone = true;
                    addon.mIsMain = false;
                    addon.mBranch = "master";
                }
            }
        }
        else
        {
            // Local subdirectory addon
            fetched = FetchAddonMetadata(url, addonId, addon);
        }

        if (fetched)
        {
            // Check if already in list (avoid duplicates from multiple repos)
            bool exists = false;
            for (const Addon& existing : mAvailableAddons)
            {
                if (existing.mMetadata.mId == addon.mMetadata.mId)
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
            {
                mAvailableAddons.push_back(addon);
            }
        }
    }

    SaveSettings();
}

bool AddonManager::DownloadAddon(const Addon& addon, std::string& outError)
{
    EnsureCacheDirectory();

    if (EditorProgress::IsActive())
    {
        std::string label = "Downloading " + addon.mMetadata.mName + "...";
        EditorProgress::SetStatus(label.c_str());
    }

    // Download the full repo and extract just this addon folder
    const std::string branch = GetAddonBranch(addon);
    std::string downloadUrl = ConvertToDownloadUrl(addon.mRepoUrl, branch);
	// extract repoName from URL for logging
    std::string repoName = addon.mRepoUrl.substr(addon.mRepoUrl.find_last_of('/') + 1) + "-" + branch;
    std::string zipPath = GetAddonCacheDirectory() + "/_temp_repo.zip";
    std::string extractDir = GetAddonCacheDirectory() + "/_temp_extract";

    // Resolve the branch head before fetching the archive so the recorded
    // commit can never be newer than the files we actually got. Best effort:
    // an empty commit just means "not tracked" in the update status.
    AddonInstallSource source;
    source.mRepoUrl = addon.mRepoUrl;
    source.mBranch = branch;
    source.mIsStandalone = addon.mIsStandalone;
    {
        std::string commitError;
        if (!FetchLatestCommit(addon.mRepoUrl, branch, addon.mIsStandalone ? std::string() : addon.mMetadata.mId,
                               source.mCommit, source.mCommitDate, commitError))
        {
            LogWarning("Addon '%s': could not record install commit: %s",
                       addon.mMetadata.mId.c_str(), commitError.c_str());
        }
    }

    if (!DownloadFile(downloadUrl, zipPath, outError))
    {
        return false;
    }

    if (DoesDirExist(extractDir.c_str()))
    {
        SYS_RemoveDirectory(extractDir.c_str());
    }

    if (EditorProgress::IsActive())
    {
        std::string label = "Extracting " + addon.mMetadata.mName + "...";
        EditorProgress::SetStatus(label.c_str());
    }

    if (!ExtractZip(zipPath, extractDir, outError))
    {
        SYS_RemoveFile(zipPath.c_str());
        return false;
    }

    extractDir = extractDir + "/" + repoName;
    //SYS_RemoveFile(zipPath.c_str());

    // Find the extracted folder (GitHub zips have repo-name-branch structure)
    DirEntry dirEntry;
    //SYS_OpenDirectory(extractDir, dirEntry);
    std::string extractedRepoFolder = extractDir;
    
    //SYS_CloseDirectory(dirEntry);

    if (extractedRepoFolder.empty())
    {
        outError = "Could not find extracted repository folder";
        SYS_RemoveDirectory(extractDir.c_str());
        return false;
    }

    // Find the addon folder
    std::string addonPath;
    if (addon.mIsStandalone)
    {
        // Standalone: the entire extracted repo IS the addon
        addonPath = extractedRepoFolder;
    }
    else
    {
        // Subdirectory: find addon folder within repo
        addonPath = extractedRepoFolder + "/" + addon.mMetadata.mId;
        if (!DoesDirExist(addonPath.c_str()))
        {
            outError = "Addon folder not found in repository: " + addon.mMetadata.mId;
            SYS_RemoveDirectory(extractDir.c_str());
            return false;
        }
    }

    // Move addon to cache
    std::string cachedAddonPath = GetAddonCacheDirectory() + "/" + addon.mMetadata.mId;
    if (DoesDirExist(cachedAddonPath.c_str()))
    {
        //SYS_RemoveDirectory(cachedAddonPath.c_str());
    }
    SYS_CreateDirectory(cachedAddonPath.c_str());

    SYS_CopyDirectoryRecursive(addonPath.c_str(), cachedAddonPath.c_str());
    //SYS_MoveDirectory(addonPath.c_str(), cachedAddonPath.c_str());

    // Clean up
    //SYS_RemoveDirectory(extractDir.c_str());

    // Install to project
    if (EditorProgress::IsActive())
    {
        std::string label = "Installing " + addon.mMetadata.mName + "...";
        EditorProgress::SetStatus(label.c_str());
    }
    return InstallAddon(cachedAddonPath, addon.mMetadata.mId, outError, &source);
}

static bool IsHexSha(const std::string& ref)
{
    if (ref.size() != 40)
    {
        return false;
    }
    for (char c : ref)
    {
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex)
        {
            return false;
        }
    }
    return true;
}

bool AddonManager::DownloadAndInstallFromUrl(const std::string& addonId,
                                             const std::string& url,
                                             const std::string& ref,
                                             std::string& outError)
{
    EnsureCacheDirectory();

    // Decide the actual zip URL to fetch.
    bool isDirectZip = false;
    {
        std::string lower = url;
        for (char& c : lower) c = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
        isDirectZip = (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".zip") == 0);
    }

    std::string zipUrl;
    std::string branchTried;
    if (isDirectZip)
    {
        zipUrl = url;
    }
    else
    {
        branchTried = ref.empty() ? "main" : ref;
        zipUrl = ConvertToDownloadUrl(url, branchTried);
    }

    std::string zipPath = GetAddonCacheDirectory() + "/_dep_" + addonId + ".zip";
    std::string extractDir = GetAddonCacheDirectory() + "/_dep_extract_" + addonId;

    if (DoesDirExist(extractDir.c_str()))
    {
        SYS_RemoveDirectory(extractDir.c_str());
    }

    if (!DownloadFile(zipUrl, zipPath, outError))
    {
        // For GitHub, try "master" if "main" failed.
        if (!isDirectZip && ref.empty())
        {
            zipUrl = ConvertToDownloadUrl(url, "master");
            if (!DownloadFile(zipUrl, zipPath, outError))
            {
                return false;
            }
            branchTried = "master";
        }
        else
        {
            return false;
        }
    }

    if (!ExtractZip(zipPath, extractDir, outError))
    {
        SYS_RemoveFile(zipPath.c_str());
        return false;
    }

    // GitHub zips wrap contents in <repo>-<branch>/. Walk the extract dir to find
    // the folder that contains a package.json (one level deep is enough for both
    // GitHub-style and flat zips).
    std::string addonRoot;
    if (SYS_DoesFileExist((extractDir + "/package.json").c_str(), false))
    {
        addonRoot = extractDir;
    }
    else
    {
        DirEntry dirEntry;
        SYS_OpenDirectory(extractDir, dirEntry);
        while (dirEntry.mValid)
        {
            if (dirEntry.mDirectory &&
                strcmp(dirEntry.mFilename, ".") != 0 &&
                strcmp(dirEntry.mFilename, "..") != 0)
            {
                std::string candidate = extractDir + "/" + dirEntry.mFilename;
                if (SYS_DoesFileExist((candidate + "/package.json").c_str(), false))
                {
                    addonRoot = candidate;
                    break;
                }
            }
            SYS_IterateDirectory(dirEntry);
        }
        SYS_CloseDirectory(dirEntry);
    }

    if (addonRoot.empty())
    {
        outError = "Extracted dependency archive '" + addonId + "' has no package.json";
        return false;
    }

    // Stage into AddonCache then call the standard install path.
    std::string cachedAddonPath = GetAddonCacheDirectory() + "/" + addonId;
    if (DoesDirExist(cachedAddonPath.c_str()))
    {
        SYS_RemoveDirectory(cachedAddonPath.c_str());
    }
    SYS_CreateDirectory(cachedAddonPath.c_str());
    SYS_CopyDirectoryRecursive(addonRoot.c_str(), cachedAddonPath.c_str());

    // Remember where this came from so the Addons window can re-download and
    // update-check a dependency that is not in any configured registry.
    AddonInstallSource source;
    source.mRepoUrl = url;
    source.mBranch = branchTried;
    source.mIsStandalone = true;
    source.mPinned = IsHexSha(ref);
    if (!isDirectZip && !source.mPinned)
    {
        std::string commitError;
        if (!FetchLatestCommit(url, branchTried, std::string(), source.mCommit, source.mCommitDate, commitError))
        {
            LogWarning("Addon '%s': could not record install commit: %s", addonId.c_str(), commitError.c_str());
        }
    }
    else if (source.mPinned)
    {
        source.mCommit = ref;
    }

    return InstallAddon(cachedAddonPath, addonId, outError, &source);
}

std::string AddonManager::GetCurrentTimestamp()
{
    time_t now = time(nullptr);
    struct tm* timeinfo = gmtime(&now);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    return std::string(buffer);
}

bool AddonManager::MergeAddonIntoProject(const std::string& addonPath, std::string& outError)
{
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    if (projectDir.empty())
    {
        outError = "No project loaded";
        return false;
    }

    // Get list of files in addon
    std::function<void(const std::string&, const std::string&)> copyRecursive;
    copyRecursive = [&](const std::string& srcDir, const std::string& relPath)
    {
        DirEntry dirEntry;
        SYS_OpenDirectory(srcDir, dirEntry);

        while (dirEntry.mValid)
        {
            std::string filename = dirEntry.mFilename;

            if (filename != "." && filename != "..")
            {
                std::string srcPath = srcDir + "/" + filename;
                std::string relativePath = relPath.empty() ? filename : (relPath + "/" + filename);

                // Skip project files
                if (filename.find(".octp") != std::string::npos ||
                    filename.find(".ini") != std::string::npos ||
                    filename == "package.json" ||
                    filename == "thumbnail.png")
                {
                    SYS_IterateDirectory(dirEntry);
                    continue;
                }

                std::string destPath = projectDir + relativePath;

                if (dirEntry.mDirectory)
                {
                    // Create directory if needed
                    if (!DoesDirExist(destPath.c_str()))
                    {
                        SYS_CreateDirectory(destPath.c_str());
                    }
                    // Recurse
                    copyRecursive(srcPath, relativePath);
                }
                else
                {
                    // Skip if destination already exists (don't overwrite)
                    if (!SYS_DoesFileExist(destPath.c_str(), false))
                    {
                        // Ensure parent directory exists
                        size_t lastSlash = destPath.find_last_of("/\\");
                        if (lastSlash != std::string::npos)
                        {
                            std::string parentDir = destPath.substr(0, lastSlash);
                            if (!DoesDirExist(parentDir.c_str()))
                            {
                                SYS_CreateDirectory(parentDir.c_str());
                            }
                        }

                        SYS_CopyFile(srcPath.c_str(), destPath.c_str());
                    }
                }
            }

            SYS_IterateDirectory(dirEntry);
        }
        SYS_CloseDirectory(dirEntry);
    };

    copyRecursive(addonPath, "");

    return true;
}

bool AddonManager::InstallAddon(const std::string& addonCachePath, const std::string& addonId, std::string& outError,
                                const AddonInstallSource* source)
{
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    if (projectDir.empty())
    {
        outError = "No project loaded";
        return false;
    }

    // Install to Packages/{addonId}/
    std::string packagesDir = projectDir + "Packages";
    if (!DoesDirExist(packagesDir.c_str()))
    {
        SYS_CreateDirectory(packagesDir.c_str());
    }

    std::string destDir = packagesDir + "/" + addonId;
    if (DoesDirExist(destDir.c_str()))
    {
        SYS_RemoveDirectory(destDir.c_str());
    }
    SYS_CreateDirectory(destDir.c_str());
    SYS_CopyDirectoryRecursive(addonCachePath.c_str(), destDir.c_str());

    // Record installation. On an update, start from the previous record so
    // user settings (enabled, native mode, sync info, script trust) survive.
    const Addon* addon = FindAddon(addonId);
    InstalledAddon installed;
    for (auto it = mInstalledAddons.begin(); it != mInstalledAddons.end(); ++it)
    {
        if (it->mId == addonId)
        {
            installed = *it;
            mInstalledAddons.erase(it);
            break;
        }
    }

    installed.mId = addonId;
    std::string packageVersion = AddonDependencyResolver::ReadPackageVersion(destDir);
    if (addon != nullptr && !addon->mMetadata.mVersion.empty())
    {
        installed.mVersion = addon->mMetadata.mVersion;
    }
    else
    {
        installed.mVersion = packageVersion.empty() ? "1.0.0" : packageVersion;
    }
    installed.mInstalledDate = GetCurrentTimestamp();

    if (addon != nullptr)
    {
        installed.mRepoUrl = addon->mRepoUrl;
        installed.mBranch = GetAddonBranch(*addon);
        installed.mIsStandalone = addon->mIsStandalone;
    }
    installed.mCommit.clear();
    installed.mCommitDate.clear();
    installed.mPinned = false;
    if (source != nullptr)
    {
        if (!source->mRepoUrl.empty()) installed.mRepoUrl = source->mRepoUrl;
        if (!source->mBranch.empty())  installed.mBranch = source->mBranch;
        installed.mCommit = source->mCommit;
        installed.mCommitDate = source->mCommitDate;
        installed.mPinned = source->mPinned;
        installed.mIsStandalone = source->mIsStandalone;
    }

    // The files on disk just changed; any earlier check result is stale.
    installed.mUpdateChecked = false;
    installed.mRemoteCommit.clear();
    installed.mRemoteCommitDate.clear();
    installed.mUpdateCheckError.clear();

    mInstalledAddons.push_back(installed);
    SaveInstalledAddons();

    // Update available addons list
    for (Addon& availAddon : mAvailableAddons)
    {
        if (availAddon.mMetadata.mId == addonId)
        {
            availAddon.mIsInstalled = true;
            availAddon.mInstalledVersion = installed.mVersion;
            break;
        }
    }

    // Recursively resolve declared dependencies before running onInstall — the
    // script may assume its deps are present on disk.
    if (mAutoResolveDeps)
    {
        std::vector<std::string> resolveOrder;
        std::string resolveErr;
        AddonDependencyResolver::Resolve(addonId, resolveOrder, resolveErr);
        if (!resolveErr.empty())
        {
            LogWarning("Addon '%s': dependency resolution reported: %s",
                       addonId.c_str(), resolveErr.c_str());
        }
    }

    // Record which declared dependencies are still absent so the Addons
    // window can say so instead of reporting a clean install.
    mLastInstallMissingDeps.clear();
    {
        std::vector<AddonDependencySpec> declaredDeps;
        std::string ignoredOnInstall;
        AddonDependencyResolver::ReadDependenciesFromDisk(destDir, declaredDeps, ignoredOnInstall);
        for (const AddonDependencySpec& dep : declaredDeps)
        {
            std::string depJson = packagesDir + "/" + dep.mId + "/package.json";
            if (!SYS_DoesFileExist(depJson.c_str(), false))
            {
                mLastInstallMissingDeps.push_back(dep.mId);
            }
        }
    }

    // Read declared onInstall path from the just-installed package.json (works
    // for native and non-native addons alike).
    std::vector<AddonDependencySpec> ignoredDeps;
    std::string onInstall;
    AddonDependencyResolver::ReadDependenciesFromDisk(destDir, ignoredDeps, onInstall);

    if (!onInstall.empty())
    {
        bool trustedNow = AddonScriptRunner::IsTrustedAddonId(addonId);
        if (!trustedNow)
        {
            // Honor any sticky "trust this addon" choice from a prior install.
            for (const InstalledAddon& rec : mInstalledAddons)
            {
                if (rec.mId == addonId && rec.mTrustedScripts)
                {
                    trustedNow = true;
                    break;
                }
            }
        }

        AddonScriptRunner::TrustResult choice = AddonScriptRunner::TrustResult::TrustOnce;
        if (!trustedNow)
        {
            const Addon* a = FindAddon(addonId);
            std::string source = a ? a->mRepoUrl : std::string();
            choice = AddonScriptRunner::ShowTrustModal(addonId, source, onInstall);
        }

        if (choice == AddonScriptRunner::TrustResult::CancelInstall)
        {
            // Revert install.
            if (DoesDirExist(destDir.c_str()))
            {
                SYS_RemoveDirectory(destDir.c_str());
            }
            for (auto it = mInstalledAddons.begin(); it != mInstalledAddons.end(); ++it)
            {
                if (it->mId == addonId)
                {
                    mInstalledAddons.erase(it);
                    break;
                }
            }
            SaveInstalledAddons();
            outError = "Install cancelled by user (declined onInstall script).";
            return false;
        }
        else if (choice == AddonScriptRunner::TrustResult::Skip)
        {
            LogDebug("Addon '%s': onInstall script skipped by user.", addonId.c_str());
        }
        else
        {
            if (choice == AddonScriptRunner::TrustResult::TrustAlways)
            {
                for (InstalledAddon& rec : mInstalledAddons)
                {
                    if (rec.mId == addonId)
                    {
                        rec.mTrustedScripts = true;
                        break;
                    }
                }
                SaveInstalledAddons();
            }
            std::string scriptErr;
            if (!AddonScriptRunner::RunOnInstall(addonId, destDir, onInstall, scriptErr))
            {
                LogWarning("Addon '%s': onInstall failed: %s",
                           addonId.c_str(), scriptErr.c_str());
            }
        }
    }

    return true;
}

bool AddonManager::UninstallAddon(const std::string& addonId)
{
    for (auto it = mInstalledAddons.begin(); it != mInstalledAddons.end(); ++it)
    {
        if (it->mId == addonId)
        {
            // Remove addon files from Packages/
            const std::string& projectDir = GetEngineState()->mProjectDirectory;
            if (!projectDir.empty())
            {
                std::string addonDir = projectDir + "Packages/" + addonId;
                if (DoesDirExist(addonDir.c_str()))
                {
                    SYS_RemoveDirectory(addonDir.c_str());
                }
                if (DoesDirExist(addonDir.c_str()))
                {
                    LogWarning("Uninstall '%s': could not fully remove %s (a file is in use).",
                               addonId.c_str(), addonDir.c_str());
                }

                // Build output for native addons. The shadow copy the DLL was
                // loaded from is released by UnloadNativeAddon; a lingering
                // .pdb lock here is not fatal, the dir is just left behind.
                std::string intermediateDir = projectDir + "Intermediate/Plugins/" + addonId;
                if (DoesDirExist(intermediateDir.c_str()))
                {
                    SYS_RemoveDirectory(intermediateDir.c_str());
                    if (DoesDirExist(intermediateDir.c_str()))
                    {
                        LogWarning("Uninstall '%s': could not remove build output %s (a file is in use).",
                                   addonId.c_str(), intermediateDir.c_str());
                    }
                }
            }

            mInstalledAddons.erase(it);
            SaveInstalledAddons();

            // Update available addons list
            for (Addon& addon : mAvailableAddons)
            {
                if (addon.mMetadata.mId == addonId)
                {
                    addon.mIsInstalled = false;
                    addon.mInstalledVersion.clear();
                    break;
                }
            }

            return true;
        }
    }

    return false;
}

void AddonManager::LoadInstalledAddons()
{
    // The ledger is re-read on every Addons window open and registry refresh.
    // Keep the session-only update-check results across those reloads so a
    // "Check for Updates" result survives closing and reopening the window.
    std::unordered_map<std::string, InstalledAddon> previous;
    for (const InstalledAddon& inst : mInstalledAddons)
    {
        previous[inst.mId] = inst;
    }

    mInstalledAddons.clear();

    std::string installedPath = GetInstalledAddonsPath();
    if (installedPath.empty() || !SYS_DoesFileExist(installedPath.c_str(), false))
    {
        return;
    }

    Stream stream;
    if (!stream.ReadFile(installedPath.c_str(), false))
    {
        return;
    }

    std::string jsonStr(stream.GetData(), stream.GetSize());
    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        return;
    }

    if (doc.HasMember("addons") && doc["addons"].IsArray())
    {
        const rapidjson::Value& addons = doc["addons"];
        for (rapidjson::SizeType i = 0; i < addons.Size(); ++i)
        {
            const rapidjson::Value& addonObj = addons[i];
            InstalledAddon installed;

            if (addonObj.HasMember("id") && addonObj["id"].IsString())
            {
                installed.mId = addonObj["id"].GetString();
            }
            if (addonObj.HasMember("version") && addonObj["version"].IsString())
            {
                installed.mVersion = addonObj["version"].GetString();
            }
            if (addonObj.HasMember("installed") && addonObj["installed"].IsString())
            {
                installed.mInstalledDate = addonObj["installed"].GetString();
            }
            if (addonObj.HasMember("repoUrl") && addonObj["repoUrl"].IsString())
            {
                installed.mRepoUrl = addonObj["repoUrl"].GetString();
            }
            if (addonObj.HasMember("enabled") && addonObj["enabled"].IsBool())
            {
                installed.mEnabled = addonObj["enabled"].GetBool();
            }
            if (addonObj.HasMember("enableNative") && addonObj["enableNative"].IsBool())
            {
                installed.mEnableNative = addonObj["enableNative"].GetBool();
            }
            if (addonObj.HasMember("nativeMode") && addonObj["nativeMode"].IsString())
            {
                const std::string mode = addonObj["nativeMode"].GetString();
                installed.mNativeMode = (mode == "binary") ?
                    NativeAddonResolveMode::Binary : NativeAddonResolveMode::Source;
            }
            if (addonObj.HasMember("lastSyncAt") && addonObj["lastSyncAt"].IsString())
            {
                installed.mLastSyncAt = addonObj["lastSyncAt"].GetString();
            }
            if (addonObj.HasMember("lastSyncSource") && addonObj["lastSyncSource"].IsString())
            {
                installed.mLastSyncSource = addonObj["lastSyncSource"].GetString();
            }
            if (addonObj.HasMember("lastSyncStatus") && addonObj["lastSyncStatus"].IsString())
            {
                installed.mLastSyncStatus = addonObj["lastSyncStatus"].GetString();
            }
            if (addonObj.HasMember("trustedScripts") && addonObj["trustedScripts"].IsBool())
            {
                installed.mTrustedScripts = addonObj["trustedScripts"].GetBool();
            }
            if (addonObj.HasMember("branch") && addonObj["branch"].IsString())
            {
                installed.mBranch = addonObj["branch"].GetString();
            }
            if (addonObj.HasMember("commit") && addonObj["commit"].IsString())
            {
                installed.mCommit = addonObj["commit"].GetString();
            }
            if (addonObj.HasMember("commitDate") && addonObj["commitDate"].IsString())
            {
                installed.mCommitDate = addonObj["commitDate"].GetString();
            }
            if (addonObj.HasMember("pinned") && addonObj["pinned"].IsBool())
            {
                installed.mPinned = addonObj["pinned"].GetBool();
            }
            if (addonObj.HasMember("standalone") && addonObj["standalone"].IsBool())
            {
                installed.mIsStandalone = addonObj["standalone"].GetBool();
            }

            if (!installed.mId.empty())
            {
                auto prev = previous.find(installed.mId);
                if (prev != previous.end() && prev->second.mCommit == installed.mCommit)
                {
                    installed.mUpdateChecked     = prev->second.mUpdateChecked;
                    installed.mRemoteCommit      = prev->second.mRemoteCommit;
                    installed.mRemoteCommitDate  = prev->second.mRemoteCommitDate;
                    installed.mUpdateCheckError  = prev->second.mUpdateCheckError;
                }
                mInstalledAddons.push_back(installed);
            }
        }
    }
}

void AddonManager::SaveInstalledAddons()
{
    std::string installedPath = GetInstalledAddonsPath();
    if (installedPath.empty())
    {
        return;
    }

    // Ensure Settings directory exists
    std::string settingsDir = GetEngineState()->mProjectDirectory + "Settings";
    if (!DoesDirExist(settingsDir.c_str()))
    {
        SYS_CreateDirectory(settingsDir.c_str());
    }

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("version", 1, allocator);

    rapidjson::Value addonsArray(rapidjson::kArrayType);
    for (const InstalledAddon& installed : mInstalledAddons)
    {
        rapidjson::Value addonObj(rapidjson::kObjectType);
        addonObj.AddMember("id", rapidjson::Value(installed.mId.c_str(), allocator), allocator);
        addonObj.AddMember("version", rapidjson::Value(installed.mVersion.c_str(), allocator), allocator);
        addonObj.AddMember("installed", rapidjson::Value(installed.mInstalledDate.c_str(), allocator), allocator);
        addonObj.AddMember("repoUrl", rapidjson::Value(installed.mRepoUrl.c_str(), allocator), allocator);
        addonObj.AddMember("enabled", installed.mEnabled, allocator);
        addonObj.AddMember("enableNative", installed.mEnableNative, allocator);
        const char* modeStr = (installed.mNativeMode == NativeAddonResolveMode::Binary) ? "binary" : "source";
        addonObj.AddMember("nativeMode", rapidjson::Value(modeStr, allocator), allocator);
        if (!installed.mLastSyncAt.empty())
        {
            addonObj.AddMember("lastSyncAt", rapidjson::Value(installed.mLastSyncAt.c_str(), allocator), allocator);
        }
        if (!installed.mLastSyncSource.empty())
        {
            addonObj.AddMember("lastSyncSource", rapidjson::Value(installed.mLastSyncSource.c_str(), allocator), allocator);
        }
        if (!installed.mLastSyncStatus.empty())
        {
            addonObj.AddMember("lastSyncStatus", rapidjson::Value(installed.mLastSyncStatus.c_str(), allocator), allocator);
        }
        if (!installed.mBranch.empty())
        {
            addonObj.AddMember("branch", rapidjson::Value(installed.mBranch.c_str(), allocator), allocator);
        }
        if (!installed.mCommit.empty())
        {
            addonObj.AddMember("commit", rapidjson::Value(installed.mCommit.c_str(), allocator), allocator);
        }
        if (!installed.mCommitDate.empty())
        {
            addonObj.AddMember("commitDate", rapidjson::Value(installed.mCommitDate.c_str(), allocator), allocator);
        }
        if (installed.mPinned)
        {
            addonObj.AddMember("pinned", true, allocator);
        }
        if (!installed.mIsStandalone)
        {
            addonObj.AddMember("standalone", false, allocator);
        }
        if (installed.mTrustedScripts)
        {
            addonObj.AddMember("trustedScripts", installed.mTrustedScripts, allocator);
        }
        addonsArray.PushBack(addonObj, allocator);
    }
    doc.AddMember("addons", addonsArray, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    Stream stream(buffer.GetString(), (uint32_t)buffer.GetSize());
    stream.WriteFile(installedPath.c_str());
}

bool AddonManager::IsAddonInstalled(const std::string& addonId) const
{
    for (const InstalledAddon& installed : mInstalledAddons)
    {
        if (installed.mId == addonId)
        {
            return true;
        }
    }
    return false;
}

const InstalledAddon* AddonManager::FindInstalled(const std::string& addonId) const
{
    for (const InstalledAddon& installed : mInstalledAddons)
    {
        if (installed.mId == addonId)
        {
            return &installed;
        }
    }
    return nullptr;
}

bool AddonManager::HasUpdate(const std::string& addonId) const
{
    AddonUpdateStatus::Kind kind = GetUpdateStatus(addonId).mKind;
    return kind == AddonUpdateStatus::NewCommits || kind == AddonUpdateStatus::NewVersion;
}

static std::string ShortSha(const std::string& sha)
{
    return sha.size() > 7 ? sha.substr(0, 7) : sha;
}

// True when both are "YYYY-MM-DDTHH:MM:SSZ"-style UTC stamps (GitHub and
// GetCurrentTimestamp both emit that form) and a is strictly later than b.
static bool IsIsoNewer(const std::string& a, const std::string& b)
{
    auto looksIso = [](const std::string& s)
    {
        return s.size() >= 19 && s[4] == '-' && s[7] == '-' && s[10] == 'T';
    };
    if (!looksIso(a) || !looksIso(b))
    {
        return false;
    }
    return a.compare(0, 19, b, 0, 19) > 0;
}

AddonUpdateStatus AddonManager::GetUpdateStatus(const std::string& addonId) const
{
    AddonUpdateStatus status;

    const InstalledAddon* inst = FindInstalled(addonId);
    if (inst == nullptr)
    {
        return status;
    }
    const Addon* addon = FindAddon(addonId);

    std::string repoUrl = inst->mRepoUrl;
    if (repoUrl.empty() && addon != nullptr)
    {
        repoUrl = addon->mRepoUrl;
    }
    std::string owner, repo;
    if (repoUrl.empty())
    {
        status.mKind = AddonUpdateStatus::NoSource;
        status.mDetail = "No repository recorded for this addon. Reinstall it from a registry to enable update checks.";
        return status;
    }
    if (!ParseGitHubRepo(repoUrl, owner, repo))
    {
        status.mKind = AddonUpdateStatus::NoSource;
        status.mDetail = "Update checks need a GitHub repository URL (source: " + repoUrl + ").";
        return status;
    }
    if (inst->mPinned)
    {
        status.mKind = AddonUpdateStatus::Pinned;
        status.mDetail = "Pinned to commit " + ShortSha(inst->mCommit) + " by the dependency that requested it.";
        return status;
    }

    // Secondary signal: the registry publishes a different version string.
    if (addon != nullptr && !inst->mVersion.empty() && !addon->mMetadata.mVersion.empty() &&
        inst->mVersion != addon->mMetadata.mVersion)
    {
        status.mKind = AddonUpdateStatus::NewVersion;
        status.mDetail = "Registry publishes v" + addon->mMetadata.mVersion + ", installed is v" + inst->mVersion + ".";
        return status;
    }
    // The registry manifest stamps each package with its last update; a stamp
    // later than our install means the published files moved even if the
    // version string did not. Works without any GitHub API call.
    if (addon != nullptr && IsIsoNewer(addon->mMetadata.mUpdated, inst->mInstalledDate))
    {
        status.mKind = AddonUpdateStatus::NewVersion;
        status.mDetail = "Registry entry updated " + addon->mMetadata.mUpdated +
                         ", after this install (" + inst->mInstalledDate + ").";
        return status;
    }

    if (!inst->mUpdateChecked)
    {
        status.mKind = AddonUpdateStatus::NotChecked;
        status.mDetail = inst->mCommit.empty()
            ? "No commit recorded (installed before commit tracking). Update once to start tracking."
            : "Installed commit " + ShortSha(inst->mCommit) + ". Click Check for Updates.";
        return status;
    }
    if (!inst->mUpdateCheckError.empty() || inst->mRemoteCommit.empty())
    {
        status.mKind = AddonUpdateStatus::Error;
        status.mDetail = inst->mUpdateCheckError.empty() ? "No commit returned for the branch." : inst->mUpdateCheckError;
        return status;
    }
    if (inst->mCommit.empty())
    {
        // Installed before commit tracking: fall back to dates. A branch head
        // committed after the install time is an update; otherwise nothing
        // has landed since.
        if (IsIsoNewer(inst->mRemoteCommitDate, inst->mInstalledDate))
        {
            status.mKind = AddonUpdateStatus::NewCommits;
            status.mDetail = "Commit " + ShortSha(inst->mRemoteCommit) + " (" + inst->mRemoteCommitDate +
                             ") landed after this install (" + inst->mInstalledDate +
                             "). No install commit was recorded; updating starts commit tracking.";
            return status;
        }
        if (!inst->mRemoteCommitDate.empty() && !inst->mInstalledDate.empty())
        {
            status.mKind = AddonUpdateStatus::UpToDate;
            status.mDetail = "No commits on the branch since this install (" + inst->mInstalledDate +
                             "). Install commit not recorded; updating starts commit tracking.";
            return status;
        }
        status.mKind = AddonUpdateStatus::NotChecked;
        status.mDetail = "Branch head is " + ShortSha(inst->mRemoteCommit) +
                         " but no install commit or date was recorded. Update once to start tracking.";
        return status;
    }

    // Primary signal: the branch head moved since install.
    if (inst->mRemoteCommit != inst->mCommit)
    {
        status.mKind = AddonUpdateStatus::NewCommits;
        status.mDetail = "New commits on '" + (inst->mBranch.empty() ? std::string("main") : inst->mBranch) + "': " +
                         ShortSha(inst->mCommit) + " -> " + ShortSha(inst->mRemoteCommit);
        if (!inst->mRemoteCommitDate.empty())
        {
            status.mDetail += " (" + inst->mRemoteCommitDate + ")";
        }
        return status;
    }

    status.mKind = AddonUpdateStatus::UpToDate;
    status.mDetail = "Installed commit " + ShortSha(inst->mCommit) + " matches the branch head.";
    return status;
}

std::string AddonManager::GetAddonBranch(const Addon& addon)
{
    if (!addon.mBranch.empty())
    {
        return addon.mBranch;
    }
    return addon.mIsMain ? "main" : "master";
}

bool AddonManager::ParseGitHubRepo(const std::string& url, std::string& outOwner, std::string& outRepo)
{
    outOwner.clear();
    outRepo.clear();

    size_t pos = url.find("github.com/");
    if (pos == std::string::npos)
    {
        return false;
    }

    std::string rest = url.substr(pos + strlen("github.com/"));
    while (!rest.empty() && rest.back() == '/')
    {
        rest.pop_back();
    }
    if (rest.size() > 4 && rest.compare(rest.size() - 4, 4, ".git") == 0)
    {
        rest.erase(rest.size() - 4);
    }

    size_t slash = rest.find('/');
    if (slash == std::string::npos)
    {
        return false;
    }
    outOwner = rest.substr(0, slash);
    outRepo = rest.substr(slash + 1);
    size_t extra = outRepo.find('/');
    if (extra != std::string::npos)
    {
        outRepo.erase(extra);
    }
    return !outOwner.empty() && !outRepo.empty();
}

bool AddonManager::FetchLatestCommit(const std::string& repoUrl, const std::string& branch,
                                     const std::string& subPath, std::string& outSha,
                                     std::string& outDate, std::string& outError)
{
    outSha.clear();
    outDate.clear();
    outError.clear();

    std::string owner, repo;
    if (!ParseGitHubRepo(repoUrl, owner, repo))
    {
        outError = "Update checks are only supported for GitHub repositories (" + repoUrl + ").";
        return false;
    }
    if (!HttpClient::IsAvailable())
    {
        outError = HttpClient::GetMissingDependencyMessage();
        if (outError.empty())
        {
            outError = "HTTP client unavailable.";
        }
        return false;
    }

    // One commit, newest first, optionally restricted to the addon's folder
    // so a legacy multi-addon registry repo doesn't flag every sibling.
    std::string url = "https://api.github.com/repos/" + owner + "/" + repo +
                      "/commits?sha=" + branch + "&per_page=1";
    if (!subPath.empty())
    {
        url += "&path=" + subPath;
    }

    UpdaterHttpResponse response = HttpClient::Get(url, 10000);

    rapidjson::Document doc;
    doc.Parse(response.mBody.c_str());

    if (!response.IsSuccess())
    {
        outError = "GitHub API HTTP " + std::to_string(response.mStatusCode);
        if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("message") && doc["message"].IsString())
        {
            outError += ": " + std::string(doc["message"].GetString());
        }
        else if (!response.mError.empty())
        {
            outError += ": " + response.mError;
        }
        return false;
    }
    if (doc.HasParseError() || !doc.IsArray())
    {
        outError = "Unexpected response from the GitHub API.";
        return false;
    }
    if (doc.Empty())
    {
        outError = "No commits found on branch '" + branch + "'" +
                   (subPath.empty() ? std::string() : " for path '" + subPath + "'") + ".";
        return false;
    }

    const rapidjson::Value& commit = doc[0];
    if (commit.IsObject() && commit.HasMember("sha") && commit["sha"].IsString())
    {
        outSha = commit["sha"].GetString();
    }
    if (commit.IsObject() && commit.HasMember("commit") && commit["commit"].IsObject())
    {
        const rapidjson::Value& inner = commit["commit"];
        if (inner.HasMember("committer") && inner["committer"].IsObject())
        {
            const rapidjson::Value& committer = inner["committer"];
            if (committer.HasMember("date") && committer["date"].IsString())
            {
                outDate = committer["date"].GetString();
            }
        }
    }

    if (outSha.empty())
    {
        outError = "GitHub API response carried no commit sha.";
        return false;
    }
    return true;
}

int AddonManager::CheckForUpdates(std::string& outSummary)
{
    // Fresh registry first: the version signal comes from there, and this
    // also re-reads the ledger (dropping any previous transient results).
    if (EditorProgress::IsActive())
    {
        EditorProgress::SetStatus("Refreshing addon repositories...");
    }
    RefreshAllRepositories();

    struct LookupResult
    {
        bool mOk = false;
        std::string mSha;
        std::string mDate;
        std::string mError;
    };
    std::unordered_map<std::string, LookupResult> memo;

    int available = 0;
    int checked = 0;
    int failed = 0;
    int skipped = 0;
    const int total = (int)mInstalledAddons.size();

    for (int i = 0; i < total; ++i)
    {
        InstalledAddon& inst = mInstalledAddons[i];
        inst.mUpdateChecked = false;
        inst.mRemoteCommit.clear();
        inst.mRemoteCommitDate.clear();
        inst.mUpdateCheckError.clear();

        const Addon* addon = FindAddon(inst.mId);
        std::string repoUrl = inst.mRepoUrl;
        std::string branch = inst.mBranch;
        bool standalone = inst.mIsStandalone;
        if (addon != nullptr)
        {
            // Registry metadata wins for records written before branch /
            // layout were tracked.
            if (repoUrl.empty()) repoUrl = addon->mRepoUrl;
            if (branch.empty())  branch = GetAddonBranch(*addon);
            if (inst.mBranch.empty()) standalone = addon->mIsStandalone;
        }
        if (branch.empty())
        {
            branch = "main";
        }

        std::string owner, repo;
        if (inst.mPinned || repoUrl.empty() || !ParseGitHubRepo(repoUrl, owner, repo))
        {
            skipped++;
            continue;
        }

        if (EditorProgress::IsActive())
        {
            std::string label = "Checking " + inst.mId + "...";
            EditorProgress::Step(label.c_str(), i, total);
        }

        std::string subPath = standalone ? std::string() : inst.mId;
        std::string key = owner + "/" + repo + "|" + branch + "|" + subPath;
        auto found = memo.find(key);
        if (found == memo.end())
        {
            LookupResult result;
            result.mOk = FetchLatestCommit(repoUrl, branch, subPath, result.mSha, result.mDate, result.mError);
            found = memo.emplace(key, result).first;
        }

        inst.mUpdateChecked = true;
        if (found->second.mOk)
        {
            inst.mRemoteCommit = found->second.mSha;
            inst.mRemoteCommitDate = found->second.mDate;
            checked++;
        }
        else
        {
            inst.mUpdateCheckError = found->second.mError;
            failed++;
            LogWarning("Update check for addon '%s' failed: %s", inst.mId.c_str(), found->second.mError.c_str());
        }

        if (HasUpdate(inst.mId))
        {
            available++;
        }
    }

    outSummary = std::to_string(checked) + " checked";
    if (available > 0) outSummary += ", " + std::to_string(available) + " with updates";
    if (failed > 0)    outSummary += ", " + std::to_string(failed) + " failed";
    if (skipped > 0)   outSummary += ", " + std::to_string(skipped) + " skipped (pinned or no GitHub source)";
    outSummary += ".";
    return available;
}

std::string AddonManager::GetInstalledVersion(const std::string& addonId) const
{
    for (const InstalledAddon& installed : mInstalledAddons)
    {
        if (installed.mId == addonId)
        {
            return installed.mVersion;
        }
    }
    return "";
}

const Addon* AddonManager::FindAddon(const std::string& addonId) const
{
    for (const Addon& addon : mAvailableAddons)
    {
        if (addon.mMetadata.mId == addonId)
        {
            return &addon;
        }
    }
    return nullptr;
}

bool AddonManager::SetInstalledAddonNativeMode(const std::string& addonId, NativeAddonResolveMode mode)
{
    for (InstalledAddon& installed : mInstalledAddons)
    {
        if (installed.mId == addonId)
        {
            installed.mNativeMode = mode;
            SaveInstalledAddons();
            return true;
        }
    }
    return false;
}

bool AddonManager::SyncNativeAddonBinary(const std::string& addonId, std::string& outError)
{
    const Addon* addon = FindAddon(addonId);
    if (addon == nullptr)
    {
        outError = "Addon not found: " + addonId;
        return false;
    }

    if (!addon->mNative.mHasNative)
    {
        outError = "Addon has no native code";
        return false;
    }

    if (addon->mNative.mBinaries.empty())
    {
        outError = "Addon has no remote binary sources configured";
        return false;
    }

    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    if (projectDir.empty())
    {
        outError = "No project loaded";
        return false;
    }

    // Find compatible binary descriptor for current platform/arch
    std::string currentPlatform;
    std::string currentArch;
#if PLATFORM_WINDOWS
    currentPlatform = "Windows";
    currentArch = "x64";
#elif PLATFORM_LINUX
    currentPlatform = "Linux";
    currentArch = "x64";
#elif PLATFORM_MAC
    currentPlatform = "Mac";
    currentArch = "arm64";
#else
    outError = "Binary mode not supported on this platform";
    return false;
#endif

    const NativeBinaryDescriptor* matchedDescriptor = nullptr;
    for (const NativeBinaryDescriptor& desc : addon->mNative.mBinaries)
    {
        if (desc.mPlatform == currentPlatform && desc.mArch == currentArch)
        {
            matchedDescriptor = &desc;
            break;
        }
    }

    if (matchedDescriptor == nullptr)
    {
        outError = "No compatible binary available for " + currentPlatform + "/" + currentArch;
        return false;
    }

    // Prepare cache directory
    std::string cacheDir = projectDir + "Intermediate/Plugins/" + addonId + "/Synced/";
    if (!DoesDirExist(cacheDir.c_str()))
    {
        SYS_CreateDirectory(cacheDir.c_str());
    }

    std::string downloadUrl;
    std::string destFilename;

    if (matchedDescriptor->mType == "url")
    {
        downloadUrl = matchedDescriptor->mValue;
        size_t lastSlash = downloadUrl.find_last_of('/');
        destFilename = (lastSlash != std::string::npos) ? downloadUrl.substr(lastSlash + 1) : "binary.dll";
    }
    else if (matchedDescriptor->mType == "releaseAsset")
    {
        if (addon->mRepoUrl.empty())
        {
            outError = "Addon has no repository URL for release asset fetch";
            return false;
        }
        // GitHub release asset URL pattern: https://github.com/{owner}/{repo}/releases/latest/download/{asset}
        std::string repoUrl = addon->mRepoUrl;
        while (!repoUrl.empty() && repoUrl.back() == '/') repoUrl.pop_back();
        downloadUrl = repoUrl + "/releases/latest/download/" + matchedDescriptor->mValue;
        destFilename = matchedDescriptor->mValue;
    }
    else if (matchedDescriptor->mType == "zip")
    {
        downloadUrl = matchedDescriptor->mValue;
        size_t lastSlash = downloadUrl.find_last_of('/');
        destFilename = (lastSlash != std::string::npos) ? downloadUrl.substr(lastSlash + 1) : "binary.zip";
    }
    else
    {
        outError = "Unknown binary descriptor type: " + matchedDescriptor->mType;
        return false;
    }

    std::string destPath = cacheDir + destFilename;

    // Download the binary
    if (!DownloadFile(downloadUrl, destPath, outError))
    {
        // Update sync status to failed
        for (InstalledAddon& installed : mInstalledAddons)
        {
            if (installed.mId == addonId)
            {
                installed.mLastSyncAt = GetCurrentTimestamp();
                installed.mLastSyncSource = downloadUrl;
                installed.mLastSyncStatus = "Failed: " + outError;
                break;
            }
        }
        SaveInstalledAddons();
        return false;
    }

    // Handle ZIP extraction if needed
    if (matchedDescriptor->mType == "zip")
    {
        std::string extractDir = cacheDir + "extracted/";
        if (!ExtractZip(destPath, extractDir, outError))
        {
            outError = "Failed to extract ZIP: " + outError;
            return false;
        }

        // Move the entry file if specified
        if (!matchedDescriptor->mEntryPath.empty())
        {
            std::string entryFile = extractDir + matchedDescriptor->mEntryPath;
            size_t lastSlash = matchedDescriptor->mEntryPath.find_last_of("/\\");
            std::string entryFilename = (lastSlash != std::string::npos) ?
                matchedDescriptor->mEntryPath.substr(lastSlash + 1) : matchedDescriptor->mEntryPath;
            std::string finalPath = cacheDir + entryFilename;
            SYS_CopyFile(entryFile.c_str(), finalPath.c_str());
        }
    }

    // Update sync metadata
    for (InstalledAddon& installed : mInstalledAddons)
    {
        if (installed.mId == addonId)
        {
            installed.mLastSyncAt = GetCurrentTimestamp();
            installed.mLastSyncSource = downloadUrl;
            installed.mLastSyncStatus = "Success";
            break;
        }
    }
    SaveInstalledAddons();

    return true;
}

#endif
