#if PLATFORM_WINDOWS

#include "Audio/Audio.h"
#include "Audio/AudioAnalysis.h"
#include "Audio/AudioConstants.h"
#include "System/System.h"

#include "Assets/SoundWave.h"
#include "Log.h"

#include <xaudio2.h>

#include <atomic>
#include <cstring>

static IXAudio2* sXAudio2 = nullptr;
static IXAudio2MasteringVoice* sMasterVoice = nullptr;
static IXAudio2SourceVoice* sSourceVoices[AUDIO_MAX_VOICES] = { };
static XAUDIO2_BUFFER sSourceBuffers[AUDIO_MAX_VOICES] = { };
static uint8_t* sStereoConvertedBuffers[AUDIO_MAX_VOICES] = { };

// ----- Streaming voices (declared here so AUD_Shutdown can tear them down) -----
static constexpr uint32_t kMaxStreamingVoices = 4;

// XAudio2 voice callback that frees the staging buffer passed via pContext when the
// corresponding buffer finishes playing. Defined up here (not later) so AUD_Shutdown
// can `delete` instances without the "incomplete type" warning.
struct StreamCallback : public IXAudio2VoiceCallback
{
    uint32_t mVoiceIndex = 0;

    STDMETHOD_(void, OnBufferEnd)(void* bufferContext) override;

    // Unused callbacks — must be stubbed for IXAudio2VoiceCallback.
    STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) override {}
    STDMETHOD_(void, OnVoiceProcessingPassEnd)()          override {}
    STDMETHOD_(void, OnStreamEnd)()                       override {}
    STDMETHOD_(void, OnBufferStart)(void*)                override {}
    STDMETHOD_(void, OnLoopEnd)(void*)                    override {}
    STDMETHOD_(void, OnVoiceError)(void*, HRESULT)        override {}
};

struct StreamingVoiceEntry
{
    IXAudio2SourceVoice* mVoice = nullptr;
    StreamCallback*      mCallback = nullptr;
    uint32_t             mSampleRate = 0;
    uint32_t             mNumChannels = 0;
    uint32_t             mBitsPerSample = 0;
    bool                 mInUse = false;
    bool                 mPaused = false;
    std::atomic<uint32_t> mPendingBuffers { 0 };
};

static StreamingVoiceEntry sStreamingVoices[kMaxStreamingVoices];

// An attempt to reuse source voices?
//struct WaveFormat
//{
//    uint32_t mSampleRate = 44100;
//    uint8_t mNumChannels = 2;
//    uint8_t mBitsPerSample = 16;
//};
//static WaveFormat sLastWaveFormats[AUDIO_MAX_VOICES] = {};

void AUD_Initialize()
{
    if (XAudio2Create(&sXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR) < 0)
    {
        LogError("Failed to create XAudio2 engine");
    }

    if (sXAudio2->CreateMasteringVoice(&sMasterVoice) < 0)
    {
        LogError("Failed to create mastering voice");
    }

    // TODO: Prime the XAudio2 internal memory pool for source voices by allocating many.
    // And then immediately destroying them I suppose?
}

void AUD_Shutdown()
{
    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i)
    {
        if (sSourceVoices[i] != nullptr)
        {
            sSourceVoices[i]->DestroyVoice();
            // TODO: Do we need to call delete??
            sSourceVoices[i] = nullptr;
        }
    }
    // Tear down any streaming voices still open (the VideoPlayer addon may have been
    // playing at shutdown). Without this, the IXAudio2SourceVoice instances outlive the
    // XAudio2 engine and any stray access from addon teardown crashes.
    for (uint32_t i = 0; i < kMaxStreamingVoices; ++i)
    {
        StreamingVoiceEntry& entry = sStreamingVoices[i];
        if (entry.mVoice != nullptr)
        {
            entry.mVoice->Stop(0);
            entry.mVoice->FlushSourceBuffers();
            entry.mVoice->DestroyVoice();
            entry.mVoice = nullptr;
        }
        if (entry.mCallback != nullptr)
        {
            delete entry.mCallback;
            entry.mCallback = nullptr;
        }
        entry.mInUse = false;
        entry.mPaused = false;
        entry.mPendingBuffers.store(0);
    }
    sMasterVoice->DestroyVoice();
    sXAudio2->Release();
    sMasterVoice = nullptr;
    sXAudio2 = nullptr;
}

