#include "TextureImportFixup/TextureImportFixupModal.h"

#if EDITOR

#include "ActionManager.h"
#include "Asset.h"
#include "AssetManager.h"
#include "Log.h"
#include "Maths.h"

#include "imgui.h"

#include <stb_image_resize2.h>

#include <algorithm>
#include <cstring>

TextureImportFixupModal* TextureImportFixupModal::Get()
{
    static TextureImportFixupModal sInstance;
    return &sInstance;
}

void TextureImportFixupModal::Enqueue(Texture* tex,
                                      const std::string& sourcePath,
                                      std::vector<uint8_t>&& pixels,
                                      uint32_t srcW, uint32_t srcH)
{
    if (tex == nullptr || srcW == 0 || srcH == 0)
        return;

    PendingRow row;
    row.mTexture       = tex;
    row.mAssetName     = tex->GetName();
    row.mSourcePath    = sourcePath;
    row.mDecodedPixels = std::move(pixels);
    row.mSrcWidth      = srcW;
    row.mSrcHeight     = srcH;
    row.mPadWidth      = Maths::NextPowerOfTwo(srcW);
    row.mPadHeight     = Maths::NextPowerOfTwo(srcH);
    row.mResizeWidth   = Maths::PrevPowerOfTwo(srcW);
    row.mResizeHeight  = Maths::PrevPowerOfTwo(srcH);

    mRows.push_back(std::move(row));
    mModalRequested = true;

    LogWarning("TextureImportFixupModal: '%s' is %ux%u (not power-of-two); awaiting fix.",
        sourcePath.c_str(), srcW, srcH);
}

void TextureImportFixupModal::Reset()
{
    mRows.clear();
    mModalRequested = false;
}

bool TextureImportFixupModal::IsAwaitingFixup(const Asset* asset) const
{
    if (asset == nullptr)
        return false;
    for (const PendingRow& row : mRows)
    {
        if (row.mResolved != FixChoice::None)
            continue;
        // Compare via mTexture.Get() rather than name -- name lookups would
        // race with rename actions while the modal is open.
        if (row.mTexture.Get() == asset)
            return true;
    }
    return false;
}

void TextureImportFixupModal::ApplyPad(PendingRow& row)
{
    Texture* tex = row.mTexture.Get<Texture>();
    if (tex == nullptr)
    {
        row.mResolved = FixChoice::Cancelled;
        return;
    }

    const uint32_t padW = row.mPadWidth;
    const uint32_t padH = row.mPadHeight;
    const uint32_t srcW = row.mSrcWidth;
    const uint32_t srcH = row.mSrcHeight;

    // Zero-init alpha=0 transparent border. Source goes to the top-left so UV (0,0)
    // stays at the same texel; downstream meshes can apply UVMax = (srcW/padW, srcH/padH)
    // to crop sampling to the content region if they want.
    std::vector<uint8_t> padded(size_t(padW) * size_t(padH) * 4, 0);
    for (uint32_t y = 0; y < srcH; ++y)
    {
        const uint8_t* srcRow = row.mDecodedPixels.data() + size_t(y) * srcW * 4;
        uint8_t* dstRow       = padded.data()              + size_t(y) * padW * 4;
        std::memcpy(dstRow, srcRow, size_t(srcW) * 4);
    }

    tex->FinalizeDeferredImport(std::move(padded), padW, padH);
    row.mResolved = FixChoice::Padded;

    // Now that mPixels matches mWidth*mHeight*4, the auto-save that Import
    // would normally have done is safe to run.
    AssetStub* stub = AssetManager::Get()->GetAssetStub(row.mAssetName + ".oct");
    if (stub != nullptr)
        AssetManager::Get()->SaveAsset(*stub);

    LogDebug("TextureImportFixupModal: padded '%s' from %ux%u to %ux%u.",
        row.mAssetName.c_str(), srcW, srcH, padW, padH);
}

void TextureImportFixupModal::ApplyResize(PendingRow& row)
{
    Texture* tex = row.mTexture.Get<Texture>();
    if (tex == nullptr)
    {
        row.mResolved = FixChoice::Cancelled;
        return;
    }

    const uint32_t srcW = row.mSrcWidth;
    const uint32_t srcH = row.mSrcHeight;
    const uint32_t dstW = row.mResizeWidth;
    const uint32_t dstH = row.mResizeHeight;

    std::vector<uint8_t> resized(size_t(dstW) * size_t(dstH) * 4, 0);
    stbir_resize_uint8_srgb(
        row.mDecodedPixels.data(), (int)srcW, (int)srcH, (int)(srcW * 4),
        resized.data(),            (int)dstW, (int)dstH, (int)(dstW * 4),
        stbir_pixel_layout::STBIR_RGBA);

    tex->FinalizeDeferredImport(std::move(resized), dstW, dstH);
    row.mResolved = FixChoice::Resized;

    AssetStub* stub = AssetManager::Get()->GetAssetStub(row.mAssetName + ".oct");
    if (stub != nullptr)
        AssetManager::Get()->SaveAsset(*stub);

    LogDebug("TextureImportFixupModal: resized '%s' from %ux%u to %ux%u.",
        row.mAssetName.c_str(), srcW, srcH, dstW, dstH);
}

