#pragma once

#include "EngineTypes.h"
#include "Log.h"
#include "Engine.h"

#include "Nodes/3D/TileMap2d.h"

#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define TILEMAP_2D_LUA_NAME "TileMap2D"
#define TILEMAP_2D_LUA_FLAG "cfTileMap2D"
#define CHECK_TILEMAP_2D(L, arg) static_cast<TileMap2D*>(CheckNodeLuaType(L, arg, TILEMAP_2D_LUA_NAME, TILEMAP_2D_LUA_FLAG));

struct TileMap2D_Lua
{
    // Coordinate helpers
    static int WorldToCell(lua_State* L);
    static int CellToWorld(lua_State* L);
    static int CellCenterToWorld(lua_State* L);

    // Cell queries / mutation
    static int GetTile(lua_State* L);
    static int SetTile(lua_State* L);
    static int ClearTile(lua_State* L);
    static int GetCell(lua_State* L);
    static int IsCellOccupied(lua_State* L);

    // Tile-definition queries (forwarded to the bound TileSet)
    static int GetTileTags(lua_State* L);
    static int HasTileTag(lua_State* L);
    static int GetTileAtlasCoord(lua_State* L);

    // Region search helpers
    static int FindAllTilesWithTag(lua_State* L);
    static int FindAllTiles(lua_State* L);
    static int GetUsedBounds(lua_State* L);

    // Collision helpers (custom 2D AABB list, no Bullet)
    static int IsSolidAt(lua_State* L);
    static int IsSolidCell(lua_State* L);

    // Bulk editing helpers
    static int FillRect(lua_State* L);
    static int ClearRect(lua_State* L);

    // Phase 3 region & neighbor helpers
    static int GetCellsInRect(lua_State* L);
    static int GetClosestTile(lua_State* L);
    static int GetClosestTilesWithTag(lua_State* L);
    static int GetClosestTilesOfType(lua_State* L);
    static int GetNeighborCells(lua_State* L);
    static int GetCollisionAt(lua_State* L);

    // Phase 3 bulk replace
    static int ReplaceTile(lua_State* L);
    static int ReplaceTilesWithTag(lua_State* L);
    static int CountTileUses(lua_State* L);

    // Phase 4 nav helpers
    static int RaycastTiles(lua_State* L);
    static int GetReachableCells(lua_State* L);

    static void Bind();
};

#endif
