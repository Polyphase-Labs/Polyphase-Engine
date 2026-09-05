#include "Audio/AudioResample.h"

#include <algorithm>
#include <cstring>

bool AUD_ResamplePcm(const uint8_t* src,
                     uint32_t srcSize,
                     uint32_t bitsPerSample,
                     uint32_t numChannels,
                     uint32_t srcRate,
                     uint32_t dstRate,
                     std::vector<uint8_t>& outPcm,
                     uint32_t& outNumSamples)
{
    outPcm.clear();
    outNumSamples = 0;

    if (src == nullptr || srcSize == 0 || srcRate == 0 || dstRate == 0)
        return false;
    if (bitsPerSample != 8 && bitsPerSample != 16)
        return false;
    if (numChannels != 1 && numChannels != 2)
        return false;

    const uint32_t bytesPerSample = bitsPerSample / 8;
    const uint32_t frameBytes = numChannels * bytesPerSample;
    const uint32_t srcFrames = srcSize / frameBytes;
    if (srcFrames == 0)
        return false;

    if (srcRate == dstRate)
    {
        outPcm.assign(src, src + (size_t)srcFrames * frameBytes);
        outNumSamples = srcFrames * numChannels;
        return true;
    }

    auto sample = [&](uint32_t frame, uint32_t ch) -> float
    {
        frame = std::min(frame, srcFrames - 1);
        const uint8_t* p = src + (size_t)frame * frameBytes + ch * bytesPerSample;
        if (bytesPerSample == 1)
        {
            return ((int)p[0] - 128) / 128.0f;
        }
        int16_t v;
        std::memcpy(&v, p, 2);
        return v / 32768.0f;
    };

    const uint64_t dstFrames64 = (uint64_t)srcFrames * dstRate / srcRate;
    if (dstFrames64 == 0 || dstFrames64 > 0xFFFFFFFFull / frameBytes)
        return false;
    const uint32_t dstFrames = (uint32_t)dstFrames64;

    outPcm.resize((size_t)dstFrames * frameBytes);
    const double step = (double)srcRate / (double)dstRate;

    for (uint32_t i = 0; i < dstFrames; ++i)
    {
        const double pos = i * step;
        const uint32_t f0 = (uint32_t)pos;
        const float t = (float)(pos - f0);

        for (uint32_t ch = 0; ch < numChannels; ++ch)
        {
            float v = sample(f0, ch) * (1.0f - t) + sample(f0 + 1, ch) * t;
            v = std::max(-1.0f, std::min(1.0f, v));

            uint8_t* dst = outPcm.data() + (size_t)i * frameBytes + ch * bytesPerSample;
            if (bytesPerSample == 1)
            {
                dst[0] = (uint8_t)std::max(0, std::min(255, (int)(v * 127.0f + 128.0f)));
            }
            else
            {
                const int16_t s = (int16_t)(v * 32767.0f);
                std::memcpy(dst, &s, 2);
            }
        }
    }

    outNumSamples = dstFrames * numChannels;
    return true;
}
