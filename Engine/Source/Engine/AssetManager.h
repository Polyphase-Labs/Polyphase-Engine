#pragma once

#include "Asset.h"
#include "AssetRef.h"
#include "Log.h"

#include "System/System.h"

#include <string>
#include <deque>
#include <unordered_map>

class Asset;
class AssetDir;
class Material;
class ParticleSystem;

struct AsyncLoadRequest
{
    std::string mName;
    std::string mPath;
    uint64_t mUuid = 0;  // For UUID-based loading
    std::vector<AssetRef*> mTargetRefs;
    std::vector<AssetStub*> mDependentAssets;
    const EmbeddedFile* mEmbeddedData = nullptr;
    TypeId mType = INVALID_TYPE_ID;
    Asset* mAsset = nullptr;
    int32_t mRequeueCount = 0;
};

#if EDITOR
// Discovered non-.oct file (e.g. .mp4, .json, .png) tracked for packaging.
// Raw files are invisible to the runtime asset system; this registry only
// exists to drive packaging-time copy / embed decisions.
//
// mAbsolutePath follows the same convention as AssetStub::mPath:
//   - engine asset:  "Engine/Assets/<...>"   (relative to CWD; engine source dir is co-located)
//   - project asset: absolute path under projectDir, e.g. "M:/.../MyProject/Assets/<...>"
//   - addon asset:   absolute path under projectDir/Packages, e.g. "M:/.../MyProject/Packages/<addon>/Assets/<...>"
// The packaging code at ActionManager.cpp derives the destination by mirroring
// saveDir's projectDir-prefix-strip rule: routing engine/project/addon paths
// follows the same logic as cooked .oct files.
struct RawAssetEntry
{
    std::string mAbsolutePath;
    bool        mEngineAsset = false;
    uint32_t    mPlatformMask = PlatformBit_All;
    bool        mEmbed        = false;
};

// Result of reading an {asset}.meta sidecar. mExists is false when no sidecar
// is present on disk; in that case the other fields hold their defaults.
struct AssetMetaSidecar
{
    uint32_t mPlatformMask = PlatformBit_All;
    bool     mEmbed        = false;
    bool     mExists       = false;
};
#endif

// Name-based lookup (backward compatible)
POLYPHASE_API Asset* FetchAsset(const std::string& name);
POLYPHASE_API Asset* LoadAsset(const std::string& name);
POLYPHASE_API void UnloadAsset(const std::string& name);
POLYPHASE_API void AsyncLoadAsset(const std::string& name, AssetRef* targetRef = nullptr);
POLYPHASE_API AssetStub* FetchAssetStub(const std::string& name);

// UUID-based lookup
POLYPHASE_API Asset* FetchAssetByUuid(uint64_t uuid);
POLYPHASE_API Asset* LoadAssetByUuid(uint64_t uuid);
POLYPHASE_API void AsyncLoadAssetByUuid(uint64_t uuid, AssetRef* targetRef = nullptr);
POLYPHASE_API AssetStub* FetchAssetStubByUuid(uint64_t uuid);

#if EDITOR
// Editor-only: addon-provided source-extension dispatch. Addons call this from
// their plugin OnLoad to teach ActionManager::ImportAsset which Asset subclass
// owns a given source-file extension (e.g. ".mp4" -> VideoClip). The extension
// must include the leading dot and be lowercase; comparisons in the dispatcher
// are case-sensitive to match the existing built-in branches.
POLYPHASE_API void RegisterImportExtension(const std::string& ext, TypeId type);
POLYPHASE_API TypeId LookupImportExtension(const std::string& ext);
#endif

template<typename T>
T* FetchAsset(const std::string& name)
{
    Asset* asset = FetchAsset(name);

    if (asset != nullptr &&
        asset->GetType() != T::GetStaticType())
    {
        LogError("Type mismatch in FetchAsset<T>()");
        asset = nullptr;
    }

    return (T*)asset;
}

template<typename T>
T* LoadAsset(const std::string& name)
{
    Asset* asset = LoadAsset(name);

    if (asset != nullptr &&
        !asset->Is(T::ClassRuntimeId()))
    {
        LogError("Type mismatch in LoadAsset<T>()");
        asset = nullptr;
    }

    return (T*)asset;
}

// Per-method POLYPHASE_API on the addon-facing surface only. A class-level
// export expands to dllexport on every member (incl. implicit special-member
// instantiations in TUs that only forward-decl AssetManager members), which
// risks C4150 against forward-declared types. Addons reach into AssetManager
// through Get() and RegisterTransientAsset() only.
class AssetManager
{
public:

    ~AssetManager();

    static void Create();
    static void Destroy();
    POLYPHASE_API static AssetManager* Get();

    void Initialize();
    void Update(float deltaTime);
    void DiscoverDirectory(AssetDir* directory, bool engineDir);
    void RefreshDirectory(AssetDir* directory);
    void Discover(const char* directoryName, const char* directoryPath);
    void DiscoverAssetRegistry(const char* registryPath);
    void DiscoverEmbeddedAssets(struct EmbeddedFile* assets, uint32_t numAssets);
    void Purge(bool purgeEngineAssets);
    bool PurgeAsset(const char* name);
    void RefSweep();
    void LoadAll();

    POLYPHASE_API void RegisterTransientAsset(Asset* asset);

    Asset* ImportEngineAsset(TypeId assetType, AssetDir* dir, const std::string& filename, ImportOptions* options = nullptr);
    void ImportEngineAssets();

    // Name-based lookup (backward compatible)
    AssetStub* GetAssetStub(const std::string& name);
    Asset* GetAsset(const std::string& name);
    AssetStub* GetSceneAsset(const std::string& name);
    Asset* LoadAsset(const std::string& name);
    Asset* LoadAsset(AssetStub& stub);
    void AsyncLoadAsset(const std::string& name, AssetRef* targetRef);

