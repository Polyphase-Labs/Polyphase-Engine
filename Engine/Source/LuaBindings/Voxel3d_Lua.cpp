#include "LuaBindings/Voxel3d_Lua.h"
#include "LuaBindings/Mesh3d_Lua.h"
#include "LuaBindings/Camera3d_Lua.h"
#include "LuaBindings/Vector_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Texture_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

int Voxel3D_Lua::SetVoxel(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    int32_t x = CHECK_INTEGER(L, 2);
    int32_t y = CHECK_INTEGER(L, 3);
    int32_t z = CHECK_INTEGER(L, 4);
    int32_t value = CHECK_INTEGER(L, 5);

    voxel->SetVoxel(x, y, z, static_cast<VoxelType>(value));

    return 0;
}

int Voxel3D_Lua::GetVoxel(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    int32_t x = CHECK_INTEGER(L, 2);
    int32_t y = CHECK_INTEGER(L, 3);
    int32_t z = CHECK_INTEGER(L, 4);

    VoxelType value = voxel->GetVoxel(x, y, z);

    lua_pushinteger(L, static_cast<int32_t>(value));
    return 1;
}

int Voxel3D_Lua::GetVoxelWorldPosition(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    int32_t x = CHECK_INTEGER(L, 2);
    int32_t y = CHECK_INTEGER(L, 3);
    int32_t z = CHECK_INTEGER(L, 4);

    glm::vec3 pos = voxel->GetVoxelWorldPosition(x, y, z);

    Vector_Lua::Create(L, glm::vec4(pos, 0.0f));
    return 1;
}

int Voxel3D_Lua::RayTest(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    glm::vec3 rayOrigin = CHECK_VECTOR(L, 2);
    glm::vec3 rayDir = CHECK_VECTOR(L, 3);
    float maxDist = 100.0f;
    if (!lua_isnone(L, 4)) { maxDist = CHECK_NUMBER(L, 4); }

    VoxelRayResult res = voxel->RayTest(rayOrigin, rayDir, maxDist);

    // Return: hit, vx, vy, vz, hitPos, prevX, prevY, prevZ, value
    lua_pushboolean(L, res.mHit);
    lua_pushinteger(L, res.mVoxel.x);
    lua_pushinteger(L, res.mVoxel.y);
    lua_pushinteger(L, res.mVoxel.z);
    Vector_Lua::Create(L, glm::vec4(res.mHitPosition, 0.0f));
    lua_pushinteger(L, res.mPrevVoxel.x);
    lua_pushinteger(L, res.mPrevVoxel.y);
    lua_pushinteger(L, res.mPrevVoxel.z);
    lua_pushinteger(L, res.mValue);
    return 9;
}

// Helper to push VoxelRayResult as 9 return values
static int PushRayResult(lua_State* L, const VoxelRayResult& res)
{
    lua_pushboolean(L, res.mHit);
    lua_pushinteger(L, res.mVoxel.x);
    lua_pushinteger(L, res.mVoxel.y);
    lua_pushinteger(L, res.mVoxel.z);
    Vector_Lua::Create(L, glm::vec4(res.mHitPosition, 0.0f));
    lua_pushinteger(L, res.mPrevVoxel.x);
    lua_pushinteger(L, res.mPrevVoxel.y);
    lua_pushinteger(L, res.mPrevVoxel.z);
    lua_pushinteger(L, res.mValue);
    return 9;
}

int Voxel3D_Lua::RayTestScreen(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    Camera3D* camera = CHECK_CAMERA_3D(L, 2);
    int32_t screenX = CHECK_INTEGER(L, 3);
    int32_t screenY = CHECK_INTEGER(L, 4);
    float maxDist = 100.0f;
    if (!lua_isnone(L, 5)) { maxDist = CHECK_NUMBER(L, 5); }

    VoxelRayResult res = voxel->RayTestScreen(camera, screenX, screenY, maxDist);
    return PushRayResult(L, res);
}

int Voxel3D_Lua::RayTestCenterCamera(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    Camera3D* camera = CHECK_CAMERA_3D(L, 2);
    float maxDist = 100.0f;
    if (!lua_isnone(L, 3)) { maxDist = CHECK_NUMBER(L, 3); }

    VoxelRayResult res = voxel->RayTestCenterCamera(camera, maxDist);
    return PushRayResult(L, res);
}

int Voxel3D_Lua::GetVoxelAtWorldPosition(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    glm::vec3 worldPos = CHECK_VECTOR(L, 2);

    VoxelPointResult res = voxel->GetVoxelAtWorldPosition(worldPos);

    // Return: valid, vx, vy, vz, worldPos, value
    lua_pushboolean(L, res.mValid);
    lua_pushinteger(L, res.mVoxel.x);
    lua_pushinteger(L, res.mVoxel.y);
    lua_pushinteger(L, res.mVoxel.z);
    Vector_Lua::Create(L, glm::vec4(res.mWorldPosition, 0.0f));
    lua_pushinteger(L, res.mValue);
    return 6;
}

