#pragma once

#if EDITOR

#include <string>
#include <vector>

/**
 * @brief Window for selecting, creating, or opening projects.
 *
 * Displays on editor startup when no project is loaded. Provides:
 * - Recent projects list
 * - Create new project (Lua or C++)
 * - Create from template
 * - Template management
 */
class ProjectSelectWindow
{
public:
    ProjectSelectWindow();
    ~ProjectSelectWindow();

    void Open();
    void Close();
    void Draw();
    bool IsOpen() const { return mIsOpen; }

    /** @brief Open the window if no project is currently loaded */
    void OpenIfNoProject();

private:
    void DrawRecentProjects();
    void DrawCreateProject();
    void DrawCloneFromGit();
    void DrawTemplates();
    void DrawAddTemplatePopup();

    void OnOpenProject(const std::string& path);
    void OnBrowseProject();
    void OnCreateNewProject();
    void OnCreateFromTemplate(const std::string& templateId);
    void OnRemoveRecentProject(const std::string& path);
    void OnAddTemplateFromGitHub();

    bool mIsOpen = false;
    int mSelectedTab = 0;

    // Add Template popup state
    bool mShowAddTemplatePopup = false;
    char mGitHubUrlBuffer[512] = {};
    std::string mAddTemplateError;

    // Create New Project state
    char mProjectNameBuffer[256] = {};
    char mProjectPathBuffer[512] = {};
    int mProjectType = 0;  // 0 = Lua, 1 = C++
    bool mInitGitRepo = false;
    int mSelectedTemplateIndex = -1;

    // Clone From Git state
    int mCloneUrlMode = 0;  // 0 = HTTPS, 1 = SSH
    char mCloneHttpsUrl[512] = {};
    char mCloneSshRepoPath[256] = {};
    char mCloneProjectName[256] = {};
    char mCloneLocation[512] = {};
    int mCloneProjectType = 0;  // 0 = Lua, 1 = C++
    int mCloneSshHostIndex = -1;
    bool mCloneInProgress = false;
    bool mCloneDone = false;
    std::string mCloneResult;
    int mCloneResultLevel = 0;

    // Recent projects removal tracking
    std::vector<std::string> mProjectsToRemove;
};

ProjectSelectWindow* GetProjectSelectWindow();

#endif
