#if EDITOR

#include "GitFetchDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "../GitOperationQueue.h"
#include "../GitOperationRequest.h"
#include "imgui.h"
#include <cstring>

static GitFetchDialog sInstance;

GitFetchDialog* GetGitFetchDialog()
{
    return &sInstance;
}

void GitFetchDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;
    mSelectedRemote = 0;
    mFetchAll = false;
    mPrune = false;
    mFetchTags = false;

    mRemoteNames.clear();

    GitService* service = GitService::Get();
    if (service && service->IsRepositoryOpen())
    {
        GitRepository* repo = service->GetCurrentRepo();
        std::vector<GitRemoteInfo> remotes = repo->GetRemotes();
        for (const auto& remote : remotes)
        {
            mRemoteNames.push_back(remote.mName);
        }
    }
}

void GitFetchDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Fetch");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Fetch", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        bool hasRemotes = !mRemoteNames.empty();

        // Remote selector with "All Remotes" option
        ImGui::Text("Remote");
        ImGui::SetNextItemWidth(-1);
        if (hasRemotes)
        {
            // Build display label
            const char* currentLabel = mFetchAll ? "All Remotes" : mRemoteNames[mSelectedRemote].c_str();

            if (ImGui::BeginCombo("##FetchRemote", currentLabel))
            {
                // Individual remotes
                for (int32_t i = 0; i < (int32_t)mRemoteNames.size(); ++i)
                {
                    bool selected = (!mFetchAll && i == mSelectedRemote);
                    if (ImGui::Selectable(mRemoteNames[i].c_str(), selected))
                    {
                        mSelectedRemote = i;
                        mFetchAll = false;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }

                ImGui::Separator();

                // All remotes option
                bool allSelected = mFetchAll;
                if (ImGui::Selectable("All Remotes", allSelected))
                {
                    mFetchAll = true;
                }
                if (allSelected) ImGui::SetItemDefaultFocus();

                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::TextDisabled("No remotes configured");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Options
        ImGui::Checkbox("Prune deleted remote branches", &mPrune);
        ImGui::Checkbox("Fetch tags", &mFetchTags);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool canFetch = hasRemotes;
        if (!canFetch) ImGui::BeginDisabled();

        if (ImGui::Button("Fetch", ImVec2(120, 0)))
        {
            GitService* service = GitService::Get();
            GitRepository* repo = service->GetCurrentRepo();

            GitOperationRequest request;
            request.mKind = GitOperationKind::Fetch;
            request.mRepoPath = repo->GetPath();
            request.mFetchAll = mFetchAll;
            request.mPrune = mPrune;
            request.mFetchTags = mFetchTags;
            request.mCancelToken = CreateCancelToken();

            if (!mFetchAll)
            {
                request.mRemoteName = mRemoteNames[mSelectedRemote];
            }

            service->GetOperationQueue()->Enqueue(request);

            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (!canFetch) ImGui::EndDisabled();

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
