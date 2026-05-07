#if PLATFORM_DOLPHIN

#include "Audio/Audio.h"
#include "Audio/AudioConstants.h"

#include "Assets/SoundWave.h"
#include "Log.h"

#include "System/System.h"

#include <gccore.h>
#include <asndlib.h>

#include <cstring>
#include <deque>

static int32_t sSampleRates[AUDIO_MAX_VOICES] = {};

// ----------------------------------------------------------------------------------------
// Streaming voices (used by the VideoPlayer addon for video-sync'd audio).
//
// libasndlib supports double-buffering per voice: ASND_SetVoice(voice, ..., firstBuffer)
// starts playback; ASND_AddVoice(voice, secondBuffer) queues one more behind it; when the
// first drains the second begins automatically. We layer a software FIFO on top of that
// so callers can submit arbitrarily many chunks ahead and we feed them into the two-slot
// hardware ring as space frees up.
//
// Buffer slot ownership is tracked via ASND_TestPointer(voice, ptr): non-zero while the
// pointer is the voice's current/queued buffer, zero once the voice has retired it. We
// poll that on AUD_Update and on each AUD_SubmitStreamBuffer / AUD_GetStreamPlayedSamples
// so retired buffers are freed and the played-frames counter advances without needing
// the (callback-driven) ASND notification path.
//
// Voice indices: streaming voices use ASND slots [AUDIO_MAX_VOICES, AUDIO_MAX_VOICES + N)
// to avoid colliding with the one-shot SoundWave voices the engine assigns from
// [0, AUDIO_MAX_VOICES). libogc2's MAX_VOICES is 16 so we have headroom.
//
// Thread model: every AUD_* call runs on the engine main thread (AUD_Update fires from
// the engine main loop, video / scripts call from Tick). No mutex needed.
// ----------------------------------------------------------------------------------------

static constexpr uint32_t kMaxStreamingVoices = 4;
static constexpr uint32_t kStreamVoiceBase    = AUDIO_MAX_VOICES;
// libasnd requires 32-byte alignment AND 32-byte size granularity for DSP DMA.
static constexpr uint32_t kStreamAlign = 32;

struct StreamBuf
{
    uint8_t* mData = nullptr;   // 32-byte-aligned, padded to multiple of 32 bytes
    uint32_t mSize = 0;         // padded size handed to ASND
    uint32_t mFrames = 0;       // useful frames (samples-per-channel) the caller submitted
};

struct StreamVoice
{
    bool        mInUse        = false;
    bool        mStarted      = false;  // true once the first ASND_SetVoice has fired since open / flush
    bool        mPaused       = false;
    uint32_t    mAsndVoice    = 0;      // libasnd voice slot
    int32_t     mFormat       = 0;      // VOICE_*_*BIT_LE
    int32_t     mSampleRate   = 0;
    uint32_t    mBytesPerFrame = 0;     // channels * (bitsPerSample / 8)
    uint32_t    mHighWaterBytes = 0;    // residue size at which we emit (~50 ms of audio)
    int32_t     mVolL         = MID_VOLUME;
    int32_t     mVolR         = MID_VOLUME;

    std::deque<StreamBuf> mPending;     // not yet handed to libasnd
    std::deque<StreamBuf> mActive;      // up to 2 entries that ASND owns; head = currently playing

    // libasndlib requires submitted buffer sizes to be 32-byte aligned. Caller
    // chunk sizes (e.g. ~20 ms of 11025 Hz mono 16-bit = 441 bytes) usually
    // aren't, so we accumulate caller bytes here and only ASND_AddVoice them
    // out as 32-byte-aligned chunks — the remainder rolls forward to the next
    // submit. Padding chunks with trailing silence (which the original impl did)
    // creates an audible click at every chunk boundary because ASND plays the
    // silence verbatim before the next real audio starts. Residue accumulation
    // keeps the audio stream gap-free.
    std::vector<uint8_t> mResidue;

    uint64_t    mPlayedFrames = 0;      // cumulative useful frames retired by the voice
};

static StreamVoice sStreams[kMaxStreamingVoices] = {};

