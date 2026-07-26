#include "LuaBindings/SkeletalMesh_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Material_Lua.h"

#include "LuaBindings/Vector_Lua.h"

#if LUA_ENABLED

int SkeletalMesh_Lua::GetMaterial(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);

    Material* ret = mesh->GetMaterial();

    Asset_Lua::Create(L, ret);
    return 1;
}

int SkeletalMesh_Lua::SetMaterial(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);
    Material* material = nullptr;
    if (!lua_isnil(L, 2)) { material = CHECK_MATERIAL(L, 2); }

    mesh->SetMaterial(material);

    return 0;
}

int SkeletalMesh_Lua::GetNumIndices(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);

    uint32_t ret = mesh->GetNumIndices();

    lua_pushinteger(L, (int)ret);
    return 1;
}

int SkeletalMesh_Lua::GetNumFaces(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);

    uint32_t ret = mesh->GetNumFaces();

    lua_pushinteger(L, (int)ret);
    return 1;
}

int SkeletalMesh_Lua::GetNumVertices(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);

    uint32_t ret = mesh->GetNumVertices();

    lua_pushinteger(L, (int)ret);
    return 1;
}

int SkeletalMesh_Lua::FindBoneIndex(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);
    const char* name = CHECK_STRING(L, 2);

    int32_t index = 1 + mesh->FindBoneIndex(name);

    lua_pushinteger(L, index);
    return 1;
}

int SkeletalMesh_Lua::GetNumBones(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);

    uint32_t ret = mesh->GetNumBones();

    lua_pushinteger(L, (int)ret);
    return 1;
}

int SkeletalMesh_Lua::GetAnimationName(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);
    int32_t index = CHECK_INDEX(L, 2);

    const char* ret = nullptr;
    const std::vector<Animation>& animations = mesh->GetAnimations();
    if (index >= 0 &&
        index < int32_t(animations.size()))
    {
        ret = mesh->GetAnimations()[index].mName.c_str();
    }

    lua_pushstring(L, ret);
    return 1;
}

int SkeletalMesh_Lua::GetNumAnimations(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);

    uint32_t ret = (uint32_t) mesh->GetAnimations().size();

    lua_pushinteger(L, (int)ret);
    return 1;
}

int SkeletalMesh_Lua::GetAnimationDuration(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);
    const char* name = CHECK_STRING(L, 2);

    float ret =mesh->GetAnimationDuration(name);

    lua_pushnumber(L, ret);
    return 1;
}

int SkeletalMesh_Lua::GetNumSections(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);
    lua_pushinteger(L, (int)mesh->GetNumSections());
    return 1;
}

int SkeletalMesh_Lua::GetSectionName(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);
    int32_t index = CHECK_INDEX(L, 2);

    if (index >= 0 && uint32_t(index) < mesh->GetNumSections())
    {
        lua_pushstring(L, mesh->GetSection(uint32_t(index)).mName.c_str());
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int SkeletalMesh_Lua::GetSectionMaterial(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);
    int32_t index = CHECK_INDEX(L, 2);

    Material* mat = nullptr;
    if (index >= 0 && uint32_t(index) < mesh->GetNumSections())
    {
        mat = mesh->GetSectionMaterial(uint32_t(index));
    }

    Asset_Lua::Create(L, mat);
    return 1;
}

int SkeletalMesh_Lua::SetSectionMaterial(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);
    int32_t index = CHECK_INDEX(L, 2);
    Material* material = nullptr;
    if (!lua_isnil(L, 3)) { material = CHECK_MATERIAL(L, 3); }

    if (index >= 0 && uint32_t(index) < mesh->GetNumSections())
    {
        mesh->SetSectionMaterial(uint32_t(index), material);
    }
    return 0;
}

int SkeletalMesh_Lua::FindSectionIndex(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);
    const char* name = CHECK_STRING(L, 2);

    lua_pushinteger(L, 1 + mesh->FindSectionIndex(name));
    return 1;
}

int SkeletalMesh_Lua::GetBounds(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);

    Bounds bounds = mesh->GetBounds();
    AABB aabb = mesh->GetAABB();

    lua_newtable(L);
    int retTable = lua_gettop(L);

    Vector_Lua::Create(L, bounds.mCenter);
    lua_setfield(L, retTable, "center");
    lua_pushnumber(L, bounds.mRadius);
    lua_setfield(L, retTable, "radius");
    Vector_Lua::Create(L, aabb.mMin);
    lua_setfield(L, retTable, "min");
    Vector_Lua::Create(L, aabb.mMax);
    lua_setfield(L, retTable, "max");

    return 1;
}

int SkeletalMesh_Lua::GetAABB(lua_State* L)
{
    SkeletalMesh* mesh = CHECK_SKELETAL_MESH(L, 1);

    CreateAABBTableLua(L, mesh->GetAABB());

    return 1;
}

void SkeletalMesh_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        SKELETAL_MESH_LUA_NAME,
        SKELETAL_MESH_LUA_FLAG,
        ASSET_LUA_NAME);

    Asset_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, GetMaterial);

    REGISTER_TABLE_FUNC(L, mtIndex, SetMaterial);

    REGISTER_TABLE_FUNC(L, mtIndex, GetNumIndices);

    REGISTER_TABLE_FUNC(L, mtIndex, GetNumFaces);

    REGISTER_TABLE_FUNC(L, mtIndex, GetNumVertices);

    REGISTER_TABLE_FUNC(L, mtIndex, FindBoneIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, GetNumBones);

    REGISTER_TABLE_FUNC(L, mtIndex, GetAnimationName);

    REGISTER_TABLE_FUNC(L, mtIndex, GetNumAnimations);

    REGISTER_TABLE_FUNC(L, mtIndex, GetAnimationDuration);

    REGISTER_TABLE_FUNC(L, mtIndex, GetNumSections);

    REGISTER_TABLE_FUNC(L, mtIndex, GetSectionName);

    REGISTER_TABLE_FUNC(L, mtIndex, GetSectionMaterial);

    REGISTER_TABLE_FUNC(L, mtIndex, SetSectionMaterial);

    REGISTER_TABLE_FUNC(L, mtIndex, FindSectionIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, GetBounds);

    REGISTER_TABLE_FUNC(L, mtIndex, GetAABB);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
