#include "LuaBindings/TransformAnimationNode3d_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/TransformKeyframe_Lua.h"
#include "LuaBindings/LuaUtils.h"

#include "Assets/TransformAnimationAsset.h"
#include "Nodes/3D/Node3d.h"

#if LUA_ENABLED

int TransformAnimationNode3D_Lua::Play(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);

    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
    {
        Asset* asset = CHECK_ASSET(L, 2);
        TransformAnimationAsset* anim = asset ? asset->As<TransformAnimationAsset>() : nullptr;
        node->Play(anim);
    }
    else
    {
        node->Play();
    }

    return 0;
}

int TransformAnimationNode3D_Lua::Pause(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    node->Pause();
    return 0;
}

int TransformAnimationNode3D_Lua::Stop(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    node->StopPlayback();
    return 0;
}

int TransformAnimationNode3D_Lua::SetAnimation(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    TransformAnimationAsset* anim = nullptr;
    if (!lua_isnil(L, 2))
    {
        Asset* asset = CHECK_ASSET(L, 2);
        anim = asset ? asset->As<TransformAnimationAsset>() : nullptr;
    }
    node->SetAnimation(anim);
    return 0;
}

int TransformAnimationNode3D_Lua::GetAnimation(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    Asset_Lua::Create(L, node->GetAnimation());
    return 1;
}

int TransformAnimationNode3D_Lua::SetKeyframes(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);

    std::vector<TransformKeyframe> keyframes;
    if (lua_istable(L, 2))
    {
        lua_len(L, 2);
        int count = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        keyframes.reserve(count);
        for (int i = 1; i <= count; ++i)
        {
            lua_geti(L, 2, i);
            TransformKeyframe_Lua* kfLua = CheckLuaType<TransformKeyframe_Lua>(L, -1, TRANSFORM_KEYFRAME_LUA_NAME, false);
            if (kfLua != nullptr)
            {
                keyframes.push_back(kfLua->mKeyframe);
            }
            lua_pop(L, 1);
        }
    }

    node->SetKeyframes(keyframes);
    return 0;
}

int TransformAnimationNode3D_Lua::SetTargetNode(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    Node3D* target = nullptr;
    if (!lua_isnil(L, 2))
    {
        Node* n = CHECK_NODE(L, 2);
        if (n != nullptr && n->IsNode3D())
        {
            target = static_cast<Node3D*>(n);
        }
    }
    node->SetTargetNode(target);
    return 0;
}

int TransformAnimationNode3D_Lua::GetTargetNode(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    Node_Lua::Create(L, node->GetTargetNode());
    return 1;
}

int TransformAnimationNode3D_Lua::ApplyKeyframe(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    TransformKeyframe_Lua* kfLua = CheckLuaType<TransformKeyframe_Lua>(L, 2, TRANSFORM_KEYFRAME_LUA_NAME);
    node->ApplyKeyframe(kfLua->mKeyframe);
    return 0;
}

int TransformAnimationNode3D_Lua::SampleNow(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    TransformKeyframe kf = node->SampleNow();
    return TransformKeyframe_Lua::Create(L, kf);
}

int TransformAnimationNode3D_Lua::SetTime(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    node->SetTime((float)lua_tonumber(L, 2));
    return 0;
}

int TransformAnimationNode3D_Lua::GetTime(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    lua_pushnumber(L, node->GetTime());
    return 1;
}

int TransformAnimationNode3D_Lua::GetDuration(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    lua_pushnumber(L, node->GetDuration());
    return 1;
}

int TransformAnimationNode3D_Lua::IsPlaying(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    lua_pushboolean(L, node->IsPlaying());
    return 1;
}

int TransformAnimationNode3D_Lua::IsPaused(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    lua_pushboolean(L, node->IsPaused());
    return 1;
}

int TransformAnimationNode3D_Lua::GetProgress(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    lua_pushnumber(L, node->GetProgress());
    return 1;
}

int TransformAnimationNode3D_Lua::SetLoop(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    bool loop = CHECK_BOOLEAN(L, 2);
    node->SetLoop(loop);
    return 0;
}

int TransformAnimationNode3D_Lua::IsLooping(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    lua_pushboolean(L, node->IsLooping());
    return 1;
}

int TransformAnimationNode3D_Lua::SetPlayRate(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    node->SetPlayRate((float)lua_tonumber(L, 2));
    return 0;
}

int TransformAnimationNode3D_Lua::GetPlayRate(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    lua_pushnumber(L, node->GetPlayRate());
    return 1;
}

int TransformAnimationNode3D_Lua::SetPlayOnStart(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    bool play = CHECK_BOOLEAN(L, 2);
    node->SetPlayOnStart(play);
    return 0;
}

int TransformAnimationNode3D_Lua::GetPlayOnStart(lua_State* L)
{
    TransformAnimationNode3D* node = CHECK_TRANSFORM_ANIM_NODE3D(L, 1);
    lua_pushboolean(L, node->GetPlayOnStart());
    return 1;
}

void TransformAnimationNode3D_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        TRANSFORM_ANIM_NODE3D_LUA_NAME,
        TRANSFORM_ANIM_NODE3D_LUA_FLAG,
        NODE_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, Play);
    REGISTER_TABLE_FUNC(L, mtIndex, Pause);
    REGISTER_TABLE_FUNC(L, mtIndex, Stop);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, GetAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, SetKeyframes);
    REGISTER_TABLE_FUNC(L, mtIndex, SetTargetNode);
    REGISTER_TABLE_FUNC(L, mtIndex, GetTargetNode);
    REGISTER_TABLE_FUNC(L, mtIndex, ApplyKeyframe);
    REGISTER_TABLE_FUNC(L, mtIndex, SampleNow);
    REGISTER_TABLE_FUNC(L, mtIndex, SetTime);
    REGISTER_TABLE_FUNC(L, mtIndex, GetTime);
    REGISTER_TABLE_FUNC(L, mtIndex, GetDuration);
    REGISTER_TABLE_FUNC(L, mtIndex, IsPlaying);
    REGISTER_TABLE_FUNC(L, mtIndex, IsPaused);
    REGISTER_TABLE_FUNC(L, mtIndex, GetProgress);
    REGISTER_TABLE_FUNC(L, mtIndex, SetLoop);
    REGISTER_TABLE_FUNC(L, mtIndex, IsLooping);
    REGISTER_TABLE_FUNC(L, mtIndex, SetPlayRate);
    REGISTER_TABLE_FUNC(L, mtIndex, GetPlayRate);
    REGISTER_TABLE_FUNC(L, mtIndex, SetPlayOnStart);
    REGISTER_TABLE_FUNC(L, mtIndex, GetPlayOnStart);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
