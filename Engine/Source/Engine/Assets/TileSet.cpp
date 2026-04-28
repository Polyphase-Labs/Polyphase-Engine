#include "Assets/TileSet.h"
#include "Assets/Texture.h"
#include "Assets/TextureAtlasUtil.h"
#include "Stream.h"
#include "Property.h"
#include "Log.h"

FORCE_LINK_DEF(TileSet);
DEFINE_ASSET(TileSet);

bool TileSet::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    TileSet* tileSet = static_cast<TileSet*>(prop->mOwner);

    // Datum::SetXxx invokes the change handler BEFORE writing the new value
    // into the underlying member, and only writes if the handler returns
    // false. RebuildTileGrid() reads mTileWidth/mTileHeight/etc., so we have
    // to commit the new value ourselves first or the rebuild uses stale
    // dimensions and the atlas grid stays at the OLD slicing.
    bool handled = false;
    if (prop->mName == "Texture")
    {
        Asset** newAsset = (Asset**)newValue;
        tileSet->mTexture = (newAsset != nullptr) ? *newAsset : nullptr;
        handled = true;
    }
    else if (prop->mName == "Atlas Tile Width")
    {
        tileSet->mTileWidth = *(const int32_t*)newValue;
        handled = true;
    }
    else if (prop->mName == "Atlas Tile Height")
    {
        tileSet->mTileHeight = *(const int32_t*)newValue;
        handled = true;
    }
    else if (prop->mName == "Atlas Margin X")
    {
        tileSet->mMarginX = *(const int32_t*)newValue;
        handled = true;
    }
    else if (prop->mName == "Atlas Margin Y")
    {
        tileSet->mMarginY = *(const int32_t*)newValue;
        handled = true;
    }
    else if (prop->mName == "Atlas Spacing X")
    {
        tileSet->mSpacingX = *(const int32_t*)newValue;
        handled = true;
    }
    else if (prop->mName == "Atlas Spacing Y")
    {
        tileSet->mSpacingY = *(const int32_t*)newValue;
        handled = true;
    }

    tileSet->RebuildTileGrid();

    HandleAssetPropChange(datum, index, newValue);

    // Returning true tells the Datum framework "I already wrote the value,
    // don't overwrite it." This matches Texture::HandlePropChange's pattern.
    return handled;
}

TileSet::TileSet()
{
    mType = TileSet::GetStaticType();
    mName = "TileSet";
}

TileSet::~TileSet()
{
}

void TileSet::Create()
{
    Asset::Create();
    RebuildTileGrid();
}

void TileSet::Destroy()
{
    Asset::Destroy();
}

void TileSet::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    outProps.push_back(Property(DatumType::Asset, "Texture", this, &mTexture, 1, HandlePropChange, int32_t(Texture::GetStaticType())));
    // Atlas pixel dimensions (NOT world cell size — that lives on the TileMap).
    outProps.push_back(Property(DatumType::Integer, "Atlas Tile Width", this, &mTileWidth, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Integer, "Atlas Tile Height", this, &mTileHeight, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Integer, "Atlas Margin X", this, &mMarginX, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Integer, "Atlas Margin Y", this, &mMarginY, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Integer, "Atlas Spacing X", this, &mSpacingX, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Integer, "Atlas Spacing Y", this, &mSpacingY, 1, HandlePropChange));
}

glm::vec4 TileSet::GetTypeColor()
{
    return glm::vec4(0.4f, 0.7f, 0.9f, 1.0f);
}

const char* TileSet::GetTypeName()
{
    return "TileSet";
}

Texture* TileSet::GetTexture() const
{
    return mTexture.Get<Texture>();
}

void TileSet::SetTexture(Texture* tex)
{
    mTexture = tex;
    RebuildTileGrid();
}

