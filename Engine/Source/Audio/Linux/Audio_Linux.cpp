#if PLATFORM_LINUX

#include "Audio/Audio.h"
#include "Audio/AudioConstants.h"
#include "System/System.h"

#include "Assets/SoundWave.h"
#include "Log.h"
#include "Maths.h"

#include <alsa/asoundlib.h>

snd_pcm_t* sSoundDevice = nullptr;
snd_pcm_uframes_t sPlaybackFrames = 0;
uint32_t sMixBufferLen = 0;
int16_t* sMixBuffer = nullptr;

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

void AUD_Initialize()
{
    int err = snd_pcm_open( &sSoundDevice, "default", SND_PCM_STREAM_PLAYBACK, 0 );
    snd_pcm_hw_params_t* hw_params = nullptr;

    if( err < 0 )
    {
        LogError("Cannot open audio device");
        OCT_ASSERT(0);
        return;
    }
    else
    {
        LogDebug("Audio device opened.");
    }

    if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0)
    {
        LogError("Failed to allocate hardware params");
        OCT_ASSERT(0);
        return;
    }

    if ((err = snd_pcm_hw_params_any(sSoundDevice, hw_params)) < 0)
    {
        LogError("Failed to initialize hardware params");
        OCT_ASSERT(0);
        return;
    }

    // Create enough buffer room for a 30 fps update rate.
    sPlaybackFrames = snd_pcm_uframes_t((1 / 15.0f) * 44100);

    err = snd_pcm_hw_params_set_rate_resample(sSoundDevice, hw_params, 1);
    err = snd_pcm_hw_params_set_access(sSoundDevice, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    err = snd_pcm_hw_params_set_format(sSoundDevice, hw_params, SND_PCM_FORMAT_S16_LE);
    err = snd_pcm_hw_params_set_channels(sSoundDevice, hw_params, 2);
    err = snd_pcm_hw_params_set_buffer_size(sSoundDevice, hw_params, sPlaybackFrames);

    unsigned int playbackRate = 44100;
    err = snd_pcm_hw_params_set_rate_near(sSoundDevice, hw_params, &playbackRate, 0);

    err = snd_pcm_hw_params(sSoundDevice, hw_params);

    snd_pcm_uframes_t bufferSize;
    snd_pcm_hw_params_get_buffer_size( hw_params, &bufferSize );
    LogDebug("Buffer size = %d frames", (int32_t) bufferSize);
    sPlaybackFrames = bufferSize;
    LogDebug("Significant bits for linear samples = %d",snd_pcm_hw_params_get_sbits(hw_params));

    snd_pcm_uframes_t periodFrames = 0;
    snd_pcm_hw_params_get_period_size(hw_params, &periodFrames, 0);
    LogDebug("Period Frames: %lu\n", periodFrames);

    snd_pcm_hw_params_free(hw_params);
    err = snd_pcm_prepare(sSoundDevice);

    sMixBufferLen = sPlaybackFrames * 4; // ( 2 samples (L/R) * 2 bytes per sample)
    sMixBuffer = new int16_t[sMixBufferLen / 2];
    memset(sMixBuffer, 0, sMixBufferLen);
    snd_pcm_writei(sSoundDevice, sMixBuffer, sPlaybackFrames);
    //snd_pcm_start(sSoundDevice);

    LogDebug("PCM name: '%s'", snd_pcm_name(sSoundDevice));
    LogDebug("PCM state: %s", snd_pcm_state_name(snd_pcm_state(sSoundDevice)));
}

void AUD_StreamingShutdown();

void AUD_Shutdown()
{
    AUD_StreamingShutdown();

    delete [] sMixBuffer;
    sMixBuffer = nullptr;

    if (sSoundDevice != nullptr)
    {
        snd_pcm_close(sSoundDevice);
    }
}

