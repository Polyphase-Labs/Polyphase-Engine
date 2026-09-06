#if PLATFORM_MAC

// macOS audio backend (AudioToolbox / AudioUnit, plain C API — no ObjC needed).
//
// Mixer half: the same software mixer as Audio_Linux.cpp, but instead of
// pushing into ALSA we fill a lock-free single-producer/single-consumer ring
// that a DefaultOutput AudioUnit render callback drains on the CoreAudio
// thread. Streaming half mirrors the voice-pool / 1-based-id / soft
// backpressure contract of Audio_Windows.cpp and Audio_Linux.cpp, one
// AudioUnit per streaming voice.

#include "Audio/Audio.h"
#include "Audio/AudioAnalysis.h"
#include "Audio/AudioConstants.h"
#include "System/System.h"

#include "Assets/SoundWave.h"
#include "Log.h"
#include "Maths.h"

#include <AudioToolbox/AudioToolbox.h>
#include <atomic>
#include <cstring>

// ---------------------------------------------------------------------------
// SPSC ring of int16 samples. Indices are monotonic; capacity is a power of 2.
// ---------------------------------------------------------------------------
struct SampleRing
{
    int16_t* mData = nullptr;
    uint32_t mCapacity = 0;   // samples
    uint32_t mMask = 0;
    std::atomic<uint32_t> mRead { 0 };
    std::atomic<uint32_t> mWrite { 0 };
    std::atomic<bool> mReset { false };

    void Init(uint32_t capacitySamples)
    {
        uint32_t cap = 1;
        while (cap < capacitySamples) cap <<= 1;
        mCapacity = cap;
        mMask = cap - 1;
        mData = new int16_t[cap];
        memset(mData, 0, cap * sizeof(int16_t));
        mRead.store(0);
        mWrite.store(0);
        mReset.store(false);
    }

    void Destroy()
    {
        delete[] mData;
        mData = nullptr;
        mCapacity = 0;
    }

    uint32_t Readable() const
    {
        return mWrite.load(std::memory_order_acquire) - mRead.load(std::memory_order_acquire);
    }

    uint32_t Writable() const
    {
        return mCapacity - Readable();
    }

    // Producer side.
    void Write(const int16_t* src, uint32_t numSamples)
    {
        uint32_t w = mWrite.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < numSamples; ++i)
        {
            mData[(w + i) & mMask] = src[i];
        }
        mWrite.store(w + numSamples, std::memory_order_release);
    }

    // Consumer side. Returns samples actually read; the rest of dst is zeroed.
    uint32_t Read(int16_t* dst, uint32_t numSamples)
    {
        if (mReset.exchange(false))
        {
            mRead.store(mWrite.load(std::memory_order_acquire), std::memory_order_release);
        }

        uint32_t r = mRead.load(std::memory_order_relaxed);
        uint32_t avail = mWrite.load(std::memory_order_acquire) - r;
        uint32_t n = (avail < numSamples) ? avail : numSamples;
        for (uint32_t i = 0; i < n; ++i)
        {
            dst[i] = mData[(r + i) & mMask];
        }
        if (n < numSamples)
        {
            memset(dst + n, 0, (numSamples - n) * sizeof(int16_t));
        }
        mRead.store(r + n, std::memory_order_release);
        return n;
    }
};

static AudioUnit CreateOutputUnit(uint32_t sampleRate, uint32_t numChannels, AURenderCallback callback, void* userData)
{
    AudioComponentDescription desc = {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
    if (component == nullptr)
    {
        LogError("CoreAudio: default output component not found");
        return nullptr;
    }

    AudioUnit unit = nullptr;
    OSStatus status = AudioComponentInstanceNew(component, &unit);
    if (status != noErr || unit == nullptr)
    {
        LogError("CoreAudio: AudioComponentInstanceNew failed (%d)", (int)status);
        return nullptr;
    }

    AudioStreamBasicDescription fmt = {};
    fmt.mSampleRate = sampleRate;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    fmt.mBitsPerChannel = 16;
    fmt.mChannelsPerFrame = numChannels;
    fmt.mBytesPerFrame = 2 * numChannels;
    fmt.mFramesPerPacket = 1;
    fmt.mBytesPerPacket = fmt.mBytesPerFrame;

    status = AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &fmt, sizeof(fmt));
    if (status != noErr)
    {
        LogError("CoreAudio: failed to set stream format (%d)", (int)status);
        AudioComponentInstanceDispose(unit);
        return nullptr;
    }

    AURenderCallbackStruct cb = {};
    cb.inputProc = callback;
    cb.inputProcRefCon = userData;
    status = AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb));
    if (status != noErr)
    {
        LogError("CoreAudio: failed to set render callback (%d)", (int)status);
        AudioComponentInstanceDispose(unit);
        return nullptr;
    }

    status = AudioUnitInitialize(unit);
    if (status != noErr)
    {
        LogError("CoreAudio: AudioUnitInitialize failed (%d)", (int)status);
        AudioComponentInstanceDispose(unit);
        return nullptr;
    }

    return unit;
}

