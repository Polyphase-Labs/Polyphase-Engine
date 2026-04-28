#if EDITOR

#include "GitRemoteEditDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "Log.h"
#include "imgui.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

#if PLATFORM_WINDOWS
#include <Windows.h>
#endif

static GitRemoteEditDialog sInstance;

GitRemoteEditDialog* GetGitRemoteEditDialog()
{
    return &sInstance;
}

void GitRemoteEditDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;

    if (!mSshConfigParsed)
    {
        ParseSshConfig();
        mSshConfigParsed = true;
    }
}

void GitRemoteEditDialog::SetMode(Mode mode)
{
    mMode = mode;
    mOriginalName.clear();
    std::memset(mRemoteName, 0, sizeof(mRemoteName));
    std::memset(mRemoteUrl, 0, sizeof(mRemoteUrl));
    std::memset(mSshRepoPath, 0, sizeof(mSshRepoPath));
    mSelectedSshHost = -1;
    Open();
}

void GitRemoteEditDialog::SetMode(Mode mode, const std::string& name, const std::string& url)
{
    mMode = mode;
    mOriginalName = name;

    std::memset(mRemoteName, 0, sizeof(mRemoteName));
    std::memset(mRemoteUrl, 0, sizeof(mRemoteUrl));
    std::memset(mSshRepoPath, 0, sizeof(mSshRepoPath));
    mSelectedSshHost = -1;

    std::strncpy(mRemoteName, name.c_str(), sizeof(mRemoteName) - 1);
    std::strncpy(mRemoteUrl, url.c_str(), sizeof(mRemoteUrl) - 1);

    // Try to detect if the URL matches an SSH host alias: git@<host>:<path>
    std::string urlStr = url;
    size_t atPos = urlStr.find('@');
    size_t colonPos = urlStr.find(':');
    if (atPos != std::string::npos && colonPos != std::string::npos && colonPos > atPos)
    {
        std::string hostPart = urlStr.substr(atPos + 1, colonPos - atPos - 1);
        std::string pathPart = urlStr.substr(colonPos + 1);

        for (int32_t i = 0; i < (int32_t)mSshHosts.size(); ++i)
        {
            if (mSshHosts[i].mHost == hostPart)
            {
                mSelectedSshHost = i;
                std::strncpy(mSshRepoPath, pathPart.c_str(), sizeof(mSshRepoPath) - 1);
                break;
            }
        }
    }

    Open();
}

void GitRemoteEditDialog::ParseSshConfig()
{
    mSshHosts.clear();

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
    const char* home = getenv("HOME");
    if (home)
    {
        configPath = std::string(home) + "/.ssh/config";
    }
#endif

    if (configPath.empty())
        return;

    std::ifstream file(configPath);
    if (!file.is_open())
    {
        LogDebug("GitRemoteEditDialog: no SSH config at %s", configPath.c_str());
        return;
    }

    SshHostEntry current;
    bool inEntry = false;

    std::string line;
    while (std::getline(file, line))
    {
        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
            continue;
        std::string trimmed = line.substr(start);

        // Skip comments
        if (trimmed[0] == '#')
            continue;

        // Parse key-value
        std::istringstream iss(trimmed);
        std::string key, value;
        iss >> key >> value;

        if (key.empty() || value.empty())
            continue;

        // Case-insensitive key comparison
        std::string keyLower = key;
        std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);

        if (keyLower == "host")
        {
            // Save previous entry if it points to github.com
            if (inEntry && !current.mHost.empty() && current.mHostName == "github.com")
            {
                mSshHosts.push_back(current);
            }

            current = SshHostEntry();
            current.mHost = value;
            inEntry = true;
        }
        else if (keyLower == "hostname" && inEntry)
        {
            current.mHostName = value;
        }
        else if (keyLower == "user" && inEntry)
        {
            current.mUser = value;
        }
    }

    // Save last entry
    if (inEntry && !current.mHost.empty() && current.mHostName == "github.com")
    {
        mSshHosts.push_back(current);
    }

    LogDebug("GitRemoteEditDialog: parsed %d SSH host entries", (int)mSshHosts.size());
}

