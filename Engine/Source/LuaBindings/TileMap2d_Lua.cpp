#include "LuaBindings/TileMap2d_Lua.h"
#include "LuaBindings/Mesh3d_Lua.h"
#include "LuaBindings/Vector_Lua.h"
#include "LuaBindings/LuaUtils.h"

#include "Assets/TileMap.h"
#include "Assets/TileSet.h"

#include <algorithm>
#include <functional>
#include <vector>
#include <set>
#include <string>

#if LUA_ENABLED

// Helper: optional second integer arg defaulting to layer 0.
static int32_t GetOptionalLayer(lua_State* L, int argIndex)
{
    if (lua_isnone(L, argIndex) || lua_isnil(L, argIndex))
        return 0;
    return int32_t(lua_tointeger(L, argIndex));
}

// Helper: push a TileCell as a table { tileIndex, atlasX, atlasY, tags, hasCollision, worldX, worldY }
static void PushCellInfoTable(lua_State* L, TileMap2D* node, int32_t cellX, int32_t cellY, int32_t layer)
{
    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;

    TileCell cell = (tileMap != nullptr) ? tileMap->GetCell(cellX, cellY, layer) : TileCell{};
    glm::vec2 worldXY = node->CellToWorld({ cellX, cellY });
    glm::ivec2 atlasCoord = { -1, -1 };
    bool hasCollision = false;

    if (tileSet != nullptr && cell.mTileIndex >= 0)
    {
        atlasCoord = tileSet->TileIndexToAtlasCoord(cell.mTileIndex);
        const TileDefinition* def = tileSet->GetTileDef(cell.mTileIndex);
        if (def != nullptr)
            hasCollision = def->mHasCollision;
    }

    lua_newtable(L);
    lua_pushinteger(L, cell.mTileIndex);  lua_setfield(L, -2, "tileIndex");
    lua_pushinteger(L, atlasCoord.x);     lua_setfield(L, -2, "atlasX");
    lua_pushinteger(L, atlasCoord.y);     lua_setfield(L, -2, "atlasY");
    lua_pushinteger(L, cellX);            lua_setfield(L, -2, "cellX");
    lua_pushinteger(L, cellY);            lua_setfield(L, -2, "cellY");
    lua_pushnumber(L, worldXY.x);         lua_setfield(L, -2, "worldX");
    lua_pushnumber(L, worldXY.y);         lua_setfield(L, -2, "worldY");
    lua_pushboolean(L, hasCollision);     lua_setfield(L, -2, "hasCollision");
    lua_pushinteger(L, cell.mFlags);      lua_setfield(L, -2, "flags");

    // Tags array (if any)
    if (tileSet != nullptr && cell.mTileIndex >= 0)
    {
        const TileDefinition* def = tileSet->GetTileDef(cell.mTileIndex);
        if (def != nullptr)
        {
            lua_newtable(L);
            for (size_t t = 0; t < def->mTags.size(); ++t)
            {
                lua_pushinteger(L, int(t + 1));
                lua_pushstring(L, def->mTags[t].c_str());
                lua_settable(L, -3);
            }
            lua_setfield(L, -2, "tags");
        }
    }
}

int TileMap2D_Lua::WorldToCell(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    glm::vec3 worldPos = CHECK_VECTOR(L, 2);

    glm::ivec2 cell = node->WorldToCell(glm::vec2(worldPos.x, worldPos.y));

    lua_pushinteger(L, cell.x);
    lua_pushinteger(L, cell.y);
    return 2;
}

int TileMap2D_Lua::CellToWorld(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t cx = CHECK_INTEGER(L, 2);
    int32_t cy = CHECK_INTEGER(L, 3);

    glm::vec2 worldXY = node->CellToWorld({ cx, cy });
    Vector_Lua::Create(L, glm::vec4(worldXY.x, worldXY.y, 0.0f, 0.0f));
    return 1;
}

