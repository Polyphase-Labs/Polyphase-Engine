#if PLATFORM_3DS

#include "Audio/Audio.h"
#include "Audio/AudioConstants.h"

#include "Assets/SoundWave.h"
#include "Log.h"

#include <3ds.h>

#include <cstring>
#include <deque>

// Audio backend: try NDSP first, fall back to CSND.
enum AudioBackend
{
    AUDIO_BACKEND_NONE,
    AUDIO_BACKEND_NDSP,
    AUDIO_BACKEND_CSND
};

static AudioBackend sBackend = AUDIO_BACKEND_NONE;
static float sSampleRates[AUDIO_MAX_VOICES] = {};

// ----------------------------------------------------------------------------------------
// Streaming voices (NDSP backend only — see CSND note below).
//
// NDSP supports queued-buffer playback natively: ndspChnWaveBufAdd appends a wave buf
// onto the channel's internal queue, and each wbuf transitions FREE -> QUEUED -> PLAYING
// -> DONE as the DSP consumes it. We own a fixed pool of in-flight wbuf slots per voice
// (the structs must persist while NDSP holds them) and overflow into a software FIFO of
// pending buffers when the pool fills up.
//
// Buffer retirement is detected by polling each slot's wbuf.status against NDSP_WBUF_DONE.
// AdvanceStream runs every frame from AUD_Update plus opportunistically inside
// AUD_SubmitStreamBuffer (so the very first submit starts playback this frame instead of
// next) and AUD_GetStreamPlayedSamples (so the audio-master video clock stays fresh).
//
// Voice channels: streaming uses NDSP channel indices [AUDIO_MAX_VOICES, +N) so they never
// collide with the engine's 8-slot one-shot pool. NDSP exposes 24 channels total.
//
// CSND backend: csndPlaySound is one-shot only and lacks a queue mechanism that would
// make video-sync'd streaming viable without per-frame channel restart hitches. When the
// 3DS falls back to CSND (no DSP firmware), AUD_OpenStream returns 0 and the addon plays
// the video silently — same as the pre-implementation stub behavior.
//
// Thread model: all AUD_* calls run on the engine main thread. No mutex needed.
// ----------------------------------------------------------------------------------------

static constexpr uint32_t kMaxStreamingVoices = 4;
static constexpr uint32_t kStreamChannelBase  = AUDIO_MAX_VOICES;
static constexpr uint32_t kStreamInFlightCap  = 4;
// linearAlloc returns 16-byte aligned. We round buffer sizes to 32 bytes for DSP cache-
// flush granularity (DSP_FlushDataCache works on cache lines).
static constexpr uint32_t kStreamAlign = 32;

struct StreamSlot
{
    ndspWaveBuf mWaveBuf = {};
    uint8_t*    mData    = nullptr;  // linearAlloc'd; null = slot free
    uint32_t    mFrames  = 0;        // useful frames (caller bytes / bytesPerFrame)
};

struct StreamPending
{
    uint8_t* mData       = nullptr;  // linearAlloc'd; ownership passes to a slot when promoted
    uint32_t mPaddedSize = 0;        // alloc size, 32-byte multiple
    uint32_t mFrames     = 0;
};

struct StreamVoice
{
    bool        mInUse        = false;
    bool        mStarted      = false;
    bool        mPaused       = false;
    uint32_t    mChannel      = 0;       // NDSP channel index
    uint16_t    mFormat       = 0;       // NDSP_FORMAT_*
    uint32_t    mSampleRate   = 0;
    uint32_t    mBytesPerFrame = 0;
    float       mVolume       = 1.0f;

    StreamSlot               mSlots[kStreamInFlightCap];
    std::deque<StreamPending> mPending;

    uint64_t    mPlayedFrames = 0;
};

static StreamVoice sStreams[kMaxStreamingVoices] = {};

static uint16_t StreamVoiceFormat(uint32_t numChannels, uint32_t bitsPerSample)
{
    const bool stereo = (numChannels == 2);
    const bool bit16  = (bitsPerSample == 16);
    if (stereo) return bit16 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_STEREO_PCM8;
    return bit16 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_MONO_PCM8;
}

static void ApplyStreamMix(uint32_t channel, float volume)
{
    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = volume; // front L
    mix[1] = volume; // front R
    ndspChnSetMix(channel, mix);
}

