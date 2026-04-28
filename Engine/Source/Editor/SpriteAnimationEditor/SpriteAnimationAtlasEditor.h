#pragma once

#if EDITOR

#include <cstdint>
#include <vector>
#include "imgui.h"

class Texture;
class SpriteAnimation;

// Modal popup for editing the atlas-mode frame layout of a SpriteAnimation
// asset. Shows the atlas texture with a configurable grid overlay; the user
// clicks cells to append them to the playback order, right-clicks to remove,
// and applies to write the grid + frame-index list back to the asset.
//
// Mirrors the lifecycle of TextureCropEditor: a single static instance, open
// from anywhere, draws itself when DrawPopup() is called per frame.
//
// Grid metadata (cols/rows/margin/spacing) carries over between editor
// sessions so users authoring multiple SpriteAnimations from the same atlas
// don't have to re-enter the grid each time. The carry-over only seeds *fresh*
// SpriteAnimations (those still at default cols=1, rows=1, with no frames).
class SpriteAnimationAtlasEditor
{
public:
    ~SpriteAnimationAtlasEditor();

    void Open(SpriteAnimation* anim);
    void DrawPopup();

    bool IsOpen() const { return mIsOpen; }

private:
    void SetTexture(Texture* tex);
    void ClearTexture();
    void DrawGridControls();
    void DrawAtlasCanvas();
    void DrawFrameList();
    int32_t CellCount() const { return mCols * mRows; }

    // Asset being edited (raw pointer — editor session is short-lived; user can't
    // delete the asset while the modal is open via the same UI).
    SpriteAnimation* mAsset = nullptr;

    // Vulkan descriptor for the atlas texture (same lifecycle as the other editors).
    Texture* mTexture = nullptr;
    ImTextureID mImGuiTexId = 0;
    uint32_t mAtlasWidth = 0;
    uint32_t mAtlasHeight = 0;

    // Working copy of the asset's grid metadata + frame list. Committed on Apply.
    int32_t mCols = 1;
    int32_t mRows = 1;
    int32_t mMarginX = 0;
    int32_t mMarginY = 0;
    int32_t mSpacingX = 0;
    int32_t mSpacingY = 0;
    std::vector<int32_t> mFrameIndices;

    int32_t mHoveredCell = -1;

    bool mIsOpen = false;
    bool mJustOpened = false;
    float mZoom = 2.0f;
    uint64_t mLastDrawFrame = 0;

    // Carry-over defaults for the next *fresh* SpriteAnimation opened. Set on
    // every Apply. Reused only when the incoming asset has not yet been
    // configured (cols=1, rows=1, no frames).
    static int32_t sLastCols;
    static int32_t sLastRows;
    static int32_t sLastMarginX;
    static int32_t sLastMarginY;
    static int32_t sLastSpacingX;
    static int32_t sLastSpacingY;
    static bool sHasCarryover;
};

SpriteAnimationAtlasEditor* GetSpriteAnimationAtlasEditor();

#endif
