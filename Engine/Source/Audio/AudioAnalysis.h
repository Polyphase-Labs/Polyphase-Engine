#pragma once

#include <stdint.h>

#include "Constants.h"
#include "AudioConstants.h"

namespace AudioAnalysis
{
    static constexpr uint32_t kFftSize     = AUDIO_FFT_SIZE;
    static constexpr uint32_t kSpectrumLen = kFftSize / 2;
    static constexpr uint32_t kMaxStreams  = AUDIO_ANALYSIS_MAX_STREAMS;

    // PCM view handed to the analysis routines. The caller fills this and
    // hands it in; we never own the data.
    struct PcmView
    {
        const uint8_t* mData          = nullptr; // raw interleaved PCM
        uint32_t       mSampleRate    = 0;
        uint32_t       mNumChannels   = 1;
        uint32_t       mBitsPerSample = 16;
        uint64_t       mCursorFrame   = 0;       // current playback frame (samples-per-channel)
        uint64_t       mTotalFrames   = 0;
        bool           mLoop          = false;
        uint32_t       mCacheKey      = 0;       // unique per voice/stream — used by per-frame cache
    };

#if AUDIO_ANALYSIS_ENABLED

    float ComputeRMS                (const PcmView& view);
    float ComputeLoudnessDb         (float rms);
    float ComputeLoudnessNormalized (float rms);
    float ComputeBandMagnitude      (const PcmView& view, float startHz, float endHz);
    void  ComputeSpectrum           (const PcmView& view, float startHz, float endHz,
                                     float* outBins, uint32_t numBins);

    // Streaming voice ring buffers (shared platform-independent state).
    // Backends call these from AUD_OpenStream / AUD_CloseStream / AUD_SubmitStreamBuffer.
    void OnStreamOpened    (uint32_t streamId, uint32_t sampleRate, uint32_t numChannels, uint32_t bitsPerSample);
    void OnStreamClosed    (uint32_t streamId);
    void OnStreamSubmitted (uint32_t streamId, const uint8_t* data, uint32_t byteSize);
    bool BuildStreamPcmView(uint32_t streamId, PcmView& outView);

#else

    inline float ComputeRMS                (const PcmView&) { return 0.0f; }
    inline float ComputeLoudnessDb         (float)          { return -60.0f; }
    inline float ComputeLoudnessNormalized (float)          { return 0.0f; }
    inline float ComputeBandMagnitude      (const PcmView&, float, float) { return 0.0f; }
    inline void  ComputeSpectrum           (const PcmView&, float, float, float* outBins, uint32_t numBins)
    {
        if (outBins) { for (uint32_t i = 0; i < numBins; ++i) outBins[i] = 0.0f; }
    }
    inline void  OnStreamOpened    (uint32_t, uint32_t, uint32_t, uint32_t) {}
    inline void  OnStreamClosed    (uint32_t)                                {}
    inline void  OnStreamSubmitted (uint32_t, const uint8_t*, uint32_t)      {}
    inline bool  BuildStreamPcmView(uint32_t, PcmView&)                      { return false; }

#endif // AUDIO_ANALYSIS_ENABLED
}