int Voxel3D_Lua::Fill(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    int32_t value = CHECK_INTEGER(L, 2);

    voxel->Fill(static_cast<VoxelType>(value));

    return 0;
}

int Voxel3D_Lua::FillRegion(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    int32_t x0 = CHECK_INTEGER(L, 2);
    int32_t y0 = CHECK_INTEGER(L, 3);
    int32_t z0 = CHECK_INTEGER(L, 4);
    int32_t x1 = CHECK_INTEGER(L, 5);
    int32_t y1 = CHECK_INTEGER(L, 6);
    int32_t z1 = CHECK_INTEGER(L, 7);
    int32_t value = CHECK_INTEGER(L, 8);

    voxel->FillRegion(x0, y0, z0, x1, y1, z1, static_cast<VoxelType>(value));

    return 0;
}

// Helper: push a VoxelInfo as a Lua table {x, y, z, position, value}
static void PushVoxelInfo(lua_State* L, const VoxelInfo& info)
{
    lua_newtable(L);
    lua_pushinteger(L, info.mCoord.x); lua_setfield(L, -2, "x");
    lua_pushinteger(L, info.mCoord.y); lua_setfield(L, -2, "y");
    lua_pushinteger(L, info.mCoord.z); lua_setfield(L, -2, "z");
    Vector_Lua::Create(L, glm::vec4(info.mWorldPosition, 0.0f));
    lua_setfield(L, -2, "position");
    lua_pushinteger(L, info.mValue); lua_setfield(L, -2, "value");
}

// Helper: push a vector of VoxelInfo as a Lua array table
static int PushVoxelInfoArray(lua_State* L, const std::vector<VoxelInfo>& infos)
{
    lua_newtable(L);
    int tableIdx = lua_gettop(L);
    for (size_t i = 0; i < infos.size(); ++i)
    {
        lua_pushinteger(L, (int)i + 1);
        PushVoxelInfo(L, infos[i]);
        lua_settable(L, tableIdx);
    }
    return 1;
}

int Voxel3D_Lua::FillSphere(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    glm::vec3 center = CHECK_VECTOR(L, 2);
    float radius = CHECK_NUMBER(L, 3);
    int32_t value = CHECK_INTEGER(L, 4);

    voxel->FillSphere(center, radius, static_cast<VoxelType>(value));
    return 0;
}

int Voxel3D_Lua::FillCylinder(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    glm::vec3 center = CHECK_VECTOR(L, 2);
    float radius = CHECK_NUMBER(L, 3);
    float height = CHECK_NUMBER(L, 4);
    int32_t axis = CHECK_INTEGER(L, 5);
    int32_t value = CHECK_INTEGER(L, 6);

    voxel->FillCylinder(center, radius, height, axis, static_cast<VoxelType>(value));
    return 0;
}

int Voxel3D_Lua::GetVoxelsInSphere(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    glm::vec3 center = CHECK_VECTOR(L, 2);
    float radius = CHECK_NUMBER(L, 3);

    auto results = voxel->GetVoxelsInSphere(center, radius);
    return PushVoxelInfoArray(L, results);
}

int Voxel3D_Lua::GetVoxelsInBox(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    glm::vec3 worldMin = CHECK_VECTOR(L, 2);
    glm::vec3 worldMax = CHECK_VECTOR(L, 3);

    auto results = voxel->GetVoxelsInBox(worldMin, worldMax);
    return PushVoxelInfoArray(L, results);
}

int Voxel3D_Lua::GetVoxelsInCylinder(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    glm::vec3 center = CHECK_VECTOR(L, 2);
    float radius = CHECK_NUMBER(L, 3);
    float height = CHECK_NUMBER(L, 4);
    int32_t axis = CHECK_INTEGER(L, 5);

    auto results = voxel->GetVoxelsInCylinder(center, radius, height, axis);
    return PushVoxelInfoArray(L, results);
}

int Voxel3D_Lua::GetVoxelNeighbors(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    int32_t x = CHECK_INTEGER(L, 2);
    int32_t y = CHECK_INTEGER(L, 3);
    int32_t z = CHECK_INTEGER(L, 4);

    auto results = voxel->GetVoxelNeighbors(x, y, z);
    return PushVoxelInfoArray(L, results);
}

int Voxel3D_Lua::MarkDirty(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);

    voxel->MarkDirty();

    return 0;
}

int Voxel3D_Lua::RebuildMesh(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);

    voxel->RebuildMesh();

    return 0;
}

int Voxel3D_Lua::IsDirty(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);

    bool dirty = voxel->IsDirty();

    lua_pushboolean(L, dirty);
    return 1;
}

int Voxel3D_Lua::GetDimensions(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);

    glm::ivec3 dims = voxel->GetDimensions();

    Vector_Lua::Create(L, glm::vec4(dims.x, dims.y, dims.z, 0.0f));
    return 1;
}

