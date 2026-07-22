#include "AudioManager.h"
#include "Assets/SoundWave.h"
#include "Asset.h"
#include "AssetManager.h"
#include "AssetRef.h"
#include "System/System.h"
#include "Log.h"

#include <vector>
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

// ============================================================================
//  Streaming sources — disk-streamed long audio (background music).
//  Separate from the 8 static voice slots: these feed the push-PCM stream API
//  (AUD_OpenStream/SubmitStreamBuffer) from a file read in small chunks, so the
//  whole track never sits resident in RAM (the point on tight-memory consoles).
//  Pumped every frame from Update(). Modelled on the VideoPlayer addon's proven
//  pattern: retry-stash a rejected chunk, prebuffer before unpausing, loop by
//  seeking, and never drop a chunk (drops become audible gaps). 2D only.
// ============================================================================
static constexpr uint32_t kMaxStreamingSources = 4;
// Per read size. On slow-media consoles (Dreamcast GD-ROM) each blocking fs read
// carries a large fixed command latency (~150 ms in emulation, which models it as
// a whole-machine stall). Read in big, rare chunks so the render hitch is
// infrequent — 128 KB ≈ one read per ~6 s of this 22 kHz audio. (Aligning reads to
// engage KOS's continuous streaming DMA is faster but flycast/RetroArch don't
// emulate that GD-ROM command — it produced silence — so we stick with per-read
// DMA + big chunks.) Desktop I/O is fast; keep 16 KB.
#if defined(POLYPHASE_PLATFORM_ADDON)
static constexpr uint32_t kStreamChunkBytes    = 128 * 1024;
#else
static constexpr uint32_t kStreamChunkBytes    = 16 * 1024;
#endif
static constexpr float    kStreamPrebufferSec  = 0.15f;     // buffer before unpausing

struct StreamingSource
{
    SoundWaveRef mSoundWave;
    Audio3D*     mComponent    = nullptr; // owning Audio3D node (null for 2D/Lua plays)
    uint32_t     mStreamId     = 0;      // AUD_OpenStream id; 0 = slot free
    SysFile*     mFile         = nullptr;
    uint64_t     mPcmOffset    = 0;      // file byte offset of PCM start
    uint64_t     mPcmSize      = 0;      // total SOURCE PCM bytes (on disk)
    uint64_t     mReadPos      = 0;      // SOURCE PCM bytes read so far (0..mPcmSize)
    uint32_t     mSrcFrameBytes = 2;     // channels * (srcBits/8) — on-disk frame size
    uint32_t     mFrameBytes   = 4;      // channels * 2 — SUBMIT (always-16-bit) frame size
    bool         mExpand8to16  = false;  // source is 8-bit unsigned; widen to s16 before submit
    float        mSampleRate   = 44100.0f;
    bool         mLoop         = false;
    bool         mUnpaused     = false;
    float        mSubmittedSec = 0.0f;   // seconds submitted so far (prebuffer gate)
    std::vector<uint8_t> mChunk;         // reusable SOURCE read buffer
    std::vector<uint8_t> mSubmit;        // reusable submit buffer (== mChunk, or widened)
    std::vector<uint8_t> mPending;       // stashed submit bytes when the queue was full
    bool         mPendingValid = false;
};
static StreamingSource sStreamingSources[kMaxStreamingSources];

// Streaming pump threading. On slow-media consoles the per-chunk disc read
// (~150 ms) would freeze the render frame if pumped from Update(). Instead a
// dedicated I/O thread runs the pump; sStreamMutex guards sStreamingSources so
// the (rare) main-thread start/stop calls don't race the pump. The render frame
// no longer touches streaming at all. Desktop keeps the fast main-thread pump.
static MutexObject* sStreamMutex = nullptr;
#if defined(POLYPHASE_PLATFORM_ADDON)
static ThreadObject* sStreamThread    = nullptr;
static volatile bool sStreamThreadRun = false;
#endif

static inline float StreamBytesToSec(const StreamingSource& s, uint32_t bytes)
{
    const float denom = s.mSampleRate * (float)s.mFrameBytes;
    return denom > 0.0f ? (float)bytes / denom : 0.0f;
}