// Drains retired slots and promotes pending buffers into NDSP's queue.
static void AdvanceStream(StreamVoice& sv)
{
    if (!sv.mInUse) return;

    // 1) Reclaim slots that NDSP has finished with.
    for (uint32_t i = 0; i < kStreamInFlightCap; ++i)
    {
        StreamSlot& s = sv.mSlots[i];
        if (s.mData != nullptr && s.mWaveBuf.status == NDSP_WBUF_DONE)
        {
            sv.mPlayedFrames += s.mFrames;
            linearFree(s.mData);
            s.mData = nullptr;
            s.mFrames = 0;
            memset(&s.mWaveBuf, 0, sizeof(s.mWaveBuf));
        }
    }

    // 2) Promote pending buffers into any free slots, up to NDSP's queue capacity.
    for (uint32_t i = 0; i < kStreamInFlightCap; ++i)
    {
        if (sv.mPending.empty()) break;

        StreamSlot& s = sv.mSlots[i];
        if (s.mData != nullptr) continue; // slot busy

        StreamPending p = sv.mPending.front();
        sv.mPending.pop_front();

        s.mData   = p.mData;
        s.mFrames = p.mFrames;

        memset(&s.mWaveBuf, 0, sizeof(s.mWaveBuf));
        s.mWaveBuf.data_vaddr = s.mData;
        s.mWaveBuf.nsamples   = p.mFrames;
        s.mWaveBuf.looping    = false;

        DSP_FlushDataCache(s.mData, p.mPaddedSize);
        ndspChnWaveBufAdd(sv.mChannel, &s.mWaveBuf);

        if (!sv.mStarted)
        {
            // First submit since open / flush. Pause-state is honored after we add
            // the first buffer so the channel doesn't free-run during startup.
            sv.mStarted = true;
            if (sv.mPaused)
            {
                ndspChnSetPaused(sv.mChannel, true);
            }
        }
    }
}

static StreamVoice* GetStreamFromId(uint32_t streamId)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return nullptr;
    StreamVoice& sv = sStreams[streamId - 1];
    return sv.mInUse ? &sv : nullptr;
}

// NDSP state
static ndspWaveBuf sWaveBufs[AUDIO_MAX_VOICES] = {};

// CSND state
// CSND channels 8-31 are available for user audio. Map voice 0-7 to channels 8-15.
#define CSND_CHANNEL_BASE 8
// CSND is mono per channel; for stereo sources we mix down to a mono buffer.
static uint8_t* sCsndMonoBuffers[AUDIO_MAX_VOICES] = {};
static uint32_t sCsndMonoSizes[AUDIO_MAX_VOICES] = {};
static bool sCsndPlaying[AUDIO_MAX_VOICES] = {};

void AUD_Initialize()
{
    // Try NDSP first (requires DSP firmware)
    LogDebug("AUD_Initialize: trying ndspInit...");
    Result rc = ndspInit();
    if (R_SUCCEEDED(rc))
    {
        sBackend = AUDIO_BACKEND_NDSP;
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);

        for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i)
        {
            sWaveBufs[i].status = NDSP_WBUF_DONE;
        }
        LogDebug("AUD_Initialize: NDSP initialized successfully");
        return;
    }
    LogWarning("AUD_Initialize: ndspInit failed (0x%08lx), trying CSND...", rc);

    // Fall back to CSND
    rc = csndInit();
    if (R_SUCCEEDED(rc))
    {
        sBackend = AUDIO_BACKEND_CSND;
        LogDebug("AUD_Initialize: CSND initialized successfully");
        return;
    }

    LogError("AUD_Initialize: Both NDSP and CSND failed. No audio available.");
}

void AUD_Shutdown()
{
    if (sBackend == AUDIO_BACKEND_NDSP)
    {
        ndspExit();
    }
    else if (sBackend == AUDIO_BACKEND_CSND)
    {
        for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i)
        {
            if (sCsndMonoBuffers[i] != nullptr)
            {
                linearFree(sCsndMonoBuffers[i]);
                sCsndMonoBuffers[i] = nullptr;
            }
        }
        csndExit();
    }
    sBackend = AUDIO_BACKEND_NONE;
}

