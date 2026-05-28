#include "AudioManager.h"
#include "Assets/SoundWave.h"
#include "Asset.h"
#include "Log.h"
#include "Maths.h"
#include "Engine.h"
#include "World.h"
#include "Profiler.h"
#include "SignalBus.h"

#include "Nodes/3D/Node3d.h"
#include "Nodes/3D/Audio3d.h"

#include "Audio/Audio.h"
#include "Audio/AudioConstants.h"

// TODO: define max audio sources as AUDIO_MAX_VOICES
#define MAX_AUDIO_SOURCES AUDIO_MAX_VOICES
#define MAX_AUDIO_CLASSES 16

struct AudioClassData
{
    float mVolume = 1.0f;
    float mPitch = 1.0f;
};

struct AudioSource
{
    SoundWaveRef mSoundWave;
    Audio3D* mComponent;
    float mVolumeMult;
    float mPitchMult;
    int32_t mPriority;
    glm::vec3 mPosition;
    float mInnerRadius;
    float mOuterRadius;
    AttenuationFunc mAttenuationFunc;
    int8_t mAudioClass;
    float mStartTime;       // Seconds offset passed to AUD_Play.
    float mPlaybackTime;    // Seconds since AUD_Play (pitch-scaled). Used by audio analysis.
    bool mLoop;

    AudioSource()
    {
        Reset();
    }

    void Set(
        SoundWave* soundWave,
        Audio3D* component,
        float volumeMult,
        float pitchMult,
        int32_t priority,
        glm::vec3 position,
        float innerRadius,
        float outerRadius,
        AttenuationFunc attenFunc,
        int8_t audioClass,
        float startTime,
        bool loop)
    {
        mSoundWave = soundWave;
        mComponent = component;
        mVolumeMult = volumeMult;
        mPitchMult = pitchMult;
        mPriority = priority;
        mPosition = position;
        mInnerRadius = innerRadius;
        mOuterRadius = outerRadius;
        mAttenuationFunc = attenFunc;
        mAudioClass = glm::clamp<int8_t>(audioClass, 0, MAX_AUDIO_CLASSES - 1);
        mStartTime = startTime;
        mPlaybackTime = 0.0f;
        mLoop = loop;
    }

    void Reset()
    {
        mSoundWave = nullptr;
        mComponent = nullptr;
        mVolumeMult = 1.0f;
        mPitchMult = 1.0f;
        mPriority = 0;
        mPosition = { 0.0f, 0.0f, 0.0f };
        mInnerRadius = -1.0f;
        mOuterRadius = -1.0f;
        mAttenuationFunc = AttenuationFunc::Count;
        mAudioClass = 0;
        mStartTime = 0.0f;
        mPlaybackTime = 0.0f;
        mLoop = false;
    }

    bool IsSpatial() const
    {
        return (mInnerRadius >= 0.0f && mOuterRadius > 0.0f);
    }
};

static AudioClassData sAudioClassData[MAX_AUDIO_CLASSES];
static AudioSource sAudioSources[MAX_AUDIO_SOURCES];
static float sMasterVolume = 1.0f;
static float sMasterPitch = 1.0f;

float CalcVolumeAttenuation(AttenuationFunc func, float innerRadius, float outerRadius, float distance)
{
    float ret = 1.0f;

    float x = glm::max(0.0f, distance - innerRadius) / (outerRadius - innerRadius);
    x = glm::clamp(x, 0.0f, 1.0f);
    x = (1.0f - x);

    switch (func)
    {
    case AttenuationFunc::Linear:
        ret = x;
        break;

    default:
        break;
    }

    return ret;
}

