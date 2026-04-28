#pragma once

#if EDITOR

#include <cstdint>
#include <string>
#include "imgui.h"

class Texture;
class Image;

class TextureAtlasViewer
{
public:
    ~TextureAtlasViewer();

    void DrawPanel();

private:
    // Loaded texture asset
    Texture* mTexture = nullptr;
    ImTextureID mImGuiTexId = 0;
    uint32_t mAtlasWidth = 0;
    uint32_t mAtlasHeight = 0;

    // Atlas grid configuration
    int32_t mTileWidth = 8;
    int32_t mTileHeight = 8;

    // Selection
    int32_t mSelectedTileIndex = -1;
    int32_t mHoveredTileIndex = -1;

    // Manual tile index lookup
    int32_t mLookupTileIndex = 0;

    // Zoom
    float mZoom = 2.0f;

    void SetTexture(Texture* tex);
    void ClearTexture();
    void DrawControls();
    void DrawAtlasImage();
    void DrawTilePreview();
};

TextureAtlasViewer* GetTextureAtlasViewer();

#endif
