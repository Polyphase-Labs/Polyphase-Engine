#pragma once

#include "EngineTypes.h"

class Stream;
class SoundWave;
class Audio3D;

struct PcmFormat
{
    uint32_t mNumChannels = 2;
    uint32_t mSampleRate = 44100;
    uint32_t mBytesPerSample = 2;
};

void AUD_Initialize();
void AUD_Shutdown();
void AUD_Update();

void AUD_Play(
    uint32_t voiceIndex,
    SoundWave* soundWave,
    float volume,
    float pitch,
    bool loop,
    float startTime,
    bool spatial);

void AUD_Stop(uint32_t voiceIndex);
bool AUD_IsPlaying(uint32_t voiceIndex);
void AUD_SetVolume(uint32_t voiceIndex, float leftVolume, float rightVolume);
void AUD_SetPitch(uint32_t voiceIndex, float pitch);

uint8_t* AUD_AllocWaveBuffer(uint32_t size);
void AUD_FreeWaveBuffer(void* buffer);
void AUD_ProcessWaveBuffer(SoundWave* soundWave);

// ------------------------------------------------------------------------------------------------
// Streaming voices — push-based PCM output for long-form / runtime-generated audio.
//
// Intended for use by native addons (e.g. the VideoPlayer) that decode PCM at runtime and
// can't pre-bake it into a SoundWave asset. Each open stream is backed by a dedicated source
// voice on the platform mixer. The engine owns the voice; the caller owns the raw PCM buffer
// which must remain valid until the corresponding SubmitStreamBuffer call returns OR the
// stream is closed. The backend copies the bytes into its own staging buffer synchronously.
//
// Returns: stream id (non-zero on success, 0 on failure).
// sampleRate: PCM frames per second (e.g. 48000).
// numChannels: 1 (mono) or 2 (stereo) supported by the Windows/XAudio2 backend.
// bitsPerSample: 16 supported (other values return 0 on Windows).
// ------------------------------------------------------------------------------------------------
uint32_t AUD_OpenStream(uint32_t sampleRate, uint32_t numChannels, uint32_t bitsPerSample);
void     AUD_CloseStream(uint32_t streamId);
// Returns the number of bytes the backend accepted. 0 means the submit queue is full and the
// caller should retry next tick. Non-zero but < byteSize is not returned by the current backend
// (it either fully accepts or fully rejects).
int32_t  AUD_SubmitStreamBuffer(uint32_t streamId, const uint8_t* data, uint32_t byteSize);
// Total samples-per-channel consumed by the stream since Open. The video player's A/V sync uses
// this as the master clock: `audio_time_seconds = played_samples / sample_rate`.
uint64_t AUD_GetStreamPlayedSamples(uint32_t streamId);
void     AUD_SetStreamVolume(uint32_t streamId, float volume);
void     AUD_SetStreamPaused(uint32_t streamId, bool paused);
// Drop every buffer currently queued on the stream without waiting for them to play.
// Used on seek / loop so the voice doesn't keep playing stale pre-seek audio.
// SamplesPlayed keeps climbing (XAudio2 can't reset it without destroying the voice);
// callers should take a snapshot via GetStreamPlayedSamples() right after Flush and
// subtract it from subsequent readings to recover a seek-relative clock.
void     AUD_FlushStream(uint32_t streamId);

// Platform Independent
void AUD_EncodeVorbis(Stream& inStream, Stream& outStream, PcmFormat format);
void AUD_DecodeVorbis(Stream& inStream, Stream& outStream, PcmFormat format);
