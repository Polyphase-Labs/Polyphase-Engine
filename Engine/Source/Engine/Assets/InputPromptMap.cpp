#include "InputPromptMap.h"
#include "Stream.h"
#include "Property.h"

FORCE_LINK_DEF(InputPromptMap);
DEFINE_ASSET(InputPromptMap);

static std::string MakeIndexKey(Platform p, GamepadType g, const std::string& path)
{
    return std::to_string((int)p) + "|" + std::to_string((int)g) + "|" + path;
}

InputPromptMap::InputPromptMap()
{
    // Required for SaveStream — WriteHeader serializes mType into the .oct
    // header, and the on-disk type is what the asset loader hands to
    // CreateInstance(). Without this the file saves with mType = 0 and the
    // next editor restart can't resolve the factory.
    mType = InputPromptMap::GetStaticType();
    mName = "InputPromptMap";
}

InputPromptMap::~InputPromptMap()
{
}

void InputPromptMap::Create()
{
    Asset::Create();
    RebuildIndex();
}

void InputPromptMap::Destroy()
{
    Asset::Destroy();
}

void InputPromptMap::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    if (mVersion < ASSET_VERSION_INPUT_PROMPT_MAP)
        return;

    uint32_t numEntries = stream.ReadUint32();
    mEntries.resize(numEntries);
    for (uint32_t i = 0; i < numEntries; ++i)
    {
        InputPromptEntry& e = mEntries[i];
        e.mPlatform = (Platform)stream.ReadInt32();
        e.mGamepadType = (GamepadType)stream.ReadInt32();
        stream.ReadString(e.mInputPath);
        e.mKind = (InputPromptKind)stream.ReadUint8();
        stream.ReadAsset(e.mSprite);
        stream.ReadAsset(e.mGlyphFont);
        e.mGlyphCodepoint = stream.ReadUint32();
        stream.ReadString(e.mFallbackText);
    }

    mIndexDirty = true;
}

void InputPromptMap::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteUint32((uint32_t)mEntries.size());
    for (const InputPromptEntry& e : mEntries)
    {
        stream.WriteInt32((int32_t)e.mPlatform);
        stream.WriteInt32((int32_t)e.mGamepadType);
        stream.WriteString(e.mInputPath);
        stream.WriteUint8((uint8_t)e.mKind);
        stream.WriteAsset(e.mSprite);
        stream.WriteAsset(e.mGlyphFont);
        stream.WriteUint32(e.mGlyphCodepoint);
        stream.WriteString(e.mFallbackText);
    }
}

void InputPromptMap::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);
    // The detailed per-entry UI lives in the custom InputPromptMapInspector
    // (editor only). The default property table here intentionally stays empty
    // so it doesn't compete with the dedicated inspector.
}

glm::vec4 InputPromptMap::GetTypeColor()
{
    return glm::vec4(0.45f, 0.55f, 1.0f, 1.0f);
}

const char* InputPromptMap::GetTypeName()
{
    return "InputPromptMap";
}

std::vector<InputPromptEntry>& InputPromptMap::GetEntries()
{
    mIndexDirty = true;  // any external mutation invalidates the index
    return mEntries;
}

const std::vector<InputPromptEntry>& InputPromptMap::GetEntries() const
{
    return mEntries;
}

void InputPromptMap::RebuildIndex()
{
    mIndex.clear();
    mIndex.reserve(mEntries.size() * 2);
    for (size_t i = 0; i < mEntries.size(); ++i)
    {
        const InputPromptEntry& e = mEntries[i];
        // Index every entry under its declared keys. We rely on Find() to fall
        // through "any" buckets so we don't have to enumerate every (platform,
        // device) combination here.
        mIndex[MakeIndexKey(e.mPlatform, e.mGamepadType, e.mInputPath)] = i;
    }
    mIndexDirty = false;
}

const InputPromptEntry* InputPromptMap::Find(Platform platform,
                                             GamepadType deviceType,
                                             const std::string& inputPath) const
{
    if (mIndexDirty)
    {
        const_cast<InputPromptMap*>(this)->RebuildIndex();
    }

    // Match priority — most specific first.
    const std::string keys[4] = {
        MakeIndexKey(platform,        deviceType,           inputPath),
        MakeIndexKey(platform,        GamepadType::Count,   inputPath),
        MakeIndexKey(Platform::Count, deviceType,           inputPath),
        MakeIndexKey(Platform::Count, GamepadType::Count,   inputPath),
    };

    for (const std::string& k : keys)
    {
        auto it = mIndex.find(k);
        if (it != mIndex.end() && it->second < mEntries.size())
            return &mEntries[it->second];
    }
    return nullptr;
}

bool InputPromptMap::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    return false;
}
