#if EDITOR

#include "EditorHotkeysWindow.h"
#include "EditorHotkeyMap.h"
#include "EditorAction.h"

#include "Input/Input.h"
#include "Input/InputTypes.h"
#include "Preferences/PreferencesManager.h"
#include "Preferences/PreferencesModule.h"
#include "System/System.h"
#include "Utilities.h"

#include "imgui.h"

#include <algorithm>
#include <string.h>

static EditorHotkeysWindow sEditorHotkeysWindow;

EditorHotkeysWindow* GetEditorHotkeysWindow()
{
    return &sEditorHotkeysWindow;
}

// Categories shown as collapsing headers, in display order. Each category
// must match the EditorActionInfo.mCategory string in EditorAction.cpp.
static const char* sCategoryOrder[] =
{
    "File",
    "Edit",
    "Mode",
    "PIE",
    "Camera",
    "View",
    "Gizmo",
    "Debug",
    "UI",
    "Spawn",
    "Hierarchy",
    "Asset",
    "Inspector",
    "Tool",
    "Paint",
    "Version Control",
};
static const int sCategoryCount = (int)(sizeof(sCategoryOrder) / sizeof(sCategoryOrder[0]));

// Categories that are expanded by default the first time the window opens.
static bool CategoryDefaultOpen(const char* cat)
{
    return (strcmp(cat, "File") == 0)
        || (strcmp(cat, "Edit") == 0)
        || (strcmp(cat, "Camera") == 0)
        || (strcmp(cat, "Gizmo") == 0)
        || (strcmp(cat, "Debug") == 0);
}

static void MarkHotkeysModuleDirty()
{
    PreferencesModule* mod = PreferencesManager::Get()->FindModule("Editor Hotkeys");
    if (mod != nullptr)
    {
        mod->SetDirty(true);
        PreferencesManager::Get()->SaveModule(mod);
    }
}

void EditorHotkeysWindow::Open()
{
    mIsOpen = true;
    RefreshPresetList();
}

bool EditorHotkeysWindow::IsOpen() const
{
    return mIsOpen;
}

bool EditorHotkeysWindow::IsCapturing() const
{
    return mCapturing;
}

void EditorHotkeysWindow::RefreshPresetList()
{
    EditorHotkeyMap* map = EditorHotkeyMap::Get();
    if (map != nullptr)
    {
        mPresetNames = map->GetPresetNames();
    }
}

void EditorHotkeysWindow::StartCapture(EditorAction action)
{
    mCapturing = true;
    mCaptureAction = action;
    mCaptureTimer = 5.0f;
}

bool EditorHotkeysWindow::PollKeyPress(int32_t& outKeyCode) const
{
    for (int32_t key = 0; key < INPUT_MAX_KEYS; ++key)
    {
        if (EditorActionKeyCodeIsModifier(key))
            continue;

        if (INP_IsKeyJustDown(key))
        {
            outKeyCode = key;
            return true;
        }
    }
    return false;
}

void EditorHotkeysWindow::Draw()
{
    if (!mIsOpen)
        return;

    EditorHotkeyMap* map = EditorHotkeyMap::Get();
    if (map == nullptr)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(720.0f, 720.0f);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, (io.DisplaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Editor Hotkeys", &mIsOpen, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::TextWrapped(
            "Remap editor shortcuts and viewport movement keys. PIE safety keys "
            "(Escape, F8, Alt+P, F10, Ctrl+Alt+P) and the X/Y/Z transform-axis "
            "lock keys are fixed and cannot be remapped.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        DrawPresetBar();
        ImGui::Spacing();

        if (mCapturing)
        {
            DrawCaptureOverlay();
        }

        DrawSearchBar();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < sCategoryCount; ++i)
        {
            DrawCategorySection(sCategoryOrder[i]);
        }
    }
    ImGui::End();
}