int TileMap2D_Lua::CellCenterToWorld(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t cx = CHECK_INTEGER(L, 2);
    int32_t cy = CHECK_INTEGER(L, 3);

    glm::vec2 worldXY = node->CellCenterToWorld({ cx, cy });
    Vector_Lua::Create(L, glm::vec4(worldXY.x, worldXY.y, 0.0f, 0.0f));
    return 1;
}

int TileMap2D_Lua::GetTile(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t cx = CHECK_INTEGER(L, 2);
    int32_t cy = CHECK_INTEGER(L, 3);
    int32_t layer = GetOptionalLayer(L, 4);

    int32_t tileIndex = -1;
    TileMap* tileMap = node->GetTileMap();
    if (tileMap != nullptr)
        tileIndex = tileMap->GetTile(cx, cy, layer);

    lua_pushinteger(L, tileIndex);
    return 1;
}

int TileMap2D_Lua::SetTile(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t cx = CHECK_INTEGER(L, 2);
    int32_t cy = CHECK_INTEGER(L, 3);
    int32_t tileIdx = CHECK_INTEGER(L, 4);
    int32_t layer = GetOptionalLayer(L, 5);

    TileMap* tileMap = node->GetTileMap();
    if (tileMap != nullptr)
    {
        tileMap->SetTile(cx, cy, tileIdx, layer);
        node->MarkDirty();
    }
    return 0;
}

int TileMap2D_Lua::ClearTile(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t cx = CHECK_INTEGER(L, 2);
    int32_t cy = CHECK_INTEGER(L, 3);
    int32_t layer = GetOptionalLayer(L, 4);

    TileMap* tileMap = node->GetTileMap();
    if (tileMap != nullptr)
    {
        tileMap->ClearCell(cx, cy, layer);
        node->MarkDirty();
    }
    return 0;
}

int TileMap2D_Lua::GetCell(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t cx = CHECK_INTEGER(L, 2);
    int32_t cy = CHECK_INTEGER(L, 3);
    int32_t layer = GetOptionalLayer(L, 4);

    PushCellInfoTable(L, node, cx, cy, layer);
    return 1;
}

int TileMap2D_Lua::IsCellOccupied(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t cx = CHECK_INTEGER(L, 2);
    int32_t cy = CHECK_INTEGER(L, 3);
    int32_t layer = GetOptionalLayer(L, 4);

    bool occupied = false;
    TileMap* tileMap = node->GetTileMap();
    if (tileMap != nullptr)
        occupied = tileMap->IsCellOccupied(cx, cy, layer);

    lua_pushboolean(L, occupied);
    return 1;
}

int TileMap2D_Lua::GetTileTags(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t tileIdx = CHECK_INTEGER(L, 2);

    lua_newtable(L);
    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileSet != nullptr)
    {
        const TileDefinition* def = tileSet->GetTileDef(tileIdx);
        if (def != nullptr)
        {
            for (size_t t = 0; t < def->mTags.size(); ++t)
            {
                lua_pushinteger(L, int(t + 1));
                lua_pushstring(L, def->mTags[t].c_str());
                lua_settable(L, -3);
            }
        }
    }
    return 1;
}

int TileMap2D_Lua::HasTileTag(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t tileIdx = CHECK_INTEGER(L, 2);
    const char* tag = CHECK_STRING(L, 3);

    bool has = false;
    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileSet != nullptr)
        has = tileSet->HasTileTag(tileIdx, tag);

    lua_pushboolean(L, has);
    return 1;
}

int TileMap2D_Lua::GetTileAtlasCoord(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t tileIdx = CHECK_INTEGER(L, 2);

    glm::ivec2 coord = { -1, -1 };
    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileSet != nullptr)
        coord = tileSet->TileIndexToAtlasCoord(tileIdx);

    lua_pushinteger(L, coord.x);
    lua_pushinteger(L, coord.y);
    return 2;
}