void AUD_Update()
{   
    int32_t frames = (int32_t) snd_pcm_avail(sSoundDevice);
    frames = glm::min(int32_t(sMixBufferLen) / 4, frames);

    //LogDebug("Frames to mix: %d", frames);
    //LogDebug("PCM state: %s", snd_pcm_state_name(snd_pcm_state(sSoundDevice)));

    if (frames > 0)
    {
        // We need to mix an audio buffer
        //OCT_ASSERT(frames * 4  <= int32_t(sMixBufferLen));
        memset(sMixBuffer, 0, frames * 4);

        for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i)
        {
            if (sVoices[i].mActive)
            {
                SoundVoice& voice = sVoices[i];
                OCT_ASSERT(voice.mSrcFrames > 0);

                // If the voice is active, that means we need to mix *frames* number of frames
                // into the mix buffer. The src voice may move at a faster or slower pace based on the 
                // pitch value, so we will need to interpolate between frames.

                // TODO: Handle pitch
                float srcDeltaFrame = 1 * voice.mPitch * (voice.mSampleRate / 44100.0f);

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

                    // Interpolate between the two src frames
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
                            // Use same src sample for left and right dst samples

                            if (voice.mBytesPerSample == 1)
                            {
                                // uint8 samples
                                // Convert from uint8_t to int16_t
                                srcSampleL[f] = *((uint8_t*) (voice.mSrcBuffer + (frameIndex * 1 * 1)));
                                srcSampleL[f] = srcSampleL[f] * 256 - 32767;
                                srcSampleR[f] = srcSampleL[f];   
                            }
                            else
                            {
                                // int16 samples
                                srcSampleL[f] = *((int16_t*) (voice.mSrcBuffer + (frameIndex * 1 * 2)));
                                srcSampleR[f] = srcSampleL[f];
                            }
                        }
                        else
                        {
                            if (voice.mBytesPerSample == 1)
                            {
                                // uint8 samples
                                srcSampleL[f] = *((uint8_t*) (voice.mSrcBuffer + (frameIndex * 2 * 1)));
                                srcSampleR[f] = *((uint8_t*) (voice.mSrcBuffer + (frameIndex * 2 * 1 + 1)));
                                srcSampleL[f] = srcSampleL[f] * 256 - 32767;
                                srcSampleR[f] = srcSampleR[f] * 256 - 32767;
                            }
                            else
                            {
                                // int16 samples
                                srcSampleL[f] = *((int16_t*) (voice.mSrcBuffer + (frameIndex * 2 * 2)));
                                srcSampleR[f] = *((int16_t*) (voice.mSrcBuffer + (frameIndex * 2 * 2 + 2)));
                            }
                        }
                    }

                    // We have four samples now, so now we need to linearly interpolate between the left and right channels
                    // to get a final 2 samples that we will accumulate into the mix buffer.
                    int32_t finalSampleL = (int32_t)glm::mix(srcSampleL[0], srcSampleL[1], frameInterpAlpha);
                    int32_t finalSampleR = (int32_t)glm::mix(srcSampleR[0], srcSampleR[1], frameInterpAlpha);
                    finalSampleL = int32_t(finalSampleL * voice.mVolumeL);
                    finalSampleR = int32_t(finalSampleR * voice.mVolumeR);
                    finalSampleL = glm::clamp(finalSampleL + (int32_t)sMixBuffer[dstFrame * 2 + 0], -32768, 32767);
                    finalSampleR = glm::clamp(finalSampleR + (int32_t)sMixBuffer[dstFrame * 2 + 1], -32768, 32767);
                    sMixBuffer[dstFrame * 2 + 0] = (int16_t)finalSampleL;
                    sMixBuffer[dstFrame * 2 + 1] = (int16_t)finalSampleR;
                }

                // We've finished getting all of the samples for this voice. Increase the current sample value
                // to keep track of where to pick up next frame. If the sound is looping we need to mod the frame value
                // otherwise let the curFrame exceed the *voice.mSrcFrames* count so we can determine that it is finished playing.

                voice.mCurFrame += (frames * srcDeltaFrame);

                if (voice.mLoop)
                {
                    voice.mCurFrame = fmod(voice.mCurFrame, (float) voice.mSrcFrames);
                }
            }
        }
    }

    snd_pcm_sframes_t framesWritten = 0;
    // We've generated the frames we need for this update, so now we just need to sent it to the audio driver.
    if (frames < 0 ||
        (frames > 0 && (framesWritten = snd_pcm_writei(sSoundDevice, sMixBuffer, frames)) == -EPIPE))
    {
        //LogWarning("Audio buffer underrun.");
        snd_pcm_prepare(sSoundDevice);
    } 
    else if (framesWritten < 0)
    {
        LogError("Can't write to PCM device. %s", snd_strerror(framesWritten));
        return;
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
// Streaming voices — PulseAudio async-API backend.
// ============================================================================
// Mirrors the Windows XAudio2 implementation in Audio_Windows.cpp: fixed pool
// of voices, 1-based stream id (0 = sentinel), graceful fallback to "no voice
// available" when PulseAudio is missing (e.g. headless container). The ALSA
// mixer above is independent of this code path.

#include <pulse/pulseaudio.h>
#include <atomic>

static constexpr uint32_t kMaxStreamingVoices = 4;

struct StreamingVoiceEntry
{
    pa_stream*            mStream = nullptr;
    uint32_t              mSampleRate = 0;
    uint32_t              mNumChannels = 0;
    uint32_t              mBitsPerSample = 0;
    bool                  mInUse = false;
    bool                  mPaused = false;
    std::atomic<uint64_t> mSamplesSubmitted { 0 };
};

static StreamingVoiceEntry sStreamingVoices[kMaxStreamingVoices];

static pa_threaded_mainloop* sPaMainloop   = nullptr;
static pa_context*           sPaContext    = nullptr;
static bool                  sPaInitFailed = false;

static void PaContextStateCb(pa_context* /*c*/, void* userdata)
{
    pa_threaded_mainloop_signal(static_cast<pa_threaded_mainloop*>(userdata), 0);
}

static void PaStreamStateCb(pa_stream* /*s*/, void* userdata)
{
    pa_threaded_mainloop_signal(static_cast<pa_threaded_mainloop*>(userdata), 0);
}

static bool EnsurePulseInitialized()
{
    if (sPaContext != nullptr) return true;
    if (sPaInitFailed) return false;

    sPaMainloop = pa_threaded_mainloop_new();
    if (sPaMainloop == nullptr)
    {
        LogWarning("PulseAudio: pa_threaded_mainloop_new failed");
        sPaInitFailed = true;
        return false;
    }
    if (pa_threaded_mainloop_start(sPaMainloop) != 0)
    {
        LogWarning("PulseAudio: pa_threaded_mainloop_start failed");
        pa_threaded_mainloop_free(sPaMainloop);
        sPaMainloop = nullptr;
        sPaInitFailed = true;
        return false;
    }

    pa_threaded_mainloop_lock(sPaMainloop);
    sPaContext = pa_context_new(pa_threaded_mainloop_get_api(sPaMainloop), "Polyphase");
    if (sPaContext == nullptr)
    {
        pa_threaded_mainloop_unlock(sPaMainloop);
        pa_threaded_mainloop_stop(sPaMainloop);
        pa_threaded_mainloop_free(sPaMainloop);
        sPaMainloop = nullptr;
        sPaInitFailed = true;
        LogWarning("PulseAudio: pa_context_new failed");
        return false;
    }

    pa_context_set_state_callback(sPaContext, PaContextStateCb, sPaMainloop);
    if (pa_context_connect(sPaContext, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0)
    {
        LogWarning("PulseAudio: pa_context_connect failed (%s)",
                   pa_strerror(pa_context_errno(sPaContext)));
        pa_context_unref(sPaContext);
        sPaContext = nullptr;
        pa_threaded_mainloop_unlock(sPaMainloop);
        pa_threaded_mainloop_stop(sPaMainloop);
        pa_threaded_mainloop_free(sPaMainloop);
        sPaMainloop = nullptr;
        sPaInitFailed = true;
        return false;
    }

    bool ready = false;
    while (true)
    {
        pa_context_state_t st = pa_context_get_state(sPaContext);
        if (st == PA_CONTEXT_READY) { ready = true; break; }
        if (st == PA_CONTEXT_FAILED || st == PA_CONTEXT_TERMINATED) break;
        pa_threaded_mainloop_wait(sPaMainloop);
    }

    if (!ready)
    {
        LogWarning("PulseAudio: context never reached READY (%s)",
                   pa_strerror(pa_context_errno(sPaContext)));
        pa_context_disconnect(sPaContext);
        pa_context_unref(sPaContext);
        sPaContext = nullptr;
        pa_threaded_mainloop_unlock(sPaMainloop);
        pa_threaded_mainloop_stop(sPaMainloop);
        pa_threaded_mainloop_free(sPaMainloop);
        sPaMainloop = nullptr;
        sPaInitFailed = true;
        return false;
    }

    pa_threaded_mainloop_unlock(sPaMainloop);
    LogDebug("PulseAudio: streaming context ready");
    return true;
}

void AUD_StreamingShutdown()
{
    if (sPaMainloop == nullptr) return;

    pa_threaded_mainloop_lock(sPaMainloop);
    for (uint32_t i = 0; i < kMaxStreamingVoices; ++i)
    {
        if (sStreamingVoices[i].mInUse && sStreamingVoices[i].mStream != nullptr)
        {
            pa_stream_disconnect(sStreamingVoices[i].mStream);
            pa_stream_unref(sStreamingVoices[i].mStream);
            sStreamingVoices[i].mStream = nullptr;
            sStreamingVoices[i].mInUse  = false;
        }
    }
    if (sPaContext != nullptr)
    {
        pa_context_disconnect(sPaContext);
        pa_context_unref(sPaContext);
        sPaContext = nullptr;
    }
    pa_threaded_mainloop_unlock(sPaMainloop);
    pa_threaded_mainloop_stop(sPaMainloop);
    pa_threaded_mainloop_free(sPaMainloop);
    sPaMainloop = nullptr;
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
    if (!EnsurePulseInitialized()) return 0;

    pa_threaded_mainloop_lock(sPaMainloop);

    int slot = -1;
    for (uint32_t i = 0; i < kMaxStreamingVoices; ++i)
    {
        if (!sStreamingVoices[i].mInUse) { slot = (int)i; break; }
    }
    if (slot < 0)
    {
        pa_threaded_mainloop_unlock(sPaMainloop);
        LogWarning("AUD_OpenStream: no free streaming voices (pool size %u)", kMaxStreamingVoices);
        return 0;
    }

    StreamingVoiceEntry& entry = sStreamingVoices[slot];

    pa_sample_spec spec = {};
    spec.format   = PA_SAMPLE_S16LE;
    spec.rate     = sampleRate;
    spec.channels = (uint8_t)numChannels;

    entry.mStream = pa_stream_new(sPaContext, "Polyphase Stream", &spec, nullptr);
    if (entry.mStream == nullptr)
    {
        pa_threaded_mainloop_unlock(sPaMainloop);
        LogError("AUD_OpenStream: pa_stream_new failed (%s)",
                 pa_strerror(pa_context_errno(sPaContext)));
        return 0;
    }

    pa_stream_set_state_callback(entry.mStream, PaStreamStateCb, sPaMainloop);

    if (pa_stream_connect_playback(entry.mStream, nullptr, nullptr,
                                   PA_STREAM_NOFLAGS, nullptr, nullptr) < 0)
    {
        LogError("AUD_OpenStream: pa_stream_connect_playback failed (%s)",
                 pa_strerror(pa_context_errno(sPaContext)));
        pa_stream_unref(entry.mStream);
        entry.mStream = nullptr;
        pa_threaded_mainloop_unlock(sPaMainloop);
        return 0;
    }

    bool ready = false;
    while (true)
    {
        pa_stream_state_t st = pa_stream_get_state(entry.mStream);
        if (st == PA_STREAM_READY) { ready = true; break; }
        if (st == PA_STREAM_FAILED || st == PA_STREAM_TERMINATED) break;
        pa_threaded_mainloop_wait(sPaMainloop);
    }

    if (!ready)
    {
        LogError("AUD_OpenStream: stream never reached READY");
        pa_stream_disconnect(entry.mStream);
        pa_stream_unref(entry.mStream);
        entry.mStream = nullptr;
        pa_threaded_mainloop_unlock(sPaMainloop);
        return 0;
    }

    entry.mSampleRate    = sampleRate;
    entry.mNumChannels   = numChannels;
    entry.mBitsPerSample = bitsPerSample;
    entry.mInUse         = true;
    entry.mPaused        = false;
    entry.mSamplesSubmitted.store(0);

    pa_threaded_mainloop_unlock(sPaMainloop);
    return (uint32_t)slot + 1;
}

void AUD_CloseStream(uint32_t streamId)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;
    if (sPaMainloop == nullptr) return;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];

    pa_threaded_mainloop_lock(sPaMainloop);
    if (!entry.mInUse)
    {
        pa_threaded_mainloop_unlock(sPaMainloop);
        return;
    }
    if (entry.mStream != nullptr)
    {
        pa_stream_disconnect(entry.mStream);
        pa_stream_unref(entry.mStream);
        entry.mStream = nullptr;
    }
    entry.mInUse         = false;
    entry.mPaused        = false;
    entry.mSampleRate    = 0;
    entry.mNumChannels   = 0;
    entry.mBitsPerSample = 0;
    entry.mSamplesSubmitted.store(0);
    pa_threaded_mainloop_unlock(sPaMainloop);
}

