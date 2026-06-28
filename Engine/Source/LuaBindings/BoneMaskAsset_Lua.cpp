#include "LuaBindings/BoneMaskAsset_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "Assets/SkeletalMesh.h"

#if LUA_ENABLED

int BoneMaskAsset_Lua::GetTargetMesh(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    Asset_Lua::Create(L, asset->GetTargetMesh());
    return 1;
}

int BoneMaskAsset_Lua::SetTargetMesh(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    SkeletalMesh* mesh = nullptr;
    if (!lua_isnil(L, 2))
    {
        Asset* a = CHECK_ASSET(L, 2);
        mesh = a ? a->As<SkeletalMesh>() : nullptr;
    }
    asset->SetTargetMesh(mesh);
    return 0;
}

int BoneMaskAsset_Lua::GetSelfOnly(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    lua_pushboolean(L, asset->GetSelfOnly());
    return 1;
}

int BoneMaskAsset_Lua::SetSelfOnly(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    bool selfOnly = CHECK_BOOLEAN(L, 2);
    asset->SetSelfOnly(selfOnly);
    return 0;
}

int BoneMaskAsset_Lua::GetNumIncludeRoots(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    lua_pushinteger(L, (int)asset->GetIncludeRoots().size());
    return 1;
}

int BoneMaskAsset_Lua::GetIncludeRoot(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    int32_t index = CHECK_INDEX(L, 2);
    const std::vector<std::string>& roots = asset->GetIncludeRoots();
    if (index >= 0 && uint32_t(index) < roots.size())
    {
        lua_pushstring(L, roots[index].c_str());
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int BoneMaskAsset_Lua::AddIncludeRoot(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    const char* name = CHECK_STRING(L, 2);
    asset->AddIncludeRoot(name);
    return 0;
}

int BoneMaskAsset_Lua::RemoveIncludeRoot(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    const char* name = CHECK_STRING(L, 2);
    asset->RemoveIncludeRoot(name);
    return 0;
}

int BoneMaskAsset_Lua::GetNumExcludeRoots(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    lua_pushinteger(L, (int)asset->GetExcludeRoots().size());
    return 1;
}

int BoneMaskAsset_Lua::GetExcludeRoot(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    int32_t index = CHECK_INDEX(L, 2);
    const std::vector<std::string>& roots = asset->GetExcludeRoots();
    if (index >= 0 && uint32_t(index) < roots.size())
    {
        lua_pushstring(L, roots[index].c_str());
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int BoneMaskAsset_Lua::AddExcludeRoot(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    const char* name = CHECK_STRING(L, 2);
    asset->AddExcludeRoot(name);
    return 0;
}

int BoneMaskAsset_Lua::RemoveExcludeRoot(lua_State* L)
{
    BoneMaskAsset* asset = CHECK_BONE_MASK_ASSET(L, 1);
    const char* name = CHECK_STRING(L, 2);
    asset->RemoveExcludeRoot(name);
    return 0;
}

void BoneMaskAsset_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        BONE_MASK_ASSET_LUA_NAME,
        BONE_MASK_ASSET_LUA_FLAG,
        ASSET_LUA_NAME);

    Asset_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, GetTargetMesh);
    REGISTER_TABLE_FUNC(L, mtIndex, SetTargetMesh);
    REGISTER_TABLE_FUNC(L, mtIndex, GetSelfOnly);
    REGISTER_TABLE_FUNC(L, mtIndex, SetSelfOnly);
    REGISTER_TABLE_FUNC(L, mtIndex, GetNumIncludeRoots);
    REGISTER_TABLE_FUNC(L, mtIndex, GetIncludeRoot);
    REGISTER_TABLE_FUNC(L, mtIndex, AddIncludeRoot);
    REGISTER_TABLE_FUNC(L, mtIndex, RemoveIncludeRoot);
    REGISTER_TABLE_FUNC(L, mtIndex, GetNumExcludeRoots);
    REGISTER_TABLE_FUNC(L, mtIndex, GetExcludeRoot);
    REGISTER_TABLE_FUNC(L, mtIndex, AddExcludeRoot);
    REGISTER_TABLE_FUNC(L, mtIndex, RemoveExcludeRoot);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
