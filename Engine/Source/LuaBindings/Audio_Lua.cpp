#include "AudioManager.h"
#include "AssetManager.h"
#include "Engine.h"
#include "Asset.h"
#include "Assets/SoundWave.h"

#include "Audio/Audio.h"

#include <vector>

#include "LuaBindings/LuaUtils.h"
#include "LuaBindings/Audio_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Vector_Lua.h"
#include "LuaBindings/SoundWave_Lua.h"

#if LUA_ENABLED

int Audio_Lua::PlaySound2D(lua_State* L)
{
    SoundWave* soundWave = CHECK_SOUND_WAVE(L, 1);
    float volume = 1.0f;
    float pitch = 1.0f;
    float startTime = 0.0f;
    bool loop = false;
    int32_t priority = 0;
    if (!lua_isnone(L, 2)) { volume = CHECK_NUMBER(L, 2); }
    if (!lua_isnone(L, 3)) { pitch = CHECK_NUMBER(L, 3); }
    if (!lua_isnone(L, 4)) { startTime = CHECK_NUMBER(L, 4); }
    if (!lua_isnone(L, 5)) { loop = CHECK_BOOLEAN(L, 5); }
    if (!lua_isnone(L, 6)) { priority = (int32_t) CHECK_INTEGER(L, 6); }

    if (soundWave != nullptr)
    {
        AudioManager::PlaySound2D(soundWave, volume, pitch, startTime, loop, priority);
    }

    return 0;
}

int Audio_Lua::PlaySound3D(lua_State* L)
{
    SoundWave* soundWave = CHECK_SOUND_WAVE(L, 1);
    glm::vec3 pos = CHECK_VECTOR(L, 2);
    float innerRadius = CHECK_NUMBER(L, 3);
    float outerRadius = CHECK_NUMBER(L, 4);
    AttenuationFunc attenFunc = AttenuationFunc::Linear;
    float volume = 1.0f;
    float pitch = 1.0f;
    float startTime = 0.0f;
    bool loop = false;
    int32_t priority = 0;
    if (!lua_isnone(L, 5)) { attenFunc = (AttenuationFunc) CHECK_INTEGER(L, 5); }
    if (!lua_isnone(L, 6)) { volume = CHECK_NUMBER(L, 6); }
    if (!lua_isnone(L, 7)) { pitch = CHECK_NUMBER(L, 7); }
    if (!lua_isnone(L, 8)) { startTime = CHECK_NUMBER(L, 8); }
    if (!lua_isnone(L, 9)) { loop = CHECK_BOOLEAN(L, 9); }
    if (!lua_isnone(L, 10)) { priority = (int32_t)CHECK_INTEGER(L, 10); }

    if (soundWave != nullptr)
    {
        AudioManager::PlaySound3D(
            soundWave, 
            pos,
            innerRadius,
            outerRadius,
            attenFunc,
            volume,
            pitch,
            startTime,
            loop,
            priority);
    }

    return 0;
}

int Audio_Lua::StopSounds(lua_State* L)
{
    SoundWave* soundWave = CHECK_SOUND_WAVE(L, 1);

    if (soundWave != nullptr)
    {
        AudioManager::StopSounds(soundWave);
    }

    return 0;
}

int Audio_Lua::UpdateSound(lua_State* L)
{
    SoundWave* soundWave = CHECK_SOUND_WAVE(L, 1);
    float volume = CHECK_NUMBER(L, 2);
    float pitch = 1.0f;
    int32_t priority = 0;

    if (!lua_isnoneornil(L, 3)) { pitch = CHECK_NUMBER(L, 3); }
    if (!lua_isnoneornil(L, 4)) { priority = CHECK_INTEGER(L, 4); }

    AudioManager::UpdateSound(soundWave, volume, pitch, priority);

    return 0;
}

int Audio_Lua::StopAllSounds(lua_State* L)
{
    AudioManager::StopAllSounds();
    return 0;
}

int Audio_Lua::IsSoundPlaying(lua_State* L)
{
    SoundWave* soundWave = CHECK_SOUND_WAVE(L, 1);

    bool ret = AudioManager::IsSoundPlaying(soundWave);

    lua_pushboolean(L, ret);
    return 1;
}

int Audio_Lua::SetAudioClassVolume(lua_State* L)
{
    int32_t audioClass = CHECK_INTEGER(L, 1);
    float volume = CHECK_NUMBER(L, 2);

    AudioManager::SetAudioClassVolume(audioClass, volume);

    return 0;
}

