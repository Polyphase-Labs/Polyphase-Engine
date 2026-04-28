#include "LuaBindings/AnimatedWidget_Lua.h"
#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Texture_Lua.h"
#include "LuaBindings/SpriteAnimation_Lua.h"
#include "LuaBindings/LuaUtils.h"

#include "Assets/Texture.h"
#include "Assets/SpriteAnimation.h"

#if LUA_ENABLED

int AnimatedWidget_Lua::Play(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    w->Play();
    return 0;
}

int AnimatedWidget_Lua::Pause(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    w->Pause();
    return 0;
}

int AnimatedWidget_Lua::Stop(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    w->Stop();
    return 0;
}

int AnimatedWidget_Lua::PlayAnimation(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    const char* name = CHECK_STRING(L, 2);
    w->PlayAnimation(name ? name : "");
    return 0;
}

int AnimatedWidget_Lua::IsPlaying(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    lua_pushboolean(L, w->IsPlaying() ? 1 : 0);
    return 1;
}

int AnimatedWidget_Lua::SetFrame(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    int32_t frame = (int32_t)CHECK_INTEGER(L, 2);
    w->SetFrame(frame);
    return 0;
}

int AnimatedWidget_Lua::GetProgress(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    lua_pushnumber(L, w->GetProgress());
    return 1;
}

int AnimatedWidget_Lua::AnimateTo(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    int32_t target = (int32_t)CHECK_INTEGER(L, 2);
    bool pauseOnFinished = (lua_gettop(L) >= 3) ? (lua_toboolean(L, 3) != 0) : true;
    ScriptFunc cb;
    if (lua_gettop(L) >= 4 && lua_isfunction(L, 4)) cb = ScriptFunc(L, 4);
    w->AnimateTo(target, pauseOnFinished, cb);
    return 0;
}

int AnimatedWidget_Lua::AnimateToProgress(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    float progress = (float)CHECK_NUMBER(L, 2);
    bool pauseOnFinished = (lua_gettop(L) >= 3) ? (lua_toboolean(L, 3) != 0) : true;
    ScriptFunc cb;
    if (lua_gettop(L) >= 4 && lua_isfunction(L, 4)) cb = ScriptFunc(L, 4);
    w->AnimateToProgress(progress, pauseOnFinished, cb);
    return 0;
}

int AnimatedWidget_Lua::CancelAnimateTo(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    w->CancelAnimateTo();
    return 0;
}

int AnimatedWidget_Lua::SetSpeed(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    float v = (float)CHECK_NUMBER(L, 2);
    w->SetSpeed(v);
    return 0;
}

int AnimatedWidget_Lua::GetSpeed(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    lua_pushnumber(L, w->GetSpeed());
    return 1;
}

int AnimatedWidget_Lua::SetAutoPlay(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    w->SetAutoPlay(v);
    return 0;
}

int AnimatedWidget_Lua::GetAutoPlay(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    lua_pushboolean(L, w->GetAutoPlay() ? 1 : 0);
    return 1;
}

int AnimatedWidget_Lua::SetLoopOverride(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    w->SetLoopOverride(v);
    return 0;
}

int AnimatedWidget_Lua::GetLoopOverride(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    lua_pushboolean(L, w->GetLoopOverride() ? 1 : 0);
    return 1;
}

int AnimatedWidget_Lua::SetDefaultAnimation(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    const char* name = CHECK_STRING(L, 2);
    w->SetDefaultAnimation(name ? name : "");
    return 0;
}

int AnimatedWidget_Lua::GetDefaultAnimation(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    lua_pushstring(L, w->GetDefaultAnimation().c_str());
    return 1;
}

int AnimatedWidget_Lua::AddAnimation(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);

    if (lua_isstring(L, 2))
    {
        const char* path = lua_tostring(L, 2);
        w->AddAnimation(std::string(path ? path : ""));
    }
    else
    {
        SpriteAnimation* asset = CheckAssetLuaType<SpriteAnimation>(L, 2,
            SPRITE_ANIMATION_LUA_NAME, SPRITE_ANIMATION_LUA_FLAG);
        w->AddAnimation(asset);
    }
    return 0;
}

