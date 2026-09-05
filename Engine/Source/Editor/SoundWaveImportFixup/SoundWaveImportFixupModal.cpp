#include "SoundWaveImportFixup/SoundWaveImportFixupModal.h"

#if EDITOR

#include "ActionManager.h"
#include "Asset.h"
#include "AssetManager.h"
#include "AudioManager.h"
#include "Log.h"

#include "Audio/Audio.h"
#include "Audio/AudioResample.h"

#include "imgui.h"

#include <cstring>

SoundWaveImportFixupModal* SoundWaveImportFixupModal::Get()
{
    static SoundWaveImportFixupModal sInstance;
    return &sInstance;
}

void SoundWaveImportFixupModal::Enqueue(SoundWave* wave, const std::string& sourcePath)
{
    if (wave == nullptr)
        return;

    PendingRow row;
    row.mSound         = wave;
    row.mAssetName     = wave->GetName();
    row.mSourcePath    = sourcePath;
    row.mSrcSampleRate = wave->GetSampleRate();
    row.mNumChannels   = wave->GetNumChannels();
    row.mBitsPerSample = wave->GetBitsPerSample();

    mRows.push_back(std::move(row));
    mModalRequested = true;

    LogWarning("SoundWaveImportFixupModal: '%s' is %u Hz (runtime target is %u Hz); awaiting choice.",
        sourcePath.c_str(), wave->GetSampleRate(), kTargetSampleRate);
}

void SoundWaveImportFixupModal::Reset()
{
    mRows.clear();
    mModalRequested = false;
}

bool SoundWaveImportFixupModal::IsAwaitingFixup(const Asset* asset) const
{
    if (asset == nullptr)
        return false;
    for (const PendingRow& row : mRows)
    {
        if (row.mResolved != FixChoice::None)
            continue;
        if (row.mSound.Get() == asset)
            return true;
    }
    return false;
}

void SoundWaveImportFixupModal::SaveRow(const PendingRow& row)
{
    AssetStub* stub = AssetManager::Get()->GetAssetStub(row.mAssetName);
    if (stub != nullptr)
        AssetManager::Get()->SaveAsset(*stub);
}

void SoundWaveImportFixupModal::ApplyResample(PendingRow& row)
{
    SoundWave* wave = row.mSound.Get<SoundWave>();
    if (wave == nullptr || wave->GetWaveData() == nullptr)
    {
        row.mResolved = FixChoice::Cancelled;
        return;
    }

    // Stop any inspector preview before the buffer is swapped out from under it.
    AudioManager::StopSounds(wave);

    std::vector<uint8_t> pcm;
    uint32_t numSamples = 0;
    const bool ok = AUD_ResamplePcm(wave->GetWaveData(), wave->GetWaveDataSize(),
                                    wave->GetBitsPerSample(), wave->GetNumChannels(),
                                    wave->GetSampleRate(), kTargetSampleRate,
                                    pcm, numSamples);
    if (!ok)
    {
        LogError("SoundWaveImportFixupModal: resample failed for '%s'; keeping %u Hz.",
            row.mAssetName.c_str(), wave->GetSampleRate());
        ApplyKeep(row);
        return;
    }

    uint8_t* newBuf = AUD_AllocWaveBuffer((uint32_t)pcm.size());
    std::memcpy(newBuf, pcm.data(), pcm.size());

    uint8_t* oldBuf = wave->GetWaveData();
    wave->SetPcmData(newBuf, (uint32_t)pcm.size(), numSamples,
                     wave->GetBitsPerSample(), wave->GetNumChannels(), kTargetSampleRate);
    AUD_FreeWaveBuffer(oldBuf);

    row.mResolved = FixChoice::Resampled;
    SaveRow(row);

    LogDebug("SoundWaveImportFixupModal: resampled '%s' from %u Hz to %u Hz.",
        row.mAssetName.c_str(), row.mSrcSampleRate, kTargetSampleRate);
}

void SoundWaveImportFixupModal::ApplyKeep(PendingRow& row)
{
    row.mResolved = FixChoice::Kept;
    if (row.mSound.Get<SoundWave>() != nullptr)
        SaveRow(row);

    LogDebug("SoundWaveImportFixupModal: kept '%s' at %u Hz.",
        row.mAssetName.c_str(), row.mSrcSampleRate);
}

