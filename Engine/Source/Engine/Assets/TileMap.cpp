#include "Assets/TileMap.h"
#include "Assets/TileSet.h"
#include "Stream.h"
#include "Property.h"
#include "Log.h"

#include <algorithm>

FORCE_LINK_DEF(TileMap);
DEFINE_ASSET(TileMap);

bool TileMap::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    TileMap* tileMap = static_cast<TileMap*>(prop->mOwner);

    // The Datum already wrote the new value into the underlying member.
    // Anything that affects rendering should mark every chunk dirty.
    tileMap->MarkAllDirty();

    HandleAssetPropChange(datum, index, newValue);
    return false;
}

TileMap::TileMap()
{
    mType = TileMap::GetStaticType();
    mName = "TileMap";

    // Phase 1 always allocates a single visual layer.
    mLayers.resize(1);
    mLayers[0].mName = "Main";
    mLayers[0].mType = TileMapLayerType::Visual;
    mLayers[0].mZOrder = 0;
}

TileMap::~TileMap()
{
}

void TileMap::Create()
{
    Asset::Create();
    EnsureLayer(0);
}

void TileMap::Destroy()
{
    Asset::Destroy();
}

void TileMap::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    outProps.push_back(Property(DatumType::Asset, "TileSet", this, &mTileSet, 1, HandlePropChange, int32_t(TileSet::GetStaticType())));
    // World units per cell (NOT atlas pixel size — that's on the TileSet).
    outProps.push_back(Property(DatumType::Integer, "Cell Size X", this, &mTileSize.x, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Integer, "Cell Size Y", this, &mTileSize.y, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Vector2D, "Origin", this, &mOrigin, 1, HandlePropChange));
}

glm::vec4 TileMap::GetTypeColor()
{
    return glm::vec4(0.3f, 0.8f, 0.5f, 1.0f);
}

const char* TileMap::GetTypeName()
{
    return "TileMap";
}

TileSet* TileMap::GetTileSet() const
{
    return mTileSet.Get<TileSet>();
}

void TileMap::SetTileSet(TileSet* tileSet)
{
    mTileSet = tileSet;
    MarkAllDirty();
#if EDITOR
    SetDirtyFlag();
#endif
}

TileMapLayer* TileMap::GetLayer(int32_t layerIndex)
{
    if (layerIndex < 0 || layerIndex >= int32_t(mLayers.size()))
        return nullptr;
    return &mLayers[layerIndex];
}

const TileMapLayer* TileMap::GetLayer(int32_t layerIndex) const
{
    if (layerIndex < 0 || layerIndex >= int32_t(mLayers.size()))
        return nullptr;
    return &mLayers[layerIndex];
}

void TileMap::EnsureLayer(int32_t layerIndex)
{
    if (layerIndex < 0)
        return;

    while (int32_t(mLayers.size()) <= layerIndex)
    {
        TileMapLayer layer;
        layer.mName = "Layer " + std::to_string(mLayers.size());
        layer.mZOrder = int32_t(mLayers.size());
        mLayers.push_back(std::move(layer));
    }
}

int64_t TileMap::PackChunkKey(int32_t chunkX, int32_t chunkY)
{
    return int64_t(uint32_t(chunkX)) | (int64_t(uint32_t(chunkY)) << 32);
}

void TileMap::UnpackChunkKey(int64_t key, int32_t& outChunkX, int32_t& outChunkY)
{
    outChunkX = int32_t(uint32_t(key & 0xFFFFFFFFu));
    outChunkY = int32_t(uint32_t((key >> 32) & 0xFFFFFFFFu));
}

int32_t TileMap::FloorDivChunk(int32_t cell)
{
    // Floor division by kTileChunkSize. Standard / rounds toward zero, so
    // for negative cells we need an explicit adjustment.
    if (cell >= 0)
        return cell / kTileChunkSize;
    return -((kTileChunkSize - 1 - cell) / kTileChunkSize);
}

