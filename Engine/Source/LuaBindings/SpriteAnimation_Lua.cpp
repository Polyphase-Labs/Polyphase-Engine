#include "LuaBindings/SpriteAnimation_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Texture_Lua.h"
#include "LuaBindings/LuaUtils.h"

#include "Assets/Texture.h"

#if LUA_ENABLED

int SpriteAnimation_Lua::GetAnimationName(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    lua_pushstring(L, anim->GetAnimationName().c_str());
    return 1;
}

int SpriteAnimation_Lua::SetAnimationName(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    const char* name = CHECK_STRING(L, 2);
    anim->SetAnimationName(name ? name : "");
    return 0;
}

int SpriteAnimation_Lua::GetMode(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    lua_pushinteger(L, int(anim->GetMode()));
    return 1;
}

int SpriteAnimation_Lua::SetMode(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    int mode = (int)CHECK_INTEGER(L, 2);
    if (mode < 0 || mode >= int(SpriteFrameSourceMode::Count)) mode = 0;
    anim->SetMode(SpriteFrameSourceMode(mode));
    return 0;
}

int SpriteAnimation_Lua::GetFps(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    lua_pushnumber(L, anim->GetFps());
    return 1;
}

int SpriteAnimation_Lua::SetFps(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    float fps = (float)CHECK_NUMBER(L, 2);
    anim->SetFps(fps);
    return 0;
}

int SpriteAnimation_Lua::GetLoop(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    lua_pushboolean(L, anim->GetLoop() ? 1 : 0);
    return 1;
}

int SpriteAnimation_Lua::SetLoop(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    bool loop = CHECK_BOOLEAN(L, 2);
    anim->SetLoop(loop);
    return 0;
}

int SpriteAnimation_Lua::GetFrameCount(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    lua_pushinteger(L, anim->GetFrameCount());
    return 1;
}

int SpriteAnimation_Lua::AddFrame(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    Texture* tex = CheckAssetLuaType<Texture>(L, 2, TEXTURE_LUA_NAME, TEXTURE_LUA_FLAG);
    anim->AddFrame(tex);
    return 0;
}

int SpriteAnimation_Lua::ClearFrames(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    anim->ClearFrames();
    return 0;
}

int SpriteAnimation_Lua::GetAtlasTexture(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    Texture* tex = anim->GetAtlasTexture();
    Asset_Lua::Create(L, tex, true /*allowNull*/);
    return 1;
}

int SpriteAnimation_Lua::SetAtlasTexture(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    Texture* tex = CheckAssetLuaType<Texture>(L, 2, TEXTURE_LUA_NAME, TEXTURE_LUA_FLAG);
    anim->SetAtlasTexture(tex);
    return 0;
}

int SpriteAnimation_Lua::SetAtlasGrid(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    int cols = (int)CHECK_INTEGER(L, 2);
    int rows = (int)CHECK_INTEGER(L, 3);
    anim->SetAtlasGrid(cols, rows);
    return 0;
}

int SpriteAnimation_Lua::SetAtlasMargin(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    int x = (int)CHECK_INTEGER(L, 2);
    int y = (int)CHECK_INTEGER(L, 3);
    anim->SetAtlasMargin(x, y);
    return 0;
}

int SpriteAnimation_Lua::SetAtlasSpacing(lua_State* L)
{
    SpriteAnimation* anim = CHECK_SPRITE_ANIMATION(L, 1);
    int x = (int)CHECK_INTEGER(L, 2);
    int y = (int)CHECK_INTEGER(L, 3);
    anim->SetAtlasSpacing(x, y);
    return 0;
}

void SpriteAnimation_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        SPRITE_ANIMATION_LUA_NAME,
        SPRITE_ANIMATION_LUA_FLAG,
        ASSET_LUA_NAME);

    Asset_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, GetAnimationName);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAnimationName);
    REGISTER_TABLE_FUNC(L, mtIndex, GetMode);
    REGISTER_TABLE_FUNC(L, mtIndex, SetMode);
    REGISTER_TABLE_FUNC(L, mtIndex, GetFps);
    REGISTER_TABLE_FUNC(L, mtIndex, SetFps);
    REGISTER_TABLE_FUNC(L, mtIndex, GetLoop);
    REGISTER_TABLE_FUNC(L, mtIndex, SetLoop);
    REGISTER_TABLE_FUNC(L, mtIndex, GetFrameCount);
    REGISTER_TABLE_FUNC(L, mtIndex, AddFrame);
    REGISTER_TABLE_FUNC(L, mtIndex, ClearFrames);
    REGISTER_TABLE_FUNC(L, mtIndex, GetAtlasTexture);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAtlasTexture);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAtlasGrid);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAtlasMargin);
    REGISTER_TABLE_FUNC(L, mtIndex, SetAtlasSpacing);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
