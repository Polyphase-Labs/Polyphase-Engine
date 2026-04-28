#if EDITOR

#include "GitCloneDialog.h"
#include "../GitService.h"
#include "../GitOperationQueue.h"
#include "../GitOperationRequest.h"
#include "imgui.h"
#include <cstring>

static GitCloneDialog sInstance;

GitCloneDialog* GetGitCloneDialog()
{
    return &sInstance;
}

void GitCloneDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;
    std::memset(mUrl, 0, sizeof(mUrl));
    std::memset(mDestPath, 0, sizeof(mDestPath));
    std::memset(mFolderName, 0, sizeof(mFolderName));
    std::memset(mBranch, 0, sizeof(mBranch));
}

void GitCloneDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Clone Repository");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Clone Repository", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Repository URL");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##CloneUrl", mUrl, sizeof(mUrl));

        ImGui::Spacing();
        ImGui::Text("Destination Path");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##CloneDestPath", mDestPath, sizeof(mDestPath));

        ImGui::Spacing();
        ImGui::Text("Folder Name (optional)");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##CloneFolderName", mFolderName, sizeof(mFolderName));

        ImGui::Spacing();
        ImGui::Text("Branch (optional, defaults to remote HEAD)");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##CloneBranch", mBranch, sizeof(mBranch));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool urlEmpty = (std::strlen(mUrl) == 0);
        bool destEmpty = (std::strlen(mDestPath) == 0);

        if (urlEmpty || destEmpty)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Clone", ImVec2(120, 0)))
        {
            std::string destPath = mDestPath;
            if (std::strlen(mFolderName) > 0)
            {
                if (!destPath.empty() && destPath.back() != '/' && destPath.back() != '\\')
                {
                    destPath += '/';
                }
                destPath += mFolderName;
            }

            GitOperationRequest request;
            request.mKind = GitOperationKind::Clone;
            request.mSourceUrl = mUrl;
            request.mDestPath = destPath;
            request.mBranchName = mBranch;

            GitService::Get()->GetOperationQueue()->Enqueue(request);

            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (urlEmpty || destEmpty)
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