int AnimatedWidget_Lua::CreateAnimation(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    const char* name = CHECK_STRING(L, 2);

    if (lua_gettop(L) >= 3 && lua_istable(L, 3))
    {
        std::vector<Texture*> textures;
        const int n = (int)lua_rawlen(L, 3);
        textures.reserve(n);
        for (int i = 1; i <= n; ++i)
        {
            lua_rawgeti(L, 3, i);
            Texture* tex = CheckAssetLuaType<Texture>(L, lua_gettop(L),
                TEXTURE_LUA_NAME, TEXTURE_LUA_FLAG, false);
            if (tex != nullptr) textures.push_back(tex);
            lua_pop(L, 1);
        }
        w->CreateAnimation(name ? name : "", textures);
    }
    else
    {
        w->CreateAnimation(name ? name : "");
    }
    return 0;
}

int AnimatedWidget_Lua::AddImage(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    const char* name = CHECK_STRING(L, 2);

    if (lua_isstring(L, 3))
    {
        const char* path = lua_tostring(L, 3);
        w->AddImage(std::string(name ? name : ""), std::string(path ? path : ""));
    }
    else
    {
        Texture* tex = CheckAssetLuaType<Texture>(L, 3, TEXTURE_LUA_NAME, TEXTURE_LUA_FLAG);
        w->AddImage(std::string(name ? name : ""), tex);
    }
    return 0;
}

int AnimatedWidget_Lua::AddImages(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    const char* name = CHECK_STRING(L, 2);
    CHECK_TABLE(L, 3);

    std::vector<std::string> paths;
    const int n = (int)lua_rawlen(L, 3);
    paths.reserve(n);
    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti(L, 3, i);
        if (lua_isstring(L, -1)) paths.push_back(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    w->AddImages(std::string(name ? name : ""), paths);
    return 0;
}

int AnimatedWidget_Lua::RemoveAnimation(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    const char* name = CHECK_STRING(L, 2);
    w->RemoveAnimation(name ? name : "");
    return 0;
}

int AnimatedWidget_Lua::HasAnimation(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    const char* name = CHECK_STRING(L, 2);
    lua_pushboolean(L, w->HasAnimation(name ? name : "") ? 1 : 0);
    return 1;
}

int AnimatedWidget_Lua::GetCurrentTexture(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    Texture* tex = w->GetCurrentTexture();
    Asset_Lua::Create(L, tex, true);
    return 1;
}

int AnimatedWidget_Lua::GetCurrentAnimationName(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    lua_pushstring(L, w->GetCurrentAnimationName().c_str());
    return 1;
}

int AnimatedWidget_Lua::GetCurrentFrameIndex(lua_State* L)
{
    AnimatedWidget* w = CHECK_ANIMATED_WIDGET(L, 1);
    lua_pushinteger(L, w->GetCurrentFrameIndex());
    return 1;
}

void AnimatedWidget_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        ANIMATED_WIDGET_LUA_NAME,
        ANIMATED_WIDGET_LUA_FLAG,
        QUAD_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, Play);
    REGISTER_TABLE_FUNC(L, mtIndex, Pause);
    REGISTER_TABLE_FUNC(L, mtIndex, Stop);
    REGISTER_TABLE_FUNC(L, mtIndex, PlayAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, IsPlaying);
    REGISTER_TABLE_FUNC(L, mtIndex, SetFrame);
    REGISTER_TABLE_FUNC(L, mtIndex, GetProgress);
    REGISTER_TABLE_FUNC(L, mtIndex, AnimateTo);
    REGISTER_TABLE_FUNC(L, mtIndex, AnimateToProgress);
    REGISTER_TABLE_FUNC(L, mtIndex, CancelAnimateTo);
    REGISTER_TABLE_FUNC(L, mtIndex, SetSpeed);
    REGISTER_TABLE_FUNC(L, mtIndex, GetSpeed);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAutoPlay);
    REGISTER_TABLE_FUNC(L, mtIndex, GetAutoPlay);
    REGISTER_TABLE_FUNC(L, mtIndex, SetLoopOverride);
    REGISTER_TABLE_FUNC(L, mtIndex, GetLoopOverride);
    REGISTER_TABLE_FUNC(L, mtIndex, SetDefaultAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, GetDefaultAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, AddAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, CreateAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, AddImage);
    REGISTER_TABLE_FUNC(L, mtIndex, AddImages);
    REGISTER_TABLE_FUNC(L, mtIndex, RemoveAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, HasAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentTexture);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentAnimationName);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentFrameIndex);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
