#pragma once

#if EDITOR

#include <cstdint>
#include <functional>
#include "imgui.h"
#include "glm/glm.hpp"

class Texture;

class TextureCropEditor
{
public:
    ~TextureCropEditor();

    void Open(Texture* texture, glm::vec2 currentUVScale, glm::vec2 currentUVOffset,
              std::function<void(glm::vec2 uvScale, glm::vec2 uvOffset)> onApply);

    void DrawPopup();

    bool IsOpen() const { return mIsOpen; }

private:
    enum class DragMode
    {
        None,
        Move,
        ResizeTL,
        ResizeTR,
        ResizeBL,
        ResizeBR,
        ResizeT,
        ResizeB,
        ResizeL,
        ResizeR
    };

    void SetTexture(Texture* tex);
    void ClearTexture();
    void DrawCropCanvas();
    void HandleCropInteraction(ImVec2 canvasPos, float drawW, float drawH, bool canvasHovered, bool canvasActive);

    // Convert between crop rect and UV scale/offset
    // Quad formula: finalUV = (baseUV + offset) * scale
    glm::vec2 CalcUVScale() const;
    glm::vec2 CalcUVOffset() const;
    void SetFromUV(glm::vec2 uvScale, glm::vec2 uvOffset);

    // Texture state
    Texture* mTexture = nullptr;
    ImTextureID mImGuiTexId = 0;
    uint32_t mTexWidth = 0;
    uint32_t mTexHeight = 0;

    // Crop rectangle in normalized [0,1] texture coords
    glm::vec2 mCropMin = glm::vec2(0.0f);
    glm::vec2 mCropMax = glm::vec2(1.0f);

    // Interaction state
    DragMode mDragMode = DragMode::None;
    glm::vec2 mDragStartMouse = glm::vec2(0.0f);
    glm::vec2 mDragStartCropMin = glm::vec2(0.0f);
    glm::vec2 mDragStartCropMax = glm::vec2(0.0f);

    // Display
    float mZoom = 1.0f;
    uint64_t mLastDrawFrame = 0;

    // Callback
    std::function<void(glm::vec2, glm::vec2)> mOnApply;
    bool mIsOpen = false;
    bool mJustOpened = false;
};

TextureCropEditor* GetTextureCropEditor();

#endif
