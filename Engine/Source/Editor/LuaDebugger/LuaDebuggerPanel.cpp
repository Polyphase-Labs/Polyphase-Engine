#if EDITOR

#include "LuaDebuggerPanel.h"
#include "LuaDebugger.h"

#include "Hotkeys/EditorHotkeyMap.h"

#include "imgui.h"

#include <cstdio>

static LuaDebuggerPanel sLuaDebuggerPanel;

LuaDebuggerPanel* GetLuaDebuggerPanel()
{
    return &sLuaDebuggerPanel;
}

void LuaDebuggerPanel::Init()
{
}

void LuaDebuggerPanel::Shutdown()
{
}

void LuaDebuggerPanel::DrawContent()
{
    LuaDebugger* dbg = LuaDebugger::Get();
    if (dbg == nullptr)
    {
        ImGui::TextDisabled("Lua debugger not available.");
        return;
    }

    bool paused = dbg->IsPaused();

    // ----- Status header -----
    if (paused)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "PAUSED");
        ImGui::SameLine();
        ImGui::Text("at %s:%d", dbg->GetPauseFile().c_str(), dbg->GetPauseLine());
        if (!dbg->GetPauseMessage().empty())
        {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "Reason: %s", dbg->GetPauseMessage().c_str());
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Running");
    }

    ImGui::Separator();

    // ----- Toolbar: Continue button (also F5 hotkey) -----
    {
        if (!paused) ImGui::BeginDisabled();

        if (ImGui::Button("Continue (F5)"))
        {
            dbg->RequestContinue();
        }

        if (!paused) ImGui::EndDisabled();

        // Hotkey: F5 fires Continue while paused. Gate on paused so we don't
        // fight Git_RefreshStatus (which is also bound to F5) during normal use.
        if (paused && EditorHotkeyMap::Get() != nullptr)
        {
            if (EditorHotkeyMap::Get()->IsActionJustTriggered(EditorAction::Debug_LuaContinue))
            {
                dbg->RequestContinue();
            }
        }
    }

    ImGui::Separator();

    // ----- Tabs: Breakpoints / Call Stack / Locals -----
    if (ImGui::BeginTabBar("##LuaDebuggerTabs"))
    {
        // ---- Breakpoints tab ----
        if (ImGui::BeginTabItem("Breakpoints"))
        {
            auto bps = dbg->GetAllBreakpoints();
            ImGui::Text("%d breakpoint(s)", (int)bps.size());
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear All"))
            {
                dbg->ClearAllBreakpoints();
            }
            ImGui::Separator();

            if (ImGui::BeginTable("##bps", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 30.0f);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < bps.size(); ++i)
                {
                    ImGui::PushID((int)i);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(bps[i].mFile.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", bps[i].mLine);
                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton("X"))
                    {
                        dbg->ClearBreakpoint(bps[i].mFile, bps[i].mLine);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        // ---- Call Stack tab ----
        if (ImGui::BeginTabItem("Call Stack"))
        {
            const auto& stack = dbg->GetCallStack();
            if (stack.empty())
            {
                ImGui::TextDisabled("No call stack (debugger is running).");
            }
            else
            {
                if (mSelectedFrame >= (int)stack.size()) mSelectedFrame = 0;

                for (size_t i = 0; i < stack.size(); ++i)
                {
                    char label[512];
                    const std::string& fn = stack[i].mFuncName.empty() ? std::string("<anonymous>") : stack[i].mFuncName;
                    snprintf(label, sizeof(label), "[%zu] %s  (%s:%d)  [%s]",
                             i,
                             fn.c_str(),
                             stack[i].mSource.c_str(),
                             stack[i].mCurrentLine,
                             stack[i].mWhat.c_str());
                    bool selected = ((int)i == mSelectedFrame);
                    if (ImGui::Selectable(label, selected))
                    {
                        mSelectedFrame = (int)i;
                    }
                }
            }
            ImGui::EndTabItem();
        }

        // ---- Locals + Upvalues tab ----
        if (ImGui::BeginTabItem("Locals"))
        {
            const auto& stack = dbg->GetCallStack();
            if (stack.empty())
            {
                ImGui::TextDisabled("No frame data.");
            }
            else
            {
                if (mSelectedFrame >= (int)stack.size()) mSelectedFrame = 0;
                const auto& f = stack[mSelectedFrame];
                ImGui::Text("Frame [%d]: %s  (%s:%d)  [%s]",
                            mSelectedFrame,
                            f.mFuncName.empty() ? "<anonymous>" : f.mFuncName.c_str(),
                            f.mSource.c_str(),
                            f.mCurrentLine,
                            f.mWhat.c_str());
                ImGui::TextDisabled("Tip: switch frames in the Call Stack tab to inspect callers.");
                ImGui::Separator();

                auto drawVarTable = [](const char* title, const std::vector<LuaDebugger::LocalVar>& vars)
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s (%d)", title, (int)vars.size());
                    if (vars.empty())
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled(" -- none in this frame");
                        return;
                    }

                    if (ImGui::BeginTable(title, 3,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
                    {
                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();

                        for (const auto& v : vars)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(v.mName.c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(v.mTypeStr.c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(v.mValueStr.c_str());
                        }
                        ImGui::EndTable();
                    }
                };

                drawVarTable("Locals",   dbg->GetSnapshotVars(mSelectedFrame, LuaDebugger::VarKind::Local));
                ImGui::Spacing();
                drawVarTable("Upvalues", dbg->GetSnapshotVars(mSelectedFrame, LuaDebugger::VarKind::Upvalue));
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

#endif // EDITOR
