#if EDITOR

#include "CliModule.h"

#include "../JsonSettings.h"
#include "CliTerminal/ITerminalProcess.h"
#include "CliTerminal/ShellPresets.h"

#include "Engine.h"
#include "System/System.h"
#include "Log.h"

#include "document.h"
#include "imgui.h"

#include <cstring>

DEFINE_PREFERENCES_MODULE(CliModule, "CLI", "External")

CliModule::CliModule() = default;
CliModule::~CliModule() = default;

namespace
{
    /** Parse "KEY=VALUE\nKEY=VALUE" text into a pair list. Lines without '=' are skipped. */
    std::vector<std::pair<std::string, std::string>> ParseEnvText(const std::string& text)
    {
        std::vector<std::pair<std::string, std::string>> out;
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t lineEnd = text.find('\n', pos);
            if (lineEnd == std::string::npos)
            {
                lineEnd = text.size();
            }
            std::string line = text.substr(pos, lineEnd - pos);
            // Trim trailing CR (Windows line endings).
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            size_t eq = line.find('=');
            if (eq != std::string::npos && eq > 0)
            {
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                out.emplace_back(std::move(key), std::move(val));
            }
            pos = lineEnd + 1;
        }
        return out;
    }
}

void CliModule::Render()
{
    bool changed = false;

    if (ImGui::Checkbox("Enable CLI Terminal", &mEnabled))
    {
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Text("Shell");
    ImGui::Separator();

    // Shell preset combo
    {
        const int presetCount = static_cast<int>(ShellPreset::Count);
        const char* current = ShellPresetDisplayName(static_cast<ShellPreset>(mShellPresetIdx));
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Shell Preset", current))
        {
            for (int i = 0; i < presetCount; ++i)
            {
                bool selected = (i == mShellPresetIdx);
                if (ImGui::Selectable(ShellPresetDisplayName(static_cast<ShellPreset>(i)), selected))
                {
                    mShellPresetIdx = i;
                    changed = true;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    if (static_cast<ShellPreset>(mShellPresetIdx) == ShellPreset::Custom)
    {
        if (DrawPathInput("Executable##CliExe", mCustomExe))
        {
            changed = true;
        }

        char argsBuf[512];
        std::strncpy(argsBuf, mCustomArgs.c_str(), sizeof(argsBuf) - 1);
        argsBuf[sizeof(argsBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("Arguments##CliArgs", argsBuf, sizeof(argsBuf)))
        {
            mCustomArgs = argsBuf;
            changed = true;
        }
    }

    // Resolved preview
    {
        std::vector<std::string> parsedArgs = SplitCommandLine(mCustomArgs);
        ResolvedShell rs = ResolveShellPreset(static_cast<ShellPreset>(mShellPresetIdx), mCustomExe, parsedArgs);
        if (rs.mResolved)
        {
            std::string preview = rs.mExecutable;
            for (const std::string& a : rs.mArgs)
            {
                preview.push_back(' ');
                preview.append(a);
            }
            ImGui::TextDisabled("Resolved: %s", preview.c_str());
        }
        else
        {
            ImGui::TextDisabled("Resolved: <error: %s>", rs.mError.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Text("Working Directory");
    ImGui::Separator();

    {
        const char* modes[] = { "Project Root", "Engine Root", "Custom" };
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::Combo("Mode##CliWd", &mWorkingDirMode, modes, IM_ARRAYSIZE(modes)))
        {
            changed = true;
        }
    }
    if (mWorkingDirMode == 2)
    {
        if (DrawPathInput("Path##CliWd", mCustomWorkingDir))
        {
            changed = true;
        }
    }

    ImGui::Spacing();
    ImGui::Text("Environment Variables");
    ImGui::Separator();

    {
        char envBuf[2048];
        std::strncpy(envBuf, mEnvVars.c_str(), sizeof(envBuf) - 1);
        envBuf[sizeof(envBuf) - 1] = '\0';
        if (ImGui::InputTextMultiline("##CliEnv", envBuf, sizeof(envBuf),
                                      ImVec2(-1, ImGui::GetTextLineHeight() * 5)))
        {
            mEnvVars = envBuf;
            changed = true;
        }
        ImGui::TextDisabled("Format: KEY=VALUE, one per line. These add to the inherited environment.");
    }

    ImGui::Spacing();
    ImGui::Text("Behavior");
    ImGui::Separator();

    if (ImGui::Checkbox("Launch on panel open", &mLaunchOnPanelOpen))
    {
        changed = true;
    }
    if (ImGui::Checkbox("Reuse session across panel open/close", &mReuseSession))
    {
        changed = true;
    }
    if (ImGui::Checkbox("Close process when panel closes", &mCloseProcessOnPanelClose))
    {
        changed = true;
    }
    if (ImGui::Checkbox("Use pseudo-console (ConPTY) for interactive shells", &mUseConPty))
    {
        changed = true;
    }
    ImGui::TextDisabled("Required for interactive tools like 'claude', 'python', 'node'.\nDisable for raw pipe-based logging only. Windows 10 1809+.");
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::DragInt("Graceful shutdown timeout (ms)", &mGracefulShutdownMs, 50.0f, 100, 30000))
    {
        changed = true;
    }

    if (changed)
    {
        SetDirty(true);
    }
}

void CliModule::LoadSettings(const rapidjson::Document& doc)
{
    mEnabled                 = JsonSettings::GetBool(doc, "enabled", true);
    mShellPresetIdx          = JsonSettings::GetInt(doc, "shellPreset", 0);
    mCustomExe               = JsonSettings::GetString(doc, "customExe", "");
    mCustomArgs              = JsonSettings::GetString(doc, "customArgs", "");
    mWorkingDirMode          = JsonSettings::GetInt(doc, "workingDirMode", 0);
    mCustomWorkingDir        = JsonSettings::GetString(doc, "customWorkingDir", "");
    mEnvVars                 = JsonSettings::GetString(doc, "envVars", "");
    mLaunchOnPanelOpen       = JsonSettings::GetBool(doc, "launchOnPanelOpen", false);
    mReuseSession            = JsonSettings::GetBool(doc, "reuseSession", true);
    mCloseProcessOnPanelClose= JsonSettings::GetBool(doc, "closeProcessOnPanelClose", true);
    mUseConPty               = JsonSettings::GetBool(doc, "useConPty", true);
    mGracefulShutdownMs      = JsonSettings::GetInt(doc, "gracefulShutdownMs", 2000);
    mLastCommand             = JsonSettings::GetString(doc, "lastCommand", "");

    if (mShellPresetIdx < 0 || mShellPresetIdx >= static_cast<int>(ShellPreset::Count))
    {
        mShellPresetIdx = 0;
    }
    if (mWorkingDirMode < 0 || mWorkingDirMode > 2)
    {
        mWorkingDirMode = 0;
    }
}

void CliModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetBool  (doc, "enabled",                 mEnabled);
    JsonSettings::SetInt   (doc, "shellPreset",             mShellPresetIdx);
    JsonSettings::SetString(doc, "customExe",               mCustomExe);
    JsonSettings::SetString(doc, "customArgs",              mCustomArgs);
    JsonSettings::SetInt   (doc, "workingDirMode",          mWorkingDirMode);
    JsonSettings::SetString(doc, "customWorkingDir",        mCustomWorkingDir);
    JsonSettings::SetString(doc, "envVars",                 mEnvVars);
    JsonSettings::SetBool  (doc, "launchOnPanelOpen",       mLaunchOnPanelOpen);
    JsonSettings::SetBool  (doc, "reuseSession",            mReuseSession);
    JsonSettings::SetBool  (doc, "closeProcessOnPanelClose",mCloseProcessOnPanelClose);
    JsonSettings::SetBool  (doc, "useConPty",               mUseConPty);
    JsonSettings::SetInt   (doc, "gracefulShutdownMs",      mGracefulShutdownMs);
    JsonSettings::SetString(doc, "lastCommand",             mLastCommand);
}

TerminalLaunchConfig CliModule::BuildLaunchConfig() const
{
    TerminalLaunchConfig cfg;

    std::vector<std::string> parsedArgs = SplitCommandLine(mCustomArgs);
    ResolvedShell rs = ResolveShellPreset(static_cast<ShellPreset>(mShellPresetIdx), mCustomExe, parsedArgs);
    if (rs.mResolved)
    {
        cfg.mExecutable = rs.mExecutable;
        cfg.mArgs = rs.mArgs;
        for (auto& kv : rs.mExtraEnv)
        {
            cfg.mEnv.push_back(kv);
        }
    }

    // Working directory.
    switch (mWorkingDirMode)
    {
        case 0:
        {
            EngineState* es = GetEngineState();
            if (es != nullptr && !es->mProjectDirectory.empty())
            {
                cfg.mWorkingDir = es->mProjectDirectory;
            }
            break;
        }
        case 1:
        {
            // Engine root: fall back to current working directory.
            cfg.mWorkingDir = SYS_GetCurrentDirectoryPath();
            break;
        }
        case 2:
        {
            cfg.mWorkingDir = mCustomWorkingDir;
            break;
        }
        default: break;
    }

    if (cfg.mWorkingDir.empty())
    {
        cfg.mWorkingDir = SYS_GetCurrentDirectoryPath();
    }

    // User-defined env vars (parsed from multi-line text), appended after preset extras.
    auto userEnv = ParseEnvText(mEnvVars);
    for (auto& kv : userEnv)
    {
        cfg.mEnv.push_back(std::move(kv));
    }

    cfg.mGracefulShutdownMs = mGracefulShutdownMs;

    return cfg;
}

bool CliModule::DrawPathInput(const char* label, std::string& path)
{
    bool changed = false;

    ImGui::PushID(label);

    float buttonWidth = 70.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float inputWidth = ImGui::GetContentRegionAvail().x - buttonWidth - spacing;

    ImGui::SetNextItemWidth(inputWidth);
    char pathBuffer[512];
    std::strncpy(pathBuffer, path.c_str(), sizeof(pathBuffer) - 1);
    pathBuffer[sizeof(pathBuffer) - 1] = '\0';
    if (ImGui::InputText("##path", pathBuffer, sizeof(pathBuffer)))
    {
        path = pathBuffer;
        changed = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Browse...", ImVec2(buttonWidth, 0)))
    {
        std::vector<std::string> files = SYS_OpenFileDialog();
        if (!files.empty() && !files[0].empty())
        {
            path = files[0];
            changed = true;
        }
    }

    ImGui::PopID();

    return changed;
}

#endif
