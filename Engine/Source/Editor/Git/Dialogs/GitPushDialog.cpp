#if EDITOR

#include "GitPushDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "../GitOperationQueue.h"
#include "../GitOperationRequest.h"
#include "imgui.h"
#include <cstring>

static GitPushDialog sInstance;

GitPushDialog* GetGitPushDialog()
{
    return &sInstance;
}

void GitPushDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;
    mPushTags = false;
    mSetUpstream = false;
    mShowForcePush = false;
    mForcePush = false;
    mSelectedRemote = 0;
    mSelectedSourceBranch = 0;
    mSelectedDestBranch = 0;

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
                mSelectedSourceBranch = i;
                mSelectedDestBranch = i;
                break;
            }
        }
    }
}

void GitPushDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Push");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Push", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        bool hasRemotes = !mRemoteNames.empty();
        bool hasBranches = !mBranchNames.empty();

        // Remote selector
        ImGui::Text("Remote");
        ImGui::SetNextItemWidth(-1);
        if (hasRemotes)
        {
            if (ImGui::BeginCombo("##PushRemote", mRemoteNames[mSelectedRemote].c_str()))
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

        // Source branch
        ImGui::Text("Source Branch");
        ImGui::SetNextItemWidth(-1);
        if (hasBranches)
        {
            if (ImGui::BeginCombo("##PushSourceBranch", mBranchNames[mSelectedSourceBranch].c_str()))
            {
                for (int32_t i = 0; i < (int32_t)mBranchNames.size(); ++i)
                {
                    bool selected = (i == mSelectedSourceBranch);
                    if (ImGui::Selectable(mBranchNames[i].c_str(), selected))
                    {
                        mSelectedSourceBranch = i;
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

        // Destination branch
        ImGui::Text("Destination Branch");
        ImGui::SetNextItemWidth(-1);
        if (hasBranches)
        {
            if (ImGui::BeginCombo("##PushDestBranch", mBranchNames[mSelectedDestBranch].c_str()))
            {
                for (int32_t i = 0; i < (int32_t)mBranchNames.size(); ++i)
                {
                    bool selected = (i == mSelectedDestBranch);
                    if (ImGui::Selectable(mBranchNames[i].c_str(), selected))
                    {
                        mSelectedDestBranch = i;
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
        ImGui::Separator();
        ImGui::Spacing();

        // Options
        ImGui::Checkbox("Push tags", &mPushTags);
        ImGui::Checkbox("Set upstream (-u)", &mSetUpstream);

        ImGui::Spacing();
        ImGui::Checkbox("Show force push option", &mShowForcePush);

        if (mShowForcePush)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Checkbox("Force push (--force-with-lease)", &mForcePush);
            if (mForcePush)
            {
                ImGui::TextWrapped("WARNING: Force push rewrites remote history. "
                    "Other collaborators may lose work. Use with caution.");
            }
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool canPush = hasRemotes && hasBranches;
        if (!canPush) ImGui::BeginDisabled();

        if (ImGui::Button("Push", ImVec2(120, 0)))
        {
            GitService* service = GitService::Get();
            GitRepository* repo = service->GetCurrentRepo();

            GitOperationRequest request;
            request.mKind = GitOperationKind::Push;
            request.mRepoPath = repo->GetPath();
            request.mRemoteName = mRemoteNames[mSelectedRemote];
            request.mBranchName = mBranchNames[mSelectedSourceBranch];

            // If source and dest differ, use refspec notation
            if (mSelectedSourceBranch != mSelectedDestBranch)
            {
                request.mBranchName = mBranchNames[mSelectedSourceBranch] + ":" + mBranchNames[mSelectedDestBranch];
            }

            request.mPushTags = mPushTags;
            request.mSetUpstream = mSetUpstream;
            request.mForce = mForcePush;
            request.mCancelToken = CreateCancelToken();

            if (mPushTags)
            {
                // Enqueue a separate PushTags operation after the push
                GitOperationRequest tagsRequest;
                tagsRequest.mKind = GitOperationKind::PushTags;
                tagsRequest.mRepoPath = repo->GetPath();
                tagsRequest.mRemoteName = mRemoteNames[mSelectedRemote];
                tagsRequest.mCancelToken = CreateCancelToken();

                service->GetOperationQueue()->Enqueue(request);
                service->GetOperationQueue()->Enqueue(tagsRequest);
            }
            else
            {
                service->GetOperationQueue()->Enqueue(request);
            }

            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (!canPush) ImGui::EndDisabled();

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