// Helper: walk all chunks of a layer and call fn(cellX, cellY, cell) for each
// non-empty cell. Used by FindAllTiles* below.
template <typename Fn>
static void ForEachOccupiedCell(TileMap* tileMap, int32_t layerIndex, Fn fn)
{
    if (tileMap == nullptr) return;
    const TileMapLayer* layer = tileMap->GetLayer(layerIndex);
    if (layer == nullptr) return;

    for (const auto& kv : layer->mChunks)
    {
        int32_t cx, cy;
        TileMap::UnpackChunkKey(kv.first, cx, cy);
        int32_t baseX = cx * kTileChunkSize;
        int32_t baseY = cy * kTileChunkSize;

        for (int32_t ly = 0; ly < kTileChunkSize; ++ly)
        {
            for (int32_t lx = 0; lx < kTileChunkSize; ++lx)
            {
                const TileCell& cell = kv.second.mCells[ly * kTileChunkSize + lx];
                if (cell.mTileIndex >= 0)
                {
                    fn(baseX + lx, baseY + ly, cell);
                }
            }
        }
    }
}

int TileMap2D_Lua::FindAllTilesWithTag(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    const char* tag = CHECK_STRING(L, 2);
    int32_t layer = GetOptionalLayer(L, 3);

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileMap == nullptr || tileSet == nullptr)
        return 1;

    int32_t count = 0;
    ForEachOccupiedCell(tileMap, layer, [&](int32_t cx, int32_t cy, const TileCell& cell)
    {
        if (tileSet->HasTileTag(cell.mTileIndex, tag))
        {
            ++count;
            lua_pushinteger(L, count);
            PushCellInfoTable(L, node, cx, cy, layer);
            lua_settable(L, tableIdx);
        }
    });

    return 1;
}

int TileMap2D_Lua::FindAllTiles(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t targetTileIdx = CHECK_INTEGER(L, 2);
    int32_t layer = GetOptionalLayer(L, 3);

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr)
        return 1;

    int32_t count = 0;
    ForEachOccupiedCell(tileMap, layer, [&](int32_t cx, int32_t cy, const TileCell& cell)
    {
        if (cell.mTileIndex == targetTileIdx)
        {
            ++count;
            lua_pushinteger(L, count);
            PushCellInfoTable(L, node, cx, cy, layer);
            lua_settable(L, tableIdx);
        }
    });

    return 1;
}

int TileMap2D_Lua::GetUsedBounds(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);

    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr || !tileMap->HasContent())
    {
        lua_pushinteger(L, 0); lua_pushinteger(L, 0);
        lua_pushinteger(L, 0); lua_pushinteger(L, 0);
        return 4;
    }

    glm::ivec2 minU = tileMap->GetMinUsed();
    glm::ivec2 maxU = tileMap->GetMaxUsed();
    lua_pushinteger(L, minU.x);
    lua_pushinteger(L, minU.y);
    lua_pushinteger(L, maxU.x);
    lua_pushinteger(L, maxU.y);
    return 4;
}

int TileMap2D_Lua::IsSolidAt(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    glm::vec3 worldPos = CHECK_VECTOR(L, 2);
    int32_t layer = GetOptionalLayer(L, 3);

    bool solid = false;
    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;

    if (tileMap != nullptr && tileSet != nullptr)
    {
        glm::ivec2 cell = node->WorldToCell(glm::vec2(worldPos.x, worldPos.y));
        int32_t tileIdx = tileMap->GetTile(cell.x, cell.y, layer);
        solid = tileSet->IsTileSolid(tileIdx);
    }

    lua_pushboolean(L, solid);
    return 1;
}

int TileMap2D_Lua::IsSolidCell(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t cx = CHECK_INTEGER(L, 2);
    int32_t cy = CHECK_INTEGER(L, 3);
    int32_t layer = GetOptionalLayer(L, 4);

    bool solid = false;
    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;

    if (tileMap != nullptr && tileSet != nullptr)
    {
        int32_t tileIdx = tileMap->GetTile(cx, cy, layer);
        solid = tileSet->IsTileSolid(tileIdx);
    }

    lua_pushboolean(L, solid);
    return 1;
}

