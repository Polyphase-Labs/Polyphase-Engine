#include "LuaBindings/SkeletalAnimationAsset_Lua.h"
#include "LuaBindings/Asset_Lua.h"

#if LUA_ENABLED

int SkeletalAnimationAsset_Lua::GetClipName(lua_State* L)
{
    SkeletalAnimationAsset* asset = CHECK_SKELETAL_ANIMATION_ASSET(L, 1);
    lua_pushstring(L, asset->GetClipName().c_str());
    return 1;
}

int SkeletalAnimationAsset_Lua::GetDuration(lua_State* L)
{
    SkeletalAnimationAsset* asset = CHECK_SKELETAL_ANIMATION_ASSET(L, 1);
    lua_pushnumber(L, asset->GetDuration());
    return 1;
}

int SkeletalAnimationAsset_Lua::GetDurationSeconds(lua_State* L)
{
    SkeletalAnimationAsset* asset = CHECK_SKELETAL_ANIMATION_ASSET(L, 1);
    lua_pushnumber(L, asset->GetDurationSeconds());
    return 1;
}

int SkeletalAnimationAsset_Lua::GetTicksPerSecond(lua_State* L)
{
    SkeletalAnimationAsset* asset = CHECK_SKELETAL_ANIMATION_ASSET(L, 1);
    lua_pushnumber(L, asset->GetTicksPerSecond());
    return 1;
}

int SkeletalAnimationAsset_Lua::GetSourceRigName(lua_State* L)
{
    SkeletalAnimationAsset* asset = CHECK_SKELETAL_ANIMATION_ASSET(L, 1);
    lua_pushstring(L, asset->GetSourceRigName().c_str());
    return 1;
}

int SkeletalAnimationAsset_Lua::GetNumChannels(lua_State* L)
{
    SkeletalAnimationAsset* asset = CHECK_SKELETAL_ANIMATION_ASSET(L, 1);
    lua_pushinteger(L, (int)asset->GetChannels().size());
    return 1;
}

int SkeletalAnimationAsset_Lua::GetChannelBoneName(lua_State* L)
{
    SkeletalAnimationAsset* asset = CHECK_SKELETAL_ANIMATION_ASSET(L, 1);
    int32_t index = CHECK_INDEX(L, 2);

    const std::vector<SkeletalAnimationChannel>& channels = asset->GetChannels();
    if (index >= 0 && uint32_t(index) < channels.size())
    {
        lua_pushstring(L, channels[index].mBoneName.c_str());
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

void SkeletalAnimationAsset_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        SKELETAL_ANIMATION_ASSET_LUA_NAME,
        SKELETAL_ANIMATION_ASSET_LUA_FLAG,
        ASSET_LUA_NAME);

    Asset_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, GetClipName);
    REGISTER_TABLE_FUNC(L, mtIndex, GetDuration);
    REGISTER_TABLE_FUNC(L, mtIndex, GetDurationSeconds);
    REGISTER_TABLE_FUNC(L, mtIndex, GetTicksPerSecond);
    REGISTER_TABLE_FUNC(L, mtIndex, GetSourceRigName);
    REGISTER_TABLE_FUNC(L, mtIndex, GetNumChannels);
    REGISTER_TABLE_FUNC(L, mtIndex, GetChannelBoneName);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