void CalcVolumeAttenuationLR(
    AttenuationFunc func,
    float innerRadius,
    float outerRadius,
    glm::vec3 srcPos,
    glm::vec3 listenerPos,
    glm::vec3 listenerRight,
    float distance,
    float& outLeft,
    float& outRight)
{
    float volume = 1.0f;

    float x = glm::max(0.0f, distance - innerRadius) / (outerRadius - innerRadius);
    x = glm::clamp(x, 0.0f, 1.0f);
    x = (1.0f - x);

    switch (func)
    {
    case AttenuationFunc::Linear:
        volume = x;
        break;

    default:
        break;
    }

    glm::vec3 toSrc = Maths::SafeNormalize(srcPos - listenerPos);
    float dot = glm::dot(listenerRight, toSrc);

    float minAttenAlpha = glm::clamp((distance - 1.0f) / 5.0f, 0.0f, 1.0f);
    float MinDirAtten = glm::mix(1.0f, 0.2f, minAttenAlpha);
    if (dot > 0)
    {
        outRight = 1.0f;
        outLeft = glm::mix(1.0f, MinDirAtten, dot);
    }
    else
    {
        outLeft = 1.0f;
        outRight = glm::mix(1.0f, MinDirAtten, -dot);
    }

    outLeft *= volume;
    outRight *= volume;
}


void PlayAudio(
    uint32_t sourceIndex,
    SoundWave* soundWave,
    Audio3D* component,
    float volumeMult,
    float pitchMult,
    int32_t priority,
    glm::vec3 position,
    float innerRadius,
    float outerRadius,
    AttenuationFunc attenFunc,
    int32_t audioClass,
    bool loop,
    float startTime)
{
    OCT_ASSERT(sourceIndex < MAX_AUDIO_SOURCES);
    OCT_ASSERT(soundWave != nullptr);

    audioClass = glm::clamp<int8_t>(audioClass, 0, MAX_AUDIO_CLASSES - 1);

    sAudioSources[sourceIndex].Set(
        soundWave,
        component,
        volumeMult,
        pitchMult,
        priority,
        position,
        innerRadius,
        outerRadius,
        attenFunc,
        audioClass,
        startTime,
        loop);

    float classVolume = sAudioClassData[audioClass].mVolume;
    float classPitch = sAudioClassData[audioClass].mPitch;

    float volume = volumeMult * soundWave->GetVolumeMultiplier() * classVolume * sMasterVolume;
    float pitch = pitchMult * soundWave->GetPitchMultiplier() * classPitch * sMasterPitch;

    if (component != nullptr)
    {
        component->NotifyAudible(true);
    }

    bool spatial = sAudioSources[sourceIndex].IsSpatial();

    AUD_Play(
        sourceIndex,
        soundWave,
        volume,
        pitch,
        loop,
        startTime,
        spatial);
}

void StopAudio(uint32_t sourceIndex)
{
    if (sAudioSources[sourceIndex].mComponent != nullptr)
    {
        sAudioSources[sourceIndex].mComponent->NotifyAudible(false);
    }

    AUD_Stop(sourceIndex);

    sAudioSources[sourceIndex].Reset();
}

uint32_t FindAvailableAudioSourceIndex(int32_t inPriority)
{
    uint32_t availableIndex = MAX_AUDIO_SOURCES;
    int32_t lowestPriority = 0x7fffffff;
    uint32_t lowestPriorityIndex = 0;

    for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
    {
        if (sAudioSources[i].mSoundWave.Get() == nullptr)
        {
            availableIndex = i;
            break;
        }

        if (sAudioSources[i].mPriority < lowestPriority)
        {
            lowestPriority = sAudioSources[i].mPriority;
            lowestPriorityIndex = i;
        }
    }

    // All sources are being used. But see if we can evict one with lower priority
    if (availableIndex == MAX_AUDIO_SOURCES &&
        lowestPriority < inPriority)
    {
        LogWarning("Evicting lower priority sound");
        StopAudio(lowestPriorityIndex);
        availableIndex = lowestPriorityIndex;
    }

    return availableIndex;
}

void AudioManager::Initialize()
{

}

void AudioManager::Shutdown()
{

}

