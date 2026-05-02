#if EDITOR

#include "AppSettingsWindow.h"
#include "EditorWidgets.h"

#include "Engine.h"
#include "Log.h"
#include "System/System.h"
#include "../PlayerInputEditor.h"
#include "../PlayerInputDebugger.h"

#include "imgui.h"

#include <cstring>
#include <algorithm>

static AppSettingsWindow sAppSettingsWindow;

AppSettingsWindow* GetAppSettingsWindow()
{
    return &sAppSettingsWindow;
}

AppSettingsWindow::AppSettingsWindow()
{
}

AppSettingsWindow::~AppSettingsWindow()
{
}

void AppSettingsWindow::Open()
{
    mIsOpen = true;
    mDirty = false;

    const EngineConfig* config = GetEngineConfig();

    strncpy(mProjectNameBuffer, config->mProjectName.c_str(), sizeof(mProjectNameBuffer) - 1);
    mProjectNameBuffer[sizeof(mProjectNameBuffer) - 1] = '\0';

    strncpy(mDefaultSceneBuffer, config->mDefaultScene.c_str(), sizeof(mDefaultSceneBuffer) - 1);
    mDefaultSceneBuffer[sizeof(mDefaultSceneBuffer) - 1] = '\0';

    strncpy(mDefaultEditorSceneBuffer, config->mDefaultEditorScene.c_str(), sizeof(mDefaultEditorSceneBuffer) - 1);
    mDefaultEditorSceneBuffer[sizeof(mDefaultEditorSceneBuffer) - 1] = '\0';

    strncpy(mIconPathBuffer, config->mIconPath.c_str(), sizeof(mIconPathBuffer) - 1);
    mIconPathBuffer[sizeof(mIconPathBuffer) - 1] = '\0';
}

void AppSettingsWindow::Close()
{
    mIsOpen = false;
}

void AppSettingsWindow::Draw()
{
    if (!mIsOpen)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(550.0f, 580.0f);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, (io.DisplaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("App Settings", &mIsOpen, windowFlags))
    {
        DrawGeneralSection();
        DrawWindowSection();
        DrawGraphicsSection();
        DrawRuntimeSection();
        DrawInputSection();
        DrawPackagingSection();
        DrawIconSection();

        ImGui::Separator();
        ImGui::BeginDisabled(!mDirty);
        if (ImGui::Button("Apply"))
        {
            WriteEngineConfig();
            const std::string& projectPath = GetEngineState()->mProjectPath;
            if (!projectPath.empty())
            {
                WriteProjectFile(projectPath, mProjectNameBuffer);
            }
            // Push the new name to the OS window so the title bar reflects it
            // immediately — Engine.cpp only sets the title once, during LoadProject.
            SYS_SetWindowTitle(mProjectNameBuffer);
            mDirty = false;
        }
        ImGui::EndDisabled();
        if (mDirty)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "(unsaved changes)");
        }
    }
    ImGui::End();

    if (!mIsOpen)
    {
        Close();
    }
}