static int32_t StreamVoiceFormat(uint32_t numChannels, uint32_t bitsPerSample)
{
    const bool stereo = (numChannels == 2);
    const bool bit16  = (bitsPerSample == 16);
    if (stereo) return bit16 ? VOICE_STEREO_16BIT_LE : VOICE_STEREO_8BIT_U;
    return bit16 ? VOICE_MONO_16BIT_LE : VOICE_MONO_8BIT_U;
}

// Empty callback registered on every streaming voice. We don't actually refill
// from IRQ here (that would need an IRQ-safe queue and is a bigger surgery).
// The callback's *only* job is to make libogc2 keep the voice in SND_WAITING
// when its queued buffers drain — without a callback the voice transitions to
// SND_UNUSED and ASND_AddVoice rejects it, forcing a SetVoice restart on
// recovery (which clicks). With this no-op callback installed, a starved voice
// sits in SND_WAITING and the next AddVoice from the main thread re-arms it
// click-free.
static void StreamVoiceIrqCallback(s32 /*voice*/)
{
    // Intentionally empty.
}

// Drains retired ASND-owned buffers and feeds new pending buffers into the voice's two-slot
// hardware ring. Cheap when there's nothing to do; safe to call frequently.
static void AdvanceStream(StreamVoice& sv)
{
    if (!sv.mInUse) return;

    // Voice status is the authoritative "is anything still queued" signal.
    // ASND_TestPointer can stay sticky for the very last drained buffer (libogc2
    // doesn't always clear snd_buf until a successor is submitted), so we use
    // status to detect the fully-drained case and retire any leftover slots.
    // ASND_StatusVoice returns one of: SND_INVALID(-1), SND_UNUSED(0),
    // SND_WORKING(1), SND_WAITING(2). With our non-NULL callback the drained
    // state is SND_WAITING (not SND_UNUSED). Anything other than SND_WORKING
    // means "voice has nothing in any slot right now" and we can retire active
    // buffers regardless of TestPointer.
    const int32_t status = sv.mStarted ? ASND_StatusVoice(sv.mAsndVoice) : SND_UNUSED;
    const bool voiceIdle = (status != SND_WORKING);

    // 1) Reclaim buffers ASND has finished with.
    while (!sv.mActive.empty())
    {
        const StreamBuf& head = sv.mActive.front();
        if (!voiceIdle && ASND_TestPointer(sv.mAsndVoice, head.mData) != 0)
        {
            // Still playing or queued; nothing to retire yet.
            break;
        }
        sv.mPlayedFrames += head.mFrames;
        SYS_AlignedFree(head.mData);
        sv.mActive.pop_front();
    }

    // 2) Hand pending buffers to ASND while there's room (max 2 in flight per voice).
    while (sv.mActive.size() < 2 && !sv.mPending.empty())
    {
        StreamBuf next = sv.mPending.front();
        sv.mPending.pop_front();

        int32_t rc = SND_OK;
        // Flush the buffer's cache lines before handing the pointer to libasnd.
        // libogc2's libasnd is shipped only as a prebuilt static (no source in
        // devkitPro), so we can't confirm it does its own DCFlushRange. The
        // call is idempotent — if libasnd does flush internally we just pay
        // a redundant cache walk, but if it doesn't the DSP would otherwise
        // DMA stale (cached) bytes and produce static / glitched audio.
        DCFlushRange(next.mData, next.mSize);

        // ASND_AddVoice requires the voice's current status to be other than
        // SND_UNUSED. With our non-NULL callback, a drained voice rests in
        // SND_WAITING (not SND_UNUSED), so AddVoice succeeds even after a
        // starvation event — no SetVoice teardown click needed on recovery.
        // SetVoice is only used for the very first start (or after Flush).
        if (!sv.mStarted)
        {
            rc = ASND_SetVoice(
                sv.mAsndVoice,
                sv.mFormat,
                sv.mSampleRate,
                /*delay=*/0,
                next.mData,
                next.mSize,
                sv.mVolL,
                sv.mVolR,
                /*callback=*/&StreamVoiceIrqCallback);
            sv.mStarted = (rc == SND_OK);
            if (sv.mPaused && rc == SND_OK)
            {
                ASND_PauseVoice(sv.mAsndVoice, 1);
            }
        }
        else
        {
            rc = ASND_AddVoice(sv.mAsndVoice, next.mData, next.mSize);
        }

        if (rc != SND_OK)
        {
            // libasnd wasn't ready (typically SND_BUSY when the second slot is still
            // mid-swap). Push the buffer back to the front of the pending queue and try
            // again next call. We don't drop it.
            sv.mPending.push_front(next);
            break;
        }
        sv.mActive.push_back(next);
    }

    // 3) End-of-stream tail flush. If the voice has fully drained (no active
    // hardware buffers, no pending), but residue has < high-water-mark bytes
    // left over from the last submit, the caller has stopped sending audio.
    // Pad the residue out to 32 bytes with silence and emit it as a final
    // buffer so its frames are accounted for in mPlayedFrames — otherwise the
    // audio clock undershoots the clip duration and the video player's loop
    // trigger never fires (it gates on playheadSec >= clipDuration). The
    // callback keeps the voice in SND_WAITING here (not SND_UNUSED), so we use
    // AddVoice to wake it click-free.
    if (sv.mStarted && sv.mActive.empty() && sv.mPending.empty() && !sv.mResidue.empty())
    {
        const size_t tailBytes = sv.mResidue.size();
        const uint32_t padded = (uint32_t(tailBytes) + (kStreamAlign - 1)) & ~(kStreamAlign - 1);
        StreamBuf b;
        b.mData = (uint8_t*)SYS_AlignedMalloc(padded, kStreamAlign);
        if (b.mData != nullptr)
        {
            memcpy(b.mData, sv.mResidue.data(), tailBytes);
            if (padded > tailBytes)
            {
                memset(b.mData + tailBytes, 0, padded - tailBytes);
            }
            b.mSize   = padded;
            // Count only the caller's real frames toward mPlayedFrames; the
            // padding silence isn't part of the source clip's duration.
            b.mFrames = (sv.mBytesPerFrame > 0) ? uint32_t(tailBytes / sv.mBytesPerFrame) : 0;

            DCFlushRange(b.mData, b.mSize);
            int32_t rc = ASND_AddVoice(sv.mAsndVoice, b.mData, b.mSize);
            if (rc == SND_OK)
            {
                sv.mActive.push_back(b);
                sv.mResidue.clear();
            }
            else
            {
                SYS_AlignedFree(b.mData);
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

void AUD_Initialize()
{
    ASND_Init();
    ASND_Pause(0);
}

void AUD_Shutdown()
{
    ASND_End();
}

void AUD_Update()
{
    for (uint32_t i = 0; i < kMaxStreamingVoices; ++i)
    {
        AdvanceStream(sStreams[i]);
    }
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
    //float startPercent = startTime / soundWave->GetDuration();
    //startPercent = glm::clamp(startPercent, 0.0f, 1.0f);
    //uint32_t startingSample = uint32_t(startPercent * soundWave->GetNumSamples());

    int32_t volumeInt = int32_t(volume * MID_VOLUME);
    int32_t pitchHz = int32_t(soundWave->GetSampleRate() * pitch);
    int32_t voiceFormat = 0;

    sSampleRates[voiceIndex] = soundWave->GetSampleRate();

    bool stereo = (soundWave->GetNumChannels() == 2);
    bool bit16 = (soundWave->GetBitsPerSample() == 16);

    // Wave data is stored in little endian format
    if (stereo)
    {
        voiceFormat = bit16 ? VOICE_STEREO_16BIT_LE : VOICE_STEREO_8BIT_U;
    }
    else
    {
        voiceFormat = bit16 ? VOICE_MONO_16BIT_LE : VOICE_MONO_8BIT_U;
    }

    if (loop)
    {
        ASND_SetInfiniteVoice(
            voiceIndex,
            voiceFormat,
            pitchHz,
            0,
            soundWave->GetWaveData(),
            soundWave->GetWaveDataSize(),
            spatial ? 0 : volumeInt,
            spatial ? 0 : volumeInt);
    }
    else
    {
        ASND_SetVoice(
            voiceIndex,
            voiceFormat,
            pitchHz,
            0,
            soundWave->GetWaveData(),
            soundWave->GetWaveDataSize(),
            spatial ? 0 : volumeInt,
            spatial ? 0 : volumeInt,
            nullptr);
    }
}

void AUD_Stop(uint32_t voiceIndex)
{
    ASND_StopVoice(voiceIndex);

}

bool AUD_IsPlaying(uint32_t voiceIndex)
{
    int32_t status = ASND_StatusVoice(voiceIndex);
    return status == SND_WORKING;
}

void AUD_SetVolume(uint32_t voiceIndex, float leftVolume, float rightVolume)
{
    int32_t leftVolInt = int32_t(MID_VOLUME * leftVolume);
    int32_t rightVolInt = int32_t(MID_VOLUME * rightVolume);
    ASND_ChangeVolumeVoice(voiceIndex, leftVolInt, rightVolInt);
}

void AUD_SetPitch(uint32_t voiceIndex, float pitch)
{
    int32_t pitchHz = int32_t(pitch * sSampleRates[voiceIndex]);
    ASND_ChangePitchVoice(voiceIndex, pitchHz);
}

uint8_t* AUD_AllocWaveBuffer(uint32_t size)
{
    return (uint8_t*)SYS_AlignedMalloc(size, 32);
}

void AUD_FreeWaveBuffer(void* buffer)
{
    SYS_AlignedFree(buffer);
}

void AUD_ProcessWaveBuffer(SoundWave* soundWave)
{

}

// Streaming voices — push-based PCM output for addons that decode at runtime
// (e.g. the VideoPlayer addon's audio track).

uint32_t AUD_OpenStream(uint32_t sampleRate, uint32_t numChannels, uint32_t bitsPerSample)
{
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

        sv.mInUse        = true;
        sv.mStarted      = false;
        sv.mPaused       = false;
        sv.mAsndVoice    = kStreamVoiceBase + i;
        sv.mFormat       = StreamVoiceFormat(numChannels, bitsPerSample);
        sv.mSampleRate   = int32_t(sampleRate);
        sv.mBytesPerFrame = numChannels * (bitsPerSample / 8);
        sv.mVolL         = MID_VOLUME;
        sv.mVolR         = MID_VOLUME;
        sv.mPlayedFrames = 0;

        // Steady-state emission threshold: ~50 ms per chunk. ASND's 2-slot ring
        // then holds ~100 ms of audio, which lets a 30 Hz Wii main thread (with
        // tick jitter from heavy GX uploads / JPEG decode) ride out individual
        // long ticks without the queue draining. Smaller chunks (e.g. each 20 ms
        // decoder slice submitted directly) leave only ~40 ms in the queue, and
        // any tick over 40 ms produces a silent gap that's audible as a wobble
        // / pop especially on loud / dense audio. Round up to 32-byte alignment.
        const uint32_t hw = uint32_t(sampleRate) * sv.mBytesPerFrame * 50u / 1000u;
        sv.mHighWaterBytes = (hw + (kStreamAlign - 1)) & ~(kStreamAlign - 1);
        // mPending / mActive are empty by default; AdvanceStream will populate them as
        // the caller submits.

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
        ASND_StopVoice(sv->mAsndVoice);
    }
    for (StreamBuf& b : sv->mActive)  { if (b.mData) SYS_AlignedFree(b.mData); }
    for (StreamBuf& b : sv->mPending) { if (b.mData) SYS_AlignedFree(b.mData); }
    sv->mActive.clear();
    sv->mPending.clear();
    sv->mResidue.clear();
    sv->mResidue.shrink_to_fit();

    sv->mInUse        = false;
    sv->mStarted      = false;
    sv->mPaused       = false;
    sv->mPlayedFrames = 0;
}

// Pull as many 32-byte-aligned chunks as possible out of the residue buffer and
// hand them to libasndlib via mPending. Leaves the trailing < 32 bytes in the
// residue for the next submit. Caller invokes AdvanceStream after to kick the
// pipeline. Returns true if any chunk was emitted.
static bool DrainResidueAligned(StreamVoice* sv)
{
    const size_t avail = sv->mResidue.size();
    if (avail < kStreamAlign) return false;

    const size_t emit = avail & ~size_t(kStreamAlign - 1); // multiple of 32

    StreamBuf b;
    b.mData = (uint8_t*)SYS_AlignedMalloc(uint32_t(emit), kStreamAlign);
    if (b.mData == nullptr)
    {
        LogError("AUD_SubmitStreamBuffer: SYS_AlignedMalloc(%u) failed", uint32_t(emit));
        return false;
    }
    memcpy(b.mData, sv->mResidue.data(), emit);
    b.mSize   = uint32_t(emit);
    b.mFrames = (sv->mBytesPerFrame > 0) ? uint32_t(emit / sv->mBytesPerFrame) : 0;
    sv->mPending.push_back(b);

    // Carry the remainder forward.
    if (avail > emit)
    {
        memmove(sv->mResidue.data(), sv->mResidue.data() + emit, avail - emit);
    }
    sv->mResidue.resize(avail - emit);
    return true;
}

int32_t AUD_SubmitStreamBuffer(uint32_t streamId, const uint8_t* data, uint32_t byteSize)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr || data == nullptr || byteSize == 0) return 0;

    // libasnd requires submitted buffer sizes to be a multiple of 32 bytes.
    // Caller chunks (~20 ms of audio) typically aren't aligned, so we accumulate
    // bytes in mResidue and emit larger 32-byte-aligned chunks downstream.
    sv->mResidue.insert(sv->mResidue.end(), data, data + byteSize);

    // Emission policy: by default, hold residue until it reaches the high-water
    // mark (~50 ms) so each emitted chunk fills a meaningful fraction of ASND's
    // 2-slot ring. Bigger chunks → deeper queue (~100 ms) → tolerates Wii main
    // thread tick jitter (heavy GX uploads / JPEG decode pulling tick rate
    // below 30 Hz) without the queue draining and producing audible gaps. Cost
    // is a one-time ~50 ms delay before audio starts after Open (or after Flush).
    //
    // Exception: a mid-stream starvation event (mActive empty after the voice
    // started). Waiting for high-water there extends the silent gap further;
    // emit whatever aligned residue we have and let the queue rebuild on the
    // next high-water hit.
    const bool starving = sv->mStarted && sv->mActive.empty();
    bool shouldEmit = false;
    if (starving)
    {
        shouldEmit = (sv->mResidue.size() >= kStreamAlign);
    }
    else
    {
        shouldEmit = (sv->mResidue.size() >= sv->mHighWaterBytes);
    }

    if (shouldEmit)
    {
        bool any = DrainResidueAligned(sv);
        if (any)
        {
            AdvanceStream(*sv);
        }
    }

    return int32_t(byteSize);
}

uint64_t AUD_GetStreamPlayedSamples(uint32_t streamId)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr) return 0;

    // Drain any retired buffers before reporting so the clock stays as fresh as possible
    // (between this and the per-frame AUD_Update, granularity is one frame at most).
    AdvanceStream(*sv);
    return sv->mPlayedFrames;
}