void TileSet::RebuildTileGrid()
{
    Texture* tex = mTexture.Get<Texture>();
    if (tex == nullptr || mTileWidth <= 0 || mTileHeight <= 0)
    {
        mAtlasColumns = 0;
        mAtlasRows = 0;
        // Don't wipe metadata — preserve any tile defs that already exist.
        return;
    }

    int32_t usableW = int32_t(tex->GetWidth()) - mMarginX * 2;
    int32_t usableH = int32_t(tex->GetHeight()) - mMarginY * 2;
    int32_t stepX = mTileWidth + mSpacingX;
    int32_t stepY = mTileHeight + mSpacingY;

    if (stepX <= 0 || stepY <= 0)
    {
        mAtlasColumns = 0;
        mAtlasRows = 0;
        return;
    }

    mAtlasColumns = (usableW + mSpacingX) / stepX;
    mAtlasRows = (usableH + mSpacingY) / stepY;
    if (mAtlasColumns < 0) mAtlasColumns = 0;
    if (mAtlasRows < 0) mAtlasRows = 0;

    int32_t totalTiles = mAtlasColumns * mAtlasRows;
    int32_t oldCount = int32_t(mTiles.size());

    if (totalTiles != oldCount)
    {
        mTiles.resize(totalTiles);
    }

    // Refresh tile index + atlas coord for every slot. Preserves user-authored
    // metadata (name, tags, collision) on overlapping slots.
    for (int32_t i = 0; i < totalTiles; ++i)
    {
        TileDefinition& def = mTiles[i];
        def.mTileIndex = i;
        def.mAtlasCoord = { i % mAtlasColumns, i / mAtlasColumns };
    }
}

int32_t TileSet::GetNumTiles() const
{
    return int32_t(mTiles.size());
}

glm::ivec2 TileSet::TileIndexToAtlasCoord(int32_t tileIndex) const
{
    if (tileIndex < 0 || mAtlasColumns <= 0 || tileIndex >= int32_t(mTiles.size()))
        return { -1, -1 };

    return { tileIndex % mAtlasColumns, tileIndex / mAtlasColumns };
}

bool TileSet::GetTileUVs(int32_t tileIndex, glm::vec2& outUV0, glm::vec2& outUV1) const
{
    Texture* tex = mTexture.Get<Texture>();
    if (tex == nullptr || tileIndex < 0 || tileIndex >= int32_t(mTiles.size()))
        return false;

    return ComputeAtlasCellUV(
        mAtlasColumns, mAtlasRows,
        mTileWidth, mTileHeight,
        mMarginX, mMarginY,
        mSpacingX, mSpacingY,
        tileIndex,
        int32_t(tex->GetWidth()), int32_t(tex->GetHeight()),
        outUV0, outUV1);
}

const TileDefinition* TileSet::GetTileDef(int32_t tileIndex) const
{
    if (tileIndex < 0 || tileIndex >= int32_t(mTiles.size()))
        return nullptr;
    return &mTiles[tileIndex];
}

TileDefinition* TileSet::GetTileDefMutable(int32_t tileIndex)
{
    if (tileIndex < 0 || tileIndex >= int32_t(mTiles.size()))
        return nullptr;
    return &mTiles[tileIndex];
}

bool TileSet::IsTileSolid(int32_t tileIndex) const
{
    const TileDefinition* def = GetTileDef(tileIndex);
    return def != nullptr && def->mHasCollision && def->mCollisionType != TileCollisionType::None;
}

int32_t TileSet::AddNineBoxBrush(const std::string& name)
{
    NineBoxBrushDef brush;
    brush.mName = name.empty() ? std::string("Brush ") + std::to_string(mNineBoxBrushes.size()) : name;
    mNineBoxBrushes.push_back(std::move(brush));
    return int32_t(mNineBoxBrushes.size() - 1);
}

void TileSet::RemoveNineBoxBrush(int32_t index)
{
    if (index < 0 || index >= int32_t(mNineBoxBrushes.size()))
        return;
    mNineBoxBrushes.erase(mNineBoxBrushes.begin() + index);
}