int Audio_Lua::GetAudioClassVolume(lua_State* L)
{
    int32_t audioClass = CHECK_INTEGER(L, 1);

    float volume = AudioManager::GetAudioClassVolume(audioClass);

    lua_pushnumber(L, volume);
    return 1;
}

int Audio_Lua::SetAudioClassPitch(lua_State* L)
{
    int32_t audioClass = CHECK_INTEGER(L, 1);
    float pitch = CHECK_NUMBER(L, 2);

    AudioManager::SetAudioClassPitch(audioClass, pitch);

    return 0;
}

int Audio_Lua::GetAudioClassPitch(lua_State* L)
{
    int32_t audioClass = CHECK_INTEGER(L, 1);

    float pitch = AudioManager::GetAudioClassPitch(audioClass);

    lua_pushnumber(L, pitch);
    return 1;
}

int Audio_Lua::SetMasterVolume(lua_State* L)
{
    float value = CHECK_NUMBER(L, 1);

    AudioManager::SetMasterVolume(value);

    return 0;
}

int Audio_Lua::GetMasterVolume(lua_State* L)
{
    float ret = AudioManager::GetMasterVolume();

    lua_pushnumber(L, ret);
    return 1;
}

int Audio_Lua::SetMasterPitch(lua_State* L)
{
    float value = CHECK_NUMBER(L, 1);

    AudioManager::SetMasterPitch(value);

    return 0;
}

int Audio_Lua::GetMasterPitch(lua_State* L)
{
    float ret = AudioManager::GetMasterPitch();

    lua_pushnumber(L, ret);
    return 1;
}

// ---- Voice transport bindings --------------------------------------------------------------------

int Audio_Lua::GetDuration(lua_State* L)
{
    uint32_t voice = (uint32_t)CHECK_INTEGER(L, 1);
    lua_pushnumber(L, AudioManager::GetVoiceDuration(voice));
    return 1;
}

int Audio_Lua::GetPlayTimeNormalized(lua_State* L)
{
    uint32_t voice = (uint32_t)CHECK_INTEGER(L, 1);
    lua_pushnumber(L, AudioManager::GetVoicePlayTimeNormalized(voice));
    return 1;
}

// ---- Audio analysis bindings ---------------------------------------------------------------------

int Audio_Lua::GetRMS(lua_State* L)
{
    uint32_t voice = (uint32_t)CHECK_INTEGER(L, 1);
    lua_pushnumber(L, AUD_GetRMS(voice));
    return 1;
}

int Audio_Lua::GetLoudness(lua_State* L)
{
    uint32_t voice = (uint32_t)CHECK_INTEGER(L, 1);
    lua_pushnumber(L, AUD_GetLoudness(voice));
    return 1;
}

int Audio_Lua::GetLoudnessDb(lua_State* L)
{
    uint32_t voice = (uint32_t)CHECK_INTEGER(L, 1);
    lua_pushnumber(L, AUD_GetLoudnessDb(voice));
    return 1;
}

int Audio_Lua::GetFrequencies(lua_State* L)
{
    uint32_t voice = (uint32_t)CHECK_INTEGER(L, 1);
    float    startHz = CHECK_NUMBER(L, 2);
    float    endHz   = CHECK_NUMBER(L, 3);
    lua_pushnumber(L, AUD_GetFrequencies(voice, startHz, endHz));
    return 1;
}

