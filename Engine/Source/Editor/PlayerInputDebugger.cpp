#if EDITOR

#include "PlayerInputDebugger.h"
#include "Input/PlayerInputSystem.h"

#include "imgui.h"

#include <cstring>

static PlayerInputDebugger sPlayerInputDebugger;

PlayerInputDebugger* GetPlayerInputDebugger()
{
    return &sPlayerInputDebugger;
}

void PlayerInputDebugger::Open()
{
    mIsOpen = true;
}

bool PlayerInputDebugger::IsOpen() const
{
    return mIsOpen;
}

void PlayerInputDebugger::Draw()
{
    if (!mIsOpen)
        return;

    PlayerInputSystem* sys = PlayerInputSystem::Get();
    if (sys == nullptr)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(750.0f, 400.0f);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, (io.DisplaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Player Input Debugger", &mIsOpen, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Checkbox("Active Only", &mFilterActiveOnly);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputText("Filter", mFilterText, sizeof(mFilterText));
        ImGui::SameLine();
        ImGui::Text("(%d actions)", (int)sys->GetActions().size());
        ImGui::Spacing();

        ImGui::Columns(8, "##debugger_cols", true);
        ImGui::SetColumnWidth(0, 100.0f);  // Category
        ImGui::SetColumnWidth(1, 110.0f);  // Action
        ImGui::SetColumnWidth(2, 85.0f);   // Mode
        ImGui::SetColumnWidth(3, 50.0f);   // Active
        ImGui::SetColumnWidth(4, 55.0f);   // Just Act
        ImGui::SetColumnWidth(5, 55.0f);   // Just Deact
        ImGui::SetColumnWidth(6, 60.0f);   // Value
        ImGui::SetColumnWidth(7, 70.0f);   // Hold/Press

        ImGui::Text("Category"); ImGui::NextColumn();
        ImGui::Text("Action"); ImGui::NextColumn();
        ImGui::Text("Mode"); ImGui::NextColumn();
        ImGui::Text("Active"); ImGui::NextColumn();
        ImGui::Text("Actv'd"); ImGui::NextColumn();
        ImGui::Text("Deact"); ImGui::NextColumn();
        ImGui::Text("Value"); ImGui::NextColumn();
        ImGui::Text("Timer"); ImGui::NextColumn();
        ImGui::Separator();

        static const char* modeShort[] = { "Single", "Hold", "Held", "Multi" };

        for (const auto& action : sys->GetActions())
        {
            // Filter
            if (mFilterActiveOnly && !action.state.isActive)
                continue;

            if (mFilterText[0] != '\0')
            {
                bool match = (strstr(action.category.c_str(), mFilterText) != nullptr ||
                              strstr(action.name.c_str(), mFilterText) != nullptr);
                if (!match) continue;
            }

            // Color coding
            if (action.state.wasJustActivated)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            else if (action.state.isActive)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
            else if (action.state.wasJustDeactivated)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));

            ImGui::Text("%s", action.category.c_str()); ImGui::NextColumn();
            ImGui::Text("%s", action.name.c_str()); ImGui::NextColumn();
            ImGui::Text("%s", modeShort[(int)action.trigger.mode]); ImGui::NextColumn();
            ImGui::Text("%s", action.state.isActive ? "YES" : "---"); ImGui::NextColumn();
            ImGui::Text("%s", action.state.wasJustActivated ? "YES" : "---"); ImGui::NextColumn();
            ImGui::Text("%s", action.state.wasJustDeactivated ? "YES" : "---"); ImGui::NextColumn();
            ImGui::Text("%.2f", action.state.value); ImGui::NextColumn();

            if (action.trigger.mode == TriggerMode::Hold)
                ImGui::Text("%.1fs", action.state.holdTimer);
            else if (action.trigger.mode == TriggerMode::MultiPress)
                ImGui::Text("%d/%d", action.state.pressCount, action.trigger.multiPressCount);
            else
                ImGui::Text("---");
            ImGui::NextColumn();

            ImGui::PopStyleColor();
        }

        ImGui::Columns(1);
    }
    ImGui::End();
}

#endif
