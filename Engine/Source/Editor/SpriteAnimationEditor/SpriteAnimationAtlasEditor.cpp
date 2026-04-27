#if EDITOR

#include "SpriteAnimationEditor/SpriteAnimationAtlasEditor.h"
#include "Assets/SpriteAnimation.h"
#include "Assets/Texture.h"
#include "AssetManager.h"
#include "Log.h"

#include "imgui.h"

#if API_VULKAN
#include "Graphics/Vulkan/Image.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "backends/imgui_impl_vulkan.h"
#endif

int32_t SpriteAnimationAtlasEditor::sLastCols = 1;
int32_t SpriteAnimationAtlasEditor::sLastRows = 1;
int32_t SpriteAnimationAtlasEditor::sLastMarginX = 0;
int32_t SpriteAnimationAtlasEditor::sLastMarginY = 0;
int32_t SpriteAnimationAtlasEditor::sLastSpacingX = 0;
int32_t SpriteAnimationAtlasEditor::sLastSpacingY = 0;
bool SpriteAnimationAtlasEditor::sHasCarryover = false;

static SpriteAnimationAtlasEditor sSpriteAnimationAtlasEditor;

SpriteAnimationAtlasEditor* GetSpriteAnimationAtlasEditor()
{
    return &sSpriteAnimationAtlasEditor;
}

SpriteAnimationAtlasEditor::~SpriteAnimationAtlasEditor()
{
    ClearTexture();
}

void SpriteAnimationAtlasEditor::SetTexture(Texture* tex)
{
    ClearTexture();

    if (tex == nullptr)
        return;

#if API_VULKAN
    TextureResource* res = tex->GetResource();
    if (res == nullptr || res->mImage == nullptr)
        return;

    mImGuiTexId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
        res->mImage->GetSampler(),
        res->mImage->GetView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    mTexture = tex;
    mAtlasWidth = tex->GetWidth();
    mAtlasHeight = tex->GetHeight();
#endif
}

void SpriteAnimationAtlasEditor::ClearTexture()
{
#if API_VULKAN
    if (mImGuiTexId != 0)
    {
        DeviceWaitIdle();
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)mImGuiTexId);
        mImGuiTexId = 0;
    }
#endif
    mTexture = nullptr;
    mAtlasWidth = 0;
    mAtlasHeight = 0;
}

void SpriteAnimationAtlasEditor::Open(SpriteAnimation* anim)
{
    if (anim == nullptr)
        return;

    mAsset = anim;

    // Pull the working copy from the asset's current state.
    mCols      = anim->GetAtlasCols();
    mRows      = anim->GetAtlasRows();
    mMarginX   = 0;
    mMarginY   = 0;
    mSpacingX  = 0;
    mSpacingY  = 0;
    mFrameIndices = anim->GetAtlasFrameIndices();

    // Reach the protected margin/spacing through GatherProperties — the asset's
    // header doesn't expose getters for those today. Cheaper alternative:
    // GatherProperties round-trip. For v1, just expose them via friend-style
    // access: we know the Property names match the asset's members.
    {
        std::vector<Property> props;
        anim->GatherProperties(props);
        for (const Property& p : props)
        {
            if (p.GetCount() == 0) continue;
            if      (p.mName == "Atlas Margin X")  mMarginX  = p.GetInteger();
            else if (p.mName == "Atlas Margin Y")  mMarginY  = p.GetInteger();
            else if (p.mName == "Atlas Spacing X") mSpacingX = p.GetInteger();
            else if (p.mName == "Atlas Spacing Y") mSpacingY = p.GetInteger();
        }
    }

    // Carry-over: if the asset is at fresh defaults and we have a previous
    // session's grid stashed, seed the working copy from that.
    const bool atDefaults = (mCols <= 1 && mRows <= 1 && mFrameIndices.empty()
                             && mMarginX == 0 && mMarginY == 0
                             && mSpacingX == 0 && mSpacingY == 0);
    if (atDefaults && sHasCarryover)
    {
        mCols     = sLastCols;
        mRows     = sLastRows;
        mMarginX  = sLastMarginX;
        mMarginY  = sLastMarginY;
        mSpacingX = sLastSpacingX;
        mSpacingY = sLastSpacingY;
    }

    SetTexture(anim->GetAtlasTexture());

    mIsOpen = true;
    mJustOpened = true;
    mHoveredCell = -1;

    // Auto-fit zoom for ~520px wide canvas.
    if (mAtlasWidth > 0)
    {
        mZoom = 520.0f / static_cast<float>(mAtlasWidth);
        if (mZoom < 0.5f) mZoom = 0.5f;
        if (mZoom > 8.0f) mZoom = 8.0f;
    }
    else
    {
        mZoom = 2.0f;
    }
}