static void CloseStreamingSlot(StreamingSource& s)
{
    if (s.mStreamId != 0) { AUD_CloseStream(s.mStreamId); s.mStreamId = 0; }
    if (s.mFile != nullptr) { SYS_FileClose(s.mFile); s.mFile = nullptr; }
    if (s.mComponent != nullptr) { s.mComponent->NotifyAudible(false); s.mComponent = nullptr; }
    s.mSoundWave = nullptr;
    s.mPcmOffset = s.mPcmSize = s.mReadPos = 0;
    s.mUnpaused = false;
    s.mSubmittedSec = 0.0f;
    s.mPendingValid = false;
    s.mPending.clear();
}

static bool IsStreamingPlaying(SoundWave* sw)
{
    SCOPED_LOCK(sStreamMutex);   // guards vs the streaming I/O thread (console)
    for (uint32_t i = 0; i < kMaxStreamingSources; ++i)
        if (sStreamingSources[i].mStreamId != 0 && sStreamingSources[i].mSoundWave.Get() == sw)
            return true;
    return false;
}

static void StopStreamingBySound(SoundWave* sw)
{
    SCOPED_LOCK(sStreamMutex);
    for (uint32_t i = 0; i < kMaxStreamingSources; ++i)
        if (sStreamingSources[i].mSoundWave.Get() == sw)
            CloseStreamingSlot(sStreamingSources[i]);
}

static void StopAllStreaming()
{
    SCOPED_LOCK(sStreamMutex);
    for (uint32_t i = 0; i < kMaxStreamingSources; ++i)
        CloseStreamingSlot(sStreamingSources[i]);
}

