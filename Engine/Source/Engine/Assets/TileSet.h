#pragma once

#include <string>
#include <vector>
#include "glm/glm.hpp"
#include "Asset.h"
#include "AssetRef.h"

class Texture;

// Per-tile collision shape kind. Phase 1 only authors None / FullSolid via the
// inspector; richer types (Box, Boxes, Slope, Polygon) are populated in Phase 2
// and beyond when the dedicated TileSet editor lands.
enum class TileCollisionType : uint8_t
{
    None = 0,
    FullSolid,
    Box,
    Boxes,
    Slope,
    Polygon,

    Count
};

// Metadata for a single tile slot in the atlas. Tile indices are stable —
// every slot in the atlas grid gets a TileDefinition, even if it is unused,
// so layouts can grow without breaking saved TileMaps.
struct TileDefinition
{
    int32_t mTileIndex = -1;
    glm::ivec2 mAtlasCoord = { 0, 0 };
    std::string mName;
    std::vector<std::string> mTags;

    bool mHasCollision = false;
    TileCollisionType mCollisionType = TileCollisionType::None;

    // Tile-local pixel coords. Phase 2 populates these from the TileSet editor.
    std::vector<glm::vec4> mCollisionRects;  // (x, y, w, h)
    std::vector<glm::vec2> mCollisionPoly;

    bool mIsAnimated = false;
    std::vector<int32_t> mAnimFrames;
    float mAnimFps = 0.0f;
};

// 9-box brush definition. Authored in the TileSet editor in Phase 3.
struct NineBoxBrushDef
{
    std::string mName;

    int32_t mTopLeft = -1;
    int32_t mTop = -1;
    int32_t mTopRight = -1;
    int32_t mLeft = -1;
    int32_t mCenter = -1;
    int32_t mRight = -1;
    int32_t mBottomLeft = -1;
    int32_t mBottom = -1;
    int32_t mBottomRight = -1;

    std::vector<std::string> mTags;
};

// Phase 4 — autotile / terrain rules.
//
// An autotile rule pattern-matches against the 8 neighbors of a cell. Each
// neighbor slot is one of three states:
//   DontCare       — neighbor is irrelevant
//   MustBeSelf     — neighbor MUST be a member of this autotile group
//   MustNotBeSelf  — neighbor MUST NOT be a member of this autotile group
//
// Slot indices follow a 3x3 grid (with +Y up) skipping the center:
//   [0]=NW [1]=N [2]=NE
//   [3]=W       [4]=E
//   [5]=SW [6]=S [7]=SE
enum class AutotileNeighborState : uint8_t
{
    DontCare = 0,
    MustBeSelf,
    MustNotBeSelf,

    Count
};

struct AutotileRule
{
    AutotileNeighborState mNeighbors[8] = {};
    std::vector<int32_t> mResultTiles;  // pick one (first for now; random in a follow-up)
};

struct AutotileSet
{
    std::string mName;
    std::vector<std::string> mMemberTags;     // a tile is "self" if it has any of these tags
    std::vector<int32_t> mMemberTileIndices;  // ...or its index appears in this list
    std::vector<AutotileRule> mRules;
};

class POLYPHASE_API TileSet : public Asset
{
public:

    DECLARE_ASSET(TileSet, Asset);

    TileSet();
    virtual ~TileSet();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    Texture* GetTexture() const;
    void SetTexture(Texture* tex);

    int32_t GetTileWidth() const { return mTileWidth; }
    int32_t GetTileHeight() const { return mTileHeight; }
    int32_t GetMarginX() const { return mMarginX; }
    int32_t GetMarginY() const { return mMarginY; }
    int32_t GetSpacingX() const { return mSpacingX; }
    int32_t GetSpacingY() const { return mSpacingY; }

    // Recompute mAtlasColumns / mAtlasRows from the texture and slicing parameters
    // and resize mTiles to match. Called automatically by HandlePropChange.
    void RebuildTileGrid();

    // Number of valid tile slots in the atlas (rows * columns).
    int32_t GetNumTiles() const;

    glm::ivec2 GetAtlasGridSize() const { return { mAtlasColumns, mAtlasRows }; }

    // Convert a tile index to its (column, row) in the atlas. Returns (-1,-1) if out of range.
    glm::ivec2 TileIndexToAtlasCoord(int32_t tileIndex) const;

    // Compute the [u0,v0]..[u1,v1] UV rectangle for a tile in [0,1] texture space.
    // Returns false if the index is out of range or no texture is bound.
    bool GetTileUVs(int32_t tileIndex, glm::vec2& outUV0, glm::vec2& outUV1) const;

    // Const accessors for tile metadata. Returns nullptr if the index is out of range.
    const TileDefinition* GetTileDef(int32_t tileIndex) const;
    TileDefinition* GetTileDefMutable(int32_t tileIndex);

    bool IsTileSolid(int32_t tileIndex) const;
    bool HasTileTag(int32_t tileIndex, const std::string& tag) const;

    const std::vector<TileDefinition>& GetTiles() const { return mTiles; }

    // 9-box brush accessors (Phase 3)
    const std::vector<NineBoxBrushDef>& GetNineBoxBrushes() const { return mNineBoxBrushes; }
    std::vector<NineBoxBrushDef>& GetNineBoxBrushesMutable() { return mNineBoxBrushes; }
    int32_t AddNineBoxBrush(const std::string& name);
    void RemoveNineBoxBrush(int32_t index);

    // Autotile accessors (Phase 4)
    const std::vector<AutotileSet>& GetAutotileSets() const { return mAutotileSets; }
    std::vector<AutotileSet>& GetAutotileSetsMutable() { return mAutotileSets; }
    int32_t AddAutotileSet(const std::string& name);
    void RemoveAutotileSet(int32_t index);

    // Returns true if the given tile index belongs to the autotile group
    // (matches one of its mMemberTileIndices or mMemberTags entries).
    bool IsTileMemberOfAutotile(int32_t autotileIndex, int32_t tileIndex) const;

    // Walk the autotile rules in order and return the first matching tile
    // index for the given 8-neighbor mask. The mask uses a single bit per
    // neighbor slot (1 = neighbor is self, 0 = neighbor is not self), in the
    // same slot order as AutotileRule::mNeighbors. Returns -1 if no rule matches.
    int32_t MatchAutotileRule(int32_t autotileIndex, uint8_t selfMask) const;

    static bool HandlePropChange(class Datum* datum, uint32_t index, const void* newValue);

protected:

    AssetRef mTexture;
    int32_t mTileWidth = 16;
    int32_t mTileHeight = 16;
    int32_t mMarginX = 0;
    int32_t mMarginY = 0;
    int32_t mSpacingX = 0;
    int32_t mSpacingY = 0;

    int32_t mAtlasColumns = 0;
    int32_t mAtlasRows = 0;

    std::vector<TileDefinition> mTiles;
    std::vector<NineBoxBrushDef> mNineBoxBrushes;
    std::vector<AutotileSet> mAutotileSets;
};