int32_t TileSet::AddAutotileSet(const std::string& name)
{
    AutotileSet set;
    set.mName = name.empty() ? std::string("Autotile ") + std::to_string(mAutotileSets.size()) : name;
    mAutotileSets.push_back(std::move(set));
    return int32_t(mAutotileSets.size() - 1);
}

void TileSet::RemoveAutotileSet(int32_t index)
{
    if (index < 0 || index >= int32_t(mAutotileSets.size()))
        return;
    mAutotileSets.erase(mAutotileSets.begin() + index);
}

bool TileSet::IsTileMemberOfAutotile(int32_t autotileIndex, int32_t tileIndex) const
{
    if (autotileIndex < 0 || autotileIndex >= int32_t(mAutotileSets.size()))
        return false;
    if (tileIndex < 0)
        return false;

    const AutotileSet& set = mAutotileSets[autotileIndex];

    for (int32_t idx : set.mMemberTileIndices)
    {
        if (idx == tileIndex)
            return true;
    }

    if (!set.mMemberTags.empty())
    {
        for (const std::string& tag : set.mMemberTags)
        {
            if (HasTileTag(tileIndex, tag))
                return true;
        }
    }

    return false;
}

int32_t TileSet::MatchAutotileRule(int32_t autotileIndex, uint8_t selfMask) const
{
    if (autotileIndex < 0 || autotileIndex >= int32_t(mAutotileSets.size()))
        return -1;

    const AutotileSet& set = mAutotileSets[autotileIndex];
    for (const AutotileRule& rule : set.mRules)
    {
        bool match = true;
        for (int32_t i = 0; i < 8; ++i)
        {
            AutotileNeighborState state = rule.mNeighbors[i];
            if (state == AutotileNeighborState::DontCare)
                continue;

            bool neighborIsSelf = ((selfMask >> i) & 0x01u) != 0;
            if (state == AutotileNeighborState::MustBeSelf && !neighborIsSelf)
            {
                match = false;
                break;
            }
            if (state == AutotileNeighborState::MustNotBeSelf && neighborIsSelf)
            {
                match = false;
                break;
            }
        }

        if (match && !rule.mResultTiles.empty())
        {
            // First-result for now; weighted/random pick can come later.
            return rule.mResultTiles[0];
        }
    }

    return -1;
}

bool TileSet::HasTileTag(int32_t tileIndex, const std::string& tag) const
{
    const TileDefinition* def = GetTileDef(tileIndex);
    if (def == nullptr)
        return false;

    for (const std::string& t : def->mTags)
    {
        if (t == tag)
            return true;
    }
    return false;
}

