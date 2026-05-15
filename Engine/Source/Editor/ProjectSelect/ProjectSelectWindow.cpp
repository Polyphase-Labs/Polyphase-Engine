#if EDITOR

#include "ProjectSelectWindow.h"
#include "EditorWidgets.h"
#include "TemplateManager.h"
#include "TemplateData.h"
#include "../Addons/AddonsMenu.h"
#include "../EditorIcons.h"
#include "EditorState.h"
#include "ActionManager.h"
#include "Engine.h"
#include "System/System.h"
#include "Log.h"
#include "Git/GitProcessRunner.h"
#include "Git/GitService.h"
#include "Git/GitRepository.h"
#include "Git/Dialogs/GitRemoteEditDialog.h"

#include "imgui.h"

#include <fstream>
#include <sstream>
#include <thread>
#include <algorithm>

static ProjectSelectWindow sProjectSelectWindow;

ProjectSelectWindow* GetProjectSelectWindow()
{
    return &sProjectSelectWindow;
}

ProjectSelectWindow::ProjectSelectWindow()
{
    memset(mProjectNameBuffer, 0, sizeof(mProjectNameBuffer));
    memset(mProjectPathBuffer, 0, sizeof(mProjectPathBuffer));
    memset(mGitHubUrlBuffer, 0, sizeof(mGitHubUrlBuffer));
    memset(mCloneHttpsUrl, 0, sizeof(mCloneHttpsUrl));
    memset(mCloneSshRepoPath, 0, sizeof(mCloneSshRepoPath));
    memset(mCloneProjectName, 0, sizeof(mCloneProjectName));
    memset(mCloneLocation, 0, sizeof(mCloneLocation));
}

ProjectSelectWindow::~ProjectSelectWindow()
{
}

void ProjectSelectWindow::Open()
{
    mIsOpen = true;
    mSelectedTab = 0;
    mShowAddTemplatePopup = false;
    mAddTemplateError.clear();
    mSelectedTemplateIndex = -1;

    // Set default project path
    std::string defaultPath;
#if PLATFORM_WINDOWS
    const char* userProfile = getenv("USERPROFILE");
    if (userProfile != nullptr)
    {
        defaultPath = std::string(userProfile) + "/Documents/PolyphaseProjects";
    }
#else
    const char* home = getenv("HOME");
    if (home != nullptr)
    {
        defaultPath = std::string(home) + "/PolyphaseProjects";
    }
#endif
    strncpy(mProjectPathBuffer, defaultPath.c_str(), sizeof(mProjectPathBuffer) - 1);
    strncpy(mCloneLocation, defaultPath.c_str(), sizeof(mCloneLocation) - 1);
    mCloneInProgress = false;
    mCloneDone = false;
    mCloneResult.clear();
    mCloneResultLevel = 0;
}

void ProjectSelectWindow::Close()
{
    mIsOpen = false;
}

void ProjectSelectWindow::OpenIfNoProject()
{
    if (GetEngineState()->mProjectPath.empty())
    {
        Open();
    }
}

