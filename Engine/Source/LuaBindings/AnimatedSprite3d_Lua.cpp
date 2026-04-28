#include "LuaBindings/AnimatedSprite3d_Lua.h"
#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/Node3d_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Texture_Lua.h"
#include "LuaBindings/Material_Lua.h"
#include "LuaBindings/SpriteAnimation_Lua.h"
#include "LuaBindings/LuaUtils.h"

#include "Assets/Texture.h"
#include "Assets/Material.h"
#include "Assets/SpriteAnimation.h"

#if LUA_ENABLED

int AnimatedSprite3D_Lua::Play(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    s->Play();
    return 0;
}

int AnimatedSprite3D_Lua::Pause(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    s->Pause();
    return 0;
}

int AnimatedSprite3D_Lua::Stop(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    s->Stop();
    return 0;
}

int AnimatedSprite3D_Lua::PlayAnimation(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    s->PlayAnimation(name ? name : "");
    return 0;
}

int AnimatedSprite3D_Lua::IsPlaying(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushboolean(L, s->IsPlaying() ? 1 : 0);
    return 1;
}

int AnimatedSprite3D_Lua::SetFrame(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    int32_t frame = (int32_t)CHECK_INTEGER(L, 2);
    s->SetFrame(frame);
    return 0;
}

int AnimatedSprite3D_Lua::GetProgress(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushnumber(L, s->GetProgress());
    return 1;
}

int AnimatedSprite3D_Lua::AnimateTo(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    int32_t target = (int32_t)CHECK_INTEGER(L, 2);
    bool pauseOnFinished = (lua_gettop(L) >= 3) ? (lua_toboolean(L, 3) != 0) : true;
    ScriptFunc cb;
    if (lua_gettop(L) >= 4 && lua_isfunction(L, 4)) cb = ScriptFunc(L, 4);
    s->AnimateTo(target, pauseOnFinished, cb);
    return 0;
}

int AnimatedSprite3D_Lua::AnimateToProgress(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    float progress = (float)CHECK_NUMBER(L, 2);
    bool pauseOnFinished = (lua_gettop(L) >= 3) ? (lua_toboolean(L, 3) != 0) : true;
    ScriptFunc cb;
    if (lua_gettop(L) >= 4 && lua_isfunction(L, 4)) cb = ScriptFunc(L, 4);
    s->AnimateToProgress(progress, pauseOnFinished, cb);
    return 0;
}

int AnimatedSprite3D_Lua::CancelAnimateTo(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    s->CancelAnimateTo();
    return 0;
}

int AnimatedSprite3D_Lua::SetSpeed(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    float v = (float)CHECK_NUMBER(L, 2);
    s->SetSpeed(v);
    return 0;
}

int AnimatedSprite3D_Lua::GetSpeed(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushnumber(L, s->GetSpeed());
    return 1;
}

int AnimatedSprite3D_Lua::SetAutoPlay(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    s->SetAutoPlay(v);
    return 0;
}

int AnimatedSprite3D_Lua::GetAutoPlay(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushboolean(L, s->GetAutoPlay() ? 1 : 0);
    return 1;
}

int AnimatedSprite3D_Lua::SetLoopOverride(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    s->SetLoopOverride(v);
    return 0;
}

int AnimatedSprite3D_Lua::GetLoopOverride(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushboolean(L, s->GetLoopOverride() ? 1 : 0);
    return 1;
}

int AnimatedSprite3D_Lua::SetDefaultAnimation(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    s->SetDefaultAnimation(name ? name : "");
    return 0;
}

int AnimatedSprite3D_Lua::GetDefaultAnimation(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushstring(L, s->GetDefaultAnimation().c_str());
    return 1;
}

int AnimatedSprite3D_Lua::AddAnimation(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);

    if (lua_isstring(L, 2))
    {
        const char* path = lua_tostring(L, 2);
        s->AddAnimation(std::string(path ? path : ""));
    }
    else
    {
        SpriteAnimation* asset = CheckAssetLuaType<SpriteAnimation>(L, 2,
            SPRITE_ANIMATION_LUA_NAME, SPRITE_ANIMATION_LUA_FLAG);
        s->AddAnimation(asset);
    }
    return 0;
}