void AudioManager::Update(float deltaTime)
{
    SCOPED_FRAME_STAT("Audio");

    // TODO:
    // (1) -- Update Active Sources --
    //     Iterate through audio sources and update volume for any 3D sounds (including components)
    //     If an source has finished playing, Reset the source (and notify the component if applicable).
    //     Do not evict 3D sounds that are out of range. We want to hear them when we return.
    //     Evict components if out of hearing range.
    // (2) -- Play New Sounds --
    //     Iterate over Audio3D list. If any component is playing and in range, but has no audio source active,
    //     then find an available audio source and Play(). Use component's mPlayTime var to start at correct time.


    // (1) Update Active Sources
    Node3D* listener = GetWorld(0)->GetAudioReceiver();
    glm::vec3 listenerPos = listener ? listener->GetWorldPosition() : glm::vec3(0,0,0);
    glm::vec3 listenerRight = listener ? listener->GetRightVector() : glm::vec3(1.0f, 0.0f, 0.0f);

    for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
    {
        int8_t audioClass = sAudioSources[i].mAudioClass;
        float classVolume = sAudioClassData[audioClass].mVolume;
        float classPitch = sAudioClassData[audioClass].mPitch;

        if (sAudioSources[i].mSoundWave.Get() != nullptr)
        {
            SoundWave* soundWave = sAudioSources[i].mSoundWave.Get<SoundWave>();
            bool stopped = false;

            // Advance the audio-analysis playback cursor (pitch-scaled, drives AUD_Get* read offset).
            const float prevPlaybackTime = sAudioSources[i].mPlaybackTime;
            sAudioSources[i].mPlaybackTime += deltaTime * sAudioSources[i].mPitchMult;

            // Looping voices: emit OnFinished + SoundFinished each time the wave wraps so
            // playlists can auto-advance from looped tracks too. Skip when the component is
            // about to be stopped this tick or the platform voice already ended — those go
            // through the natural-stop / natural-end paths below. The 'fired' flag prevents
            // the natural-end branch from double-emitting if a script handler calls
            // SetSoundWave (which auto-releases the slot and would otherwise re-trigger it).
            bool fired = false;
            if (sAudioSources[i].mLoop &&
                AUD_IsPlaying(i) &&
                (sAudioSources[i].mComponent == nullptr || sAudioSources[i].mComponent->IsPlaying()))
            {
                const float duration = soundWave->GetDuration();
                if (duration > 0.0f)
                {
                    const float prevCursor = sAudioSources[i].mStartTime + prevPlaybackTime;
                    const float currCursor = sAudioSources[i].mStartTime + sAudioSources[i].mPlaybackTime;
                    const int32_t prevLoops = (int32_t)glm::floor(prevCursor / duration);
                    const int32_t currLoops = (int32_t)glm::floor(currCursor / duration);
                    if (currLoops > prevLoops)
                    {
                        Audio3D* loopComp = sAudioSources[i].mComponent;
                        SoundWave* loopWave = soundWave;
                        if (loopComp != nullptr)
                        {
                            loopComp->OnSoundFinished();
                        }
                        if (loopWave != nullptr)
                        {
                            GetSignalBus()->Emit("SoundFinished", { Datum(loopWave) });
                        }
                        fired = true;
                    }
                }
            }

            if (sAudioSources[i].mComponent != nullptr &&
                !sAudioSources[i].mComponent->IsPlaying())
            {
                // If the component has been stopped, then stop the source!
                StopAudio(i);
                stopped = true;
            }
            else if (!AUD_IsPlaying(i))
            {
                // Sound wave reached its natural end. Snapshot the component + soundwave BEFORE we
                // clear the source so signal handlers can inspect them. Loop mode normally stays in
                // AUD_IsPlaying=true and is handled by the loop-wrap path above — this branch only
                // catches the non-looping natural-end case (and any defensive fall-through).
                Audio3D*   finishedComp = sAudioSources[i].mComponent;
                SoundWave* finishedWave = soundWave;

                if (finishedComp != nullptr && !finishedComp->GetLoop())
                {
                    finishedComp->StopAudio();
                }

                StopAudio(i);
                stopped = true;

                // Per-node OnFinished signal (Audio3D scripts) + global SoundFinished SignalBus
                // emit. Skipped if the loop-wrap path above already fired this tick — e.g. a
                // script handler called SetSoundWave, which auto-released this slot and made
                // AUD_IsPlaying return false here.
                if (!fired)
                {
                    if (finishedComp != nullptr)
                    {
                        finishedComp->OnSoundFinished();
                    }
                    if (finishedWave != nullptr)
                    {
                        GetSignalBus()->Emit("SoundFinished", { Datum(finishedWave) });
                    }
                }
            }
            else if (sAudioSources[i].IsSpatial())
            {
                // Update attenuation of the 3D sound
                if (sAudioSources[i].mComponent != nullptr)
                {
                    Audio3D* comp = sAudioSources[i].mComponent;
                    sAudioSources[i].mPosition = comp->GetWorldPosition();
                    sAudioSources[i].mVolumeMult = comp->GetVolume();
                }

                float dist = glm::distance(listenerPos, sAudioSources[i].mPosition);

                if (sAudioSources[i].mComponent != nullptr && 
                    dist > sAudioSources[i].mOuterRadius)
                {
                    // Sound is no longer in hearing range.
                    // If this belongs to a component, StopAudio() the sound.
                    StopAudio(i);
                    stopped = true;
                }
                else
                {

#if 0
                    // Otherwise update the new volume
                    float volume = CalcVolumeAttenuation(
                        sAudioSources[i].mAttenuationFunc,
                        sAudioSources[i].mInnerRadius,
                        sAudioSources[i].mOuterRadius,
                        dist);

                    volume = volume * sAudioSources[i].mVolumeMult * soundWave->GetVolumeMultiplier() * classVolume * sMasterVolume;
                    AUD_SetVolume(i, volume, volume);
#else
                    float volLeft = 1.0f;
                    float volRight = 1.0f;

                    CalcVolumeAttenuationLR(sAudioSources[i].mAttenuationFunc,
                        sAudioSources[i].mInnerRadius,
                        sAudioSources[i].mOuterRadius,
                        sAudioSources[i].mPosition,
                        listenerPos,
                        listenerRight,
                        dist,
                        volLeft,
                        volRight);

                    volLeft = volLeft * sAudioSources[i].mVolumeMult * soundWave->GetVolumeMultiplier() * classVolume * sMasterVolume;
                    volRight = volRight * sAudioSources[i].mVolumeMult * soundWave->GetVolumeMultiplier() * classVolume * sMasterVolume;
                    AUD_SetVolume(i, volLeft, volRight);
#endif
                }

                if (!stopped &&
                    sAudioSources[i].mComponent != nullptr)
                {
                    if (sAudioSources[i].mPitchMult != sAudioSources[i].mComponent->GetPitch())
                    {
                        sAudioSources[i].mPitchMult = sAudioSources[i].mComponent->GetPitch();
                        AUD_SetPitch(i, sAudioSources[i].mPitchMult * classPitch * sMasterPitch);
                    }
                }
            }
        }
    }

    // (2) Play New Sounds
    World* world = GetWorld(0);
    if (world != nullptr)
    {
        const std::vector<Audio3D*>& audioNodes = world->GetAudios();

        for (uint32_t i = 0; i < audioNodes.size(); ++i)
        {
            Audio3D* node = audioNodes[i];

            // In the case that the node is playing, but it is inaudible (not a current sound source)
            // Then we need to check if it should be audible
            if (node->IsPlaying() &&
                !node->IsAudible() &&
                node->GetVolume() > 0.0f &&
                node->GetSoundWave() != nullptr)
            {
                // We need to check the distance to the listener. Should it be audible?
                glm::vec3 nodePosition = node->GetWorldPosition();
                float dist = glm::distance(listenerPos, nodePosition);
                float outerRadius = glm::max(0.0f, node->GetOuterRadius());

                if (dist < outerRadius)
                {
                    // It should be audible, so attempt to add it as a sound source.
                    uint32_t sourceIndex = FindAvailableAudioSourceIndex(node->GetPriority());

                    float soundDuration = node->GetSoundWave()->GetDuration();
                    float startTime = glm::mod(node->GetStartOffset() + node->GetPlayTime(), soundDuration);
                    if (startTime >= soundDuration)
                    {
                        startTime = 0.0f;
                    }

                    if (sourceIndex < MAX_AUDIO_SOURCES)
                    {
                        PlayAudio(
                            sourceIndex,
                            node->GetSoundWave(),
                            node,
                            node->GetVolume(),
                            node->GetPitch(),
                            node->GetPriority(),
                            nodePosition,
                            node->GetInnerRadius(),
                            node->GetOuterRadius(),
                            node->GetAttenuationFunc(),
                            node->GetAudioClass(),
                            node->GetLoop(),
                            startTime);
                    }
                }
            }
        }
    }
}