// Begin streaming a SoundWave marked mStreaming from disk. `component` is the
// owning Audio3D node (nullptr for 2D / Lua plays); when set, it's marked audible
// so the auto-start loop doesn't re-trigger it, and un-marked when the stream ends.
// Returns false (and falls through to silence) if there's no free slot, the file
// can't be opened, or the platform has no streaming backend (AUD_OpenStream = 0).
static bool StartStreamingMusic(SoundWave* sw, Audio3D* component, float volumeMult, bool loop)
{
    SCOPED_LOCK(sStreamMutex);   // guards vs the streaming I/O thread (console)

    // No PCM to stream (e.g. an asset packaged before the streaming fix, or a bad
    // cook). Reject up front — otherwise the pump would open a stream, immediately
    // hit "fully played" (readPos 0 >= size 0), close it, un-mark the node audible,
    // and the auto-start loop would re-trigger it every frame forever.
    if (sw == nullptr || sw->GetStreamPcmSize() == 0)
    {
        LogWarning("AudioManager: streaming '%s' has 0 PCM bytes (repackage needed?) — skipping",
                   sw != nullptr ? sw->GetName().c_str() : "<null>");
        return false;
    }

    StreamingSource* slot = nullptr;
    for (uint32_t i = 0; i < kMaxStreamingSources; ++i)
    {
        if (sStreamingSources[i].mStreamId == 0 && sStreamingSources[i].mSoundWave.Get() == nullptr)
        {
            slot = &sStreamingSources[i];
            break;
        }
    }
    if (slot == nullptr)
    {
        LogWarning("AudioManager: no free streaming slot for '%s'", sw->GetName().c_str());
        return false;
    }

    // Resolve the .oct path from the asset stub so we can re-open it for reading.
    AssetStub* stub = AssetManager::Get()->GetAssetStubByUuid(sw->GetUuid());
    if (stub == nullptr) stub = AssetManager::Get()->GetAssetStub(sw->GetName());
    if (stub == nullptr || stub->mPath.empty())
    {
        LogWarning("AudioManager: streaming '%s' has no on-disk path (embedded?)", sw->GetName().c_str());
        return false;
    }

    SysFile* file = SYS_FileOpenRead(stub->mPath.c_str(), true);
    if (file == nullptr)
    {
        LogWarning("AudioManager: streaming can't open '%s'", stub->mPath.c_str());
        return false;
    }

    const uint32_t channels = (sw->GetNumChannels() > 0) ? sw->GetNumChannels() : 1;
    const uint32_t rate     = sw->GetSampleRate();
    const uint32_t srcBits  = (sw->GetBitsPerSample() == 8) ? 8u : 16u;

    // The streaming backends are all 16-bit only. If the asset is 8-bit unsigned PCM,
    // the pump widens each byte to s16 (mExpand8to16) before submitting — so the
    // backend always sees 16-bit and we don't need per-platform 8-bit stream support.
    const uint32_t streamId = AUD_OpenStream(rate, channels, 16);
    if (streamId == 0)
    {
        SYS_FileClose(file);
        LogWarning("AudioManager: AUD_OpenStream failed for '%s' (streaming unavailable on this platform)",
                   sw->GetName().c_str());
        return false;
    }

    SYS_FileSeek(file, sw->GetStreamPcmOffset());

    slot->mSoundWave     = sw;
    slot->mComponent     = component;
    slot->mStreamId      = streamId;
    slot->mFile          = file;
    slot->mPcmOffset     = sw->GetStreamPcmOffset();
    slot->mPcmSize       = sw->GetStreamPcmSize();
    slot->mReadPos       = 0;
    slot->mSrcFrameBytes = channels * (srcBits / 8);   // on-disk frame size (2 or 4, or 1/2 for 8-bit)
    slot->mFrameBytes    = channels * 2;               // submit frame size (always 16-bit)
    slot->mExpand8to16   = (srcBits == 8);
    slot->mSampleRate    = (float)rate;
    slot->mLoop          = loop;
    slot->mUnpaused      = false;
    slot->mSubmittedSec  = 0.0f;
    slot->mPendingValid  = false;
    if (slot->mChunk.size()  < kStreamChunkBytes)     slot->mChunk.resize(kStreamChunkBytes);
    if (slot->mSubmit.size() < kStreamChunkBytes * 2) slot->mSubmit.resize(kStreamChunkBytes * 2); // 8→16 doubles

    const int8_t cls = glm::clamp<int8_t>(sw->GetAudioClass(), 0, MAX_AUDIO_CLASSES - 1);
    const float volume = volumeMult * sw->GetVolumeMultiplier() * sAudioClassData[cls].mVolume * sMasterVolume;
    AUD_SetStreamVolume(streamId, volume);
    AUD_SetStreamPaused(streamId, true);   // stays paused until the prebuffer fills (pump)

    if (component != nullptr)
    {
        component->NotifyAudible(true);    // so the auto-start loop treats it as playing
    }

    LogDebug("AudioManager: streaming '%s' (%u Hz, %u ch, %u-bit%s, %llu PCM bytes, loop=%d)",
             sw->GetName().c_str(), rate, channels, srcBits, slot->mExpand8to16 ? "->16" : "",
             (unsigned long long)slot->mPcmSize, (int)loop);
    return true;
}