void EditorHotkeysWindow::DrawPresetBar()
{
    EditorHotkeyMap* map = EditorHotkeyMap::Get();

    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SetNextItemWidth(220.0f);
        const char* preview = (mSelectedPreset >= 0 && mSelectedPreset < (int)mPresetNames.size())
            ? mPresetNames[mSelectedPreset].c_str() : "Select preset...";
        if (ImGui::BeginCombo("##hk_preset_select", preview))
        {
            for (int i = 0; i < (int)mPresetNames.size(); ++i)
            {
                bool selected = (mSelectedPreset == i);
                if (ImGui::Selectable(mPresetNames[i].c_str(), selected))
                {
                    mSelectedPreset = i;
                }
            }
            ImGui::EndCombo();
        }

        bool hasSelection = (mSelectedPreset >= 0 && mSelectedPreset < (int)mPresetNames.size());

        ImGui::SameLine();
        if (!hasSelection) ImGui::BeginDisabled();
        if (ImGui::Button("Load"))
        {
            map->LoadPreset(mPresetNames[mSelectedPreset]);
            MarkHotkeysModuleDirty();
        }

        // The built-in "Default" preset cannot be deleted -- it's auto-regenerated
        // on next launch anyway, but blocking it here avoids the confusing
        // "delete then nothing happens" cycle.
        bool isDefaultSelected = hasSelection &&
            mPresetNames[mSelectedPreset] == EditorHotkeyMap::GetDefaultPresetName();
        ImGui::SameLine();
        if (isDefaultSelected) ImGui::BeginDisabled();
        if (ImGui::Button("Delete"))
        {
            map->DeletePreset(mPresetNames[mSelectedPreset]);
            mSelectedPreset = -1;
            RefreshPresetList();
        }
        if (isDefaultSelected)
        {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("The Default preset is built-in and cannot be deleted.");
        }
        if (!hasSelection) ImGui::EndDisabled();

        ImGui::Spacing();

        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("##hk_new_preset", mNewPresetName, sizeof(mNewPresetName));
        ImGui::SameLine();
        bool validName = (mNewPresetName[0] != '\0');
        if (!validName) ImGui::BeginDisabled();
        if (ImGui::Button("Save Preset"))
        {
            map->SavePreset(mNewPresetName);
            mNewPresetName[0] = '\0';
            RefreshPresetList();
        }
        if (!validName) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !validName)
            ImGui::SetTooltip("Enter a name for the preset.");

        ImGui::Spacing();
        ImGui::TextDisabled("Share presets:");
        ImGui::SetNextItemWidth(380.0f);
        ImGui::InputText("##hk_io_path", mImportExportPath, sizeof(mImportExportPath));
        ImGui::SameLine();
        bool validPath = (mImportExportPath[0] != '\0');
        if (!validPath) ImGui::BeginDisabled();
        if (ImGui::Button("Export to file"))
        {
            map->ExportToFile(mImportExportPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Import from file"))
        {
            if (map->ImportFromFile(mImportExportPath))
            {
                MarkHotkeysModuleDirty();
            }
        }
        if (!validPath) ImGui::EndDisabled();

        if (ImGui::Button("Open Presets Folder"))
        {
            std::string dir = EditorHotkeyMap::GetPresetsDirectory();
#if PLATFORM_WINDOWS
            for (char& c : dir) { if (c == '/') c = '\\'; }
            SYS_Exec(("explorer \"" + dir + "\"").c_str());
#elif PLATFORM_LINUX
            SYS_Exec(("xdg-open \"" + dir + "\" &").c_str());
#endif
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset All to Defaults"))
        {
            map->ResetAllToDefaults();
            MarkHotkeysModuleDirty();
        }
    }
}

void EditorHotkeysWindow::DrawSearchBar()
{
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##hk_search", "Search actions...", mSearchBuffer, sizeof(mSearchBuffer));
}

// Simple case-insensitive substring search that works on every platform.
static bool ActionMatchesSearch(const char* actionName, const char* needle)
{
    if (needle == nullptr || needle[0] == '\0')
        return true;
    if (actionName == nullptr)
        return false;

    size_t nlen = strlen(actionName);
    size_t hlen = strlen(needle);
    if (hlen > nlen)
        return false;

    for (size_t i = 0; i + hlen <= nlen; ++i)
    {
        size_t j = 0;
        for (; j < hlen; ++j)
        {
            char a = actionName[i + j]; if (a >= 'A' && a <= 'Z') a += 32;
            char b = needle[j];         if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
        }
        if (j == hlen)
            return true;
    }
    return false;
}

void EditorHotkeysWindow::DrawCategorySection(const char* category)
{
    // Collect actions in this category that match the search filter.
    std::vector<EditorAction> filteredActions;
    for (int32_t i = 0; i < (int32_t)EditorAction::Count; ++i)
    {
        const EditorActionInfo& info = GetEditorActionInfo((EditorAction)i);
        if (strcmp(info.mCategory, category) != 0)
            continue;
        if (!ActionMatchesSearch(info.mName, mSearchBuffer))
            continue;
        filteredActions.push_back((EditorAction)i);
    }

    if (filteredActions.empty())
        return;

    ImGuiTreeNodeFlags flags = CategoryDefaultOpen(category) ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    if (ImGui::CollapsingHeader(category, flags))
    {
        ImGui::Columns(3, nullptr, false);
        ImGui::SetColumnWidth(0, 240.0f);
        ImGui::SetColumnWidth(1, 220.0f);
        ImGui::SetColumnWidth(2, 220.0f);

        for (EditorAction a : filteredActions)
        {
            DrawActionRow(a);
        }

        ImGui::Columns(1);
    }
}

void EditorHotkeysWindow::DrawActionRow(EditorAction action)
{
    EditorHotkeyMap* map = EditorHotkeyMap::Get();
    const EditorActionInfo& info = GetEditorActionInfo(action);
    KeyBinding binding = map->GetBinding(action);

    bool capturingThis = mCapturing && mCaptureAction == action;

    ImGui::PushID((int)action);

    if (capturingThis)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    ImGui::TextUnformatted(info.mName);
    if (capturingThis)
        ImGui::PopStyleColor();
    if (ImGui::IsItemHovered() && info.mDescription[0] != '\0')
        ImGui::SetTooltip("%s", info.mDescription);
    ImGui::NextColumn();

    if (capturingThis)
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press a key...");
    else
        ImGui::TextUnformatted(EditorHotkeyMap::BindingToDisplayString(binding).c_str());

    // Show conflicts inline.
    std::vector<EditorAction> conflicts = map->FindConflicts(binding, action);
    if (!conflicts.empty() && binding.IsValid())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), " (conflict)");
        if (ImGui::IsItemHovered())
        {
            std::string tip = "Also bound to:";
            for (EditorAction c : conflicts)
            {
                tip += "\n  - ";
                tip += GetEditorActionInfo(c).mName;
            }
            ImGui::SetTooltip("%s", tip.c_str());
        }
    }
    ImGui::NextColumn();

    if (!mCapturing)
    {
        if (ImGui::SmallButton("Remap"))
        {
            StartCapture(action);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
        {
            map->ClearBinding(action);
            MarkHotkeysModuleDirty();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset"))
        {
            map->ResetActionToDefault(action);
            MarkHotkeysModuleDirty();
        }
    }
    else
    {
        ImGui::TextDisabled("...");
    }
    ImGui::NextColumn();

    ImGui::PopID();
}

void EditorHotkeysWindow::DrawCaptureOverlay()
{
    EditorHotkeyMap* map = EditorHotkeyMap::Get();

    mCaptureTimer -= ImGui::GetIO().DeltaTime;

    // Timeout cancels capture so the user is never stuck.
    if (mCaptureTimer <= 0.0f)
    {
        mCapturing = false;
        return;
    }

    int32_t pressedKey = -1;
    if (PollKeyPress(pressedKey))
    {
        KeyBinding b;
        b.mKeyCode = pressedKey;
        b.mCtrl  = INP_IsKeyDown(POLYPHASE_KEY_CONTROL_L) || INP_IsKeyDown(POLYPHASE_KEY_CONTROL_R);
        b.mShift = INP_IsKeyDown(POLYPHASE_KEY_SHIFT_L)   || INP_IsKeyDown(POLYPHASE_KEY_SHIFT_R);
        b.mAlt   = INP_IsKeyDown(POLYPHASE_KEY_ALT_L)     || INP_IsKeyDown(POLYPHASE_KEY_ALT_R);
        b.mRequireSpace = INP_IsKeyDown(POLYPHASE_KEY_SPACE);

        map->SetBinding(mCaptureAction, b);
        MarkHotkeysModuleDirty();

        mCapturing = false;
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    ImGui::Text("Listening for key... (%.0fs)  Hold Ctrl/Shift/Alt/Space for modifiers.", mCaptureTimer);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

#endif