int32_t AUD_SubmitStreamBuffer(uint32_t streamId, const uint8_t* data, uint32_t byteSize)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return 0;
    if (sPaMainloop == nullptr) return 0;
    if (data == nullptr || byteSize == 0) return 0;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];

    pa_threaded_mainloop_lock(sPaMainloop);
    if (!entry.mInUse || entry.mStream == nullptr)
    {
        pa_threaded_mainloop_unlock(sPaMainloop);
        return 0;
    }

    size_t writable = pa_stream_writable_size(entry.mStream);
    if (writable == 0 || writable == (size_t)-1)
    {
        // Soft backpressure (matches Windows: caller retries on next tick).
        pa_threaded_mainloop_unlock(sPaMainloop);
        return 0;
    }

    size_t toWrite = (writable < (size_t)byteSize) ? writable : (size_t)byteSize;
    if (pa_stream_write(entry.mStream, data, toWrite, nullptr, 0, PA_SEEK_RELATIVE) < 0)
    {
        LogWarning("AUD_SubmitStreamBuffer: pa_stream_write failed (%s)",
                   pa_strerror(pa_context_errno(sPaContext)));
        pa_threaded_mainloop_unlock(sPaMainloop);
        return 0;
    }

    const uint32_t bytesPerFrame = entry.mNumChannels * (entry.mBitsPerSample / 8);
    if (bytesPerFrame > 0)
    {
        entry.mSamplesSubmitted.fetch_add(toWrite / bytesPerFrame);
    }

    pa_threaded_mainloop_unlock(sPaMainloop);
    return (int32_t)toWrite;
}