void AudioManager::PlaySound2D(
    SoundWave* soundWave,
    float volumeMult,
    float pitchMult,
    float startTime,
    bool loop,
    int32_t priority)
{
    uint32_t sourceIndex = FindAvailableAudioSourceIndex(priority);

    if (soundWave != nullptr && 
        sourceIndex < MAX_AUDIO_SOURCES)
    {
        PlayAudio(
            sourceIndex,
            soundWave,
            nullptr,
            volumeMult,
            pitchMult,
            priority,
            glm::vec3(0, 0, 0),
            -1.0f,
            -1.0f,
            AttenuationFunc::Count,
            soundWave->GetAudioClass(),
            loop,
            startTime);
    }
}

void AudioManager::PlaySound3D(
    SoundWave* soundWave,
    glm::vec3 worldPosition,
    float innerRadius,
    float outerRadius,
    AttenuationFunc attenFunc,
    float volumeMult,
    float pitchMult,
    float startTime,
    bool loop,
    int32_t priority)
{
    uint32_t sourceIndex = FindAvailableAudioSourceIndex(priority);

    if (soundWave != nullptr && 
        sourceIndex != MAX_AUDIO_SOURCES)
    {
        PlayAudio(
            sourceIndex,
            soundWave,
            nullptr,
            volumeMult,
            pitchMult,
            priority,
            worldPosition,
            innerRadius,
            outerRadius,
            attenFunc,
            soundWave->GetAudioClass(),
            loop,
            startTime);
    }
}