    // UUID-based lookup (primary)
    AssetStub* GetAssetStubByUuid(uint64_t uuid);
    Asset* GetAssetByUuid(uint64_t uuid);
    Asset* LoadAssetByUuid(uint64_t uuid);
    void AsyncLoadAssetByUuid(uint64_t uuid, AssetRef* targetRef);

    // Path-based lookup (e.g., "Assets/Models/SM_Plane" or "Models/SM_Plane")
    AssetStub* GetAssetStubByPath(const std::string& path);
    Asset* LoadAssetByPath(const std::string& path);
    void SaveAsset(const std::string& name);
    void SaveAsset(AssetStub& stub);
    bool UnloadAsset(const std::string& name);
    bool UnloadAsset(AssetStub& stub);

    bool DoesAssetExist(const std::string& name);
    bool RenameAsset(Asset* asset, const std::string& newName);
    std::string GetParentDirectory(const std::string& path);
    bool RenameDirectory(AssetDir* dir, const std::string& newName);
    void GatherScriptFilesRecursive(const std::string& dirPath, const std::string& relativePath, std::vector<std::string>& scriptFiles);
    std::vector<std::string> GetAvailableScriptFiles();
    std::vector<std::string> GetAvailableFontFiles();
    AssetDir* FindProjectDirectory();
    AssetDir* FindProjectRootDirectory();
    AssetDir* FindEngineDirectory();
    AssetDir* FindPackagesDirectory();
    AssetDir* GetRootDirectory();
    void DiscoverAddonPackages(const std::string& packagesDir);
    std::string GetPolyphaseDirectory();
    void GatherScriptFiles(const std::string &dir, std::vector<std::string> &outFiles);
    void GatherFontFiles(const std::string& dir, std::vector<std::string>& outFiles);
    AssetStub* FindDefaultScene();
    std::string FindDefaultScenePath();
    void UnloadProjectDirectory();
    std::unordered_map<std::string, AssetStub*>& GetAssetMap();
    std::vector<AssetStub*> GatherDirtyAssets();

    AssetStub* RegisterAsset(const std::string& filename, TypeId type, AssetDir* directory, EmbeddedFile* embeddedAsset, bool engineAsset, uint64_t uuid = 0);
    AssetStub* CreateAndRegisterAsset(TypeId assetType, AssetDir* directory, const std::string& filename, bool engineAsset);
    AssetDir* GetAssetDirFromPath(const std::string& dirPath);

    bool IsPurging() const;

protected:

    static ThreadFuncRet AsyncLoadThreadFunc(void* in);

    static AssetManager* sInstance;
    AssetManager();

    void UpdateEndLoadQueue();

    std::unordered_map<std::string, AssetStub*> mAssetMap;      // Name-based lookup (first wins)
    std::unordered_map<std::string, AssetStub*> mAssetPathMap;  // Path-based lookup (e.g., "Models/SM_Plane")
    std::unordered_map<uint64_t, AssetStub*> mUuidMap;          // UUID-based lookup (primary)
    std::vector<Asset*> mTransientAssets;
    AssetDir* mRootDirectory = nullptr;
    bool mPurging = false;
    bool mDestructing = false;
    std::deque<AsyncLoadRequest*> mBeginLoadQueue;
    std::deque<AsyncLoadRequest*> mEndLoadQueue;
    ThreadObject* mAsyncLoadThread = {};
    MutexObject* mMutex = {};

#if EDITOR
public:
    glm::vec4 GetEditorAssetColor(TypeId type);
    void InitAssetColorMap();

    // Raw (non-.oct) asset packaging registry. Populated during DiscoverDirectory.
    const std::vector<RawAssetEntry>& GetRawAssetEntries() const { return mRawAssetEntries; }

    // Addon-provided source-extension dispatch (see free-function declarations
    // above for usage). Stored on the manager so the table survives addon hot-
    // reload as long as the addon re-registers in its OnLoad.
    void RegisterImportExtension(const std::string& ext, TypeId type);
    TypeId LookupImportExtension(const std::string& ext) const;

    // Read the {asset}.meta sidecar at <assetPath>.meta. Returns defaults with
    // mExists=false when the sidecar is missing or unparseable.
    static AssetMetaSidecar LoadAssetMeta(const std::string& assetPath);

    // Write the {asset}.meta sidecar. If platformMask == PlatformBit_All AND
    // embed == false, the sidecar is deleted from disk instead of written so
    // the source tree stays clean for assets at all-defaults.
    static void SaveAssetMeta(const std::string& assetPath, uint32_t platformMask, bool embed);

    // Persist new packaging flags to disk via SaveAssetMeta AND update in-memory
    // AssetStub::mPlatformMask/mEmbed (if assetPath matches a known stub) AND
    // RawAssetEntry::mPlatformMask/mEmbed (if assetPath matches a raw entry).
    // Use this from the editor inspector so subsequent packaging sees the new
    // flags without needing a full rediscover.
    void ApplyAssetMetaFlags(const std::string& assetPath, uint32_t platformMask, bool embed);

protected:
    std::unordered_map<TypeId, glm::vec4> mAssetColorMap;
    std::vector<RawAssetEntry> mRawAssetEntries;
    std::unordered_map<std::string, TypeId> mImportExtensionMap;
#endif
};

template<typename T>
T* NewTransientAsset()
{
    T* ret = new T();
    AssetManager::Get()->RegisterTransientAsset(ret);
    return ret;

    // Caller still needs to call Create() when ready!
}
