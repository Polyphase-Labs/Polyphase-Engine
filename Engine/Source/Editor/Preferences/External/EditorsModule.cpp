#if EDITOR

#include "EditorsModule.h"
#include "EditorWidgets.h"
#include "../JsonSettings.h"
#include "ScriptEditor/ScriptEditorWindow.h"

#include "System/System.h"
#include "Log.h"

#include "document.h"
#include "imgui.h"

DEFINE_PREFERENCES_MODULE(EditorsModule, "Editors", "External")

EditorsModule::EditorsModule()
{
}

EditorsModule::~EditorsModule()
{
}

void EditorsModule::Render()
{
    bool changed = false;

    // Lua Editor settings
    ImGui::Text("Lua Editor");
    ImGui::Separator();

    if (DrawPathInput("Path##LuaEditor", mLuaEditorPath, "Select Lua Editor Executable"))
    {
        changed = true;
    }

    ImGui::SetNextItemWidth(-1);
    char luaArgsBuffer[512];
    strncpy(luaArgsBuffer, mLuaEditorArgs.c_str(), sizeof(luaArgsBuffer) - 1);
    luaArgsBuffer[sizeof(luaArgsBuffer) - 1] = '\0';
    if (ImGui::InputText("Args##LuaEditor", luaArgsBuffer, sizeof(luaArgsBuffer)))
    {
        mLuaEditorArgs = luaArgsBuffer;
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Command-line arguments. Default: {editor} {file}");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // C++ Editor settings
    ImGui::Text("C++ Editor / IDE");
    ImGui::Separator();

    if (DrawPathInput("Path##CppEditor", mCppEditorPath, "Select C++ Editor Executable"))
    {
        changed = true;
    }

    ImGui::SetNextItemWidth(-1);
    char cppArgsBuffer[512];
    strncpy(cppArgsBuffer, mCppEditorArgs.c_str(), sizeof(cppArgsBuffer) - 1);
    cppArgsBuffer[sizeof(cppArgsBuffer) - 1] = '\0';
    if (ImGui::InputText("Args##CppEditor", cppArgsBuffer, sizeof(cppArgsBuffer)))
    {
        mCppEditorArgs = cppArgsBuffer;
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Command-line arguments. Default: {editor} {project}");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // C# IDE
    ImGui::Text("C# IDE");
    ImGui::Separator();

    const char* csharpIdeNames[] = { "Visual Studio", "VS Code", "Custom" };
    if (ImGui::Combo("IDE##CSharpIde", &mCSharpIde, csharpIdeNames, 3))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("What Tools > CSharp > Open C# Solution launches.\n"
                          "Visual Studio opens Game.sln; VS Code opens the Scripts/CSharp\n"
                          "folder (its C# extension needs the workspace, not the file).");
    }

    if (mCSharpIde == (int)CSharpIde::Custom)
    {
        if (DrawPathInput("Path##CSharpIde", mCSharpIdePath, "Select C# IDE Executable"))
        {
            changed = true;
        }

        ImGui::SetNextItemWidth(-1);
        char csIdeArgsBuffer[512];
        strncpy(csIdeArgsBuffer, mCSharpIdeArgs.c_str(), sizeof(csIdeArgsBuffer) - 1);
        csIdeArgsBuffer[sizeof(csIdeArgsBuffer) - 1] = '\0';
        if (ImGui::InputText("Args##CSharpIde", csIdeArgsBuffer, sizeof(csIdeArgsBuffer)))
        {
            mCSharpIdeArgs = csIdeArgsBuffer;
            changed = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Command-line arguments. Default: {editor} {folder}\n"
                              "Placeholders: {editor}, {solution}, {project}, {folder}");
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Internal Editor
    ImGui::Text("Internal Editor");
    ImGui::Separator();
    if (Polyphase::Checkbox("Use Internal Editor", &mUseInternalEditor))
    {
        changed = true;
    }
    ImGui::TextDisabled("Open Lua scripts in the built-in Script Editor tab instead of an external program.");

    ImGui::Spacing();
    ImGui::Spacing();

    // Placeholder help
    ImGui::TextDisabled("Placeholders: {editor}, {file}, {filedir}, {project}");

    if (changed)
    {
        SetDirty(true);
    }
}

void EditorsModule::LoadSettings(const rapidjson::Document& doc)
{
    mLuaEditorPath = JsonSettings::GetString(doc, "luaEditorPath", "");
    mLuaEditorArgs = JsonSettings::GetString(doc, "luaEditorArgs", "{editor} {file}");
    mCppEditorPath = JsonSettings::GetString(doc, "cppEditorPath", "");
    mCppEditorArgs = JsonSettings::GetString(doc, "cppEditorArgs", "{editor} {project}");
    mUseInternalEditor = JsonSettings::GetBool(doc, "useInternalEditor", false);
    mCSharpIde = JsonSettings::GetInt(doc, "csharpIde", (int)CSharpIde::VisualStudio);
    mCSharpIdePath = JsonSettings::GetString(doc, "csharpIdePath", "");
    mCSharpIdeArgs = JsonSettings::GetString(doc, "csharpIdeArgs", "{editor} {folder}");
}

void EditorsModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetString(doc, "luaEditorPath", mLuaEditorPath);
    JsonSettings::SetString(doc, "luaEditorArgs", mLuaEditorArgs);
    JsonSettings::SetString(doc, "cppEditorPath", mCppEditorPath);
    JsonSettings::SetString(doc, "cppEditorArgs", mCppEditorArgs);
    JsonSettings::SetBool(doc, "useInternalEditor", mUseInternalEditor);
    JsonSettings::SetInt(doc, "csharpIde", mCSharpIde);
    JsonSettings::SetString(doc, "csharpIdePath", mCSharpIdePath);
    JsonSettings::SetString(doc, "csharpIdeArgs", mCSharpIdeArgs);
}

bool EditorsModule::IsLuaEditorConfigured() const
{
    return !mLuaEditorPath.empty();
}

bool EditorsModule::IsCppEditorConfigured() const
{
    return !mCppEditorPath.empty();
}

std::string EditorsModule::BuildLuaOpenCommand(const std::string& filePath) const
{
    std::string args = mLuaEditorArgs.empty() ? "{editor} {file}" : mLuaEditorArgs;

    // Get file directory
    std::string fileDir = filePath;
    size_t lastSlash = fileDir.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        fileDir = fileDir.substr(0, lastSlash);
    }

    std::string cmd = args;
    ReplaceAll(cmd, "{editor}", "\"" + mLuaEditorPath + "\"");
    ReplaceAll(cmd, "{file}", "\"" + filePath + "\"");
    ReplaceAll(cmd, "{filedir}", "\"" + fileDir + "\"");

#if PLATFORM_WINDOWS
    cmd = "start \"\" " + cmd;
#elif PLATFORM_LINUX
    cmd += " &";
#endif

    return cmd;
}