int TileMap2D_Lua::FillRect(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t x1 = CHECK_INTEGER(L, 2);
    int32_t y1 = CHECK_INTEGER(L, 3);
    int32_t x2 = CHECK_INTEGER(L, 4);
    int32_t y2 = CHECK_INTEGER(L, 5);
    int32_t tileIdx = CHECK_INTEGER(L, 6);
    int32_t layer = GetOptionalLayer(L, 7);

    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return 0;

    int32_t minX = std::min(x1, x2);
    int32_t maxX = std::max(x1, x2);
    int32_t minY = std::min(y1, y2);
    int32_t maxY = std::max(y1, y2);

    for (int32_t cy = minY; cy <= maxY; ++cy)
    {
        for (int32_t cx = minX; cx <= maxX; ++cx)
        {
            tileMap->SetTile(cx, cy, tileIdx, layer);
        }
    }

    node->MarkDirty();
    return 0;
}

int TileMap2D_Lua::ClearRect(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t x1 = CHECK_INTEGER(L, 2);
    int32_t y1 = CHECK_INTEGER(L, 3);
    int32_t x2 = CHECK_INTEGER(L, 4);
    int32_t y2 = CHECK_INTEGER(L, 5);
    int32_t layer = GetOptionalLayer(L, 6);

    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return 0;

    int32_t minX = std::min(x1, x2);
    int32_t maxX = std::max(x1, x2);
    int32_t minY = std::min(y1, y2);
    int32_t maxY = std::max(y1, y2);

    for (int32_t cy = minY; cy <= maxY; ++cy)
    {
        for (int32_t cx = minX; cx <= maxX; ++cx)
        {
            tileMap->ClearCell(cx, cy, layer);
        }
    }

    node->MarkDirty();
    return 0;
}

// ---------------------------------------------------------------------------
// Phase 3 — region, neighbor, replace
// ---------------------------------------------------------------------------

int TileMap2D_Lua::GetCellsInRect(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t x1 = CHECK_INTEGER(L, 2);
    int32_t y1 = CHECK_INTEGER(L, 3);
    int32_t x2 = CHECK_INTEGER(L, 4);
    int32_t y2 = CHECK_INTEGER(L, 5);
    int32_t layer = GetOptionalLayer(L, 6);

    int32_t minX = std::min(x1, x2);
    int32_t maxX = std::max(x1, x2);
    int32_t minY = std::min(y1, y2);
    int32_t maxY = std::max(y1, y2);

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return 1;

    int32_t count = 0;
    for (int32_t cy = minY; cy <= maxY; ++cy)
    {
        for (int32_t cx = minX; cx <= maxX; ++cx)
        {
            if (!tileMap->IsCellOccupied(cx, cy, layer)) continue;
            ++count;
            lua_pushinteger(L, count);
            PushCellInfoTable(L, node, cx, cy, layer);
            lua_settable(L, tableIdx);
        }
    }
    return 1;
}

int TileMap2D_Lua::GetClosestTile(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    glm::vec3 worldPos = CHECK_VECTOR(L, 2);
    int32_t layer = GetOptionalLayer(L, 3);

    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr || !tileMap->HasContent())
    {
        lua_pushnil(L);
        return 1;
    }

    float bestDist = 1e30f;
    int32_t bestX = 0, bestY = 0;
    bool found = false;

    glm::ivec2 minU = tileMap->GetMinUsed();
    glm::ivec2 maxU = tileMap->GetMaxUsed();

    for (int32_t cy = minU.y; cy <= maxU.y; ++cy)
    {
        for (int32_t cx = minU.x; cx <= maxU.x; ++cx)
        {
            if (!tileMap->IsCellOccupied(cx, cy, layer)) continue;

            glm::vec2 worldXY = node->CellCenterToWorld({ cx, cy });
            float dx = worldXY.x - worldPos.x;
            float dy = worldXY.y - worldPos.y;
            float dist2 = dx * dx + dy * dy;
            if (dist2 < bestDist)
            {
                bestDist = dist2;
                bestX = cx;
                bestY = cy;
                found = true;
            }
        }
    }

    if (!found)
    {
        lua_pushnil(L);
        return 1;
    }

    PushCellInfoTable(L, node, bestX, bestY, layer);
    return 1;
}