int32_t TileMap::FloorModChunk(int32_t cell)
{
    int32_t r = cell % kTileChunkSize;
    if (r < 0)
        r += kTileChunkSize;
    return r;
}

TileCell TileMap::GetCell(int32_t cellX, int32_t cellY, int32_t layerIndex) const
{
    const TileMapLayer* layer = GetLayer(layerIndex);
    if (layer == nullptr)
        return TileCell{};

    int32_t cx = FloorDivChunk(cellX);
    int32_t cy = FloorDivChunk(cellY);
    int64_t key = PackChunkKey(cx, cy);

    auto it = layer->mChunks.find(key);
    if (it == layer->mChunks.end())
        return TileCell{};

    int32_t lx = FloorModChunk(cellX);
    int32_t ly = FloorModChunk(cellY);
    return it->second.mCells[ly * kTileChunkSize + lx];
}

void TileMap::SetCell(int32_t cellX, int32_t cellY, const TileCell& cell, int32_t layerIndex)
{
    EnsureLayer(layerIndex);
    TileMapLayer* layer = GetLayer(layerIndex);
    if (layer == nullptr)
        return;

    int32_t cx = FloorDivChunk(cellX);
    int32_t cy = FloorDivChunk(cellY);
    int64_t key = PackChunkKey(cx, cy);

    int32_t lx = FloorModChunk(cellX);
    int32_t ly = FloorModChunk(cellY);

    auto it = layer->mChunks.find(key);
    if (it == layer->mChunks.end())
    {
        if (cell.mTileIndex < 0)
        {
            // Writing empty into an empty chunk: nothing to do.
            return;
        }
        it = layer->mChunks.emplace(key, TileChunk{}).first;
    }

    TileCell& existing = it->second.mCells[ly * kTileChunkSize + lx];
    if (existing.mTileIndex == cell.mTileIndex &&
        existing.mFlags == cell.mFlags &&
        existing.mVariant == cell.mVariant)
    {
        return;  // No change.
    }

    existing = cell;
    mDirtyChunks.insert(key);

#if EDITOR
    // Mark the asset as having unsaved changes so it shows up with a *
    // prefix in the asset browser and surfaces in the shutdown unsaved-check
    // popup. Without this, paint strokes silently disappear on editor close.
    SetDirtyFlag();
#endif

    // Used-bounds tracking. Only grow the AABB; clearing cells doesn't shrink
    // it (RecomputeUsedBounds is the catch-all for that path).
    if (cell.mTileIndex >= 0)
    {
        if (!mHasContent)
        {
            mMinUsed = { cellX, cellY };
            mMaxUsed = { cellX, cellY };
            mHasContent = true;
        }
        else
        {
            // Explicit int32_t to avoid std::min/max template-deduction failure
            // on devkitARM (3DS) where int32_t is `long int` while glm::ivec2's
            // members are plain `int`.
            mMinUsed.x = std::min<int32_t>(mMinUsed.x, cellX);
            mMinUsed.y = std::min<int32_t>(mMinUsed.y, cellY);
            mMaxUsed.x = std::max<int32_t>(mMaxUsed.x, cellX);
            mMaxUsed.y = std::max<int32_t>(mMaxUsed.y, cellY);
        }
    }
}

void TileMap::ClearCell(int32_t cellX, int32_t cellY, int32_t layerIndex)
{
    TileCell empty;
    SetCell(cellX, cellY, empty, layerIndex);
}

bool TileMap::IsCellOccupied(int32_t cellX, int32_t cellY, int32_t layerIndex) const
{
    return GetCell(cellX, cellY, layerIndex).mTileIndex >= 0;
}

int32_t TileMap::GetTile(int32_t cellX, int32_t cellY, int32_t layerIndex) const
{
    return GetCell(cellX, cellY, layerIndex).mTileIndex;
}

void TileMap::SetTile(int32_t cellX, int32_t cellY, int32_t tileIndex, int32_t layerIndex)
{
    TileCell cell = GetCell(cellX, cellY, layerIndex);
    cell.mTileIndex = tileIndex;
    SetCell(cellX, cellY, cell, layerIndex);
}

