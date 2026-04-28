#if EDITOR

#include "TilePaint/TilePicker.h"

#include "Assets/Texture.h"
#include "Graphics/GraphicsTypes.h"
#include "Graphics/Vulkan/Image.h"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include <vulkan/vulkan.h>

#include "Maths.h"

#include <unordered_map>
#include <string>
#include <algorithm>

namespace TilePicker
{
    struct CachedDescriptor
    {
        Texture* mLast = nullptr;
        ImTextureID mId = 0;
    };

    static std::unordered_map<std::string, CachedDescriptor> sCache;

    void* GetOrCreateImTextureID(const char* cacheKey, Texture* atlasTex)
    {
        CachedDescriptor& cache = sCache[cacheKey];
        if (cache.mLast == atlasTex && cache.mId != 0)
            return (void*)cache.mId;

        if (cache.mId != 0)
        {
            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)cache.mId);
            cache.mId = 0;
        }

        if (atlasTex != nullptr)
        {
            TextureResource* res = atlasTex->GetResource();
            if (res != nullptr && res->mImage != nullptr)
            {
                cache.mId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
                    res->mImage->GetSampler(),
                    res->mImage->GetView(),
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        cache.mLast = atlasTex;
        return (void*)cache.mId;
    }

    bool DrawTileButton(const char* id, void* atlasTexId, int32_t tileIdx,
                        int32_t tilesX, int32_t tilesY, float size)
    {
        bool clicked = false;
        ImTextureID texId = (ImTextureID)atlasTexId;

        if (texId != 0 && tilesX > 0 && tilesY > 0 && tileIdx >= 0)
        {
            int32_t col = tileIdx % tilesX;
            int32_t row = tileIdx / tilesX;
            float u0 = float(col) / float(tilesX);
            float v0 = float(row) / float(tilesY);
            float u1 = float(col + 1) / float(tilesX);
            float v1 = float(row + 1) / float(tilesY);

            ImGui::PushID(id);
            clicked = ImGui::ImageButton(id, texId, ImVec2(size, size), ImVec2(u0, v0), ImVec2(u1, v1));
            ImGui::PopID();

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Tile %d (Row %d, Col %d)", tileIdx, row, col);
                ImGui::Image(texId, ImVec2(64, 64), ImVec2(u0, v0), ImVec2(u1, v1));
                ImGui::EndTooltip();
            }
        }
        else
        {
            ImGui::PushID(id);
            clicked = ImGui::Button("?", ImVec2(size + 8, size + 8));
            ImGui::PopID();
        }

        return clicked;
    }

    void DrawTilePickerPopup(const char* popupId, void* atlasTexId,
                             int32_t tilesX, int32_t tilesY,
                             int32_t& outTileIdx)
    {
        if (!ImGui::BeginPopup(popupId))
            return;

        ImGui::Text("Click a tile:");
        ImTextureID texId = (ImTextureID)atlasTexId;

        if (texId != 0 && tilesX > 0 && tilesY > 0)
        {
            float tileDrawSize = 24.0f;
            float gridW = tilesX * tileDrawSize;
            float gridH = tilesY * tileDrawSize;
            float maxW = 400.0f;
            float maxH = 300.0f;

            ImGui::BeginChild("TileGrid",
                ImVec2(glm::min(gridW + 16.0f, maxW), glm::min(gridH + 16.0f, maxH)),
                true, ImGuiWindowFlags_HorizontalScrollbar);

            ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Image(texId, ImVec2(gridW, gridH));
            bool imageHovered = ImGui::IsItemHovered();

            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (int32_t gx = 0; gx <= tilesX; ++gx)
                dl->AddLine(ImVec2(origin.x + gx * tileDrawSize, origin.y),
                            ImVec2(origin.x + gx * tileDrawSize, origin.y + gridH),
                            IM_COL32(255, 255, 255, 30));
            for (int32_t gy = 0; gy <= tilesY; ++gy)
                dl->AddLine(ImVec2(origin.x, origin.y + gy * tileDrawSize),
                            ImVec2(origin.x + gridW, origin.y + gy * tileDrawSize),
                            IM_COL32(255, 255, 255, 30));

            if (outTileIdx >= 0 && outTileIdx < tilesX * tilesY)
            {
                int32_t sc = outTileIdx % tilesX;
                int32_t sr = outTileIdx / tilesX;
                dl->AddRect(ImVec2(origin.x + sc * tileDrawSize, origin.y + sr * tileDrawSize),
                            ImVec2(origin.x + (sc + 1) * tileDrawSize, origin.y + (sr + 1) * tileDrawSize),
                            IM_COL32(0, 255, 0, 255), 0, 0, 2.0f);
            }

            ImVec2 mp = ImGui::GetMousePos();
            if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                int32_t clickCol = int32_t((mp.x - origin.x) / tileDrawSize);
                int32_t clickRow = int32_t((mp.y - origin.y) / tileDrawSize);
                if (clickCol >= 0 && clickCol < tilesX && clickRow >= 0 && clickRow < tilesY)
                {
                    outTileIdx = clickRow * tilesX + clickCol;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndChild();
        }
        else
        {
            ImGui::Text("No atlas texture set.");
        }

        ImGui::EndPopup();
    }

    bool DrawInlineTileGrid(const char* id, void* atlasTexId,
                            int32_t tilesX, int32_t tilesY,
                            int32_t& selectedTileIdx,
                            float tileDrawSize,
                            float maxWidth, float maxHeight)
    {
        bool clickedThisFrame = false;
        ImTextureID texId = (ImTextureID)atlasTexId;

        if (texId == 0 || tilesX <= 0 || tilesY <= 0)
        {
            ImGui::TextDisabled("No atlas bound.");
            return false;
        }

        float gridW = tilesX * tileDrawSize;
        float gridH = tilesY * tileDrawSize;

        ImGui::BeginChild(id,
            ImVec2(glm::min(gridW + 16.0f, maxWidth), glm::min(gridH + 16.0f, maxHeight)),
            true, ImGuiWindowFlags_HorizontalScrollbar);

        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Image(texId, ImVec2(gridW, gridH));
        bool imageHovered = ImGui::IsItemHovered();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (int32_t gx = 0; gx <= tilesX; ++gx)
            dl->AddLine(ImVec2(origin.x + gx * tileDrawSize, origin.y),
                        ImVec2(origin.x + gx * tileDrawSize, origin.y + gridH),
                        IM_COL32(255, 255, 255, 50));
        for (int32_t gy = 0; gy <= tilesY; ++gy)
            dl->AddLine(ImVec2(origin.x, origin.y + gy * tileDrawSize),
                        ImVec2(origin.x + gridW, origin.y + gy * tileDrawSize),
                        IM_COL32(255, 255, 255, 50));

        if (selectedTileIdx >= 0 && selectedTileIdx < tilesX * tilesY)
        {
            int32_t sc = selectedTileIdx % tilesX;
            int32_t sr = selectedTileIdx / tilesX;
            dl->AddRect(ImVec2(origin.x + sc * tileDrawSize, origin.y + sr * tileDrawSize),
                        ImVec2(origin.x + (sc + 1) * tileDrawSize, origin.y + (sr + 1) * tileDrawSize),
                        IM_COL32(0, 255, 0, 255), 0, 0, 2.0f);
        }

        ImVec2 mp = ImGui::GetMousePos();
        if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            int32_t clickCol = int32_t((mp.x - origin.x) / tileDrawSize);
            int32_t clickRow = int32_t((mp.y - origin.y) / tileDrawSize);
            if (clickCol >= 0 && clickCol < tilesX && clickRow >= 0 && clickRow < tilesY)
            {
                selectedTileIdx = clickRow * tilesX + clickCol;
                clickedThisFrame = true;
            }
        }

        ImGui::EndChild();
        return clickedThisFrame;
    }
}

#endif