void AUD_Update()
{

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
    OCT_ASSERT(sSourceVoices[voiceIndex] == nullptr);

    bool monoInput = (soundWave->GetNumChannels() == 1);
    if (monoInput)
    {
        OCT_ASSERT(sStereoConvertedBuffers[voiceIndex] == nullptr);
        sStereoConvertedBuffers[voiceIndex] = new uint8_t[soundWave->GetWaveDataSize() * 2];

        uint32_t numSamples = soundWave->GetNumSamples();
        uint32_t sampleSize = (soundWave->GetBitsPerSample() == 8) ? 1 : 2;

        uint8_t* srcData = soundWave->GetWaveData();
        uint8_t* dstData = sStereoConvertedBuffers[voiceIndex];

        for (uint32_t i = 0; i < numSamples; ++i)
        {
            memcpy(dstData, srcData, sampleSize);
            memcpy(dstData + sampleSize, srcData, sampleSize);

            srcData += sampleSize;
            dstData += (sampleSize * 2);
        }
    }

    float startPercent = startTime / soundWave->GetDuration();
    startPercent = glm::clamp(startPercent, 0.0f, 1.0f);
    uint32_t startingSample = uint32_t(startPercent * soundWave->GetNumSamples());

    sSourceBuffers[voiceIndex].Flags = XAUDIO2_END_OF_STREAM;
    sSourceBuffers[voiceIndex].AudioBytes = soundWave->GetWaveDataSize();
    sSourceBuffers[voiceIndex].pAudioData = soundWave->GetWaveData();
    sSourceBuffers[voiceIndex].PlayBegin = startingSample;
    sSourceBuffers[voiceIndex].PlayLength = 0;
    sSourceBuffers[voiceIndex].LoopBegin = 0;
    sSourceBuffers[voiceIndex].LoopLength = 0;
    sSourceBuffers[voiceIndex].LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = soundWave->GetNumChannels();
    waveFormat.nSamplesPerSec = soundWave->GetSampleRate();
    waveFormat.wBitsPerSample = (uint16_t)soundWave->GetBitsPerSample();
    waveFormat.nBlockAlign = soundWave->GetBlockAlign();
    waveFormat.nAvgBytesPerSec = soundWave->GetByteRate();
    waveFormat.cbSize = 0;

    if (monoInput)
    {
        // Use ephemeral stereo buffer
        sSourceBuffers[voiceIndex].AudioBytes *= 2;
        sSourceBuffers[voiceIndex].pAudioData = sStereoConvertedBuffers[voiceIndex];
        waveFormat.nAvgBytesPerSec *= 2;
        waveFormat.nBlockAlign *= 2;
        waveFormat.nChannels = 2;
    }

    if (sXAudio2->CreateSourceVoice(&sSourceVoices[voiceIndex], &waveFormat) >= 0)
    {
        sSourceVoices[voiceIndex]->SubmitSourceBuffer(&sSourceBuffers[voiceIndex]);

        // Spatial sounds will update their volume every frame.
        sSourceVoices[voiceIndex]->SetVolume(spatial ? 0.0f : volume);

        sSourceVoices[voiceIndex]->SetFrequencyRatio(pitch);
        sSourceVoices[voiceIndex]->Start();
    }
    else
    {
        LogError("Error creating XAUDIO2 source voice");
        OCT_ASSERT(0);
    }
}

void AUD_Stop(uint32_t voiceIndex)
{
    OCT_ASSERT(sSourceVoices[voiceIndex] != nullptr);
    sSourceVoices[voiceIndex]->Stop();
    sSourceVoices[voiceIndex]->DestroyVoice();
    sSourceVoices[voiceIndex] = nullptr;

    if (sStereoConvertedBuffers[voiceIndex] != nullptr)
    {
        delete sStereoConvertedBuffers[voiceIndex];
        sStereoConvertedBuffers[voiceIndex] = nullptr;
    }
}

bool AUD_IsPlaying(uint32_t voiceIndex)
{
    OCT_ASSERT(sSourceVoices[voiceIndex] != nullptr);
    XAUDIO2_VOICE_STATE state;
    sSourceVoices[voiceIndex]->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    return (state.BuffersQueued != 0);
}