int32_t TileMap::ReplaceTile(int32_t oldIndex, int32_t newIndex, int32_t layerIndex)
{
    TileMapLayer* layer = GetLayer(layerIndex);
    if (layer == nullptr) return 0;

    int32_t count = 0;
    for (auto& kv : layer->mChunks)
    {
        for (int32_t i = 0; i < kTileChunkSize * kTileChunkSize; ++i)
        {
            TileCell& cell = kv.second.mCells[i];
            if (cell.mTileIndex == oldIndex)
            {
                cell.mTileIndex = newIndex;
                ++count;
            }
        }
        if (count > 0)
            mDirtyChunks.insert(kv.first);
    }
#if EDITOR
    if (count > 0) SetDirtyFlag();
#endif
    return count;
}

int32_t TileMap::ReplaceTilesWithTag(const std::string& tag, int32_t newIndex, int32_t layerIndex)
{
    TileMapLayer* layer = GetLayer(layerIndex);
    if (layer == nullptr) return 0;

    TileSet* tileSet = GetTileSet();
    if (tileSet == nullptr) return 0;

    int32_t count = 0;
    for (auto& kv : layer->mChunks)
    {
        bool chunkDirty = false;
        for (int32_t i = 0; i < kTileChunkSize * kTileChunkSize; ++i)
        {
            TileCell& cell = kv.second.mCells[i];
            if (cell.mTileIndex >= 0 && tileSet->HasTileTag(cell.mTileIndex, tag))
            {
                cell.mTileIndex = newIndex;
                ++count;
                chunkDirty = true;
            }
        }
        if (chunkDirty)
            mDirtyChunks.insert(kv.first);
    }
#if EDITOR
    if (count > 0) SetDirtyFlag();
#endif
    return count;
}

int32_t TileMap::CountTileUses(int32_t tileIndex, int32_t layerIndex) const
{
    const TileMapLayer* layer = GetLayer(layerIndex);
    if (layer == nullptr) return 0;

    int32_t count = 0;
    for (const auto& kv : layer->mChunks)
    {
        for (int32_t i = 0; i < kTileChunkSize * kTileChunkSize; ++i)
        {
            if (kv.second.mCells[i].mTileIndex == tileIndex)
                ++count;
        }
    }
    return count;
}

void TileMap::MarkAllDirty()
{
    for (const TileMapLayer& layer : mLayers)
    {
        for (const auto& kv : layer.mChunks)
        {
            mDirtyChunks.insert(kv.first);
        }
    }
}

void TileMap::RecomputeUsedBounds()
{
    mHasContent = false;
    for (const TileMapLayer& layer : mLayers)
    {
        for (const auto& kv : layer.mChunks)
        {
            int32_t cx, cy;
            UnpackChunkKey(kv.first, cx, cy);
            int32_t baseX = cx * kTileChunkSize;
            int32_t baseY = cy * kTileChunkSize;

            for (int32_t ly = 0; ly < kTileChunkSize; ++ly)
            {
                for (int32_t lx = 0; lx < kTileChunkSize; ++lx)
                {
                    const TileCell& c = kv.second.mCells[ly * kTileChunkSize + lx];
                    if (c.mTileIndex < 0)
                        continue;

                    int32_t wx = baseX + lx;
                    int32_t wy = baseY + ly;
                    if (!mHasContent)
                    {
                        mMinUsed = { wx, wy };
                        mMaxUsed = { wx, wy };
                        mHasContent = true;
                    }
                    else
                    {
                        mMinUsed.x = std::min<int32_t>(mMinUsed.x, wx);
                        mMinUsed.y = std::min<int32_t>(mMinUsed.y, wy);
                        mMaxUsed.x = std::max<int32_t>(mMaxUsed.x, wx);
                        mMaxUsed.y = std::max<int32_t>(mMaxUsed.y, wy);
                    }
                }
            }
        }
    }
}