void AUD_Update()
{
    if (sBackend != AUDIO_BACKEND_NDSP) return;

    for (uint32_t i = 0; i < kMaxStreamingVoices; ++i)
    {
        AdvanceStream(sStreams[i]);
    }
}

// Helper: convert interleaved stereo to mono by averaging L+R samples.
// Returns a linearAlloc'd buffer that the caller must track and free.
static uint8_t* MixStereoToMono(const uint8_t* stereoData, uint32_t numSamples, bool bit16, uint32_t& outSize)
{
    uint32_t monoSamples = numSamples / 2;
    uint32_t bytesPerSample = bit16 ? 2 : 1;
    outSize = monoSamples * bytesPerSample;

    uint8_t* mono = (uint8_t*)linearAlloc(outSize);
    if (mono == nullptr)
    {
        return nullptr;
    }

    if (bit16)
    {
        const int16_t* src = (const int16_t*)stereoData;
        int16_t* dst = (int16_t*)mono;
        for (uint32_t i = 0; i < monoSamples; ++i)
        {
            int32_t left = src[i * 2];
            int32_t right = src[i * 2 + 1];
            dst[i] = (int16_t)((left + right) / 2);
        }
    }
    else
    {
        for (uint32_t i = 0; i < monoSamples; ++i)
        {
            int32_t left = (int8_t)stereoData[i * 2];
            int32_t right = (int8_t)stereoData[i * 2 + 1];
            mono[i] = (uint8_t)(int8_t)((left + right) / 2);
        }
    }

    return mono;
}

void AUD_Play(
    uint32_t voiceIndex,
    SoundWave* soundWave,
    float volume,
    float pitch,
    bool loop,
    float startTime,
    bool spatial)
{
    if (sBackend == AUDIO_BACKEND_NONE)
    {
        return;
    }

    if (soundWave->GetWaveData() == nullptr || soundWave->GetWaveDataSize() == 0)
    {
        LogError("AUD_Play: No wave data (data=%p size=%u)", soundWave->GetWaveData(), soundWave->GetWaveDataSize());
        return;
    }

    bool stereo = (soundWave->GetNumChannels() == 2);
    bool bit16 = (soundWave->GetBitsPerSample() == 16);
    uint32_t sampleRate = soundWave->GetSampleRate();
    uint32_t nsamples = soundWave->GetNumSamples() / soundWave->GetNumChannels();

    sSampleRates[voiceIndex] = (float)sampleRate;

    float volumeLeft = spatial ? 0.0f : volume;
    float volumeRight = spatial ? 0.0f : volume;

    LogDebug("AUD_Play [%s]: voice=%u ch=%u bits=%u rate=%u samples=%u size=%u vol=%.2f pitch=%.2f loop=%d",
        (sBackend == AUDIO_BACKEND_NDSP) ? "NDSP" : "CSND",
        voiceIndex, soundWave->GetNumChannels(), soundWave->GetBitsPerSample(),
        sampleRate, nsamples, soundWave->GetWaveDataSize(),
        volume, pitch, (int)loop);

    if (sBackend == AUDIO_BACKEND_NDSP)
    {
        uint16_t voiceFormat = 0;
        if (stereo)
        {
            voiceFormat = bit16 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_STEREO_PCM8;
        }
        else
        {
            voiceFormat = bit16 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_MONO_PCM8;
        }

        ndspChnReset(voiceIndex);
        ndspChnSetInterp(voiceIndex, NDSP_INTERP_LINEAR);
        ndspChnSetRate(voiceIndex, (float)sampleRate * pitch);
        ndspChnSetFormat(voiceIndex, voiceFormat);

        float mix[12];
        memset(mix, 0, sizeof(mix));
        mix[0] = volumeLeft;
        mix[1] = volumeRight;
        ndspChnSetMix(voiceIndex, mix);

        memset(&sWaveBufs[voiceIndex], 0, sizeof(ndspWaveBuf));
        sWaveBufs[voiceIndex].data_vaddr = soundWave->GetWaveData();
        sWaveBufs[voiceIndex].nsamples = nsamples;
        sWaveBufs[voiceIndex].looping = loop;

        DSP_FlushDataCache(soundWave->GetWaveData(), soundWave->GetWaveDataSize());
        ndspChnWaveBufAdd(voiceIndex, &sWaveBufs[voiceIndex]);
    }
    else if (sBackend == AUDIO_BACKEND_CSND)
    {
        uint32_t chn = CSND_CHANNEL_BASE + voiceIndex;

        // Free previous mono mixdown buffer if any
        if (sCsndMonoBuffers[voiceIndex] != nullptr)
        {
            linearFree(sCsndMonoBuffers[voiceIndex]);
            sCsndMonoBuffers[voiceIndex] = nullptr;
            sCsndMonoSizes[voiceIndex] = 0;
        }

        uint8_t* playData = soundWave->GetWaveData();
        uint32_t playSize = soundWave->GetWaveDataSize();

        // CSND channels are mono. Mix stereo down to mono.
        if (stereo)
        {
            uint32_t monoSize = 0;
            uint8_t* monoData = MixStereoToMono(playData, soundWave->GetNumSamples(), bit16, monoSize);
            if (monoData == nullptr)
            {
                LogError("AUD_Play [CSND]: Failed to allocate mono mixdown buffer");
                return;
            }
            sCsndMonoBuffers[voiceIndex] = monoData;
            sCsndMonoSizes[voiceIndex] = monoSize;
            playData = monoData;
            playSize = monoSize;
        }

        u32 flags = SOUND_LINEAR_INTERP;
        flags |= bit16 ? SOUND_FORMAT_16BIT : SOUND_FORMAT_8BIT;
        flags |= loop ? SOUND_REPEAT : SOUND_ONE_SHOT;

        float vol = spatial ? 0.0f : volume;
        float pan = 0.0f; // center

        uint32_t pitchedRate = (uint32_t)((float)sampleRate * pitch);

        GSPGPU_FlushDataCache(playData, playSize);

        csndPlaySound(chn, flags, pitchedRate, vol, pan,
            playData,
            loop ? playData : NULL,
            playSize);

        sCsndPlaying[voiceIndex] = true;
        LogDebug("AUD_Play [CSND]: playing on channel %u", chn);
    }
}