static void DestroyOutputUnit(AudioUnit unit)
{
    if (unit != nullptr)
    {
        AudioOutputUnitStop(unit);
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
    }
}

// ---------------------------------------------------------------------------
// Mixer
// ---------------------------------------------------------------------------
static const uint32_t kMixSampleRate = 44100;
static const uint32_t kMixChannels = 2;

static AudioUnit sMixUnit = nullptr;
static SampleRing sMixRing;
static uint32_t sPlaybackFrames = 0;
static uint32_t sMixBufferLen = 0;   // bytes
static int16_t* sMixBuffer = nullptr;

struct SoundVoice
{
    int32_t mSampleRate = 44100;
    float mPitch = 1.0f;
    float mVolumeL = 1.0f;
    float mVolumeR = 1.0f;
    uint8_t* mSrcBuffer = nullptr;
    uint32_t mSrcBufferLen = 0;
    uint32_t mSrcFrames = 0;
    float mCurFrame = 0;
    uint32_t mNumChannels = 2;
    uint32_t mBytesPerSample = 2;
    bool mLoop = false;
    bool mActive = false;
};

static SoundVoice sVoices[AUDIO_MAX_VOICES];

static OSStatus MixRenderCallback(void* inRefCon,
                                  AudioUnitRenderActionFlags* ioActionFlags,
                                  const AudioTimeStamp* inTimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList* ioData)
{
    (void)inRefCon; (void)ioActionFlags; (void)inTimeStamp; (void)inBusNumber;

    int16_t* dst = (int16_t*)ioData->mBuffers[0].mData;
    uint32_t samples = inNumberFrames * kMixChannels;
    sMixRing.Read(dst, samples);   // zero-fills on underrun, never blocks
    return noErr;
}

void AUD_StreamingShutdown();

void AUD_Initialize()
{
    // Same headroom as the Linux backend: enough for a 15 Hz update rate.
    sPlaybackFrames = uint32_t((1 / 15.0f) * kMixSampleRate);
    sMixBufferLen = sPlaybackFrames * 4;   // 2 samples (L/R) * 2 bytes per sample
    sMixBuffer = new int16_t[sMixBufferLen / 2];
    memset(sMixBuffer, 0, sMixBufferLen);

    // Ring holds ~2 update periods so a slow frame doesn't underrun.
    sMixRing.Init(sPlaybackFrames * kMixChannels * 2);

    sMixUnit = CreateOutputUnit(kMixSampleRate, kMixChannels, MixRenderCallback, nullptr);
    if (sMixUnit == nullptr)
    {
        LogError("Cannot open audio device");
        return;
    }

    OSStatus status = AudioOutputUnitStart(sMixUnit);
    if (status != noErr)
    {
        LogError("CoreAudio: AudioOutputUnitStart failed (%d)", (int)status);
        DestroyOutputUnit(sMixUnit);
        sMixUnit = nullptr;
        return;
    }

    LogDebug("Audio device opened (CoreAudio, %u Hz, ring %u frames).", kMixSampleRate, sMixRing.mCapacity / kMixChannels);
}

void AUD_Shutdown()
{
    AUD_StreamingShutdown();

    DestroyOutputUnit(sMixUnit);
    sMixUnit = nullptr;

    sMixRing.Destroy();

    delete [] sMixBuffer;
    sMixBuffer = nullptr;
}

