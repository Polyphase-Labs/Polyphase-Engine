#if EDITOR

#include "TextureCrop/TextureCropEditor.h"
#include "Assets/Texture.h"
#include "Log.h"

#include "imgui.h"

#if API_VULKAN
#include "Graphics/Vulkan/Image.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "backends/imgui_impl_vulkan.h"
#endif

static TextureCropEditor sTextureCropEditor;

TextureCropEditor* GetTextureCropEditor()
{
    return &sTextureCropEditor;
}

TextureCropEditor::~TextureCropEditor()
{
    ClearTexture();
}

void TextureCropEditor::SetTexture(Texture* tex)
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
    mTexWidth = tex->GetWidth();
    mTexHeight = tex->GetHeight();
#endif
}

void TextureCropEditor::ClearTexture()
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
    mTexWidth = 0;
    mTexHeight = 0;
}

void TextureCropEditor::Open(Texture* texture, glm::vec2 currentUVScale, glm::vec2 currentUVOffset,
                             std::function<void(glm::vec2 uvScale, glm::vec2 uvOffset)> onApply)
{
    SetTexture(texture);
    SetFromUV(currentUVScale, currentUVOffset);
    mOnApply = onApply;
    mIsOpen = true;
    mJustOpened = true;
    mDragMode = DragMode::None;

    // Auto-calculate zoom to fit ~400px wide
    if (mTexWidth > 0)
    {
        mZoom = 400.0f / static_cast<float>(mTexWidth);
        if (mZoom < 0.5f) mZoom = 0.5f;
        if (mZoom > 4.0f) mZoom = 4.0f;
    }
    else
    {
        mZoom = 1.0f;
    }
}

glm::vec2 TextureCropEditor::CalcUVScale() const
{
    return mCropMax - mCropMin;
}

glm::vec2 TextureCropEditor::CalcUVOffset() const
{
    glm::vec2 scale = CalcUVScale();
    glm::vec2 offset(0.0f);
    if (scale.x > 0.0001f)
        offset.x = mCropMin.x / scale.x;
    if (scale.y > 0.0001f)
        offset.y = mCropMin.y / scale.y;
    return offset;
}

void TextureCropEditor::SetFromUV(glm::vec2 uvScale, glm::vec2 uvOffset)
{
    // Reverse: cropMin = uvOffset * uvScale, cropMax = cropMin + uvScale
    mCropMin = uvOffset * uvScale;
    mCropMax = mCropMin + uvScale;

    // Clamp to valid range
    mCropMin = glm::clamp(mCropMin, glm::vec2(0.0f), glm::vec2(1.0f));
    mCropMax = glm::clamp(mCropMax, glm::vec2(0.0f), glm::vec2(1.0f));

    // Ensure min < max
    if (mCropMax.x <= mCropMin.x) mCropMax.x = mCropMin.x + 0.01f;
    if (mCropMax.y <= mCropMin.y) mCropMax.y = mCropMin.y + 0.01f;
}

