#if EDITOR

#include "GitInitDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "imgui.h"
#include <cstring>

static GitInitDialog sInstance;

GitInitDialog* GetGitInitDialog()
{
    return &sInstance;
}

void GitInitDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;
    std::memset(mProjectPath, 0, sizeof(mProjectPath));
    std::memset(mInitialBranchName, 0, sizeof(mInitialBranchName));
    std::strncpy(mInitialBranchName, "main", sizeof(mInitialBranchName) - 1);
    mCreateInitialCommit = true;
}

void GitInitDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Initialize Repository");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Initialize Repository", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Project Path");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##InitProjectPath", mProjectPath, sizeof(mProjectPath));

        ImGui::Spacing();
        ImGui::Text("Initial Branch Name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##InitBranchName", mInitialBranchName, sizeof(mInitialBranchName));

        ImGui::Spacing();
        ImGui::Checkbox("Create initial empty commit", &mCreateInitialCommit);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool pathEmpty = (std::strlen(mProjectPath) == 0);
        bool branchEmpty = (std::strlen(mInitialBranchName) == 0);

        if (pathEmpty || branchEmpty)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Initialize", ImVec2(120, 0)))
        {
            GitService* service = GitService::Get();
            if (service->InitRepository(mProjectPath, mInitialBranchName))
            {
                if (mCreateInitialCommit && service->GetCurrentRepo())
                {
                    service->GetCurrentRepo()->Commit("Initial commit", "", false);
                }
            }

            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (pathEmpty || branchEmpty)
        {
            ImGui::EndDisabled();
        }

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