int Audio_Lua::GetSpectrum(lua_State* L)
{
    uint32_t voice    = (uint32_t)CHECK_INTEGER(L, 1);
    float    startHz  = CHECK_NUMBER(L, 2);
    float    endHz    = CHECK_NUMBER(L, 3);
    int32_t  numBins  = (int32_t)CHECK_INTEGER(L, 4);
    if (numBins < 1) numBins = 1;
    if (numBins > 1024) numBins = 1024;

    std::vector<float> bins((size_t)numBins, 0.0f);
    AUD_GetSpectrum(voice, startHz, endHz, bins.data(), (uint32_t)numBins);

    lua_newtable(L);
    for (int32_t i = 0; i < numBins; ++i)
    {
        lua_pushnumber(L, bins[(size_t)i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int Audio_Lua::GetStreamRMS(lua_State* L)
{
    uint32_t streamId = (uint32_t)CHECK_INTEGER(L, 1);
    lua_pushnumber(L, AUD_GetStreamRMS(streamId));
    return 1;
}

int Audio_Lua::GetStreamLoudness(lua_State* L)
{
    uint32_t streamId = (uint32_t)CHECK_INTEGER(L, 1);
    lua_pushnumber(L, AUD_GetStreamLoudness(streamId));
    return 1;
}

int Audio_Lua::GetStreamLoudnessDb(lua_State* L)
{
    uint32_t streamId = (uint32_t)CHECK_INTEGER(L, 1);
    lua_pushnumber(L, AUD_GetStreamLoudnessDb(streamId));
    return 1;
}

int Audio_Lua::GetStreamFrequencies(lua_State* L)
{
    uint32_t streamId = (uint32_t)CHECK_INTEGER(L, 1);
    float    startHz  = CHECK_NUMBER(L, 2);
    float    endHz    = CHECK_NUMBER(L, 3);
    lua_pushnumber(L, AUD_GetStreamFrequencies(streamId, startHz, endHz));
    return 1;
}

int Audio_Lua::GetStreamSpectrum(lua_State* L)
{
    uint32_t streamId = (uint32_t)CHECK_INTEGER(L, 1);
    float    startHz  = CHECK_NUMBER(L, 2);
    float    endHz    = CHECK_NUMBER(L, 3);
    int32_t  numBins  = (int32_t)CHECK_INTEGER(L, 4);
    if (numBins < 1) numBins = 1;
    if (numBins > 1024) numBins = 1024;

    std::vector<float> bins((size_t)numBins, 0.0f);
    AUD_GetStreamSpectrum(streamId, startHz, endHz, bins.data(), (uint32_t)numBins);

    lua_newtable(L);
    for (int32_t i = 0; i < numBins; ++i)
    {
        lua_pushnumber(L, bins[(size_t)i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

void Audio_Lua::Bind()
{
    lua_State* L = GetLua();

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    REGISTER_TABLE_FUNC(L, tableIdx, PlaySound2D);

    REGISTER_TABLE_FUNC(L, tableIdx, PlaySound3D);

    REGISTER_TABLE_FUNC(L, tableIdx, StopSounds);
    REGISTER_TABLE_FUNC_EX(L, tableIdx, StopSounds, "StopSound");

    REGISTER_TABLE_FUNC(L, tableIdx, UpdateSound);

    REGISTER_TABLE_FUNC(L, tableIdx, StopAllSounds);

    REGISTER_TABLE_FUNC(L, tableIdx, IsSoundPlaying);

    REGISTER_TABLE_FUNC(L, tableIdx, SetAudioClassVolume);

    REGISTER_TABLE_FUNC(L, tableIdx, GetAudioClassVolume);

    REGISTER_TABLE_FUNC(L, tableIdx, SetAudioClassPitch);

    REGISTER_TABLE_FUNC(L, tableIdx, GetAudioClassPitch);

    REGISTER_TABLE_FUNC(L, tableIdx, SetMasterVolume);

    REGISTER_TABLE_FUNC(L, tableIdx, GetMasterVolume);

    REGISTER_TABLE_FUNC(L, tableIdx, SetMasterPitch);

    REGISTER_TABLE_FUNC(L, tableIdx, GetMasterPitch);

    REGISTER_TABLE_FUNC(L, tableIdx, GetDuration);
    REGISTER_TABLE_FUNC(L, tableIdx, GetPlayTimeNormalized);

    REGISTER_TABLE_FUNC(L, tableIdx, GetRMS);
    REGISTER_TABLE_FUNC(L, tableIdx, GetLoudness);
    REGISTER_TABLE_FUNC(L, tableIdx, GetLoudnessDb);
    REGISTER_TABLE_FUNC(L, tableIdx, GetFrequencies);
    REGISTER_TABLE_FUNC(L, tableIdx, GetSpectrum);

    REGISTER_TABLE_FUNC(L, tableIdx, GetStreamRMS);
    REGISTER_TABLE_FUNC(L, tableIdx, GetStreamLoudness);
    REGISTER_TABLE_FUNC(L, tableIdx, GetStreamLoudnessDb);
    REGISTER_TABLE_FUNC(L, tableIdx, GetStreamFrequencies);
    REGISTER_TABLE_FUNC(L, tableIdx, GetStreamSpectrum);

    lua_setglobal(L, AUDIO_LUA_NAME);

    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
