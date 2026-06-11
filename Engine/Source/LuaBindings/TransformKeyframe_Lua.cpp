#include "Engine.h"
#include "EngineTypes.h"
#include "Log.h"
#include "Maths.h"

#include "LuaBindings/TransformKeyframe_Lua.h"
#include "LuaBindings/Vector_Lua.h"

#if LUA_ENABLED

int TransformKeyframe_Lua::Create(lua_State* L)
{
    int numArgs = lua_gettop(L);

    TransformKeyframe_Lua* newKf = (TransformKeyframe_Lua*)lua_newuserdata(L, sizeof(TransformKeyframe_Lua));
    new (newKf) TransformKeyframe_Lua();
    luaL_getmetatable(L, TRANSFORM_KEYFRAME_LUA_NAME);
    OCT_ASSERT(lua_istable(L, -1));
    lua_setmetatable(L, -2);

    if (numArgs == 1 && lua_isuserdata(L, 1))
    {
        TransformKeyframe_Lua* src = CheckLuaType<TransformKeyframe_Lua>(L, 1, TRANSFORM_KEYFRAME_LUA_NAME, false);
        if (src != nullptr)
        {
            newKf->mKeyframe = src->mKeyframe;
        }
    }
    else if (numArgs >= 1 && lua_isnumber(L, 1))
    {
        newKf->mKeyframe.mTime = (float)lua_tonumber(L, 1);
    }

    return 1;
}

int TransformKeyframe_Lua::Create(lua_State* L, const TransformKeyframe& value)
{
    TransformKeyframe_Lua* newKf = (TransformKeyframe_Lua*)lua_newuserdata(L, sizeof(TransformKeyframe_Lua));
    new (newKf) TransformKeyframe_Lua();
    newKf->mKeyframe = value;
    luaL_getmetatable(L, TRANSFORM_KEYFRAME_LUA_NAME);
    OCT_ASSERT(lua_istable(L, -1));
    lua_setmetatable(L, -2);

    return 1;
}

int TransformKeyframe_Lua::Index(lua_State* L)
{
    TransformKeyframe& kf = CHECK_TRANSFORM_KEYFRAME(L, 1);
    const char* key = CHECK_STRING(L, 2);

    if (strcmp(key, "time") == 0)
    {
        lua_pushnumber(L, kf.mTime);
        return 1;
    }
    if (strcmp(key, "position") == 0)
    {
        Vector_Lua::Create(L, kf.mPosition);
        return 1;
    }
    if (strcmp(key, "rotation") == 0)
    {
        glm::vec3 euler = glm::degrees(glm::eulerAngles(kf.mRotation));
        Vector_Lua::Create(L, euler);
        return 1;
    }
    if (strcmp(key, "scale") == 0)
    {
        Vector_Lua::Create(L, kf.mScale);
        return 1;
    }
    if (strcmp(key, "interp") == 0)
    {
        lua_pushinteger(L, (int)kf.mInterpMode);
        return 1;
    }
    if (strcmp(key, "signal") == 0)
    {
        lua_pushstring(L, kf.mSignal.c_str());
        return 1;
    }

    // Fall back to metatable for methods (Lerp, Clone, etc.)
    lua_getglobal(L, TRANSFORM_KEYFRAME_LUA_NAME);
    lua_pushstring(L, key);
    lua_rawget(L, -2);
    return 1;
}