// Helper that walks the used bounds, applying a predicate to each occupied
// cell, returning matches sorted by distance from worldPos and bounded by
// maxDistance.
static int PushClosestMatches(lua_State* L, TileMap2D* node, const glm::vec3& worldPos,
                              float maxDistance, int32_t layer,
                              std::function<bool(const TileCell&)> predicate)
{
    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr || !tileMap->HasContent())
        return 1;

    struct Hit { int32_t cx, cy; float dist2; };
    std::vector<Hit> hits;

    glm::ivec2 minU = tileMap->GetMinUsed();
    glm::ivec2 maxU = tileMap->GetMaxUsed();
    float maxDist2 = (maxDistance > 0.0f) ? (maxDistance * maxDistance) : 1e30f;

    for (int32_t cy = minU.y; cy <= maxU.y; ++cy)
    {
        for (int32_t cx = minU.x; cx <= maxU.x; ++cx)
        {
            TileCell cell = tileMap->GetCell(cx, cy, layer);
            if (cell.mTileIndex < 0) continue;
            if (!predicate(cell)) continue;

            glm::vec2 worldXY = node->CellCenterToWorld({ cx, cy });
            float dx = worldXY.x - worldPos.x;
            float dy = worldXY.y - worldPos.y;
            float dist2 = dx * dx + dy * dy;
            if (dist2 <= maxDist2)
                hits.push_back({ cx, cy, dist2 });
        }
    }

    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.dist2 < b.dist2; });

    for (size_t i = 0; i < hits.size(); ++i)
    {
        lua_pushinteger(L, int(i + 1));
        PushCellInfoTable(L, node, hits[i].cx, hits[i].cy, layer);
        lua_settable(L, tableIdx);
    }
    return 1;
}

int TileMap2D_Lua::GetClosestTilesWithTag(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    const char* tag = CHECK_STRING(L, 2);
    glm::vec3 worldPos = CHECK_VECTOR(L, 3);
    float maxDist = 0.0f;
    if (!lua_isnone(L, 4)) { maxDist = CHECK_NUMBER(L, 4); }
    int32_t layer = GetOptionalLayer(L, 5);

    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileSet == nullptr)
    {
        lua_newtable(L);
        return 1;
    }

    std::string tagStr(tag);
    return PushClosestMatches(L, node, worldPos, maxDist, layer,
        [tileSet, tagStr](const TileCell& cell) {
            return tileSet->HasTileTag(cell.mTileIndex, tagStr);
        });
}

int TileMap2D_Lua::GetClosestTilesOfType(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t targetTile = CHECK_INTEGER(L, 2);
    glm::vec3 worldPos = CHECK_VECTOR(L, 3);
    float maxDist = 0.0f;
    if (!lua_isnone(L, 4)) { maxDist = CHECK_NUMBER(L, 4); }
    int32_t layer = GetOptionalLayer(L, 5);

    return PushClosestMatches(L, node, worldPos, maxDist, layer,
        [targetTile](const TileCell& cell) {
            return cell.mTileIndex == targetTile;
        });
}

int TileMap2D_Lua::GetNeighborCells(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t cellX = CHECK_INTEGER(L, 2);
    int32_t cellY = CHECK_INTEGER(L, 3);
    bool includeDiagonals = false;
    if (!lua_isnone(L, 4)) includeDiagonals = lua_toboolean(L, 4) != 0;
    int32_t layer = GetOptionalLayer(L, 5);

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    static const int kOffsets8[8][2] = {
        { -1,  0 }, {  1,  0 }, {  0, -1 }, {  0,  1 },
        { -1, -1 }, {  1, -1 }, { -1,  1 }, {  1,  1 }
    };

    int32_t count = includeDiagonals ? 8 : 4;
    for (int32_t i = 0; i < count; ++i)
    {
        int32_t nx = cellX + kOffsets8[i][0];
        int32_t ny = cellY + kOffsets8[i][1];
        lua_pushinteger(L, i + 1);
        PushCellInfoTable(L, node, nx, ny, layer);
        lua_settable(L, tableIdx);
    }
    return 1;
}

