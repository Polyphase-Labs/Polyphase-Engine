#pragma once

#include "EngineTypes.h"

#include "Audio/AudioAnalysis.h"

class SoundWave;
class Audio3D;

class AudioManager
{
public:

    static void Initialize();
    static void Shutdown();
    static void Update(float deltaTime);

    static void PlaySound2D(
        SoundWave* soundWave,
        float volumeMult = 1.0f,
        float pitchMult = 1.0f,
        float startTime = 0.0f,
        bool loop = false,
        int32_t priority = 0);

    static void PlaySound3D(
        SoundWave* soundWave,
        glm::vec3 worldPosition,
        float innerRadius,
        float outerRadius,
        AttenuationFunc attenFunc = AttenuationFunc::Linear,
        float volumeMult = 1.0f,
        float pitchMult = 1.0f,
        float startTime = 0.0f,
        bool loop = false,
        int32_t priority = 0);

    static void UpdateSound(
        SoundWave* soundWave,
        float volume,
        float pitch,
        bool loop = false,
        int32_t priority = 0);

    static void StopComponent(Audio3D* comp);
    static void StopSounds(SoundWave* soundWave);
    static void StopSound(const std::string& name);
    static void StopAllSounds();

    static bool IsSoundPlaying(SoundWave* soundWave);

    static void SetAudioClassVolume(int8_t audioClass, float volume);
    static void SetAudioClassPitch(int8_t audioClass, float pitch);
    static float GetAudioClassVolume(int8_t audioClass);
    static float GetAudioClassPitch(int8_t audioClass);

    static void SetMasterVolume(float volume);
    static void SetMasterPitch(float pitch);
    static float GetMasterVolume();
    static float GetMasterPitch();

    // Audio-analysis helpers. Fills `outView` with the SoundWave PCM + current cursor for the voice;
    // returns false if the voice is idle. Used by AUD_Get* in the platform-indep Audio.cpp.
    static bool GetVoicePcmInfo(uint32_t voiceIndex, AudioAnalysis::PcmView& outView);

    // Linear scan for the voice currently driving this Audio3D node. Returns the voice index, or
    // AUDIO_MAX_VOICES if the node isn't bound to a voice right now.
    static uint32_t FindVoiceIndex(Audio3D* component);

    // Duration of the SoundWave bound to a voice slot, in seconds. Returns 0 if the slot is idle
    // or has no wave. Pitch-independent (reports the wave's natural length).
    static float GetVoiceDuration(uint32_t voiceIndex);

    // Current playback cursor on a voice slot expressed as [0, 1] of its SoundWave's duration.
    // Looping voices wrap via fmod; non-looping voices clamp to [0, 1]. Returns 0 for idle slots
    // or zero-duration waves.
    static float GetVoicePlayTimeNormalized(uint32_t voiceIndex);
};
