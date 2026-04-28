#pragma once

#include "Engine.h"

#if LUA_ENABLED

#define SAVE_DATA_LUA_NAME "SaveData"

struct SaveData_Lua
{
    static int SetInt(lua_State* L);
    static int GetInt(lua_State* L);
    static int SetFloat(lua_State* L);
    static int GetFloat(lua_State* L);
    static int SetBool(lua_State* L);
    static int GetBool(lua_State* L);
    static int SetString(lua_State* L);
    static int GetString(lua_State* L);
    static int SetVector(lua_State* L);
    static int GetVector(lua_State* L);
    static int SetVector2D(lua_State* L);
    static int GetVector2D(lua_State* L);
    static int SetColor(lua_State* L);
    static int GetColor(lua_State* L);

    static int Save(lua_State* L);
    static int Load(lua_State* L);
    static int DoesSaveExist(lua_State* L);
    static int DeleteSave(lua_State* L);

    static int HasKey(lua_State* L);
    static int DeleteKey(lua_State* L);
    static int DeleteAll(lua_State* L);

    static void Bind();
};

#endif