void AUD_Stop(uint32_t voiceIndex)
{
    if (sBackend == AUDIO_BACKEND_NDSP)
    {
        ndspChnWaveBufClear(voiceIndex);
        sWaveBufs[voiceIndex].status = NDSP_WBUF_DONE;
    }
    else if (sBackend == AUDIO_BACKEND_CSND)
    {
        uint32_t chn = CSND_CHANNEL_BASE + voiceIndex;
        CSND_SetPlayState(chn, 0);
        csndExecCmds(true);
        sCsndPlaying[voiceIndex] = false;

        if (sCsndMonoBuffers[voiceIndex] != nullptr)
        {
            linearFree(sCsndMonoBuffers[voiceIndex]);
            sCsndMonoBuffers[voiceIndex] = nullptr;
            sCsndMonoSizes[voiceIndex] = 0;
        }
    }
}

bool AUD_IsPlaying(uint32_t voiceIndex)
{
    if (sBackend == AUDIO_BACKEND_NDSP)
    {
        return sWaveBufs[voiceIndex].status != NDSP_WBUF_DONE;
    }
    else if (sBackend == AUDIO_BACKEND_CSND)
    {
        if (!sCsndPlaying[voiceIndex])
        {
            return false;
        }

        uint32_t chn = CSND_CHANNEL_BASE + voiceIndex;
        u8 playing = 0;
        csndIsPlaying(chn, &playing);
        if (playing == 0)
        {
            sCsndPlaying[voiceIndex] = false;

            if (sCsndMonoBuffers[voiceIndex] != nullptr)
            {
                linearFree(sCsndMonoBuffers[voiceIndex]);
                sCsndMonoBuffers[voiceIndex] = nullptr;
                sCsndMonoSizes[voiceIndex] = 0;
            }
        }
        return playing != 0;
    }

    return false;
}

void AUD_SetVolume(uint32_t voiceIndex, float leftVolume, float rightVolume)
{
    if (sBackend == AUDIO_BACKEND_NDSP)
    {
        float mix[12];
        memset(mix, 0, sizeof(mix));
        mix[0] = leftVolume;
        mix[1] = rightVolume;
        ndspChnSetMix(voiceIndex, mix);
    }
    else if (sBackend == AUDIO_BACKEND_CSND)
    {
        uint32_t chn = CSND_CHANNEL_BASE + voiceIndex;
        // Pack L/R volumes into the format CSND expects
        u32 lvol = (u32)(glm::clamp(leftVolume, 0.0f, 1.0f) * 0x8000);
        u32 rvol = (u32)(glm::clamp(rightVolume, 0.0f, 1.0f) * 0x8000);
        u32 volumes = lvol | (rvol << 16);
        CSND_SetVol(chn, volumes, volumes);
        csndExecCmds(true);
    }
}