void AudioManager::UpdateSound(
    SoundWave* soundWave,
    float volume,
    float pitch,
    bool loop,
    int32_t priority)
{
    if (soundWave != nullptr)
    {
        for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
        {
            if (sAudioSources[i].mSoundWave == soundWave)
            {
                sAudioSources[i].mVolumeMult = volume;
                sAudioSources[i].mPitchMult = pitch;
                //sAudioSources[i].mLoop = loop;
                sAudioSources[i].mPriority = priority;

                // Adjust pitch and volume based on soundwave asset and sound class
                float classVolume = GetAudioClassVolume(sAudioSources[i].mAudioClass);
                float classPitch = GetAudioClassPitch(sAudioSources[i].mAudioClass);

                volume = volume * soundWave->GetVolumeMultiplier() * classVolume * sMasterVolume;
                pitch = pitch * soundWave->GetPitchMultiplier() * classPitch * sMasterPitch;

                AUD_SetVolume(i, volume, volume);
                AUD_SetPitch(i, pitch);

                break;
            }
        }
    }
}

void AudioManager::StopComponent(Audio3D* comp)
{
    for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
    {
        if (sAudioSources[i].mComponent == comp)
        {
            StopAudio(i);
            break;
        }
    }
}

void AudioManager::StopSounds(SoundWave* soundWave)
{
    if (soundWave == nullptr)
        return;

    for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
    {
        if (sAudioSources[i].mSoundWave.Get() == soundWave)
        {
            StopAudio(i);
        }
    }
}