int Voxel3D_Lua::SetAtlasTexture(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    Texture* texture = nullptr;
    if (!lua_isnil(L, 2))
    {
        texture = CHECK_TEXTURE(L, 2);
    }

    uint32_t tilesX = 16;
    uint32_t tilesY = 16;
    if (lua_gettop(L) >= 3)
    {
        tilesX = static_cast<uint32_t>(lua_tointeger(L, 3));
    }
    if (lua_gettop(L) >= 4)
    {
        tilesY = static_cast<uint32_t>(lua_tointeger(L, 4));
    }

    voxel->SetAtlasTexture(texture, tilesX, tilesY);

    return 0;
}

int Voxel3D_Lua::GetAtlasTexture(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);

    Texture* texture = voxel->GetAtlasTexture();

    Asset_Lua::Create(L, texture);
    return 1;
}

int Voxel3D_Lua::SetAtlasEnabled(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    bool enabled = lua_toboolean(L, 2) != 0;

    voxel->SetAtlasEnabled(enabled);

    return 0;
}

int Voxel3D_Lua::IsAtlasEnabled(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);

    bool enabled = voxel->IsAtlasEnabled();

    lua_pushboolean(L, enabled);
    return 1;
}

int Voxel3D_Lua::SetMaterialTexture(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    int32_t materialId = static_cast<int32_t>(lua_tointeger(L, 2));

    int numArgs = lua_gettop(L);
    if (numArgs >= 5)
    {
        // 4-arg version: (id, top, bottom, side)
        int32_t topTile = static_cast<int32_t>(lua_tointeger(L, 3));
        int32_t bottomTile = static_cast<int32_t>(lua_tointeger(L, 4));
        int32_t sideTile = static_cast<int32_t>(lua_tointeger(L, 5));
        voxel->SetMaterialTexture(static_cast<VoxelType>(materialId), topTile, bottomTile, sideTile);
    }
    else
    {
        // 2-arg version: (id, allFacesTile)
        int32_t tile = static_cast<int32_t>(lua_tointeger(L, 3));
        voxel->SetMaterialTexture(static_cast<VoxelType>(materialId), tile);
    }

    return 0;
}

int Voxel3D_Lua::SetMaterialTint(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    int32_t materialId = static_cast<int32_t>(lua_tointeger(L, 2));
    glm::vec4 tint = CHECK_VECTOR(L, 3);

    voxel->SetMaterialTint(static_cast<VoxelType>(materialId), tint);

    return 0;
}

int Voxel3D_Lua::DisableMaterialTexture(lua_State* L)
{
    Voxel3D* voxel = CHECK_VOXEL_3D(L, 1);
    int32_t materialId = static_cast<int32_t>(lua_tointeger(L, 2));

    voxel->DisableMaterialTexture(static_cast<VoxelType>(materialId));

    return 0;
}

void Voxel3D_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        VOXEL_3D_LUA_NAME,
        VOXEL_3D_LUA_FLAG,
        MESH_3D_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, SetVoxel);
    REGISTER_TABLE_FUNC(L, mtIndex, GetVoxel);
    REGISTER_TABLE_FUNC(L, mtIndex, GetVoxelWorldPosition);
    REGISTER_TABLE_FUNC(L, mtIndex, RayTest);
    REGISTER_TABLE_FUNC(L, mtIndex, RayTestScreen);
    REGISTER_TABLE_FUNC(L, mtIndex, RayTestCenterCamera);
    REGISTER_TABLE_FUNC(L, mtIndex, GetVoxelAtWorldPosition);
    REGISTER_TABLE_FUNC(L, mtIndex, Fill);
    REGISTER_TABLE_FUNC(L, mtIndex, FillRegion);
    REGISTER_TABLE_FUNC(L, mtIndex, FillSphere);
    REGISTER_TABLE_FUNC(L, mtIndex, FillCylinder);
    REGISTER_TABLE_FUNC(L, mtIndex, GetVoxelsInSphere);
    REGISTER_TABLE_FUNC(L, mtIndex, GetVoxelsInBox);
    REGISTER_TABLE_FUNC(L, mtIndex, GetVoxelsInCylinder);
    REGISTER_TABLE_FUNC(L, mtIndex, GetVoxelNeighbors);
    REGISTER_TABLE_FUNC(L, mtIndex, MarkDirty);
    REGISTER_TABLE_FUNC(L, mtIndex, RebuildMesh);
    REGISTER_TABLE_FUNC(L, mtIndex, IsDirty);
    REGISTER_TABLE_FUNC(L, mtIndex, GetDimensions);

    // Atlas texturing
    REGISTER_TABLE_FUNC(L, mtIndex, SetAtlasTexture);
    REGISTER_TABLE_FUNC(L, mtIndex, GetAtlasTexture);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAtlasEnabled);
    REGISTER_TABLE_FUNC(L, mtIndex, IsAtlasEnabled);
    REGISTER_TABLE_FUNC(L, mtIndex, SetMaterialTexture);
    REGISTER_TABLE_FUNC(L, mtIndex, SetMaterialTint);
    REGISTER_TABLE_FUNC(L, mtIndex, DisableMaterialTexture);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
