#include "Assets/SpriteAnimation.h"
#include "Assets/Texture.h"
#include "Assets/TextureAtlasUtil.h"

#include "Stream.h"
#include "Property.h"
#include "Log.h"

FORCE_LINK_DEF(SpriteAnimation);
DEFINE_ASSET(SpriteAnimation);

static const char* sFrameSourceModeStrings[] =
{
    "Discrete",
    "Atlas Grid",
};
static_assert(int32_t(SpriteFrameSourceMode::Count) == 2,
              "Need to update SpriteFrameSourceMode string table");

bool SpriteAnimation::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    SpriteAnimation* anim = static_cast<SpriteAnimation*>(prop->mOwner);
    bool handled = false;

    if (prop->mName == "Mode")
    {
        anim->mMode = SpriteFrameSourceMode(*(const uint8_t*)newValue);
        handled = true;
    }

    HandleAssetPropChange(datum, index, newValue);
    return handled;
}

SpriteAnimation::SpriteAnimation()
{
    mType = SpriteAnimation::GetStaticType();
    mName = "SpriteAnimation";
}

SpriteAnimation::~SpriteAnimation()
{
}

void SpriteAnimation::Create()
{
    Asset::Create();
}

void SpriteAnimation::Destroy()
{
    Asset::Destroy();
}

bool SpriteAnimation::Import(const std::string& path, ImportOptions* options)
{
    // Default to base Asset::Import. Folder-of-PNGs import is a Phase 2 nicety —
    // for v1 users add frames manually in the inspector, or via script with
    // SpriteAnimator's CreateAnimation/AddImage runtime API.
    return Asset::Import(path, options);
}

void SpriteAnimation::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    SCOPED_CATEGORY("Sprite Animation");

    outProps.push_back(Property(DatumType::String, "Animation Name", this, &mAnimationName));
    outProps.push_back(Property(DatumType::Byte, "Mode", this, &mMode, 1, HandlePropChange,
                                NULL_DATUM, int32_t(SpriteFrameSourceMode::Count), sFrameSourceModeStrings));
    outProps.push_back(Property(DatumType::Float, "FPS", this, &mFps));
    outProps.push_back(Property(DatumType::Bool, "Loop", this, &mLoop));

    // Both modes' fields are always exposed; the cleanest way for users to see
    // only relevant ones is via mode-aware DrawCustomProperty (added later).
    // For v1 the simpler approach is "everything visible, only what matches
    // the mode is read at runtime". This keeps the asset inspector trivial.

    // Discrete fields
    outProps.push_back(Property(DatumType::Asset, "Frames", this, &mFrames, 1, nullptr,
                                int32_t(Texture::GetStaticType())).MakeVector());

    // Atlas fields
    outProps.push_back(Property(DatumType::Asset, "Atlas Texture", this, &mAtlasTexture, 1, nullptr,
                                int32_t(Texture::GetStaticType())));
    outProps.push_back(Property(DatumType::Integer, "Atlas Cols", this, &mAtlasCols));
    outProps.push_back(Property(DatumType::Integer, "Atlas Rows", this, &mAtlasRows));
    outProps.push_back(Property(DatumType::Integer, "Atlas Margin X", this, &mAtlasMarginX));
    outProps.push_back(Property(DatumType::Integer, "Atlas Margin Y", this, &mAtlasMarginY));
    outProps.push_back(Property(DatumType::Integer, "Atlas Spacing X", this, &mAtlasSpacingX));
    outProps.push_back(Property(DatumType::Integer, "Atlas Spacing Y", this, &mAtlasSpacingY));
    outProps.push_back(Property(DatumType::Integer, "Atlas Frame Indices", this, &mAtlasFrameIndices).MakeVector());
}

glm::vec4 SpriteAnimation::GetTypeColor()
{
    return glm::vec4(0.95f, 0.55f, 0.15f, 1.0f);
}

const char* SpriteAnimation::GetTypeName()
{
    return "SpriteAnimation";
}

void SpriteAnimation::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteString(mAnimationName);
    stream.WriteUint8(uint8_t(mMode));
    stream.WriteFloat(mFps);
    stream.WriteBool(mLoop);

    // Write both modes' data unconditionally so the asset can be edited freely
    // between modes without losing the inactive mode's setup.

    // Discrete frames
    uint32_t numFrames = uint32_t(mFrames.size());
    stream.WriteUint32(numFrames);
    for (uint32_t i = 0; i < numFrames; ++i)
    {
        stream.WriteAsset(mFrames[i]);
    }

    // Atlas
    stream.WriteAsset(mAtlasTexture);
    stream.WriteInt32(mAtlasCols);
    stream.WriteInt32(mAtlasRows);
    stream.WriteInt32(mAtlasMarginX);
    stream.WriteInt32(mAtlasMarginY);
    stream.WriteInt32(mAtlasSpacingX);
    stream.WriteInt32(mAtlasSpacingY);
    uint32_t numAtlasIndices = uint32_t(mAtlasFrameIndices.size());
    stream.WriteUint32(numAtlasIndices);
    for (uint32_t i = 0; i < numAtlasIndices; ++i)
    {
        stream.WriteInt32(mAtlasFrameIndices[i]);
    }
}

