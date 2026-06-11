#include "LuaBindings/TransformAnimationAsset_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/TransformKeyframe_Lua.h"

#if LUA_ENABLED

int TransformAnimationAsset_Lua::Sample(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    float time = (float)lua_tonumber(L, 2);
    TransformKeyframe kf = asset->Sample(time);
    return TransformKeyframe_Lua::Create(L, kf);
}

int TransformAnimationAsset_Lua::GetDuration(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    lua_pushnumber(L, asset->GetDuration());
    return 1;
}

int TransformAnimationAsset_Lua::SetDuration(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    asset->SetDuration((float)lua_tonumber(L, 2));
    return 0;
}

int TransformAnimationAsset_Lua::GetKeyframeCount(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    lua_pushinteger(L, (int)asset->GetKeyframeCount());
    return 1;
}

int TransformAnimationAsset_Lua::GetKeyframe(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    int idx = (int)lua_tointeger(L, 2);
    // Lua is 1-indexed
    size_t cidx = (size_t)(idx - 1);
    if (cidx >= asset->GetKeyframeCount())
    {
        lua_pushnil(L);
        return 1;
    }
    return TransformKeyframe_Lua::Create(L, asset->GetKeyframe(cidx));
}

int TransformAnimationAsset_Lua::IsLooping(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    lua_pushboolean(L, asset->IsLooping());
    return 1;
}

int TransformAnimationAsset_Lua::SetLooping(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    asset->SetLooping(lua_toboolean(L, 2));
    return 0;
}

int TransformAnimationAsset_Lua::GetPlayRate(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    lua_pushnumber(L, asset->GetPlayRate());
    return 1;
}

int TransformAnimationAsset_Lua::SetPlayRate(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    asset->SetPlayRate((float)lua_tonumber(L, 2));
    return 0;
}

int TransformAnimationAsset_Lua::AddKeyframe(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    TransformKeyframe_Lua* kfLua = CheckLuaType<TransformKeyframe_Lua>(L, 2, TRANSFORM_KEYFRAME_LUA_NAME);
    asset->AddKeyframe(kfLua->mKeyframe);
    return 0;
}

int TransformAnimationAsset_Lua::RemoveKeyframe(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    int idx = (int)lua_tointeger(L, 2);
    asset->RemoveKeyframe((size_t)(idx - 1));
    return 0;
}

int TransformAnimationAsset_Lua::ClearKeyframes(lua_State* L)
{
    TransformAnimationAsset* asset = CHECK_TRANSFORM_ANIMATION_ASSET(L, 1);
    asset->ClearKeyframes();
    return 0;
}

void TransformAnimationAsset_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        TRANSFORM_ANIMATION_ASSET_LUA_NAME,
        TRANSFORM_ANIMATION_ASSET_LUA_FLAG,
        ASSET_LUA_NAME);

    Asset_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, Sample);
    REGISTER_TABLE_FUNC(L, mtIndex, GetDuration);
    REGISTER_TABLE_FUNC(L, mtIndex, SetDuration);
    REGISTER_TABLE_FUNC(L, mtIndex, GetKeyframeCount);
    REGISTER_TABLE_FUNC(L, mtIndex, GetKeyframe);
    REGISTER_TABLE_FUNC(L, mtIndex, IsLooping);
    REGISTER_TABLE_FUNC(L, mtIndex, SetLooping);
    REGISTER_TABLE_FUNC(L, mtIndex, GetPlayRate);
    REGISTER_TABLE_FUNC(L, mtIndex, SetPlayRate);
    REGISTER_TABLE_FUNC(L, mtIndex, AddKeyframe);
    REGISTER_TABLE_FUNC(L, mtIndex, RemoveKeyframe);
    REGISTER_TABLE_FUNC(L, mtIndex, ClearKeyframes);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