uint64_t AUD_GetStreamPlayedSamples(uint32_t streamId)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return 0;
    if (sPaMainloop == nullptr) return 0;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];

    pa_threaded_mainloop_lock(sPaMainloop);
    if (!entry.mInUse || entry.mStream == nullptr)
    {
        pa_threaded_mainloop_unlock(sPaMainloop);
        return 0;
    }

    pa_usec_t usec = 0;
    int rv = pa_stream_get_time(entry.mStream, &usec);
    uint32_t rate = entry.mSampleRate;
    uint64_t fallback = entry.mSamplesSubmitted.load();
    pa_threaded_mainloop_unlock(sPaMainloop);

    if (rv == 0 && rate > 0)
    {
        return ((uint64_t)usec * rate) / 1000000ULL;
    }
    // pa_stream_get_time returns -PA_ERR_NODATA before the first sample plays;
    // fall back to submitted-sample count so the addon's audio-as-master clock
    // degrades to wall-clock timing rather than wedging at 0.
    return fallback;
}

void AUD_SetStreamVolume(uint32_t streamId, float volume)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;
    if (sPaMainloop == nullptr) return;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];

    pa_threaded_mainloop_lock(sPaMainloop);
    if (!entry.mInUse || entry.mStream == nullptr)
    {
        pa_threaded_mainloop_unlock(sPaMainloop);
        return;
    }

    pa_cvolume cv;
    pa_cvolume_set(&cv, entry.mNumChannels, pa_sw_volume_from_linear(volume));
    uint32_t idx = pa_stream_get_index(entry.mStream);
    pa_operation* op = pa_context_set_sink_input_volume(sPaContext, idx, &cv, nullptr, nullptr);
    if (op != nullptr) pa_operation_unref(op);
    pa_threaded_mainloop_unlock(sPaMainloop);
}