void SoundWaveImportFixupModal::ApplyCancel(PendingRow& row)
{
    SoundWave* wave = row.mSound.Get<SoundWave>();
    if (wave != nullptr)
    {
        AudioManager::StopSounds(wave);
        AssetStub* stub = AssetManager::Get()->GetAssetStub(row.mAssetName);
        if (stub != nullptr)
        {
            ActionManager::Get()->DeleteAsset(stub);
        }
    }
    row.mResolved = FixChoice::Cancelled;

    LogDebug("SoundWaveImportFixupModal: cancelled '%s'.", row.mAssetName.c_str());
}

void SoundWaveImportFixupModal::Draw()
{
    if (mRows.empty())
        return;

    const char* kPopupId = "Sound Import - Non-standard Sample Rate";

    if (mModalRequested && !ImGui::IsPopupOpen(kPopupId))
    {
        ImGui::OpenPopup(kPopupId);
        mModalRequested = false;
    }

    if (ImGui::IsPopupOpen(kPopupId))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.55f, io.DisplaySize.y * 0.55f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if (!ImGui::BeginPopupModal(kPopupId, nullptr, ImGuiWindowFlags_NoCollapse))
    {
        return;
    }

    ImGui::TextWrapped(
        "These sounds have a sample rate other than 44100 Hz. Polyphase's runtime targets "
        "44.1 kHz: Windows and Linux play any rate, but Android plays other rates at the "
        "wrong speed/pitch and the console (3DS, Wii, GameCube, PSP) cook paths assume 44.1 kHz. "
        "Pick a fix per row, or use the bulk buttons at the bottom.");
    ImGui::Spacing();
    ImGui::TextDisabled("Resample: linear-interpolated conversion to 44100 Hz; safe on every platform.");
    ImGui::TextDisabled("Keep: stores the source rate unchanged; desktop-only projects can use this.");
    ImGui::Spacing();

    if (ImGui::BeginChild("FixupList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true))
    {
        for (size_t r = 0; r < mRows.size(); ++r)
        {
            PendingRow& row = mRows[r];
            ImGui::PushID((int)r);
            ImGui::Separator();

            ImGui::Text("Asset: '%s'", row.mAssetName.c_str());
            ImGui::TextDisabled("Source: %s", row.mSourcePath.c_str());

            ImGui::Text("Sample Rate: ");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%u Hz", row.mSrcSampleRate);
            ImGui::SameLine();
            ImGui::TextDisabled("(%u-bit, %u ch)", row.mBitsPerSample, row.mNumChannels);

            if (row.mResolved != FixChoice::None)
            {
                const char* tag =
                    row.mResolved == FixChoice::Resampled ? "[Resampled]" :
                    row.mResolved == FixChoice::Kept      ? "[Kept]"      :
                                                            "[Cancelled]";
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%s", tag);
            }
            else if (row.mSound.Get<SoundWave>() == nullptr)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f),
                    "(Asset no longer alive - dismiss this row.)");
                if (ImGui::Button("Dismiss"))
                {
                    row.mResolved = FixChoice::Cancelled;
                }
            }
            else
            {
                char resampleLabel[64];
                snprintf(resampleLabel, sizeof(resampleLabel), "Resample to %u Hz", kTargetSampleRate);
                if (ImGui::Button(resampleLabel))
                {
                    ApplyResample(row);
                }

                ImGui::SameLine();
                char keepLabel[64];
                snprintf(keepLabel, sizeof(keepLabel), "Keep %u Hz", row.mSrcSampleRate);
                if (ImGui::Button(keepLabel))
                {
                    ApplyKeep(row);
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel Import"))
                {
                    ApplyCancel(row);
                }
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("Resample All"))
    {
        for (PendingRow& row : mRows)
        {
            if (row.mResolved == FixChoice::None)
                ApplyResample(row);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Keep All"))
    {
        for (PendingRow& row : mRows)
        {
            if (row.mResolved == FixChoice::None)
                ApplyKeep(row);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel All"))
    {
        for (PendingRow& row : mRows)
        {
            if (row.mResolved == FixChoice::None)
                ApplyCancel(row);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Close"))
    {
        // The assets are already valid, so anything unresolved at Close is
        // kept (and saved) rather than discarded.
        for (PendingRow& row : mRows)
        {
            if (row.mResolved == FixChoice::None)
                ApplyKeep(row);
        }
        mRows.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

#endif // EDITOR