void AUD_SetVolume(uint32_t voiceIndex, float leftVolume, float rightVolume)
{
    OCT_ASSERT(sSourceVoices[voiceIndex] != nullptr);

    // Use this version to set volume of all channels
    // sSourceVoices[voiceIndex]->SetVolume((leftVolume + rightVolume) / 2.0f);

    // Use this version to set volume of left/right ear
    sSourceVoices[voiceIndex]->SetVolume(1.0f);
    float volumes[2] = { leftVolume, rightVolume };
    sSourceVoices[voiceIndex]->SetChannelVolumes(2, volumes);
}

void AUD_SetPitch(uint32_t voiceIndex, float pitch)
{
    OCT_ASSERT(sSourceVoices[voiceIndex] != nullptr);
    sSourceVoices[voiceIndex]->SetFrequencyRatio(pitch);
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

// ================================================================================================
// Streaming voices (XAudio2 implementation)
// Declarations for StreamingVoiceEntry / sStreamingVoices / kMaxStreamingVoices are at the
// top of this file so AUD_Shutdown can tear them down.
// ================================================================================================

// Out-of-line definition for the StreamCallback::OnBufferEnd method declared at the top
// of the file. Lives here (not inline up top) so it can reach sStreamingVoices and
// SYS_AlignedFree without needing more forward decls above.
void StreamCallback::OnBufferEnd(void* bufferContext)
{
    if (bufferContext != nullptr)
    {
        SYS_AlignedFree(bufferContext);
    }
    if (mVoiceIndex < kMaxStreamingVoices)
    {
        // Best-effort pending count. Race-free enough: worst case we report one more
        // pending than actually queued for a few microseconds.
        uint32_t prev = sStreamingVoices[mVoiceIndex].mPendingBuffers.load();
        if (prev > 0)
        {
            sStreamingVoices[mVoiceIndex].mPendingBuffers.store(prev - 1);
        }
    }
}

uint32_t AUD_OpenStream(uint32_t sampleRate, uint32_t numChannels, uint32_t bitsPerSample)
{
    if (sXAudio2 == nullptr || sMasterVoice == nullptr)
    {
        // Either audio never initialized or already shut down. Return 0 silently —
        // callers (e.g. VideoPlayer3D) treat 0 as "no streaming voice available" and
        // gracefully fall back to video-only playback.
        return 0;
    }
    if (numChannels != 1 && numChannels != 2)
    {
        LogWarning("AUD_OpenStream: only mono/stereo supported (got %u channels)", numChannels);
        return 0;
    }
    if (bitsPerSample != 16)
    {
        LogWarning("AUD_OpenStream: only 16-bit PCM supported (got %u bps)", bitsPerSample);
        return 0;
    }

    for (uint32_t i = 0; i < kMaxStreamingVoices; ++i)
    {
        if (!sStreamingVoices[i].mInUse)
        {
            StreamingVoiceEntry& entry = sStreamingVoices[i];

            WAVEFORMATEX waveFormat = {};
            waveFormat.wFormatTag      = WAVE_FORMAT_PCM;
            waveFormat.nChannels       = (WORD)numChannels;
            waveFormat.nSamplesPerSec  = sampleRate;
            waveFormat.wBitsPerSample  = (WORD)bitsPerSample;
            waveFormat.nBlockAlign     = waveFormat.nChannels * (waveFormat.wBitsPerSample / 8);
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

            entry.mCallback = new StreamCallback();
            entry.mCallback->mVoiceIndex = i;

            HRESULT hr = sXAudio2->CreateSourceVoice(
                &entry.mVoice,
                &waveFormat,
                0, XAUDIO2_DEFAULT_FREQ_RATIO,
                entry.mCallback,
                nullptr, nullptr);
            if (FAILED(hr) || entry.mVoice == nullptr)
            {
                LogError("AUD_OpenStream: CreateSourceVoice failed (hr=0x%08lx)", (unsigned long)hr);
                delete entry.mCallback;
                entry.mCallback = nullptr;
                return 0;
            }

            // Force frequency ratio to 1.0 so playback rate matches the declared sample
            // rate (a stale default > 1 would pitch-shift the output).
            entry.mVoice->SetFrequencyRatio(1.0f);
            entry.mVoice->Start(0);
            entry.mSampleRate     = sampleRate;
            entry.mNumChannels    = numChannels;
            entry.mBitsPerSample  = bitsPerSample;
            entry.mInUse          = true;
            entry.mPaused         = false;
            entry.mPendingBuffers.store(0);

            const uint32_t streamId = i + 1;
            AudioAnalysis::OnStreamOpened(streamId, sampleRate, numChannels, bitsPerSample);
            return streamId; // 0 is sentinel for invalid
        }
    }

    LogWarning("AUD_OpenStream: no free streaming voices (pool size %u)", kMaxStreamingVoices);
    return 0;
}

void AUD_CloseStream(uint32_t streamId)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;
    AudioAnalysis::OnStreamClosed(streamId);
    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse) return;

    if (entry.mVoice != nullptr)
    {
        entry.mVoice->Stop(0);
        entry.mVoice->FlushSourceBuffers();
        entry.mVoice->DestroyVoice();
        entry.mVoice = nullptr;
    }

    // Any buffers still queued had their OnBufferEnd fired during FlushSourceBuffers.
    if (entry.mCallback != nullptr)
    {
        delete entry.mCallback;
        entry.mCallback = nullptr;
    }

    entry.mInUse = false;
    entry.mPaused = false;
    entry.mSampleRate = 0;
    entry.mNumChannels = 0;
    entry.mBitsPerSample = 0;
    entry.mPendingBuffers.store(0);
}