void AUD_SetPitch(uint32_t voiceIndex, float pitch)
{
    if (sBackend == AUDIO_BACKEND_NDSP)
    {
        float pitchHz = pitch * sSampleRates[voiceIndex];
        ndspChnSetRate(voiceIndex, pitchHz);
    }
    else if (sBackend == AUDIO_BACKEND_CSND)
    {
        uint32_t chn = CSND_CHANNEL_BASE + voiceIndex;
        u32 sampleRate = (u32)(pitch * sSampleRates[voiceIndex]);
        u32 timer = CSND_TIMER(sampleRate);
        if (timer < 0x0042) timer = 0x0042;
        else if (timer > 0xFFFF) timer = 0xFFFF;
        CSND_SetTimer(chn, timer);
        csndExecCmds(true);
    }
}

uint8_t* AUD_AllocWaveBuffer(uint32_t size)
{
    uint8_t* buffer = (uint8_t*)linearAlloc(size);
    if (buffer == nullptr)
    {
        LogError("AUD_AllocWaveBuffer: linearAlloc failed for size %u", size);
    }
    return buffer;
}

void AUD_FreeWaveBuffer(void* buffer)
{
    linearFree(buffer);
}

void AUD_ProcessWaveBuffer(SoundWave* soundWave)
{
    if (soundWave->GetBitsPerSample() == 8)
    {
        uint8_t* waveData = soundWave->GetWaveData();

        // Convert from unsigned to signed PCM
        for (uint32_t i = 0; i < soundWave->GetNumSamples(); ++i)
        {
            int32_t signedValue = (int32_t) waveData[i];
            signedValue -= 128;
            signedValue = glm::clamp(signedValue, int32_t(-128), int32_t(127));
            int8_t signedChar = (int8_t)signedValue;
            memcpy(&waveData[i], &signedChar, sizeof(int8_t));
        }
    }
}

// Streaming voices — push-based PCM output for addons that decode at runtime
// (e.g. the VideoPlayer addon's audio track). NDSP backend only; CSND fallback
// returns 0 so callers play video silently.

uint32_t AUD_OpenStream(uint32_t sampleRate, uint32_t numChannels, uint32_t bitsPerSample)
{
    if (sBackend != AUDIO_BACKEND_NDSP)
    {
        // CSND fallback or audio uninitialized — no streaming path. Return 0 so the
        // caller (VideoPlayer addon) gracefully falls back to video-only playback.
        return 0;
    }
    if (numChannels != 1 && numChannels != 2)
    {
        LogWarning("AUD_OpenStream: only mono/stereo supported (got %u channels)", numChannels);
        return 0;
    }
    if (bitsPerSample != 8 && bitsPerSample != 16)
    {
        LogWarning("AUD_OpenStream: only 8 / 16-bit PCM supported (got %u bps)", bitsPerSample);
        return 0;
    }

    for (uint32_t i = 0; i < kMaxStreamingVoices; ++i)
    {
        StreamVoice& sv = sStreams[i];
        if (sv.mInUse) continue;

        sv.mInUse         = true;
        sv.mStarted       = false;
        sv.mPaused        = false;
        sv.mChannel       = kStreamChannelBase + i;
        sv.mFormat        = StreamVoiceFormat(numChannels, bitsPerSample);
        sv.mSampleRate    = sampleRate;
        sv.mBytesPerFrame = numChannels * (bitsPerSample / 8);
        sv.mVolume        = 1.0f;
        sv.mPlayedFrames  = 0;
        for (uint32_t s = 0; s < kStreamInFlightCap; ++s)
        {
            sv.mSlots[s].mData = nullptr;
            sv.mSlots[s].mFrames = 0;
            memset(&sv.mSlots[s].mWaveBuf, 0, sizeof(ndspWaveBuf));
        }

        ndspChnReset(sv.mChannel);
        ndspChnSetInterp(sv.mChannel, NDSP_INTERP_LINEAR);
        ndspChnSetRate(sv.mChannel, float(sampleRate));
        ndspChnSetFormat(sv.mChannel, sv.mFormat);
        ApplyStreamMix(sv.mChannel, sv.mVolume);

        return i + 1; // 0 is the "not available" sentinel
    }

    LogWarning("AUD_OpenStream: no free streaming voices (pool size %u)", kMaxStreamingVoices);
    return 0;
}

