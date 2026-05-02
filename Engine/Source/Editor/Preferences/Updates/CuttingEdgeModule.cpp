#if EDITOR

#include "CuttingEdgeModule.h"
#include "EditorWidgets.h"
#include "../JsonSettings.h"

#include "document.h"
#include "imgui.h"

DEFINE_PREFERENCES_MODULE(CuttingEdgeModule, "Cutting Edge", "Updates")

CuttingEdgeModule* CuttingEdgeModule::sInstance = nullptr;

CuttingEdgeModule::CuttingEdgeModule()
{
    sInstance = this;
}

CuttingEdgeModule::~CuttingEdgeModule()
{
    if (sInstance == this)
    {
        sInstance = nullptr;
    }
}

CuttingEdgeModule* CuttingEdgeModule::Get()
{
    return sInstance;
}

void CuttingEdgeModule::Render()
{
    bool changed = false;

    ImGui::Text("Cutting Edge Updates");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("Enable this option to receive beta and pre-release builds when available. "
                       "These builds may contain new features and improvements that are still being tested.");

    ImGui::Spacing();

    if (Polyphase::Checkbox("Opt into cutting edge builds", &mCuttingEdgeEnabled))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("When enabled, the auto-updater will notify you about beta and pre-release versions.");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.1f, 1.0f),
        "Warning: Pre-release builds may be less stable than official releases.");

    if (changed)
    {
        SetDirty(true);
    }
}

void CuttingEdgeModule::SetCuttingEdgeEnabled(bool enabled)
{
    mCuttingEdgeEnabled = enabled;
    SetDirty(true);
}

void CuttingEdgeModule::LoadSettings(const rapidjson::Document& doc)
{
    mCuttingEdgeEnabled = JsonSettings::GetBool(doc, "cuttingEdgeEnabled", false);
}

void CuttingEdgeModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetBool(doc, "cuttingEdgeEnabled", mCuttingEdgeEnabled);
}

#endif
