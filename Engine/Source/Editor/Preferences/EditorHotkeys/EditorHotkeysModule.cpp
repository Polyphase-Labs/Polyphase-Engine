#if EDITOR

#include "EditorHotkeysModule.h"
#include "../JsonSettings.h"
#include "Hotkeys/EditorHotkeyMap.h"
#include "Hotkeys/EditorHotkeysWindow.h"

#include "document.h"
#include "imgui.h"

DEFINE_PREFERENCES_MODULE(EditorHotkeysModule, "Editor Hotkeys", "")

EditorHotkeysModule::EditorHotkeysModule()
{
}

EditorHotkeysModule::~EditorHotkeysModule()
{
}

void EditorHotkeysModule::Render()
{
    ImGui::Text("Editor Hotkey Bindings");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Remap menu shortcuts, viewport pilot movement, debug toggles and "
        "anything else the editor binds. Save and share preset files with "
        "other team members.");
    ImGui::Spacing();

    EditorHotkeyMap* map = EditorHotkeyMap::Get();
    if (map == nullptr)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "EditorHotkeyMap not initialized.");
        return;
    }

    int boundCount = 0;
    for (int32_t i = 0; i < (int32_t)EditorAction::Count; ++i)
    {
        if (map->GetBinding((EditorAction)i).IsValid())
            ++boundCount;
    }
    ImGui::Text("Bound actions: %d / %d", boundCount, (int)EditorAction::Count);

    ImGui::Spacing();
    if (ImGui::Button("Open Editor Hotkeys Window..."))
    {
        GetEditorHotkeysWindow()->Open();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset All to Defaults"))
    {
        map->ResetAllToDefaults();
        SetDirty(true);
    }
}

void EditorHotkeysModule::LoadSettings(const rapidjson::Document& doc)
{
    EditorHotkeyMap* map = EditorHotkeyMap::Get();
    if (map == nullptr)
        return;
    map->DeserializeFromJson(doc);
}

void EditorHotkeysModule::SaveSettings(rapidjson::Document& doc)
{
    EditorHotkeyMap* map = EditorHotkeyMap::Get();
    if (map == nullptr)
        return;
    map->SerializeToJson(doc);
}

#endif
