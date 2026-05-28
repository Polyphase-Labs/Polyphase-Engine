#include "AudioAnalysis.h"

#if AUDIO_ANALYSIS_ENABLED

#include "Engine.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace AudioAnalysis
{
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kLoudnessFloorDb = -60.0f;

    // ---- Hann window (lazy init) -----------------------------------------------------------------
    static float sHannWindow[kFftSize];
    static bool  sHannInit = false;

    static void EnsureHannWindow()
    {
        if (sHannInit) return;
        for (uint32_t i = 0; i < kFftSize; ++i)
        {
            sHannWindow[i] = 0.5f * (1.0f - cosf((2.0f * kPi * i) / (kFftSize - 1)));
        }
        sHannInit = true;
    }

    // ---- Per-cache-key per-frame magnitude cache --------------------------------------------------
    // Keyed by view.mCacheKey + the engine frame counter. Two calls to GetSpectrum on the same voice
    // in the same frame reuse the FFT.
    static constexpr uint32_t kCacheSlots = AUDIO_MAX_VOICES + kMaxStreams;

    struct CacheEntry
    {
        uint32_t mFrame        = 0xFFFFFFFFu;
        uint32_t mCacheKey     = 0;
        uint64_t mCursorFrame  = 0;
        float    mMagnitudes[kSpectrumLen];
        bool     mValid        = false;
    };

    static CacheEntry sCache[kCacheSlots];

    static CacheEntry* FindCacheSlot(uint32_t cacheKey)
    {
        // Open-addressed; small table, linear scan is fine.
        const uint32_t homeSlot = cacheKey % kCacheSlots;
        for (uint32_t probe = 0; probe < kCacheSlots; ++probe)
        {
            const uint32_t slot = (homeSlot + probe) % kCacheSlots;
            if (sCache[slot].mCacheKey == cacheKey || !sCache[slot].mValid)
                return &sCache[slot];
        }
        return &sCache[homeSlot]; // fallback eviction
    }

    // ---- PCM decode helpers -----------------------------------------------------------------------
    // Returns a single mono float in [-1, 1] sampled at frame `frame` (with looping).
    static inline float SamplePcmFrame(const PcmView& view, int64_t frame)
    {
        if (view.mTotalFrames == 0 || view.mData == nullptr) return 0.0f;

        if (view.mLoop)
        {
            // C-style modulo with negative handling.
            int64_t mod = frame % (int64_t)view.mTotalFrames;
            if (mod < 0) mod += (int64_t)view.mTotalFrames;
            frame = mod;
        }
        else
        {
            if (frame < 0 || frame >= (int64_t)view.mTotalFrames) return 0.0f;
        }

        const uint32_t ch = view.mNumChannels ? view.mNumChannels : 1;

        if (view.mBitsPerSample == 16)
        {
            const int16_t* p = reinterpret_cast<const int16_t*>(view.mData) + frame * ch;
            int32_t sum = 0;
            for (uint32_t c = 0; c < ch; ++c) sum += p[c];
            return (float)sum / (float)(32768 * (int32_t)ch);
        }
        else if (view.mBitsPerSample == 8)
        {
            const uint8_t* p = view.mData + frame * ch;
            int32_t sum = 0;
            for (uint32_t c = 0; c < ch; ++c) sum += (int32_t)p[c] - 128;
            return (float)sum / (float)(128 * (int32_t)ch);
        }
        return 0.0f;
    }

    // ---- FFT (radix-2 in-place Cooley-Tukey) ------------------------------------------------------
    static void FftInPlace(float* real, float* imag, uint32_t n)
    {
        // Bit reversal.
        uint32_t j = 0;
        for (uint32_t i = 1; i < n; ++i)
        {
            uint32_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j)
            {
                float tr = real[i]; real[i] = real[j]; real[j] = tr;
                float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
            }
        }

        // Butterflies.
        for (uint32_t size = 2; size <= n; size <<= 1)
        {
            const uint32_t half = size >> 1;
            const float theta = -2.0f * kPi / (float)size;
            const float wpr = cosf(theta);
            const float wpi = sinf(theta);
            for (uint32_t i = 0; i < n; i += size)
            {
                float wr = 1.0f;
                float wi = 0.0f;
                for (uint32_t k = 0; k < half; ++k)
                {
                    const uint32_t a = i + k;
                    const uint32_t b = a + half;
                    const float tr = wr * real[b] - wi * imag[b];
                    const float ti = wr * imag[b] + wi * real[b];
                    real[b] = real[a] - tr;
                    imag[b] = imag[a] - ti;
                    real[a] += tr;
                    imag[a] += ti;
                    const float nwr = wr * wpr - wi * wpi;
                    wi = wr * wpi + wi * wpr;
                    wr = nwr;
                }
            }
        }
    }

    // Computes (or retrieves cached) magnitude spectrum for this view.
    // Returns pointer to a per-key magnitude buffer of length kSpectrumLen.
    static const float* GetMagnitudes(const PcmView& view)
    {
        const uint32_t frameNum = GetEngineState() ? GetEngineState()->mFrameNumber : 0u;

        CacheEntry* slot = FindCacheSlot(view.mCacheKey);
        if (slot->mValid &&
            slot->mFrame == frameNum &&
            slot->mCacheKey == view.mCacheKey &&
            slot->mCursorFrame == view.mCursorFrame)
        {
            return slot->mMagnitudes;
        }

        EnsureHannWindow();

        // Pull last kFftSize frames ending at cursor.
        float real[kFftSize];
        float imag[kFftSize];

        const int64_t end = (int64_t)view.mCursorFrame;
        const int64_t start = end - (int64_t)kFftSize;
        for (uint32_t i = 0; i < kFftSize; ++i)
        {
            const float s = SamplePcmFrame(view, start + (int64_t)i);
            real[i] = s * sHannWindow[i];
            imag[i] = 0.0f;
        }

        FftInPlace(real, imag, kFftSize);

        // Magnitudes. Normalize by kFftSize/2 so unit sine peaks near 1.0.
        const float invN = 2.0f / (float)kFftSize;
        for (uint32_t i = 0; i < kSpectrumLen; ++i)
        {
            slot->mMagnitudes[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]) * invN;
        }

        slot->mFrame       = frameNum;
        slot->mCacheKey    = view.mCacheKey;
        slot->mCursorFrame = view.mCursorFrame;
        slot->mValid       = true;
        return slot->mMagnitudes;
    }

    // ---- Public API -------------------------------------------------------------------------------

    float ComputeRMS(const PcmView& view)
    {
        if (view.mData == nullptr || view.mTotalFrames == 0) return 0.0f;

        const int64_t end = (int64_t)view.mCursorFrame;
        const int64_t start = end - (int64_t)kFftSize;
        double sumSq = 0.0;
        for (uint32_t i = 0; i < kFftSize; ++i)
        {
            const float s = SamplePcmFrame(view, start + (int64_t)i);
            sumSq += (double)s * (double)s;
        }
        return (float)sqrt(sumSq / (double)kFftSize);
    }

    float ComputeLoudnessDb(float rms)
    {
        if (rms <= 0.0f) return kLoudnessFloorDb;
        float dB = 20.0f * log10f(rms);
        if (dB < kLoudnessFloorDb) dB = kLoudnessFloorDb;
        if (dB > 0.0f) dB = 0.0f;
        return dB;
    }

    float ComputeLoudnessNormalized(float rms)
    {
        const float dB = ComputeLoudnessDb(rms);
        return 1.0f - (dB / kLoudnessFloorDb); // -60 → 0, 0 → 1
    }

    static inline void FreqRangeToBins(const PcmView& view, float startHz, float endHz,
                                       uint32_t& outStartBin, uint32_t& outEndBin)
    {
        const float binHz = view.mSampleRate ? (float)view.mSampleRate / (float)kFftSize : 1.0f;
        if (startHz < 0.0f) startHz = 0.0f;
        if (endHz   < startHz) endHz = startHz;
        int32_t sb = (int32_t)(startHz / binHz);
        int32_t eb = (int32_t)(endHz   / binHz);
        if (sb < 0) sb = 0;
        if (eb >= (int32_t)kSpectrumLen) eb = (int32_t)kSpectrumLen - 1;
        if (eb < sb) eb = sb;
        outStartBin = (uint32_t)sb;
        outEndBin   = (uint32_t)eb;
    }

    float ComputeBandMagnitude(const PcmView& view, float startHz, float endHz)
    {
        if (view.mData == nullptr || view.mTotalFrames == 0 || view.mSampleRate == 0) return 0.0f;

        const float* mags = GetMagnitudes(view);
        uint32_t sb, eb;
        FreqRangeToBins(view, startHz, endHz, sb, eb);

        double sum = 0.0;
        const uint32_t n = (eb - sb) + 1;
        for (uint32_t i = sb; i <= eb; ++i) sum += mags[i];
        return (float)(sum / (double)n);
    }

    void ComputeSpectrum(const PcmView& view, float startHz, float endHz,
                         float* outBins, uint32_t numBins)
    {
        if (!outBins || numBins == 0) return;
        for (uint32_t i = 0; i < numBins; ++i) outBins[i] = 0.0f;

        if (view.mData == nullptr || view.mTotalFrames == 0 || view.mSampleRate == 0) return;

        const float* mags = GetMagnitudes(view);
        uint32_t sb, eb;
        FreqRangeToBins(view, startHz, endHz, sb, eb);
        const uint32_t span = (eb - sb) + 1;

        for (uint32_t i = 0; i < numBins; ++i)
        {
            // Map output bin i to a sub-range of [sb..eb].
            const uint32_t lo = sb + (i * span) / numBins;
            const uint32_t hiExcl = sb + ((i + 1) * span) / numBins;
            const uint32_t hi = (hiExcl > lo) ? (hiExcl - 1) : lo;

            double sum = 0.0;
            uint32_t n = 0;
            for (uint32_t k = lo; k <= hi && k <= eb; ++k) { sum += mags[k]; ++n; }
            outBins[i] = n ? (float)(sum / (double)n) : 0.0f;
        }
    }