void TileMap::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteAsset(mTileSet);
    stream.WriteInt32(mTileSize.x);
    stream.WriteInt32(mTileSize.y);
    stream.WriteVec2(mOrigin);
    stream.WriteBool(mAllowNegativeCoords);

    uint32_t numLayers = uint32_t(mLayers.size());
    stream.WriteUint32(numLayers);
    for (uint32_t l = 0; l < numLayers; ++l)
    {
        const TileMapLayer& layer = mLayers[l];
        stream.WriteString(layer.mName);
        stream.WriteBool(layer.mVisible);
        stream.WriteBool(layer.mLocked);
        stream.WriteFloat(layer.mOpacity);
        stream.WriteUint8(uint8_t(layer.mType));
        stream.WriteInt32(layer.mZOrder);

        // Drop empty chunks before writing so we don't waste bytes on layers
        // that were touched and then cleared.
        uint32_t numNonEmpty = 0;
        for (const auto& kv : layer.mChunks)
        {
            const TileChunk& chunk = kv.second;
            for (int32_t i = 0; i < kTileChunkSize * kTileChunkSize; ++i)
            {
                if (chunk.mCells[i].mTileIndex >= 0)
                {
                    ++numNonEmpty;
                    break;
                }
            }
        }

        stream.WriteUint32(numNonEmpty);
        for (const auto& kv : layer.mChunks)
        {
            const TileChunk& chunk = kv.second;
            bool hasAny = false;
            for (int32_t i = 0; i < kTileChunkSize * kTileChunkSize; ++i)
            {
                if (chunk.mCells[i].mTileIndex >= 0)
                {
                    hasAny = true;
                    break;
                }
            }
            if (!hasAny)
                continue;

            int32_t cx, cy;
            UnpackChunkKey(kv.first, cx, cy);
            stream.WriteInt32(cx);
            stream.WriteInt32(cy);

            for (int32_t i = 0; i < kTileChunkSize * kTileChunkSize; ++i)
            {
                const TileCell& c = chunk.mCells[i];
                stream.WriteInt32(c.mTileIndex);
                stream.WriteUint8(c.mFlags);
                stream.WriteInt16(c.mVariant);
            }
        }
    }
}

void TileMap::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    if (mVersion < ASSET_VERSION_TILEMAP_BASE)
    {
        return;
    }

    stream.ReadAsset(mTileSet);
    mTileSize.x = stream.ReadInt32();
    mTileSize.y = stream.ReadInt32();
    mOrigin = stream.ReadVec2();
    mAllowNegativeCoords = stream.ReadBool();

    uint32_t numLayers = stream.ReadUint32();
    mLayers.clear();
    mLayers.resize(numLayers);
    for (uint32_t l = 0; l < numLayers; ++l)
    {
        TileMapLayer& layer = mLayers[l];
        stream.ReadString(layer.mName);
        layer.mVisible = stream.ReadBool();
        layer.mLocked = stream.ReadBool();
        layer.mOpacity = stream.ReadFloat();
        layer.mType = TileMapLayerType(stream.ReadUint8());
        layer.mZOrder = stream.ReadInt32();

        uint32_t numChunks = stream.ReadUint32();
        for (uint32_t c = 0; c < numChunks; ++c)
        {
            int32_t cx = stream.ReadInt32();
            int32_t cy = stream.ReadInt32();
            int64_t key = PackChunkKey(cx, cy);

            TileChunk chunk;
            for (int32_t i = 0; i < kTileChunkSize * kTileChunkSize; ++i)
            {
                TileCell& cell = chunk.mCells[i];
                cell.mTileIndex = stream.ReadInt32();
                cell.mFlags = stream.ReadUint8();
                cell.mVariant = stream.ReadInt16();
            }
            layer.mChunks.emplace(key, std::move(chunk));
        }
    }

    // Always have at least one layer (Phase 1 invariant).
    if (mLayers.empty())
    {
        mLayers.resize(1);
        mLayers[0].mName = "Main";
    }

    RecomputeUsedBounds();
    MarkAllDirty();
}