void AUD_CloseStream(uint32_t streamId)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr) return;

    if (sv->mStarted)
    {
        ndspChnWaveBufClear(sv->mChannel);
        ndspChnReset(sv->mChannel);
    }
    for (uint32_t s = 0; s < kStreamInFlightCap; ++s)
    {
        if (sv->mSlots[s].mData != nullptr)
        {
            linearFree(sv->mSlots[s].mData);
            sv->mSlots[s].mData = nullptr;
            sv->mSlots[s].mFrames = 0;
            memset(&sv->mSlots[s].mWaveBuf, 0, sizeof(ndspWaveBuf));
        }
    }
    for (StreamPending& p : sv->mPending)
    {
        if (p.mData != nullptr) linearFree(p.mData);
    }
    sv->mPending.clear();

    sv->mInUse        = false;
    sv->mStarted      = false;
    sv->mPaused       = false;
    sv->mPlayedFrames = 0;
}

int32_t AUD_SubmitStreamBuffer(uint32_t streamId, const uint8_t* data, uint32_t byteSize)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr || data == nullptr || byteSize == 0) return 0;

    // Round to 32 bytes for DSP_FlushDataCache granularity. Pad bytes are silence;
    // the played-frames counter only counts caller-submitted useful frames so the
    // clock isn't skewed by the padding.
    const uint32_t padded = (byteSize + (kStreamAlign - 1)) & ~(kStreamAlign - 1);

    StreamPending p;
    p.mData = (uint8_t*)linearAlloc(padded);
    if (p.mData == nullptr)
    {
        LogError("AUD_SubmitStreamBuffer: linearAlloc(%u) failed", padded);
        return 0;
    }
    memcpy(p.mData, data, byteSize);
    if (padded > byteSize)
    {
        memset(p.mData + byteSize, 0, padded - byteSize);
    }
    p.mPaddedSize = padded;
    p.mFrames     = (sv->mBytesPerFrame > 0) ? (byteSize / sv->mBytesPerFrame) : 0;

    sv->mPending.push_back(p);

    // Kick the pipeline immediately so first-submit starts playback this frame.
    AdvanceStream(*sv);

    return int32_t(byteSize);
}

uint64_t AUD_GetStreamPlayedSamples(uint32_t streamId)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr) return 0;

    AdvanceStream(*sv);
    return sv->mPlayedFrames;
}

void AUD_SetStreamVolume(uint32_t streamId, float volume)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr) return;

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    sv->mVolume = volume;
    ApplyStreamMix(sv->mChannel, sv->mVolume);
}

void AUD_SetStreamPaused(uint32_t streamId, bool paused)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr) return;
    if (sv->mPaused == paused) return;

    sv->mPaused = paused;
    if (sv->mStarted)
    {
        ndspChnSetPaused(sv->mChannel, paused);
    }
}

void AUD_FlushStream(uint32_t streamId)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr) return;

    if (sv->mStarted)
    {
        ndspChnWaveBufClear(sv->mChannel);
    }
    for (uint32_t s = 0; s < kStreamInFlightCap; ++s)
    {
        if (sv->mSlots[s].mData != nullptr)
        {
            linearFree(sv->mSlots[s].mData);
            sv->mSlots[s].mData = nullptr;
            sv->mSlots[s].mFrames = 0;
            memset(&sv->mSlots[s].mWaveBuf, 0, sizeof(ndspWaveBuf));
        }
    }
    for (StreamPending& p : sv->mPending)
    {
        if (p.mData != nullptr) linearFree(p.mData);
    }
    sv->mPending.clear();
    // mPlayedFrames is left as-is per the AUD_FlushStream contract: callers snapshot
    // it post-flush and treat subsequent readings as deltas.
    sv->mStarted = false;
}

#endif