int TransformKeyframe_Lua::NewIndex(lua_State* L)
{
    TransformKeyframe& kf = CHECK_TRANSFORM_KEYFRAME(L, 1);
    const char* key = CHECK_STRING(L, 2);

    if (strcmp(key, "time") == 0)
    {
        kf.mTime = (float)lua_tonumber(L, 3);
        return 0;
    }
    if (strcmp(key, "position") == 0)
    {
        Vector_Lua* vecLua = CheckLuaType<Vector_Lua>(L, 3, VECTOR_LUA_NAME, false);
        if (vecLua != nullptr)
        {
            kf.mPosition = glm::vec3(vecLua->mVector);
        }
        return 0;
    }
    if (strcmp(key, "rotation") == 0)
    {
        Vector_Lua* vecLua = CheckLuaType<Vector_Lua>(L, 3, VECTOR_LUA_NAME, false);
        if (vecLua != nullptr)
        {
            glm::vec3 euler = glm::radians(glm::vec3(vecLua->mVector));
            kf.mRotation = glm::quat(euler);
        }
        return 0;
    }
    if (strcmp(key, "scale") == 0)
    {
        Vector_Lua* vecLua = CheckLuaType<Vector_Lua>(L, 3, VECTOR_LUA_NAME, false);
        if (vecLua != nullptr)
        {
            kf.mScale = glm::vec3(vecLua->mVector);
        }
        return 0;
    }
    if (strcmp(key, "interp") == 0)
    {
        int v = (int)lua_tointeger(L, 3);
        if (v >= 0 && v < (int)InterpMode::Count)
        {
            kf.mInterpMode = (InterpMode)v;
        }
        return 0;
    }
    if (strcmp(key, "signal") == 0)
    {
        const char* s = lua_tostring(L, 3);
        kf.mSignal = (s != nullptr) ? s : "";
        return 0;
    }

    LogError("Lua script attempted to assign an unknown field on TransformKeyframe: %s", key);
    return 0;
}

int TransformKeyframe_Lua::ToString(lua_State* L)
{
    TransformKeyframe& kf = CHECK_TRANSFORM_KEYFRAME(L, 1);
    glm::vec3 euler = glm::degrees(glm::eulerAngles(kf.mRotation));
    lua_pushfstring(L, "TransformKeyframe{ t=%f pos=(%f,%f,%f) rot=(%f,%f,%f) scale=(%f,%f,%f) signal=%s }",
        kf.mTime,
        kf.mPosition.x, kf.mPosition.y, kf.mPosition.z,
        euler.x, euler.y, euler.z,
        kf.mScale.x, kf.mScale.y, kf.mScale.z,
        kf.mSignal.c_str());
    return 1;
}

int TransformKeyframe_Lua::Lerp(lua_State* L)
{
    TransformKeyframe_Lua* a = CheckLuaType<TransformKeyframe_Lua>(L, 1, TRANSFORM_KEYFRAME_LUA_NAME);
    TransformKeyframe_Lua* b = CheckLuaType<TransformKeyframe_Lua>(L, 2, TRANSFORM_KEYFRAME_LUA_NAME);
    float t = (float)lua_tonumber(L, 3);

    TransformKeyframe out = TransformKeyframe::Lerp(a->mKeyframe, b->mKeyframe, t);
    return TransformKeyframe_Lua::Create(L, out);
}

int TransformKeyframe_Lua::Clone(lua_State* L)
{
    TransformKeyframe& src = CHECK_TRANSFORM_KEYFRAME(L, 1);
    return TransformKeyframe_Lua::Create(L, src);
}

void TransformKeyframe_Lua::Bind()
{
    lua_State* L = GetLua();
    OCT_ASSERT(lua_gettop(L) == 0);

    luaL_newmetatable(L, TRANSFORM_KEYFRAME_LUA_NAME);
    int mtIndex = lua_gettop(L);

    REGISTER_TABLE_FUNC(L, mtIndex, Create);
    REGISTER_TABLE_FUNC(L, mtIndex, Lerp);
    REGISTER_TABLE_FUNC(L, mtIndex, Clone);

    REGISTER_TABLE_FUNC_EX(L, mtIndex, Index, "__index");
    REGISTER_TABLE_FUNC_EX(L, mtIndex, NewIndex, "__newindex");
    REGISTER_TABLE_FUNC_EX(L, mtIndex, ToString, "__tostring");

    lua_setglobal(L, TRANSFORM_KEYFRAME_LUA_NAME);

    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