int TileMap2D_Lua::GetCollisionAt(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    glm::vec3 worldPos = CHECK_VECTOR(L, 2);
    int32_t layer = GetOptionalLayer(L, 3);

    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileSet == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }

    glm::ivec2 cell = node->WorldToCell(glm::vec2(worldPos.x, worldPos.y));
    int32_t tileIdx = tileMap->GetTile(cell.x, cell.y, layer);
    const TileDefinition* def = tileSet->GetTileDef(tileIdx);
    if (def == nullptr || !def->mHasCollision)
    {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushinteger(L, cell.x); lua_setfield(L, -2, "cellX");
    lua_pushinteger(L, cell.y); lua_setfield(L, -2, "cellY");
    lua_pushinteger(L, tileIdx); lua_setfield(L, -2, "tileIndex");
    lua_pushboolean(L, true); lua_setfield(L, -2, "hasCollision");
    lua_pushinteger(L, int32_t(def->mCollisionType)); lua_setfield(L, -2, "collisionType");

    // Collision rects (tile-local pixel coords)
    if (!def->mCollisionRects.empty())
    {
        lua_newtable(L);
        for (size_t r = 0; r < def->mCollisionRects.size(); ++r)
        {
            lua_pushinteger(L, int(r + 1));
            lua_newtable(L);
            const glm::vec4& rect = def->mCollisionRects[r];
            lua_pushnumber(L, rect.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, rect.y); lua_setfield(L, -2, "y");
            lua_pushnumber(L, rect.z); lua_setfield(L, -2, "w");
            lua_pushnumber(L, rect.w); lua_setfield(L, -2, "h");
            lua_settable(L, -3);
        }
        lua_setfield(L, -2, "rects");
    }
    return 1;
}

int TileMap2D_Lua::ReplaceTile(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t oldIdx = CHECK_INTEGER(L, 2);
    int32_t newIdx = CHECK_INTEGER(L, 3);
    int32_t layer = GetOptionalLayer(L, 4);

    int32_t replaced = 0;
    TileMap* tileMap = node->GetTileMap();
    if (tileMap != nullptr)
    {
        replaced = tileMap->ReplaceTile(oldIdx, newIdx, layer);
        if (replaced > 0) node->MarkDirty();
    }
    lua_pushinteger(L, replaced);
    return 1;
}

int TileMap2D_Lua::ReplaceTilesWithTag(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    const char* tag = CHECK_STRING(L, 2);
    int32_t newIdx = CHECK_INTEGER(L, 3);
    int32_t layer = GetOptionalLayer(L, 4);

    int32_t replaced = 0;
    TileMap* tileMap = node->GetTileMap();
    if (tileMap != nullptr)
    {
        replaced = tileMap->ReplaceTilesWithTag(tag, newIdx, layer);
        if (replaced > 0) node->MarkDirty();
    }
    lua_pushinteger(L, replaced);
    return 1;
}

int TileMap2D_Lua::CountTileUses(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t tileIdx = CHECK_INTEGER(L, 2);
    int32_t layer = GetOptionalLayer(L, 3);

    int32_t count = 0;
    TileMap* tileMap = node->GetTileMap();
    if (tileMap != nullptr)
        count = tileMap->CountTileUses(tileIdx, layer);

    lua_pushinteger(L, count);
    return 1;
}

// ---------------------------------------------------------------------------
// Phase 4 — navigation helpers
// ---------------------------------------------------------------------------