void TextureCropEditor::DrawPopup()
{
    if (!mIsOpen)
        return;

    // Guard against multiple DrawPopup calls per frame (e.g. duplicate properties)
    ImGuiIO& io = ImGui::GetIO();
    uint64_t frameCount = (uint64_t)ImGui::GetFrameCount();
    if (mLastDrawFrame == frameCount)
        return;
    mLastDrawFrame = frameCount;

    if (mJustOpened)
    {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(600.0f, 700.0f), ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        mJustOpened = false;
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, 400.0f), ImVec2(FLT_MAX, FLT_MAX));

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
    if (mDragMode != DragMode::None)
        windowFlags |= ImGuiWindowFlags_NoMove;

    bool open = mIsOpen;
    if (ImGui::Begin("Texture Crop Editor", &open, windowFlags))
    {
        if (mImGuiTexId == 0 || mTexWidth == 0 || mTexHeight == 0)
        {
            ImGui::Text("No texture loaded.");
            if (ImGui::Button("Close"))
            {
                mIsOpen = false;
                ClearTexture();
            }
            ImGui::End();
            return;
        }

        // Zoom slider
        ImGui::Text("Zoom:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SliderFloat("##CropZoom", &mZoom, 0.25f, 8.0f, "%.2fx");

        ImGui::Separator();

        // Draw the texture with crop overlay
        DrawCropCanvas();

        ImGui::Separator();

        // Numeric crop inputs
        ImGui::Text("Crop Region (normalized):");

        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputFloat("Min X", &mCropMin.x, 0.0f, 0.0f, "%.4f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputFloat("Min Y", &mCropMin.y, 0.0f, 0.0f, "%.4f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputFloat("Max X", &mCropMax.x, 0.0f, 0.0f, "%.4f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputFloat("Max Y", &mCropMax.y, 0.0f, 0.0f, "%.4f");

        // Clamp inputs
        mCropMin = glm::clamp(mCropMin, glm::vec2(0.0f), glm::vec2(1.0f));
        mCropMax = glm::clamp(mCropMax, glm::vec2(0.0f), glm::vec2(1.0f));
        if (mCropMax.x <= mCropMin.x) mCropMax.x = glm::min(mCropMin.x + 0.01f, 1.0f);
        if (mCropMax.y <= mCropMin.y) mCropMax.y = glm::min(mCropMin.y + 0.01f, 1.0f);

        // Pixel coordinates display
        ImGui::Text("Pixels: (%d, %d) - (%d, %d)  Size: %dx%d",
            (int)(mCropMin.x * mTexWidth), (int)(mCropMin.y * mTexHeight),
            (int)(mCropMax.x * mTexWidth), (int)(mCropMax.y * mTexHeight),
            (int)((mCropMax.x - mCropMin.x) * mTexWidth),
            (int)((mCropMax.y - mCropMin.y) * mTexHeight));

        // Cropped preview
        ImGui::Separator();
        ImGui::Text("Preview:");
        float previewSize = 128.0f;
        float cropAspect = ((mCropMax.x - mCropMin.x) * mTexWidth) /
                           ((mCropMax.y - mCropMin.y) * mTexHeight);
        float previewW = previewSize;
        float previewH = previewSize;
        if (cropAspect > 1.0f)
            previewH = previewSize / cropAspect;
        else
            previewW = previewSize * cropAspect;

        ImGui::Image(mImGuiTexId, ImVec2(previewW, previewH),
                     ImVec2(mCropMin.x, mCropMin.y), ImVec2(mCropMax.x, mCropMax.y));

        // UV output display
        glm::vec2 outScale = CalcUVScale();
        glm::vec2 outOffset = CalcUVOffset();
        ImGui::Text("UV Scale: (%.4f, %.4f)  UV Offset: (%.4f, %.4f)",
            outScale.x, outScale.y, outOffset.x, outOffset.y);

        ImGui::Separator();

        // Buttons
        if (ImGui::Button("Apply"))
        {
            if (mOnApply)
            {
                mOnApply(CalcUVScale(), CalcUVOffset());
            }
            mIsOpen = false;
            ClearTexture();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            mIsOpen = false;
            ClearTexture();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            mCropMin = glm::vec2(0.0f);
            mCropMax = glm::vec2(1.0f);
        }
    }
    ImGui::End();

    // Handle close via X button
    if (!open && mIsOpen)
    {
        mIsOpen = false;
        ClearTexture();
    }
}

void TextureCropEditor::DrawCropCanvas()
{
    float drawW = mTexWidth * mZoom;
    float drawH = mTexHeight * mZoom;

    // Use remaining vertical space for the canvas, leaving room for controls below
    float reservedHeight = 180.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float canvasH = glm::max(avail.y - reservedHeight, 100.0f);

    ImGui::BeginChild("CropCanvas", ImVec2(0, canvasH), true,
        ImGuiWindowFlags_HorizontalScrollbar);

    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    // Draw the texture
    ImGui::Image(mImGuiTexId, ImVec2(drawW, drawH));

    // Overlay an invisible button to capture mouse input and prevent parent window drag
    ImGui::SetCursorScreenPos(cursorPos);
    ImGui::InvisibleButton("##CropInteraction", ImVec2(drawW, drawH));
    bool canvasHovered = ImGui::IsItemHovered();
    bool canvasActive = ImGui::IsItemActive();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Darken area outside crop
    float cropMinPx_x = cursorPos.x + mCropMin.x * drawW;
    float cropMinPx_y = cursorPos.y + mCropMin.y * drawH;
    float cropMaxPx_x = cursorPos.x + mCropMax.x * drawW;
    float cropMaxPx_y = cursorPos.y + mCropMax.y * drawH;

    ImU32 dimColor = IM_COL32(0, 0, 0, 128);

    // Top strip
    drawList->AddRectFilled(
        ImVec2(cursorPos.x, cursorPos.y),
        ImVec2(cursorPos.x + drawW, cropMinPx_y), dimColor);
    // Bottom strip
    drawList->AddRectFilled(
        ImVec2(cursorPos.x, cropMaxPx_y),
        ImVec2(cursorPos.x + drawW, cursorPos.y + drawH), dimColor);
    // Left strip
    drawList->AddRectFilled(
        ImVec2(cursorPos.x, cropMinPx_y),
        ImVec2(cropMinPx_x, cropMaxPx_y), dimColor);
    // Right strip
    drawList->AddRectFilled(
        ImVec2(cropMaxPx_x, cropMinPx_y),
        ImVec2(cursorPos.x + drawW, cropMaxPx_y), dimColor);

    // Draw crop rectangle border
    drawList->AddRect(
        ImVec2(cropMinPx_x, cropMinPx_y),
        ImVec2(cropMaxPx_x, cropMaxPx_y),
        IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);

    // Draw drag handles
    float handleSize = 6.0f;
    ImU32 handleColor = IM_COL32(255, 255, 255, 255);
    ImU32 handleBorderColor = IM_COL32(0, 0, 0, 255);

    float midX = (cropMinPx_x + cropMaxPx_x) * 0.5f;
    float midY = (cropMinPx_y + cropMaxPx_y) * 0.5f;

    // Corner handles
    ImVec2 handles[] = {
        ImVec2(cropMinPx_x, cropMinPx_y), // TL
        ImVec2(cropMaxPx_x, cropMinPx_y), // TR
        ImVec2(cropMinPx_x, cropMaxPx_y), // BL
        ImVec2(cropMaxPx_x, cropMaxPx_y), // BR
        ImVec2(midX, cropMinPx_y),         // T
        ImVec2(midX, cropMaxPx_y),         // B
        ImVec2(cropMinPx_x, midY),         // L
        ImVec2(cropMaxPx_x, midY),         // R
    };

    for (int i = 0; i < 8; ++i)
    {
        ImVec2 hMin(handles[i].x - handleSize, handles[i].y - handleSize);
        ImVec2 hMax(handles[i].x + handleSize, handles[i].y + handleSize);
        drawList->AddRectFilled(hMin, hMax, handleColor);
        drawList->AddRect(hMin, hMax, handleBorderColor);
    }

    // Handle mouse interaction
    HandleCropInteraction(cursorPos, drawW, drawH, canvasHovered, canvasActive);

    ImGui::EndChild();
}

void TextureCropEditor::HandleCropInteraction(ImVec2 canvasPos, float drawW, float drawH, bool canvasHovered, bool canvasActive)
{
    ImVec2 mousePos = ImGui::GetMousePos();
    float handleSize = 8.0f;

    float cropMinPx_x = canvasPos.x + mCropMin.x * drawW;
    float cropMinPx_y = canvasPos.y + mCropMin.y * drawH;
    float cropMaxPx_x = canvasPos.x + mCropMax.x * drawW;
    float cropMaxPx_y = canvasPos.y + mCropMax.y * drawH;
    float midX = (cropMinPx_x + cropMaxPx_x) * 0.5f;
    float midY = (cropMinPx_y + cropMaxPx_y) * 0.5f;

    auto nearHandle = [&](float hx, float hy) -> bool {
        return (mousePos.x >= hx - handleSize && mousePos.x <= hx + handleSize &&
                mousePos.y >= hy - handleSize && mousePos.y <= hy + handleSize);
    };

    auto inCropRect = [&]() -> bool {
        return (mousePos.x >= cropMinPx_x && mousePos.x <= cropMaxPx_x &&
                mousePos.y >= cropMinPx_y && mousePos.y <= cropMaxPx_y);
    };

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && canvasHovered)
    {
        mDragStartMouse = glm::vec2(mousePos.x, mousePos.y);
        mDragStartCropMin = mCropMin;
        mDragStartCropMax = mCropMax;

        if (nearHandle(cropMinPx_x, cropMinPx_y))
            mDragMode = DragMode::ResizeTL;
        else if (nearHandle(cropMaxPx_x, cropMinPx_y))
            mDragMode = DragMode::ResizeTR;
        else if (nearHandle(cropMinPx_x, cropMaxPx_y))
            mDragMode = DragMode::ResizeBL;
        else if (nearHandle(cropMaxPx_x, cropMaxPx_y))
            mDragMode = DragMode::ResizeBR;
        else if (nearHandle(midX, cropMinPx_y))
            mDragMode = DragMode::ResizeT;
        else if (nearHandle(midX, cropMaxPx_y))
            mDragMode = DragMode::ResizeB;
        else if (nearHandle(cropMinPx_x, midY))
            mDragMode = DragMode::ResizeL;
        else if (nearHandle(cropMaxPx_x, midY))
            mDragMode = DragMode::ResizeR;
        else if (inCropRect())
            mDragMode = DragMode::Move;
        else
            mDragMode = DragMode::None;
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && mDragMode != DragMode::None)
    {
        float dx = (mousePos.x - mDragStartMouse.x) / drawW;
        float dy = (mousePos.y - mDragStartMouse.y) / drawH;

        glm::vec2 newMin = mDragStartCropMin;
        glm::vec2 newMax = mDragStartCropMax;

        switch (mDragMode)
        {
            case DragMode::Move:
            {
                glm::vec2 size = mDragStartCropMax - mDragStartCropMin;
                newMin = mDragStartCropMin + glm::vec2(dx, dy);
                // Clamp movement so crop stays in bounds
                newMin = glm::clamp(newMin, glm::vec2(0.0f), glm::vec2(1.0f) - size);
                newMax = newMin + size;
                break;
            }
            case DragMode::ResizeTL:
                newMin.x = mDragStartCropMin.x + dx;
                newMin.y = mDragStartCropMin.y + dy;
                break;
            case DragMode::ResizeTR:
                newMax.x = mDragStartCropMax.x + dx;
                newMin.y = mDragStartCropMin.y + dy;
                break;
            case DragMode::ResizeBL:
                newMin.x = mDragStartCropMin.x + dx;
                newMax.y = mDragStartCropMax.y + dy;
                break;
            case DragMode::ResizeBR:
                newMax.x = mDragStartCropMax.x + dx;
                newMax.y = mDragStartCropMax.y + dy;
                break;
            case DragMode::ResizeT:
                newMin.y = mDragStartCropMin.y + dy;
                break;
            case DragMode::ResizeB:
                newMax.y = mDragStartCropMax.y + dy;
                break;
            case DragMode::ResizeL:
                newMin.x = mDragStartCropMin.x + dx;
                break;
            case DragMode::ResizeR:
                newMax.x = mDragStartCropMax.x + dx;
                break;
            default:
                break;
        }

        // Clamp and enforce minimum size
        float minCropSize = 0.01f;
        newMin = glm::clamp(newMin, glm::vec2(0.0f), glm::vec2(1.0f));
        newMax = glm::clamp(newMax, glm::vec2(0.0f), glm::vec2(1.0f));

        if (newMax.x - newMin.x >= minCropSize && newMax.y - newMin.y >= minCropSize)
        {
            mCropMin = newMin;
            mCropMax = newMax;
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        mDragMode = DragMode::None;
    }

    // Set cursor based on hover state
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (nearHandle(cropMinPx_x, cropMinPx_y) || nearHandle(cropMaxPx_x, cropMaxPx_y))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
        else if (nearHandle(cropMaxPx_x, cropMinPx_y) || nearHandle(cropMinPx_x, cropMaxPx_y))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
        else if (nearHandle(midX, cropMinPx_y) || nearHandle(midX, cropMaxPx_y))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        else if (nearHandle(cropMinPx_x, midY) || nearHandle(cropMaxPx_x, midY))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        else if (inCropRect())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
}

#endif