void AppSettingsWindow::DrawGeneralSection()
{
    if (!ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    EngineConfig* config = GetMutableEngineConfig();
    bool changed = false;

    if (ImGui::InputText("Project Name", mProjectNameBuffer, sizeof(mProjectNameBuffer)))
    {
        config->mProjectName = mProjectNameBuffer;
        GetEngineState()->mProjectName = mProjectNameBuffer;
        // .octp + Config.ini are written together when Apply is clicked.
        changed = true;
    }

    int gameCode = (int)config->mGameCode;
    if (ImGui::InputInt("Game Code", &gameCode))
    {
        config->mGameCode = (uint32_t)gameCode;
        changed = true;
    }

    int version = (int)config->mVersion;
    if (ImGui::InputInt("Version", &version))
    {
        config->mVersion = (uint32_t)version;
        changed = true;
    }

    if (ImGui::InputText("Default Scene", mDefaultSceneBuffer, sizeof(mDefaultSceneBuffer)))
    {
        config->mDefaultScene = mDefaultSceneBuffer;
        changed = true;
    }

    if (ImGui::InputText("Default Editor Scene", mDefaultEditorSceneBuffer, sizeof(mDefaultEditorSceneBuffer)))
    {
        config->mDefaultEditorScene = mDefaultEditorSceneBuffer;
        changed = true;
    }

    if (changed)
    {
        mDirty = true;
    }
}

void AppSettingsWindow::DrawWindowSection()
{
    if (!ImGui::CollapsingHeader("Window", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    EngineConfig* config = GetMutableEngineConfig();
    bool changed = false;

    int width = config->mWindowWidth;
    if (ImGui::InputInt("Window Width", &width))
    {
        if (width > 0)
        {
            config->mWindowWidth = width;
            changed = true;
        }
    }

    int height = config->mWindowHeight;
    if (ImGui::InputInt("Window Height", &height))
    {
        if (height > 0)
        {
            config->mWindowHeight = height;
            changed = true;
        }
    }

    if (Polyphase::Checkbox("Fullscreen", &config->mFullscreen))
    {
        changed = true;
    }

    if (changed)
    {
        mDirty = true;
    }
}

void AppSettingsWindow::DrawGraphicsSection()
{
    if (!ImGui::CollapsingHeader("Graphics"))
    {
        return;
    }

    EngineConfig* config = GetMutableEngineConfig();
    bool changed = false;

    if (Polyphase::Checkbox("Validate Graphics", &config->mValidateGraphics))
    {
        changed = true;
    }

    if (Polyphase::Checkbox("Linear Color Space", &config->mLinearColorSpace))
    {
        changed = true;
    }

    int colorScale = config->mColorScale;
    if (ImGui::InputInt("Color Scale", &colorScale))
    {
        if (colorScale > 0)
        {
            config->mColorScale = colorScale;
            changed = true;
        }
    }

    int lqMaxTexSize = config->mLqMaxTextureSize;
    if (ImGui::InputInt("LQ Max Texture Size", &lqMaxTexSize))
    {
        if (lqMaxTexSize >= 0)
        {
            config->mLqMaxTextureSize = lqMaxTexSize;
            changed = true;
        }
    }

    if (Polyphase::Checkbox("LQ Enable MipMaps", &config->mLqEnableMipMaps))
    {
        changed = true;
    }

    if (changed)
    {
        mDirty = true;
    }
}

void AppSettingsWindow::DrawRuntimeSection()
{
    if (!ImGui::CollapsingHeader("Runtime"))
    {
        return;
    }

    EngineConfig* config = GetMutableEngineConfig();
    bool changed = false;

    if (Polyphase::Checkbox("Logging", &config->mLogging))
    {
        changed = true;
    }

    if (Polyphase::Checkbox("Log to File", &config->mLogToFile))
    {
        changed = true;
    }

    if (Polyphase::Checkbox("Script Hot Reload", &config->mScriptHotReload))
    {
        changed = true;
    }

    if (Polyphase::Checkbox("Use Asset Registry", &config->mUseAssetRegistry))
    {
        changed = true;
    }

    if (changed)
    {
        mDirty = true;
    }
}

void AppSettingsWindow::DrawInputSection()
{
    if (!ImGui::CollapsingHeader("Input"))
    {
        return;
    }

    if (ImGui::Button("Edit Player Inputs"))
    {
        GetPlayerInputEditor()->Open();
    }
    if (ImGui::Button("Debug Player Inputs"))
    {
        GetPlayerInputDebugger()->Open();
    }
}

void AppSettingsWindow::DrawPackagingSection()
{
    if (!ImGui::CollapsingHeader("Packaging"))
    {
        return;
    }

    EngineConfig* config = GetMutableEngineConfig();
    bool changed = false;

    if (Polyphase::Checkbox("Package for Steam", &config->mPackageForSteam))
    {
        changed = true;
    }

    if (changed)
    {
        mDirty = true;
    }
}

void AppSettingsWindow::DrawIconSection()
{
    if (!ImGui::CollapsingHeader("Application Icon", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    EngineConfig* config = GetMutableEngineConfig();
    const std::string& projectDir = GetEngineState()->mProjectDirectory;

    // Display current icon path
    if (config->mIconPath.empty())
    {
        ImGui::TextDisabled("No icon set (using default)");
    }
    else
    {
        ImGui::Text("Icon: %s", config->mIconPath.c_str());

        std::string fullPath = projectDir + config->mIconPath;
        if (!SYS_DoesFileExist(fullPath.c_str(), false))
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "(file not found)");
        }
    }

    // Browse button
    if (ImGui::Button("Browse..."))
    {
        std::vector<std::string> files = SYS_OpenFileDialog();
        if (!files.empty())
        {
            std::string selectedPath = files[0];

            // Normalize separators
            std::replace(selectedPath.begin(), selectedPath.end(), '\\', '/');

            // Make path relative to project directory if possible
            std::string projDir = projectDir;
            std::replace(projDir.begin(), projDir.end(), '\\', '/');

            if (selectedPath.find(projDir) == 0)
            {
                // File is inside project directory, store relative path
                config->mIconPath = selectedPath.substr(projDir.length());
            }
            else
            {
                // File is outside project, copy it into the project root
                std::string fileName = SYS_GetFileName(selectedPath);
                std::string destPath = projDir + fileName;
                SYS_CopyFile(selectedPath.c_str(), destPath.c_str());
                config->mIconPath = fileName;
            }

            strncpy(mIconPathBuffer, config->mIconPath.c_str(), sizeof(mIconPathBuffer) - 1);
            mIconPathBuffer[sizeof(mIconPathBuffer) - 1] = '\0';

            mDirty = true;

            // Update window icon live
            std::string fullIconPath = projectDir + config->mIconPath;
            SYS_SetWindowIcon(fullIconPath.c_str());
        }
    }

    ImGui::SameLine();

    // Clear button
    bool hasIcon = !config->mIconPath.empty();
    if (!hasIcon)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Clear"))
    {
        config->mIconPath = "";
        mIconPathBuffer[0] = '\0';
        mDirty = true;

        // Reset to default icon
        SYS_SetWindowIcon("");
    }

    if (!hasIcon)
    {
        ImGui::EndDisabled();
    }
}

#endif