// Per-frame pump: keep each streaming voice's queue fed from disk. Runs on the
// MAIN thread (file I/O must never happen on the audio mixer thread).
static void UpdateStreamingSources()
{
    for (uint32_t i = 0; i < kMaxStreamingSources; ++i)
    {
        StreamingSource& s = sStreamingSources[i];
        if (s.mStreamId == 0) continue;

        // 0) If an owning Audio3D node stopped playing (user called Stop, went out of
        //    range, etc.), tear the stream down — CloseStreamingSlot clears NotifyAudible.
        if (s.mComponent != nullptr && !s.mComponent->IsPlaying())
        {
            CloseStreamingSlot(s);
            continue;
        }

        // 1) Resubmit a stashed chunk first — never drop it. The backend may accept
        //    only part of it (a chunk can exceed the ring's free space), so consume
        //    exactly what it took and keep the rest pending.
        if (s.mPendingValid)
        {
            const uint32_t pend = (uint32_t)s.mPending.size();
            const int32_t  acc  = AUD_SubmitStreamBuffer(s.mStreamId, s.mPending.data(), pend);
            if (acc <= 0) continue;                       // queue still full — retry next frame
            s.mSubmittedSec += StreamBytesToSec(s, (uint32_t)acc);
            if ((uint32_t)acc < pend)
            {
                s.mPending.erase(s.mPending.begin(), s.mPending.begin() + acc);
                continue;                                 // remainder still pending
            }
            s.mPendingValid = false;
            s.mPending.clear();
        }

        // 2) Read + submit chunks until the queue fills or we reach end-of-stream.
        while (!s.mPendingValid)
        {
            uint64_t remaining = s.mPcmSize - s.mReadPos;
            if (remaining < s.mSrcFrameBytes)   // no full source frame left (incl. odd tail)
            {
                if (s.mLoop) { SYS_FileSeek(s.mFile, s.mPcmOffset); s.mReadPos = 0; remaining = s.mPcmSize; }
                else { s.mReadPos = s.mPcmSize; break; }   // consume tail so step 4 can end it
            }

            uint32_t want = (uint32_t)((remaining < (uint64_t)kStreamChunkBytes) ? remaining : kStreamChunkBytes);
            want -= (s.mSrcFrameBytes > 0 ? (want % s.mSrcFrameBytes) : 0);   // whole SOURCE frames only
            if (want == 0) break;

            const uint32_t got = SYS_FileRead(s.mFile, s.mChunk.data(), want);
            if (got == 0) break;   // read error / short read
            s.mReadPos += got;

            // Widen 8-bit unsigned → s16 if needed; the backend only takes 16-bit.
            const uint8_t* submitData;
            uint32_t       submitBytes;
            if (s.mExpand8to16)
            {
                int16_t* dst = reinterpret_cast<int16_t*>(s.mSubmit.data());
                for (uint32_t b = 0; b < got; ++b)
                {
                    dst[b] = (int16_t)(((int32_t)s.mChunk[b] - 128) << 8);
                }
                submitData  = s.mSubmit.data();
                submitBytes = got * 2;
            }
            else
            {
                submitData  = s.mChunk.data();
                submitBytes = got;
            }

            const int32_t acc = AUD_SubmitStreamBuffer(s.mStreamId, submitData, submitBytes);
            if (acc <= 0)
            {
                // Queue full — stash the whole chunk for next frame.
                s.mPending.assign(submitData, submitData + submitBytes);
                s.mPendingValid = true;
                break;
            }
            s.mSubmittedSec += StreamBytesToSec(s, (uint32_t)acc);
            if ((uint32_t)acc < submitBytes)
            {
                // Partial accept (chunk exceeded ring free space) — stash the remainder.
                s.mPending.assign(submitData + acc, submitData + submitBytes);
                s.mPendingValid = true;
                break;
            }
        }

        // 3) Prebuffer gate — unpause once enough is queued.
        if (!s.mUnpaused && s.mSubmittedSec >= kStreamPrebufferSec)
        {
            AUD_SetStreamPaused(s.mStreamId, false);
            s.mUnpaused = true;
        }

        // 4) One-shot end: close after the backend has played everything out.
        //    Frame counts are in SOURCE frames (AUD_GetStreamPlayedSamples returns the
        //    stream's source-rate frame cursor), so divide by mSrcFrameBytes not the
        //    16-bit submit frame size.
        if (!s.mLoop && !s.mPendingValid && s.mPcmSize > 0 && s.mReadPos >= s.mPcmSize && s.mSrcFrameBytes > 0)
        {
            const uint64_t playedFrames = AUD_GetStreamPlayedSamples(s.mStreamId);
            const uint64_t totalFrames  = s.mPcmSize / s.mSrcFrameBytes;
            if (playedFrames >= totalFrames) CloseStreamingSlot(s);
        }
    }
}

#if defined(POLYPHASE_PLATFORM_ADDON)
// Dedicated streaming I/O thread (console): runs the pump — including the slow
// blocking disc read — off the render thread, so the frame never stalls. Holds
// sStreamMutex during the read; only the rare main-thread start/stop calls
// contend for it (never the render loop).
static ThreadFuncRet StreamingIOThread(void* /*arg*/)
{
    while (sStreamThreadRun)
    {
        {
            SCOPED_LOCK(sStreamMutex);
            UpdateStreamingSources();
        }
        SYS_Sleep(5);   // ~200 Hz; ring + SPU buffer comfortably cover the gap
    }
    THREAD_RETURN();
}
#endif

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
    sStreamMutex = SYS_CreateMutex();