void AudioManager::StopSound(const std::string& name)
{
    for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
    {
        SoundWave* soundWave = sAudioSources[i].mSoundWave.Get<SoundWave>();

        if (soundWave && soundWave->GetName() == name)
        {
            StopAudio(i);
        }
    }
}

void AudioManager::StopAllSounds()
{
    for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
    {
        if (sAudioSources[i].mSoundWave.Get() != nullptr)
        {
            StopAudio(i);
        }
    }
}

bool AudioManager::IsSoundPlaying(SoundWave* soundWave)
{
    bool playing = false;

    if (soundWave != nullptr)
    {
        for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
        {
            if (sAudioSources[i].mSoundWave == soundWave)
            {
                playing = true;
                LogDebug("Sound %s is playing at voice %d", soundWave->GetName().c_str(), i);
                break;
            }
        }
    }

    return playing;
}

static void RefreshAudioVolume()
{
    // Refresh volume for 2D sounds (3D sounds will naturally adjust their volume on Update()).
    for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
    {
        if (sAudioSources[i].mSoundWave != nullptr)
        {
            int8_t audioClass = sAudioSources[i].mAudioClass;

            float sourceVolume = sAudioSources[i].mVolumeMult;
            float waveVolume = sAudioSources[i].mSoundWave.Get<SoundWave>()->GetVolumeMultiplier();
            float classVolume = sAudioClassData[audioClass].mVolume;

            float volume = sourceVolume * waveVolume * classVolume * sMasterVolume;
            AUD_SetVolume(i, volume, volume);
        }
    }
}

static void RefreshAudioPitch()
{
    // Refresh pitch for 2D sounds (3D sounds will naturally adjust their volume on Update()).
    for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
    {
        if (sAudioSources[i].mSoundWave != nullptr)
        {
            int8_t audioClass = sAudioSources[i].mAudioClass;

            float sourcePitch = sAudioSources[i].mPitchMult;
            float wavePitch = sAudioSources[i].mSoundWave.Get<SoundWave>()->GetPitchMultiplier();
            float classPitch = sAudioClassData[audioClass].mPitch;

            float pitch = sourcePitch * wavePitch * classPitch * sMasterPitch;
            AUD_SetPitch(i, pitch);
        }
    }
}

void AudioManager::SetAudioClassVolume(int8_t audioClass, float volume)
{
    if (audioClass >= 0 && audioClass < MAX_AUDIO_CLASSES)
    {
        sAudioClassData[audioClass].mVolume = volume;
        RefreshAudioVolume();
    }
}

void AudioManager::SetAudioClassPitch(int8_t audioClass, float pitch)
{
    if (audioClass >= 0 && audioClass < MAX_AUDIO_CLASSES)
    {
        sAudioClassData[audioClass].mPitch = pitch;
        RefreshAudioPitch();
    }
}

float AudioManager::GetAudioClassVolume(int8_t audioClass)
{
    float ret = 1.0f;

    if (audioClass >= 0 && audioClass < MAX_AUDIO_CLASSES)
    {
        ret = sAudioClassData[audioClass].mVolume;
    }

    return ret;
}

float AudioManager::GetAudioClassPitch(int8_t audioClass)
{
    float ret = 1.0f;

    if (audioClass >= 0 && audioClass < MAX_AUDIO_CLASSES)
    {
        ret = sAudioClassData[audioClass].mPitch;
    }

    return ret;
}

void AudioManager::SetMasterVolume(float volume)
{
    if (sMasterVolume != volume)
    {
        sMasterVolume = volume;
        RefreshAudioVolume();
    }
}