void AUD_Update()
{
    if (sMixUnit == nullptr || sMixBuffer == nullptr)
    {
        return;
    }

    int32_t frames = (int32_t)(sMixRing.Writable() / kMixChannels);
    frames = glm::min(int32_t(sMixBufferLen) / 4, frames);

    if (frames > 0)
    {
        memset(sMixBuffer, 0, frames * 4);

        for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i)
        {
            if (sVoices[i].mActive)
            {
                SoundVoice& voice = sVoices[i];
                OCT_ASSERT(voice.mSrcFrames > 0);

                // The src voice may move at a faster or slower pace based on the
                // pitch value, so we interpolate between frames.
                float srcDeltaFrame = 1 * voice.mPitch * (voice.mSampleRate / (float)kMixSampleRate);

                for (int32_t dstFrame = 0; dstFrame < frames; ++dstFrame)
                {
                    float srcFrameFloat = voice.mCurFrame + (dstFrame * srcDeltaFrame);

                    int32_t srcFrames[2] = { int32_t(srcFrameFloat), int32_t(srcFrameFloat) + 1 };
                    float frameInterpAlpha = fmod(srcFrameFloat, 1.0f);

                    int16_t srcSampleL[2] = { 0, 0 };
                    int16_t srcSampleR[2] = { 0, 0 };

                    if (voice.mLoop)
                    {
                        if (srcFrames[0] >= int32_t(voice.mSrcFrames))
                            srcFrames[0] = srcFrames[0] % voice.mSrcFrames;
                        if (srcFrames[1] >= int32_t(voice.mSrcFrames))
                            srcFrames[1] = srcFrames[1] % voice.mSrcFrames;
                    }

                    for (int32_t f = 0; f < 2; ++f)
                    {
                        int32_t frameIndex = srcFrames[f];

                        if (frameIndex >= int32_t(voice.mSrcFrames))
                        {
                            srcSampleL[f] = 0;
                            srcSampleR[f] = 0;
                        }
                        else if (voice.mNumChannels == 1)
                        {
                            if (voice.mBytesPerSample == 1)
                            {
                                srcSampleL[f] = *((uint8_t*) (voice.mSrcBuffer + (frameIndex * 1 * 1)));
                                srcSampleL[f] = srcSampleL[f] * 256 - 32767;
                                srcSampleR[f] = srcSampleL[f];
                            }
                            else
                            {
                                srcSampleL[f] = *((int16_t*) (voice.mSrcBuffer + (frameIndex * 1 * 2)));
                                srcSampleR[f] = srcSampleL[f];
                            }
                        }
                        else
                        {
                            if (voice.mBytesPerSample == 1)
                            {
                                srcSampleL[f] = *((uint8_t*) (voice.mSrcBuffer + (frameIndex * 2 * 1)));
                                srcSampleR[f] = *((uint8_t*) (voice.mSrcBuffer + (frameIndex * 2 * 1 + 1)));
                                srcSampleL[f] = srcSampleL[f] * 256 - 32767;
                                srcSampleR[f] = srcSampleR[f] * 256 - 32767;
                            }
                            else
                            {
                                srcSampleL[f] = *((int16_t*) (voice.mSrcBuffer + (frameIndex * 2 * 2)));
                                srcSampleR[f] = *((int16_t*) (voice.mSrcBuffer + (frameIndex * 2 * 2 + 2)));
                            }
                        }
                    }

                    int32_t finalSampleL = (int32_t)glm::mix(srcSampleL[0], srcSampleL[1], frameInterpAlpha);
                    int32_t finalSampleR = (int32_t)glm::mix(srcSampleR[0], srcSampleR[1], frameInterpAlpha);
                    finalSampleL = int32_t(finalSampleL * voice.mVolumeL);
                    finalSampleR = int32_t(finalSampleR * voice.mVolumeR);
                    finalSampleL = glm::clamp(finalSampleL + (int32_t)sMixBuffer[dstFrame * 2 + 0], -32768, 32767);
                    finalSampleR = glm::clamp(finalSampleR + (int32_t)sMixBuffer[dstFrame * 2 + 1], -32768, 32767);
                    sMixBuffer[dstFrame * 2 + 0] = (int16_t)finalSampleL;
                    sMixBuffer[dstFrame * 2 + 1] = (int16_t)finalSampleR;
                }

                voice.mCurFrame += (frames * srcDeltaFrame);

                if (voice.mLoop)
                {
                    voice.mCurFrame = fmod(voice.mCurFrame, (float) voice.mSrcFrames);
                }
            }
        }

        sMixRing.Write(sMixBuffer, frames * kMixChannels);
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
    OCT_ASSERT(!sVoices[voiceIndex].mActive);

    sVoices[voiceIndex].mActive = true;
    sVoices[voiceIndex].mBytesPerSample = soundWave->GetBitsPerSample() / 8;
    sVoices[voiceIndex].mCurFrame = 0.0f;
    sVoices[voiceIndex].mLoop = loop;
    sVoices[voiceIndex].mNumChannels = soundWave->GetNumChannels();
    sVoices[voiceIndex].mPitch = pitch;
    sVoices[voiceIndex].mSampleRate = soundWave->GetSampleRate();
    sVoices[voiceIndex].mSrcBuffer = soundWave->GetWaveData();
    sVoices[voiceIndex].mSrcBufferLen = soundWave->GetWaveDataSize();
    sVoices[voiceIndex].mVolumeL = spatial ? 0.0f : volume;
    sVoices[voiceIndex].mVolumeR = spatial ? 0.0f : volume;

    int32_t bytesPerFrame = sVoices[voiceIndex].mBytesPerSample * sVoices[voiceIndex].mNumChannels;
    sVoices[voiceIndex].mSrcFrames = sVoices[voiceIndex].mSrcBufferLen / bytesPerFrame;

    OCT_ASSERT(sVoices[voiceIndex].mSrcBufferLen % bytesPerFrame == 0);
    OCT_ASSERT(bytesPerFrame > 0 &&
           bytesPerFrame <= 4);
}