void SpriteAnimationAtlasEditor::DrawPopup()
{
    if (!mIsOpen)
        return;

    // Idempotent within a frame so multiple GatherProperties calls don't
    // double-draw. Same pattern as TextureCropEditor.
    uint64_t frameCount = (uint64_t)ImGui::GetFrameCount();
    if (mLastDrawFrame == frameCount)
        return;
    mLastDrawFrame = frameCount;

    ImGuiIO& io = ImGui::GetIO();
    if (mJustOpened)
    {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(820.0f, 720.0f), ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        mJustOpened = false;
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(640.0f, 480.0f), ImVec2(FLT_MAX, FLT_MAX));

    bool open = mIsOpen;
    if (ImGui::Begin("Sprite Atlas Editor", &open, ImGuiWindowFlags_NoCollapse))
    {
        if (mAsset == nullptr)
        {
            ImGui::Text("No SpriteAnimation asset open.");
            if (ImGui::Button("Close"))
            {
                mIsOpen = false;
                ClearTexture();
            }
            ImGui::End();
            return;
        }

        // Top: drop-target hint + atlas size info.
        if (mTexture != nullptr)
        {
            ImGui::Text("Atlas: %s  (%dx%d)", mTexture->GetName().c_str(), mAtlasWidth, mAtlasHeight);
        }
        else
        {
            ImGui::TextDisabled("No atlas texture set on asset. Assign one in the inspector.");
        }

        ImGui::Separator();
        DrawGridControls();
        ImGui::Separator();

        // Two-column layout: atlas canvas on the left, frame list on the right.
        ImVec2 avail = ImGui::GetContentRegionAvail();
        const float reservedBottom = 56.0f;       // for Apply/Cancel buttons
        const float listW = 240.0f;
        const float canvasW = avail.x - listW - 8.0f;
        const float bodyH = (avail.y > reservedBottom) ? (avail.y - reservedBottom) : 200.0f;

        if (canvasW > 100.0f)
        {
            ImGui::BeginChild("AtlasCanvasOuter", ImVec2(canvasW, bodyH), true,
                              ImGuiWindowFlags_HorizontalScrollbar);
            DrawAtlasCanvas();
            ImGui::EndChild();
            ImGui::SameLine();
        }

        ImGui::BeginChild("FrameListOuter", ImVec2(listW, bodyH), true);
        DrawFrameList();
        ImGui::EndChild();

        ImGui::Separator();

        // Apply / Cancel / Reset
        if (ImGui::Button("Apply"))
        {
            mAsset->SetAtlasGrid(mCols, mRows);
            mAsset->SetAtlasMargin(mMarginX, mMarginY);
            mAsset->SetAtlasSpacing(mSpacingX, mSpacingY);
            mAsset->GetAtlasFrameIndicesMutable() = mFrameIndices;

            // Stash for the next fresh asset.
            sLastCols     = mCols;
            sLastRows     = mRows;
            sLastMarginX  = mMarginX;
            sLastMarginY  = mMarginY;
            sLastSpacingX = mSpacingX;
            sLastSpacingY = mSpacingY;
            sHasCarryover = true;

            // Persist immediately so Cancel-after-Apply doesn't roll back.
            AssetStub* stub = AssetManager::Get()->GetAssetStub(mAsset->GetName());
            if (stub != nullptr)
            {
                AssetManager::Get()->SaveAsset(*stub);
            }

            mIsOpen = false;
            mAsset = nullptr;
            ClearTexture();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            mIsOpen = false;
            mAsset = nullptr;
            ClearTexture();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Frames"))
        {
            mFrameIndices.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Add All Cells"))
        {
            mFrameIndices.clear();
            mFrameIndices.reserve(CellCount());
            for (int32_t i = 0; i < CellCount(); ++i)
            {
                mFrameIndices.push_back(i);
            }
        }
    }
    ImGui::End();

    if (!open && mIsOpen)
    {
        mIsOpen = false;
        mAsset = nullptr;
        ClearTexture();
    }
}

void SpriteAnimationAtlasEditor::DrawGridControls()
{
    ImGui::Text("Grid:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("Cols##SAE", &mCols, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("Rows##SAE", &mRows, 0, 0);

    ImGui::SameLine();
    ImGui::Text("  Margin:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("X##SAEmar", &mMarginX, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("Y##SAEmar", &mMarginY, 0, 0);

    ImGui::SameLine();
    ImGui::Text("  Spacing:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("X##SAEsp", &mSpacingX, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("Y##SAEsp", &mSpacingY, 0, 0);

    if (mCols < 1) mCols = 1;
    if (mRows < 1) mRows = 1;
    if (mMarginX < 0) mMarginX = 0;
    if (mMarginY < 0) mMarginY = 0;
    if (mSpacingX < 0) mSpacingX = 0;
    if (mSpacingY < 0) mSpacingY = 0;

    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("##SAEZoom", &mZoom, 0.25f, 8.0f, "%.2fx");
    ImGui::SameLine();
    ImGui::TextDisabled("Click to add frame, right-click to remove");
}

void SpriteAnimationAtlasEditor::DrawAtlasCanvas()
{
    if (mImGuiTexId == 0 || mAtlasWidth == 0 || mAtlasHeight == 0)
    {
        ImGui::TextDisabled("(no atlas texture)");
        return;
    }

    // Cell pixel size derived the same way SpriteAnimation does at runtime so
    // the editor preview matches what the engine will sample.
    const int32_t usableW = int32_t(mAtlasWidth) - mMarginX * 2 - mSpacingX * (mCols - 1);
    const int32_t usableH = int32_t(mAtlasHeight) - mMarginY * 2 - mSpacingY * (mRows - 1);
    if (usableW <= 0 || usableH <= 0)
    {
        ImGui::TextDisabled("(margin/spacing exceeds atlas size)");
        return;
    }
    const int32_t cellW = usableW / mCols;
    const int32_t cellH = usableH / mRows;
    if (cellW <= 0 || cellH <= 0)
    {
        ImGui::TextDisabled("(cell size is zero — adjust grid)");
        return;
    }

    const float drawW = mAtlasWidth * mZoom;
    const float drawH = mAtlasHeight * mZoom;

    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImGui::Image(mImGuiTexId, ImVec2(drawW, drawH));

    ImGui::SetCursorScreenPos(cursorPos);
    ImGui::InvisibleButton("##SAECanvas", ImVec2(drawW, drawH));
    bool hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    auto cellRect = [&](int32_t cellIndex, ImVec2& outMin, ImVec2& outMax)
    {
        int32_t col = cellIndex % mCols;
        int32_t row = cellIndex / mCols;
        float px0 = (float)(mMarginX + col * (cellW + mSpacingX));
        float py0 = (float)(mMarginY + row * (cellH + mSpacingY));
        outMin = ImVec2(cursorPos.x + px0 * mZoom, cursorPos.y + py0 * mZoom);
        outMax = ImVec2(outMin.x + cellW * mZoom, outMin.y + cellH * mZoom);
    };

    // Grid lines (just at cell boundaries).
    ImU32 gridColor = IM_COL32(255, 255, 255, 50);
    for (int32_t i = 0; i < CellCount(); ++i)
    {
        ImVec2 mn, mx;
        cellRect(i, mn, mx);
        drawList->AddRect(mn, mx, gridColor);
    }

    // Highlight cells already in the frame list, badged with their playback index.
    for (size_t pi = 0; pi < mFrameIndices.size(); ++pi)
    {
        int32_t cellIdx = mFrameIndices[pi];
        if (cellIdx < 0 || cellIdx >= CellCount()) continue;

        ImVec2 mn, mx;
        cellRect(cellIdx, mn, mx);
        drawList->AddRectFilled(mn, mx, IM_COL32(0, 255, 0, 35));
        drawList->AddRect(mn, mx, IM_COL32(0, 255, 0, 200), 0.0f, 0, 2.0f);

        char badge[16];
        snprintf(badge, sizeof(badge), "%d", (int)pi);
        ImVec2 tsz = ImGui::CalcTextSize(badge);
        ImVec2 tpos(mn.x + 3.0f, mn.y + 1.0f);
        drawList->AddRectFilled(tpos, ImVec2(tpos.x + tsz.x + 4.0f, tpos.y + tsz.y + 2.0f),
                                IM_COL32(0, 0, 0, 180));
        drawList->AddText(ImVec2(tpos.x + 2.0f, tpos.y + 1.0f), IM_COL32(255, 255, 255, 255), badge);
    }

    // Hover + click handling.
    mHoveredCell = -1;
    if (hovered)
    {
        ImVec2 mp = ImGui::GetMousePos();
        // Convert mouse to atlas pixel coords.
        int32_t px = (int32_t)((mp.x - cursorPos.x) / mZoom);
        int32_t py = (int32_t)((mp.y - cursorPos.y) / mZoom);

        // Compute cell from pixel coords (account for margin + spacing).
        int32_t cx = (px - mMarginX) / (cellW + mSpacingX);
        int32_t cy = (py - mMarginY) / (cellH + mSpacingY);
        // Reject hits in margin/spacing gaps.
        int32_t inCellX = (px - mMarginX) - cx * (cellW + mSpacingX);
        int32_t inCellY = (py - mMarginY) - cy * (cellH + mSpacingY);
        if (cx >= 0 && cx < mCols && cy >= 0 && cy < mRows
            && inCellX >= 0 && inCellX < cellW
            && inCellY >= 0 && inCellY < cellH)
        {
            mHoveredCell = cy * mCols + cx;

            ImVec2 mn, mx;
            cellRect(mHoveredCell, mn, mx);
            drawList->AddRect(mn, mx, IM_COL32(255, 255, 0, 220), 0.0f, 0, 2.0f);

            ImGui::BeginTooltip();
            ImGui::Text("Cell %d  (col %d, row %d)", mHoveredCell, cx, cy);
            ImGui::EndTooltip();

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                mFrameIndices.push_back(mHoveredCell);
            }
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                // Remove the LAST occurrence so repeated frames in the playback
                // list can be culled one at a time.
                for (int32_t i = (int32_t)mFrameIndices.size() - 1; i >= 0; --i)
                {
                    if (mFrameIndices[i] == mHoveredCell)
                    {
                        mFrameIndices.erase(mFrameIndices.begin() + i);
                        break;
                    }
                }
            }
        }
    }
}

void SpriteAnimationAtlasEditor::DrawFrameList()
{
    ImGui::Text("Playback Order  (%d frame%s)",
                (int)mFrameIndices.size(),
                mFrameIndices.size() == 1 ? "" : "s");
    ImGui::Separator();

    int32_t removeIndex = -1;
    int32_t moveUpIndex = -1;
    int32_t moveDownIndex = -1;

    for (size_t i = 0; i < mFrameIndices.size(); ++i)
    {
        ImGui::PushID((int)i);

        ImGui::Text("%2zu:", i);
        ImGui::SameLine();

        const int32_t cellIdx = mFrameIndices[i];
        const int32_t col = (mCols > 0) ? (cellIdx % mCols) : 0;
        const int32_t row = (mCols > 0) ? (cellIdx / mCols) : 0;
        ImGui::Text("cell %d  (%d,%d)", cellIdx, col, row);

        ImGui::SameLine();
        if (ImGui::SmallButton("^") && i > 0)
        {
            moveUpIndex = (int32_t)i;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("v") && i + 1 < mFrameIndices.size())
        {
            moveDownIndex = (int32_t)i;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x"))
        {
            removeIndex = (int32_t)i;
        }

        ImGui::PopID();
    }

    if (removeIndex >= 0)
    {
        mFrameIndices.erase(mFrameIndices.begin() + removeIndex);
    }
    if (moveUpIndex > 0)
    {
        std::swap(mFrameIndices[moveUpIndex], mFrameIndices[moveUpIndex - 1]);
    }
    if (moveDownIndex >= 0 && moveDownIndex + 1 < (int32_t)mFrameIndices.size())
    {
        std::swap(mFrameIndices[moveDownIndex], mFrameIndices[moveDownIndex + 1]);
    }
}

#endif
