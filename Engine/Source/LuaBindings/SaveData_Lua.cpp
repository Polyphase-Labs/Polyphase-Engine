#include "EngineTypes.h"
#include "Log.h"
#include "Engine.h"
#include "Datum.h"
#include "Stream.h"

#include "System/System.h"

#include "LuaBindings/SaveData_Lua.h"
#include "LuaBindings/Vector_Lua.h"
#include "LuaBindings/LuaTypeCheck.h"

#include <unordered_map>
#include <string>

#if LUA_ENABLED

static const uint32_t SAVE_DATA_VERSION = 1;

static std::unordered_map<std::string, Datum> sSaveData;

// --- Setters ---

int SaveData_Lua::SetInt(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    int32_t value = CHECK_INTEGER(L, 2);

    sSaveData[key] = value;

    return 0;
}

int SaveData_Lua::SetFloat(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    float value = CHECK_NUMBER(L, 2);

    sSaveData[key] = value;

    return 0;
}

int SaveData_Lua::SetBool(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    bool value = CHECK_BOOLEAN(L, 2);

    sSaveData[key] = value;

    return 0;
}

int SaveData_Lua::SetString(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    const char* value = CHECK_STRING(L, 2);

    sSaveData[key] = std::string(value);

    return 0;
}

int SaveData_Lua::SetVector(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    glm::vec4 vec = CHECK_VECTOR(L, 2);

    sSaveData[key] = glm::vec3(vec);

    return 0;
}

int SaveData_Lua::SetVector2D(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    glm::vec4 vec = CHECK_VECTOR(L, 2);

    sSaveData[key] = glm::vec2(vec);

    return 0;
}

int SaveData_Lua::SetColor(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    glm::vec4 value = CHECK_VECTOR(L, 2);

    sSaveData[key] = value;

    return 0;
}

// --- Getters ---

int SaveData_Lua::GetInt(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    int32_t defaultValue = (lua_gettop(L) >= 2) ? (int32_t)lua_tointeger(L, 2) : 0;

    auto it = sSaveData.find(key);
    if (it != sSaveData.end() && it->second.GetType() == DatumType::Integer)
    {
        lua_pushinteger(L, it->second.GetInteger());
    }
    else
    {
        lua_pushinteger(L, defaultValue);
    }

    return 1;
}

int SaveData_Lua::GetFloat(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    float defaultValue = (lua_gettop(L) >= 2) ? (float)lua_tonumber(L, 2) : 0.0f;

    auto it = sSaveData.find(key);
    if (it != sSaveData.end() && it->second.GetType() == DatumType::Float)
    {
        lua_pushnumber(L, it->second.GetFloat());
    }
    else
    {
        lua_pushnumber(L, defaultValue);
    }

    return 1;
}

int SaveData_Lua::GetBool(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    bool defaultValue = (lua_gettop(L) >= 2) ? (bool)lua_toboolean(L, 2) : false;

    auto it = sSaveData.find(key);
    if (it != sSaveData.end() && it->second.GetType() == DatumType::Bool)
    {
        lua_pushboolean(L, it->second.GetBool());
    }
    else
    {
        lua_pushboolean(L, defaultValue);
    }

    return 1;
}

int SaveData_Lua::GetString(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);
    const char* defaultValue = (lua_gettop(L) >= 2) ? lua_tostring(L, 2) : "";

    auto it = sSaveData.find(key);
    if (it != sSaveData.end() && it->second.GetType() == DatumType::String)
    {
        lua_pushstring(L, it->second.GetString().c_str());
    }
    else
    {
        lua_pushstring(L, defaultValue);
    }

    return 1;
}

int SaveData_Lua::GetVector(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);

    auto it = sSaveData.find(key);
    if (it != sSaveData.end() && it->second.GetType() == DatumType::Vector)
    {
        Vector_Lua::Create(L, it->second.GetVector());
    }
    else if (lua_gettop(L) >= 2)
    {
        glm::vec4 defaultValue = CHECK_VECTOR(L, 2);
        Vector_Lua::Create(L, glm::vec3(defaultValue));
    }
    else
    {
        Vector_Lua::Create(L, glm::vec3(0.0f));
    }

    return 1;
}

int SaveData_Lua::GetVector2D(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);

    auto it = sSaveData.find(key);
    if (it != sSaveData.end() && it->second.GetType() == DatumType::Vector2D)
    {
        Vector_Lua::Create(L, it->second.GetVector2D());
    }
    else if (lua_gettop(L) >= 2)
    {
        glm::vec4 defaultValue = CHECK_VECTOR(L, 2);
        Vector_Lua::Create(L, glm::vec2(defaultValue));
    }
    else
    {
        Vector_Lua::Create(L, glm::vec2(0.0f));
    }

    return 1;
}