int TileMap2D_Lua::RaycastTiles(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    glm::vec3 start = CHECK_VECTOR(L, 2);
    glm::vec3 end = CHECK_VECTOR(L, 3);
    int32_t layer = GetOptionalLayer(L, 4);

    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;

    auto pushHitResult = [&](bool hit, int32_t hitX, int32_t hitY,
                             float hitWorldX, float hitWorldY, int32_t tileIdx)
    {
        lua_newtable(L);
        lua_pushboolean(L, hit);                lua_setfield(L, -2, "hit");
        lua_pushinteger(L, hitX);               lua_setfield(L, -2, "cellX");
        lua_pushinteger(L, hitY);               lua_setfield(L, -2, "cellY");
        lua_pushnumber(L, hitWorldX);           lua_setfield(L, -2, "worldX");
        lua_pushnumber(L, hitWorldY);           lua_setfield(L, -2, "worldY");
        lua_pushinteger(L, tileIdx);            lua_setfield(L, -2, "tileIndex");
    };

    if (tileMap == nullptr || tileSet == nullptr)
    {
        pushHitResult(false, 0, 0, 0.0f, 0.0f, -1);
        return 1;
    }

    glm::ivec2 tileSize = tileMap->GetTileSize();
    if (tileSize.x <= 0 || tileSize.y <= 0)
    {
        pushHitResult(false, 0, 0, 0.0f, 0.0f, -1);
        return 1;
    }

    // 2D DDA in cell space.
    glm::ivec2 startCell = node->WorldToCell(glm::vec2(start.x, start.y));
    glm::ivec2 endCell = node->WorldToCell(glm::vec2(end.x, end.y));

    float dx = float(endCell.x - startCell.x);
    float dy = float(endCell.y - startCell.y);
    float steps = std::max(std::fabs(dx), std::fabs(dy));
    if (steps < 1.0f) steps = 1.0f;
    float stepX = dx / steps;
    float stepY = dy / steps;

    constexpr int32_t kMaxRaySteps = 1024;
    int32_t maxIter = std::min(int32_t(steps) + 1, kMaxRaySteps);

    float cx = float(startCell.x);
    float cy = float(startCell.y);
    int32_t lastCellX = startCell.x;
    int32_t lastCellY = startCell.y;

    for (int32_t i = 0; i <= maxIter; ++i)
    {
        int32_t ix = int32_t(std::floor(cx + 0.5f));
        int32_t iy = int32_t(std::floor(cy + 0.5f));
        lastCellX = ix;
        lastCellY = iy;

        int32_t tileIdx = tileMap->GetTile(ix, iy, layer);
        if (tileIdx >= 0 && tileSet->IsTileSolid(tileIdx))
        {
            glm::vec2 worldXY = node->CellCenterToWorld({ ix, iy });
            pushHitResult(true, ix, iy, worldXY.x, worldXY.y, tileIdx);
            return 1;
        }

        cx += stepX;
        cy += stepY;
    }

    glm::vec2 endWorld = node->CellCenterToWorld({ lastCellX, lastCellY });
    pushHitResult(false, lastCellX, lastCellY, endWorld.x, endWorld.y, -1);
    return 1;
}