void SpriteAnimation::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    stream.ReadString(mAnimationName);
    mMode = SpriteFrameSourceMode(stream.ReadUint8());
    mFps = stream.ReadFloat();
    mLoop = stream.ReadBool();

    uint32_t numFrames = stream.ReadUint32();
    mFrames.resize(numFrames);
    for (uint32_t i = 0; i < numFrames; ++i)
    {
        stream.ReadAsset(mFrames[i]);
    }

    stream.ReadAsset(mAtlasTexture);
    mAtlasCols = stream.ReadInt32();
    mAtlasRows = stream.ReadInt32();
    mAtlasMarginX = stream.ReadInt32();
    mAtlasMarginY = stream.ReadInt32();
    mAtlasSpacingX = stream.ReadInt32();
    mAtlasSpacingY = stream.ReadInt32();
    uint32_t numAtlasIndices = stream.ReadUint32();
    mAtlasFrameIndices.resize(numAtlasIndices);
    for (uint32_t i = 0; i < numAtlasIndices; ++i)
    {
        mAtlasFrameIndices[i] = stream.ReadInt32();
    }
}

void SpriteAnimation::AddFrame(Texture* tex)
{
    mFrames.push_back(TextureRef(tex));
}

void SpriteAnimation::ClearFrames()
{
    mFrames.clear();
}

Texture* SpriteAnimation::GetAtlasTexture() const
{
    return mAtlasTexture.Get<Texture>();
}

void SpriteAnimation::SetAtlasTexture(Texture* tex)
{
    mAtlasTexture = tex;
}

int32_t SpriteAnimation::GetFrameCount() const
{
    if (mMode == SpriteFrameSourceMode::Discrete)
    {
        return int32_t(mFrames.size());
    }
    else
    {
        return int32_t(mAtlasFrameIndices.size());
    }
}

Texture* SpriteAnimation::GetFrameTexture(int32_t frameIndex) const
{
    if (mMode == SpriteFrameSourceMode::Discrete)
    {
        if (frameIndex < 0 || frameIndex >= int32_t(mFrames.size()))
            return nullptr;
        return mFrames[frameIndex].Get<Texture>();
    }
    else
    {
        // All atlas frames share the atlas texture; only UVs change.
        return mAtlasTexture.Get<Texture>();
    }
}

bool SpriteAnimation::GetFrameUV(int32_t frameIndex, glm::vec2& outUV0, glm::vec2& outUV1) const
{
    if (mMode == SpriteFrameSourceMode::Discrete)
    {
        outUV0 = glm::vec2(0.0f, 0.0f);
        outUV1 = glm::vec2(1.0f, 1.0f);
        return frameIndex >= 0 && frameIndex < int32_t(mFrames.size());
    }

    // Atlas mode
    if (frameIndex < 0 || frameIndex >= int32_t(mAtlasFrameIndices.size()))
        return false;

    Texture* tex = mAtlasTexture.Get<Texture>();
    if (tex == nullptr)
        return false;

    if (mAtlasCols <= 0 || mAtlasRows <= 0)
        return false;

    // Cell pixel size = (texW - 2*margin - (cols-1)*spacing) / cols, same for rows.
    const int32_t texW = int32_t(tex->GetWidth());
    const int32_t texH = int32_t(tex->GetHeight());
    const int32_t usableW = texW - mAtlasMarginX * 2 - mAtlasSpacingX * (mAtlasCols - 1);
    const int32_t usableH = texH - mAtlasMarginY * 2 - mAtlasSpacingY * (mAtlasRows - 1);
    if (usableW <= 0 || usableH <= 0)
        return false;

    const int32_t cellW = usableW / mAtlasCols;
    const int32_t cellH = usableH / mAtlasRows;

    return ComputeAtlasCellUV(
        mAtlasCols, mAtlasRows,
        cellW, cellH,
        mAtlasMarginX, mAtlasMarginY,
        mAtlasSpacingX, mAtlasSpacingY,
        mAtlasFrameIndices[frameIndex],
        texW, texH,
        outUV0, outUV1);
}