void AUD_SetStreamVolume(uint32_t streamId, float volume)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr) return;

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    sv->mVolL = int32_t(volume * MID_VOLUME);
    sv->mVolR = int32_t(volume * MID_VOLUME);
    if (sv->mStarted)
    {
        ASND_ChangeVolumeVoice(sv->mAsndVoice, sv->mVolL, sv->mVolR);
    }
}

void AUD_SetStreamPaused(uint32_t streamId, bool paused)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr) return;
    if (sv->mPaused == paused) return;

    sv->mPaused = paused;
    if (sv->mStarted)
    {
        ASND_PauseVoice(sv->mAsndVoice, paused ? 1 : 0);
    }
}

void AUD_FlushStream(uint32_t streamId)
{
    StreamVoice* sv = GetStreamFromId(streamId);
    if (sv == nullptr) return;

    if (sv->mStarted)
    {
        ASND_StopVoice(sv->mAsndVoice);
    }
    for (StreamBuf& b : sv->mActive)  { if (b.mData) SYS_AlignedFree(b.mData); }
    for (StreamBuf& b : sv->mPending) { if (b.mData) SYS_AlignedFree(b.mData); }
    sv->mActive.clear();
    sv->mPending.clear();
    sv->mResidue.clear();
    // mPlayedFrames is left as-is per the AUD_FlushStream contract: callers are
    // expected to snapshot it post-flush and treat subsequent readings as deltas.
    sv->mStarted = false;
}

#endif