int SaveData_Lua::GetColor(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);

    auto it = sSaveData.find(key);
    if (it != sSaveData.end() && it->second.GetType() == DatumType::Color)
    {
        Vector_Lua::Create(L, it->second.GetColor());
    }
    else if (lua_gettop(L) >= 2)
    {
        glm::vec4 defaultValue = CHECK_VECTOR(L, 2);
        Vector_Lua::Create(L, defaultValue);
    }
    else
    {
        Vector_Lua::Create(L, glm::vec4(0.0f));
    }

    return 1;
}

// --- File operations ---

int SaveData_Lua::Save(lua_State* L)
{
    const char* saveName = CHECK_STRING(L, 1);

    Stream stream;
    stream.WriteUint32(SAVE_DATA_VERSION);
    stream.WriteUint32((uint32_t)sSaveData.size());

    for (const auto& pair : sSaveData)
    {
        stream.WriteString(pair.first);
        pair.second.WriteStream(stream, false);
    }

    SYS_WriteSave(saveName, stream);

    return 0;
}

int SaveData_Lua::Load(lua_State* L)
{
    const char* saveName = CHECK_STRING(L, 1);

    if (!SYS_DoesSaveExist(saveName))
    {
        LogWarning("SaveData.Load: Save file '%s' does not exist.", saveName);
        return 0;
    }

    Stream stream;
    SYS_ReadSave(saveName, stream);

    if (stream.GetSize() < 8)
    {
        LogError("SaveData.Load: Save file '%s' is corrupt or empty.", saveName);
        return 0;
    }

    uint32_t version = stream.ReadUint32();
    uint32_t count = stream.ReadUint32();

    if (version > SAVE_DATA_VERSION)
    {
        LogError("SaveData.Load: Save file '%s' has unsupported version %u.", saveName, version);
        return 0;
    }

    sSaveData.clear();

    for (uint32_t i = 0; i < count; ++i)
    {
        std::string key;
        stream.ReadString(key);

        Datum datum;
        datum.ReadStream(stream, version, false, false);

        sSaveData[key] = datum;
    }

    return 0;
}

int SaveData_Lua::DoesSaveExist(lua_State* L)
{
    const char* saveName = CHECK_STRING(L, 1);

    bool exists = SYS_DoesSaveExist(saveName);

    lua_pushboolean(L, exists);
    return 1;
}

int SaveData_Lua::DeleteSave(lua_State* L)
{
    const char* saveName = CHECK_STRING(L, 1);

    SYS_DeleteSave(saveName);

    return 0;
}

// --- Key management ---

int SaveData_Lua::HasKey(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);

    bool exists = (sSaveData.find(key) != sSaveData.end());

    lua_pushboolean(L, exists);
    return 1;
}

int SaveData_Lua::DeleteKey(lua_State* L)
{
    const char* key = CHECK_STRING(L, 1);

    sSaveData.erase(key);

    return 0;
}

int SaveData_Lua::DeleteAll(lua_State* L)
{
    sSaveData.clear();

    return 0;
}

// --- Binding ---

void SaveData_Lua::Bind()
{
    lua_State* L = GetLua();

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    REGISTER_TABLE_FUNC(L, tableIdx, SetInt);
    REGISTER_TABLE_FUNC(L, tableIdx, GetInt);
    REGISTER_TABLE_FUNC(L, tableIdx, SetFloat);
    REGISTER_TABLE_FUNC(L, tableIdx, GetFloat);
    REGISTER_TABLE_FUNC(L, tableIdx, SetBool);
    REGISTER_TABLE_FUNC(L, tableIdx, GetBool);
    REGISTER_TABLE_FUNC(L, tableIdx, SetString);
    REGISTER_TABLE_FUNC(L, tableIdx, GetString);
    REGISTER_TABLE_FUNC(L, tableIdx, SetVector);
    REGISTER_TABLE_FUNC(L, tableIdx, GetVector);
    REGISTER_TABLE_FUNC(L, tableIdx, SetVector2D);
    REGISTER_TABLE_FUNC(L, tableIdx, GetVector2D);
    REGISTER_TABLE_FUNC(L, tableIdx, SetColor);
    REGISTER_TABLE_FUNC(L, tableIdx, GetColor);

    REGISTER_TABLE_FUNC(L, tableIdx, Save);
    REGISTER_TABLE_FUNC(L, tableIdx, Load);
    REGISTER_TABLE_FUNC(L, tableIdx, DoesSaveExist);
    REGISTER_TABLE_FUNC(L, tableIdx, DeleteSave);

    REGISTER_TABLE_FUNC(L, tableIdx, HasKey);
    REGISTER_TABLE_FUNC(L, tableIdx, DeleteKey);
    REGISTER_TABLE_FUNC(L, tableIdx, DeleteAll);

    lua_setglobal(L, "SaveData");

    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