int AnimatedSprite3D_Lua::CreateAnimation(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
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
        s->CreateAnimation(name ? name : "", textures);
    }
    else
    {
        s->CreateAnimation(name ? name : "");
    }
    return 0;
}

int AnimatedSprite3D_Lua::AddImage(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);

    if (lua_isstring(L, 3))
    {
        const char* path = lua_tostring(L, 3);
        s->AddImage(std::string(name ? name : ""), std::string(path ? path : ""));
    }
    else
    {
        Texture* tex = CheckAssetLuaType<Texture>(L, 3, TEXTURE_LUA_NAME, TEXTURE_LUA_FLAG);
        s->AddImage(std::string(name ? name : ""), tex);
    }
    return 0;
}

int AnimatedSprite3D_Lua::AddImages(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
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
    s->AddImages(std::string(name ? name : ""), paths);
    return 0;
}

int AnimatedSprite3D_Lua::RemoveAnimation(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    s->RemoveAnimation(name ? name : "");
    return 0;
}

int AnimatedSprite3D_Lua::HasAnimation(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    lua_pushboolean(L, s->HasAnimation(name ? name : "") ? 1 : 0);
    return 1;
}

int AnimatedSprite3D_Lua::GetCurrentTexture(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    Texture* tex = s->GetCurrentTexture();
    Asset_Lua::Create(L, tex, true);
    return 1;
}

int AnimatedSprite3D_Lua::GetCurrentAnimationName(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushstring(L, s->GetCurrentAnimationName().c_str());
    return 1;
}

int AnimatedSprite3D_Lua::GetCurrentFrameIndex(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushinteger(L, s->GetCurrentFrameIndex());
    return 1;
}

int AnimatedSprite3D_Lua::SetMaterial(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    Material* mat = CheckAssetLuaType<Material>(L, 2, MATERIAL_LUA_NAME, MATERIAL_LUA_FLAG);
    s->SetMaterial(mat);
    return 0;
}

int AnimatedSprite3D_Lua::GetMaterial(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    Material* mat = s->GetMaterial();
    Asset_Lua::Create(L, mat, true);
    return 1;
}

int AnimatedSprite3D_Lua::SetAffectDiffuse(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    s->SetAffectDiffuse(v);
    return 0;
}

int AnimatedSprite3D_Lua::SetAffectAlpha(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    s->SetAffectAlpha(v);
    return 0;
}

int AnimatedSprite3D_Lua::SetAffectEmission(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    s->SetAffectEmission(v);
    return 0;
}

int AnimatedSprite3D_Lua::GetAffectDiffuse(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushboolean(L, s->GetAffectDiffuse() ? 1 : 0);
    return 1;
}

int AnimatedSprite3D_Lua::GetAffectAlpha(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushboolean(L, s->GetAffectAlpha() ? 1 : 0);
    return 1;
}

int AnimatedSprite3D_Lua::GetAffectEmission(lua_State* L)
{
    AnimatedSprite3D* s = CHECK_ANIMATED_SPRITE_3D(L, 1);
    lua_pushboolean(L, s->GetAffectEmission() ? 1 : 0);
    return 1;
}

void AnimatedSprite3D_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        ANIMATED_SPRITE_3D_LUA_NAME,
        ANIMATED_SPRITE_3D_LUA_FLAG,
        NODE_3D_LUA_NAME);

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
    REGISTER_TABLE_FUNC(L, mtIndex, SetMaterial);
    REGISTER_TABLE_FUNC(L, mtIndex, GetMaterial);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAffectDiffuse);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAffectAlpha);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAffectEmission);
    REGISTER_TABLE_FUNC(L, mtIndex, GetAffectDiffuse);
    REGISTER_TABLE_FUNC(L, mtIndex, GetAffectAlpha);
    REGISTER_TABLE_FUNC(L, mtIndex, GetAffectEmission);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
