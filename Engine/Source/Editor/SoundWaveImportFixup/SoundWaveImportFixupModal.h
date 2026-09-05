#pragma once

#include "Engine.h"

#if EDITOR

#include "AssetRef.h"
#include "Engine/Assets/SoundWave.h"

#include <cstdint>
#include <string>
#include <vector>

// Post-import prompt for WAVs whose sample rate isn't 44.1 kHz (or 22.05 kHz,
// which the runtime already upsamples). Desktop backends play any rate, but
// Android relabels everything as 44.1 kHz without resampling and the console
// cook paths assume 44.1 kHz, so the user picks Resample / Keep / Cancel per
// row or in bulk. Unlike TextureImportFixupModal the asset is already fully
// valid when enqueued; the modal only defers the auto-save so the .oct is
// written once with the final PCM.
class SoundWaveImportFixupModal
{
public:

    static constexpr uint32_t kTargetSampleRate = 44100;

    enum class FixChoice
    {
        None,
        Resampled,
        Kept,
        Cancelled,
    };

    struct PendingRow
    {
        AssetRef    mSound;             // imported asset (auto-cleared on unload)
        std::string mAssetName;         // captured at enqueue so the row still labels after teardown
        std::string mSourcePath;
        uint32_t    mSrcSampleRate = 0;
        uint32_t    mNumChannels = 0;
        uint32_t    mBitsPerSample = 0;
        FixChoice   mResolved = FixChoice::None;
    };

    static SoundWaveImportFixupModal* Get();

    // Called from SoundWave::Import when the parsed sample rate is non-standard.
    void Enqueue(SoundWave* wave, const std::string& sourcePath);

    bool HasPending() const { return !mRows.empty(); }

    // True while `asset` is enqueued and unresolved. ActionManager skips the
    // import auto-save while this is true; the modal saves after resolving.
    bool IsAwaitingFixup(const Asset* asset) const;

    void Reset();

    // Call from EditorImguiDraw every frame.
    void Draw();

private:

    SoundWaveImportFixupModal() = default;

    void ApplyResample(PendingRow& row);
    void ApplyKeep(PendingRow& row);
    void ApplyCancel(PendingRow& row);
    void SaveRow(const PendingRow& row);

    std::vector<PendingRow> mRows;
    bool mModalRequested = false;
};

#endif // EDITOR
