#if EDITOR

#include "TilemapGridModule.h"
#include "../../../JsonSettings.h"
#include "../../../PreferencesManager.h"

#include "document.h"
#include "imgui.h"

DEFINE_PREFERENCES_MODULE(TilemapGridModule, "Tilemap Grid", "Appearance/Viewport")

TilemapGridModule* TilemapGridModule::sInstance = nullptr;

TilemapGridModule::TilemapGridModule()
{
    sInstance = this;
}

TilemapGridModule::~TilemapGridModule()
{
    if (sInstance == this)
    {
        sInstance = nullptr;
    }
}

TilemapGridModule* TilemapGridModule::Get()
{
    return sInstance;
}

void TilemapGridModule::Render()
{
    bool changed = false;

    ImGui::TextWrapped("Colors used by the Tile Paint cell grid overlay (View > Cell Grid).");
    ImGui::Spacing();

    ImGui::Text("Minor Line Color");
    if (ImGui::ColorEdit4("##TilemapGridMinor", &mMinorColor.x, ImGuiColorEditFlags_AlphaBar))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color of regular cell-boundary grid lines.");

    ImGui::Spacing();

    if (ImGui::Checkbox("Highlight World Axes", &mHighlightAxes))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Draw the X=0 and Y=0 grid lines in a separate color so the origin is easy to find.");

    ImGui::BeginDisabled(!mHighlightAxes);
    {
        ImGui::Text("Axis Color");
        if (ImGui::ColorEdit4("##TilemapGridAxis", &mAxisColor.x, ImGuiColorEditFlags_AlphaBar))
        {
            changed = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color of the X=0 and Y=0 highlight lines.");
    }
    ImGui::EndDisabled();

    if (changed)
    {
        SetDirty(true);
    }
}

void TilemapGridModule::LoadSettings(const rapidjson::Document& doc)
{
    mMinorColor    = JsonSettings::GetVec4(doc, "minorColor",    glm::vec4(1.0f, 1.0f, 1.0f, 0.35f));
    mAxisColor     = JsonSettings::GetVec4(doc, "axisColor",     glm::vec4(1.0f, 1.0f, 0.4f, 0.85f));
    mHighlightAxes = JsonSettings::GetBool(doc, "highlightAxes", true);
}

void TilemapGridModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetVec4(doc, "minorColor",    mMinorColor);
    JsonSettings::SetVec4(doc, "axisColor",     mAxisColor);
    JsonSettings::SetBool(doc, "highlightAxes", mHighlightAxes);
}

#endif
