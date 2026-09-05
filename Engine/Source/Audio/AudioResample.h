#pragma once

#include <stdint.h>
#include <vector>

// Linear-interpolating PCM sample-rate conversion. Input is interleaved
// 8-bit unsigned or 16-bit signed PCM with 1 or 2 channels; output keeps the
// same bit depth and channel count at dstRate. outNumSamples counts
// interleaved samples (frames * channels), matching SoundWave::mNumSamples.
bool AUD_ResamplePcm(const uint8_t* src,
                     uint32_t srcSize,
                     uint32_t bitsPerSample,
                     uint32_t numChannels,
                     uint32_t srcRate,
                     uint32_t dstRate,
                     std::vector<uint8_t>& outPcm,
                     uint32_t& outNumSamples);