void TextureImportFixupModal::ApplyCancel(PendingRow& row)
{
    Texture* tex = row.mTexture.Get<Texture>();
    if (tex != nullptr)
    {
        AssetStub* stub = AssetManager::Get()->GetAssetStub(row.mAssetName + ".oct");
        if (stub != nullptr)
        {
            ActionManager::Get()->DeleteAsset(stub);
        }
    }
    row.mResolved = FixChoice::Cancelled;

    LogDebug("TextureImportFixupModal: cancelled '%s'.", row.mAssetName.c_str());
}

void TextureImportFixupModal::Draw()
{
    if (mRows.empty())
        return;

    const char* kPopupId = "Texture Import - Non-Power-of-Two";

    if (mModalRequested && !ImGui::IsPopupOpen(kPopupId))
    {
        ImGui::OpenPopup(kPopupId);
        mModalRequested = false;
    }

    if (ImGui::IsPopupOpen(kPopupId))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.55f, io.DisplaySize.y * 0.65f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if (!ImGui::BeginPopupModal(kPopupId, nullptr, ImGuiWindowFlags_NoCollapse))
    {
        return;
    }

    ImGui::TextWrapped(
        "These textures have dimensions that aren't a power of two. Polyphase requires "
        "power-of-two dimensions (e.g. 64, 128, 256, 512, 1024) for mipmapping and for "
        "compatibility with console platforms (PSP, 3DS, Wii, GameCube). Pick a fix per "
        "row, or use the bulk buttons at the bottom.");
    ImGui::Spacing();
    ImGui::TextDisabled("Pad: keeps every source pixel intact; new area is transparent.");
    ImGui::TextDisabled("Resize: bilinear-srgb downscale; lossy but smaller GPU footprint.");
    ImGui::Spacing();

    if (ImGui::BeginChild("FixupList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true))
    {
        for (size_t r = 0; r < mRows.size(); ++r)
        {
            PendingRow& row = mRows[r];
            ImGui::PushID((int)r);
            ImGui::Separator();

            const bool widthBad  = !Maths::IsPowerOfTwo(row.mSrcWidth);
            const bool heightBad = !Maths::IsPowerOfTwo(row.mSrcHeight);

            ImGui::Text("Asset: '%s'", row.mAssetName.c_str());
            ImGui::TextDisabled("Source: %s", row.mSourcePath.c_str());

            ImGui::Text("Dimensions: ");
            ImGui::SameLine();
            ImGui::TextColored(widthBad  ? ImVec4(1.0f, 0.5f, 0.4f, 1.0f) : ImVec4(0.7f, 0.9f, 0.7f, 1.0f),
                "%u", row.mSrcWidth);
            ImGui::SameLine();
            ImGui::Text(" x ");
            ImGui::SameLine();
            ImGui::TextColored(heightBad ? ImVec4(1.0f, 0.5f, 0.4f, 1.0f) : ImVec4(0.7f, 0.9f, 0.7f, 1.0f),
                "%u", row.mSrcHeight);

            if (row.mResolved != FixChoice::None)
            {
                const char* tag =
                    row.mResolved == FixChoice::Padded    ? "[Padded]"   :
                    row.mResolved == FixChoice::Resized   ? "[Resized]"  :
                                                            "[Cancelled]";
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%s", tag);
            }
            else if (row.mTexture.Get<Texture>() == nullptr)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f),
                    "(Asset no longer alive - cancel this row.)");
                if (ImGui::Button("Dismiss"))
                {
                    row.mResolved = FixChoice::Cancelled;
                }
            }
            else
            {
                char padLabel[64];
                snprintf(padLabel, sizeof(padLabel), "Pad to %ux%u",
                    row.mPadWidth, row.mPadHeight);
                if (ImGui::Button(padLabel))
                {
                    ApplyPad(row);
                }

                ImGui::SameLine();
                char resizeLabel[64];
                snprintf(resizeLabel, sizeof(resizeLabel), "Resize to %ux%u",
                    row.mResizeWidth, row.mResizeHeight);
                if (ImGui::Button(resizeLabel))
                {
                    ApplyResize(row);
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

    if (ImGui::Button("Pad All"))
    {
        for (PendingRow& row : mRows)
        {
            if (row.mResolved == FixChoice::None)
                ApplyPad(row);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Resize All"))
    {
        for (PendingRow& row : mRows)
        {
            if (row.mResolved == FixChoice::None)
                ApplyResize(row);
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
        // Anything still unresolved at Close is treated as Cancel so we don't
        // leave half-imported textures sitting in the registry with no GPU resource.
        for (PendingRow& row : mRows)
        {
            if (row.mResolved == FixChoice::None)
                ApplyCancel(row);
        }
        mRows.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

#endif // EDITOR