void AUD_Stop(uint32_t voiceIndex)
{
    sVoices[voiceIndex].mActive = false;
}

bool AUD_IsPlaying(uint32_t voiceIndex)
{
    return sVoices[voiceIndex].mActive &&
           sVoices[voiceIndex].mCurFrame < sVoices[voiceIndex].mSrcFrames;
}

void AUD_SetVolume(uint32_t voiceIndex, float leftVolume, float rightVolume)
{
    sVoices[voiceIndex].mVolumeL = leftVolume;
    sVoices[voiceIndex].mVolumeR = rightVolume;
}

void AUD_SetPitch(uint32_t voiceIndex, float pitch)
{
    sVoices[voiceIndex].mPitch = pitch;
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

// ============================================================================
// Streaming voices — one AudioUnit per voice, fed from a byte ring.
// ============================================================================
static constexpr uint32_t kMaxStreamingVoices = 4;

struct StreamingVoiceEntry
{
    AudioUnit             mUnit = nullptr;
    SampleRing            mRing;
    uint32_t              mSampleRate = 0;
    uint32_t              mNumChannels = 0;
    uint32_t              mBitsPerSample = 0;
    bool                  mInUse = false;
    bool                  mPaused = false;
    std::atomic<float>    mVolume { 1.0f };
    std::atomic<uint64_t> mFramesPlayed { 0 };
};

static StreamingVoiceEntry sStreamingVoices[kMaxStreamingVoices];

static OSStatus StreamRenderCallback(void* inRefCon,
                                     AudioUnitRenderActionFlags* ioActionFlags,
                                     const AudioTimeStamp* inTimeStamp,
                                     UInt32 inBusNumber,
                                     UInt32 inNumberFrames,
                                     AudioBufferList* ioData)
{
    (void)ioActionFlags; (void)inTimeStamp; (void)inBusNumber;

    StreamingVoiceEntry* entry = (StreamingVoiceEntry*)inRefCon;
    int16_t* dst = (int16_t*)ioData->mBuffers[0].mData;
    uint32_t channels = entry->mNumChannels;
    uint32_t samples = inNumberFrames * channels;

    uint32_t got = entry->mRing.Read(dst, samples);

    float volume = entry->mVolume.load(std::memory_order_relaxed);
    if (volume != 1.0f)
    {
        for (uint32_t i = 0; i < got; ++i)
        {
            dst[i] = (int16_t)glm::clamp((int32_t)(dst[i] * volume), -32768, 32767);
        }
    }

    // Only count real frames so the audio clock stalls (rather than drifts)
    // on underrun.
    entry->mFramesPlayed.fetch_add(got / channels, std::memory_order_relaxed);
    return noErr;
}

void AUD_StreamingShutdown()
{
    for (uint32_t i = 0; i < kMaxStreamingVoices; ++i)
    {
        StreamingVoiceEntry& entry = sStreamingVoices[i];
        if (entry.mInUse)
        {
            DestroyOutputUnit(entry.mUnit);
            entry.mUnit = nullptr;
            entry.mRing.Destroy();
            entry.mInUse = false;
        }
    }
}

uint32_t AUD_OpenStream(uint32_t sampleRate, uint32_t numChannels, uint32_t bitsPerSample)
{
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

    int slot = -1;
    for (uint32_t i = 0; i < kMaxStreamingVoices; ++i)
    {
        if (!sStreamingVoices[i].mInUse) { slot = (int)i; break; }
    }
    if (slot < 0)
    {
        LogWarning("AUD_OpenStream: no free streaming voices (pool size %u)", kMaxStreamingVoices);
        return 0;
    }

    StreamingVoiceEntry& entry = sStreamingVoices[slot];
    entry.mSampleRate    = sampleRate;
    entry.mNumChannels   = numChannels;
    entry.mBitsPerSample = bitsPerSample;
    entry.mPaused        = false;
    entry.mVolume.store(1.0f);
    entry.mFramesPlayed.store(0);

    // One second of audio; SubmitStreamBuffer applies soft backpressure past that.
    entry.mRing.Init(sampleRate * numChannels);

    entry.mUnit = CreateOutputUnit(sampleRate, numChannels, StreamRenderCallback, &entry);
    if (entry.mUnit == nullptr)
    {
        entry.mRing.Destroy();
        LogError("AUD_OpenStream: failed to create output unit");
        return 0;
    }

    OSStatus status = AudioOutputUnitStart(entry.mUnit);
    if (status != noErr)
    {
        LogError("AUD_OpenStream: AudioOutputUnitStart failed (%d)", (int)status);
        DestroyOutputUnit(entry.mUnit);
        entry.mUnit = nullptr;
        entry.mRing.Destroy();
        return 0;
    }

    entry.mInUse = true;

    const uint32_t streamId = (uint32_t)slot + 1;
    AudioAnalysis::OnStreamOpened(streamId, sampleRate, numChannels, bitsPerSample);
    return streamId;
}

void AUD_CloseStream(uint32_t streamId)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;
    AudioAnalysis::OnStreamClosed(streamId);

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse) return;

    DestroyOutputUnit(entry.mUnit);
    entry.mUnit = nullptr;
    entry.mRing.Destroy();

    entry.mInUse         = false;
    entry.mPaused        = false;
    entry.mSampleRate    = 0;
    entry.mNumChannels   = 0;
    entry.mBitsPerSample = 0;
    entry.mFramesPlayed.store(0);
}