void GitRemoteEditDialog::RebuildUrlFromSshHost()
{
    if (mSelectedSshHost < 0 || mSelectedSshHost >= (int32_t)mSshHosts.size())
        return;

    const SshHostEntry& host = mSshHosts[mSelectedSshHost];
    std::string user = host.mUser.empty() ? "git" : host.mUser;
    std::string url = user + "@" + host.mHost + ":" + mSshRepoPath;

    std::memset(mRemoteUrl, 0, sizeof(mRemoteUrl));
    std::strncpy(mRemoteUrl, url.c_str(), sizeof(mRemoteUrl) - 1);
}

void GitRemoteEditDialog::Draw()
{
    if (!mIsOpen) return;

    const char* title = (mMode == Mode::Add) ? "Add Remote" : "Edit Remote";

    if (mJustOpened)
    {
        ImGui::OpenPopup(title);
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(550, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(title, &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // Remote name
        ImGui::Text("Remote Name");
        ImGui::SetNextItemWidth(-1);
        if (mMode == Mode::Edit)
        {
            ImGui::BeginDisabled();
            ImGui::InputText("##RemoteName", mRemoteName, sizeof(mRemoteName));
            ImGui::EndDisabled();
        }
        else
        {
            ImGui::InputText("##RemoteName", mRemoteName, sizeof(mRemoteName));
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // SSH host dropdown
        if (!mSshHosts.empty())
        {
            ImGui::Text("SSH Host");
            ImGui::SetNextItemWidth(-1);

            // Build preview label
            const char* preview = "Custom / HTTPS";
            if (mSelectedSshHost >= 0 && mSelectedSshHost < (int32_t)mSshHosts.size())
            {
                preview = mSshHosts[mSelectedSshHost].mHost.c_str();
            }

            if (ImGui::BeginCombo("##SshHost", preview))
            {
                // Custom option
                if (ImGui::Selectable("Custom / HTTPS", mSelectedSshHost == -1))
                {
                    mSelectedSshHost = -1;
                }

                ImGui::Separator();

                for (int32_t i = 0; i < (int32_t)mSshHosts.size(); ++i)
                {
                    const SshHostEntry& entry = mSshHosts[i];

                    // Build display label: "github-polyphase (github.com)"
                    std::string label = entry.mHost;
                    if (!entry.mHostName.empty() && entry.mHostName != entry.mHost)
                    {
                        label += " (" + entry.mHostName + ")";
                    }

                    bool selected = (mSelectedSshHost == i);
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        mSelectedSshHost = i;
                        RebuildUrlFromSshHost();
                    }
                }

                ImGui::EndCombo();
            }

            // If SSH host is selected, show repo path input
            if (mSelectedSshHost >= 0 && mSelectedSshHost < (int32_t)mSshHosts.size())
            {
                ImGui::Spacing();

                const SshHostEntry& host = mSshHosts[mSelectedSshHost];
                std::string user = host.mUser.empty() ? "git" : host.mUser;
                ImGui::TextDisabled("URL: %s@%s:<repo path>", user.c_str(), host.mHost.c_str());

                ImGui::Text("Repository Path");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##SshRepoPath", mSshRepoPath, sizeof(mSshRepoPath)))
                {
                    RebuildUrlFromSshHost();
                }
                ImGui::TextDisabled("e.g. polyphase-engine/my-project.git");

                ImGui::Spacing();

                // Show the resulting URL read-only
                ImGui::Text("Resulting URL:");
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", mRemoteUrl);
            }
            else
            {
                // Custom URL input
                ImGui::Spacing();
                ImGui::Text("Remote URL");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##RemoteUrl", mRemoteUrl, sizeof(mRemoteUrl));
            }
        }
        else
        {
            // No SSH config found, just show URL input
            ImGui::Text("Remote URL");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##RemoteUrl", mRemoteUrl, sizeof(mRemoteUrl));
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool nameEmpty = (std::strlen(mRemoteName) == 0);
        bool urlEmpty = (std::strlen(mRemoteUrl) == 0);

        if (nameEmpty || urlEmpty) ImGui::BeginDisabled();

        if (ImGui::Button("Save", ImVec2(120, 0)))
        {
            GitService* service = GitService::Get();
            if (service && service->IsRepositoryOpen())
            {
                GitRepository* repo = service->GetCurrentRepo();

                if (mMode == Mode::Add)
                {
                    repo->AddRemote(mRemoteName, mRemoteUrl);
                }
                else
                {
                    repo->EditRemote(mOriginalName, mRemoteUrl);
                }
            }

            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (nameEmpty || urlEmpty) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

#endif