std::string EditorsModule::BuildCppOpenCommand(const std::string& filePath, const std::string& projectPath) const
{
    std::string args = mCppEditorArgs.empty() ? "{editor} {project}" : mCppEditorArgs;

    // Get file directory
    std::string fileDir = filePath;
    size_t lastSlash = fileDir.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        fileDir = fileDir.substr(0, lastSlash);
    }

    std::string cmd = args;
    ReplaceAll(cmd, "{editor}", "\"" + mCppEditorPath + "\"");
    ReplaceAll(cmd, "{file}", "\"" + filePath + "\"");
    ReplaceAll(cmd, "{filedir}", "\"" + fileDir + "\"");
    ReplaceAll(cmd, "{project}", "\"" + projectPath + "\"");

#if PLATFORM_WINDOWS
    cmd = "start \"\" " + cmd;
#elif PLATFORM_LINUX
    cmd += " &";
#endif

    return cmd;
}

void EditorsModule::OpenLuaScript(const std::string& filePath)
{
    if (SYS_DoesFileExist(filePath.c_str(), false)) {
        if (mUseInternalEditor)
        {
            GetScriptEditorWindow()->OpenFile(filePath);
            return;
        }
    }

    if (!IsLuaEditorConfigured())
    {
        LogWarning("No Lua editor configured. Set one in Preferences > External > Editors.");
        return;
    }

    std::string cmd = BuildLuaOpenCommand(filePath);
    LogDebug("Opening Lua script: %s", cmd.c_str());
    // Detached: the old SYS_Exec path leaked stdout/stderr pipe handles into
    // the spawned editor (e.g. VS Code) so the parent blocked on ReadFile until
    // the editor closed. First-launch of Code.exe would freeze Polyphase until
    // the user closed VS Code; already-running instances worked because the
    // helper relay exited fast. SYS_ExecDetached doesn't inherit handles, so
    // there's no rope to keep us tied.
    SYS_ExecDetached(cmd.c_str());
}

void EditorsModule::OpenCppFile(const std::string& filePath, const std::string& vcxprojPath)
{
    if (!IsCppEditorConfigured())
    {
        LogWarning("No C++ editor configured. Set one in Preferences > External > Editors.");
        return;
    }

    std::string cmd = BuildCppOpenCommand(filePath, vcxprojPath);
    LogDebug("Opening C++ file: %s", cmd.c_str());
    SYS_ExecDetached(cmd.c_str());
}