int32_t AUD_SubmitStreamBuffer(uint32_t streamId, const uint8_t* data, uint32_t byteSize)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return 0;
    if (data == nullptr || byteSize == 0) return 0;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse || entry.mUnit == nullptr) return 0;

    const uint32_t bytesPerFrame = entry.mNumChannels * (entry.mBitsPerSample / 8);
    if (bytesPerFrame == 0) return 0;

    uint32_t writableSamples = entry.mRing.Writable();
    uint32_t wantSamples = byteSize / 2;
    uint32_t samples = (writableSamples < wantSamples) ? writableSamples : wantSamples;
    // Keep whole frames.
    samples -= samples % entry.mNumChannels;
    if (samples == 0)
    {
        // Soft backpressure (matches Windows: caller retries on next tick).
        return 0;
    }

    entry.mRing.Write((const int16_t*)data, samples);

    uint32_t toWrite = samples * 2;
    AudioAnalysis::OnStreamSubmitted(streamId, data, toWrite);
    return (int32_t)toWrite;
}

uint64_t AUD_GetStreamPlayedSamples(uint32_t streamId)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return 0;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse) return 0;

    return entry.mFramesPlayed.load(std::memory_order_relaxed);
}

void AUD_SetStreamVolume(uint32_t streamId, float volume)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse) return;

    entry.mVolume.store(glm::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
}

void AUD_SetStreamPaused(uint32_t streamId, bool paused)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse || entry.mUnit == nullptr) return;

    if (paused != entry.mPaused)
    {
        if (paused)
            AudioOutputUnitStop(entry.mUnit);
        else
            AudioOutputUnitStart(entry.mUnit);
        entry.mPaused = paused;
    }
}

void AUD_FlushStream(uint32_t streamId)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];
    if (!entry.mInUse) return;

    if (entry.mPaused)
    {
        // Callback is not running; safe to drop the queue directly.
        entry.mRing.mRead.store(entry.mRing.mWrite.load(std::memory_order_acquire), std::memory_order_release);
    }
    else
    {
        // Consumed by the render callback on its next tick.
        entry.mRing.mReset.store(true);
    }
}

#endif