#if defined(POLYPHASE_PLATFORM_ADDON)
    // NOTE: a dedicated streaming I/O thread (StreamingIOThread) is available but
    // left DISABLED — on the emulated GD-ROM each fs read command blocks the whole
    // machine (~150 ms), so moving it to a thread doesn't unblock the render frame.
    // Real hardware may benefit; flip sStreamThreadRun on to try. The practical
    // mitigation is a large read chunk (fewer reads) — see kStreamChunkBytes.
    sStreamThreadRun = false;
    (void)&StreamingIOThread;
#endif
}

void AudioManager::Shutdown()
{
#if defined(POLYPHASE_PLATFORM_ADDON)
    sStreamThreadRun = false;
    if (sStreamThread != nullptr)
    {
        SYS_JoinThread(sStreamThread);
        SYS_DestroyThread(sStreamThread);
        sStreamThread = nullptr;
    }
#endif
    StopAllStreaming();
    if (sStreamMutex != nullptr) { SYS_DestroyMutex(sStreamMutex); sStreamMutex = nullptr; }
}

void AudioManager::Update(float deltaTime)
{
    SCOPED_FRAME_STAT("Audio");

    // Feed disk-streamed music voices (background music). On console the pump
    // runs on the dedicated I/O thread (StreamingIOThread) so the slow disc read
    // never stalls the frame; only pump here when that thread isn't running.
#if defined(POLYPHASE_PLATFORM_ADDON)
    if (!sStreamThreadRun)
#endif
    {
        SCOPED_LOCK(sStreamMutex);
        UpdateStreamingSources();
    }

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
                    // Streaming assets (long BGM) never enter the static voice pool — they
                    // feed the push-PCM stream API from disk. StartStreamingMusic marks the
                    // node audible on success so this loop stops re-triggering it, and clears
                    // that when the stream ends or the node stops.
                    SoundWave* streamWave = node->GetSoundWave();
                    if (streamWave != nullptr && streamWave->IsStreaming() && streamWave->GetStreamPcmSize() > 0)
                    {
                        StartStreamingMusic(streamWave, node, node->GetVolume(), node->GetLoop());
                        continue;
                    }

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
    // Streaming assets (long BGM) don't use the static voice pool — they feed the
    // push-PCM stream API from disk. Re-playing restarts the track. GetStreamPcmSize()
    // is only populated on the memory-tight console runtime; in the editor a Streaming
    // asset full-loads into mWaveData (size stays 0), so fall through to resident play.
    if (soundWave != nullptr && soundWave->IsStreaming() && soundWave->GetStreamPcmSize() > 0)
    {
        StopStreamingBySound(soundWave);
        StartStreamingMusic(soundWave, nullptr, volumeMult, loop);
        return;
    }

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
    // Streaming assets play non-spatially (2D). Route to the streaming path — but only
    // when stream metadata exists (console runtime); the editor full-loads it (size 0).
    if (soundWave != nullptr && soundWave->IsStreaming() && soundWave->GetStreamPcmSize() > 0)
    {
        StopStreamingBySound(soundWave);
        StartStreamingMusic(soundWave, nullptr, volumeMult, loop);
        return;
    }

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


void AudioManager::PlaySoundAtPosition(
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
    return PlaySound3D(
        soundWave,
        worldPosition,
        innerRadius,
        outerRadius,
        attenFunc,
        volumeMult,
        pitchMult,
        startTime,
        loop,
        priority);
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

    // Also tear down a streaming voice this node owns (streaming BGM never enters
    // the static pool above). Needed when the node is destroyed or stopped.
    {
        SCOPED_LOCK(sStreamMutex);   // guards vs the streaming I/O thread (console)
        for (uint32_t i = 0; i < kMaxStreamingSources; ++i)
        {
            if (sStreamingSources[i].mStreamId != 0 &&
                sStreamingSources[i].mComponent == comp)
            {
                CloseStreamingSlot(sStreamingSources[i]);
                break;
            }
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

    StopStreamingBySound(soundWave);   // also stop it if it's a streaming source
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

    StopAllStreaming();
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

        if (!playing && IsStreamingPlaying(soundWave))
        {
            playing = true;
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

