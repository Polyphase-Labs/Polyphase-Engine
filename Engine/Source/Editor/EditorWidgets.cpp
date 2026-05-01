#if EDITOR

#define IMGUI_DEFINE_MATH_OPERATORS
#include "EditorWidgets.h"

#include "imgui_internal.h"

namespace Polyphase
{
    float gCheckboxSize = 16.0f;

    // Re-implementation of ImGui::Checkbox that decouples the *visual* box size
    // (gCheckboxSize) from the *layout* height (theme frame height). The box is
    // drawn smaller and centered vertically within a row that is the standard
    // frame height, so the checkbox sits flush with adjacent buttons / inputs /
    // combos and the label baseline lines up with their text.
    bool Checkbox(const char* label, bool* v)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *ImGui::GetCurrentContext();
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        const float layout_h = ImGui::GetFrameHeight();
        const float square_sz = ImMin(gCheckboxSize, layout_h);

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(
            pos,
            pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f),
                         layout_h));
        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
        if (pressed)
        {
            *v = !(*v);
            ImGui::MarkItemEdited(id);
        }

        const float box_y_offset = (layout_h - square_sz) * 0.5f;
        const ImRect check_bb(
            ImVec2(pos.x, pos.y + box_y_offset),
            ImVec2(pos.x + square_sz, pos.y + box_y_offset + square_sz));

        ImGui::RenderNavHighlight(total_bb, id);
        ImGui::RenderFrame(
            check_bb.Min, check_bb.Max,
            ImGui::GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive
                                                 : hovered ? ImGuiCol_FrameBgHovered
                                                           : ImGuiCol_FrameBg),
            true, style.FrameRounding);

        ImU32 check_col = ImGui::GetColorU32(ImGuiCol_CheckMark);
        bool mixed_value = (g.LastItemData.InFlags & ImGuiItemFlags_MixedValue) != 0;
        if (mixed_value)
        {
            ImVec2 pad(ImMax(1.0f, IM_FLOOR(square_sz / 3.6f)),
                       ImMax(1.0f, IM_FLOOR(square_sz / 3.6f)));
            window->DrawList->AddRectFilled(check_bb.Min + pad, check_bb.Max - pad, check_col, style.FrameRounding);
        }
        else if (*v)
        {
            const float pad = ImMax(1.0f, IM_FLOOR(square_sz / 6.0f));
            ImGui::RenderCheckMark(window->DrawList, check_bb.Min + ImVec2(pad, pad), check_col, square_sz - pad * 2.0f);
        }

        ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, pos.y + style.FramePadding.y);
        if (g.LogEnabled)
            ImGui::LogRenderedText(&label_pos, mixed_value ? "[~]" : *v ? "[x]" : "[ ]");
        if (label_size.x > 0.0f)
            ImGui::RenderText(label_pos, label);

        return pressed;
    }
}

#endif