int TileMap2D_Lua::GetReachableCells(lua_State* L)
{
    TileMap2D* node = CHECK_TILEMAP_2D(L, 1);
    int32_t startX = CHECK_INTEGER(L, 2);
    int32_t startY = CHECK_INTEGER(L, 3);
    int32_t maxDistance = CHECK_INTEGER(L, 4);
    const char* passTag = nullptr;
    if (!lua_isnone(L, 5) && !lua_isnil(L, 5))
        passTag = lua_tostring(L, 5);
    int32_t layer = GetOptionalLayer(L, 6);

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileMap == nullptr) return 1;

    // BFS, pruning by tag filter and capped by maxDistance (Manhattan).
    constexpr int32_t kMaxReachable = 4096;
    std::set<int64_t> visited;
    struct ReachNode { int32_t cx, cy, dist; };
    std::vector<ReachNode> queue;
    queue.reserve(64);
    queue.push_back({ startX, startY, 0 });

    int32_t emitted = 0;
    size_t qHead = 0;
    while (qHead < queue.size() && emitted < kMaxReachable)
    {
        ReachNode cur = queue[qHead++];
        int64_t key = (int64_t(uint32_t(cur.cx)) << 32) | int64_t(uint32_t(cur.cy));
        if (visited.count(key) > 0) continue;
        visited.insert(key);

        // Apply tag filter — if a passTag is given, the cell's tile must
        // either be empty (no tile) or carry that tag to be traversable.
        if (passTag != nullptr && tileSet != nullptr)
        {
            int32_t tileIdx = tileMap->GetTile(cur.cx, cur.cy, layer);
            if (tileIdx >= 0 && !tileSet->HasTileTag(tileIdx, passTag))
                continue;
        }

        ++emitted;
        lua_pushinteger(L, emitted);
        PushCellInfoTable(L, node, cur.cx, cur.cy, layer);
        lua_settable(L, tableIdx);

        if (cur.dist < maxDistance)
        {
            queue.push_back({ cur.cx + 1, cur.cy,     cur.dist + 1 });
            queue.push_back({ cur.cx - 1, cur.cy,     cur.dist + 1 });
            queue.push_back({ cur.cx,     cur.cy + 1, cur.dist + 1 });
            queue.push_back({ cur.cx,     cur.cy - 1, cur.dist + 1 });
        }
    }
    return 1;
}

void TileMap2D_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        TILEMAP_2D_LUA_NAME,
        TILEMAP_2D_LUA_FLAG,
        MESH_3D_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, WorldToCell);
    REGISTER_TABLE_FUNC(L, mtIndex, CellToWorld);
    REGISTER_TABLE_FUNC(L, mtIndex, CellCenterToWorld);

    REGISTER_TABLE_FUNC(L, mtIndex, GetTile);
    REGISTER_TABLE_FUNC(L, mtIndex, SetTile);
    REGISTER_TABLE_FUNC(L, mtIndex, ClearTile);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCell);
    REGISTER_TABLE_FUNC(L, mtIndex, IsCellOccupied);

    REGISTER_TABLE_FUNC(L, mtIndex, GetTileTags);
    REGISTER_TABLE_FUNC(L, mtIndex, HasTileTag);
    REGISTER_TABLE_FUNC(L, mtIndex, GetTileAtlasCoord);

    REGISTER_TABLE_FUNC(L, mtIndex, FindAllTilesWithTag);
    REGISTER_TABLE_FUNC(L, mtIndex, FindAllTiles);
    REGISTER_TABLE_FUNC(L, mtIndex, GetUsedBounds);

    REGISTER_TABLE_FUNC(L, mtIndex, IsSolidAt);
    REGISTER_TABLE_FUNC(L, mtIndex, IsSolidCell);

    REGISTER_TABLE_FUNC(L, mtIndex, FillRect);
    REGISTER_TABLE_FUNC(L, mtIndex, ClearRect);

    // Phase 3 — region, neighbor, replace
    REGISTER_TABLE_FUNC(L, mtIndex, GetCellsInRect);
    REGISTER_TABLE_FUNC(L, mtIndex, GetClosestTile);
    REGISTER_TABLE_FUNC(L, mtIndex, GetClosestTilesWithTag);
    REGISTER_TABLE_FUNC(L, mtIndex, GetClosestTilesOfType);
    REGISTER_TABLE_FUNC(L, mtIndex, GetNeighborCells);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCollisionAt);
    REGISTER_TABLE_FUNC(L, mtIndex, ReplaceTile);
    REGISTER_TABLE_FUNC(L, mtIndex, ReplaceTilesWithTag);
    REGISTER_TABLE_FUNC(L, mtIndex, CountTileUses);

    // Phase 4 — navigation helpers
    REGISTER_TABLE_FUNC(L, mtIndex, RaycastTiles);
    REGISTER_TABLE_FUNC(L, mtIndex, GetReachableCells);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
