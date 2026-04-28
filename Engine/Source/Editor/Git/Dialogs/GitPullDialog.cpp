#if EDITOR

#include "GitPullDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "../GitOperationQueue.h"
#include "../GitOperationRequest.h"
#include "imgui.h"
#include <cstring>

static GitPullDialog sInstance;

GitPullDialog* GetGitPullDialog()
{
    return &sInstance;
}

void GitPullDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;
    mSelectedRemote = 0;
    mSelectedBranch = 0;
    mSelectedStrategy = 0;

    mRemoteNames.clear();
    mBranchNames.clear();

    GitService* service = GitService::Get();
    if (service && service->IsRepositoryOpen())
    {
        GitRepository* repo = service->GetCurrentRepo();

        std::vector<GitRemoteInfo> remotes = repo->GetRemotes();
        for (const auto& remote : remotes)
        {
            mRemoteNames.push_back(remote.mName);
        }

        std::vector<GitBranchInfo> branches = repo->GetBranches();
        std::string currentBranch = repo->GetCurrentBranch();

        for (const auto& branch : branches)
        {
            if (branch.mIsLocal)
            {
                mBranchNames.push_back(branch.mName);
            }
        }

        // Select current branch by default
        for (int32_t i = 0; i < (int32_t)mBranchNames.size(); ++i)
        {
            if (mBranchNames[i] == currentBranch)
            {
                mSelectedBranch = i;
                break;
            }
        }
    }
}

void GitPullDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Pull");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Pull", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        bool hasRemotes = !mRemoteNames.empty();
        bool hasBranches = !mBranchNames.empty();

        // Remote selector
        ImGui::Text("Remote");
        ImGui::SetNextItemWidth(-1);
        if (hasRemotes)
        {
            if (ImGui::BeginCombo("##PullRemote", mRemoteNames[mSelectedRemote].c_str()))
            {
                for (int32_t i = 0; i < (int32_t)mRemoteNames.size(); ++i)
                {
                    bool selected = (i == mSelectedRemote);
                    if (ImGui::Selectable(mRemoteNames[i].c_str(), selected))
                    {
                        mSelectedRemote = i;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::TextDisabled("No remotes configured");
        }

        ImGui::Spacing();

        // Branch selector
        ImGui::Text("Branch");
        ImGui::SetNextItemWidth(-1);
        if (hasBranches)
        {
            if (ImGui::BeginCombo("##PullBranch", mBranchNames[mSelectedBranch].c_str()))
            {
                for (int32_t i = 0; i < (int32_t)mBranchNames.size(); ++i)
                {
                    bool selected = (i == mSelectedBranch);
                    if (ImGui::Selectable(mBranchNames[i].c_str(), selected))
                    {
                        mSelectedBranch = i;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::TextDisabled("No local branches");
        }

        ImGui::Spacing();

        // Strategy combo
        ImGui::Text("Strategy");
        ImGui::SetNextItemWidth(-1);
        const char* strategies[] = { "Merge", "Fast-Forward Only" };
        ImGui::Combo("##PullStrategy", &mSelectedStrategy, strategies, 2);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool canPull = hasRemotes && hasBranches;
        if (!canPull) ImGui::BeginDisabled();

        if (ImGui::Button("Pull", ImVec2(120, 0)))
        {
            GitService* service = GitService::Get();
            GitRepository* repo = service->GetCurrentRepo();

            GitOperationRequest request;
            request.mKind = GitOperationKind::Pull;
            request.mRepoPath = repo->GetPath();
            request.mRemoteName = mRemoteNames[mSelectedRemote];
            request.mBranchName = mBranchNames[mSelectedBranch];
            request.mFastForwardOnly = (mSelectedStrategy == 1);
            request.mCancelToken = CreateCancelToken();

            service->GetOperationQueue()->Enqueue(request);

            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (!canPull) ImGui::EndDisabled();

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