void TileSet::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteAsset(mTexture);
    stream.WriteInt32(mTileWidth);
    stream.WriteInt32(mTileHeight);
    stream.WriteInt32(mMarginX);
    stream.WriteInt32(mMarginY);
    stream.WriteInt32(mSpacingX);
    stream.WriteInt32(mSpacingY);
    stream.WriteInt32(mAtlasColumns);
    stream.WriteInt32(mAtlasRows);

    // Tile definitions. Phase 2 (ASSET_VERSION_TILESET_METADATA) persists
    // tags, collision rects, and animation frames in addition to the Phase 1
    // base fields.
    uint32_t numTiles = uint32_t(mTiles.size());
    stream.WriteUint32(numTiles);
    for (uint32_t i = 0; i < numTiles; ++i)
    {
        const TileDefinition& def = mTiles[i];
        stream.WriteInt32(def.mTileIndex);
        stream.WriteInt32(def.mAtlasCoord.x);
        stream.WriteInt32(def.mAtlasCoord.y);
        stream.WriteString(def.mName);
        stream.WriteBool(def.mHasCollision);
        stream.WriteUint8(uint8_t(def.mCollisionType));

        // Phase 2 metadata.
        uint32_t numTags = uint32_t(def.mTags.size());
        stream.WriteUint32(numTags);
        for (uint32_t t = 0; t < numTags; ++t)
            stream.WriteString(def.mTags[t]);

        uint32_t numColRects = uint32_t(def.mCollisionRects.size());
        stream.WriteUint32(numColRects);
        for (uint32_t r = 0; r < numColRects; ++r)
            stream.WriteVec4(def.mCollisionRects[r]);

        uint32_t numColPoly = uint32_t(def.mCollisionPoly.size());
        stream.WriteUint32(numColPoly);
        for (uint32_t p = 0; p < numColPoly; ++p)
            stream.WriteVec2(def.mCollisionPoly[p]);

        stream.WriteBool(def.mIsAnimated);
        uint32_t numFrames = uint32_t(def.mAnimFrames.size());
        stream.WriteUint32(numFrames);
        for (uint32_t f = 0; f < numFrames; ++f)
            stream.WriteInt32(def.mAnimFrames[f]);
        stream.WriteFloat(def.mAnimFps);
    }

    // Nine-box brushes. Empty in Phase 1.
    uint32_t numBrushes = uint32_t(mNineBoxBrushes.size());
    stream.WriteUint32(numBrushes);
    for (uint32_t i = 0; i < numBrushes; ++i)
    {
        const NineBoxBrushDef& b = mNineBoxBrushes[i];
        stream.WriteString(b.mName);
        stream.WriteInt32(b.mTopLeft);
        stream.WriteInt32(b.mTop);
        stream.WriteInt32(b.mTopRight);
        stream.WriteInt32(b.mLeft);
        stream.WriteInt32(b.mCenter);
        stream.WriteInt32(b.mRight);
        stream.WriteInt32(b.mBottomLeft);
        stream.WriteInt32(b.mBottom);
        stream.WriteInt32(b.mBottomRight);
    }

    // Autotile sets (Phase 4 — ASSET_VERSION_TILESET_AUTOTILE).
    uint32_t numAutotiles = uint32_t(mAutotileSets.size());
    stream.WriteUint32(numAutotiles);
    for (uint32_t a = 0; a < numAutotiles; ++a)
    {
        const AutotileSet& set = mAutotileSets[a];
        stream.WriteString(set.mName);

        uint32_t numMemberTags = uint32_t(set.mMemberTags.size());
        stream.WriteUint32(numMemberTags);
        for (uint32_t t = 0; t < numMemberTags; ++t)
            stream.WriteString(set.mMemberTags[t]);

        uint32_t numMemberIdx = uint32_t(set.mMemberTileIndices.size());
        stream.WriteUint32(numMemberIdx);
        for (uint32_t m = 0; m < numMemberIdx; ++m)
            stream.WriteInt32(set.mMemberTileIndices[m]);

        uint32_t numRules = uint32_t(set.mRules.size());
        stream.WriteUint32(numRules);
        for (uint32_t r = 0; r < numRules; ++r)
        {
            const AutotileRule& rule = set.mRules[r];
            for (int32_t n = 0; n < 8; ++n)
                stream.WriteUint8(uint8_t(rule.mNeighbors[n]));

            uint32_t numResults = uint32_t(rule.mResultTiles.size());
            stream.WriteUint32(numResults);
            for (uint32_t rt = 0; rt < numResults; ++rt)
                stream.WriteInt32(rule.mResultTiles[rt]);
        }
    }
}

