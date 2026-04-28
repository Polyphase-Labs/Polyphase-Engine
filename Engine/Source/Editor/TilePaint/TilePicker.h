#pragma once

#if EDITOR

#include <cstdint>

class Texture;

// Reusable atlas tile picker. Hoisted from the Voxel sculpt panel so the
// TileMap2D paint panel and (eventually) the TileSet asset editor can share
// the same UI primitives.
namespace TilePicker
{
    // Get or create the ImGui texture descriptor for an atlas. Caches one
    // descriptor per (cacheKey, texture) pair so multiple panels can call this
    // every frame without leaking descriptors. Returns 0 if no atlas is bound.
    void* GetOrCreateImTextureID(const char* cacheKey, Texture* atlasTex);

    // Single tile preview button. Returns true if the user clicked it this frame.
    bool DrawTileButton(const char* id, void* atlasTexId, int32_t tileIdx,
                        int32_t tilesX, int32_t tilesY, float size);

    // Popup containing a scrollable atlas grid. The user clicks a tile to
    // select it; the chosen index is written into outTileIdx. Caller is
    // responsible for opening the popup with ImGui::OpenPopup(popupId).
    void DrawTilePickerPopup(const char* popupId, void* atlasTexId,
                             int32_t tilesX, int32_t tilesY,
                             int32_t& outTileIdx);

    // Inline atlas grid (not a popup) suitable for the palette panel. Returns
    // true if the user clicked a tile this frame.
    bool DrawInlineTileGrid(const char* id, void* atlasTexId,
                            int32_t tilesX, int32_t tilesY,
                            int32_t& selectedTileIdx,
                            float tileDrawSize,
                            float maxWidth, float maxHeight);
}

#endif