void AUD_SetStreamPaused(uint32_t streamId, bool paused)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;
    if (sPaMainloop == nullptr) return;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];

    pa_threaded_mainloop_lock(sPaMainloop);
    if (!entry.mInUse || entry.mStream == nullptr)
    {
        pa_threaded_mainloop_unlock(sPaMainloop);
        return;
    }
    if (paused != entry.mPaused)
    {
        pa_operation* op = pa_stream_cork(entry.mStream, paused ? 1 : 0, nullptr, nullptr);
        if (op != nullptr) pa_operation_unref(op);
        entry.mPaused = paused;
    }
    pa_threaded_mainloop_unlock(sPaMainloop);
}

void AUD_FlushStream(uint32_t streamId)
{
    if (streamId == 0 || streamId > kMaxStreamingVoices) return;
    if (sPaMainloop == nullptr) return;

    StreamingVoiceEntry& entry = sStreamingVoices[streamId - 1];

    pa_threaded_mainloop_lock(sPaMainloop);
    if (!entry.mInUse || entry.mStream == nullptr)
    {
        pa_threaded_mainloop_unlock(sPaMainloop);
        return;
    }
    pa_operation* op = pa_stream_flush(entry.mStream, nullptr, nullptr);
    if (op != nullptr) pa_operation_unref(op);
    pa_threaded_mainloop_unlock(sPaMainloop);
}

#endif