void AudioManager::SetMasterPitch(float pitch)
{
    if (sMasterPitch != pitch)
    {
        sMasterPitch = pitch;
        RefreshAudioPitch();
    }
}

float AudioManager::GetMasterVolume()
{
    return sMasterVolume;
}

float AudioManager::GetMasterPitch()
{
    return sMasterPitch;
}

bool AudioManager::GetVoicePcmInfo(uint32_t voiceIndex, AudioAnalysis::PcmView& outView)
{
    outView = AudioAnalysis::PcmView();
    if (voiceIndex >= MAX_AUDIO_SOURCES) return false;

    SoundWave* soundWave = sAudioSources[voiceIndex].mSoundWave.Get<SoundWave>();
    if (soundWave == nullptr) return false;
    if (soundWave->GetWaveData() == nullptr) return false;
    if (soundWave->GetNumSamples() == 0) return false;
    if (soundWave->GetSampleRate() == 0) return false;

    const uint64_t totalFrames = soundWave->GetNumSamples();
    const double cursorSeconds = (double)sAudioSources[voiceIndex].mStartTime
                               + (double)sAudioSources[voiceIndex].mPlaybackTime;
    double cursorFramesD = cursorSeconds * (double)soundWave->GetSampleRate();
    if (sAudioSources[voiceIndex].mLoop && totalFrames > 0)
    {
        cursorFramesD = fmod(cursorFramesD, (double)totalFrames);
        if (cursorFramesD < 0.0) cursorFramesD += (double)totalFrames;
    }
    if (cursorFramesD < 0.0) cursorFramesD = 0.0;
    if (!sAudioSources[voiceIndex].mLoop && cursorFramesD > (double)totalFrames)
    {
        cursorFramesD = (double)totalFrames;
    }

    outView.mData          = soundWave->GetWaveData();
    outView.mSampleRate    = soundWave->GetSampleRate();
    outView.mNumChannels   = soundWave->GetNumChannels();
    outView.mBitsPerSample = soundWave->GetBitsPerSample();
    outView.mCursorFrame   = (uint64_t)cursorFramesD;
    outView.mTotalFrames   = totalFrames;
    outView.mLoop          = sAudioSources[voiceIndex].mLoop;
    outView.mCacheKey      = voiceIndex;
    return true;
}

uint32_t AudioManager::FindVoiceIndex(Audio3D* component)
{
    if (component == nullptr) return MAX_AUDIO_SOURCES;
    for (uint32_t i = 0; i < MAX_AUDIO_SOURCES; ++i)
    {
        if (sAudioSources[i].mComponent == component) return i;
    }
    return MAX_AUDIO_SOURCES;
}

float AudioManager::GetVoiceDuration(uint32_t voiceIndex)
{
    if (voiceIndex >= MAX_AUDIO_SOURCES) return 0.0f;
    SoundWave* wave = sAudioSources[voiceIndex].mSoundWave.Get<SoundWave>();
    return wave ? wave->GetDuration() : 0.0f;
}

float AudioManager::GetVoicePlayTimeNormalized(uint32_t voiceIndex)
{
    if (voiceIndex >= MAX_AUDIO_SOURCES) return 0.0f;
    SoundWave* wave = sAudioSources[voiceIndex].mSoundWave.Get<SoundWave>();
    if (wave == nullptr) return 0.0f;

    const float duration = wave->GetDuration();
    if (duration <= 0.0f) return 0.0f;

    float cursor = sAudioSources[voiceIndex].mStartTime + sAudioSources[voiceIndex].mPlaybackTime;
    if (sAudioSources[voiceIndex].mLoop)
    {
        cursor = fmodf(cursor, duration);
        if (cursor < 0.0f) cursor += duration;
    }
    else
    {
        cursor = glm::clamp(cursor, 0.0f, duration);
    }
    return cursor / duration;
}

