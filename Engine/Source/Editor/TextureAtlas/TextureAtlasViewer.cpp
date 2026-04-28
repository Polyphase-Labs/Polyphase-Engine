#if EDITOR

#include "TextureAtlas/TextureAtlasViewer.h"
#include "EditorConstants.h"
#include "Assets/Texture.h"
#include "AssetManager.h"
#include "Log.h"

#include "imgui.h"

#if API_VULKAN
#include "Graphics/Vulkan/Image.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "backends/imgui_impl_vulkan.h"
#endif

static TextureAtlasViewer sTextureAtlasViewer;

TextureAtlasViewer* GetTextureAtlasViewer()
{
    return &sTextureAtlasViewer;
}

TextureAtlasViewer::~TextureAtlasViewer()
{
    ClearTexture();
}

void TextureAtlasViewer::SetTexture(Texture* tex)
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

void TextureAtlasViewer::ClearTexture()
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

void TextureAtlasViewer::DrawPanel()
{
    DrawControls();

    if (mImGuiTexId != 0 && mAtlasWidth > 0 && mAtlasHeight > 0)
    {
        ImGui::Separator();
        DrawAtlasImage();
        ImGui::Separator();
        DrawTilePreview();
    }
}

void TextureAtlasViewer::DrawControls()
{
    // Drop target for texture assets
    ImGui::Text("Drop a Texture asset here:");

    ImVec2 dropSize(ImGui::GetContentRegionAvail().x, 40.0f);
    ImVec2 dropPos = ImGui::GetCursorScreenPos();

    // Draw drop zone
    ImU32 dropColor = mTexture != nullptr ? IM_COL32(40, 100, 40, 255) : IM_COL32(60, 60, 60, 255);
    ImGui::GetWindowDrawList()->AddRectFilled(dropPos, ImVec2(dropPos.x + dropSize.x, dropPos.y + dropSize.y), dropColor, 4.0f);
    ImGui::GetWindowDrawList()->AddRect(dropPos, ImVec2(dropPos.x + dropSize.x, dropPos.y + dropSize.y), IM_COL32(120, 120, 120, 255), 4.0f, 0, 1.5f);

    // Label inside drop zone
    const char* label = mTexture != nullptr ? "Texture loaded - drop another to replace" : "Drag & drop texture here...";
    ImVec2 textSize = ImGui::CalcTextSize(label);
    ImGui::SetCursorScreenPos(ImVec2(dropPos.x + (dropSize.x - textSize.x) * 0.5f, dropPos.y + (dropSize.y - textSize.y) * 0.5f));
    ImGui::TextUnformatted(label);

    // Invisible button to create drop target
    ImGui::SetCursorScreenPos(dropPos);
    ImGui::InvisibleButton("##AtlasDrop", dropSize);

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
        {
            AssetStub* stub = *(AssetStub**)payload->Data;
            if (stub != nullptr && stub->mType == Texture::GetStaticType())
            {
                // Load the asset if not already loaded
                if (stub->mAsset == nullptr)
                {
                    AssetManager::Get()->LoadAsset(*stub);
                }

                if (stub->mAsset != nullptr)
                {
                    SetTexture(static_cast<Texture*>(stub->mAsset));
                    LogDebug("TextureAtlasViewer: Loaded texture '%s' (%dx%d)",
                        stub->mName.c_str(), mAtlasWidth, mAtlasHeight);
                }
            }
            else
            {
                LogWarning("TextureAtlasViewer: Dropped asset is not a Texture");
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (mTexture != nullptr)
    {
        ImGui::Text("Size: %dx%d", mAtlasWidth, mAtlasHeight);
    }

    // Tile size
    ImGui::Text("Tile Size:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("W##TileW", &mTileWidth, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("H##TileH", &mTileHeight, 0, 0);

    if (mTileWidth < 1) mTileWidth = 1;
    if (mTileHeight < 1) mTileHeight = 1;

    if (mAtlasWidth > 0 && mAtlasHeight > 0)
    {
        uint32_t tilesX = mAtlasWidth / mTileWidth;
        uint32_t tilesY = mAtlasHeight / mTileHeight;
        ImGui::SameLine();
        ImGui::Text("  Grid: %dx%d (%d tiles)", tilesX, tilesY, tilesX * tilesY);
    }

    // Zoom
    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("##Zoom", &mZoom, 0.5f, 8.0f, "%.1fx");

    // Tile index lookup
    ImGui::Separator();
    ImGui::Text("Tile Index Lookup:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputInt("##LookupIdx", &mLookupTileIndex, 0, 0))
    {
        if (mLookupTileIndex < 0) mLookupTileIndex = 0;
        mSelectedTileIndex = mLookupTileIndex;
    }

    if (mSelectedTileIndex >= 0 && mAtlasWidth > 0 && mTileWidth > 0 && mTileHeight > 0)
    {
        uint32_t tilesX = mAtlasWidth / mTileWidth;
        if (tilesX > 0)
        {
            int32_t tileCol = mSelectedTileIndex % tilesX;
            int32_t tileRow = mSelectedTileIndex / tilesX;
            ImGui::SameLine();
            ImGui::Text("  Row: %d  Col: %d  Pixel: (%d, %d)",
                tileRow, tileCol,
                tileCol * mTileWidth, tileRow * mTileHeight);
        }
    }
}

void TextureAtlasViewer::DrawAtlasImage()
{
    if (mImGuiTexId == 0)
        return;

    uint32_t tilesX = mAtlasWidth / mTileWidth;
    uint32_t tilesY = mAtlasHeight / mTileHeight;

    float drawW = mAtlasWidth * mZoom;
    float drawH = mAtlasHeight * mZoom;

    ImGui::BeginChild("AtlasScroll", ImVec2(0, 0), true,
        ImGuiWindowFlags_HorizontalScrollbar);

    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    // Draw the atlas image
    ImGui::Image(mImGuiTexId, ImVec2(drawW, drawH));

    // Draw grid overlay
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float tileSzX = static_cast<float>(mTileWidth) * mZoom;
    float tileSzY = static_cast<float>(mTileHeight) * mZoom;

    // Grid lines
    ImU32 gridColor = IM_COL32(255, 255, 255, 40);
    for (uint32_t x = 0; x <= tilesX; ++x)
    {
        float px = cursorPos.x + x * tileSzX;
        drawList->AddLine(ImVec2(px, cursorPos.y), ImVec2(px, cursorPos.y + drawH), gridColor);
    }
    for (uint32_t y = 0; y <= tilesY; ++y)
    {
        float py = cursorPos.y + y * tileSzY;
        drawList->AddLine(ImVec2(cursorPos.x, py), ImVec2(cursorPos.x + drawW, py), gridColor);
    }

    // Handle mouse interaction
    ImVec2 mousePos = ImGui::GetMousePos();
    mHoveredTileIndex = -1;

    if (mousePos.x >= cursorPos.x && mousePos.x < cursorPos.x + drawW &&
        mousePos.y >= cursorPos.y && mousePos.y < cursorPos.y + drawH)
    {
        int32_t col = static_cast<int32_t>((mousePos.x - cursorPos.x) / tileSzX);
        int32_t row = static_cast<int32_t>((mousePos.y - cursorPos.y) / tileSzY);

        if (col >= 0 && col < (int32_t)tilesX && row >= 0 && row < (int32_t)tilesY)
        {
            mHoveredTileIndex = row * tilesX + col;

            // Highlight hovered tile
            ImVec2 tileMin(cursorPos.x + col * tileSzX, cursorPos.y + row * tileSzY);
            ImVec2 tileMax(tileMin.x + tileSzX, tileMin.y + tileSzY);
            drawList->AddRect(tileMin, tileMax, IM_COL32(255, 255, 0, 180), 0.0f, 0, 2.0f);

            // Tooltip
            ImGui::BeginTooltip();
            ImGui::Text("Tile %d (Row %d, Col %d)", mHoveredTileIndex, row, col);
            ImGui::EndTooltip();

            // Click to select
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                mSelectedTileIndex = mHoveredTileIndex;
                mLookupTileIndex = mHoveredTileIndex;
            }
        }
    }

    // Highlight selected tile
    if (mSelectedTileIndex >= 0 && tilesX > 0)
    {
        int32_t selCol = mSelectedTileIndex % tilesX;
        int32_t selRow = mSelectedTileIndex / tilesX;

        if (selCol < (int32_t)tilesX && selRow < (int32_t)tilesY)
        {
            ImVec2 tileMin(cursorPos.x + selCol * tileSzX, cursorPos.y + selRow * tileSzY);
            ImVec2 tileMax(tileMin.x + tileSzX, tileMin.y + tileSzY);
            drawList->AddRect(tileMin, tileMax, IM_COL32(0, 255, 0, 255), 0.0f, 0, 3.0f);
        }
    }

    ImGui::EndChild();
}

void TextureAtlasViewer::DrawTilePreview()
{
    if (mSelectedTileIndex < 0 || mImGuiTexId == 0 || mAtlasWidth == 0 || mAtlasHeight == 0)
        return;

    uint32_t tilesX = mAtlasWidth / mTileWidth;
    uint32_t tilesY = mAtlasHeight / mTileHeight;

    if (tilesX == 0 || tilesY == 0)
        return;

    int32_t col = mSelectedTileIndex % tilesX;
    int32_t row = mSelectedTileIndex / tilesX;

    if (col >= (int32_t)tilesX || row >= (int32_t)tilesY)
        return;

    // Calculate UV rect for this tile
    float u0 = static_cast<float>(col * mTileWidth) / static_cast<float>(mAtlasWidth);
    float v0 = static_cast<float>(row * mTileHeight) / static_cast<float>(mAtlasHeight);
    float u1 = static_cast<float>((col + 1) * mTileWidth) / static_cast<float>(mAtlasWidth);
    float v1 = static_cast<float>((row + 1) * mTileHeight) / static_cast<float>(mAtlasHeight);

    ImGui::Text("Selected Tile: %d", mSelectedTileIndex);
    ImGui::SameLine();
    ImGui::Text("  UV: (%.4f, %.4f) - (%.4f, %.4f)", u0, v0, u1, v1);

    // Draw zoomed preview of the selected tile
    float previewSize = 128.0f;
    ImGui::Image(mImGuiTexId, ImVec2(previewSize, previewSize), ImVec2(u0, v0), ImVec2(u1, v1));
}

#endif
