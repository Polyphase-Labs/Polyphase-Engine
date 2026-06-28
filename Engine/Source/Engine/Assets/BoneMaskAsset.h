#pragma once

#include "Asset.h"
#include "AssetRef.h"
#include "Factory.h"

#include <string>
#include <vector>

class SkeletalMesh;

// Per-bone bitset addressed by name-based subtree includes/excludes.
// Resolved lazily against any SkeletalMesh — masks are name-keyed at save
// time so the same .bmask can drive any rig that names its bones the same
// way. Resolution caches the bitset against the last queried mesh; rebuilds
// happen on Resolve() when the mesh pointer changes or on explicit
// InvalidateCache() (e.g. when the owning component swaps its mesh).
class POLYPHASE_API BoneMaskAsset : public Asset
{
public:

    DECLARE_ASSET(BoneMaskAsset, Asset);

    BoneMaskAsset();
    ~BoneMaskAsset();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;

    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    SkeletalMesh* GetTargetMesh() const;
    void SetTargetMesh(SkeletalMesh* mesh);

    const std::vector<std::string>& GetIncludeRoots() const { return mIncludeRoots; }
    const std::vector<std::string>& GetExcludeRoots() const { return mExcludeRoots; }
    bool GetSelfOnly() const { return mSelfOnly; }

    void SetIncludeRoots(const std::vector<std::string>& roots);
    void SetExcludeRoots(const std::vector<std::string>& roots);
    void SetSelfOnly(bool selfOnly);

    void AddIncludeRoot(const std::string& boneName);
    void RemoveIncludeRoot(const std::string& boneName);
    void AddExcludeRoot(const std::string& boneName);
    void RemoveExcludeRoot(const std::string& boneName);

    // Resolve into a per-bone bitset against `mesh`. Empty rig => empty bitset.
    // The bitset stays alive on the cache; the returned reference is valid
    // until the next Resolve() / InvalidateCache() call.
    const std::vector<uint8_t>& Resolve(const SkeletalMesh* mesh);

    void InvalidateCache();

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

protected:

    SkeletalMeshRef          mTargetMesh;
    std::vector<std::string> mIncludeRoots;
    std::vector<std::string> mExcludeRoots;
    bool                     mSelfOnly = false;

    struct ResolvedCache
    {
        const SkeletalMesh*  mMesh = nullptr;
        std::vector<uint8_t> mBitset;
    };
    ResolvedCache mCache;
};
