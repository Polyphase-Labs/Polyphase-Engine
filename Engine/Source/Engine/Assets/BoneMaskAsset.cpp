#include "Assets/BoneMaskAsset.h"
#include "Assets/SkeletalMesh.h"
#include "Stream.h"
#include "Log.h"

FORCE_LINK_DEF(BoneMaskAsset);
DEFINE_ASSET(BoneMaskAsset);

bool BoneMaskAsset::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    // Let the framework commit the value, then invalidate our cached bitset
    // so the next Resolve() against any mesh rebuilds with the new spec.
    Property* prop = static_cast<Property*>(datum);
    BoneMaskAsset* self = static_cast<BoneMaskAsset*>(prop->mOwner);
    if (self != nullptr)
    {
        self->InvalidateCache();
    }
    return HandleAssetPropChange(datum, index, newValue);
}

BoneMaskAsset::BoneMaskAsset()
{
    mType = BoneMaskAsset::GetStaticType();
}

BoneMaskAsset::~BoneMaskAsset()
{
}

void BoneMaskAsset::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    if (stream.GetAssetVersion() >= ASSET_VERSION_BONE_MASK)
    {
        stream.ReadAsset(mTargetMesh);

        uint32_t includeCount = stream.ReadUint32();
        mIncludeRoots.assign(includeCount, std::string());
        for (uint32_t i = 0; i < includeCount; ++i)
        {
            stream.ReadString(mIncludeRoots[i]);
        }

        uint32_t excludeCount = stream.ReadUint32();
        mExcludeRoots.assign(excludeCount, std::string());
        for (uint32_t i = 0; i < excludeCount; ++i)
        {
            stream.ReadString(mExcludeRoots[i]);
        }

        mSelfOnly = stream.ReadBool();
    }

    InvalidateCache();
}

void BoneMaskAsset::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteAsset(mTargetMesh);

    stream.WriteUint32((uint32_t)mIncludeRoots.size());
    for (const std::string& name : mIncludeRoots)
    {
        stream.WriteString(name);
    }

    stream.WriteUint32((uint32_t)mExcludeRoots.size());
    for (const std::string& name : mExcludeRoots)
    {
        stream.WriteString(name);
    }

    stream.WriteBool(mSelfOnly);
}

void BoneMaskAsset::Create()
{
    Asset::Create();
}

void BoneMaskAsset::Destroy()
{
    mTargetMesh = nullptr;
    mIncludeRoots.clear();
    mExcludeRoots.clear();
    InvalidateCache();
    Asset::Destroy();
}

void BoneMaskAsset::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    outProps.push_back(Property(DatumType::Asset, "Target Mesh", this, &mTargetMesh, 1, HandlePropChange,
                                int32_t(SkeletalMesh::GetStaticType())));
    outProps.push_back(Property(DatumType::String, "Include Roots", this, &mIncludeRoots, 1, HandlePropChange).MakeVector());
    outProps.push_back(Property(DatumType::String, "Exclude Roots", this, &mExcludeRoots, 1, HandlePropChange).MakeVector());
    outProps.push_back(Property(DatumType::Bool, "Self Only", this, &mSelfOnly, 1, HandlePropChange));
}

glm::vec4 BoneMaskAsset::GetTypeColor()
{
    return glm::vec4(0.30f, 0.65f, 0.45f, 1.0f);
}

const char* BoneMaskAsset::GetTypeName()
{
    return "BoneMaskAsset";
}

SkeletalMesh* BoneMaskAsset::GetTargetMesh() const
{
    return mTargetMesh.Get<SkeletalMesh>();
}

void BoneMaskAsset::SetTargetMesh(SkeletalMesh* mesh)
{
    mTargetMesh = mesh;
    InvalidateCache();
}

void BoneMaskAsset::SetIncludeRoots(const std::vector<std::string>& roots)
{
    mIncludeRoots = roots;
    InvalidateCache();
}

void BoneMaskAsset::SetExcludeRoots(const std::vector<std::string>& roots)
{
    mExcludeRoots = roots;
    InvalidateCache();
}

void BoneMaskAsset::SetSelfOnly(bool selfOnly)
{
    mSelfOnly = selfOnly;
    InvalidateCache();
}

void BoneMaskAsset::AddIncludeRoot(const std::string& boneName)
{
    for (const std::string& existing : mIncludeRoots)
    {
        if (existing == boneName) return;
    }
    mIncludeRoots.push_back(boneName);
    InvalidateCache();
}

void BoneMaskAsset::RemoveIncludeRoot(const std::string& boneName)
{
    for (auto it = mIncludeRoots.begin(); it != mIncludeRoots.end(); ++it)
    {
        if (*it == boneName)
        {
            mIncludeRoots.erase(it);
            InvalidateCache();
            return;
        }
    }
}

void BoneMaskAsset::AddExcludeRoot(const std::string& boneName)
{
    for (const std::string& existing : mExcludeRoots)
    {
        if (existing == boneName) return;
    }
    mExcludeRoots.push_back(boneName);
    InvalidateCache();
}

void BoneMaskAsset::RemoveExcludeRoot(const std::string& boneName)
{
    for (auto it = mExcludeRoots.begin(); it != mExcludeRoots.end(); ++it)
    {
        if (*it == boneName)
        {
            mExcludeRoots.erase(it);
            InvalidateCache();
            return;
        }
    }
}

const std::vector<uint8_t>& BoneMaskAsset::Resolve(const SkeletalMesh* mesh)
{
    static const std::vector<uint8_t> kEmpty;

    if (mesh == nullptr)
    {
        mCache.mMesh = nullptr;
        mCache.mBitset.clear();
        return kEmpty;
    }

    if (mCache.mMesh == mesh && mCache.mBitset.size() == mesh->GetNumBones())
    {
        return mCache.mBitset;
    }

    mesh->GatherSubtreeBoneSet(mIncludeRoots, mExcludeRoots, mSelfOnly, mCache.mBitset);
    mCache.mMesh = mesh;
    return mCache.mBitset;
}

void BoneMaskAsset::InvalidateCache()
{
    mCache.mMesh = nullptr;
    mCache.mBitset.clear();
}
