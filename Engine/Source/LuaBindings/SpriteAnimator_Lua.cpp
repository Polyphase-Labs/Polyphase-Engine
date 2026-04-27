#include "LuaBindings/SpriteAnimator_Lua.h"
#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Texture_Lua.h"
#include "LuaBindings/SpriteAnimation_Lua.h"
#include "LuaBindings/LuaUtils.h"

#include "Assets/Texture.h"
#include "Assets/SpriteAnimation.h"

#if LUA_ENABLED

int SpriteAnimator_Lua::Play(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    anim->Play();
    return 0;
}

int SpriteAnimator_Lua::Pause(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    anim->Pause();
    return 0;
}

int SpriteAnimator_Lua::Stop(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    anim->Stop();
    return 0;
}

int SpriteAnimator_Lua::PlayAnimation(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    const char* name = CHECK_STRING(L, 2);
    anim->PlayAnimation(name ? name : "");
    return 0;
}

int SpriteAnimator_Lua::IsPlaying(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    lua_pushboolean(L, anim->IsPlaying() ? 1 : 0);
    return 1;
}

int SpriteAnimator_Lua::SetSpeed(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    float v = (float)CHECK_NUMBER(L, 2);
    anim->SetSpeed(v);
    return 0;
}

int SpriteAnimator_Lua::GetSpeed(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    lua_pushnumber(L, anim->GetSpeed());
    return 1;
}

int SpriteAnimator_Lua::SetAutoPlay(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    anim->SetAutoPlay(v);
    return 0;
}

int SpriteAnimator_Lua::GetAutoPlay(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    lua_pushboolean(L, anim->GetAutoPlay() ? 1 : 0);
    return 1;
}

int SpriteAnimator_Lua::SetLoopOverride(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    anim->SetLoopOverride(v);
    return 0;
}

int SpriteAnimator_Lua::GetLoopOverride(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    lua_pushboolean(L, anim->GetLoopOverride() ? 1 : 0);
    return 1;
}

int SpriteAnimator_Lua::SetDefaultAnimation(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    const char* name = CHECK_STRING(L, 2);
    anim->SetDefaultAnimation(name ? name : "");
    return 0;
}

int SpriteAnimator_Lua::GetDefaultAnimation(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    lua_pushstring(L, anim->GetDefaultAnimation().c_str());
    return 1;
}

// Accepts either (asset) or (path-string).
int SpriteAnimator_Lua::AddAnimation(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);

    if (lua_isstring(L, 2))
    {
        const char* path = lua_tostring(L, 2);
        anim->AddAnimation(std::string(path ? path : ""));
    }
    else
    {
        SpriteAnimation* asset = CheckAssetLuaType<SpriteAnimation>(L, 2,
            SPRITE_ANIMATION_LUA_NAME, SPRITE_ANIMATION_LUA_FLAG);
        anim->AddAnimation(asset);
    }
    return 0;
}

// (name) or (name, {textures...})
int SpriteAnimator_Lua::CreateAnimation(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    const char* name = CHECK_STRING(L, 2);

    if (lua_gettop(L) >= 3 && lua_istable(L, 3))
    {
        std::vector<Texture*> textures;
        const int n = (int)lua_rawlen(L, 3);
        textures.reserve(n);
        for (int i = 1; i <= n; ++i)
        {
            lua_rawgeti(L, 3, i);
            // Each entry should be a Texture asset userdata.
            Texture* tex = CheckAssetLuaType<Texture>(L, lua_gettop(L),
                TEXTURE_LUA_NAME, TEXTURE_LUA_FLAG, false /*throwError*/);
            if (tex != nullptr)
            {
                textures.push_back(tex);
            }
            lua_pop(L, 1);
        }
        anim->CreateAnimation(name ? name : "", textures);
    }
    else
    {
        anim->CreateAnimation(name ? name : "");
    }
    return 0;
}

// (name, texture) or (name, path)
int SpriteAnimator_Lua::AddImage(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    const char* name = CHECK_STRING(L, 2);

    if (lua_isstring(L, 3))
    {
        const char* path = lua_tostring(L, 3);
        anim->AddImage(std::string(name ? name : ""), std::string(path ? path : ""));
    }
    else
    {
        Texture* tex = CheckAssetLuaType<Texture>(L, 3, TEXTURE_LUA_NAME, TEXTURE_LUA_FLAG);
        anim->AddImage(std::string(name ? name : ""), tex);
    }
    return 0;
}

// (name, {paths...})
int SpriteAnimator_Lua::AddImages(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    const char* name = CHECK_STRING(L, 2);
    CHECK_TABLE(L, 3);

    std::vector<std::string> paths;
    const int n = (int)lua_rawlen(L, 3);
    paths.reserve(n);
    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti(L, 3, i);
        if (lua_isstring(L, -1))
        {
            paths.push_back(lua_tostring(L, -1));
        }
        lua_pop(L, 1);
    }
    anim->AddImages(std::string(name ? name : ""), paths);
    return 0;
}

int SpriteAnimator_Lua::RemoveAnimation(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    const char* name = CHECK_STRING(L, 2);
    anim->RemoveAnimation(name ? name : "");
    return 0;
}

int SpriteAnimator_Lua::HasAnimation(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    const char* name = CHECK_STRING(L, 2);
    lua_pushboolean(L, anim->HasAnimation(name ? name : "") ? 1 : 0);
    return 1;
}

int SpriteAnimator_Lua::GetCurrentTexture(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    Texture* tex = anim->GetCurrentTexture();
    Asset_Lua::Create(L, tex, true /*allowNull*/);
    return 1;
}

int SpriteAnimator_Lua::GetCurrentUVScale(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    glm::vec2 s = anim->GetCurrentUVScale();
    lua_pushnumber(L, s.x);
    lua_pushnumber(L, s.y);
    return 2;
}

int SpriteAnimator_Lua::GetCurrentUVOffset(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    glm::vec2 o = anim->GetCurrentUVOffset();
    lua_pushnumber(L, o.x);
    lua_pushnumber(L, o.y);
    return 2;
}

int SpriteAnimator_Lua::GetCurrentUVRect(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    glm::vec4 r = anim->GetCurrentUVRect();
    lua_pushnumber(L, r.x);
    lua_pushnumber(L, r.y);
    lua_pushnumber(L, r.z);
    lua_pushnumber(L, r.w);
    return 4;
}

int SpriteAnimator_Lua::GetCurrentAnimationName(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    lua_pushstring(L, anim->GetCurrentAnimationName().c_str());
    return 1;
}

int SpriteAnimator_Lua::GetCurrentFrameIndex(lua_State* L)
{
    SpriteAnimator* anim = CHECK_SPRITE_ANIMATOR(L, 1);
    lua_pushinteger(L, anim->GetCurrentFrameIndex());
    return 1;
}

void SpriteAnimator_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        SPRITE_ANIMATOR_LUA_NAME,
        SPRITE_ANIMATOR_LUA_FLAG,
        NODE_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, Play);
    REGISTER_TABLE_FUNC(L, mtIndex, Pause);
    REGISTER_TABLE_FUNC(L, mtIndex, Stop);
    REGISTER_TABLE_FUNC(L, mtIndex, PlayAnimation);
    REGISTER_TABLE_FUNC(L, mtIndex, IsPlaying);

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
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentUVScale);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentUVOffset);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentUVRect);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentAnimationName);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentFrameIndex);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