int32_t AUD_SubmitStreamBuffer(uint32_t streamId, const uint8_t* data, uint32_t byteSize)
{
    if (sXAudio2 == nullptr) return 0;
    if (streamId == 0 || streamId > kMaxStreamingVoices) return 0;
    if (data == nullptr || byteSize == 0) return 0;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse || entry.mVoice == nullptr) return 0;

    // Soft backpressure: cap pending buffers so the engine doesn't grow unboundedly if the
    // producer outruns playback (e.g. looping while paused). The producer should retry.
    if (entry.mPendingBuffers.load() >= 16) return 0;

    // Take a private copy: XAudio2 holds the pointer until OnBufferEnd, and our callers
    // reuse their source buffer on the next submit.
    uint8_t* copy = (uint8_t*)SYS_AlignedMalloc(byteSize, 16);
    if (copy == nullptr) return 0;
    std::memcpy(copy, data, byteSize);

    XAUDIO2_BUFFER buffer = {};
    buffer.AudioBytes = byteSize;
    buffer.pAudioData = copy;
    buffer.pContext   = copy; // freed in OnBufferEnd

    HRESULT hr = entry.mVoice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr))
    {
        SYS_AlignedFree(copy);
        LogWarning("AUD_SubmitStreamBuffer: SubmitSourceBuffer failed (hr=0x%08lx)", (unsigned long)hr);
        return 0;
    }

    entry.mPendingBuffers.fetch_add(1);
    AudioAnalysis::OnStreamSubmitted(streamId, data, byteSize);
    return (int32_t)byteSize;
}

uint64_t AUD_GetStreamPlayedSamples(uint32_t streamId)
{
    if (sXAudio2 == nullptr) return 0;
    if (streamId == 0 || streamId > kMaxStreamingVoices) return 0;
    const StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse || entry.mVoice == nullptr) return 0;

    XAUDIO2_VOICE_STATE state = {};
    entry.mVoice->GetState(&state, 0 /* include SamplesPlayed */);
    return state.SamplesPlayed;
}

void AUD_SetStreamVolume(uint32_t streamId, float volume)
{
    if (sXAudio2 == nullptr) return;
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;
    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse || entry.mVoice == nullptr) return;
    entry.mVoice->SetVolume(volume);
}

void AUD_SetStreamPaused(uint32_t streamId, bool paused)
{
    if (sXAudio2 == nullptr) return;
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;
    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse || entry.mVoice == nullptr) return;
    if (paused && !entry.mPaused)
    {
        entry.mVoice->Stop(0);
        entry.mPaused = true;
    }
    else if (!paused && entry.mPaused)
    {
        entry.mVoice->Start(0);
        entry.mPaused = false;
    }
}

void AUD_FlushStream(uint32_t streamId)
{
    if (sXAudio2 == nullptr) return;
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;
    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse || entry.mVoice == nullptr) return;
    // Stop→Flush→(Start if not paused) so FlushSourceBuffers fires OnBufferEnd for each
    // queued buffer. That reclaims our staging allocations and empties the voice's queue.
    const bool wasPlaying = !entry.mPaused;
    entry.mVoice->Stop(0);
    entry.mVoice->FlushSourceBuffers();
    entry.mPendingBuffers.store(0);
    if (wasPlaying) entry.mVoice->Start(0);
}

#endif
