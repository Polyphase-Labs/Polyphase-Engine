#pragma once

#include "Asset.h"

class POLYPHASE_API SoundWave : public Asset
{
public:

    DECLARE_ASSET(SoundWave, Asset);

    SoundWave();
    ~SoundWave();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual bool Import(const std::string& path, ImportOptions* options) override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;
    virtual const char* GetTypeImportExt() override;

    void SetPcmData(uint8_t* data, uint32_t size, uint32_t numSamples, uint32_t bitsPerSample, uint32_t numChannels, uint32_t sampleRate);

    // Decode an in-memory WAV (or OGG, when AUD_DecodeVorbis is available)
    // buffer into `out` and Create() it. Format detected via magic bytes:
    // RIFF/WAVE for WAV, "OggS" for OGG. Pass formatHint = "wav"/"ogg" to
    // skip detection. Runtime-safe.
    static bool LoadFromMemory(const uint8_t* data, size_t size, const char* formatHint, SoundWave& out);

    void SetVolumeMultiplier(float volume);
    float GetVolumeMultiplier() const;

    void SetPitchMultiplier(float pitch);
    float GetPitchMultiplier() const;

    void SetAudioClass(int8_t audioClass);
    int8_t GetAudioClass() const;

    uint8_t* GetWaveData() const;
    uint32_t GetWaveDataSize() const;

    // Streaming (long BGM): when set, LoadStream does NOT decode the whole PCM into
    // RAM. Instead it records where the PCM lives in the .oct so AudioManager can
    // stream it from disk in chunks. GetWaveData() returns nullptr for these.
    bool IsStreaming() const { return mStreaming; }
    uint64_t GetStreamPcmOffset() const { return mStreamPcmOffset; }
    uint32_t GetStreamPcmSize() const { return mStreamPcmSize; }

    uint32_t GetNumChannels() const;
    uint32_t GetBitsPerSample() const;
    uint32_t GetSampleRate() const;
    uint32_t GetNumSamples() const;
    uint32_t GetBlockAlign() const;
    uint32_t GetByteRate() const;

    float GetDuration() const;

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    uint8_t* mWaveData = nullptr;
    uint32_t mWaveDataSize = 0;

    uint8_t* mCompressedData = nullptr;
    uint32_t mCompressedSize = 0;

    // Properties
    float mVolumeMultiplier = 1.0f;
    float mPitchMultiplier = 1.0f;
    int8_t mAudioClass = 0;
    bool mCompress = false;
    bool mCompressInternal = false;
    bool mStreaming = false;

    // Streaming metadata (valid when mStreaming): where the raw PCM lives in the
    // .oct file, so it can be re-opened and read in chunks instead of resident.
    uint64_t mStreamPcmOffset = 0;
    uint32_t mStreamPcmSize = 0;

    // Soundwave Format
    uint32_t mNumChannels = 1;
    uint32_t mBitsPerSample = 8;
    uint32_t mSampleRate = 22050;
    uint32_t mNumSamples = 0;
    uint32_t mBlockAlign = 0;
    uint32_t mByteRate = 0;
};
