#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "glm/glm.hpp"
#include "Asset.h"
#include "AssetRef.h"

class TileSet;

// Tiles within a layer are stored in fixed-size chunks. Empty chunks are not
// allocated, so painting outward (including in negative coordinates) only
// costs memory for the cells that have actually been touched.
static constexpr int32_t kTileChunkSize = 32;

enum class TileMapLayerType : uint8_t
{
    Visual = 0,
    Collision,
    Decal,
    Gameplay,

    Count
};

// Single placed cell in a layer. mTileIndex of -1 means "no tile".
struct TileCell
{
    int32_t mTileIndex = -1;
    uint8_t mFlags = 0;     // bit 0 = flipX, bit 1 = flipY, bit 2 = rot90, bit 3 = hidden
    uint8_t mPadding = 0;
    int16_t mVariant = 0;   // for animated/variant tiles
};

// A fixed-size grid of cells. Allocated lazily when the first cell is painted.
struct TileChunk
{
    TileCell mCells[kTileChunkSize * kTileChunkSize] = {};
};

struct TileMapLayer
{
    std::string mName = "Layer";
    bool mVisible = true;
    bool mLocked = false;
    float mOpacity = 1.0f;
    TileMapLayerType mType = TileMapLayerType::Visual;
    int32_t mZOrder = 0;

    // Sparse chunk storage. Key packs (chunkX, chunkY) into one int64_t so
    // negative coordinates work natively.
    std::unordered_map<int64_t, TileChunk> mChunks;
};

class POLYPHASE_API TileMap : public Asset
{
public:

    DECLARE_ASSET(TileMap, Asset);

    TileMap();
    virtual ~TileMap();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    TileSet* GetTileSet() const;
    void SetTileSet(TileSet* tileSet);

    glm::ivec2 GetTileSize() const { return mTileSize; }
    void SetTileSize(glm::ivec2 size) { mTileSize = size; }

    glm::vec2 GetOrigin() const { return mOrigin; }
    void SetOrigin(glm::vec2 origin) { mOrigin = origin; }

    int32_t GetNumLayers() const { return int32_t(mLayers.size()); }
    TileMapLayer* GetLayer(int32_t layerIndex);
    const TileMapLayer* GetLayer(int32_t layerIndex) const;

    // Cell APIs. Layer index is clamped to [0, numLayers). Out-of-range layers
    // become no-ops on writes / return -1 on reads.
    TileCell GetCell(int32_t cellX, int32_t cellY, int32_t layerIndex = 0) const;
    void SetCell(int32_t cellX, int32_t cellY, const TileCell& cell, int32_t layerIndex = 0);
    void ClearCell(int32_t cellX, int32_t cellY, int32_t layerIndex = 0);
    bool IsCellOccupied(int32_t cellX, int32_t cellY, int32_t layerIndex = 0) const;

    // Convenience for the simple "tile index" path used by paint tools and Lua.
    int32_t GetTile(int32_t cellX, int32_t cellY, int32_t layerIndex = 0) const;
    void SetTile(int32_t cellX, int32_t cellY, int32_t tileIndex, int32_t layerIndex = 0);

    // Bulk replace helpers (Phase 3). Returns the number of cells modified.
    // ReplaceTile swaps every cell whose mTileIndex equals oldIndex.
    // ReplaceTilesWithTag uses the bound TileSet to match cells whose tile carries the tag.
    int32_t ReplaceTile(int32_t oldIndex, int32_t newIndex, int32_t layerIndex = 0);
    int32_t ReplaceTilesWithTag(const std::string& tag, int32_t newIndex, int32_t layerIndex = 0);
    int32_t CountTileUses(int32_t tileIndex, int32_t layerIndex = 0) const;

    // Used-area bookkeeping. Updated by SetCell so the renderer can build a
    // tight AABB without scanning every chunk.
    glm::ivec2 GetMinUsed() const { return mMinUsed; }
    glm::ivec2 GetMaxUsed() const { return mMaxUsed; }
    bool HasContent() const { return mHasContent; }

    // Dirty-chunk tracking. The node calls TakeDirtyChunks() each tick to
    // figure out which chunks need a vertex rebuild + GPU upload.
    const std::unordered_set<int64_t>& GetDirtyChunks() const { return mDirtyChunks; }
    void ClearDirtyChunks() { mDirtyChunks.clear(); }
    void MarkAllDirty();

    // Chunk key helpers — exposed because the node and the paint manager need them.
    static int64_t PackChunkKey(int32_t chunkX, int32_t chunkY);
    static void UnpackChunkKey(int64_t key, int32_t& outChunkX, int32_t& outChunkY);
    static int32_t FloorDivChunk(int32_t cell);
    static int32_t FloorModChunk(int32_t cell);

    static bool HandlePropChange(class Datum* datum, uint32_t index, const void* newValue);

protected:

    void EnsureLayer(int32_t layerIndex);
    void RecomputeUsedBounds();

    AssetRef mTileSet;
    // Default to 1 world unit per cell — matches standard 2D engine conventions
    // and keeps freshly-spawned TileMap2D nodes visible without scaling. Override
    // in the inspector if you want pixel-perfect or larger physics units.
    glm::ivec2 mTileSize = { 1, 1 };
    glm::vec2 mOrigin = { 0.0f, 0.0f };
    bool mAllowNegativeCoords = true;

    std::vector<TileMapLayer> mLayers;

    glm::ivec2 mMinUsed = { 0, 0 };
    glm::ivec2 mMaxUsed = { 0, 0 };
    bool mHasContent = false;

    std::unordered_set<int64_t> mDirtyChunks;
};
