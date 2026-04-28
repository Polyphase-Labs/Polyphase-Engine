#include "LuaBindings/Terrain3d_Lua.h"
#include "LuaBindings/Mesh3d_Lua.h"
#include "LuaBindings/Vector_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Texture_Lua.h"
#include "LuaBindings/MaterialLite_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

int Terrain3D_Lua::SetHeight(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    int32_t x = CHECK_INTEGER(L, 2);
    int32_t z = CHECK_INTEGER(L, 3);
    float height = CHECK_NUMBER(L, 4);

    terrain->SetHeight(x, z, height);

    return 0;
}

int Terrain3D_Lua::GetHeight(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    int32_t x = CHECK_INTEGER(L, 2);
    int32_t z = CHECK_INTEGER(L, 3);

    lua_pushnumber(L, terrain->GetHeight(x, z));
    return 1;
}

int Terrain3D_Lua::GetHeightAtWorldPos(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    float worldX = CHECK_NUMBER(L, 2);
    float worldZ = CHECK_NUMBER(L, 3);

    lua_pushnumber(L, terrain->GetHeightAtWorldPos(worldX, worldZ));
    return 1;
}

int Terrain3D_Lua::SetHeightFromTexture(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    Texture* texture = nullptr;
    if (!lua_isnil(L, 2))
    {
        texture = CHECK_TEXTURE(L, 2);
    }

    terrain->SetHeightFromTexture(texture);

    return 0;
}

int Terrain3D_Lua::FlattenAll(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    float height = 0.0f;
    if (lua_gettop(L) >= 2)
    {
        height = CHECK_NUMBER(L, 2);
    }

    terrain->FlattenAll(height);

    return 0;
}

int Terrain3D_Lua::Resize(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    int32_t resX = CHECK_INTEGER(L, 2);
    int32_t resZ = CHECK_INTEGER(L, 3);
    float worldW = CHECK_NUMBER(L, 4);
    float worldD = CHECK_NUMBER(L, 5);

    terrain->Resize(resX, resZ, worldW, worldD);

    return 0;
}

int Terrain3D_Lua::GetResolution(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);

    lua_pushinteger(L, terrain->GetResolutionX());
    lua_pushinteger(L, terrain->GetResolutionZ());
    return 2;
}

int Terrain3D_Lua::GetWorldSize(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);

    lua_pushnumber(L, terrain->GetWorldWidth());
    lua_pushnumber(L, terrain->GetWorldDepth());
    return 2;
}

int Terrain3D_Lua::GetHeightScale(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);

    lua_pushnumber(L, terrain->mHeightScale);
    return 1;
}

int Terrain3D_Lua::SetHeightScale(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    float scale = CHECK_NUMBER(L, 2);

    terrain->mHeightScale = scale;
    terrain->MarkDirty();

    return 0;
}

int Terrain3D_Lua::MarkDirty(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    terrain->MarkDirty();
    return 0;
}

int Terrain3D_Lua::RebuildMesh(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    terrain->RebuildMesh();
    return 0;
}

int Terrain3D_Lua::IsDirty(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    lua_pushboolean(L, terrain->IsDirty());
    return 1;
}

int Terrain3D_Lua::SetMaterialSlot(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    int32_t slot = CHECK_INTEGER(L, 2);
    Material* mat = nullptr;
    if (!lua_isnil(L, 3))
    {
        mat = CHECK_MATERIAL_LITE(L, 3);
    }

    terrain->SetMaterialSlot(slot, mat);

    return 0;
}

int Terrain3D_Lua::GetMaterialSlot(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    int32_t slot = CHECK_INTEGER(L, 2);

    Material* mat = terrain->GetMaterialSlot(slot);
    Asset_Lua::Create(L, mat);
    return 1;
}

int Terrain3D_Lua::SetMaterialWeight(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    int32_t x = CHECK_INTEGER(L, 2);
    int32_t z = CHECK_INTEGER(L, 3);
    int32_t slot = CHECK_INTEGER(L, 4);
    float weight = CHECK_NUMBER(L, 5);

    terrain->SetMaterialWeight(x, z, slot, weight);

    return 0;
}

int Terrain3D_Lua::GetMaterialWeight(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    int32_t x = CHECK_INTEGER(L, 2);
    int32_t z = CHECK_INTEGER(L, 3);
    int32_t slot = CHECK_INTEGER(L, 4);

    lua_pushnumber(L, terrain->GetMaterialWeight(x, z, slot));
    return 1;
}

int Terrain3D_Lua::SetSnapGridSize(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);
    float size = CHECK_NUMBER(L, 2);

    terrain->mSnapGridSize = size;

    return 0;
}

int Terrain3D_Lua::GetSnapGridSize(lua_State* L)
{
    Terrain3D* terrain = CHECK_TERRAIN_3D(L, 1);

    lua_pushnumber(L, terrain->mSnapGridSize);
    return 1;
}

void Terrain3D_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        TERRAIN_3D_LUA_NAME,
        TERRAIN_3D_LUA_FLAG,
        MESH_3D_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    // Heightmap
    REGISTER_TABLE_FUNC(L, mtIndex, SetHeight);
    REGISTER_TABLE_FUNC(L, mtIndex, GetHeight);
    REGISTER_TABLE_FUNC(L, mtIndex, GetHeightAtWorldPos);
    REGISTER_TABLE_FUNC(L, mtIndex, SetHeightFromTexture);
    REGISTER_TABLE_FUNC(L, mtIndex, FlattenAll);
    REGISTER_TABLE_FUNC(L, mtIndex, Resize);

    // Dimensions
    REGISTER_TABLE_FUNC(L, mtIndex, GetResolution);
    REGISTER_TABLE_FUNC(L, mtIndex, GetWorldSize);
    REGISTER_TABLE_FUNC(L, mtIndex, GetHeightScale);
    REGISTER_TABLE_FUNC(L, mtIndex, SetHeightScale);

    // Mesh control
    REGISTER_TABLE_FUNC(L, mtIndex, MarkDirty);
    REGISTER_TABLE_FUNC(L, mtIndex, RebuildMesh);
    REGISTER_TABLE_FUNC(L, mtIndex, IsDirty);

    // Material slots
    REGISTER_TABLE_FUNC(L, mtIndex, SetMaterialSlot);
    REGISTER_TABLE_FUNC(L, mtIndex, GetMaterialSlot);

    // Splatmap
    REGISTER_TABLE_FUNC(L, mtIndex, SetMaterialWeight);
    REGISTER_TABLE_FUNC(L, mtIndex, GetMaterialWeight);

    // Snapping
    REGISTER_TABLE_FUNC(L, mtIndex, SetSnapGridSize);
    REGISTER_TABLE_FUNC(L, mtIndex, GetSnapGridSize);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