void TileSet::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    if (mVersion < ASSET_VERSION_TILESET_BASE)
    {
        // Should never happen — TileSet was added at this version.
        return;
    }

    stream.ReadAsset(mTexture);
    mTileWidth = stream.ReadInt32();
    mTileHeight = stream.ReadInt32();
    mMarginX = stream.ReadInt32();
    mMarginY = stream.ReadInt32();
    mSpacingX = stream.ReadInt32();
    mSpacingY = stream.ReadInt32();
    mAtlasColumns = stream.ReadInt32();
    mAtlasRows = stream.ReadInt32();

    uint32_t numTiles = stream.ReadUint32();
    mTiles.resize(numTiles);
    for (uint32_t i = 0; i < numTiles; ++i)
    {
        TileDefinition& def = mTiles[i];
        def.mTileIndex = stream.ReadInt32();
        def.mAtlasCoord.x = stream.ReadInt32();
        def.mAtlasCoord.y = stream.ReadInt32();
        stream.ReadString(def.mName);
        def.mHasCollision = stream.ReadBool();
        def.mCollisionType = TileCollisionType(stream.ReadUint8());

        if (mVersion >= ASSET_VERSION_TILESET_METADATA)
        {
            uint32_t numTags = stream.ReadUint32();
            def.mTags.resize(numTags);
            for (uint32_t t = 0; t < numTags; ++t)
                stream.ReadString(def.mTags[t]);

            uint32_t numColRects = stream.ReadUint32();
            def.mCollisionRects.resize(numColRects);
            for (uint32_t r = 0; r < numColRects; ++r)
                def.mCollisionRects[r] = stream.ReadVec4();

            uint32_t numColPoly = stream.ReadUint32();
            def.mCollisionPoly.resize(numColPoly);
            for (uint32_t p = 0; p < numColPoly; ++p)
                def.mCollisionPoly[p] = stream.ReadVec2();

            def.mIsAnimated = stream.ReadBool();
            uint32_t numFrames = stream.ReadUint32();
            def.mAnimFrames.resize(numFrames);
            for (uint32_t f = 0; f < numFrames; ++f)
                def.mAnimFrames[f] = stream.ReadInt32();
            def.mAnimFps = stream.ReadFloat();
        }
    }

    uint32_t numBrushes = stream.ReadUint32();
    mNineBoxBrushes.resize(numBrushes);
    for (uint32_t i = 0; i < numBrushes; ++i)
    {
        NineBoxBrushDef& b = mNineBoxBrushes[i];
        stream.ReadString(b.mName);
        b.mTopLeft = stream.ReadInt32();
        b.mTop = stream.ReadInt32();
        b.mTopRight = stream.ReadInt32();
        b.mLeft = stream.ReadInt32();
        b.mCenter = stream.ReadInt32();
        b.mRight = stream.ReadInt32();
        b.mBottomLeft = stream.ReadInt32();
        b.mBottom = stream.ReadInt32();
        b.mBottomRight = stream.ReadInt32();
    }

    // Autotile sets (Phase 4)
    if (mVersion >= ASSET_VERSION_TILESET_AUTOTILE)
    {
        uint32_t numAutotiles = stream.ReadUint32();
        mAutotileSets.resize(numAutotiles);
        for (uint32_t a = 0; a < numAutotiles; ++a)
        {
            AutotileSet& set = mAutotileSets[a];
            stream.ReadString(set.mName);

            uint32_t numMemberTags = stream.ReadUint32();
            set.mMemberTags.resize(numMemberTags);
            for (uint32_t t = 0; t < numMemberTags; ++t)
                stream.ReadString(set.mMemberTags[t]);

            uint32_t numMemberIdx = stream.ReadUint32();
            set.mMemberTileIndices.resize(numMemberIdx);
            for (uint32_t m = 0; m < numMemberIdx; ++m)
                set.mMemberTileIndices[m] = stream.ReadInt32();

            uint32_t numRules = stream.ReadUint32();
            set.mRules.resize(numRules);
            for (uint32_t r = 0; r < numRules; ++r)
            {
                AutotileRule& rule = set.mRules[r];
                for (int32_t n = 0; n < 8; ++n)
                    rule.mNeighbors[n] = AutotileNeighborState(stream.ReadUint8());

                uint32_t numResults = stream.ReadUint32();
                rule.mResultTiles.resize(numResults);
                for (uint32_t rt = 0; rt < numResults; ++rt)
                    rule.mResultTiles[rt] = stream.ReadInt32();
            }
        }
    }
}