void EditorsModule::OpenCSharpWorkspace(const std::string& csharpDir)
{
    std::string sln = csharpDir + "/Game.sln";
    std::string csproj = csharpDir + "/Game.csproj";
    bool haveSln = SYS_DoesFileExist(sln.c_str(), false);
    bool haveCsproj = SYS_DoesFileExist(csproj.c_str(), false);

    switch ((CSharpIde)mCSharpIde)
    {
        case CSharpIde::VSCode:
        {
            // Folder open — the C# extension needs the workspace to give
            // IntelliSense; opening the .csproj as a file just shows XML.
            static int sHasCode = -1;
            if (sHasCode == -1)
            {
                std::string whereOut;
#if PLATFORM_WINDOWS
                SYS_Exec("where code", &whereOut);
#else
                SYS_Exec("which code", &whereOut);
#endif
                sHasCode = (!whereOut.empty() && whereOut.find("code") != std::string::npos) ? 1 : 0;
            }
            if (sHasCode != 1)
            {
                LogWarning("VS Code CLI ('code') not found on PATH. Install VS Code or switch "
                           "the C# IDE in Preferences > External > Editors.");
                return;
            }
            std::string cmd = "code \"" + csharpDir + "\"";
            SYS_ExecDetached(cmd.c_str());
            LogDebug("Opened %s in VS Code. IntelliSense needs the 'C# Dev Kit' (or 'C#') "
                     "extension — VS Code will suggest it when it sees Game.csproj.",
                csharpDir.c_str());
            return;
        }

        case CSharpIde::Custom:
        {
            if (mCSharpIdePath.empty())
            {
                LogWarning("No custom C# IDE configured. Set one in Preferences > External > Editors.");
                return;
            }
            std::string cmd = mCSharpIdeArgs.empty() ? "{editor} {folder}" : mCSharpIdeArgs;
            ReplaceAll(cmd, "{editor}", "\"" + mCSharpIdePath + "\"");
            ReplaceAll(cmd, "{solution}", "\"" + sln + "\"");
            ReplaceAll(cmd, "{project}", "\"" + csproj + "\"");
            ReplaceAll(cmd, "{folder}", "\"" + csharpDir + "\"");
#if PLATFORM_WINDOWS
            cmd = "start \"\" " + cmd;
#endif
            SYS_ExecDetached(cmd.c_str());
            return;
        }

        case CSharpIde::VisualStudio:
        default:
        {
            std::string target = haveSln ? sln : (haveCsproj ? csproj : "");
            if (target.empty())
            {
                LogWarning("No Game.sln/Game.csproj under Scripts/CSharp/. "
                           "Use Tools > CSharp > Enable C# for This Project first.");
                return;
            }
#if PLATFORM_WINDOWS
            // Launch devenv.exe explicitly (resolved once via vswhere).
            // `start "" file.sln` obeys the FILE ASSOCIATION, which on machines
            // with VS Code installed frequently points at VS Code — exactly what
            // choosing "Visual Studio" here is supposed to override.
            static std::string sDevenvExe;
            static bool sDevenvProbed = false;
            if (!sDevenvProbed)
            {
                sDevenvProbed = true;
                std::string out;
                SYS_Exec("External\\vswhere\\vswhere.exe -latest -property productPath", &out);
                while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
                    out.pop_back();
                if (out.size() > 4 && out.compare(out.size() - 4, 4, ".exe") == 0)
                {
                    sDevenvExe = out;
                }
            }

            if (!sDevenvExe.empty())
            {
                std::string args = "\"" + target + "\"";
                SYS_SpawnDetachedExecutable(sDevenvExe.c_str(), args.c_str());
            }
            else
            {
                LogWarning("Visual Studio not found (vswhere returned nothing) — falling back "
                           "to the OS file association. Pick a different C# IDE in "
                           "Preferences > External > Editors if this opens the wrong program.");
                std::string cmd = "start \"\" \"" + target + "\"";
                SYS_ExecDetached(cmd.c_str());
            }
#else
            std::string cmd = "xdg-open \"" + target + "\"";
            SYS_ExecDetached(cmd.c_str());
#endif
            return;
        }
    }
}

bool EditorsModule::DrawPathInput(const char* label, std::string& path, const char* dialogTitle)
{
    bool changed = false;

    ImGui::PushID(label);

    float buttonWidth = 70.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float inputWidth = ImGui::GetContentRegionAvail().x - buttonWidth - spacing;

    ImGui::SetNextItemWidth(inputWidth);
    char pathBuffer[512];
    strncpy(pathBuffer, path.c_str(), sizeof(pathBuffer) - 1);
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

void EditorsModule::ReplaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
    {
        return;
    }

    size_t startPos = 0;
    while ((startPos = str.find(from, startPos)) != std::string::npos)
    {
        str.replace(startPos, from.length(), to);
        startPos += to.length();
    }
}

#endif