#if AUDIO_ANALYSIS_STREAMS_ENABLED

    // ---- Stream ring buffers ----------------------------------------------------------------------
    struct StreamRing
    {
        uint32_t mStreamId      = 0;
        bool     mInUse         = false;
        uint32_t mSampleRate    = 0;
        uint32_t mNumChannels   = 1;
        uint32_t mBitsPerSample = 16;
        uint32_t mBytesPerFrame = 2;
        uint8_t* mData          = nullptr;
        uint32_t mCapacityBytes = 0;
        uint32_t mWriteOffset   = 0;   // next byte to write
        uint64_t mTotalBytesIn  = 0;   // monotonic — used to compute virtual cursor
    };

    static StreamRing sStreams[kMaxStreams];

    static StreamRing* FindStream(uint32_t streamId)
    {
        for (uint32_t i = 0; i < kMaxStreams; ++i)
        {
            if (sStreams[i].mInUse && sStreams[i].mStreamId == streamId) return &sStreams[i];
        }
        return nullptr;
    }

    static StreamRing* AcquireStream(uint32_t streamId)
    {
        // Reuse existing slot if a stale OnStreamClosed was missed.
        for (uint32_t i = 0; i < kMaxStreams; ++i)
        {
            if (sStreams[i].mInUse && sStreams[i].mStreamId == streamId) return &sStreams[i];
        }
        for (uint32_t i = 0; i < kMaxStreams; ++i)
        {
            if (!sStreams[i].mInUse) { sStreams[i].mStreamId = streamId; return &sStreams[i]; }
        }
        return nullptr;
    }

    static void ReleaseStream(StreamRing& r)
    {
        if (r.mData) { free(r.mData); r.mData = nullptr; }
        r.mInUse         = false;
        r.mStreamId      = 0;
        r.mCapacityBytes = 0;
        r.mWriteOffset   = 0;
        r.mTotalBytesIn  = 0;
    }

    void OnStreamOpened(uint32_t streamId, uint32_t sampleRate, uint32_t numChannels, uint32_t bitsPerSample)
    {
        if (streamId == 0 || sampleRate == 0 || numChannels == 0) return;
        if (bitsPerSample != 8 && bitsPerSample != 16) return;

        StreamRing* r = AcquireStream(streamId);
        if (!r) return;

        if (r->mInUse && r->mData) free(r->mData);

        r->mSampleRate    = sampleRate;
        r->mNumChannels   = numChannels;
        r->mBitsPerSample = bitsPerSample;
        r->mBytesPerFrame = (bitsPerSample / 8) * numChannels;

        const float seconds = AUDIO_ANALYSIS_STREAM_SECONDS;
        uint32_t cap = (uint32_t)(seconds * (float)sampleRate) * r->mBytesPerFrame;
        // Ensure at least one FFT window's worth.
        const uint32_t minCap = kFftSize * r->mBytesPerFrame * 2;
        if (cap < minCap) cap = minCap;

        r->mData          = (uint8_t*)malloc(cap);
        r->mCapacityBytes = r->mData ? cap : 0;
        r->mWriteOffset   = 0;
        r->mTotalBytesIn  = 0;
        r->mInUse         = true;

        if (r->mData) memset(r->mData, 0, r->mCapacityBytes);
    }

    void OnStreamClosed(uint32_t streamId)
    {
        StreamRing* r = FindStream(streamId);
        if (r) ReleaseStream(*r);
    }

    void OnStreamSubmitted(uint32_t streamId, const uint8_t* data, uint32_t byteSize)
    {
        if (!data || byteSize == 0) return;
        StreamRing* r = FindStream(streamId);
        if (!r || !r->mData || r->mCapacityBytes == 0) return;

        // If the incoming chunk is bigger than the ring, skip to its tail.
        if (byteSize >= r->mCapacityBytes)
        {
            const uint32_t skip = byteSize - r->mCapacityBytes;
            data     += skip;
            byteSize -= skip;
            memcpy(r->mData, data, byteSize);
            r->mWriteOffset  = byteSize % r->mCapacityBytes;
            r->mTotalBytesIn += skip + byteSize;
            return;
        }

        const uint32_t firstChunk = (r->mWriteOffset + byteSize <= r->mCapacityBytes)
                                  ? byteSize
                                  : (r->mCapacityBytes - r->mWriteOffset);
        memcpy(r->mData + r->mWriteOffset, data, firstChunk);
        if (firstChunk < byteSize)
        {
            memcpy(r->mData, data + firstChunk, byteSize - firstChunk);
            r->mWriteOffset = byteSize - firstChunk;
        }
        else
        {
            r->mWriteOffset = (r->mWriteOffset + byteSize) % r->mCapacityBytes;
        }
        r->mTotalBytesIn += byteSize;
    }

    bool BuildStreamPcmView(uint32_t streamId, PcmView& outView)
    {
        const StreamRing* r = FindStream(streamId);
        if (!r || !r->mData || r->mCapacityBytes == 0) return false;

        outView.mData          = r->mData;
        outView.mSampleRate    = r->mSampleRate;
        outView.mNumChannels   = r->mNumChannels;
        outView.mBitsPerSample = r->mBitsPerSample;
        outView.mTotalFrames   = r->mCapacityBytes / r->mBytesPerFrame;
        // mCursorFrame is "the most-recently-written frame". Loop wrap-around handled by SamplePcmFrame
        // (we set mLoop = true so it indexes modulo mTotalFrames).
        outView.mCursorFrame   = (uint64_t)(r->mWriteOffset / r->mBytesPerFrame);
        outView.mLoop          = true;
        outView.mCacheKey      = AUDIO_MAX_VOICES + (uint32_t)((uintptr_t)(r - &sStreams[0]));
        return true;
    }

#else // AUDIO_ANALYSIS_STREAMS_ENABLED

    void OnStreamOpened    (uint32_t, uint32_t, uint32_t, uint32_t) {}
    void OnStreamClosed    (uint32_t)                                {}
    void OnStreamSubmitted (uint32_t, const uint8_t*, uint32_t)      {}
    bool BuildStreamPcmView(uint32_t, PcmView&)                      { return false; }

#endif // AUDIO_ANALYSIS_STREAMS_ENABLED
}

#endif // AUDIO_ANALYSIS_ENABLED