void ProjectSelectWindow::Draw()
{
    if (!mIsOpen)
    {
        return;
    }

    // Process any pending project removals
    for (const std::string& path : mProjectsToRemove)
    {
        std::vector<std::string>& recentProjects = GetEditorState()->mRecentProjects;
        for (auto it = recentProjects.begin(); it != recentProjects.end(); ++it)
        {
            if (*it == path)
            {
                recentProjects.erase(it);
                break;
            }
        }
    }
    mProjectsToRemove.clear();

    // Center the modal window
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(700.0f, 500.0f);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, (io.DisplaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Polyphase - Project Select", &mIsOpen, windowFlags))
    {
        // Tab bar
        if (ImGui::BeginTabBar("ProjectSelectTabs"))
        {
            if (ImGui::BeginTabItem("Recent Projects"))
            {
                mSelectedTab = 0;
                DrawRecentProjects();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Create New"))
            {
                mSelectedTab = 1;
                DrawCreateProject();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Clone From Git"))
            {
                mSelectedTab = 3;
                DrawCloneFromGit();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Templates"))
            {
                mSelectedTab = 2;
                DrawTemplates();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    // Draw popups
    DrawAddTemplatePopup();
}

void ProjectSelectWindow::DrawRecentProjects()
{
    const std::vector<std::string>& recentProjects = GetEditorState()->mRecentProjects;

    float contentHeight = 380.0f;
    ImGui::BeginChild("RecentProjectsList", ImVec2(0, contentHeight), true);

    if (recentProjects.empty())
    {
        ImGui::TextDisabled("No recent projects.");
    }
    else
    {
        static int selectedIndex = -1;

        for (int i = 0; i < (int)recentProjects.size(); ++i)
        {
            const std::string& path = recentProjects[i];
            std::string projectName = path;
            std::string projectDirectory = path;

            // Extract project name and directory from path
            size_t slashPos = path.find_last_of("/\\");
            if (slashPos != std::string::npos)
            {
                projectName = path.substr(slashPos + 1);
                projectDirectory = path.substr(0, slashPos);
            }
            // Remove .octp extension
            size_t dotPos = projectName.find_last_of('.');
            if (dotPos != std::string::npos)
            {
                projectName = projectName.substr(0, dotPos);
            }

            ImGui::PushID(i);

            bool isSelected = (selectedIndex == i);
            if (ImGui::Selectable(("##Project" + std::to_string(i)).c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, 40)))
            {
                selectedIndex = i;
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    OnOpenProject(path);
                }
            }

            // Draw project info on the selectable
            ImGui::SameLine(10);
            ImGui::BeginGroup();
            ImGui::Text("%s", projectName.c_str());
            ImGui::TextDisabled("%s", path.c_str());
            ImGui::EndGroup();

            // Remove button on the right
			// take background color out to make the button more visible

            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
            //ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            //ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
            if (ImGui::Button(ICON_MATERIAL_SYMBOLS_FOLDER_OPEN_SHARP, ImVec2(0,25) ))
            {
                SYS_ExplorerOpenDirectory(projectDirectory.c_str());
            }
            ImGui::SameLine(ImGui::GetWindowWidth() - 60);
            if (ImGui::Button(ICON_ZONDICONS_TRASH, ImVec2(0,25) ))
            {

                mProjectsToRemove.push_back(path);
                if (selectedIndex == i)
                {
                    selectedIndex = -1;
                }
            }
            //ImGui::PopStyleColor(2);

            ImGui::PopID();
        }
    }

    ImGui::EndChild();

    // Buttons at bottom
    ImGui::Spacing();

    if (ImGui::Button("Browse...", ImVec2(100, 0)))
    {
        OnBrowseProject();
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 120);

    bool hasSelection = false;
    int selectedIndex = -1;
    for (int i = 0; i < (int)recentProjects.size(); ++i)
    {
        // Check if this item is selected (simplified - in real code we'd track this properly)
    }

    if (ImGui::Button("Open Selected", ImVec2(100, 0)))
    {
        // Find selected and open
        // For simplicity, double-click handles opening
    }
}

void ProjectSelectWindow::DrawCreateProject()
{
    ImGui::Spacing();

    ImGui::Text("Project Name:");
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("##ProjectName", mProjectNameBuffer, sizeof(mProjectNameBuffer));

    ImGui::Spacing();

    ImGui::Text("Location:");
    ImGui::SetNextItemWidth(330);
    ImGui::InputText("##ProjectPath", mProjectPathBuffer, sizeof(mProjectPathBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse...##PathBrowse", ImVec2(60, 0)))
    {
        std::string selectedPath = SYS_SelectFolderDialog();
        if (!selectedPath.empty())
        {
            strncpy(mProjectPathBuffer, selectedPath.c_str(), sizeof(mProjectPathBuffer) - 1);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Project Type:");
    ImGui::RadioButton("Lua", &mProjectType, 0);
    ImGui::SameLine();
    ImGui::RadioButton("C++", &mProjectType, 1);

    ImGui::Spacing();

    if (mProjectType == 1)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
            "C++ projects create a standalone copy of the engine.");
    }

    ImGui::Spacing();

    Polyphase::Checkbox("Init Git Repo", &mInitGitRepo);

    ImGui::Spacing();
    ImGui::Spacing();

    // Create button
    float buttonWidth = 150.0f;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);

    bool canCreate = (strlen(mProjectNameBuffer) > 0 && strlen(mProjectPathBuffer) > 0);

    if (!canCreate)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Create Project", ImVec2(buttonWidth, 40)))
    {
        OnCreateNewProject();
    }

    if (!canCreate)
    {
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Or create an addon:");
    ImGui::Spacing();
    DrawAddonsCreateItems_ProjectSelect();
}

// ---------------------------------------------------------------------------
// SSH config parsing (reused from GitRemoteEditDialog pattern)
// ---------------------------------------------------------------------------

struct CloneSshHost
{
    std::string mHost;
    std::string mHostName;
    std::string mUser;
};

static std::vector<CloneSshHost> sCloneSshHosts;
static bool sCloneSshParsed = false;

static void ParseCloneSshConfig()
{
    if (sCloneSshParsed) return;
    sCloneSshParsed = true;
    sCloneSshHosts.clear();

    std::string configPath;
#if PLATFORM_WINDOWS
    char* userProfile = nullptr;
    size_t len = 0;
    if (_dupenv_s(&userProfile, &len, "USERPROFILE") == 0 && userProfile)
    {
        configPath = std::string(userProfile) + "\\.ssh\\config";
        free(userProfile);
    }
#else
    const char* homeDir = getenv("HOME");
    if (homeDir)
        configPath = std::string(homeDir) + "/.ssh/config";
#endif
    if (configPath.empty()) return;

    std::ifstream file(configPath);
    if (!file.is_open()) return;

    CloneSshHost current;
    bool inEntry = false;
    std::string line;
    while (std::getline(file, line))
    {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        std::string trimmed = line.substr(start);
        if (trimmed[0] == '#') continue;

        std::istringstream iss(trimmed);
        std::string key, value;
        iss >> key >> value;
        if (key.empty() || value.empty()) continue;

        std::string keyLower = key;
        std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);

        if (keyLower == "host")
        {
            if (inEntry && !current.mHost.empty() && current.mHostName == "github.com")
                sCloneSshHosts.push_back(current);
            current = CloneSshHost();
            current.mHost = value;
            inEntry = true;
        }
        else if (keyLower == "hostname" && inEntry) current.mHostName = value;
        else if (keyLower == "user" && inEntry) current.mUser = value;
    }
    if (inEntry && !current.mHost.empty() && current.mHostName == "github.com")
        sCloneSshHosts.push_back(current);
}

void ProjectSelectWindow::DrawCloneFromGit()
{
    ParseCloneSshConfig();

    ImGui::Spacing();
    ImGui::Text("Clone a Git repository and create a new project from it.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- URL Mode tabs ---
    ImGui::Text("Repository URL:");
    ImGui::RadioButton("HTTPS", &mCloneUrlMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("SSH", &mCloneUrlMode, 1);
    ImGui::Spacing();

    // Computed clone URL
    std::string cloneUrl;

    if (mCloneUrlMode == 0)
    {
        // HTTPS
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##CloneHttpsUrl", "https://github.com/user/repo.git", mCloneHttpsUrl, sizeof(mCloneHttpsUrl));
        cloneUrl = mCloneHttpsUrl;
    }
    else
    {
        // SSH host dropdown
        if (!sCloneSshHosts.empty())
        {
            ImGui::Text("SSH Host:");
            ImGui::SetNextItemWidth(-1);

            const char* preview = (mCloneSshHostIndex >= 0 && mCloneSshHostIndex < (int)sCloneSshHosts.size())
                ? sCloneSshHosts[mCloneSshHostIndex].mHost.c_str()
                : "Select SSH host...";

            if (ImGui::BeginCombo("##CloneSshHost", preview))
            {
                for (int i = 0; i < (int)sCloneSshHosts.size(); ++i)
                {
                    const auto& entry = sCloneSshHosts[i];
                    std::string label = entry.mHost + " (" + entry.mHostName + ")";
                    if (ImGui::Selectable(label.c_str(), mCloneSshHostIndex == i))
                        mCloneSshHostIndex = i;
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "No GitHub SSH hosts found in ~/.ssh/config");
        }

        ImGui::Spacing();
        ImGui::Text("Repository Path:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##CloneSshPath", "user/repo.git", mCloneSshRepoPath, sizeof(mCloneSshRepoPath));

        // Build SSH URL
        if (mCloneSshHostIndex >= 0 && mCloneSshHostIndex < (int)sCloneSshHosts.size())
        {
            const auto& host = sCloneSshHosts[mCloneSshHostIndex];
            std::string user = host.mUser.empty() ? "git" : host.mUser;
            cloneUrl = user + "@" + host.mHost + ":" + mCloneSshRepoPath;
            ImGui::TextDisabled("URL: %s", cloneUrl.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Project settings ---
    ImGui::Text("Project Name:");
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("##CloneProjectName", mCloneProjectName, sizeof(mCloneProjectName));

    ImGui::Spacing();

    ImGui::Text("Location:");
    ImGui::SetNextItemWidth(330);
    ImGui::InputText("##CloneLocation", mCloneLocation, sizeof(mCloneLocation));
    ImGui::SameLine();
    if (ImGui::Button("Browse...##CloneBrowse", ImVec2(60, 0)))
    {
        std::string selectedPath = SYS_SelectFolderDialog();
        if (!selectedPath.empty())
            strncpy(mCloneLocation, selectedPath.c_str(), sizeof(mCloneLocation) - 1);
    }

    ImGui::Spacing();

    ImGui::Text("Project Type:");
    ImGui::RadioButton("Lua##CloneType", &mCloneProjectType, 0);
    ImGui::SameLine();
    ImGui::RadioButton("C++##CloneType", &mCloneProjectType, 1);

    ImGui::Spacing();

    // Result display
    if (!mCloneResult.empty())
    {
        ImVec4 color;
        if (mCloneResultLevel == 0)
            color = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
        else if (mCloneResultLevel == 1)
            color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
        else
            color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", mCloneResult.c_str());
        ImGui::PopStyleColor();
    }

    if (mCloneInProgress)
    {
        ImGui::TextDisabled("Cloning repository...");
    }

    ImGui::Spacing();

    // --- Clone + Create button ---
    float buttonWidth = 180.0f;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);

    bool canClone = !cloneUrl.empty()
        && strlen(mCloneProjectName) > 0
        && strlen(mCloneLocation) > 0
        && !mCloneInProgress
        && !mCloneDone;

    if (!canClone) ImGui::BeginDisabled();

    if (ImGui::Button("Clone & Create Project", ImVec2(buttonWidth, 40)))
    {
        mCloneInProgress = true;
        mCloneDone = false;
        mCloneResult.clear();
        mCloneResultLevel = 0;

        std::string url = cloneUrl;
        std::string projectName = mCloneProjectName;
        std::string location = mCloneLocation;
        bool isCpp = (mCloneProjectType == 1);
        // For SSH, also capture the HTTPS-equivalent URL for the remote
        // (the clone URL IS the remote URL — git sets origin automatically)

        std::thread cloneThread([this, url, projectName, location, isCpp]()
        {
            std::string destPath = location + "/" + projectName;

            // Clone
            std::string output;
            GitCancelToken token = CreateCancelToken();
            int32_t exitCode = GitProcessRunner::Run(
                ".",
                {"git", "clone", "--progress", url, destPath},
                [&output](const std::string& line) { output += line + "\n"; },
                [&output](const std::string& line) { output += line + "\n"; },
                token
            );

            if (exitCode != 0)
            {
                mCloneResult = "Clone failed:\n" + output;
                mCloneResultLevel = 2;
                mCloneInProgress = false;
                mCloneDone = true;
                return;
            }

            mCloneResult = "Clone successful. Creating project...";
            mCloneResultLevel = 0;
            mCloneInProgress = false;
            mCloneDone = true;

            // The project creation and opening must happen on the main thread.
            // We'll set a flag and let the next Draw() pick it up.
        });
        cloneThread.detach();
    }

    if (!canClone) ImGui::EndDisabled();

    // After clone completes, create the project
    if (mCloneDone && mCloneResultLevel == 0)
    {
        std::string destPath = std::string(mCloneLocation) + "/" + mCloneProjectName;
        bool isCpp = (mCloneProjectType == 1);

        // Create the project structure in the cloned directory
        ActionManager::Get()->CreateNewProject(destPath.c_str(), isCpp);

        if (!GetEngineState()->mProjectPath.empty())
        {
            // Open the git repo in the Git panel
            GitService* gitService = GitService::Get();
            if (gitService)
            {
                gitService->OpenRepository(destPath);
            }

            mCloneResult = "Project created and opened!";
            mCloneResultLevel = 0;
            Close();
        }
        else
        {
            mCloneResult = "Clone succeeded but project creation failed.";
            mCloneResultLevel = 2;
        }
        mCloneDone = false; // Prevent re-entry
    }
}

void ProjectSelectWindow::DrawTemplates()
{
    TemplateManager* tm = TemplateManager::Get();
    if (tm == nullptr)
    {
        ImGui::TextDisabled("Template manager not initialized.");
        return;
    }

    // Header with Add Template button
    if (ImGui::Button("+ Add Template"))
    {
        mShowAddTemplatePopup = true;
        memset(mGitHubUrlBuffer, 0, sizeof(mGitHubUrlBuffer));
        mAddTemplateError.clear();
    }

    ImGui::Separator();
    ImGui::Spacing();

    const std::vector<Template>& templates = tm->GetTemplates();

    if (templates.empty())
    {
        ImGui::TextDisabled("No templates installed.");
        ImGui::TextDisabled("Click '+ Add Template' to install templates from GitHub.");
        return;
    }

    // Template grid
    float cardWidth = 150.0f;
    float cardHeight = 120.0f;
    float spacing = 10.0f;
    int cardsPerRow = (int)((ImGui::GetContentRegionAvail().x + spacing) / (cardWidth + spacing));
    if (cardsPerRow < 1) cardsPerRow = 1;

    ImGui::BeginChild("TemplateGrid", ImVec2(0, 280), true);

    for (int i = 0; i < (int)templates.size(); ++i)
    {
        const Template& tmpl = templates[i];

        if (i > 0 && i % cardsPerRow != 0)
        {
            ImGui::SameLine();
        }

        ImGui::PushID(i);

        bool isSelected = (mSelectedTemplateIndex == i);
        ImVec2 cardPos = ImGui::GetCursorScreenPos();

        // Card background
        ImGui::BeginGroup();
        if (ImGui::Selectable(("##Template" + std::to_string(i)).c_str(), isSelected,
                              ImGuiSelectableFlags_None, ImVec2(cardWidth, cardHeight)))
        {
            mSelectedTemplateIndex = i;
        }

        // Draw template info
        ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 5, cardPos.y + 5));

        // Placeholder for thumbnail
        ImGui::BeginGroup();
        ImVec2 thumbSize(cardWidth - 10, 60);
        ImGui::Dummy(thumbSize);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            ImVec2(cardPos.x + 5, cardPos.y + 5),
            ImVec2(cardPos.x + cardWidth - 5, cardPos.y + 65),
            IM_COL32(60, 60, 80, 255)
        );

        ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 5, cardPos.y + 70));
        ImGui::TextWrapped("%s", tmpl.mMetadata.mName.c_str());
        ImGui::EndGroup();

        ImGui::EndGroup();
        ImGui::PopID();
    }

    ImGui::EndChild();

    // Selected template info
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (mSelectedTemplateIndex >= 0 && mSelectedTemplateIndex < (int)templates.size())
    {
        const Template& selected = templates[mSelectedTemplateIndex];

        ImGui::Text("Selected: %s", selected.mMetadata.mName.c_str());
        if (!selected.mMetadata.mAuthor.empty())
        {
            ImGui::Text("Author: %s", selected.mMetadata.mAuthor.c_str());
        }
        if (!selected.mMetadata.mDescription.empty())
        {
            ImGui::TextWrapped("%s", selected.mMetadata.mDescription.c_str());
        }

        ImGui::Spacing();

        if (ImGui::Button("Remove Template", ImVec2(120, 0)))
        {
            tm->RemoveTemplate(selected.mMetadata.mId);
            mSelectedTemplateIndex = -1;
        }

        ImGui::SameLine();

        if (ImGui::Button("Create from Template", ImVec2(150, 0)))
        {
            OnCreateFromTemplate(selected.mMetadata.mId);
        }
    }
    else
    {
        ImGui::TextDisabled("Select a template to see details.");
    }
}

void ProjectSelectWindow::DrawAddTemplatePopup()
{
    if (!mShowAddTemplatePopup)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 200));

    if (ImGui::Begin("Add Template", &mShowAddTemplatePopup,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("GitHub URL:");
        ImGui::SetNextItemWidth(370);
        ImGui::InputText("##GitHubUrl", mGitHubUrlBuffer, sizeof(mGitHubUrlBuffer));

        ImGui::Spacing();
        ImGui::TextWrapped("Enter a GitHub repository URL containing a project template.");
        ImGui::TextWrapped("The repository should have Assets/, Scripts/, and optionally a package.json.");

        if (!mAddTemplateError.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", mAddTemplateError.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Add", ImVec2(80, 0)))
        {
            OnAddTemplateFromGitHub();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            mShowAddTemplatePopup = false;
        }
    }
    ImGui::End();
}

void ProjectSelectWindow::OnOpenProject(const std::string& path)
{
    ActionManager::Get()->RequestOpenProject(path.c_str());
    Close();
}

void ProjectSelectWindow::OnBrowseProject()
{
    std::string selectedFolder = SYS_SelectFolderDialog();

    if (!selectedFolder.empty())
    {
        ActionManager::Get()->RequestOpenProject(selectedFolder.c_str());
        // Close immediately -- the actual open runs on the next tick from
        // the deferred dispatcher, after this window is gone, so the modal
        // doesn't have to fight the project-select window for the user's
        // attention.
        Close();
    }
}

void ProjectSelectWindow::OnCreateNewProject()
{
    std::string projectPath = std::string(mProjectPathBuffer) + "/" + mProjectNameBuffer;
    bool isCpp = (mProjectType == 1);

    ActionManager::Get()->CreateNewProject(projectPath.c_str(), isCpp);

    // Close if project was created successfully
    if (!GetEngineState()->mProjectPath.empty())
    {
        if (mInitGitRepo)
        {
            GitService* gitService = GitService::Get();
            if (gitService != nullptr)
            {
                if (gitService->InitRepository(projectPath, "main"))
                {
                    GitRepository* repo = gitService->GetCurrentRepo();
                    if (repo != nullptr)
                    {
                        std::vector<std::string> toStage;
                        for (const GitStatusEntry& entry : repo->GetStatus())
                        {
                            toStage.push_back(entry.mPath);
                        }
                        if (!toStage.empty())
                        {
                            repo->StageFiles(toStage);
                            repo->RefreshStatus();
                        }
                        repo->Commit("Initial commit", "", false);
                    }
                }
                else
                {
                    LogError("Failed to init git repo at %s", projectPath.c_str());
                }
            }
        }

        Close();
    }
}

void ProjectSelectWindow::OnCreateFromTemplate(const std::string& templateId)
{
    TemplateManager* tm = TemplateManager::Get();
    if (tm == nullptr)
    {
        return;
    }

    // Use the Create New tab settings for project name and path
    if (strlen(mProjectNameBuffer) == 0)
    {
        // Default to template name
        const Template* tmpl = tm->FindTemplate(templateId);
        if (tmpl != nullptr)
        {
            strncpy(mProjectNameBuffer, tmpl->mMetadata.mName.c_str(), sizeof(mProjectNameBuffer) - 1);
        }
    }

    std::string projectPath = std::string(mProjectPathBuffer) + "/" + mProjectNameBuffer;
    std::string error;

    if (tm->CreateProjectFromTemplate(templateId, projectPath, mProjectNameBuffer, error))
    {
        // Find the .octp file and open it
        std::string octpPath = projectPath + "/" + mProjectNameBuffer + ".octp";
        if (SYS_DoesFileExist(octpPath.c_str(), false))
        {
            ActionManager::Get()->RequestOpenProject(octpPath.c_str());
            Close();
        }
        else
        {
            LogError("Created project but could not find .octp file: %s", octpPath.c_str());
        }
    }
    else
    {
        LogError("Failed to create project from template: %s", error.c_str());
    }
}

void ProjectSelectWindow::OnRemoveRecentProject(const std::string& path)
{
    mProjectsToRemove.push_back(path);
}

void ProjectSelectWindow::OnAddTemplateFromGitHub()
{
    std::string url = mGitHubUrlBuffer;
    if (url.empty())
    {
        mAddTemplateError = "Please enter a GitHub URL.";
        return;
    }

    TemplateManager* tm = TemplateManager::Get();
    if (tm == nullptr)
    {
        mAddTemplateError = "Template manager not initialized.";
        return;
    }

    std::string error;
    if (tm->AddTemplateFromGitHub(url, error))
    {
        mShowAddTemplatePopup = false;
        mAddTemplateError.clear();
    }
    else
    {
        mAddTemplateError = error;
    }
}

#endif
